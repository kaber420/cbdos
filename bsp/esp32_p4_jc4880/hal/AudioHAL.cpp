#include "AudioHAL.h"
#include "TouchHAL.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_codec_dev_defaults.h"
#include "es8311_codec.h"

static const char* TAG = "AudioHAL";

AudioHAL::AudioHAL() {}
AudioHAL::~AudioHAL() {
    if (playDevHandle) {
        esp_codec_dev_close(playDevHandle);
        esp_codec_dev_delete(playDevHandle);
        playDevHandle = nullptr;
    }
}

esp_err_t AudioHAL::init(uint32_t sampleRate) {
    if (initialized) return ESP_OK;

    ESP_LOGI(TAG, "=== Inicializando AudioHAL con esp_codec_dev (ESP32-P4 JC4880P443C) ===");
    currentSampleRate = sampleRate;

    // 1. Configurar Pin PA (Power Amplifier) en nivel bajo para evitar pop
    if (BOARD_AUDIO_PA_GPIO >= 0) {
        gpio_config_t pa_conf = {
            .pin_bit_mask = (1ULL << BOARD_AUDIO_PA_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&pa_conf);
        gpio_set_level((gpio_num_t)BOARD_AUDIO_PA_GPIO, 0);
        ESP_LOGI(TAG, "Amplificador de audio configurado en silencio (GPIO %d)", BOARD_AUDIO_PA_GPIO);
    }

    // 2. Configurar Interfaz de Control I2C usando el bus compartido
    audio_codec_i2c_cfg_t i2c_cfg = {};
    i2c_cfg.port = (uint8_t)BOARD_TOUCH_I2C_PORT;
    i2c_cfg.addr = BOARD_AUDIO_CODEC_ADDR;
    i2c_cfg.bus_handle = TouchHAL::getInstance().getI2cBusHandle();
    
    const audio_codec_ctrl_if_t* i2c_ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!i2c_ctrl) {
        ESP_LOGE(TAG, "Fallo al crear interfaz I2C para códec");
        return ESP_FAIL;
    }

    // 3. Configurar Interfaz de Datos I2S con buffers DMA continuos y estables (Dúplex TX + RX)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BOARD_AUDIO_I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 16;
    chan_cfg.dma_frame_num = 512;
    chan_cfg.auto_clear = true;
    i2s_chan_handle_t tx_handle = nullptr;
    i2s_chan_handle_t rx_handle = nullptr;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al crear canales I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)BOARD_AUDIO_MCLK_GPIO,
            .bclk = (gpio_num_t)BOARD_AUDIO_BCLK_GPIO,
            .ws = (gpio_num_t)BOARD_AUDIO_WS_GPIO,
            .dout = (gpio_num_t)BOARD_AUDIO_DOUT_GPIO,
            .din = (gpio_num_t)BOARD_AUDIO_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar modo estándar I2S TX: %s", esp_err_to_name(ret));
        return ret;
    }

    if (rx_handle) {
        ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Aviso: Fallo al inicializar I2S RX: %s", esp_err_to_name(ret));
        }
    }

    audio_codec_i2s_cfg_t i2s_cfg = {};
    i2s_cfg.port = (uint8_t)BOARD_AUDIO_I2S_PORT;
    i2s_cfg.tx_handle = tx_handle;
    i2s_cfg.rx_handle = rx_handle;
    
    const audio_codec_data_if_t* data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (!data_if) {
        ESP_LOGE(TAG, "Fallo al crear interfaz de datos I2S");
        return ESP_FAIL;
    }

    // 4. Crear interfaz del códec ES8311 (Dúplex DAC + ADC)
    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = i2c_ctrl;
    es8311_cfg.gpio_if = audio_codec_new_gpio();
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = BOARD_AUDIO_PA_GPIO;
    es8311_cfg.pa_reverted = false;
    es8311_cfg.master_mode = false;
    es8311_cfg.use_mclk = true;
    es8311_cfg.digital_mic = false;
    es8311_cfg.mclk_div = 256;

    const audio_codec_if_t* codec_if = es8311_codec_new(&es8311_cfg);
    if (!codec_if) {
        ESP_LOGE(TAG, "Fallo al crear interfaz de códec ES8311");
        return ESP_FAIL;
    }

    // 5. Crear instancia del dispositivo códec para reproducción (OUT)
    esp_codec_dev_cfg_t play_dev_cfg = {};
    play_dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    play_dev_cfg.codec_if = codec_if;
    play_dev_cfg.data_if = data_if;

    playDevHandle = esp_codec_dev_new(&play_dev_cfg);
    if (!playDevHandle) {
        ESP_LOGE(TAG, "Fallo al instanciar códec ES8311 (Play)");
        return ESP_FAIL;
    }

    // 6. Crear instancia del dispositivo códec para grabación (IN)
    if (rx_handle) {
        esp_codec_dev_cfg_t rec_dev_cfg = {};
        rec_dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
        rec_dev_cfg.codec_if = codec_if;
        rec_dev_cfg.data_if = data_if;

        recordDevHandle = esp_codec_dev_new(&rec_dev_cfg);
        if (!recordDevHandle) {
            ESP_LOGW(TAG, "Aviso: Fallo al instanciar dev grabación ES8311");
        } else {
            esp_codec_dev_sample_info_t fs_rec = {};
            fs_rec.bits_per_sample = 16;
            fs_rec.channel = 2; // I2S Philips slot stereo
            fs_rec.sample_rate = sampleRate;
            fs_rec.mclk_multiple = 256;
            if (esp_codec_dev_open(recordDevHandle, &fs_rec) == ESP_OK) {
                esp_codec_dev_set_in_gain(recordDevHandle, 24.0f); // Ganancia micrófono +24dB
                ESP_LOGI(TAG, "Dispositivo de grabación ES8311 abierto correctamente");
            }
        }
    }

    // 7. Abrir dispositivo para reproducción
    esp_codec_dev_sample_info_t fs = {};
    fs.bits_per_sample = 16;
    fs.channel = 2;
    fs.sample_rate = sampleRate;
    fs.mclk_multiple = 256;

    ret = esp_codec_dev_open(playDevHandle, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al abrir códec para reproducción: %s", esp_err_to_name(ret));
        return ret;
    }

    // Habilitar PA una vez configurado y estabilizado el códec
    if (BOARD_AUDIO_PA_GPIO >= 0) {
        gpio_set_level((gpio_num_t)BOARD_AUDIO_PA_GPIO, 1);
    }

    // 7. Aplicar volumen inicial
    setVolume(currentVolume);

    initialized = true;
    ESP_LOGI(TAG, "AudioHAL (ES8311) listo y operativo a %lu Hz (Arranque silencioso)", sampleRate);

    return ESP_OK;
}

esp_err_t AudioHAL::setSampleRate(uint32_t sampleRate) {
    if (sampleRate == 0 || sampleRate == currentSampleRate) return ESP_OK;
    if (!playDevHandle) return ESP_FAIL;

    esp_codec_dev_sample_info_t fs = {};
    fs.bits_per_sample = 16;
    fs.channel = 2;
    fs.sample_rate = sampleRate;
    fs.mclk_multiple = 256;

    // Sincronizar ambos canales (TX y RX) para evitar conflicto de clock peer en I2S dúplex
    if (recordDevHandle) {
        esp_codec_dev_close(recordDevHandle);
    }
    esp_codec_dev_close(playDevHandle);

    esp_err_t ret = esp_codec_dev_open(playDevHandle, &fs);
    if (recordDevHandle) {
        esp_codec_dev_open(recordDevHandle, &fs);
    }

    if (ret == ESP_OK) {
        currentSampleRate = sampleRate;
        ESP_LOGI(TAG, "I2S Hardware Sample Rate actualizado a %lu Hz", sampleRate);
        if (BOARD_AUDIO_PA_GPIO >= 0) {
            gpio_set_level((gpio_num_t)BOARD_AUDIO_PA_GPIO, 1);
        }
        setVolume(currentVolume);
    } else {
        ESP_LOGE(TAG, "Fallo al reabrir códec con nuevo sample rate %lu: %s", sampleRate, esp_err_to_name(ret));
    }
    return ret;
}

void AudioHAL::setVolume(uint8_t volumePercent) {
    if (volumePercent > 100) volumePercent = 100;
    if (currentVolume == volumePercent && initialized) return;
    currentVolume = volumePercent;

    if (playDevHandle) {
        esp_codec_dev_set_out_vol(playDevHandle, currentVolume);
    }
    ESP_LOGI(TAG, "Volumen ajustado a %d%%", currentVolume);
}

void AudioHAL::mute(bool enable) {
    muted = enable;
    if (playDevHandle) {
        esp_codec_dev_set_out_mute(playDevHandle, enable);
    }
}

esp_err_t AudioHAL::writeAudio(const void* src, size_t size, size_t* bytesWritten, uint32_t timeoutMs) {
    (void)timeoutMs;
    if (!playDevHandle || !src || size == 0) return ESP_ERR_INVALID_STATE;
    int ret = esp_codec_dev_write(playDevHandle, (void*)src, size);
    if (bytesWritten) {
        *bytesWritten = (ret == ESP_CODEC_DEV_OK) ? size : 0;
    }
    return (ret == ESP_CODEC_DEV_OK) ? ESP_OK : ESP_FAIL;
}

void AudioHAL::playTone(uint32_t freqHz, uint32_t durationMs) {
    if (!playDevHandle || freqHz == 0 || durationMs == 0) return;

    size_t totalSamples = (currentSampleRate * durationMs) / 1000;
    const size_t CHUNK_SAMPLES = 256;
    int16_t buffer[CHUNK_SAMPLES * 2]; // Estéreo

    float phase = 0.0f;
    float phaseInc = (2.0f * (float)M_PI * (float)freqHz) / (float)currentSampleRate;
    size_t samplesGenerated = 0;

    float amplitude = 18000.0f;

    while (samplesGenerated < totalSamples) {
        size_t toGen = totalSamples - samplesGenerated;
        if (toGen > CHUNK_SAMPLES) toGen = CHUNK_SAMPLES;

        for (size_t i = 0; i < toGen; i++) {
            float env = 1.0f;
            size_t cur = samplesGenerated + i;
            if (cur < 200) {
                env = (float)cur / 200.0f;
            } else if (cur > totalSamples - 200) {
                env = (float)(totalSamples - cur) / 200.0f;
            }

            int16_t sample = (int16_t)(std::sin(phase) * amplitude * env);
            buffer[i * 2] = sample;     // Izquierda
            buffer[i * 2 + 1] = sample; // Derecha

            phase += phaseInc;
            if (phase >= 2.0f * (float)M_PI) {
                phase -= 2.0f * (float)M_PI;
            }
        }

        esp_codec_dev_write(playDevHandle, buffer, toGen * 2 * sizeof(int16_t));
        samplesGenerated += toGen;
    }
}

void AudioHAL::playBeep() {
    playTone(880, 50);
    vTaskDelay(pdMS_TO_TICKS(15));
    playTone(1320, 65);
}

void AudioHAL::playStartupChime() {
    playTone(523, 80);  // Do5
    vTaskDelay(pdMS_TO_TICKS(10));
    playTone(659, 80);  // Mi5
    vTaskDelay(pdMS_TO_TICKS(10));
    playTone(784, 100); // Sol5
    vTaskDelay(pdMS_TO_TICKS(10));
    playTone(1046, 160);// Do6
}

extern "C" bool Board_Audio_Init(uint32_t sampleRate) {
    return AudioHAL::getInstance().init(sampleRate) == ESP_OK;
}

extern "C" void Board_Audio_SetVolume(uint8_t volumePercent) {
    AudioHAL::getInstance().setVolume(volumePercent);
}

extern "C" int Board_Audio_Write(const void* data, size_t size) {
    size_t written = 0;
    AudioHAL::getInstance().writeAudio(data, size, &written, portMAX_DELAY);
    return (int)written;
}

esp_err_t AudioHAL::readAudio(void* dest, size_t size, size_t* bytesRead, uint32_t timeoutMs) {
    (void)timeoutMs;
    if (!recordDevHandle || !dest || size == 0) return ESP_ERR_INVALID_STATE;
    int ret = esp_codec_dev_read(recordDevHandle, dest, size);
    if (bytesRead) {
        *bytesRead = (ret == ESP_CODEC_DEV_OK) ? size : 0;
    }
    return (ret == ESP_CODEC_DEV_OK) ? ESP_OK : ESP_FAIL;
}

void AudioHAL::setMicGain(float dbGain) {
    if (recordDevHandle) {
        esp_codec_dev_set_in_gain(recordDevHandle, dbGain);
    }
}

extern "C" int Board_Audio_Read(void* data, size_t size) {
    size_t readBytes = 0;
    AudioHAL::getInstance().readAudio(data, size, &readBytes, 100);
    return (int)readBytes;
}

