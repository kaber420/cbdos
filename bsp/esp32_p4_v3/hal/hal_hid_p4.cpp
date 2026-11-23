#include "cbdos/hid.hpp"
#include "cbdos/system.hpp"
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

namespace cbdos {
namespace bsp {

void initHidDriverP4() {
    s_p4HidDriver.init();
    ::cbdos::hid::registerDriver(&s_p4HidDriver);
    ESP_LOGI(TAG, "Driver USB HID para ESP32-P4 registrado y activo en cbdos::hid");
}

} // namespace bsp
} // namespace cbdos

