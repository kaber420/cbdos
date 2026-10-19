#pragma once

#include "cbdos/wifi_types.hpp"
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <map>

namespace cbdos {
namespace security {

/**
 * @brief Callback invocado cuando se descubre o actualiza un AP
 */
using WifiApCallback = std::function<void(const radio::WifiApRecord& ap)>;

/**
 * @brief Callback invocado cuando el IDS detecta una anomalia
 */
using WifiAlertCallback = std::function<void(const radio::WifiAlert& alert)>;

/**
 * @brief Servicio central de Wi-Fi Shield: Recon pasivo, IDS Sentinel y Sniffer.
 * Logica 100% agnostica de plataforma; el acceso a la radio se inyecta via HAL.
 */
class WifiShieldService {
public:
    static WifiShieldService& getInstance();

    // Ciclo de vida
    bool init();

    // Control de escaneo (Recon)
    bool startScan(uint32_t durationSeconds = 0);
    void stopScan();
    bool isScanning() const;
    uint32_t getScanRevision() const;
    void notifyScanStopped();

    // Ingesta de datos desde el HAL de radio
    void processApRecord(const radio::WifiApRecord& ap);
    void processRawFrame(const radio::WifiRawFrame& frame);

    // Air Recon (lista de APs)
    std::vector<radio::WifiApRecord> getAccessPoints() const;
    size_t getAccessPointCount() const;
    void clearAccessPoints();
    void setApCallback(WifiApCallback cb);

    // IDS Sentinel
    radio::WifiAlert getAlert() const;
    void clearAlert();
    uint32_t getTotalAlertPackets() const;
    std::vector<std::string> getAlertLog() const;
    void setAlertCallback(WifiAlertCallback cb);

    radio::WifiIdsConfig getIdsConfig() const;
    void setIdsConfig(const radio::WifiIdsConfig& cfg);
    void setIdsEnabled(bool enabled);
    bool isIdsEnabled() const;
    void setIdsDetection(radio::WifiAlertType type, bool enabled);

    // Sniffer (metricas por tipo de trama)
    void setSnifferEnabled(bool enabled);
    bool isSnifferEnabled() const;
    uint32_t getSniffedFrameCount() const;
    uint32_t getBeaconCount() const;
    uint32_t getDeauthCount() const;
    uint32_t getDataFrameCount() const;
    void resetSnifferCounters();

    // Controladores de hardware inyectados por el BSP
    using ScanControlHandler = std::function<bool(bool start, uint32_t durationSeconds)>;
    using SnifferControlHandler = std::function<bool(bool enable)>;
    void registerScanControl(ScanControlHandler handler);
    void registerSnifferControl(SnifferControlHandler handler);

private:
    WifiShieldService();
    ~WifiShieldService() = default;
    WifiShieldService(const WifiShieldService&) = delete;
    WifiShieldService& operator=(const WifiShieldService&) = delete;

    void evaluateFrames(const radio::WifiRawFrame& frame);
    void flagRogueApLocked();
    void pushAlertLocked(radio::WifiAlertType type, const char* desc, const std::string& target);
    uint32_t pruneWindowLocked(std::vector<uint32_t>& stamps, uint32_t now) const;

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    std::atomic<bool> m_scanning{false};
    std::atomic<uint32_t> m_scanRevision{0};
    bool m_snifferEnabled = false;
    ScanControlHandler m_scanControl = nullptr;
    SnifferControlHandler m_snifferControl = nullptr;

    std::vector<radio::WifiApRecord> m_aps;
    WifiApCallback m_apCallback = nullptr;

    // IDS
    radio::WifiAlert m_alert;
    radio::WifiIdsConfig m_idsConfig;
    uint32_t m_totalAlertPackets = 0;
    std::vector<uint32_t> m_deauthTimes;
    std::vector<uint32_t> m_disassocTimes;
    std::vector<uint32_t> m_beaconTimes;              // Tasa informativa (no dispara alerta)
    std::map<uint64_t, uint32_t> m_beaconBssids;      // BSSID unicos con MAC local -> ultima vez (ms)
    bool m_beaconAlertLatched = false;
    std::vector<std::string> m_alertLog;
    WifiAlertCallback m_alertCallback = nullptr;

    // Sniffer
    uint32_t m_sniffedFrames = 0;
    uint32_t m_beaconCount = 0;
    uint32_t m_deauthCount = 0;
    uint32_t m_dataFrames = 0;
};

} // namespace security
} // namespace cbdos
