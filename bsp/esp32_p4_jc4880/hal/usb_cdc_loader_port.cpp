#include "usb_cdc_loader_port.hpp"
#include "hal_usb_cdc_p4.hpp"
#include "hal_usb_host_p4.hpp"
#include "cbdos/usb_host.hpp"
#include <esp_loader_io.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "USB_LOADER_PORT";

esp_loader_error_t loader_port_usb_cdc_init(uint32_t timeout_ms) {
    auto* host = cbdos::bsp::getP4UsbHostBackend();
    if (host) {
        host->init();
    }

    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (!cdc) {
        return ESP_LOADER_ERROR_FAIL;
    }

    if (!cdc->tryAcquire(cbdos::usb::CdcOwner::Flasher, timeout_ms)) {
        ESP_LOGE(TAG, "No se pudo adquirir canal CDC para Flasher");
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Canal CDC adquirido exitosamente por el Flasher");
    return ESP_LOADER_SUCCESS;
}

void loader_port_usb_cdc_deinit(void) {
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (cdc) {
        cdc->release(cbdos::usb::CdcOwner::Flasher);
        ESP_LOGI(TAG, "Canal CDC liberado por el Flasher");
    }
}

esp_loader_error_t loader_port_usb_cdc_reset_target(void) {
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (!cdc) return ESP_LOADER_ERROR_FAIL;
    return cdc->resetTarget(false) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_FAIL;
}

extern "C" {

esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout) {
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (!cdc) return ESP_LOADER_ERROR_FAIL;

    size_t written = 0;
    auto res = cdc->write(data, size, written, timeout);
    if (res == cbdos::usb::UsbIoResult::Timeout) return ESP_LOADER_ERROR_TIMEOUT;
    if (res != cbdos::usb::UsbIoResult::Ok || written != size) return ESP_LOADER_ERROR_FAIL;
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout) {
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (!cdc) return ESP_LOADER_ERROR_FAIL;

    size_t total_read = 0;
    int64_t start_time = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout * 1000;

    while (total_read < size) {
        size_t chunk_read = 0;
        int64_t elapsed = esp_timer_get_time() - start_time;
        if (elapsed >= timeout_us) {
            return (total_read > 0) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
        }
        uint32_t remaining_ms = (uint32_t)((timeout_us - elapsed) / 1000);
        if (remaining_ms < 1) remaining_ms = 1;

        auto res = cdc->read(data + total_read, size - total_read, chunk_read, remaining_ms);
        total_read += chunk_read;
        if (res == cbdos::usb::UsbIoResult::Disconnected) {
            return ESP_LOADER_ERROR_FAIL;
        }
    }
    return ESP_LOADER_SUCCESS;
}

void loader_port_enter_bootloader(void) {
    auto* host = cbdos::bsp::getP4UsbHostBackend();
    if (host) {
        host->enterQuarantine(1500);
    }
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (cdc) {
        cdc->resetTarget(true);
    }
}

void loader_port_reset_target(void) {
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (cdc) {
        cdc->resetTarget(false);
    }
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t baudrate) {
    auto* cdc = cbdos::bsp::getP4UsbCdcChannel();
    if (!cdc) return ESP_LOADER_ERROR_FAIL;
    return cdc->setLineCoding(baudrate) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_FAIL;
}

} // extern "C"
