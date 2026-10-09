#pragma once

#include <cstdint>
#include <string>

namespace cbdos {
namespace security {

enum class LockPolicy : uint8_t {
    Disabled     = 0,  // Sin bloqueo. Acceso directo total.
    Lockscreen   = 1,  // Bloqueo total de pantalla tras reposo o arranque.
    SettingsOnly = 2   // Kiosco: Vistas operativas abiertas, PIN para ajustes/salida.
};

class LockService {
public:
    static LockService& getInstance();

    // Inicializa el servicio leyendo la configuración y PIN de NVS (namespace: "cbdos_sec")
    bool init();

    // Estado de bloqueo
    bool isLocked();
    void lock();
    void unlock();

    // Verificación y gestión de PIN
    bool verifyPin(const std::string& pin);
    bool setPin(const std::string& oldPin, const std::string& newPin);
    bool forceSetPin(const std::string& newPin);
    bool hasCustomPin();

    // Políticas de seguridad
    LockPolicy getPolicy();
    void setPolicy(LockPolicy policy);

    // Helpers
    static const char* getPolicyName(LockPolicy policy);

private:
    LockService() = default;

    bool m_initialized = false;
    bool m_locked = false;
    LockPolicy m_policy = LockPolicy::Disabled;
    std::string m_pinHash = "1234";
};

} // namespace security
} // namespace cbdos
