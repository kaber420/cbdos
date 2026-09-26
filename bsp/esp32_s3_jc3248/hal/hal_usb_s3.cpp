#include "cbdos/usb_host.hpp"
#include "cbdos/system.hpp"

namespace cbdos {
namespace bsp {

class S3UsbHostBackend : public cbdos::usb::IUsbHostBackend {
public:
    bool init() override {
        cbdos::system::log(cbdos::system::LogLevel::Info, "HAL_USB_S3",
                           "ESP32-S3: USB Host no disponible en esta revision de hardware");
        return false;
    }

    void deinit() override {}

    bool supportsHost() const override {
        return false;
    }

    bool registerDriver(cbdos::usb::IUsbDriver* driver) override {
        (void)driver;
        return false;
    }

    bool unregisterDriver(cbdos::usb::IUsbDriver* driver) override {
        (void)driver;
        return false;
    }

    bool isDeviceConnected() const override {
        return false;
    }

    bool getActiveDevice(cbdos::usb::UsbDeviceInfo& outInfo) const override {
        outInfo.isConnected = false;
        return false;
    }

    void registerEventCallback(cbdos::usb::UsbEventCallback callback) override {
        (void)callback;
    }

    cbdos::usb::IUsbCdcChannel* getCdcChannel() override {
        return nullptr;
    }
};

static S3UsbHostBackend s_s3UsbHostBackend;

void initUsbHostS3() {
    cbdos::usb::setUsbHostBackend(&s_s3UsbHostBackend);
}

// Auto-registro estático para asegurar que el backend esté seteado siempre en S3
struct S3UsbHostRegistrar {
    S3UsbHostRegistrar() {
        cbdos::usb::setUsbHostBackend(&s_s3UsbHostBackend);
    }
};
static S3UsbHostRegistrar s_registrar;

} // namespace bsp
} // namespace cbdos
