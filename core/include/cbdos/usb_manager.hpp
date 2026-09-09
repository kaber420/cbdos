#pragma once

#include <cstdint>

namespace cbdos {
namespace usb {

// Modo exclusivo del único PHY USB-OTG HS.
// Solo un stack puede ser dueño del hardware por arranque.
enum class UsbMode : uint8_t {
    Hid = 0,  // TinyUSB Device: teclado/ratón hacia el PC
    Host = 1  // USB Host: módem, flasher, CDC, periféricos
};

// Gestor de sistema (core agnóstico). Las apps y vistas SOLO hablan con
// esta API; jamás tocan TinyUSB, USB Host, ESP-IDF ni Arduino directo.
class UsbManager {
public:
    static UsbManager& getInstance();

    // Lee el modo persistido (llamar una vez al arranque, con persistencia lista).
    bool init();

    // Modo aplicado en este arranque (decide qué stack inicia el BSP).
    UsbMode getBootMode() const { return m_bootMode; }
    // Modo pedido por el usuario (aplica tras reboot).
    UsbMode getPendingMode() const { return m_pendingMode; }
    bool needsReboot() const { return m_pendingMode != m_bootMode; }

    // Pide un modo. Persiste y avisa si requiere reboot. No toca hardware.
    bool requestMode(UsbMode mode);
    void reboot();

private:
    UsbManager() = default;
    UsbMode m_bootMode = UsbMode::Hid;
    UsbMode m_pendingMode = UsbMode::Hid;
    bool m_initialized = false;
};

}  // namespace usb
}  // namespace cbdos
