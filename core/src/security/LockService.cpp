#include "cbdos/security.hpp"
#include "cbdos/vault.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/system.hpp"
#include "cbdos_build_profile.h"

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace security {

static const char* TAG = "LockService";
static const char* NVS_NS = "cbdos_sec";
static const char* KEY_PIN = "pin";
static const char* KEY_POLICY = "policy";
static const char* KEY_PIN_SALT = "pin_salt";

static std::string hashPinPbkdf2(const std::string& pin, const std::string& salt) {
    uint8_t out[32];
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return "";
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, md, 1);
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
                                  reinterpret_cast<const uint8_t*>(pin.data()), pin.size(),
                                  reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
                                  5000, 32, out);
    mbedtls_md_free(&md_ctx);

    char hex[65] = {0};
    for (int i = 0; i < 32; ++i) {
        snprintf(hex + (i * 2), 3, "%02x", out[i]);
    }
    return std::string(hex);
}

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
        VaultService::getInstance().lock();
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Sistema bloqueado y boveda purgada");
    }
}

void LockService::unlock() {
    m_locked = false;
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Sistema desbloqueado");
}

bool LockService::verifyPin(const std::string& pin) {
    if (!m_initialized) init();

    auto* be = persistence::getBackend();
    std::string salt = "cbdos_default_salt";
    if (be && be->begin(NVS_NS, true)) {
        salt = be->getString(KEY_PIN_SALT, "cbdos_default_salt");
        be->end();
    }

    // Compatibilidad retroactiva: si m_pinHash tiene longitud corta (ej. "1234"), migrar a PBKDF2
    if (m_pinHash == pin && m_pinHash.size() < 32) {
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "PIN plano legado detectado. Migrando a PBKDF2 hash...");
        forceSetPin(pin);
        unlock();
        VaultService::getInstance().unlock(pin);
        return true;
    }

    std::string computed = hashPinPbkdf2(pin, salt);
    if (!computed.empty() && computed == m_pinHash) {
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "PIN verificado correctamente (PBKDF2)");
        unlock();
        VaultService::getInstance().unlock(pin);
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

    auto* be = persistence::getBackend();
    std::string salt = "cbdos_salt_" + std::to_string(cbdos::system::getTimeMs());
    std::string hashed = hashPinPbkdf2(newPin, salt);
    if (hashed.empty()) return false;

    m_pinHash = hashed;
    if (be && be->begin(NVS_NS, false)) {
        be->setString(KEY_PIN, hashed);
        be->setString(KEY_PIN_SALT, salt);
        be->end();
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "PIN actualizado y persistido como PBKDF2 en NVS");
        VaultService::getInstance().unlock(newPin);
        return true;
    }

    cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error persistiendo nuevo PIN en NVS");
    return false;
}

bool LockService::hasCustomPin() {
    if (!m_initialized) init();
    auto* be = persistence::getBackend();
    std::string salt = "cbdos_default_salt";
    if (be && be->begin(NVS_NS, true)) {
        salt = be->getString(KEY_PIN_SALT, "cbdos_default_salt");
        be->end();
    }
    return m_pinHash != "1234" && m_pinHash != hashPinPbkdf2("1234", salt);
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
