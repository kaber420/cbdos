#include "hal_flasher_p4.hpp"
#include "usb_cdc_loader_port.hpp"
#include "cbdos/flasher.hpp"
#include <esp_loader.h>
#include <esp_loader_io.h>
#include <esp32_port.h>

#include <driver/uart.h>
#include <driver/gpio.h>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static const char* TAG_FLASHER = "HAL_FLASHER_P4";

#define UART_PORT_NUM       UART_NUM_1

namespace cbdos {
namespace system {

FlasherServiceP4::FlasherServiceP4() {
    // 0. Preset USB-Serial Nativo por cable USB-C (ESP32-C3 / S3 / C6 de fábrica)
    cbdos::flasher::FlasherPreset p0;
    p0.id = "p4_usb_cdc_native";
    p0.name = "🔌 USB-Serial Nativo (Puerto USB-C ESP32-C3/S3)";
    p0.description = "Flasheo directo por cable USB-C usando USB-Serial/JTAG de fabrica.";
    p0.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXION DIRECTA POR CABLE USB-C           |\n"
        "+---------------------------------------------+\n"
        "| [Cable USB] Conectar ESP32-C3/S3 al puerto  |\n"
        "|             USB Host de la placa JC4880.    |\n"
        "| [Auto-Boot] El reset y entrada a bootloader |\n"
        "|             se controlan por DTR/RTS en USB |\n"
        "+---------------------------------------------+\n"
        "| No requiere cables sueltos ni pines GPIO.   |";
    p0.config.transport = cbdos::flasher::FlasherTransport::USB_CDC_NATIVE;
    p0.config.txPin = -1;
    p0.config.rxPin = -1;
    p0.config.bootPin = -1;
    p0.config.rstPin = -1;
    p0.config.baudRate = 115200;
    p0.config.flashOffset = 0x0;
    p0.config.binPath = "/sdcard/firmware.bin";
    p0.config.presetName = p0.name;
    p0.useEmbeddedBin = false;
    m_presets.push_back(p0);

    // 1. Preset Coprocesador ESP32-C6 (Integrado en placa JC4880P443C)
    cbdos::flasher::FlasherPreset p1;
    p1.id = "p4_c6_internal";
    p1.name = "ESP32-C6 Coprocesador (JC4880)";
    p1.description = "Coprocesador WiFi 6 / BT 5 integrado en modulo P4.";
    p1.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES EN CONECTOR JP1 (2x13)          |\n"
        "+---------------------------------------------+\n"
        "| [Jumper 1] Pin 19 (IO32) --> Pin 20 (C6_RX) |\n"
        "| [Jumper 2] Pin 21 (IO28) --> Pin 22 (C6_TX) |\n"
        "| [Cable 1]  Pin 17 (IO34) --> Pin 24 (C6_IO9)|\n"
        "+---------------------------------------------+\n"
        "| Energia (IO36) y Reset (IO54) son INTERNOS  |";
    p1.config.transport = cbdos::flasher::FlasherTransport::UART_PINS;
    p1.config.txPin = 32;
    p1.config.rxPin = 28;
    p1.config.bootPin = 34;
    p1.config.rstPin = 54;
    p1.config.baudRate = 115200;
    p1.config.flashOffset = 0x0;
    p1.config.binPath = "/sdcard/c6_slave.bin";
    p1.config.presetName = p1.name;
    p1.useEmbeddedBin = false;
    m_presets.push_back(p1);

    // 2. Preset ESP Externo vía Header JP1 (ESP32-P4)
    cbdos::flasher::FlasherPreset p2;
    p2.id = "p4_jp1_external";
    p2.name = "ESP Externo (Header JP1 P4)";
    p2.description = "Flasheo de chips ESP externos conectados a JP1.";
    p2.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES JP1 HACIA ESP EXTERNO           |\n"
        "+---------------------------------------------+\n"
        "| [UART TX]  JP1 Pin 19 (GPIO 32) -> Target RX|\n"
        "| [UART RX]  JP1 Pin 21 (GPIO 28) -> Target TX|\n"
        "| [BOOT IO0] JP1 Pin 17 (GPIO 34) -> Target IO0\n"
        "| [RESET]    JP1 Pin 54 / Manual  -> Target EN|\n"
        "| [POWER]    JP1 Pin 01 (3.3V/5V) -> Target VCC\n"
        "| [TIERRA]   JP1 Pin 05 (GND)     -> Target GND|\n"
        "+---------------------------------------------+";
    p2.config.txPin = 32;
    p2.config.rxPin = 28;
    p2.config.bootPin = 34;
    p2.config.rstPin = 54;
    p2.config.baudRate = 115200;
    p2.config.flashOffset = 0x0;
    p2.config.binPath = "/sdcard/firmware.bin";
    p2.config.presetName = p2.name;
    p2.useEmbeddedBin = false;
    m_presets.push_back(p2);

    // 3. Preset ESP32-C3 Dongle Bridge (Vía JP1)
    cbdos::flasher::FlasherPreset p3;
    p3.id = "p4_c3_bridge";
    p3.name = "ESP32-C3 Dongle Bridge (JP1)";
    p3.description = "Flasheo de modulo ESP32-C3 para modem / sniffer USB.";
    p3.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES JP1 HACIA ESP32-C3              |\n"
        "+---------------------------------------------+\n"
        "| [UART TX]  JP1 Pin 19 (GPIO 32) -> C3 RXD   |\n"
        "| [UART RX]  JP1 Pin 21 (GPIO 28) -> C3 TXD   |\n"
        "| [BOOT IO9] JP1 Pin 17 (GPIO 34) -> C3 IO9   |\n"
        "| [RESET]    JP1 Pin 54           -> C3 EN/RST|\n"
        "| [POWER]    JP1 Pin 01 (3.3V)    -> C3 3V3   |\n"
        "| [TIERRA]   JP1 Pin 05 (GND)     -> C3 GND   |\n"
        "+---------------------------------------------+";
    p3.config.txPin = 32;
    p3.config.rxPin = 28;
    p3.config.bootPin = 34;
    p3.config.rstPin = 54;
    p3.config.baudRate = 115200;
    p3.config.flashOffset = 0x0;
    p3.config.binPath = "/sdcard/espnow_usb_bridge_c3.bin";
    p3.config.presetName = p3.name;
    p3.useEmbeddedBin = false;
    m_presets.push_back(p3);

    // 4. Preset ESP Externo para ESP32-S3 (JC3248W535)
    cbdos::flasher::FlasherPreset p4;
    p4.id = "s3_external";
    p4.name = "ESP Externo (JC3248W535 S3)";
    p4.description = "Flasheo desde tarjeta S3 usando pines libres.";
    p4.wiringInfo =
        "+---------------------------------------------+\n"
        "|  CONEXIONES S3 HACIA ESP EXTERNO            |\n"
        "+---------------------------------------------+\n"
        "| [UART TX]  GPIO 15        -> Target RX      |\n"
        "| [UART RX]  GPIO 16        -> Target TX      |\n"
        "| [BOOT IO0] GPIO 0         -> Target GPIO 0  |\n"
        "| [RESET]    Boton Manual   -> Target EN / RST|\n"
        "| [FIRMWARE] MicroSD        -> /sdcard/*.bin  |\n"
        "+---------------------------------------------+";
    p4.config.txPin = 15;
    p4.config.rxPin = 16;
    p4.config.bootPin = 0;
    p4.config.rstPin = -1;
    p4.config.baudRate = 115200;
    p4.config.flashOffset = 0x0;
    p4.config.binPath = "/sdcard/firmware.bin";
    p4.config.presetName = p4.name;
    p4.useEmbeddedBin = false;
    m_presets.push_back(p4);

    // 5. Preset Personalizado / Manual
    cbdos::flasher::FlasherPreset p5;
    p5.id = "custom";
    p5.name = "Personalizado / Manual";
    p5.description = "Configuracion libre de GPIOs, baudrate y firmware.";
    p5.wiringInfo =
        "+---------------------------------------------+\n"
        "|  GUIA DE CONEXION PERSONALIZADA             |\n"
        "+---------------------------------------------+\n"
        "| 1. Conectar Host TX al pin RX del objetivo. |\n"
        "| 2. Conectar Host RX al pin TX del objetivo. |\n"
        "| 3. Conectar Host BOOT al pin GPIO0/IO9.     |\n"
        "| 4. Conectar Host RST al pin Reset/EN.       |\n"
        "| 5. Conectar masa GND comun entre placas.    |\n"
        "+---------------------------------------------+";
    p5.config.txPin = 32;
    p5.config.rxPin = 28;
    p5.config.bootPin = 34;
    p5.config.rstPin = 54;
    p5.config.baudRate = 115200;
    p5.config.flashOffset = 0x0;
    p5.config.binPath = "/sdcard/firmware.bin";
    p5.config.presetName = p5.name;
    p5.useEmbeddedBin = false;
    m_presets.push_back(p5);

    m_activeConfig = m_presets[0].config;
}

FlasherServiceP4& FlasherServiceP4::getInstance() {
    static FlasherServiceP4 instance;
    return instance;
}

const std::vector<cbdos::flasher::FlasherPreset>& FlasherServiceP4::getPresets() const {
    return m_presets;
}

cbdos::flasher::FlasherConfig FlasherServiceP4::getDefaultConfig() const {
    if (!m_presets.empty()) {
        return m_presets[0].config;
    }
    return cbdos::flasher::FlasherConfig();
}

bool FlasherServiceP4::startFlash(const cbdos::flasher::FlasherConfig& config, cbdos::flasher::FlasherProgressCb progressCb) {
    if (m_busy) {
        ESP_LOGW(TAG_FLASHER, "Flasheador ocupado actualmente");
        return false;
    }
    m_activeConfig = config;
    m_busy = true;
    m_cb = progressCb;
    m_status = cbdos::flasher::FlasherStatus::EnteringBootloader;
    m_progress = 0;
    m_message = "Iniciando modo bootloader...";

    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    xTaskCreatePinnedToCore(flashTaskWrapper, "flasher_task", 8192, this, 5, nullptr, 0);
    return true;
}

bool FlasherServiceP4::startFlash(cbdos::flasher::FlasherProgressCb progressCb) {
    return startFlash(getDefaultConfig(), progressCb);
}

void FlasherServiceP4::flashTaskWrapper(void* arg) {
    auto* self = static_cast<FlasherServiceP4*>(arg);
    self->runFlashTask();
    vTaskDelete(nullptr);
}

void FlasherServiceP4::runFlashTask() {
    ESP_LOGI(TAG_FLASHER, "=== Iniciando Flasheo Universal [%s] ===", m_activeConfig.presetName.c_str());
    ESP_LOGI(TAG_FLASHER, "Pines: TX=%d, RX=%d, BOOT=%d, RST=%d, Baud=%lu, Offset=0x%lx",
             m_activeConfig.txPin, m_activeConfig.rxPin, m_activeConfig.bootPin,
             m_activeConfig.rstPin, (unsigned long)m_activeConfig.baudRate, (unsigned long)m_activeConfig.flashOffset);

    // 1. Determinar fuente del binario
    const uint8_t* binData = nullptr;
    size_t binSize = 0;
    uint8_t* sdBuf = nullptr;

    if (!m_activeConfig.binPath.empty()) {
        FILE* fSd = fopen(m_activeConfig.binPath.c_str(), "rb");
        if (fSd) {
            fseek(fSd, 0, SEEK_END);
            binSize = ftell(fSd);
            fseek(fSd, 0, SEEK_SET);
            ESP_LOGI(TAG_FLASHER, "Firmware cargado desde almacenamiento (%s): %u bytes", m_activeConfig.binPath.c_str(), binSize);
            sdBuf = (uint8_t*)malloc(binSize);
            if (sdBuf) {
                fread(sdBuf, 1, binSize, fSd);
                binData = sdBuf;
            }
            fclose(fSd);
        } else {
            ESP_LOGW(TAG_FLASHER, "No se encontro archivo: %s", m_activeConfig.binPath.c_str());
        }
    }

    if (!binData || binSize == 0) {
        m_status = cbdos::flasher::FlasherStatus::Failed;
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "Error: No se encontro el archivo %s en MicroSD", m_activeConfig.binPath.c_str());
        m_message = errBuf;
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        m_busy = false;
        return;
    }

    // 2. Configurar transporte según el tipo seleccionado (USB Host CDC o UART Pins)
    bool isUsbNative = (m_activeConfig.transport == cbdos::flasher::FlasherTransport::USB_CDC_NATIVE);

    m_status = cbdos::flasher::FlasherStatus::Connecting;
    m_progress = 5;
    m_message = isUsbNative ? "Buscando ESP32 conectado por cable USB-C..." 
                            : "Sincronizando con ROM del microcontrolador objetivo...";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    if (isUsbNative) {
        if (loader_port_usb_cdc_init(5000) != ESP_LOADER_SUCCESS) {
            m_status = cbdos::flasher::FlasherStatus::Failed;
            m_message = "No se detectó ningún ESP32 en el puerto USB-C (Verifica cable OTG)";
            if (m_cb) m_cb(m_status, 0, m_message.c_str());
            if (sdBuf) free(sdBuf);
            m_busy = false;
            return;
        }
    } else {
        loader_esp32_config_t config = {};
        config.baud_rate = m_activeConfig.baudRate > 0 ? m_activeConfig.baudRate : 115200;
        config.uart_port = UART_PORT_NUM;
        config.uart_rx_pin = static_cast<gpio_num_t>(m_activeConfig.rxPin);
        config.uart_tx_pin = static_cast<gpio_num_t>(m_activeConfig.txPin);
        config.reset_trigger_pin = m_activeConfig.rstPin >= 0 ? static_cast<gpio_num_t>(m_activeConfig.rstPin) : GPIO_NUM_NC;
        config.gpio0_trigger_pin = m_activeConfig.bootPin >= 0 ? static_cast<gpio_num_t>(m_activeConfig.bootPin) : GPIO_NUM_NC;

        if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
            m_status = cbdos::flasher::FlasherStatus::Failed;
            m_message = "Fallo al inicializar puerto UART para flasheo";
            if (m_cb) m_cb(m_status, 0, m_message.c_str());
            if (sdBuf) free(sdBuf);
            m_busy = false;
            return;
        }
    }

    // 3. Conectar e identificar chip
    esp_loader_connect_args_t connect_args = ESP_LOADER_CONNECT_DEFAULT();
    connect_args.trials = isUsbNative ? 20 : 15;
    connect_args.sync_timeout = isUsbNative ? 300 : 200;

    esp_loader_error_t err = esp_loader_connect(&connect_args);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG_FLASHER, "Fallo esp_loader_connect (err=%d)", err);
        m_status = cbdos::flasher::FlasherStatus::Failed;
        m_message = isUsbNative ? "ESP32 no respondió al bootloader por USB"
                                : "No responde el chip (Verifica conexiones TX/RX/BOOT/VCC)";
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        if (isUsbNative) loader_port_usb_cdc_deinit();
        else loader_port_esp32_deinit();
        if (sdBuf) free(sdBuf);
        m_busy = false;
        return;
    }

    target_chip_t target = esp_loader_get_target();
    ESP_LOGI(TAG_FLASHER, "Microcontrolador detectado exitosamente! Target chip ID: %d", target);

    // 4. Preparar memoria Flash
    m_status = cbdos::flasher::FlasherStatus::Erasing;
    m_progress = 15;
    m_message = "Preparando memoria Flash...";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    err = esp_loader_flash_start(m_activeConfig.flashOffset, binSize, 4096);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG_FLASHER, "Fallo esp_loader_flash_start (err=%d)", err);
        m_status = cbdos::flasher::FlasherStatus::Failed;
        m_message = "Fallo al preparar sector de Flash";
        if (m_cb) m_cb(m_status, 0, m_message.c_str());
        if (isUsbNative) loader_port_usb_cdc_deinit();
        else loader_port_esp32_deinit();
        if (sdBuf) free(sdBuf);
        m_busy = false;
        return;
    }

    // 5. Grabar payload
    m_status = cbdos::flasher::FlasherStatus::Writing;
    uint32_t written = 0;
    const uint32_t blockSize = 4096;

    while (written < binSize) {
        uint32_t toWrite = binSize - written;
        if (toWrite > blockSize) toWrite = blockSize;

        err = esp_loader_flash_write((void*)(binData + written), toWrite);
        if (err != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG_FLASHER, "Fallo esp_loader_flash_write en offset %u (err=%d)", written, err);
            m_status = cbdos::flasher::FlasherStatus::Failed;
            m_message = "Error durante escritura de Flash";
            if (m_cb) m_cb(m_status, 0, m_message.c_str());
            if (isUsbNative) loader_port_usb_cdc_deinit();
            else loader_port_esp32_deinit();
            if (sdBuf) free(sdBuf);
            m_busy = false;
            return;
        }

        written += toWrite;
        m_progress = 15 + (written * 80 / binSize);
        char progMsg[64];
        snprintf(progMsg, sizeof(progMsg), "Escribiendo Flash (%u / %u bytes)...", written, binSize);
        m_message = progMsg;
        if (m_cb) m_cb(m_status, m_progress, m_message.c_str());
    }

    // 6. Finalizar y verificar
    m_status = cbdos::flasher::FlasherStatus::Verifying;
    m_progress = 98;
    m_message = "Verificando integridad y reiniciando chip...";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    err = esp_loader_flash_finish(true);
    if (err != ESP_LOADER_SUCCESS) {
        ESP_LOGW(TAG_FLASHER, "Aviso esp_loader_flash_finish: %d", err);
    }

    // 7. Liberar puerto y reiniciar el target en modo normal
    if (isUsbNative) {
        loader_port_usb_cdc_reset_target();
        loader_port_usb_cdc_deinit();
    } else {
        loader_port_esp32_deinit();

        if (m_activeConfig.bootPin >= 0) {
            gpio_set_direction(static_cast<gpio_num_t>(m_activeConfig.bootPin), GPIO_MODE_OUTPUT);
            gpio_set_level(static_cast<gpio_num_t>(m_activeConfig.bootPin), 1);
        }

        if (m_activeConfig.rstPin >= 0) {
            gpio_set_direction(static_cast<gpio_num_t>(m_activeConfig.rstPin), GPIO_MODE_OUTPUT);
            gpio_set_level(static_cast<gpio_num_t>(m_activeConfig.rstPin), 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(static_cast<gpio_num_t>(m_activeConfig.rstPin), 1);
        }
    }

    if (sdBuf) free(sdBuf);

    m_status = cbdos::flasher::FlasherStatus::Success;
    m_progress = 100;
    m_message = "¡Microcontrolador programado exitosamente al 100%!";
    if (m_cb) m_cb(m_status, m_progress, m_message.c_str());

    ESP_LOGI(TAG_FLASHER, "=== Flasheo Completado con Éxito ===");
    m_busy = false;
}

} // namespace system

namespace flasher {

bool isSupported() {
    return true;
}

bool isBusy() {
    return system::FlasherServiceP4::getInstance().isBusy();
}

const std::vector<FlasherPreset>& getPresets() {
    return system::FlasherServiceP4::getInstance().getPresets();
}

FlasherConfig getDefaultConfig() {
    return system::FlasherServiceP4::getInstance().getDefaultConfig();
}

bool startFlash(const FlasherConfig& config, FlasherProgressCb progressCb) {
    return system::FlasherServiceP4::getInstance().startFlash(config, progressCb);
}

bool startFlash(FlasherProgressCb progressCb) {
    return system::FlasherServiceP4::getInstance().startFlash(progressCb);
}

} // namespace flasher
} // namespace cbdos
