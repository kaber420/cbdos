#include "cbdos/hid.hpp"
#include "cbdos/system.hpp"
#include "cbdos/usb_manager.hpp"
#include "security/kerberos/KerberosManager.hpp"
#include <Arduino.h>

#if defined(CONFIG_TINYUSB_ENABLED) || defined(ARDUINO_USB_MODE)
#include <USB.h>
#include <USBHID.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>

namespace {

// Descriptor FIDO2 / CTAPHID puro (Modo UsbMode::Fido)
// Oficial FIDO Alliance sin Report ID: exactamente 64B IN / 64B OUT
static const uint8_t s_s3FidoReportDesc[] = {
    0x06, 0xD0, 0xF1, // Usage Page (FIDO Alliance 0xF1D0)
    0x09, 0x01,       // Usage (CTAPHID)
    0xA1, 0x01,       // Collection (Application)
    0x09, 0x20,       //   Usage (Input Report Data)
    0x15, 0x00,       //   Logical Min 0
    0x26, 0xFF, 0x00, //   Logical Max 255
    0x75, 0x08,       //   Report Size 8
    0x95, 0x40,       //   Report Count 64 (0x40)
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0x09, 0x21,       //   Usage (Output Report Data)
    0x15, 0x00,       //   Logical Min 0
    0x26, 0xFF, 0x00, //   Logical Max 255
    0x75, 0x08,       //   Report Size 8
    0x95, 0x40,       //   Report Count 64 (0x40)
    0x91, 0x02,       //   Output (Data,Var,Abs)
    0xC0              // End Collection
};

#define KH_QN 16
static uint8_t s_q[KH_QN][64];
static volatile uint8_t s_qhead = 0, s_qtail = 0;

// Dispositivo USB FIDO2 / CTAPHID puro
class USBHIDS3Fido : public USBHIDDevice {
public:
    void init() {
        static bool s_added = false;
        if (!s_added) {
            s_added = true;
            USBHID::addDevice(this, sizeof(s_s3FidoReportDesc));
        }
        m_hid.begin();

        cbdos::security::KerberosManager::instance().init();
        cbdos::security::KerberosManager::instance().setUsbSendCallback([this](const uint8_t packet[64]) {
            sendFidoReport(packet);
        });
    }

    bool ready() {
        return m_hid.ready();
    }

    uint16_t _onGetDescriptor(uint8_t* dst) override {
        memcpy(dst, s_s3FidoReportDesc, sizeof(s_s3FidoReportDesc));
        return sizeof(s_s3FidoReportDesc);
    }

    void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        (void)report_id;
        if (buffer && len >= 64) {
            uint8_t next = (uint8_t)((s_qhead + 1) % KH_QN);
            if (next != s_qtail) {
                memcpy(s_q[s_qhead], buffer, 64);
                s_qhead = next;
            }
        }
    }

    void _onSetFeature(uint8_t report_id, const uint8_t* buffer, uint16_t len) override {
        _onOutput(report_id, buffer, len);
    }

    bool sendFidoReport(const uint8_t report[64]) {
        for (int i = 0; i < 200; i++) {
            if (m_hid.ready() && m_hid.SendReport(0, report, 64)) {
                return true;
            }
            delay(1);
        }
        return false;
    }

private:
    USBHID m_hid;
};

// Acceso lazy mediante singletons locales para evitar registrar endpoints
// de modos no seleccionados al arranque
static USBHIDKeyboard& getKeyboard() {
    static USBHIDKeyboard s_keyboard;
    return s_keyboard;
}

static USBHIDMouse& getMouse() {
    static USBHIDMouse s_mouse;
    return s_mouse;
}

static USBHIDS3Fido& getFidoDevice() {
    static USBHIDS3Fido s_fido;
    return s_fido;
}

} // namespace

void cbdos_hid_s3_poll() {
    while (s_qtail != s_qhead) {
        uint8_t* pkt = s_q[s_qtail];
        s_qtail = (uint8_t)((s_qtail + 1) % KH_QN);
        cbdos::security::KerberosManager::instance().handleIncomingUsbReport(pkt);
    }
}

namespace {

class Esp32S3HidDriver : public cbdos::hid::IHidDriver {
public:
    Esp32S3HidDriver() : m_ledState(0), m_enabled(false) {}

    // Arranque Offline-First: m_enabled inicia en false.
    // Prohibido iniciar la pila USB hardware síncronamente al boot.
    bool init() {
        m_enabled = false;
        return true;
    }

    bool enable() override {
        if (m_enabled) return true;

        auto mode = cbdos::usb::UsbManager::getInstance().getBootMode();
        if (mode == cbdos::usb::UsbMode::Fido) {
            getFidoDevice().init();
            USB.productName("KERBEROS FIDO2 Security Key");
            USB.manufacturerName("CBDos / Poseidon");
            USB.begin();
        } else if (mode == cbdos::usb::UsbMode::Hid) {
            getKeyboard().begin();
            getMouse().begin();

            getKeyboard().onEvent(ARDUINO_USB_HID_KEYBOARD_LED_EVENT, [](void* arg, esp_event_base_t base, int32_t id, void* data) {
                if (data) {
                    auto* d = (arduino_usb_hid_keyboard_event_data_t*)data;
                    auto* drv = (Esp32S3HidDriver*)arg;
                    if (drv) drv->updateLedState(d->leds);
                }
            });

            USB.productName("CBDos Composite HID (Keyboard/Mouse)");
            USB.manufacturerName("CBDos / Poseidon");
            USB.begin();
        }

        m_enabled = true;
        return true;
    }

    bool disable() override {
        if (!m_enabled) return true;
        auto mode = cbdos::usb::UsbManager::getInstance().getBootMode();
        if (mode == cbdos::usb::UsbMode::Hid) {
            getKeyboard().end();
            getMouse().end();
        }
        m_enabled = false;
        return true;
    }

    bool isEnabled() const override {
        return m_enabled;
    }

    bool isConnected() override {
        return m_enabled && (bool)USB;
    }

    bool isReady() override {
        if (!m_enabled) return false;
        if (cbdos::usb::UsbManager::getInstance().getBootMode() == cbdos::usb::UsbMode::Fido) {
            return getFidoDevice().ready();
        }
        return (bool)USB;
    }

    void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override {
        if (!m_enabled || cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Hid) return;
        KeyReport kr;
        kr.modifiers = modifiers;
        kr.reserved = 0;
        memcpy(kr.keys, keycodes, 6);
        getKeyboard().sendReport(&kr);
    }

    void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override {
        if (!m_enabled || cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Hid) return;
        if (x != 0 || y != 0 || wheel != 0) {
            getMouse().move(x, y, wheel);
        }
        if (buttons & cbdos::hid::MOUSE_BTN_LEFT) getMouse().press(MOUSE_LEFT);
        else getMouse().release(MOUSE_LEFT);

        if (buttons & cbdos::hid::MOUSE_BTN_RIGHT) getMouse().press(MOUSE_RIGHT);
        else getMouse().release(MOUSE_RIGHT);

        if (buttons & cbdos::hid::MOUSE_BTN_MIDDLE) getMouse().press(MOUSE_MIDDLE);
        else getMouse().release(MOUSE_MIDDLE);
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

static Esp32S3HidDriver s_s3HidDriver;

} // namespace

#else

namespace {

class DummyS3HidDriver : public cbdos::hid::IHidDriver {
public:
    bool enable() override { return false; }
    bool disable() override { return false; }
    bool isEnabled() const override { return false; }
    bool isConnected() override { return false; }
    bool isReady() override { return false; }
    void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override { (void)modifiers; (void)keycodes; }
    void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override { (void)buttons; (void)x; (void)y; (void)wheel; }
    uint8_t getHostLedState() override { return 0; }
};

static DummyS3HidDriver s_s3HidDriver;

} // namespace

#endif

namespace cbdos {
namespace bsp {

void initHidDriverS3() {
#if defined(CONFIG_TINYUSB_ENABLED) || defined(ARDUINO_USB_MODE)
    s_s3HidDriver.init();
#endif
    cbdos::hid::registerDriver(&s_s3HidDriver);
}

} // namespace bsp
} // namespace cbdos
