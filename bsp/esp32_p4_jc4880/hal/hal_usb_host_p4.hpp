#pragma once

#include "cbdos/usb_host.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <vector>

namespace cbdos {
namespace bsp {

class P4UsbHostBackend : public cbdos::usb::IUsbHostBackend {
public:
    P4UsbHostBackend();
    ~P4UsbHostBackend() override;

    bool init() override;
    void deinit() override;
    bool supportsHost() const override { return true; }

    bool registerDriver(cbdos::usb::IUsbDriver* driver) override;
    bool unregisterDriver(cbdos::usb::IUsbDriver* driver) override;

    bool isDeviceConnected() const override;
    bool getActiveDevice(cbdos::usb::UsbDeviceInfo& outInfo) const override;

    void registerEventCallback(cbdos::usb::UsbEventCallback callback) override;
    cbdos::usb::IUsbCdcChannel* getCdcChannel() override;

    // Métodos internos invocados por las tareas FreeRTOS
    void handleDeviceConnected(uint16_t vid, uint16_t pid, uint8_t dev_class);
    void handleDeviceDisconnected();

    // Ventana de cuarentena para evitar desconexiones falsas durante reset de targets nativos
    void enterQuarantine(uint32_t durationMs = 1500);
    bool isQuarantined() const;

private:
    SemaphoreHandle_t m_mutex = nullptr;
    bool m_initialized = false;
    bool m_hostInstalled = false;
    bool m_cdcDriverInstalled = false;

    TaskHandle_t m_hostLibTaskHdl = nullptr;
    TaskHandle_t m_mgrTaskHdl = nullptr;
    QueueHandle_t m_eventQueue = nullptr;

    cbdos::usb::UsbDeviceInfo m_activeDevice;
    cbdos::usb::IUsbDriver* m_activeDriver = nullptr;
    std::vector<cbdos::usb::IUsbDriver*> m_drivers;
    cbdos::usb::UsbEventCallback m_eventCb = nullptr;

    int64_t m_quarantineUntilUs = 0;
};

void initUsbHostBackendP4();
P4UsbHostBackend* getP4UsbHostBackend();

} // namespace bsp
} // namespace cbdos
