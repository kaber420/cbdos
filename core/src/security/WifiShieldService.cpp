#include "WifiShieldService.hpp"
#include "OuiDatabase.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace cbdos {
namespace security {

static uint32_t wifiNowMs() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
    );
}

static std::string wifiFormatMac(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

WifiShieldService& WifiShieldService::getInstance() {
    static WifiShieldService s_instance;
    return s_instance;
}

WifiShieldService::WifiShieldService() {
    init();
}

bool WifiShieldService::init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        m_aps.reserve(64);
        m_deauthTimes.reserve(64);
        m_disassocTimes.reserve(64);
        m_beaconTimes.reserve(128);
        m_alertLog.reserve(32);
        m_initialized = true;
    }
    return true;
}

// ────────────────────────────────────────────────────────────────
// Control de radio
// ────────────────────────────────────────────────────────────────

bool WifiShieldService::startScan() {
    ScanControlHandler handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scanning = true;
        handler = m_scanControl;
    }
    return handler ? handler(true) : false;
}

void WifiShieldService::stopScan() {
    ScanControlHandler handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scanning = false;
        handler = m_scanControl;
    }
    if (handler) handler(false);
}

bool WifiShieldService::isScanning() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scanning;
}

void WifiShieldService::registerScanControl(ScanControlHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_scanControl = handler;
}

void WifiShieldService::registerSnifferControl(SnifferControlHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snifferControl = handler;
}

// ────────────────────────────────────────────────────────────────
// Air Recon
// ────────────────────────────────────────────────────────────────

void WifiShieldService::setApCallback(WifiApCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apCallback = cb;
}

std::vector<radio::WifiApRecord> WifiShieldService::getAccessPoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_aps;
}

size_t WifiShieldService::getAccessPointCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_aps.size();
}

void WifiShieldService::clearAccessPoints() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aps.clear();
}

void WifiShieldService::processApRecord(const radio::WifiApRecord& ap) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t now = ap.lastSeenMs ? ap.lastSeenMs : wifiNowMs();

    auto it = std::find_if(m_aps.begin(), m_aps.end(), [&](const radio::WifiApRecord& a) {
        return memcmp(a.bssid, ap.bssid, 6) == 0;
    });

    if (it != m_aps.end()) {
        it->rssi = ap.rssi;
        it->channel = ap.channel;
        it->lastSeenMs = now;
        it->hitCount++;
        if (!ap.ssid.empty()) it->ssid = ap.ssid;
        if (ap.auth != radio::WifiAuthMode::Unknown) it->auth = ap.auth;
    } else {
        if (m_aps.size() >= 80) {
            m_aps.erase(m_aps.begin());
        }
        radio::WifiApRecord rec = ap;
        rec.bssidStr = wifiFormatMac(ap.bssid);
        rec.vendor = network::lookupVendorByMac(ap.bssid);
        rec.firstSeenMs = now;
        rec.lastSeenMs = now;
        rec.hitCount = 1;
        m_aps.push_back(rec);
        it = m_aps.end() - 1;
    }

    // Deteccion de Evil Twin: mismo SSID visible en varios BSSID
    if (m_idsConfig.enabled && m_idsConfig.detectRogueAp && !it->ssid.empty()) {
        bool duplicate = false;
        for (auto& other : m_aps) {
            if (&other == &(*it)) continue;
            if (other.ssid == it->ssid && memcmp(other.bssid, it->bssid, 6) != 0) {
                duplicate = true;
                other.rogueSuspect = true;
            }
        }
        if (duplicate) {
            it->rogueSuspect = true;
            char desc[96];
            snprintf(desc, sizeof(desc), "Evil Twin: '%s' anunciado por varios BSSID", it->ssid.c_str());
            radio::WifiAlert alert;
            alert.active = true;
            alert.type = radio::WifiAlertType::RogueAp;
            alert.packetCount = 1;
            alert.packetsPerSec = 0.0f;
            alert.lastAlertMs = now;
            alert.description = desc;
            alert.targetBssid = it->bssidStr;
            m_alert = alert;
            if (m_idsConfig.logging) {
                while (m_alertLog.size() >= (m_idsConfig.maxLogEntries ? m_idsConfig.maxLogEntries : 1)) {
                    m_alertLog.erase(m_alertLog.begin());
                }
                m_alertLog.push_back("[ROGUE] " + it->ssid + " (" + it->bssidStr + ")");
            }
            if (m_alertCallback) m_alertCallback(m_alert);
        }
    }

    if (m_apCallback) m_apCallback(*it);
}

// ────────────────────────────────────────────────────────────────
// IDS Sentinel
// ────────────────────────────────────────────────────────────────

void WifiShieldService::setAlertCallback(WifiAlertCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alertCallback = cb;
}

radio::WifiAlert WifiShieldService::getAlert() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_alert;
}

void WifiShieldService::clearAlert() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_alert = radio::WifiAlert();
}

uint32_t WifiShieldService::getTotalAlertPackets() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalAlertPackets;
}

std::vector<std::string> WifiShieldService::getAlertLog() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_alertLog;
}

radio::WifiIdsConfig WifiShieldService::getIdsConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idsConfig;
}

void WifiShieldService::setIdsConfig(const radio::WifiIdsConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_idsConfig = cfg;
    if (!cfg.enabled) m_alert = radio::WifiAlert();
}

void WifiShieldService::setIdsEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_idsConfig.enabled = enabled;
    if (!enabled) {
        m_alert = radio::WifiAlert();
        m_beaconBssids.clear();
        m_beaconAlertLatched = false;
    }
}

bool WifiShieldService::isIdsEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idsConfig.enabled;
}

void WifiShieldService::setIdsDetection(radio::WifiAlertType type, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (type) {
        case radio::WifiAlertType::DeauthFlood:   m_idsConfig.detectDeauth = enabled; break;
        case radio::WifiAlertType::DisassocFlood: m_idsConfig.detectDisassoc = enabled; break;
        case radio::WifiAlertType::BeaconSpam:    m_idsConfig.detectBeaconSpam = enabled; break;
        case radio::WifiAlertType::RogueAp:       m_idsConfig.detectRogueAp = enabled; break;
        default: break;
    }
}

uint32_t WifiShieldService::pruneWindowLocked(std::vector<uint32_t>& stamps, uint32_t now) const {
    uint32_t windowMs = m_idsConfig.windowMs ? m_idsConfig.windowMs : 2000;
    stamps.erase(
        std::remove_if(stamps.begin(), stamps.end(),
                       [&](uint32_t t) { return (now - t) > windowMs; }),
        stamps.end());
    return static_cast<uint32_t>(stamps.size());
}

void WifiShieldService::pushAlertLocked(radio::WifiAlertType type, const char* desc,
                                        const std::string& target) {
    uint32_t now = wifiNowMs();
    uint32_t windowMs = m_idsConfig.windowMs ? m_idsConfig.windowMs : 2000;
    size_t n = 0;
    switch (type) {
        case radio::WifiAlertType::DeauthFlood:   n = m_deauthTimes.size(); break;
        case radio::WifiAlertType::DisassocFlood: n = m_disassocTimes.size(); break;
        case radio::WifiAlertType::BeaconSpam:    n = m_beaconTimes.size(); break;
        default: n = 1; break;
    }

    radio::WifiAlert alert;
    alert.active = true;
    alert.type = type;
    alert.packetCount = m_totalAlertPackets;
    alert.packetsPerSec = static_cast<float>(n) / (static_cast<float>(windowMs) / 1000.0f);
    alert.lastAlertMs = now;
    alert.description = desc;
    alert.targetBssid = target;
    m_alert = alert;

    if (m_idsConfig.logging) {
        while (m_alertLog.size() >= (m_idsConfig.maxLogEntries ? m_idsConfig.maxLogEntries : 1)) {
            m_alertLog.erase(m_alertLog.begin());
        }
        char entry[128];
        snprintf(entry, sizeof(entry), "[%u ms] %s", static_cast<unsigned>(now), desc);
        m_alertLog.push_back(std::string(entry));
    }
    if (m_alertCallback) m_alertCallback(m_alert);
}

void WifiShieldService::evaluateFrames(const radio::WifiRawFrame& frame) {
    if (!m_idsConfig.enabled) return;

    uint32_t now = frame.timestampMs ? frame.timestampMs : wifiNowMs();
    uint8_t threshold = m_idsConfig.minPacketsInWindow ? m_idsConfig.minPacketsInWindow : 1;

    if (frame.type == radio::WifiFrameType::Management) {
        if (frame.subtype == 0x0C && m_idsConfig.detectDeauth) {
            m_deauthTimes.push_back(now);
            pruneWindowLocked(m_deauthTimes, now);
            if (m_deauthTimes.size() >= threshold) {
                pushAlertLocked(radio::WifiAlertType::DeauthFlood,
                                "Deauth Flood: rafaga de desautenticacion 802.11",
                                wifiFormatMac(frame.sa));
            }
        } else if (frame.subtype == 0x0A && m_idsConfig.detectDisassoc) {
            m_disassocTimes.push_back(now);
            pruneWindowLocked(m_disassocTimes, now);
            if (m_disassocTimes.size() >= threshold) {
                pushAlertLocked(radio::WifiAlertType::DisassocFlood,
                                "Disassoc Flood: rafaga de desasociacion 802.11",
                                wifiFormatMac(frame.sa));
            }
        } else if (frame.subtype == 0x08 && m_idsConfig.detectBeaconSpam) {
            uint32_t windowMs = m_idsConfig.windowMs ? m_idsConfig.windowMs : 2000;

            // Tasa de balizas: METRICA INFORMATIVA (no dispara alerta por si sola)
            m_beaconTimes.push_back(now);
            pruneWindowLocked(m_beaconTimes, now);

            // Deteccion real: BSSID unicos con MAC localmente administrada (modelo Bruce).
            // Un AP legitimo reutiliza su MAC; una herramienta de spam rota MACs distintas.
            const uint8_t* bssid = frame.bssid;
            if (bssid[0] & 0x02) {
                uint64_t key = 0;
                for (int i = 0; i < 6; ++i) key = (key << 8) | bssid[i];
                m_beaconBssids[key] = now;
            }
            for (auto it = m_beaconBssids.begin(); it != m_beaconBssids.end();) {
                if ((now - it->second) > windowMs) it = m_beaconBssids.erase(it);
                else ++it;
            }

            uint8_t uniqueThreshold = m_idsConfig.beaconUniqueThreshold ? m_idsConfig.beaconUniqueThreshold : 20;
            if (m_beaconBssids.size() >= uniqueThreshold) {
                if (!m_beaconAlertLatched) {
                    m_beaconAlertLatched = true;
                    float rate = static_cast<float>(m_beaconTimes.size()) /
                                 (static_cast<float>(windowMs) / 1000.0f);
                    char desc[112];
                    snprintf(desc, sizeof(desc),
                             "Beacon Spam: %u BSSID unicos con MAC aleatoria (%.1f pps)",
                             static_cast<unsigned>(m_beaconBssids.size()), rate);
                    pushAlertLocked(radio::WifiAlertType::BeaconSpam, desc, "<multiple>");
                }
            } else {
                m_beaconAlertLatched = false;
            }
        }
    }
}

void WifiShieldService::processRawFrame(const radio::WifiRawFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_snifferEnabled) {
        m_sniffedFrames++;
        if (frame.type == radio::WifiFrameType::Management) {
            if (frame.subtype == 0x08) m_beaconCount++;
            else if (frame.subtype == 0x0C) m_deauthCount++;
        } else if (frame.type == radio::WifiFrameType::Data) {
            m_dataFrames++;
        }
    }

    if (frame.type == radio::WifiFrameType::Management) {
        if (frame.subtype == 0x0C || frame.subtype == 0x0A || frame.subtype == 0x08) {
            m_totalAlertPackets++;
            evaluateFrames(frame);
        }
    }
}

// ────────────────────────────────────────────────────────────────
// Sniffer
// ────────────────────────────────────────────────────────────────

void WifiShieldService::setSnifferEnabled(bool enabled) {
    SnifferControlHandler handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snifferEnabled = enabled;
        handler = m_snifferControl;
    }
    if (handler) handler(enabled);
}

bool WifiShieldService::isSnifferEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snifferEnabled;
}

uint32_t WifiShieldService::getSniffedFrameCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sniffedFrames;
}

uint32_t WifiShieldService::getBeaconCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_beaconCount;
}

uint32_t WifiShieldService::getDeauthCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_deauthCount;
}

uint32_t WifiShieldService::getDataFrameCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dataFrames;
}

void WifiShieldService::resetSnifferCounters() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sniffedFrames = 0;
    m_beaconCount = 0;
    m_deauthCount = 0;
    m_dataFrames = 0;
}

} // namespace security
} // namespace cbdos
