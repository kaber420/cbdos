#pragma once

#include <string>
#include <mutex>

namespace cbdos {
namespace system {

enum class ResourceClaimMode {
    Exclusive,
    Shared,
    SystemLocked // Para uso exclusivo de drivers internos del sistema (Pantalla, Audio, etc)
};

class UniversalResourceManager {
public:
    static UniversalResourceManager& getInstance();

    // Reclama un GPIO si está permitido (Whitelist) y no está en uso
    bool claimGpio(int pin, const std::string& owner, ResourceClaimMode mode = ResourceClaimMode::Exclusive);
    
    // Libera un GPIO previamente reclamado
    void releaseGpio(int pin, const std::string& owner);

    // Verifica si el pin es permitido y está libre
    bool isGpioAvailable(int pin) const;

private:
    UniversalResourceManager();
    ~UniversalResourceManager();
    
    UniversalResourceManager(const UniversalResourceManager&) = delete;
    UniversalResourceManager& operator=(const UniversalResourceManager&) = delete;

    std::mutex m_mutex;
    
    // Simple state tracking for up to 64 pins
    struct PinState {
        bool inUse = false;
        std::string owner = "";
        ResourceClaimMode mode = ResourceClaimMode::Exclusive;
    };
    
    PinState m_pins[64];
};

} // namespace system
} // namespace cbdos
