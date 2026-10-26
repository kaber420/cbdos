#include "cbdos/neighbor_discovery.hpp"
#include "cbdos/rtos.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace network {

static INeighborDiscoveryBackend* s_backend = nullptr;

void setNeighborDiscoveryBackend(INeighborDiscoveryBackend* backend) {
    s_backend = backend;
}

INeighborDiscoveryBackend* getNeighborDiscoveryBackend() {
    return s_backend;
}

std::string NeighborDevice::getMacString() const {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

std::string NeighborDevice::getBrandString() const {
    switch (brand) {
        case NeighborBrand::Ubiquiti: return "Ubiquiti";
        case NeighborBrand::MikroTik: return "MikroTik";
        case NeighborBrand::Cisco:    return "Cisco";
        default:                      return "Generic";
    }
}

NeighborDiscoveryService& NeighborDiscoveryService::getInstance() {
    static NeighborDiscoveryService instance;
    return instance;
}

bool NeighborDiscoveryService::isScanning() const {
    return m_isScanning;
}

void NeighborDiscoveryService::clear() {
    m_devices.clear();
}

std::vector<NeighborDevice> NeighborDiscoveryService::getDiscoveredDevices() const {
    return m_devices;
}

bool NeighborDiscoveryService::findByIp(const std::string& ip, NeighborDevice& outDev) const {
    for (const auto& dev : m_devices) {
        if (dev.ip == ip) {
            outDev = dev;
            return true;
        }
    }
    return false;
}

void NeighborDiscoveryService::setNeighborDiscoveredCallback(OnNeighborDiscoveredCb cb) {
    m_onDeviceCb = cb;
}

struct DiscoveryTaskParams {
    NeighborDiscoveryService* service;
    uint32_t timeoutMs;
};

void NeighborDiscoveryService::discoveryTask(void* param) {
    auto* p = static_cast<DiscoveryTaskParams*>(param);
    if (!p || !p->service) {
        cbdos::rtos::deleteTask(nullptr);
        return;
    }

    auto onDevice = [p](const NeighborDevice& dev) {
        bool found = false;
        for (auto& existing : p->service->m_devices) {
            if (existing.ip == dev.ip || 
                (existing.mac[0] != 0 && std::memcmp(existing.mac, dev.mac, 6) == 0)) {
                existing = dev;
                found = true;
                break;
            }
        }
        if (!found) {
            p->service->m_devices.push_back(dev);
        }
        if (p->service->m_onDeviceCb) {
            p->service->m_onDeviceCb(dev);
        }
    };

    if (s_backend) {
        // Sondeo Ubiquiti (UDP 10001)
        s_backend->scanUbiquiti(p->timeoutMs / 2, onDevice);
        // Sondeo MikroTik MNDP (UDP 5678)
        s_backend->scanMikrotik(p->timeoutMs / 2, onDevice);
    }

    p->service->m_isScanning = false;
    delete p;

    // Autodestrucción limpia de la tarea FreeRTOS
    cbdos::rtos::deleteTask(nullptr);
}

bool NeighborDiscoveryService::triggerScan(uint32_t timeoutMs) {
    if (m_isScanning) {
        return false;
    }
    if (!s_backend || !s_backend->isNetworkConnected()) {
        return false;
    }

    m_isScanning = true;

    auto* params = new DiscoveryTaskParams{this, timeoutMs};
    cbdos::rtos::TaskHandle h = cbdos::rtos::createTask(
        NeighborDiscoveryService::discoveryTask,
        "disco_worker",
        8192,
        params,
        4,
        0 // Core 0 (no compite con LVGL en Core 1)
    );

    if (!h) {
        m_isScanning = false;
        delete params;
        return false;
    }

    return true;
}

} // namespace network
} // namespace cbdos
