#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace cbdos {
namespace radio {

/**
 * @brief Categorías taxonómicas de dispositivos BLE basadas en Bruce y Marauder
 */
enum class BleDeviceCategory : uint8_t {
    Unknown = 0,
    AppleAirTag,        // Baliza Find My / AirTag (Anti-Stalking)
    AppleAirPods,       // Auriculares AirPods / Beats (con batería L/R/Case)
    AppleDevice,        // Ecosistema Apple (Nearby Action, Apple TV, etc.)
    SamsungSmartTag,    // Rastreador SmartTag / SmartTag2 (SmartThings Find)
    SamsungDevice,      // Ecosistema Samsung (Galaxy Buds, etc.)
    MetaSmartGlasses,   // Gafas inteligentes con cámara (Ray-Ban Meta, Stories)
    AndroidFastPair,    // Accesorios Google GFPS (Pixel Buds, etc.)
    WindowsSwiftPair,   // Accesorios de emparejamiento rápido Windows
    FlipperZero,        // Flipper Zero BLE
    PeripheralMouse,    // Ratón inalámbrico BLE (Appearance 0x03C2)
    PeripheralKeyboard, // Teclado inalámbrico BLE (Appearance 0x03C1)
    PeripheralGamepad,  // Mando inalámbrico BLE (Appearance 0x03C4)
    DroneRemoteId       // Baliza de dron comercial (ASTM F3411 Remote ID)
};

/**
 * @brief Registro enriquecido de dispositivo BLE descubierto en el aire
 */
struct BleDeviceRecord {
    uint8_t mac[6] = {0};
    std::string macStr;
    std::string name;
    std::string vendor;
    std::string details;
    BleDeviceCategory category = BleDeviceCategory::Unknown;
    int8_t rssi = -100;
    int8_t txPower = 0;
    
    // Niveles de batería (0..100%, 0xFF = Desconocido)
    uint8_t batteryL = 0xFF;
    uint8_t batteryR = 0xFF;
    uint8_t batteryCase = 0xFF;
    bool hasBattery = false;
    
    // Banderas de auditoría táctica
    bool isTracker = false;       // Baliza de rastreo físico (AirTag, SmartTag, Tile)
    bool isCameraDevice = false;   // Dispositivo con cámara (Ray-Ban Meta)
    bool isRandomMac = true;      // MAC privada rotativa
    
    // Métricas de persistencia
    uint32_t firstSeenMs = 0;
    uint32_t lastSeenMs = 0;
    uint32_t hitCount = 0;

    // Payload publicitario crudo recibido (para clonación / spoofing real)
    uint8_t rawPayload[62] = {0};
    size_t rawPayloadLen = 0;
};

/**
 * @brief Paquete publicitario BLE sin procesar (GAP Advertising)
 */
struct BleRawPacket {
    uint8_t mac[6] = {0};
    bool isRandomMac = true;
    int8_t rssi = -100;
    uint8_t payload[62] = {0};
    size_t payloadLen = 0;
    uint32_t timestampMs = 0;
};

/**
 * @brief Tipos de ataques de inundación publicitaria BLE detectables
 */
enum class BleSpamType : uint8_t {
    None = 0,
    AppleJuice,       // Flood de popups iOS (AirPods, AppleTV, etc.)
    AndroidFastPair,  // Flood de anuncios FastPair GFPS
    WindowsSwiftPair, // Flood de popups SwiftPair para Windows
    SamsungBuds,      // Flood de vinculación Galaxy Buds
    FlipperSpam       // Flood originado por herramientas Flipper/Evil-M5
};

/**
 * @brief Estructura de alerta del centinela pasivo (Spam IDS)
 */
struct BleSpamAlert {
    bool active = false;
    BleSpamType type = BleSpamType::None;
    uint32_t packetCount = 0;
    float packetsPerSec = 0.0f;
    uint32_t lastAlertMs = 0;
    std::string description;
};

/**
 * @brief Configuracion del centinela pasivo anti-spam (Spam IDS)
 * Permite activar/desactivar el IDS completo, cada firma de ataque de forma
 * independiente y ajustar la sensibilidad de la ventana de deteccion.
 */
struct BleSpamIdsConfig {
    bool enabled = true;              // Interruptor maestro del IDS
    bool detectAppleJuice = true;     // Flood de emparejamiento iOS (AirPods/Nearby)
    bool detectFastPair = true;       // Flood Google Fast Pair (GFPS)
    bool detectSwiftPair = true;      // Flood Microsoft Swift Pair
    bool detectSamsungBuds = true;    // Flood de vinculacion Galaxy Buds
    bool detectFlipperSpam = true;    // Flood generico originado por Flipper/Evil-M5
    uint16_t windowMs = 2000;         // Tamano de la ventana deslizante (ms)
    uint8_t minPacketsInWindow = 3;   // Umbral de paquetes para disparar alerta
    bool logging = true;              // Registrar eventos en el log tactico
    uint8_t maxLogEntries = 30;       // Numero maximo de lineas del log
    bool audioAlert = false;          // Senal acustica opcional en alerta
};

/**
 * @brief Convierte una categoría a una etiqueta de texto legible
 */
inline const char* getCategoryName(BleDeviceCategory cat) {
    switch (cat) {
        case BleDeviceCategory::AppleAirTag: return "Apple AirTag";
        case BleDeviceCategory::AppleAirPods: return "Apple AirPods";
        case BleDeviceCategory::AppleDevice: return "Apple Device";
        case BleDeviceCategory::SamsungSmartTag: return "Samsung SmartTag";
        case BleDeviceCategory::SamsungDevice: return "Samsung Device";
        case BleDeviceCategory::MetaSmartGlasses: return "Ray-Ban Meta";
        case BleDeviceCategory::AndroidFastPair: return "Fast Pair";
        case BleDeviceCategory::WindowsSwiftPair: return "Swift Pair";
        case BleDeviceCategory::FlipperZero: return "Flipper Zero";
        case BleDeviceCategory::PeripheralMouse: return "Mouse BLE";
        case BleDeviceCategory::PeripheralKeyboard: return "Teclado BLE";
        case BleDeviceCategory::PeripheralGamepad: return "Gamepad BLE";
        case BleDeviceCategory::DroneRemoteId: return "Drone Remote ID";
        default: return "Dispositivo BLE";
    }
}

/**
 * @brief Retorna una etiqueta corta de la categoría (sin emojis UTF-8)
 */
inline const char* getCategoryTag(BleDeviceCategory cat) {
    switch (cat) {
        case BleDeviceCategory::AppleAirTag: return "[TAG]";
        case BleDeviceCategory::AppleAirPods: return "[PODS]";
        case BleDeviceCategory::AppleDevice: return "[APPLE]";
        case BleDeviceCategory::SamsungSmartTag: return "[TAG]";
        case BleDeviceCategory::SamsungDevice: return "[SAMSUNG]";
        case BleDeviceCategory::MetaSmartGlasses: return "[CAM]";
        case BleDeviceCategory::AndroidFastPair: return "[FAST]";
        case BleDeviceCategory::WindowsSwiftPair: return "[SWIFT]";
        case BleDeviceCategory::FlipperZero: return "[FLIPPER]";
        case BleDeviceCategory::PeripheralMouse: return "[MOUSE]";
        case BleDeviceCategory::PeripheralKeyboard: return "[KEYBD]";
        case BleDeviceCategory::PeripheralGamepad: return "[GAMEPAD]";
        case BleDeviceCategory::DroneRemoteId: return "[DRONE]";
        default: return "[BLE]";
    }
}


} // namespace radio
} // namespace cbdos
