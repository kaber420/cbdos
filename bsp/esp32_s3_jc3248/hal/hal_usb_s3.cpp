#include "hal_usb_s3.hpp"
#include "hal_usb_cdc_s3.hpp"
#include "cbdos/usb_manager.hpp"
#include <esp_log.h>
#include <esp_timer.h>
#include <usb/usb_host.h>
#include <usb/cdc_acm_host.h>
#include <cstring>
#include <algorithm>

static const char* TAG = "HAL_USB_HOST_S3";

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

static S3UsbHostBackend* s_backendInstance = nullptr;

static void s3_on_native_new_dev_cb(usb_device_handle_t usb_dev) {
    const usb_device_desc_t *desc = nullptr;
    if (usb_host_get_device_descriptor(usb_dev, &desc) == ESP_OK && desc) {
        ESP_LOGI(TAG, "🔌 Interrupción S3: Dispositivo detectado -> VID=0x%04X, PID=0x%04X, Clase=0x%02X",
                 desc->idVendor, desc->idProduct, desc->bDeviceClass);

        if (s_backendInstance) {
            s_backendInstance->postEvent(true, desc->idVendor, desc->idProduct, desc->bDeviceClass);
        }
    }
}

static void s3_usb_host_lib_task(void* arg) {
    auto* backend = static_cast<S3UsbHostBackend*>(arg);
    while (backend) {
        uint32_t event_flags = 0;
        esp_err_t err = usb_host_lib_handle_events(pdMS_TO_TICKS(10), &event_flags);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "usb_host_lib_handle_events: %s", esp_err_to_name(err));
        }
    }
    vTaskDelete(NULL);
}

static void s3_usb_mgr_task(void* arg) {
    auto* backend = static_cast<S3UsbHostBackend*>(arg);
    UsbEvtMsg msg;
    while (backend && backend->getEventQueue()) {
        if (xQueueReceive(backend->getEventQueue(), &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.type == EVT_CONNECTED) {
                // Dar 50ms para estabilización de enumeración
                vTaskDelay(pdMS_TO_TICKS(50));
                backend->handleDeviceConnected(msg.vid, msg.pid, msg.dev_class);
            } else if (msg.type == EVT_DISCONNECTED) {
                backend->handleDeviceDisconnected();
            }
        }
    }
    vTaskDelete(NULL);
}

} // anonymous namespace

static S3UsbHostBackend s_s3HostBackend;

S3UsbHostBackend* getS3UsbHostBackend() {
    return &s_s3HostBackend;
}

void initUsbHostBackendS3() {
    s_s3HostBackend.init();
}

S3UsbHostBackend::S3UsbHostBackend() {
    m_mutex = xSemaphoreCreateMutex();
    s_backendInstance = this;
}

S3UsbHostBackend::~S3UsbHostBackend() {
    deinit();
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    s_backendInstance = nullptr;
}

bool S3UsbHostBackend::supportsHost() const {
    return (cbdos::usb::UsbManager::getInstance().getBootMode() == cbdos::usb::UsbMode::Host);
}

void S3UsbHostBackend::enterQuarantine(uint32_t durationMs) {
    m_quarantineUntilUs = esp_timer_get_time() + (int64_t)durationMs * 1000;
    ESP_LOGI(TAG, "Activando ventana de cuarentena USB S3 (%u ms)", durationMs);
}

bool S3UsbHostBackend::isQuarantined() const {
    return (esp_timer_get_time() < m_quarantineUntilUs);
}

bool S3UsbHostBackend::init() {
    if (m_initialized) return true;

    cbdos::usb::setUsbHostBackend(this);

    if (cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Host) {
        ESP_LOGI(TAG, "ESP32-S3: USB Host no iniciado (sistema arrancado en modo HID/Device)");
        return false;
    }

    ESP_LOGI(TAG, "Iniciando subsistema USB Host en ESP32-S3...");

    // 0. Crear cola de eventos y tarea gestora asíncrona
    if (!m_eventQueue) {
        m_eventQueue = xQueueCreate(10, sizeof(UsbEvtMsg));
    }
    if (!m_mgrTaskHdl) {
        xTaskCreatePinnedToCore(s3_usb_mgr_task, "usb_mgr_s3", 4096, this, 5, &m_mgrTaskHdl, 0);
    }

    // 1. Instalar la librería central de USB Host
    if (!m_hostInstalled) {
        usb_host_config_t host_config = {};
        host_config.skip_phy_setup = false;
        host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;

        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Error fatal instalando USB Host Library en S3: %s", esp_err_to_name(err));
            return false;
        }
        m_hostInstalled = true;
        xTaskCreatePinnedToCore(s3_usb_host_lib_task, "usb_host_s3_lib", 4096, this, 5, &m_hostLibTaskHdl, 0);
    }

    // 2. Instalar el driver CDC-ACM Host unificado
    if (!m_cdcDriverInstalled) {
        cdc_acm_host_driver_config_t driver_config = {};
        driver_config.driver_task_stack_size = 4096;
        driver_config.driver_task_priority = 5;
        driver_config.xCoreID = 0;
        driver_config.new_dev_cb = s3_on_native_new_dev_cb;
        esp_err_t err = cdc_acm_host_install(&driver_config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Error fatal instalando CDC-ACM Driver en S3: %s", esp_err_to_name(err));
            return false;
        }
        m_cdcDriverInstalled = true;
    }

    // 3. Registrar el canal CDC como driver USB predeterminado
    registerDriver(getS3UsbCdcChannel());

    m_initialized = true;
    ESP_LOGI(TAG, "HAL USB Host S3 inicializado con éxito");
    return true;
}

void S3UsbHostBackend::deinit() {
    if (!m_initialized) return;
    handleDeviceDisconnected();
    if (m_mgrTaskHdl) {
        vTaskDelete(m_mgrTaskHdl);
        m_mgrTaskHdl = nullptr;
    }
    if (m_eventQueue) {
        vQueueDelete(m_eventQueue);
        m_eventQueue = nullptr;
    }
    m_initialized = false;
}

bool S3UsbHostBackend::registerDriver(cbdos::usb::IUsbDriver* driver) {
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

    ESP_LOGI(TAG, "Driver USB registrado en HAL S3: '%s' (prio=%u)", driver->getDriverName(), driver->getPriority());

    if (m_activeDevice.isConnected && !m_activeDriver) {
        if (driver->match(m_activeDevice)) {
            m_activeDriver = driver;
            driver->onAttach(m_activeDevice);
        }
    }

    xSemaphoreGive(m_mutex);
    return true;
}

bool S3UsbHostBackend::unregisterDriver(cbdos::usb::IUsbDriver* driver) {
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

bool S3UsbHostBackend::isDeviceConnected() const {
    return m_activeDevice.isConnected;
}

bool S3UsbHostBackend::getActiveDevice(cbdos::usb::UsbDeviceInfo& outInfo) const {
    if (m_activeDevice.isConnected) {
        outInfo = m_activeDevice;
        return true;
    }
    outInfo = cbdos::usb::UsbDeviceInfo();
    return false;
}

void S3UsbHostBackend::registerEventCallback(cbdos::usb::UsbEventCallback callback) {
    m_eventCb = callback;
}

cbdos::usb::IUsbCdcChannel* S3UsbHostBackend::getCdcChannel() {
    return getS3UsbCdcChannel();
}

void S3UsbHostBackend::handleDeviceConnected(uint16_t vid, uint16_t pid, uint8_t dev_class) {
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

    ESP_LOGI(TAG, "S3 Dispositivo Conectado: %s %s (VID:0x%04X, PID:0x%04X, Cuarentena=%s)",
             m_activeDevice.manufacturer, m_activeDevice.product, vid, pid,
             wasQuarantined ? "SI" : "NO");

    m_activeDriver = nullptr;
    for (auto* driver : m_drivers) {
        if (driver->match(m_activeDevice)) {
            ESP_LOGI(TAG, "S3 Asignando driver '%s' a dispositivo", driver->getDriverName());
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

void S3UsbHostBackend::handleDeviceDisconnected() {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;

    if (!m_activeDevice.isConnected) {
        xSemaphoreGive(m_mutex);
        return;
    }

    if (isQuarantined()) {
        ESP_LOGW(TAG, "S3 Desconexión detectada bajo cuarentena: ignorando falso disconnect");
        xSemaphoreGive(m_mutex);
        return;
    }

    ESP_LOGW(TAG, "S3 Dispositivo USB desconectado físicamente (VID:0x%04X PID:0x%04X)",
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

void S3UsbHostBackend::postEvent(bool connected, uint16_t vid, uint16_t pid, uint8_t dev_class) {
    if (!m_eventQueue) return;
    UsbEvtMsg msg;
    msg.type = connected ? EVT_CONNECTED : EVT_DISCONNECTED;
    msg.vid = vid;
    msg.pid = pid;
    msg.dev_class = dev_class;
    xQueueSend(m_eventQueue, &msg, 0);
}

// Auto-registro estático para asegurar que el backend esté seteado siempre en S3
struct S3UsbHostRegistrar {
    S3UsbHostRegistrar() {
        cbdos::usb::setUsbHostBackend(&s_s3HostBackend);
    }
};
static S3UsbHostRegistrar s_registrar;

} // namespace bsp
} // namespace cbdos
