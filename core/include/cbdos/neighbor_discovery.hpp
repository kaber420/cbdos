#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

namespace cbdos {
namespace network {

enum class NeighborBrand {
    Unknown,
    Ubiquiti,
    MikroTik,
    Cisco,
    Generic
};

struct NeighborDevice {
    std::string ip;
    uint8_t mac[6]{0};
    NeighborBrand brand{NeighborBrand::Unknown};
    std::string protocol;     // "ubnt", "mndp", "cdp", "lldp"
    std::string model;        // ej: "PowerBeam 5AC Gen2", "RB750Gr3", "LiteBeam 5AC"
    std::string identity;     // Hostname / SSID / Nombre de sistema
    std::string firmware;     // ej: "WA.v8.7.1", "RouterOS v7.14"
    std::string interface;    // Interfaz reportada (ej: "ath0", "ether1")
    uint32_t uptimeSec{0};
    uint32_t lastSeenMs{0};

    std::string getMacString() const;
    std::string getBrandString() const;
};

using OnNeighborDiscoveredCb = std::function<void(const NeighborDevice& dev)>;

class INeighborDiscoveryBackend {
public:
    virtual ~INeighborDiscoveryBackend() = default;

    virtual bool isNetworkConnected() const = 0;

    // Escaneo activo de Ubiquiti (UDP broadcast 10001)
    virtual bool scanUbiquiti(uint32_t timeoutMs, OnNeighborDiscoveredCb onDevice) = 0;

    // Escaneo activo / pasivo de MikroTik MNDP (UDP broadcast 5678)
    virtual bool scanMikrotik(uint32_t timeoutMs, OnNeighborDiscoveredCb onDevice) = 0;
};

void setNeighborDiscoveryBackend(INeighborDiscoveryBackend* backend);
INeighborDiscoveryBackend* getNeighborDiscoveryBackend();

class NeighborDiscoveryService {
public:
    static NeighborDiscoveryService& getInstance();

    // Dispara escaneo en background sin bloquear LVGL
    bool triggerScan(uint32_t timeoutMs = 3500);

    // Consulta de dispositivos en caché
    std::vector<NeighborDevice> getDiscoveredDevices() const;
    bool findByIp(const std::string& ip, NeighborDevice& outDev) const;
    void clear();

    // Callback reactivo cuando un nuevo vecino es hallado
    void setNeighborDiscoveredCallback(OnNeighborDiscoveredCb cb);

    bool isScanning() const;

private:
    NeighborDiscoveryService() = default;
    ~NeighborDiscoveryService() = default;
    NeighborDiscoveryService(const NeighborDiscoveryService&) = delete;
    NeighborDiscoveryService& operator=(const NeighborDiscoveryService&) = delete;

    mutable std::mutex m_mutex;
    std::vector<NeighborDevice> m_devices;
    OnNeighborDiscoveredCb m_onDeviceCb{nullptr};
    std::atomic<bool> m_isScanning{false};

    static void discoveryTask(void* param);
};

} // namespace network
} // namespace cbdos
