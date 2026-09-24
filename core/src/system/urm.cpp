#include "cbdos/urm.hpp"
#include "cbdos/system.hpp"
#include "cbdos_device_tree.h"

namespace cbdos {
namespace system {

static const char* TAG = "URM";

UniversalResourceManager& UniversalResourceManager::getInstance() {
    static UniversalResourceManager instance;
    return instance;
}

UniversalResourceManager::UniversalResourceManager() {
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "URM Inicializado (Whitelist mode). Pines permitidos: %d", cbdos::board::NUM_EXPANSION_PINS);
}

UniversalResourceManager::~UniversalResourceManager() {}

bool UniversalResourceManager::isGpioAvailable(int pin) const {
    if (pin < 0 || pin >= 64) return false;
    
    // Whitelist check
    bool isAllowed = false;
    for (size_t i = 0; i < cbdos::board::NUM_EXPANSION_PINS; ++i) {
        if (cbdos::board::EXPANSION_PINS[i] == pin) {
            isAllowed = true;
            break;
        }
    }
    
    if (!isAllowed) {
        return false;
    }
    
    // In-use check
    return !m_pins[pin].inUse;
}

bool UniversalResourceManager::claimGpio(int pin, const std::string& owner, ResourceClaimMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (pin < 0 || pin >= 64) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Reclamo invalido: pin %d fuera de rango", pin);
        return false;
    }
    
    // Firewall Check (Whitelist)
    bool isAllowed = false;
    for (size_t i = 0; i < cbdos::board::NUM_EXPANSION_PINS; ++i) {
        if (cbdos::board::EXPANSION_PINS[i] == pin) {
            isAllowed = true;
            break;
        }
    }
    
    if (!isAllowed) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Acceso denegado: pin %d no esta en la whitelist (Hardware Firewall)", pin);
        return false;
    }
    
    if (m_pins[pin].inUse) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Pin %d ya reclamado por '%s'", pin, m_pins[pin].owner.c_str());
        return false;
    }
    
    m_pins[pin].inUse = true;
    m_pins[pin].owner = owner;
    m_pins[pin].mode = mode;
    
    cbdos::system::log(cbdos::system::LogLevel::Debug, TAG, "Pin %d reclamado por '%s'", pin, owner.c_str());
    return true;
}

void UniversalResourceManager::releaseGpio(int pin, const std::string& owner) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (pin >= 0 && pin < 64 && m_pins[pin].inUse && m_pins[pin].owner == owner) {
        m_pins[pin].inUse = false;
        m_pins[pin].owner = "";
        cbdos::system::log(cbdos::system::LogLevel::Debug, TAG, "Pin %d liberado por '%s'", pin, owner.c_str());
    }
}

} // namespace system
} // namespace cbdos
