#include "hal_usb_hid_host_p4.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <usb/hid_host.h>
#include <usb/hid_usage_keyboard.h>
#include <cstring>
#include <lvgl.h>

static const char* TAG = "HAL_USB_HID_P4";

namespace cbdos {
namespace bsp {

namespace {

static QueueHandle_t s_kbQueue = nullptr;
static volatile bool s_keyboardConnected = false;
static hid_host_device_handle_t s_currentDev = nullptr;
static uint8_t s_prevKeys[6] = {0};
static uint8_t s_prevMods = 0;

// Estado de bloqueos del teclado
static bool s_capsLock = false;
static bool s_numLock  = true; // NumLock activo por defecto para habilitar números en el pad

static void syncKeyboardLeds() {
    if (!s_currentDev) return;
    uint8_t leds = 0;
    if (s_numLock)  leds |= 0x01; // Bit 0: NumLock LED
    if (s_capsLock) leds |= 0x02; // Bit 1: CapsLock LED
    hid_class_request_set_report(s_currentDev, 0x02 /* HID_REPORT_TYPE_OUTPUT */, 0, &leds, 1);
}

static uint32_t hid_to_lvkey(uint8_t code, uint8_t mods) {
    bool shift = (mods & 0x22) != 0; // LShift (0x02) o RShift (0x20)
    bool ctrl  = (mods & 0x11) != 0; // LCtrl (0x01) o RCtrl (0x10)

    // Letras 'a'..'z' (Usage 0x04..0x1D)
    if (code >= 0x04 && code <= 0x1D) {
        // Atajo Ctrl+C: generar 0x03 (ETX para abortar procesos en consola/SSH)
        if (ctrl && code == 0x06) {
            return 0x03;
        }
        char c = static_cast<char>('a' + (code - 0x04));
        bool upper = shift ^ s_capsLock; // XOR: Shift invierte CapsLock
        return upper ? static_cast<uint32_t>(c - 32) : static_cast<uint32_t>(c);
    }

    // Fila numérica principal '1'..'9' (Usage 0x1E..0x26)
    if (code >= 0x1E && code <= 0x26) {
        static const char* sh = "!@#$%^&*(";
        return shift ? static_cast<uint32_t>(sh[code - 0x1E])
                     : static_cast<uint32_t>('1' + (code - 0x1E));
    }

    // Tecla '0' (Usage 0x27)
    if (code == 0x27) {
        return shift ? ')' : '0';
    }

    // Teclas del Teclado Numérico dedicado (Numpad Usage 0x54..0x63)
    switch (code) {
        case 0x54: return '/';
        case 0x55: return '*';
        case 0x56: return '-';
        case 0x57: return '+';
        case 0x58: return LV_KEY_ENTER;
        case 0x59: return s_numLock ? '1' : (uint32_t)LV_KEY_END;
        case 0x5A: return s_numLock ? '2' : (uint32_t)LV_KEY_DOWN;
        case 0x5B: return s_numLock ? '3' : (uint32_t)LV_KEY_NEXT; // PageDown
        case 0x5C: return s_numLock ? '4' : (uint32_t)LV_KEY_LEFT;
        case 0x5D: return s_numLock ? '5' : 0;
        case 0x5E: return s_numLock ? '6' : (uint32_t)LV_KEY_RIGHT;
        case 0x5F: return s_numLock ? '7' : (uint32_t)LV_KEY_HOME;
        case 0x60: return s_numLock ? '8' : (uint32_t)LV_KEY_UP;
        case 0x61: return s_numLock ? '9' : (uint32_t)LV_KEY_PREV; // PageUp
        case 0x62: return s_numLock ? '0' : (uint32_t)LV_KEY_DEL;  // Insert
        case 0x63: return s_numLock ? '.' : (uint32_t)LV_KEY_DEL;  // Delete
        default: break;
    }

    // Teclas de control, navegación LVGL y símbolos estándar
    switch (code) {
        case 0x28: return LV_KEY_ENTER;
        case 0x29: return LV_KEY_ESC;
        case 0x2A: return LV_KEY_BACKSPACE;
        case 0x2B: return LV_KEY_NEXT;        // Tabulador
        case 0x2C: return ' ';                 // Espacio
        case 0x2D: return shift ? '_' : '-';
        case 0x2E: return shift ? '+' : '=';
        case 0x2F: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x31: return shift ? '|' : '\\';
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x35: return shift ? '~' : '`';
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
        case 0x4A: return LV_KEY_HOME;
        case 0x4C: return LV_KEY_DEL;
        case 0x4D: return LV_KEY_END;
        case 0x4F: return LV_KEY_RIGHT;
        case 0x50: return LV_KEY_LEFT;
        case 0x51: return LV_KEY_DOWN;
        case 0x52: return LV_KEY_UP;
        default:   return 0;
    }
}

static void postKeyEvent(uint32_t key, bool pressed) {
    if (!s_kbQueue || key == 0) return;
    KeyboardEvent ev{key, pressed};
    xQueueSend(s_kbQueue, &ev, 0);
}

static void clearKeyboardState() {
    for (int i = 0; i < 6; i++) {
        if (s_prevKeys[i] != 0) {
            uint32_t k = hid_to_lvkey(s_prevKeys[i], s_prevMods);
            if (k) postKeyEvent(k, false);
            s_prevKeys[i] = 0;
        }
    }
    s_prevMods = 0;
    s_keyboardConnected = false;
}

static void processBootKeyboardReport(const uint8_t* report, size_t len) {
    if (len < 8) return;
    // Rollover / Phantom state: ignorar
    if (report[2] == 0x01) return;

    uint8_t mods = report[0];
    const uint8_t* newKeys = report + 2;

    // 1. Detectar teclas liberadas
    for (int i = 0; i < 6; i++) {
        uint8_t oldKey = s_prevKeys[i];
        if (oldKey == 0) continue;
        bool stillPressed = false;
        for (int j = 0; j < 6; j++) {
            if (newKeys[j] == oldKey) {
                stillPressed = true;
                break;
            }
        }
        if (!stillPressed) {
            uint32_t k = hid_to_lvkey(oldKey, mods);
            if (k) postKeyEvent(k, false);
        }
    }

    // 2. Detectar teclas recién presionadas
    for (int i = 0; i < 6; i++) {
        uint8_t key = newKeys[i];
        if (key == 0) continue;
        bool wasPressed = false;
        for (int j = 0; j < 6; j++) {
            if (s_prevKeys[j] == key) {
                wasPressed = true;
                break;
            }
        }
        if (!wasPressed) {
            // Manejo de CapsLock toggle (Usage 0x39)
            if (key == 0x39) {
                s_capsLock = !s_capsLock;
                syncKeyboardLeds();
                continue;
            }

            // Manejo de NumLock toggle (Usage 0x53)
            if (key == 0x53) {
                s_numLock = !s_numLock;
                syncKeyboardLeds();
                continue;
            }

            uint32_t k = hid_to_lvkey(key, mods);
            if (k) {
                postKeyEvent(k, true);
            }
        }
    }

    memcpy(s_prevKeys, newKeys, 6);
    s_prevMods = mods;
}

static void usb_hid_interface_cb(hid_host_device_handle_t hid_dev,
                                const hid_host_interface_event_t event,
                                void* arg) {
    uint8_t buf[64];
    size_t len = 0;

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            if (hid_host_device_get_raw_input_report_data(hid_dev, buf, sizeof(buf), &len) == ESP_OK) {
                if (len >= 8) {
                    processBootKeyboardReport(buf, len);
                }
            }
            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⌨️ Teclado USB desconectado de la interfaz");
            clearKeyboardState();
            hid_host_device_close(hid_dev);
            s_currentDev = nullptr;
            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGE(TAG, "Error de transferencia en interfaz USB HID");
            break;
        default:
            break;
    }
}

static void usb_hid_device_cb(hid_host_device_handle_t hid_dev,
                              const hid_host_driver_event_t event,
                              void* arg) {
    hid_host_dev_params_t params;
    if (hid_host_device_get_params(hid_dev, &params) != ESP_OK) {
        return;
    }

    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE && params.proto == HID_PROTOCOL_KEYBOARD) {
            const hid_host_device_config_t dev_cfg = {
                .callback = usb_hid_interface_cb,
                .callback_arg = NULL,
            };

            if (hid_host_device_open(hid_dev, &dev_cfg) == ESP_OK) {
                // Iniciar transferencia de datos de interrupción inmediatamente (sin esperar control transfer bloqueante)
                hid_host_device_start(hid_dev);
                s_currentDev = hid_dev;
                s_keyboardConnected = true;

                // Sincronizar estado inicial de LEDs físicos (NumLock activo por defecto)
                syncKeyboardLeds();

                ESP_LOGI(TAG, "⌨️ Teclado USB HID vinculado correctamente (NumLock=%s, CapsLock=%s)",
                         s_numLock ? "ON" : "OFF", s_capsLock ? "ON" : "OFF");
            } else {
                ESP_LOGE(TAG, "Fallo al abrir dispositivo USB HID de teclado");
            }
        }
    }
}

} // anonymous namespace

bool initUsbHidHostP4() {
    if (!s_kbQueue) {
        s_kbQueue = xQueueCreate(32, sizeof(KeyboardEvent));
    }

    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = usb_hid_device_cb,
        .callback_arg = NULL,
    };

    esp_err_t err = hid_host_install(&hid_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Error fatal instalando driver HID Host: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Driver USB HID Host P4 instalado con éxito");
    return true;
}

void deinitUsbHidHostP4() {
    clearKeyboardState();
    if (s_currentDev) {
        hid_host_device_close(s_currentDev);
        s_currentDev = nullptr;
    }
    hid_host_uninstall();
    if (s_kbQueue) {
        vQueueDelete(s_kbQueue);
        s_kbQueue = nullptr;
    }
}

QueueHandle_t getUsbKeyboardQueue() {
    return s_kbQueue;
}

bool isUsbKeyboardConnected() {
    return s_keyboardConnected;
}

} // namespace bsp
} // namespace cbdos
