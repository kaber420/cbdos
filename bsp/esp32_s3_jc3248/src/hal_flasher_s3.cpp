#include "cbdos_build_profile.h"
#if CBDOS_FEATURE_FLASHER

#include "cbdos/flasher.hpp"

namespace cbdos {
namespace flasher {

static std::vector<FlasherPreset> s_s3Presets = {
    {
        "usb_cdc_native",
        "🔌 USB-Serial Nativo (ESP32-C3/S3)",
        "Flasheo directo por cable USB usando USB-Serial/JTAG de fabrica.",
        "+---------------------------------------------+\n"
        "|  CONEXION DIRECTA POR CABLE USB             |\n"
        "+---------------------------------------------+\n"
        "| [Cable USB] Conectar objetivo al puerto USB.|\n"
        "| [Auto-Boot] Control de bootloader por DTR/RTS|\n"
        "+---------------------------------------------+",
        {FlasherTransport::USB_CDC_NATIVE, -1, -1, -1, -1, 115200, 0x0, "/sdcard/firmware.bin", "USB-Serial Nativo"},
        false
    },
    {
        "s3_external",
        "ESP Externo (JC3248W535 S3)",
        "Flasheo desde tarjeta S3 usando pines libres.",
        "+---------------------------------------------+\n"
        "|  CONEXIONES S3 HACIA ESP EXTERNO            |\n"
        "+---------------------------------------------+\n"
        "| [UART TX]  GPIO 15        -> Target RX      |\n"
        "| [UART RX]  GPIO 16        -> Target TX      |\n"
        "| [BOOT IO0] GPIO 0         -> Target GPIO 0  |\n"
        "| [RESET]    Boton Manual   -> Target EN / RST|\n"
        "| [FIRMWARE] MicroSD        -> /sdcard/*.bin  |\n"
        "+---------------------------------------------+",
        {FlasherTransport::UART_PINS, 15, 16, 0, -1, 115200, 0x0, "/sdcard/firmware.bin", "ESP Externo (JC3248W535 S3)"},
        false
    },
    {
        "p4_c6_internal",
        "ESP32-C6 Coprocesador (JC4880)",
        "Coprocesador WiFi 6 / BT 5 (Solo placa P4).",
        "+---------------------------------------------+\n"
        "| Preset exclusivo de la placa ESP32-P4.      |\n"
        "+---------------------------------------------+",
        {FlasherTransport::UART_PINS, 32, 28, 34, 54, 115200, 0x0, "/sdcard/c6_slave.bin", "ESP32-C6 Coprocesador"},
        false
    },
    {
        "custom",
        "Personalizado / Manual",
        "Configuracion libre de GPIOs, baudrate y firmware.",
        "+---------------------------------------------+\n"
        "| 1. Conectar Host TX al pin RX del objetivo. |\n"
        "| 2. Conectar Host RX al pin TX del objetivo. |\n"
        "| 3. Conectar Host BOOT al pin GPIO0/IO9.     |\n"
        "| 4. Conectar Host RST al pin Reset/EN.       |\n"
        "| 5. Conectar masa GND comun entre placas.    |\n"
        "+---------------------------------------------+",
        {FlasherTransport::UART_PINS, 15, 16, 0, -1, 115200, 0x0, "/sdcard/firmware.bin", "Personalizado / Manual"},
        false
    }
};

bool isSupported() {
    return true;
}

bool isBusy() {
    return false;
}

const std::vector<FlasherPreset>& getPresets() {
    return s_s3Presets;
}

FlasherConfig getDefaultConfig() {
    if (!s_s3Presets.empty()) {
        return s_s3Presets[0].config;
    }
    return FlasherConfig();
}

bool startFlash(const FlasherConfig& config, FlasherProgressCb progressCb) {
    if (progressCb) {
        progressCb(FlasherStatus::Failed, 0, "Flasheador hardware UART en S3 en desarrollo");
    }
    return false;
}

bool startFlash(FlasherProgressCb progressCb) {
    return startFlash(getDefaultConfig(), progressCb);
}

} // namespace flasher
} // namespace cbdos

#endif // CBDOS_FEATURE_FLASHER

