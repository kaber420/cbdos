#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "cbdos/radio.hpp"

struct WiFiConfig {
    std::string ssid;
    std::string password;
    bool useStaticIp = false;
    std::string staticIp;
    std::string gateway;
    std::string subnet;
    std::string dns1;
    std::string dns2;
};

struct LoRaConfig {
    float frequency = 915.0f;
    int8_t txPower = 14;
    float bandwidth = 250.0f;
    uint8_t spreadingFactor = 7;
    uint8_t codingRate = 5;
    uint16_t syncWord = 0x32;
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};

struct FLRCConfig {
    float frequency = 2.400f;
    int8_t txPower = 10;
    float bandwidth = 1.2f;
    uint8_t dataRate = 1;
    uint8_t codingRate = 2;
    uint16_t syncWord = 0x7B5A;
    bool enableCRC = true;
    uint16_t preambleLength = 8;
};

struct GatewayConfig {
    std::string id;
    std::string name;
    std::string address;
    std::string domain;
    int mqttPort = 1883;
    bool mqttUseTls = false;
    std::string authToken;
    std::string authType = "token";
    std::string discoveryMethod = "static";
    std::string notes;
};

struct TimeConfig {
    std::string ntpServer = "pool.ntp.org";
    int32_t gmtOffsetSeconds = -21600;
    int32_t daylightOffsetSeconds = 0;
    bool enabled = true;
};

struct SystemConfig {
    uint8_t brightness = 70;
    uint8_t volume = 70;
    bool autoConnectWifi = false;
    int32_t gmtOffsetSeconds = -21600; // -21600 = GMT-6 por defecto
    int32_t daylightOffsetSeconds = 0;
    uint32_t screenTimeoutSeconds = 60;
    std::string defaultTheme = "dark";
    std::string language = "es"; // Fase 1 i18n: "es" | "en" (NVS cbdos_sys/lang)
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool init();

    // System / Preferences
    bool loadSystem(SystemConfig& cfg);
    bool saveSystem(const SystemConfig& cfg);
    uint8_t getBrightness();
    void setBrightness(uint8_t percent);
    uint8_t getVolume();
    void setVolume(uint8_t percent);
    bool isWifiAutoConnect();
    void setWifiAutoConnect(bool enable);
    int32_t getTimezoneOffset();
    void setTimezoneOffset(int32_t offsetSec);
    uint32_t getIdleTimeoutSec();
    void setIdleTimeoutSec(uint32_t seconds);
    std::string getLanguage();
    void setLanguage(const std::string& lang); // "es" | "en", otro -> "es"

    // WiFi
    bool loadWiFi(WiFiConfig& cfg);
    bool saveWiFi(const WiFiConfig& cfg);

    // LoRa
    bool loadLoRa(LoRaConfig& cfg);
    bool saveLoRa(const LoRaConfig& cfg);

    // FLRC
    bool loadFLRC(FLRCConfig& cfg);
    bool saveFLRC(const FLRCConfig& cfg);

    // Time / NTP
    bool loadTime(TimeConfig& cfg);
    bool saveTime(const TimeConfig& cfg);

    // Gateways
    bool importGateway(const std::string& encPath, const std::string& pin, std::string& errorOut);
    bool removeGateway(const std::string& gwId);
    std::vector<GatewayConfig> listGateways();
    bool setActiveGateway(const std::string& gwId);
    bool loadActiveGateway(GatewayConfig& gw);

    // Radio Integrada (2.4 GHz)
    bool loadRadio(cbdos::radio::RadioConfig& cfg);
    bool saveRadio(const cbdos::radio::RadioConfig& cfg);

    // Limpieza NVS
    bool clearLegacyConfig();
    bool clearAllNvs();

private:
    ConfigManager() = default;
};
