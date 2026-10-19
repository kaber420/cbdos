#pragma once

#include "cbdos/ble_types.hpp"
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace cbdos {
namespace security {

/**
 * @brief Callback invocado cuando se detecta o actualiza un dispositivo BLE
 */
using BleDeviceCallback = std::function<void(const radio::BleDeviceRecord& dev)>;

/**
 * @brief Callback invocado cuando el centinela detecta una ráfaga de spam
 */
using BleSpamAlertCallback = std::function<void(const radio::BleSpamAlert& alert)>;

/**
 * @brief Servicio central de BLE Shield: Air Recon, Geiger Tracker y Spam IDS
 */
class BleShieldService {
public:
    static BleShieldService& getInstance();

    // Inicialización y control de escaneo
    bool init();
    void startScan();
    void stopScan();
    bool isScanning() const { return m_scanning; }

    // Procesamiento de paquetes de radio (GAP Advertising)
    void processRawPacket(const radio::BleRawPacket& pkt);

    // Métricas y acceso a dispositivos (Air Recon)
    std::vector<radio::BleDeviceRecord> getDevices() const;
    size_t getDeviceCount() const;
    void clearDevices();
    void setDeviceCallback(BleDeviceCallback cb);

    // Modo Localizador Geiger
    void setGeigerTarget(const uint8_t mac[6]);
    void clearGeigerTarget();
    bool hasGeigerTarget() const { return m_geigerActive; }
    bool getGeigerTargetMac(uint8_t outMac[6]) const;
    int8_t getGeigerRssi() const { return m_geigerSmoothedRssi; }
    uint8_t getGeigerProximity() const { return m_geigerProximityPercent; }
    float getGeigerDistanceMeters() const { return m_geigerEstDistance; }
    uint32_t getGeigerTickIntervalMs() const { return m_geigerTickIntervalMs; }
    void setGeigerAudioEnabled(bool enabled) { m_geigerAudioEnabled = enabled; }
    bool isGeigerAudioEnabled() const { return m_geigerAudioEnabled; }

    // Centinela Pasivo Anti-Spam (Spam IDS)
    radio::BleSpamAlert getSpamAlert() const;
    void clearSpamAlert();
    uint32_t getTotalSpamPackets() const { return m_totalSpamPackets; }
    std::vector<std::string> getSpamLog() const;
    void setSpamAlertCallback(BleSpamAlertCallback cb);

    // Configuracion del IDS (activar/desactivar funciones y sensibilidad)
    radio::BleSpamIdsConfig getIdsConfig() const;
    void setIdsConfig(const radio::BleSpamIdsConfig& cfg);
    void setIdsEnabled(bool enabled);
    bool isIdsEnabled() const;
    void setIdsDetection(radio::BleSpamType type, bool enabled);

    // Registro de driver de hardware real para control de escaneo
    using BleScanControlHandler = std::function<bool(bool start)>;
    void registerScanControl(BleScanControlHandler handler);

private:
    BleShieldService();
    ~BleShieldService() = default;
    BleShieldService(const BleShieldService&) = delete;
    BleShieldService& operator=(const BleShieldService&) = delete;

    void updateGeigerCalculations(int8_t rawRssi);
    void evaluateSpamPatterns(const radio::BleRawPacket& pkt, radio::BleDeviceCategory cat);

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    bool m_scanning = false;
    BleScanControlHandler m_scanControl = nullptr;


    // Dispositivos descubiertos
    std::vector<radio::BleDeviceRecord> m_devices;
    BleDeviceCallback m_deviceCallback = nullptr;

    // Geiger Tracker
    bool m_geigerActive = false;
    uint8_t m_geigerTargetMac[6] = {0};
    int8_t m_geigerSmoothedRssi = -100;
    uint8_t m_geigerProximityPercent = 0;
    float m_geigerEstDistance = 0.0f;
    uint32_t m_geigerTickIntervalMs = 1000;
    bool m_geigerAudioEnabled = true;

    // Spam IDS
    radio::BleSpamAlert m_currentAlert;
    radio::BleSpamIdsConfig m_idsConfig;
    uint32_t m_totalSpamPackets = 0;
    std::vector<uint32_t> m_recentSpamTimestamps;
    std::vector<std::string> m_spamLog;
    BleSpamAlertCallback m_spamCallback = nullptr;
};

} // namespace security
} // namespace cbdos
