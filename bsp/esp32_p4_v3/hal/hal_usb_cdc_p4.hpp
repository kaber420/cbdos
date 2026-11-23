#pragma once

#include "cbdos/usb_host.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/stream_buffer.h>
#include <freertos/semphr.h>
#include <usb/cdc_acm_host.h>

namespace cbdos {
namespace bsp {

class P4UsbCdcChannel : public cbdos::usb::IUsbCdcChannel, public cbdos::usb::IUsbDriver {
public:
    P4UsbCdcChannel();
    ~P4UsbCdcChannel() override;

    // --- cbdos::usb::IUsbDriver ---
    const char* getDriverName() const override { return "P4_CDC_ACM_Driver"; }
    uint8_t getPriority() const override { return 10; }
    bool match(const cbdos::usb::UsbDeviceInfo& dev) override;
    bool onAttach(const cbdos::usb::UsbDeviceInfo& dev) override;
    void onDetach(const cbdos::usb::UsbDeviceInfo& dev) override;

    // --- cbdos::serial::ISerialPort ---
    bool open(const cbdos::serial::SerialConfig& config) override;
    void close() override;
    bool isOpen() const override;
    size_t available() override;
    size_t read(uint8_t* buffer, size_t maxLen) override;
    std::string readString(size_t maxLen = 1024) override;
    size_t write(const uint8_t* data, size_t len) override;
    size_t writeString(const std::string& str) override;
    void flush() override;
    bool setBaudrate(uint32_t baudrate) override;
    bool setControlPin(bool level) override;
    bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false) override;

    // --- cbdos::usb::IUsbCdcChannel ---
    bool tryAcquire(cbdos::usb::CdcOwner owner, uint32_t timeoutMs = 1000) override;
    void release(cbdos::usb::CdcOwner owner) override;
    cbdos::usb::CdcOwner getCurrentOwner() const override { return m_owner; }

    cbdos::usb::UsbIoResult read(uint8_t* buffer, size_t maxLen, size_t& outLen, uint32_t timeoutMs) override;
    cbdos::usb::UsbIoResult write(const uint8_t* data, size_t len, size_t& outWritten, uint32_t timeoutMs) override;
    void purge() override;
    bool setLineCoding(uint32_t baud, uint8_t dataBits = 8, uint8_t parity = 0, uint8_t stopBits = 1) override;
    bool resetTarget(bool enterBootloader) override;

    // Callbacks nativos de ESP-IDF
    static bool cdcRxDemuxCallback(const uint8_t *data, size_t len, void *user_arg);
    static void cdcDevEventCallback(const cdc_acm_host_dev_event_data_t *event, void *user_arg);

    cdc_acm_dev_hdl_t getCdcHandle() const { return m_cdcDev; }

private:
    bool sendNativeJtagBootSequence(bool enterBootloader);
    bool sendStandardBridgeBootSequence(bool enterBootloader);

    mutable SemaphoreHandle_t m_mutex;
    cdc_acm_dev_hdl_t m_cdcDev = nullptr;
    RingbufHandle_t m_terminalRingBuf = nullptr;
    StreamBufferHandle_t m_flasherStreamBuf = nullptr;

    cbdos::usb::CdcOwner m_owner = cbdos::usb::CdcOwner::None;
    uint16_t m_activeVid = 0;
    uint16_t m_activePid = 0;
    bool m_isOpen = false;
    bool m_isAttached = false;
};

P4UsbCdcChannel* getP4UsbCdcChannel();

} // namespace bsp
} // namespace cbdos
