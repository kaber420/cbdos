#include "cbdos/hid.hpp"
#include "cbdos/system.hpp"
#include "cbdos/board_identity.hpp"
#include "cbdos/tts.hpp"
#include <esp_log.h>
#include <tinyusb.h>
#include <class/hid/hid_device.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstring>

static const char* TAG = "HAL_HID_P4";

namespace {

// Descriptor de reporte HID compuesto: Keyboard (Report ID 1) + Mouse (Report ID 2)
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE))
};

#define EPNUM_HID   0x81
#define TUD_HID_CONFIG_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t hid_configuration_descriptor[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_HID_CONFIG_DESC_LEN, 0, 100),
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_report_descriptor), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5)
};

class Esp32P4HidDriver : public cbdos::hid::IHidDriver {
public:
    Esp32P4HidDriver() : m_ledState(0), m_enabled(false) {}

    bool init() {
        m_enabled = false;
        ESP_LOGI(TAG, "Driver USB HID registrado (inactivo por defecto)");
        return true;
    }

    bool enable() override {
        if (m_enabled) return true;

        ESP_LOGI(TAG, "Activando stack TinyUSB Device HID...");

        const tinyusb_config_t tusb_cfg = {
            .device_descriptor = NULL,
            .string_descriptor = NULL,
            .string_descriptor_count = 0,
            .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
            .fs_configuration_descriptor = hid_configuration_descriptor,
            .hs_configuration_descriptor = hid_configuration_descriptor,
            .qualifier_descriptor = NULL,
#else
            .configuration_descriptor = hid_configuration_descriptor,
#endif
            .self_powered = false,
            .vbus_monitor_io = 0
        };

        esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "tinyusb_driver_install fallo con error: %s", esp_err_to_name(ret));
            return false;
        }

        ESP_LOGI(TAG, "tinyusb_driver_install OK. Habilitando conexión D+/D- (tud_connect)...");
        tud_connect();

        m_enabled = true;
        return true;
    }

    bool disable() override {
        if (!m_enabled) return true;

        ESP_LOGI(TAG, "Desconectando y desactivando stack TinyUSB Device HID...");
        tud_disconnect();
        tinyusb_driver_uninstall();

        m_enabled = false;
        return true;
    }

    bool isEnabled() const override {
        return m_enabled;
    }

    bool isConnected() override {
        return m_enabled && (tud_mounted() || tud_connected());
    }

    bool isReady() override {
        return m_enabled && tud_hid_ready();
    }

    void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override {
        if (!m_enabled) return;
        if (!tud_hid_ready()) {
            cbdos::system::sleepMs(5);
            if (!tud_hid_ready()) return;
        }

        hid_keyboard_report_t report;
        report.modifier = modifiers;
        report.reserved = 0;
        memcpy(report.keycode, keycodes, 6);

        tud_hid_report(REPORT_ID_KEYBOARD, &report, sizeof(report));
    }

    void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override {
        if (!m_enabled) return;
        if (!tud_hid_ready()) {
            cbdos::system::sleepMs(5);
            if (!tud_hid_ready()) return;
        }

        hid_mouse_report_t report;
        report.buttons = buttons;
        report.x = x;
        report.y = y;
        report.wheel = wheel;
        report.pan = 0;

        tud_hid_report(REPORT_ID_MOUSE, &report, sizeof(report));
    }

    uint8_t getHostLedState() override {
        return m_ledState;
    }

    void updateLedState(uint8_t state) {
        m_ledState = state;
        cbdos::hid::onHostLedStateChanged(state);
    }

private:
    uint8_t m_ledState;
    bool m_enabled;
};

static Esp32P4HidDriver s_p4HidDriver;

} // namespace

// Invocado por TinyUSB cuando el Host envía un SET_REPORT (Control de LEDs de teclado)
extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type,
                                      uint8_t const* buffer, uint16_t bufsize) {
    (void)instance;
    if (report_type == HID_REPORT_TYPE_OUTPUT && report_id == REPORT_ID_KEYBOARD && bufsize >= 1) {
        uint8_t led_status = buffer[0];
        s_p4HidDriver.updateLedState(led_status);
        ESP_LOGD(TAG, "Host LED SET_REPORT: 0x%02X (Num:%d Caps:%d Scroll:%d)",
                 led_status,
                 (led_status & cbdos::hid::LED_NUMLOCK) ? 1 : 0,
                 (led_status & cbdos::hid::LED_CAPSLOCK) ? 1 : 0,
                 (led_status & cbdos::hid::LED_SCROLLLOCK) ? 1 : 0);
    }
}

// Invocado por TinyUSB para obtener el descriptor de reporte
extern "C" uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}

// Invocado por TinyUSB cuando el host pide GET_REPORT
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                         hid_report_type_t report_type,
                                         uint8_t* buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// ============================================================================
// CONSOLA INTERACTIVA DE DEBUG Y CONTROL EN CALIENTE POR SERIAL (/dev/ttyACM0)
// Cambiar a 0 o comentar para deshabilitar en producción
// ============================================================================
#define ENABLE_CBDOS_SERIAL_DEBUG_CLI 1

#if ENABLE_CBDOS_SERIAL_DEBUG_CLI
#include "cbdos/ducky.hpp"
#include "../../../core/src/lua/LuaEngine.hpp"
#include "usb_cdc_loader_port.hpp"
#include "usb_device_manager.hpp"
#include <esp_loader_io.h>
#include <driver/usb_serial_jtag.h>

static void serial_interactive_cli_task(void* arg) {
    (void)arg;
    
    // Configurar driver USB-Serial-JTAG para lectura no bloqueante
    usb_serial_jtag_driver_config_t usb_s_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 2048,
    };
    usb_serial_jtag_driver_install(&usb_s_cfg);
    
    ESP_LOGI(TAG, "=== CONSOLA SERIAL EN CALIENTE LISTA ===");
    ESP_LOGI(TAG, "Puedes enviar Lua (ej: hid.type('ls\\n')) o DuckyScript (ej: ducky: STRING hola)");
    
    std::string line_buf;
    uint8_t ch;
    
    while (1) {
        int read_bytes = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(50));
        if (read_bytes > 0) {
            if (ch == '\r' || ch == '\n') {
                if (!line_buf.empty()) {
                    printf("\n[SERIAL_CLI_IN] %s\n", line_buf.c_str());
                    
                    if (line_buf.rfind("ducky:", 0) == 0) {
                        std::string duckyCmd = line_buf.substr(6);
                        while (!duckyCmd.empty() && duckyCmd.front() == ' ') duckyCmd.erase(0, 1);
                        ::cbdos::ducky::DuckyInterpreter::getInstance().loadFromString(duckyCmd);
                        ::cbdos::ducky::DuckyInterpreter::getInstance().run();
                        while (::cbdos::ducky::DuckyInterpreter::getInstance().getState() == ::cbdos::ducky::ExecutionState::Running) {
                            ::cbdos::ducky::DuckyInterpreter::getInstance().step();
                            vTaskDelay(pdMS_TO_TICKS(5));
                        }
                        printf("[SERIAL_CLI_OUT] Ducky ejecutado OK.\n");
                    } else if (line_buf == "usb: status" || line_buf == "usb: info") {
                        auto& mgr = ::cbdos::usb::UsbDeviceManager::getInstance();
                        if (!mgr.isDeviceConnected()) {
                            printf("[SERIAL_CLI_OUT] 🔌 Puerto USB OTG Libre: Ningún dispositivo conectado físicamente.\n");
                        } else {
                            const auto* dev = mgr.getActiveDevice();
                            printf("[SERIAL_CLI_OUT] ⚡ Dispositivo USB Conectado:\n");
                            printf("  Fabricante:  %s\n", dev->manufacturer);
                            printf("  Producto:    %s\n", dev->product);
                            printf("  VID:PID:     0x%04X : 0x%04X\n", dev->vid, dev->pid);
                            printf("  Clase:       %d\n", (int)dev->devClass);
                        }
                    } else if (line_buf == "c3: status" || line_buf == "radio: probe" || line_buf == "c3: probe") {
                        printf("[SERIAL_CLI_OUT] 🔍 Sondeando módem ESP32-C3 en puerto USB OTG High-Speed...\n");
                        esp_loader_error_t err = loader_port_usb_cdc_init(1500);
                        if (err != ESP_LOADER_SUCCESS) {
                            printf("[SERIAL_CLI_ERR] ❌ No se detectó dispositivo USB en el puerto OTG (err=%d)\n", err);
                        } else {
                            // 1. Drenar buffer previo
                            uint8_t trash[128];
                            while (loader_port_read(trash, sizeof(trash), 10) == ESP_LOADER_SUCCESS);

                            // 2. Enviar trama binaria GET_STATUS con CRC8 correcto (0x5E)
                            uint8_t get_status_frame[] = { 0xAA, 0x55, 0x03, 0x00, 0x01, 0x01, 0x5E };
                            loader_port_write(get_status_frame, sizeof(get_status_frame), 500);
                            
                            // 3. Buscar magic bytes 0xAA 0x55
                            bool synced = false;
                            uint8_t b = 0;
                            int tries = 0;
                            while (tries++ < 50) {
                                if (loader_port_read(&b, 1, 50) == ESP_LOADER_SUCCESS && b == 0xAA) {
                                    if (loader_port_read(&b, 1, 50) == ESP_LOADER_SUCCESS && b == 0x55) {
                                        synced = true;
                                        break;
                                    }
                                }
                            }

                            if (synced) {
                                uint8_t dir = 0, len_h = 0, len_l = 0;
                                loader_port_read(&dir, 1, 50);
                                loader_port_read(&len_h, 1, 50);
                                loader_port_read(&len_l, 1, 50);
                                uint16_t plen = (len_h << 8) | len_l;
                                if (plen > 0 && plen < 128) {
                                    uint8_t p[128] = {0};
                                    loader_port_read(p, plen, 200);
                                    uint8_t crc = 0;
                                    loader_port_read(&crc, 1, 50);

                                    if (dir == 0x04 && plen >= 11 && p[0] == 0x01 && p[1] == 0x00) {
                                        char mac_str[24];
                                        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", p[2], p[3], p[4], p[5], p[6], p[7]);
                                        const char* mode_str = (p[8] == 2) ? "Long Range (LR) 🚀" : "Normal (802.11 b/g/n) ⚡";
                                        uint8_t chan = p[9];
                                        float pwr = p[10] * 0.25f;
                                        uint8_t peers = p[11];
                                        char alias_str[32] = {0};
                                        if (plen > 11) {
                                            size_t alen = plen - 11;
                                            if (alen > 31) alen = 31;
                                            memcpy(alias_str, p + 11, alen);
                                            alias_str[alen] = '\0';
                                        } else {
                                            strcpy(alias_str, "N/A");
                                        }
                                        printf("\n======================================================\n");
                                        printf("  🛰️ [P4 USB Host] ESP32-C3 MÓDEM DE RADIO ENLACE OK!\n");
                                        printf("======================================================\n");
                                        printf("  Nodo Alias:     %s\n", alias_str);
                                        printf("  MAC Hardware:   %s\n", mac_str);
                                        printf("  Modo Radio:     %s\n", mode_str);
                                        printf("  Canal Activo:   Canal %u\n", chan);
                                        printf("  Potencia TX:    %.2f dBm\n", pwr);
                                        printf("  Peers en Aire:  %u\n", peers);
                                        printf("======================================================\n\n");
                                    } else {
                                        printf("[SERIAL_CLI_ERR] Formato de payload no reconocido (dir=0x%02X len=%u)\n", dir, plen);
                                    }
                                }
                            } else {
                                printf("[SERIAL_CLI_ERR] ⚠️ Timeout esperando sincronización de trama 0xAA 0x55 del C3\n");
                            }
                        }
                    } else if (line_buf == "c3: ping") {
                        printf("[SERIAL_CLI_OUT] 📡 Emitiendo paquete de radio ESP-NOW al aire a través del C3...\n");
                        // Trama de paquete de radio: DIR_PC_TO_DONGLE (0x01)
                        uint8_t ping_data[] = { 0x01, 0x00, 0xAA, 0x55, 'P', '4', '_', 'R', 'A', 'D', 'I', 'O' };
                        // Magic (2B) + DIR (1B) + Len (2B) + Payload + CRC8 (1B)
                        uint8_t tx_frame[32];
                        tx_frame[0] = 0xAA;
                        tx_frame[1] = 0x55;
                        tx_frame[2] = 0x01; // DIR_PC_TO_DONGLE
                        tx_frame[3] = 0x00;
                        tx_frame[4] = sizeof(ping_data);
                        memcpy(tx_frame + 5, ping_data, sizeof(ping_data));
                        // Calcular CRC8
                        uint8_t crc = 0;
                        for (size_t i = 0; i < sizeof(ping_data); i++) {
                            uint8_t extract = ping_data[i];
                            for (uint8_t t = 8; t; t--) {
                                uint8_t sum = (crc ^ extract) & 0x01;
                                crc >>= 1;
                                if (sum) crc ^= 0x8C;
                                extract >>= 1;
                            }
                        }
                        tx_frame[5 + sizeof(ping_data)] = crc;
                        loader_port_write(tx_frame, 6 + sizeof(ping_data), 500);
                        printf("[SERIAL_CLI_OUT] ✅ Trama enviada al C3 para emisión por radio ESP-NOW!\n");
                    } else if (line_buf.rfind("tts: ", 0) == 0 || line_buf.rfind("tts ", 0) == 0) {
                        std::string textToSpeak = line_buf.substr(line_buf.find(' ') + 1);
                        printf("[SERIAL_CLI_OUT] 🗣️ Sintetizando voz: \"%s\"...\n", textToSpeak.c_str());
                        bool ok = ::cbdos::tts::speak(textToSpeak);
                        if (ok) {
                            printf("[SERIAL_CLI_OUT] ✅ Texto encolado en PicoTTS.\n");
                        } else {
                            printf("[SERIAL_CLI_ERR] ❌ Error al iniciar síntesis TTS (¿MicroSD insertada con diccionarios?).\n");
                        }
                    } else if (::cbdos::board_identity::isVersionQuery(line_buf)) {
                        // Identidad v1 para el flasheador web (antes de que Lua vea la línea).
                        printf("%s\n", ::cbdos::board_identity::bannerFor(
                            *::cbdos::board_identity::findBoard("jc4880p443")).c_str());
                    } else {
                        std::string outRes;
                        bool ok = ::LuaEngine::getInstance().executeString(line_buf, &outRes);
                        if (ok) {
                            printf("[SERIAL_CLI_OUT] OK: %s\n", outRes.c_str());
                        } else {
                            printf("[SERIAL_CLI_ERR] %s\n", ::LuaEngine::getInstance().getLastError().c_str());
                        }
                    }
                    line_buf.clear();
                }
            } else if (ch >= 32 && ch <= 126) {
                line_buf += (char)ch;
            }
        }
    }
}
#endif

namespace cbdos {
namespace bsp {

void initHidDriverP4() {
    s_p4HidDriver.init();
    ::cbdos::hid::registerDriver(&s_p4HidDriver);
    ESP_LOGI(TAG, "Driver USB HID para ESP32-P4 registrado y activo en cbdos::hid");
    
#if ENABLE_CBDOS_SERIAL_DEBUG_CLI
    // Tarea interactiva serie para pruebas y desarrollo en tiempo real
    xTaskCreatePinnedToCore(serial_interactive_cli_task, "serial_cli_task", 6144, NULL, 3, NULL, 0);
#endif
}

} // namespace bsp
} // namespace cbdos
