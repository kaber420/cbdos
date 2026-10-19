#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace cbdos {
namespace radio {

/**
 * @brief Modo de autenticacion/cifrado de una red Wi-Fi
 */
enum class WifiAuthMode : uint8_t {
    Open = 0,
    WEP,
    WPA,
    WPA2,
    WPA3,
    WPA2Enterprise,
    WPA3Enterprise,
    Unknown
};

/**
 * @brief Banda de operacion estimada por canal
 */
enum class WifiBand : uint8_t {
    Ghz24 = 0,
    Ghz5,
    Unknown
};

/**
 * @brief Registro de un punto de acceso descubierto en el aire
 */
struct WifiApRecord {
    uint8_t bssid[6] = {0};
    std::string bssidStr;
    std::string ssid;
    std::string vendor;
    uint8_t channel = 0;
    int8_t rssi = -100;
    WifiAuthMode auth = WifiAuthMode::Unknown;
    WifiBand band = WifiBand::Unknown;
    bool hidden = false;
    bool wps = false;
    bool rogueSuspect = false;   // Mismo SSID visto con varios BSSID (posible Evil Twin)
    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs = 0;
    uint32_t hitCount = 0;
};

/**
 * @brief Tipos de ataque/anomalia Wi-Fi detectables por el IDS pasivo
 */
enum class WifiAlertType : uint8_t {
    None = 0,
    DeauthFlood,     // Rafaga de tramas de desautenticacion (0x0C)
    DisassocFlood,   // Rafaga de tramas de desasociacion (0x0A)
    BeaconSpam,      // Inundacion de balizas con MAC rotativa
    RogueAp,         // Evil Twin / AP clonado
    Karma            // Respuestas a sondas no solicitadas
};

/**
 * @brief Estructura de alerta del centinela pasivo Wi-Fi
 */
struct WifiAlert {
    bool active = false;
    WifiAlertType type = WifiAlertType::None;
    uint32_t packetCount = 0;
    float packetsPerSec = 0.0f;
    uint32_t lastAlertMs = 0;
    std::string description;
    std::string targetBssid;
};

/**
 * @brief Configuracion del centinela pasivo Wi-Fi (IDS)
 */
struct WifiIdsConfig {
    bool enabled = true;
    bool detectDeauth = true;
    bool detectDisassoc = true;
    bool detectBeaconSpam = true;
    bool detectRogueAp = true;
    uint16_t windowMs = 2000;
    uint8_t minPacketsInWindow = 5;
    uint8_t beaconUniqueThreshold = 20; // BSSID unicos con MAC local para alertar (modelo Bruce)
    bool logging = true;
    uint8_t maxLogEntries = 30;
    bool audioAlert = false;
};

/**
 * @brief Tipo de trama 802.11 (campo Frame Control, bits 2-3)
 */
enum class WifiFrameType : uint8_t {
    Management = 0,
    Control,
    Data,
    Extension
};

/**
 * @brief Trama 802.11 cruda resumida para el motor IDS (sin payload)
 */
struct WifiRawFrame {
    WifiFrameType type = WifiFrameType::Management;
    uint8_t subtype = 0;       // Management subtypes: 0x08 Beacon, 0x0A Disassoc, 0x0C Deauth
    uint8_t sa[6] = {0};       // Direccion origen (addr2)
    uint8_t da[6] = {0};       // Direccion destino (addr1)
    uint8_t bssid[6] = {0};    // Direccion BSSID (addr3)
    uint8_t channel = 0;
    int8_t rssi = -100;
    uint32_t timestampMs = 0;
};

/**
 * @brief Etiqueta legible del modo de autenticacion
 */
inline const char* getWifiAuthName(WifiAuthMode m) {
    switch (m) {
        case WifiAuthMode::Open: return "ABIERTA";
        case WifiAuthMode::WEP: return "WEP";
        case WifiAuthMode::WPA: return "WPA";
        case WifiAuthMode::WPA2: return "WPA2";
        case WifiAuthMode::WPA3: return "WPA3";
        case WifiAuthMode::WPA2Enterprise: return "WPA2-ENT";
        case WifiAuthMode::WPA3Enterprise: return "WPA3-ENT";
        default: return "?";
    }
}

/**
 * @brief Etiqueta legible del tipo de alerta IDS
 */
inline const char* getWifiAlertName(WifiAlertType t) {
    switch (t) {
        case WifiAlertType::DeauthFlood: return "DEAUTH FLOOD";
        case WifiAlertType::DisassocFlood: return "DISASSOC FLOOD";
        case WifiAlertType::BeaconSpam: return "BEACON SPAM";
        case WifiAlertType::RogueAp: return "ROGUE AP / EVIL TWIN";
        case WifiAlertType::Karma: return "KARMA";
        default: return "NINGUNA";
    }
}

} // namespace radio
} // namespace cbdos
