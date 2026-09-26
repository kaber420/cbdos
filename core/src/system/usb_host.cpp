#include "cbdos/usb_host.hpp"

namespace cbdos {
namespace usb {

static IUsbHostBackend* s_usbHostBackend = nullptr;

IUsbHostBackend* getUsbHostBackend() {
    return s_usbHostBackend;
}

void setUsbHostBackend(IUsbHostBackend* backend) {
    s_usbHostBackend = backend;
}

} // namespace usb
} // namespace cbdos
