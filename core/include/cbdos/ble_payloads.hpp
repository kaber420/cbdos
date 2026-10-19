#pragma once

// Generadores de payloads publicitarios BLE.
//
// Código agnóstico: no conoce NimBLE, Bluedroid ni FreeRTOS. Cada generador
// escribe un buffer de AD structures crudas (formato legacy de 31 bytes) que el
// HAL entrega directamente al stack. Esto permite validar los bytes en host y
// los mantiene idénticos a los que emite el stack de referencia.
//
// Los formats provienen de la suite BLE de Bruce (referencias/bruce,
// src/modules/ble/ble_spam.cpp), reescritos como funciones puras.

#include "cbdos/ble.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace cbdos {
namespace ble {

/** Tamaño máximo del payload publicitario legacy. */
static constexpr size_t BLE_ADV_PAYLOAD_MAX = 31;

/** Payload publicitario ya construido. */
struct BleAdvPayload {
    uint8_t data[BLE_ADV_PAYLOAD_MAX] = {0};
    size_t len = 0;
    uint8_t scanRsp[BLE_ADV_PAYLOAD_MAX] = {0};
    size_t scanRspLen = 0;
    bool isApple = false;
    bool scannable = false;
    bool hasCustomMac = false;
    uint8_t customMac[6] = {0};
    bool valid = false;
};

/** Dispositivo Apple capaz de provocar popup de vinculación por ProximityPair. */
struct AppleProximityDevice {
    const char* name;
    uint16_t deviceId;
};

/** Preset de nombre para Microsoft Swift Pair. */
struct SwiftPairPreset {
    const char* name;
};

/** Preset de periférico HID estándar (Teclados, Ratones, Gamepads). */
struct HidPreset {
    const char* name;
    uint16_t appearance;
};

/** Generador de payloads BLE. */
class BlePayloadRegistry {
public:
    /** Siembra el PRNG interno con entropía del sistema/hardware. */
    static void seed(uint64_t entropy);

    /** Construye un payload nuevo. `variantIndex` selecciona el modelo concreto
     *  dentro de la categoría; cualquier índice fuera de rango se trata como
     *  "Random / All" y se elige un modelo al azar. */
    static BleAdvPayload build(BlePayloadCategory category, int variantIndex,
                               const char* customName = nullptr);

    /** Construye el payload y aplica un nombre propio (solo Swift Pair). */
    static BleAdvPayload buildSwiftPair(const char* customName);

    /** Construye un paquete de spoofing de AirTag / Tracker a partir del payload
     *  crudo capturado (mismo formato que airtag_scan.c de GhostESP). */
    static BleAdvPayload buildAirTagSpoof(const uint8_t* rawPayload, size_t rawLen,
                                          const uint8_t mac[6]);

    /** Variantes disponibles para una categoría, en el orden que usa la UI. */
    static std::vector<std::string> getVariantNames(BlePayloadCategory category);
    static size_t getVariantCount(BlePayloadCategory category);

    /** Etiqueta legible de la categoría. */
    static const char* getCategoryName(BlePayloadCategory category);

    /** Catálogo Apple ProximityPair compartido (27 modelos). */
    static const std::vector<AppleProximityDevice>& getAppleProximityDevices();

    /** Catálogo de presets de Swift Pair. */
    static const std::vector<SwiftPairPreset>& getSwiftPairPresets();

    /** Catálogo de presets de periféricos HID. */
    static const std::vector<HidPreset>& getHidPresets();

    /** Constructor de baliza genérica configurable. */
    static BleAdvPayload buildGenericBeacon(const char* deviceName, uint16_t uuid16,
                                           uint16_t major, uint16_t minor,
                                           int8_t txPower);
};

} // namespace ble
} // namespace cbdos