#include "cbdos/security.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/system.hpp"
#include "cbdos_build_profile.h"

namespace cbdos {
namespace security {

static const char* TAG = "LockService";
static const char* NVS_NS = "cbdos_sec";
static const char* KEY_PIN = "pin";
static const char* KEY_POLICY = "policy";

LockService& LockService::getInstance() {
    static LockService instance;
    return instance;
}

bool LockService::init() {
    if (m_initialized) return true;

    auto* be = persistence::getBackend();
    if (be && be->begin(NVS_NS, true)) {
        m_pinHash = be->getString(KEY_PIN, "1234");
        uint8_t pol = be->getUChar(KEY_POLICY,
#if CBDOS_FEATURE_KIOSK_LOCK
            static_cast<uint8_t>(LockPolicy::SettingsOnly)
#else
            static_cast<uint8_t>(LockPolicy::Disabled)
#endif
        );
        m_policy = static_cast<LockPolicy>(pol);
        be->end();
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "NVS leido con exito: policy=%d (%s), pin_len=%d",
                           (int)m_policy, getPolicyName(m_policy), (int)m_pinHash.length());
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "No se pudo abrir NVS ns='%s'. Usando valores por defecto", NVS_NS);
#if CBDOS_FEATURE_KIOSK_LOCK
        m_policy = LockPolicy::SettingsOnly;
#else
        m_policy = LockPolicy::Disabled;
#endif
    }

    if (m_policy == LockPolicy::Lockscreen) {
        m_locked = true;
    } else {
        m_locked = false;
    }

    m_initialized = true;
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "LockService iniciado. Politica: %s, Bloqueado: %s",
                       getPolicyName(m_policy), m_locked ? "SI" : "NO");
    return true;
}

bool LockService::isLocked() {
    if (!m_initialized) init();
    return m_locked;
}

void LockService::lock() {
    if (!m_initialized) init();
    if (m_policy != LockPolicy::Disabled) {
        m_locked = true;
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Sistema bloqueado");
    }
}

void LockService::unlock() {
    m_locked = false;
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Sistema desbloqueado");
}

bool LockService::verifyPin(const std::string& pin) {
    if (!m_initialized) init();

    if (pin == m_pinHash) {
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "PIN verificado correctamente");
        unlock();
        return true;
    }

    cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Intento de PIN fallido");
    return false;
}

bool LockService::setPin(const std::string& oldPin, const std::string& newPin) {
    if (!verifyPin(oldPin)) {
        return false;
    }
    return forceSetPin(newPin);
}

bool LockService::forceSetPin(const std::string& newPin) {
    if (newPin.empty()) return false;
    if (!m_initialized) init();

    m_pinHash = newPin;
    auto* be = persistence::getBackend();
    if (be && be->begin(NVS_NS, false)) {
        be->setString(KEY_PIN, newPin);
        be->end();
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "PIN actualizado y persistido en NVS namespace='%s'", NVS_NS);
        return true;
    }

    cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error persistiendo nuevo PIN en NVS");
    return false;
}

bool LockService::hasCustomPin() {
    if (!m_initialized) init();
    return m_pinHash != "1234";
}

LockPolicy LockService::getPolicy() {
    if (!m_initialized) init();
    return m_policy;
}

void LockService::setPolicy(LockPolicy policy) {
    if (!m_initialized) init();
    m_policy = policy;
    auto* be = persistence::getBackend();
    if (be && be->begin(NVS_NS, false)) {
        be->setUChar(KEY_POLICY, static_cast<uint8_t>(policy));
        be->end();
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Politica actualizada y persistida en NVS: %s (%d)",
                           getPolicyName(policy), static_cast<int>(policy));
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error persistiendo politica en NVS");
    }
}

const char* LockService::getPolicyName(LockPolicy policy) {
    switch (policy) {
        case LockPolicy::Disabled: return "Disabled";
        case LockPolicy::Lockscreen: return "Lockscreen";
        case LockPolicy::SettingsOnly: return "SettingsOnly";
        default: return "Unknown";
    }
}

} // namespace security
} // namespace cbdos
