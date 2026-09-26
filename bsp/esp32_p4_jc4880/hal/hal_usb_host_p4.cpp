#include "hal_usb_host_p4.hpp"
#include "hal_usb_cdc_p4.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <usb/usb_host.h>
#include <usb/cdc_acm_host.h>
#include <cstring>
#include <algorithm>

static const char* TAG = "HAL_USB_HOST_P4";

namespace cbdos {
namespace bsp {

namespace {

enum UsbEvtType {
    EVT_CONNECTED,
    EVT_DISCONNECTED
};

struct UsbEvtMsg {
    UsbEvtType type;
    uint16_t vid;
    uint16_t pid;
    uint8_t dev_class;
};

static P4UsbHostBackend* s_backendInstance = nullptr;

static void on_native_new_dev_cb(usb_device_handle_t usb_dev) {
    const usb_device_desc_t *desc = nullptr;
    if (usb_host_get_device_descriptor(usb_dev, &desc) == ESP_OK && desc) {
        ESP_LOGI(TAG, "🔌 Interrupción Hardware: Dispositivo detectado -> VID=0x%04X, PID=0x%04X, Clase=0x%02X",
                 desc->idVendor, desc->idProduct, desc->bDeviceClass);

        if (s_backendInstance) {
            s_backendInstance->handleDeviceConnected(desc->idVendor, desc->idProduct, desc->bDeviceClass);
        }
    }
}

static void p4_usb_host_lib_task(void* arg) {
    auto* backend = static_cast<P4UsbHostBackend*>(arg);
    while (backend) {
        uint32_t event_flags = 0;
        esp_err_t err = usb_host_lib_handle_events(pdMS_TO_TICKS(10), &event_flags);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "usb_host_lib_handle_events: %s", esp_err_to_name(err));
        }
    }
    vTaskDelete(NULL);
}

} // anonymous namespace

static P4UsbHostBackend s_p4HostBackend;

P4UsbHostBackend* getP4UsbHostBackend() {
    return &s_p4HostBackend;
}

void initUsbHostBackendP4() {
    s_p4HostBackend.init();
}

P4UsbHostBackend::P4UsbHostBackend() {
    m_mutex = xSemaphoreCreateMutex();
    s_backendInstance = this;
}

P4UsbHostBackend::~P4UsbHostBackend() {
    deinit();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    s_backendInstance = nullptr;
}

void P4UsbHostBackend::enterQuarantine(uint32_t durationMs) {
    m_quarantineUntilUs = esp_timer_get_time() + (int64_t)durationMs * 1000;
    ESP_LOGI(TAG, "Activando ventana de cuarentena USB (%u ms)", durationMs);
}

bool P4UsbHostBackend::isQuarantined() const {
    return (esp_timer_get_time() < m_quarantineUntilUs);
}

bool P4UsbHostBackend::init() {
    if (m_initialized) return true;

    cbdos::usb::setUsbHostBackend(this);

    // 1. Instalar la librería central de USB Host (única en todo el sistema)
    if (!m_hostInstalled) {
        usb_host_config_t host_config = {};
        host_config.skip_phy_setup = false;
        host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;

        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Error fatal instalando USB Host Library: %s", esp_err_to_name(err));
            return false;
        }
        m_hostInstalled = true;
        xTaskCreatePinnedToCore(p4_usb_host_lib_task, "usb_host_p4_lib", 4096, this, 5, &m_hostLibTaskHdl, 0);
    }

    // 2. Instalar el driver CDC-ACM Host unificado
    if (!m_cdcDriverInstalled) {
        cdc_acm_host_driver_config_t driver_config = {};
        driver_config.driver_task_stack_size = 4096;
        driver_config.driver_task_priority = 5;
        driver_config.xCoreID = 0;
        driver_config.new_dev_cb = on_native_new_dev_cb;
        esp_err_t err = cdc_acm_host_install(&driver_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Error fatal instalando CDC-ACM Driver: %s", esp_err_to_name(err));
            return false;
        }
        m_cdcDriverInstalled = true;
    }

    // 3. Registrar el canal CDC como driver USB predeterminado
    registerDriver(getP4UsbCdcChannel());

    m_initialized = true;
    ESP_LOGI(TAG, "HAL USB Host P4 inicializado con éxito");
    return true;
}

void P4UsbHostBackend::deinit() {
    if (!m_initialized) return;
    handleDeviceDisconnected();
    m_initialized = false;
}

bool P4UsbHostBackend::registerDriver(cbdos::usb::IUsbDriver* driver) {
    if (!driver) return false;
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return false;

    for (auto* d : m_drivers) {
        if (d == driver) {
            xSemaphoreGive(m_mutex);
            return true;
        }
    }

    m_drivers.push_back(driver);
    std::sort(m_drivers.begin(), m_drivers.end(), [](cbdos::usb::IUsbDriver* a, cbdos::usb::IUsbDriver* b) {
        return a->getPriority() > b->getPriority();
    });

    ESP_LOGI(TAG, "Driver USB registrado en HAL: '%s' (prio=%u)", driver->getDriverName(), driver->getPriority());

    // Si ya hay un dispositivo conectado sin driver asignado, intentar match
    if (m_activeDevice.isConnected && !m_activeDriver) {
        if (driver->match(m_activeDevice)) {
            m_activeDriver = driver;
            driver->onAttach(m_activeDevice);
        }
    }

    xSemaphoreGive(m_mutex);
    return true;
}

bool P4UsbHostBackend::unregisterDriver(cbdos::usb::IUsbDriver* driver) {
    if (!driver) return false;
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return false;

    if (m_activeDriver == driver) {
        driver->onDetach(m_activeDevice);
        m_activeDriver = nullptr;
    }

    auto it = std::find(m_drivers.begin(), m_drivers.end(), driver);
    if (it != m_drivers.end()) {
        m_drivers.erase(it);
        xSemaphoreGive(m_mutex);
        return true;
    }

    xSemaphoreGive(m_mutex);
    return false;
}

bool P4UsbHostBackend::isDeviceConnected() const {
    return m_activeDevice.isConnected;
}

bool P4UsbHostBackend::getActiveDevice(cbdos::usb::UsbDeviceInfo& outInfo) const {
    if (m_activeDevice.isConnected) {
        outInfo = m_activeDevice;
        return true;
    }
    outInfo = cbdos::usb::UsbDeviceInfo();
    return false;
}

void P4UsbHostBackend::registerEventCallback(cbdos::usb::UsbEventCallback callback) {
    m_eventCb = callback;
}

cbdos::usb::IUsbCdcChannel* P4UsbHostBackend::getCdcChannel() {
    return getP4UsbCdcChannel();
}

void P4UsbHostBackend::handleDeviceConnected(uint16_t vid, uint16_t pid, uint8_t dev_class) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;

    bool wasQuarantined = isQuarantined();

    m_activeDevice.vid = vid;
    m_activeDevice.pid = pid;
    m_activeDevice.isConnected = true;
    m_activeDevice.role = cbdos::usb::DeviceRole::Unassigned;

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
        m_activeDevice.devClass = cbdos::usb::DeviceClass::CdcAcm;
    } else if (dev_class == 0xFF) {
        m_activeDevice.devClass = cbdos::usb::DeviceClass::VendorSpecific;
    } else if (dev_class == 0x08) {
        m_activeDevice.devClass = cbdos::usb::DeviceClass::MassStorage;
    } else if (dev_class == 0x03) {
        m_activeDevice.devClass = cbdos::usb::DeviceClass::Hid;
    } else {
        m_activeDevice.devClass = cbdos::usb::DeviceClass::Unknown;
    }

    ESP_LOGI(TAG, "Dispositivo Conectado: %s %s (VID:0x%04X, PID:0x%04X, Cuarentena=%s)",
             m_activeDevice.manufacturer, m_activeDevice.product, vid, pid,
             wasQuarantined ? "SI" : "NO");

    m_activeDriver = nullptr;
    for (auto* driver : m_drivers) {
        if (driver->match(m_activeDevice)) {
            ESP_LOGI(TAG, "Asignando driver '%s' a dispositivo", driver->getDriverName());
            m_activeDriver = driver;
            if (driver->onAttach(m_activeDevice)) {
                break;
            } else {
                m_activeDriver = nullptr;
            }
        }
    }

    cbdos::usb::UsbEventCause cause = wasQuarantined ? 
        cbdos::usb::UsbEventCause::TargetResetInduced : 
        cbdos::usb::UsbEventCause::PhysicalHotPlug;

    if (m_eventCb) {
        m_eventCb(m_activeDevice, true, cause);
    }

    xSemaphoreGive(m_mutex);
}

void P4UsbHostBackend::handleDeviceDisconnected() {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;

    if (!m_activeDevice.isConnected) {
        xSemaphoreGive(m_mutex);
        return;
    }

    if (isQuarantined()) {
        ESP_LOGW(TAG, "Desconexión detectada bajo cuarentena: ignorando falso disconnect para mantener sesión");
        xSemaphoreGive(m_mutex);
        return;
    }

    ESP_LOGW(TAG, "Dispositivo USB desconectado físicamente (VID:0x%04X PID:0x%04X)",
             m_activeDevice.vid, m_activeDevice.pid);

    if (m_activeDriver) {
        m_activeDriver->onDetach(m_activeDevice);
        m_activeDriver = nullptr;
    }

    if (m_eventCb) {
        m_eventCb(m_activeDevice, false, cbdos::usb::UsbEventCause::PhysicalHotPlug);
    }

    m_activeDevice = cbdos::usb::UsbDeviceInfo();
    xSemaphoreGive(m_mutex);
}

} // namespace bsp
} // namespace cbdos
