#include "usb_device_manager.hpp"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <usb/usb_host.h>
#include <usb/cdc_acm_host.h>
#include <cstring>
#include <algorithm>

static const char* TAG = "CBDOS_USB_MGR";

namespace cbdos {
namespace usb {

namespace {

enum UsbEventType {
    USB_EVT_CONNECTED,
    USB_EVT_DISCONNECTED
};

struct UsbEventMsg {
    UsbEventType type;
    uint16_t vid;
    uint16_t pid;
    uint8_t dev_class;
};

static QueueHandle_t s_usb_queue = nullptr;
static bool s_usb_host_installed = false;
static bool s_cdc_driver_installed = false;
static TaskHandle_t s_host_lib_task_hdl = nullptr;
static TaskHandle_t s_manager_task_hdl = nullptr;

static void usb_host_lib_task(void* arg) {
    while (s_usb_host_installed) {
        uint32_t event_flags = 0;
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), &event_flags);
    }
    vTaskDelete(NULL);
}

static void on_native_new_dev_cb(usb_device_handle_t usb_dev) {
    const usb_device_desc_t *desc = nullptr;
    if (usb_host_get_device_descriptor(usb_dev, &desc) == ESP_OK && desc) {
        ESP_LOGI(TAG, "🔌 Interrupción Hardware: Dispositivo USB detectado -> VID=0x%04X, PID=0x%04X, Clase=0x%02X",
                 desc->idVendor, desc->idProduct, desc->bDeviceClass);

        if (s_usb_queue) {
            UsbEventMsg msg;
            msg.type = USB_EVT_CONNECTED;
            msg.vid = desc->idVendor;
            msg.pid = desc->idProduct;
            msg.dev_class = desc->bDeviceClass;
            xQueueSend(s_usb_queue, &msg, 0);
        }
    }
}

static void cdc_dev_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_arg) {
    switch (event->type) {
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGW(TAG, "🔌 Interrupción Hardware: Dispositivo USB desconectado físicamente");
            if (s_usb_queue) {
                UsbEventMsg msg;
                msg.type = USB_EVT_DISCONNECTED;
                msg.vid = 0;
                msg.pid = 0;
                msg.dev_class = 0;
                xQueueSend(s_usb_queue, &msg, 0);
            }
            break;
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "Error en dispositivo CDC-ACM (err=%d)", event->data.error);
            break;
        default:
            break;
    }
}

static void usb_manager_task(void* arg) {
    UsbDeviceManager* mgr = static_cast<UsbDeviceManager*>(arg);
    UsbEventMsg msg;

    while (1) {
        if (xQueueReceive(s_usb_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.type == USB_EVT_CONNECTED) {
                mgr->handleDeviceConnected(msg.vid, msg.pid, msg.dev_class);
            } else if (msg.type == USB_EVT_DISCONNECTED) {
                mgr->handleDeviceDisconnected();
            }
        }
    }
}

} // anonymous namespace

UsbDeviceManager& UsbDeviceManager::getInstance() {
    static UsbDeviceManager instance;
    return instance;
}

UsbDeviceManager::UsbDeviceManager() = default;
UsbDeviceManager::~UsbDeviceManager() {
    deinit();
}

bool UsbDeviceManager::init() {
    if (m_initialized) return true;

    if (!s_usb_queue) {
        s_usb_queue = xQueueCreate(8, sizeof(UsbEventMsg));
    }

    if (!s_usb_host_installed) {
        const usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo al instalar USB Host Library: %s", esp_err_to_name(err));
            return false;
        }
        s_usb_host_installed = true;
        xTaskCreatePinnedToCore(usb_host_lib_task, "usb_host_lib", 4096, NULL, 5, &s_host_lib_task_hdl, 0);
    }

    if (!s_cdc_driver_installed) {
        const cdc_acm_host_driver_config_t driver_config = {
            .driver_task_stack_size = 4096,
            .driver_task_priority = 5,
            .xCoreID = 0,
            .new_dev_cb = on_native_new_dev_cb, // 👈 Callback nativo de inserción activado
        };
        esp_err_t err = cdc_acm_host_install(&driver_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo al instalar CDC-ACM Host Driver: %s", esp_err_to_name(err));
            return false;
        }
        s_cdc_driver_installed = true;
    }

    if (!s_manager_task_hdl) {
        xTaskCreatePinnedToCore(usb_manager_task, "usb_mgr_task", 4096, this, 4, &s_manager_task_hdl, 0);
    }

    m_initialized = true;
    ESP_LOGI(TAG, "UsbDeviceManager inicializado con soporte Hot-Plug nativo");
    return true;
}

void UsbDeviceManager::deinit() {
    if (!m_initialized) return;
    handleDeviceDisconnected();
    m_initialized = false;
}

bool UsbDeviceManager::registerDriver(IUsbDriver* driver) {
    if (!driver) return false;
    for (auto* d : m_drivers) {
        if (d == driver) return true;
    }
    m_drivers.push_back(driver);
    // Ordenar por prioridad descendente
    std::sort(m_drivers.begin(), m_drivers.end(), [](IUsbDriver* a, IUsbDriver* b) {
        return a->getPriority() > b->getPriority();
    });
    ESP_LOGI(TAG, "Driver USB registrado: '%s' (prioridad=%u)", driver->getDriverName(), driver->getPriority());

    // Si ya hay un dispositivo conectado sin driver asignado, intentar match
    if (m_activeDevice.isConnected && !m_activeDriver) {
        if (driver->match(m_activeDevice)) {
            m_activeDriver = driver;
            driver->onAttach(m_activeDevice);
        }
    }
    return true;
}

bool UsbDeviceManager::unregisterDriver(IUsbDriver* driver) {
    if (!driver) return false;
    if (m_activeDriver == driver) {
        driver->onDetach(m_activeDevice);
        m_activeDriver = nullptr;
    }
    auto it = std::find(m_drivers.begin(), m_drivers.end(), driver);
    if (it != m_drivers.end()) {
        m_drivers.erase(it);
        return true;
    }
    return false;
}

bool UsbDeviceManager::isDeviceConnected() const {
    return m_activeDevice.isConnected;
}

const UsbDeviceInfo* UsbDeviceManager::getActiveDevice() const {
    return m_activeDevice.isConnected ? &m_activeDevice : nullptr;
}

void UsbDeviceManager::registerEventCallback(UsbDeviceEventCallback cb, void* user_ctx) {
    m_eventCb = cb;
    m_eventUserCtx = user_ctx;
}

void UsbDeviceManager::handleDeviceConnected(uint16_t vid, uint16_t pid, uint8_t dev_class) {
    m_activeDevice.vid = vid;
    m_activeDevice.pid = pid;
    m_activeDevice.isConnected = true;
    m_activeDevice.role = DeviceRole::Unassigned;

    // Identificación automática de fabricantes conocidos
    bool isKnownSerial = false;
    if (vid == 0x303A) {
        strncpy(m_activeDevice.manufacturer, "Espressif Systems", sizeof(m_activeDevice.manufacturer) - 1);
        if (pid == 0x1001) {
            strncpy(m_activeDevice.product, "ESP32 USB-Serial-JTAG", sizeof(m_activeDevice.product) - 1);
        } else if (pid == 0x0002) {
            strncpy(m_activeDevice.product, "ESP32-S2 USB CDC", sizeof(m_activeDevice.product) - 1);
        } else {
            strncpy(m_activeDevice.product, "Dispositivo Espressif", sizeof(m_activeDevice.product) - 1);
        }
        isKnownSerial = true;
    } else if (vid == 0x10C4) {
        strncpy(m_activeDevice.manufacturer, "Silicon Labs", sizeof(m_activeDevice.manufacturer) - 1);
        strncpy(m_activeDevice.product, "CP210x UART Bridge", sizeof(m_activeDevice.product) - 1);
        isKnownSerial = true;
    } else if (vid == 0x1A86) {
        strncpy(m_activeDevice.manufacturer, "Winchiphead", sizeof(m_activeDevice.manufacturer) - 1);
        strncpy(m_activeDevice.product, "CH340 Serial Converter", sizeof(m_activeDevice.product) - 1);
        isKnownSerial = true;
    } else if (vid == 0x0403) {
        strncpy(m_activeDevice.manufacturer, "FTDI", sizeof(m_activeDevice.manufacturer) - 1);
        strncpy(m_activeDevice.product, "FT232 / FTDI Serial", sizeof(m_activeDevice.product) - 1);
        isKnownSerial = true;
    } else {
        strncpy(m_activeDevice.manufacturer, "Desconocido", sizeof(m_activeDevice.manufacturer) - 1);
        snprintf(m_activeDevice.product, sizeof(m_activeDevice.product), "USB %04X:%04X", vid, pid);
    }

    if (dev_class == 0x02 || isKnownSerial) {
        m_activeDevice.devClass = DeviceClass::CdcAcm;
    } else if (dev_class == 0xFF) {
        m_activeDevice.devClass = DeviceClass::VendorSpecific;
    } else if (dev_class == 0x08) {
        m_activeDevice.devClass = DeviceClass::MassStorage;
    } else if (dev_class == 0x03) {
        m_activeDevice.devClass = DeviceClass::Hid;
    } else {
        m_activeDevice.devClass = DeviceClass::Unknown;
    }

    ESP_LOGI(TAG, "Hardware Identificado: '%s' - '%s' (VID=0x%04X, PID=0x%04X)",
             m_activeDevice.manufacturer, m_activeDevice.product, vid, pid);

    // Ronda de matching entre drivers registrados
    m_activeDriver = nullptr;
    for (auto* driver : m_drivers) {
        if (driver->match(m_activeDevice)) {
            ESP_LOGI(TAG, "Asignando dispositivo a Driver: '%s'", driver->getDriverName());
            m_activeDriver = driver;
            if (driver->onAttach(m_activeDevice)) {
                break;
            } else {
                m_activeDriver = nullptr;
            }
        }
    }

    if (!m_activeDriver) {
        ESP_LOGI(TAG, "Dispositivo conectado sin driver exclusivo (disponible para uso general)");
    }

    // Notificar observadores / UI
    if (m_eventCb) {
        m_eventCb(m_activeDevice, true, m_eventUserCtx);
    }
}

void UsbDeviceManager::handleDeviceDisconnected() {
    if (!m_activeDevice.isConnected) return;

    ESP_LOGW(TAG, "Liberando recursos de dispositivo desconectado: VID=0x%04X, PID=0x%04X",
             m_activeDevice.vid, m_activeDevice.pid);

    if (m_activeDriver) {
        m_activeDriver->onDetach(m_activeDevice);
        m_activeDriver = nullptr;
    }

    if (m_eventCb) {
        m_eventCb(m_activeDevice, false, m_eventUserCtx);
    }

    m_activeDevice = UsbDeviceInfo();
}

} // namespace usb
} // namespace cbdos
