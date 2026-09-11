#include "cbdos/config_manager.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/system.hpp"

static const char* TAG_CFG = "ConfigManager";

static SystemConfig s_cachedSys;
static WiFiConfig s_cachedWiFi;
static LoRaConfig s_cachedLoRa;
static FLRCConfig s_cachedFLRC;
static TimeConfig s_cachedTime;
static std::vector<GatewayConfig> s_cachedGateways;
static std::string s_cachedActiveGwId = "";

bool ConfigManager::init() {
    auto* backend = cbdos::persistence::getBackend();
    if (backend) {
        // Inicializar o verificar que los namespaces respondan
        if (backend->begin("cbdos_sys", true)) {
            backend->end();
        }
    }
    return true;
}

bool ConfigManager::clearLegacyConfig() {
    auto* backend = cbdos::persistence::getBackend();
    if (backend) {
        if (backend->begin("tablehub", false)) {
            backend->clear();
            backend->end();
            cbdos::system::log(cbdos::system::LogLevel::Info, TAG_CFG, "Namespace legacy 'tablehub' borrado");
            return true;
        }
    }
    return true;
}

bool ConfigManager::clearAllNvs() {
    auto* backend = cbdos::persistence::getBackend();
    const char* namespaces[] = {"cbdos_sys", "cbdos_wifi", "cbdos_radio", "cbdos_time", "cbdos_lora", "cbdos_flrc", "wifi", "tablehub"};
    if (backend) {
        for (const char* ns : namespaces) {
            if (backend->begin(ns, false)) {
                backend->clear();
                backend->end();
            }
        }
    }
    s_cachedSys = SystemConfig{};
    s_cachedWiFi = WiFiConfig{};
    s_cachedLoRa = LoRaConfig{};
    s_cachedFLRC = FLRCConfig{};
    s_cachedTime = TimeConfig{};
    s_cachedGateways.clear();
    s_cachedActiveGwId.clear();
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_CFG, "Todos los namespaces NVS borrados correctamente");
    return true;
}

// ─── System Config ───
bool ConfigManager::loadSystem(SystemConfig& cfg) {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", true)) {
        cfg.brightness = backend->getUChar("bright", 70);
        cfg.volume = backend->getUChar("vol", 70);
        cfg.autoConnectWifi = backend->getBool("wifi_auto", false);
        cfg.gmtOffsetSeconds = backend->getInt("gmt_off", -21600);
        cfg.daylightOffsetSeconds = backend->getInt("dst_off", 0);
        cfg.screenTimeoutSeconds = backend->getUInt("scr_tout", 60);
        cfg.defaultTheme = backend->getString("theme", "dark");
        cfg.language = backend->getString("lang", "es");
        if (cfg.language != "en") cfg.language = "es";
        backend->end();
        s_cachedSys = cfg;
        return true;
    }
    cfg = s_cachedSys;
    return true;
}

bool ConfigManager::saveSystem(const SystemConfig& cfg) {
    s_cachedSys = cfg;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setUChar("bright", cfg.brightness);
        backend->setUChar("vol", cfg.volume);
        backend->setBool("wifi_auto", cfg.autoConnectWifi);
        backend->setInt("gmt_off", cfg.gmtOffsetSeconds);
        backend->setInt("dst_off", cfg.daylightOffsetSeconds);
        backend->setUInt("scr_tout", cfg.screenTimeoutSeconds);
        backend->setString("theme", cfg.defaultTheme);
        std::string lang = (cfg.language == "en") ? "en" : "es";
        backend->setString("lang", lang);
        backend->end();
        return true;
    }
    return false;
}

uint8_t ConfigManager::getBrightness() {
    SystemConfig cfg;
    loadSystem(cfg);
    return cfg.brightness;
}

void ConfigManager::setBrightness(uint8_t percent) {
    s_cachedSys.brightness = percent;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setUChar("bright", percent);
        backend->end();
    }
}

uint8_t ConfigManager::getVolume() {
    SystemConfig cfg;
    loadSystem(cfg);
    return cfg.volume;
}

void ConfigManager::setVolume(uint8_t percent) {
    s_cachedSys.volume = percent;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setUChar("vol", percent);
        backend->end();
    }
}

bool ConfigManager::isWifiAutoConnect() {
    SystemConfig cfg;
    loadSystem(cfg);
    return cfg.autoConnectWifi;
}

void ConfigManager::setWifiAutoConnect(bool enable) {
    s_cachedSys.autoConnectWifi = enable;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setBool("wifi_auto", enable);
        backend->end();
    }
}

int32_t ConfigManager::getTimezoneOffset() {
    SystemConfig cfg;
    loadSystem(cfg);
    return cfg.gmtOffsetSeconds;
}

void ConfigManager::setTimezoneOffset(int32_t offsetSec) {
    s_cachedSys.gmtOffsetSeconds = offsetSec;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setInt("gmt_off", offsetSec);
        backend->end();
    }
}

uint32_t ConfigManager::getIdleTimeoutSec() {
    SystemConfig cfg;
    loadSystem(cfg);
    return cfg.screenTimeoutSeconds;
}

void ConfigManager::setIdleTimeoutSec(uint32_t seconds) {
    s_cachedSys.screenTimeoutSeconds = seconds;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setUInt("scr_tout", seconds);
        backend->end();
    }
}

std::string ConfigManager::getLanguage() {
    SystemConfig cfg;
    loadSystem(cfg);
    return cfg.language;
}

void ConfigManager::setLanguage(const std::string& lang) {
    std::string code = (lang == "en") ? "en" : "es";
    s_cachedSys.language = code;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setString("lang", code);
        backend->end();
    }
}

// ─── WiFi Config ───
bool ConfigManager::loadWiFi(WiFiConfig& cfg) {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_wifi", true)) {
        cfg.ssid = backend->getString("ssid", "");
        cfg.password = backend->getString("pass", "");
        cfg.useStaticIp = backend->getBool("static_en", false);
        cfg.staticIp = backend->getString("ip", "");
        cfg.gateway = backend->getString("gw", "");
        cfg.subnet = backend->getString("sub", "");
        cfg.dns1 = backend->getString("dns1", "");
        cfg.dns2 = backend->getString("dns2", "");
        backend->end();
        s_cachedWiFi = cfg;
    } else {
        cfg = s_cachedWiFi;
    }

    // Fallback de migración: buscar en namespace legacy "wifi" si no hay SSID
    if (cfg.ssid.empty() && backend && backend->begin("wifi", true)) {
        cfg.ssid = backend->getString("ssid", "");
        cfg.password = backend->getString("pass", "");
        backend->end();
        if (!cfg.ssid.empty()) {
            saveWiFi(cfg);
        }
    }

    return !cfg.ssid.empty();
}

bool ConfigManager::saveWiFi(const WiFiConfig& cfg) {
    s_cachedWiFi = cfg;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_wifi", false)) {
        backend->setString("ssid", cfg.ssid);
        backend->setString("pass", cfg.password);
        backend->setBool("static_en", cfg.useStaticIp);
        backend->setString("ip", cfg.staticIp);
        backend->setString("gw", cfg.gateway);
        backend->setString("sub", cfg.subnet);
        backend->setString("dns1", cfg.dns1);
        backend->setString("dns2", cfg.dns2);
        backend->end();
        return true;
    }
    return false;
}

// ─── LoRa Config ───
bool ConfigManager::loadLoRa(LoRaConfig& cfg) {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_lora", true)) {
        cfg.frequency = backend->getFloat("freq", 915.0f);
        cfg.txPower = (int8_t)backend->getUChar("txpwr", 14);
        cfg.bandwidth = backend->getFloat("bw", 250.0f);
        cfg.spreadingFactor = backend->getUChar("sf", 7);
        cfg.codingRate = backend->getUChar("cr", 5);
        cfg.syncWord = backend->getUShort("sync", 0x32);
        cfg.enableCRC = backend->getBool("crc", true);
        cfg.preambleLength = backend->getUShort("preamb", 8);
        backend->end();
        s_cachedLoRa = cfg;
        return true;
    }
    cfg = s_cachedLoRa;
    return true;
}

bool ConfigManager::saveLoRa(const LoRaConfig& cfg) {
    s_cachedLoRa = cfg;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_lora", false)) {
        backend->setFloat("freq", cfg.frequency);
        backend->setUChar("txpwr", (uint8_t)cfg.txPower);
        backend->setFloat("bw", cfg.bandwidth);
        backend->setUChar("sf", cfg.spreadingFactor);
        backend->setUChar("cr", cfg.codingRate);
        backend->setUShort("sync", cfg.syncWord);
        backend->setBool("crc", cfg.enableCRC);
        backend->setUShort("preamb", cfg.preambleLength);
        backend->end();
        return true;
    }
    return false;
}

// ─── FLRC Config ───
bool ConfigManager::loadFLRC(FLRCConfig& cfg) {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_flrc", true)) {
        cfg.frequency = backend->getFloat("freq", 2.400f);
        cfg.txPower = (int8_t)backend->getUChar("txpwr", 10);
        cfg.bandwidth = backend->getFloat("bw", 1.2f);
        cfg.dataRate = backend->getUChar("rate", 1);
        cfg.codingRate = backend->getUChar("cr", 2);
        cfg.syncWord = backend->getUShort("sync", 0x7B5A);
        cfg.enableCRC = backend->getBool("crc", true);
        cfg.preambleLength = backend->getUShort("preamb", 8);
        backend->end();
        s_cachedFLRC = cfg;
        return true;
    }
    cfg = s_cachedFLRC;
    return true;
}

bool ConfigManager::saveFLRC(const FLRCConfig& cfg) {
    s_cachedFLRC = cfg;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_flrc", false)) {
        backend->setFloat("freq", cfg.frequency);
        backend->setUChar("txpwr", (uint8_t)cfg.txPower);
        backend->setFloat("bw", cfg.bandwidth);
        backend->setUChar("rate", cfg.dataRate);
        backend->setUChar("cr", cfg.codingRate);
        backend->setUShort("sync", cfg.syncWord);
        backend->setBool("crc", cfg.enableCRC);
        backend->setUShort("preamb", cfg.preambleLength);
        backend->end();
        return true;
    }
    return false;
}

// ─── Time / NTP Config ───
bool ConfigManager::loadTime(TimeConfig& cfg) {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_time", true)) {
        cfg.ntpServer = backend->getString("server", "pool.ntp.org");
        cfg.gmtOffsetSeconds = backend->getInt("offset", -21600);
        cfg.daylightOffsetSeconds = backend->getInt("dst", 0);
        cfg.enabled = backend->getBool("enabled", true);
        backend->end();
        s_cachedTime = cfg;
        return true;
    }
    SystemConfig sysCfg;
    loadSystem(sysCfg);
    cfg.gmtOffsetSeconds = sysCfg.gmtOffsetSeconds;
    cfg.daylightOffsetSeconds = sysCfg.daylightOffsetSeconds;
    s_cachedTime = cfg;
    return true;
}

bool ConfigManager::saveTime(const TimeConfig& cfg) {
    s_cachedTime = cfg;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_time", false)) {
        backend->setString("server", cfg.ntpServer);
        backend->setInt("offset", cfg.gmtOffsetSeconds);
        backend->setInt("dst", cfg.daylightOffsetSeconds);
        backend->setBool("enabled", cfg.enabled);
        backend->end();
    }
    setTimezoneOffset(cfg.gmtOffsetSeconds);
    return true;
}

// ─── Gateways Config ───
bool ConfigManager::importGateway(const std::string& encPath, const std::string& pin, std::string& errorOut) {
    if (pin == "1234") {
        GatewayConfig gw;
        gw.id = "gw_mock_" + std::to_string(s_cachedGateways.size() + 1);
        gw.name = "Mock Gateway";
        gw.address = "127.0.0.1";
        gw.mqttPort = 1883;
        gw.authToken = "auth-mock-token-abc";
        s_cachedGateways.push_back(gw);
        return true;
    }
    errorOut = "PIN incorrecto (usa 1234).";
    return false;
}

bool ConfigManager::removeGateway(const std::string& gwId) {
    for (auto it = s_cachedGateways.begin(); it != s_cachedGateways.end(); ++it) {
        if (it->id == gwId) {
            s_cachedGateways.erase(it);
            if (s_cachedActiveGwId == gwId) s_cachedActiveGwId.clear();
            return true;
        }
    }
    return false;
}

std::vector<GatewayConfig> ConfigManager::listGateways() {
    return s_cachedGateways;
}

bool ConfigManager::setActiveGateway(const std::string& gwId) {
    s_cachedActiveGwId = gwId;
    return true;
}

bool ConfigManager::loadActiveGateway(GatewayConfig& gw) {
    for (const auto& item : s_cachedGateways) {
        if (item.id == s_cachedActiveGwId) {
            gw = item;
            return true;
        }
    }
    return false;
}

// ─── Radio Config (2.4 GHz) ───
static cbdos::radio::RadioConfig s_cachedRadio;

bool ConfigManager::loadRadio(cbdos::radio::RadioConfig& cfg) {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_radio", true)) {
        cfg.enabled = backend->getBool("enabled", true);
        uint8_t m = backend->getUChar("mode", static_cast<uint8_t>(cbdos::radio::RadioMode::EspNow));
        cfg.mode = static_cast<cbdos::radio::RadioMode>(m);
        cfg.channel = backend->getUChar("channel", 1);
        if (cfg.channel < 1 || cfg.channel > 13) cfg.channel = 1;
        cfg.txPower = static_cast<int8_t>(backend->getUChar("tx_pwr", 20));
        backend->end();
        s_cachedRadio = cfg;
        return true;
    }
    cfg = s_cachedRadio;
    return true;
}

bool ConfigManager::saveRadio(const cbdos::radio::RadioConfig& cfg) {
    s_cachedRadio = cfg;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("cbdos_radio", false)) {
        backend->setBool("enabled", cfg.enabled);
        backend->setUChar("mode", static_cast<uint8_t>(cfg.mode));
        backend->setUChar("channel", cfg.channel);
        backend->setUChar("tx_pwr", static_cast<uint8_t>(cfg.txPower));
        backend->end();
        return true;
    }
    return false;
}
