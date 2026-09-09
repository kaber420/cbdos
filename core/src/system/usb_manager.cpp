#include "cbdos/usb_manager.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/system.hpp"

namespace cbdos {
namespace usb {

namespace {
constexpr const char* kNs = "cbdos_usb";
constexpr const char* kKeyMode = "mode";

UsbMode sanitize(uint8_t v) {
    return (v == static_cast<uint8_t>(UsbMode::Host)) ? UsbMode::Host : UsbMode::Hid;
}
}  // namespace

UsbManager& UsbManager::getInstance() {
    static UsbManager instance;
    return instance;
}

bool UsbManager::init() {
    auto* be = persistence::getBackend();
    if (be && be->begin(kNs, true)) {
        uint8_t v = be->getUChar(kKeyMode, static_cast<uint8_t>(UsbMode::Hid));
        be->end();
        m_bootMode = sanitize(v);
    } else {
        m_bootMode = UsbMode::Hid;
    }
    m_pendingMode = m_bootMode;
    m_initialized = true;
    return true;
}

bool UsbManager::requestMode(UsbMode mode) {
    m_pendingMode = mode;
    auto* be = persistence::getBackend();
    if (be && be->begin(kNs, false)) {
        bool ok = be->setUChar(kKeyMode, static_cast<uint8_t>(mode));
        be->end();
        return ok;
    }
    // Sin backend (tests): solo RAM, el reboot lo aplicaría igual.
    return true;
}

void UsbManager::reboot() {
    system::restart();
}

}  // namespace usb
}  // namespace cbdos
