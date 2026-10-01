#include "cbdos/audio.hpp"
#include "cbdos_device_tree.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <cmath>

namespace cbdos {
namespace bsp {

class S3AudioSink : public cbdos::audio::IAudioSink {
public:
    bool init(uint32_t sampleRate = 44100, uint8_t channels = 2, uint8_t bitsPerSample = 16) override {
        (void)channels;
        (void)bitsPerSample;
        if (m_installed) return true;

        i2s_config_t cfg = {
            .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate          = sampleRate,
            .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count        = 8,
            .dma_buf_len          = 512,
            .use_apll             = false,
            .tx_desc_auto_clear   = true,
            .fixed_mclk           = 0
        };

        i2s_pin_config_t pins = {
            .bck_io_num   = cbdos::board::audio::PIN_BCLK,
            .ws_io_num    = cbdos::board::audio::PIN_WS,
            .data_out_num = cbdos::board::audio::PIN_DOUT,
            .data_in_num  = (cbdos::board::audio::PIN_DIN >= 0) ? cbdos::board::audio::PIN_DIN : I2S_PIN_NO_CHANGE
        };

        if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) {
            Serial.println("[AudioHAL-S3] Error instalando driver I2S");
            return false;
        }
        if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
            Serial.println("[AudioHAL-S3] Error configurando pines I2S");
            return false;
        }
        i2s_zero_dma_buffer(I2S_NUM_0);
        m_installed = true;
        m_sampleRate = sampleRate;
        Serial.printf("[AudioHAL-S3] I2S listo en GPIOs BCK=%d, WS=%d, DOUT=%d a %lu Hz\n",
                      cbdos::board::audio::PIN_BCLK,
                      cbdos::board::audio::PIN_WS,
                      cbdos::board::audio::PIN_DOUT,
                      (unsigned long)sampleRate);
        return true;
    }

    void deinit() override {
        if (m_installed) {
            i2s_driver_uninstall(I2S_NUM_0);
            m_installed = false;
        }
    }

    size_t write(const void* pcmData, size_t sizeBytes, uint32_t timeoutMs = 1000) override {
        if (!m_installed) {
            if (!init(m_sampleRate)) return 0;
        }
        if (!pcmData || sizeBytes == 0) return 0;

        size_t bytesWritten = 0;
        TickType_t timeoutTicks = pdMS_TO_TICKS(timeoutMs);

        if (m_muted || m_volume == 0) {
            uint8_t zeroBuf[256] = {0};
            size_t left = sizeBytes;
            while (left > 0) {
                size_t chunk = left > sizeof(zeroBuf) ? sizeof(zeroBuf) : left;
                i2s_write(I2S_NUM_0, zeroBuf, chunk, &bytesWritten, timeoutTicks);
                left -= chunk;
            }
            return sizeBytes;
        }

        if (m_volume >= 100) {
            esp_err_t ret = i2s_write(I2S_NUM_0, pcmData, sizeBytes, &bytesWritten, timeoutTicks);
            return (ret == ESP_OK) ? bytesWritten : 0;
        }

        // Escalado de volumen por software para 16-bit PCM estéreo
        int16_t scaledBuf[256];
        const int16_t* inSamples = (const int16_t*)pcmData;
        size_t sampleCount = sizeBytes / sizeof(int16_t);
        size_t offset = 0;
        int32_t volFactor = (int32_t)m_volume;
        size_t totalWritten = 0;

        while (offset < sampleCount) {
            size_t chunk = (sampleCount - offset > 256) ? 256 : (sampleCount - offset);
            for (size_t i = 0; i < chunk; ++i) {
                scaledBuf[i] = (int16_t)(((int32_t)inSamples[offset + i] * volFactor) / 100);
            }
            esp_err_t ret = i2s_write(I2S_NUM_0, scaledBuf, chunk * sizeof(int16_t), &bytesWritten, timeoutTicks);
            if (ret != ESP_OK) break;
            totalWritten += bytesWritten;
            offset += chunk;
        }

        return totalWritten;
    }

    void setVolume(uint8_t volumePercent) override {
        if (volumePercent > 100) volumePercent = 100;
        m_volume = volumePercent;
    }

    uint8_t getVolume() const override {
        return m_volume;
    }

    bool setSampleRate(uint32_t sampleRate) override {
        if (!m_installed) {
            return init(sampleRate);
        }
        if (m_sampleRate == sampleRate) return true;
        m_sampleRate = sampleRate;
        return i2s_set_sample_rates(I2S_NUM_0, sampleRate) == ESP_OK;
    }

    void mute(bool enable) override {
        m_muted = enable;
    }

    bool isMuted() const override {
        return m_muted;
    }

    void playTone(uint32_t freqHz, uint32_t durationMs) override {
        if (!m_installed) init(m_sampleRate);
        if (freqHz == 0 || durationMs == 0) return;

        const uint32_t sRate = m_sampleRate ? m_sampleRate : 44100;
        uint32_t totalSamples = (sRate * durationMs) / 1000;
        const size_t CHUNK = 256;
        int16_t buffer[CHUNK * 2]; // Estéreo

        float phase = 0.0f;
        float phaseInc = (2.0f * (float)M_PI * (float)freqHz) / (float)sRate;
        int32_t volFactor = (int32_t)m_volume;
        float amplitude = (32767.0f * volFactor) / 100.0f;

        size_t written = 0;
        for (uint32_t i = 0; i < totalSamples; i += CHUNK) {
            size_t samplesThisChunk = (totalSamples - i > CHUNK) ? CHUNK : (totalSamples - i);
            for (size_t s = 0; s < samplesThisChunk; ++s) {
                int16_t val = (int16_t)(sinf(phase) * amplitude);
                buffer[s * 2]     = val;
                buffer[s * 2 + 1] = val;
                phase += phaseInc;
                if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
            }
            i2s_write(I2S_NUM_0, buffer, samplesThisChunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        }
    }

    void playBeep() override {
        playTone(1200, 80);
    }

private:
    bool m_installed = false;
    bool m_muted = false;
    uint32_t m_sampleRate = 44100;
    uint8_t m_volume = 75;
};

class S3AudioSource : public cbdos::audio::IAudioSource {
public:
    bool init(const cbdos::audio::RecordConfig& cfg) override {
        (void)cfg;
        return true;
    }

    void deinit() override {}

    size_t read(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100) override {
        (void)dest;
        (void)sizeBytes;
        (void)timeoutMs;
        return 0; // S3 JC3248 no tiene microfono incorporado
    }

    void setMicGain(uint8_t gainDb) override {
        (void)gainDb;
    }

    float getPeakLevel() override {
        return 0.0f;
    }
};

static S3AudioSink s_s3AudioSink;
static S3AudioSource s_s3AudioSource;

void initAudioBackendS3() {
    cbdos::audio::setAudioSink(&s_s3AudioSink);
    cbdos::audio::setAudioSource(&s_s3AudioSource);
}

} // namespace bsp
} // namespace cbdos
