#include "hal_usb_cdc_p4.hpp"
#include "hal_usb_host_p4.hpp"
#include "usb/vcp_cp210x.h"
#include "usb/vcp_ch34x.h"
#include <esp_log.h>
#include <cstring>

static const char* TAG = "HAL_USB_CDC_P4";

namespace cbdos {
namespace bsp {

static P4UsbCdcChannel s_cdcChannel;

P4UsbCdcChannel* getP4UsbCdcChannel() {
    return &s_cdcChannel;
}

P4UsbCdcChannel::P4UsbCdcChannel() {
    m_mutex = xSemaphoreCreateMutex();
    m_flasherStreamBuf = xStreamBufferCreate(16384, 1);
    m_terminalRingBuf = xRingbufferCreate(2048, RINGBUF_TYPE_BYTEBUF);
}

P4UsbCdcChannel::~P4UsbCdcChannel() {
    close();
    if (m_flasherStreamBuf) {
        vStreamBufferDelete(m_flasherStreamBuf);
        m_flasherStreamBuf = nullptr;
    }
    if (m_terminalRingBuf) {
        vRingbufferDelete(m_terminalRingBuf);
        m_terminalRingBuf = nullptr;
    }
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

// ────────────────────────────────────────────────────────────────
// Implementación IUsbDriver
// ────────────────────────────────────────────────────────────────

bool P4UsbCdcChannel::match(const cbdos::usb::UsbDeviceInfo& dev) {
    // Matchea cualquier dispositivo CDC o puente serie
    return (dev.devClass == cbdos::usb::DeviceClass::CdcAcm ||
            dev.devClass == cbdos::usb::DeviceClass::VendorSpecific);
}

bool P4UsbCdcChannel::onAttach(const cbdos::usb::UsbDeviceInfo& dev) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout esperando mutex en onAttach");
        return false;
    }

    m_activeVid = dev.vid;
    m_activePid = dev.pid;
    m_isAttached = true;

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 4096,
        .in_buffer_size = 4096,
        .event_cb = cdcDevEventCallback,
        .data_cb = cdcRxDemuxCallback,
        .user_arg = this,
    };

    esp_err_t err = ESP_FAIL;
    if (dev.vid == 0x10C4) {
        err = cp210x_vcp_open(dev.pid, 0, &dev_config, &m_cdcDev);
    } else if (dev.vid == 0x1A86) {
        err = ch34x_vcp_open(dev.pid, 0, &dev_config, &m_cdcDev);
    } else {
        err = cdc_acm_host_open(dev.vid, dev.pid, 0, &dev_config, &m_cdcDev);
        if (err != ESP_OK) {
            err = cdc_acm_host_open_vendor_specific(dev.vid, dev.pid, 0, &dev_config, &m_cdcDev);
        }
    }

    if (err != ESP_OK || !m_cdcDev) {
        ESP_LOGE(TAG, "Fallo al abrir CDC en VID:0x%04X PID:0x%04X (%s)", dev.vid, dev.pid, esp_err_to_name(err));
        m_cdcDev = nullptr;
        m_isAttached = false;
        xSemaphoreGive(m_mutex);
        return false;
    }

    m_isOpen = true;
    purge();
    ESP_LOGI(TAG, "Canal USB CDC conectado y listo en VID:0x%04X PID:0x%04X", dev.vid, dev.pid);
    xSemaphoreGive(m_mutex);
    return true;
}

void P4UsbCdcChannel::onDetach(const cbdos::usb::UsbDeviceInfo& dev) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGW(TAG, "Dispositivo USB CDC desconectado físicamente (VID:0x%04X)", dev.vid);
        m_isAttached = false;
        m_isOpen = false;
        m_owner = cbdos::usb::CdcOwner::None;
        if (m_cdcDev) {
            cdc_acm_host_close(m_cdcDev);
            m_cdcDev = nullptr;
        }
        purge();
        xSemaphoreGive(m_mutex);
    }
}

// ────────────────────────────────────────────────────────────────
// Callbacks Nativos de Recepción y Eventos (ISR-Safe)
// ────────────────────────────────────────────────────────────────

bool P4UsbCdcChannel::cdcRxDemuxCallback(const uint8_t *data, size_t len, void *user_arg) {
    auto* self = static_cast<P4UsbCdcChannel*>(user_arg);
    if (!self || !data || len == 0) return true;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (self->m_owner == cbdos::usb::CdcOwner::Flasher) {
        if (self->m_flasherStreamBuf) {
            xStreamBufferSendFromISR(self->m_flasherStreamBuf, data, len, &xHigherPriorityTaskWoken);
        }
    } else {
        if (self->m_terminalRingBuf) {
            xRingbufferSendFromISR(self->m_terminalRingBuf, data, len, &xHigherPriorityTaskWoken);
        }
    }

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    return true;
}

void P4UsbCdcChannel::cdcDevEventCallback(const cdc_acm_host_dev_event_data_t *event, void *user_arg) {
    auto* self = static_cast<P4UsbCdcChannel*>(user_arg);
    if (!self || !event) return;

    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
        ESP_LOGW(TAG, "Evento nativo: Dispositivo CDC desconectado");
        self->m_isAttached = false;
        self->m_isOpen = false;
        self->m_owner = cbdos::usb::CdcOwner::None;
        if (self->m_cdcDev) {
            cdc_acm_host_close(self->m_cdcDev);
            self->m_cdcDev = nullptr;
        }
        self->purge();

        auto* backend = getP4UsbHostBackend();
        if (backend) {
            backend->postEvent(false);
        }
    } else if (event->type == CDC_ACM_HOST_ERROR) {
        ESP_LOGE(TAG, "Evento nativo: Error en bus CDC (%d)", event->data.error);
    }
}

// ────────────────────────────────────────────────────────────────
// Arbitraje y Preemption
// ────────────────────────────────────────────────────────────────

bool P4UsbCdcChannel::tryAcquire(cbdos::usb::CdcOwner owner, uint32_t timeoutMs) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        return false;
    }

    if (m_owner == owner) {
        xSemaphoreGive(m_mutex);
        return true;
    }

    if (m_owner == cbdos::usb::CdcOwner::None) {
        m_owner = owner;
        purge();
        xSemaphoreGive(m_mutex);
        return true;
    }

    // Preemption: Flasher tiene prioridad absoluta sobre Terminal
    if (owner == cbdos::usb::CdcOwner::Flasher && m_owner == cbdos::usb::CdcOwner::Terminal) {
        ESP_LOGI(TAG, "Arbitraje: Flasher desaloja temporalmente a Terminal Serial");
        m_owner = cbdos::usb::CdcOwner::Flasher;
        purge();
        xSemaphoreGive(m_mutex);
        return true;
    }

    // Terminal no puede desalojar a Flasher
    xSemaphoreGive(m_mutex);
    return false;
}

void P4UsbCdcChannel::release(cbdos::usb::CdcOwner owner) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (m_owner == owner) {
            ESP_LOGI(TAG, "Arbitraje: Canal CDC liberado por propietario anterior");
            m_owner = cbdos::usb::CdcOwner::None;
            purge();
        }
        xSemaphoreGive(m_mutex);
    }
}

// ────────────────────────────────────────────────────────────────
// I/O de Alta Velocidad (Flasher y Apps)
// ────────────────────────────────────────────────────────────────

cbdos::usb::UsbIoResult P4UsbCdcChannel::read(uint8_t* buffer, size_t maxLen, size_t& outLen, uint32_t timeoutMs) {
    outLen = 0;
    if (!m_cdcDev || !m_isAttached) return cbdos::usb::UsbIoResult::Disconnected;
    if (!buffer || maxLen == 0) return cbdos::usb::UsbIoResult::Ok;

    size_t received = 0;
    if (m_owner == cbdos::usb::CdcOwner::Flasher) {
        if (!m_flasherStreamBuf) return cbdos::usb::UsbIoResult::HardwareError;
        received = xStreamBufferReceive(m_flasherStreamBuf, buffer, maxLen, pdMS_TO_TICKS(timeoutMs));
    } else {
        if (!m_terminalRingBuf) return cbdos::usb::UsbIoResult::HardwareError;
        size_t item_size = 0;
        uint8_t* item = (uint8_t*)xRingbufferReceiveUpTo(m_terminalRingBuf, &item_size, pdMS_TO_TICKS(timeoutMs), maxLen);
        if (item && item_size > 0) {
            memcpy(buffer, item, item_size);
            vRingbufferReturnItem(m_terminalRingBuf, (void*)item);
            received = item_size;
        }
    }

    outLen = received;
    if (received > 0) return cbdos::usb::UsbIoResult::Ok;
    return (!m_isAttached) ? cbdos::usb::UsbIoResult::Disconnected : cbdos::usb::UsbIoResult::Timeout;
}

cbdos::usb::UsbIoResult P4UsbCdcChannel::write(const uint8_t* data, size_t len, size_t& outWritten, uint32_t timeoutMs) {
    outWritten = 0;
    if (!m_cdcDev || !m_isAttached) return cbdos::usb::UsbIoResult::Disconnected;
    if (!data || len == 0) return cbdos::usb::UsbIoResult::Ok;

    esp_err_t err = cdc_acm_host_data_tx_blocking(m_cdcDev, data, len, pdMS_TO_TICKS(timeoutMs));
    if (err == ESP_OK) {
        outWritten = len;
        return cbdos::usb::UsbIoResult::Ok;
    } else if (err == ESP_ERR_TIMEOUT) {
        return cbdos::usb::UsbIoResult::Timeout;
    } else {
        return cbdos::usb::UsbIoResult::HardwareError;
    }
}

void P4UsbCdcChannel::purge() {
    if (m_flasherStreamBuf) {
        xStreamBufferReset(m_flasherStreamBuf);
    }
    if (m_terminalRingBuf) {
        size_t item_size = 0;
        while (uint8_t* item = (uint8_t*)xRingbufferReceiveUpTo(m_terminalRingBuf, &item_size, 0, 1024)) {
            vRingbufferReturnItem(m_terminalRingBuf, (void*)item);
        }
    }
}

bool P4UsbCdcChannel::setLineCoding(uint32_t baud, uint8_t dataBits, uint8_t parity, uint8_t stopBits) {
    if (!m_cdcDev || !m_isAttached) return false;
    cdc_acm_line_coding_t line_coding = {
        .dwDTERate = baud,
        .bCharFormat = stopBits,
        .bParityType = parity,
        .bDataBits = dataBits,
    };
    esp_err_t err = cdc_acm_host_line_coding_set(m_cdcDev, &line_coding);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "cdc_acm_host_line_coding_set retorno %s (ignorado para targets USB-JTAG)", esp_err_to_name(err));
    }
    return true;
}

// ────────────────────────────────────────────────────────────────
// Secuencia de Reset Adaptativa
// ────────────────────────────────────────────────────────────────

bool P4UsbCdcChannel::sendNativeJtagBootSequence(bool enterBootloader) {
    if (!m_cdcDev) return false;
    ESP_LOGI(TAG, "Ejecutando secuencia JTAG de 4 pasos para target nativo Espressif (%s)",
             enterBootloader ? "Bootloader" : "Run");

    if (enterBootloader) {
        // Secuencia estricta de 4 pasos de esptool para USB-Serial-JTAG
        cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, false, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, true, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
    } else {
        cdc_acm_host_set_control_line_state(m_cdcDev, false, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
    }
    return true;
}

bool P4UsbCdcChannel::sendStandardBridgeBootSequence(bool enterBootloader) {
    if (!m_cdcDev) return false;
    ESP_LOGI(TAG, "Ejecutando secuencia estándar DTR/RTS para puente serie (%s)",
             enterBootloader ? "Bootloader" : "Run");

    if (enterBootloader) {
        // Modo DFU: DTR=false (GPIO0=0), RTS=true (Reset activo)
        cdc_acm_host_set_control_line_state(m_cdcDev, false, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
        vTaskDelay(pdMS_TO_TICKS(20));
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
    } else {
        // Modo Normal: DTR=true (GPIO0=1), RTS=true (Reset activo)
        cdc_acm_host_set_control_line_state(m_cdcDev, true, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
    }
    return true;
}

bool P4UsbCdcChannel::resetTarget(bool enterBootloader) {
    if (!m_cdcDev || !m_isAttached) return false;
    if (m_activeVid == 0x303A) {
        return sendNativeJtagBootSequence(enterBootloader);
    } else {
        return sendStandardBridgeBootSequence(enterBootloader);
    }
}

// ────────────────────────────────────────────────────────────────
// Métodos de cbdos::serial::ISerialPort (Compatibilidad de Terminal)
// ────────────────────────────────────────────────────────────────

bool P4UsbCdcChannel::open(const cbdos::serial::SerialConfig& config) {
    if (!m_isAttached || !m_cdcDev) {
        ESP_LOGW(TAG, "No se puede abrir puerto CDC: dispositivo no adjuntado");
        return false;
    }
    if (!tryAcquire(cbdos::usb::CdcOwner::Terminal, 1000)) {
        ESP_LOGW(TAG, "No se pudo adquirir canal CDC para Terminal");
        return false;
    }
    setBaudrate(config.baudrate);
    cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
    m_isOpen = true;
    return true;
}

void P4UsbCdcChannel::close() {
    if (m_owner == cbdos::usb::CdcOwner::Terminal) {
        release(cbdos::usb::CdcOwner::Terminal);
    }
    m_isOpen = false;
}

bool P4UsbCdcChannel::isOpen() const {
    return m_isOpen && m_isAttached;
}

size_t P4UsbCdcChannel::available() {
    if (!m_isOpen || !m_isAttached) return 0;
    if (m_owner == cbdos::usb::CdcOwner::Flasher) {
        return (m_flasherStreamBuf) ? xStreamBufferBytesAvailable(m_flasherStreamBuf) : 0;
    } else {
        if (!m_terminalRingBuf) return 0;
        UBaseType_t items = 0;
        vRingbufferGetInfo(m_terminalRingBuf, nullptr, nullptr, nullptr, nullptr, &items);
        return (size_t)items;
    }
}

size_t P4UsbCdcChannel::read(uint8_t* buffer, size_t maxLen) {
    size_t outLen = 0;
    cbdos::usb::UsbIoResult res = read(buffer, maxLen, outLen, 0);
    return (res == cbdos::usb::UsbIoResult::Ok) ? outLen : 0;
}

std::string P4UsbCdcChannel::readString(size_t maxLen) {
    std::string res;
    if (!m_isOpen || maxLen == 0) return res;
    uint8_t buf[256];
    size_t toRead = (maxLen < sizeof(buf)) ? maxLen : sizeof(buf);
    size_t r = read(buf, toRead);
    if (r > 0) res.assign((char*)buf, r);
    return res;
}

size_t P4UsbCdcChannel::write(const uint8_t* data, size_t len) {
    size_t outWritten = 0;
    cbdos::usb::UsbIoResult res = write(data, len, outWritten, 100);
    return (res == cbdos::usb::UsbIoResult::Ok) ? outWritten : 0;
}

size_t P4UsbCdcChannel::writeString(const std::string& str) {
    return write((const uint8_t*)str.data(), str.size());
}

void P4UsbCdcChannel::flush() {
    // Flush implícita en transmisiones CDC de FreeRTOS
}

bool P4UsbCdcChannel::setBaudrate(uint32_t baudrate) {
    return setLineCoding(baudrate);
}

bool P4UsbCdcChannel::setControlPin(bool level) {
    if (!m_cdcDev || !m_isAttached) return false;
    return (cdc_acm_host_set_control_line_state(m_cdcDev, level, false) == ESP_OK);
}

bool P4UsbCdcChannel::pulseControlPin(uint32_t durationMs, bool enterBootloader) {
    (void)durationMs;
    return resetTarget(enterBootloader);
}

} // namespace bsp
} // namespace cbdos
