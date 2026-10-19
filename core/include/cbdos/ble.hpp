#pragma once

// Contrato HAL de Bluetooth Low Energy (BLE) y fachada agnóstica del dominio.
//
// Este header define la frontera entre el núcleo agnóstico de CBDos y el stack
// de radio real. El core nunca incluye cabeceras de plataforma (NimBLEDevice.h,
// BLEDevice.h, host/ble_gap.h, etc.); únicamente habla con IBleAdapter.
//
// El estado del adaptador se mantiene en una sola instancia. Solo se admite una
// actividad de radio a la vez (escaneo o emisión) porque ambas saturan la misma
// radio: a intervalos de pocos milisegundos de emisión, el escaneo pierde la
// casi totalidad de sus resultados. El arbitraje vive en el core (ver
// BleTransmitter) y el HAL se limita a obedecer start/stop.

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace ble {

/**
 * @brief Estado actual del adaptador BLE
 */
enum class BleAdapterState : uint8_t {
    Unavailable = 0,  // El target no tiene radio BT (ej. ESP32-P4)
    Idle,             // Stack inicializado, sin actividad
    Scanning,         // Escaneo GAP activo (Air Recon)
    Advertising       // Emisión publicitaria activa (BLE Lab)
};

/**
 * @brief Categorías de payload de emisión soportadas por BLE Lab
 */
enum class BlePayloadCategory : uint8_t {
    ApplePairing = 0,      // Continuity ProximityPair, popup "nuevo dispositivo"
    AppleAction,           // Continuity NearbyAction, modales de acción
    AppleNotYourDevice,    // ProximityPair variante "Not Your Device"
    AppleCrash,            // Continuity CustomCrash (iOS 17 lockup NearbyAction + NearbyInfo)
    AndroidFastPair,       // Google Fast Pair GFPS (0xFE2C)
    WindowsSwiftPair,      // Microsoft Swift Pair (CID 0x0006)
    SamsungBuds,           // Galaxy Buds (CID 0x0075)
    SamsungWatch,          // Galaxy Watch (CID 0x0075)
    GenericBeacon,         // Baliza genérica configurable
    HidPeripherals,        // Periféricos HID estándar (0x1812: Teclados, Ratones, Gamepads)
    SpoofAirTag,           // Clonación de AirTag / Tracker detectado en Air Recon (GhostESP)
    SpamAll                // Multi-protocolo activo (Kitchen Sink: rota entre todos los protocolos)
};

/**
 * @brief Política de rotación de MAC para evitar deduplicación en el host
 */
enum class BleMacRotationMode : uint8_t {
    Off = 0,
    EveryPacket,
    Every2,
    Every3,
    Every5,
    Every10,
    Every25,
    Every50
};

/**
 * @brief Configuración de una sesión de emisión publicitaria
 */
struct BleAdvParams {
    const uint8_t* data = nullptr;       // Payload crudo (AD structures sin procesar)
    size_t len = 0;                      // Longitud del payload (máx. 31 legacy)
    const uint8_t* scanRspData = nullptr;// Payload opcional de Scan Response (máx. 31)
    size_t scanRspLen = 0;               // Longitud de Scan Response
    uint16_t intervalMs = 100;           // Ventana de rotación de anuncio (20..10240 ms)
    int8_t txPowerDbm = 0;               // Potencia TX; 127 = potencia por defecto
    BleMacRotationMode macRotation = BleMacRotationMode::EveryPacket;
    bool isApple = false;                // Regla GhostESP: Apple usa intervalos HCI 0x30..0x40 y DISC_MODE_GEN
    bool scannable = false;              // Si true, usa BLE_GAP_DISC_MODE_GEN para responder SCAN_REQ
    bool hasCustomMac = false;           // Si true, clona customMac (ej. AirTag Spoof)
    uint8_t customMac[6] = {0};
};

/**
 * @brief Métricas en vivo de la sesión de emisión
 */
struct BleAdvStats {
    uint32_t packetsSent = 0;
    uint32_t elapsedMs = 0;
    float packetsPerSec = 0.0f;
};

/**
 * @brief Callback de notificación de cambio de estado del adaptador
 */
using BleAdapterStateCallback = std::function<void(BleAdapterState state)>;

/**
 * @brief Interfaz abstracta del adaptador BLE (HAL)
 *
 * Implementaciones: NimBLE en ESP32-S3 (bsp/esp32_s3_jc3248/hal/hal_ble_s3.cpp).
 * En targets sin radio (ESP32-P4) se provee un stub con isAvailable()==false.
 */
class IBleAdapter {
public:
    virtual ~IBleAdapter() = default;

    /** ¿Existe radio BT en este target? */
    virtual bool isAvailable() const = 0;

    /** Arranca el stack de host. Idempotente. */
    virtual bool init(const char* deviceName) = 0;

    /** Inicia escaneo GAP. Devuelve false si el stack no está listo. */
    virtual bool startScan() = 0;
    virtual bool stopScan() = 0;

    /** Inicia emisión con el payload indicado. Detiene antes cualquier escaneo. */
    virtual bool startAdvertising(const BleAdvParams& params) = 0;
    virtual bool stopAdvertising() = 0;

    /** Aplica una nueva potencia de transmisión al anuncio en curso. */
    virtual bool setTxPower(int8_t dbm) = 0;

    /** Estado actual del adaptador. */
    virtual BleAdapterState getState() const = 0;

    /** Libera el stack por completo. Dejar el stack vivo es intencional:
     *  NimBLEHost::deinit() es costoso y no aporta nada mientras no se
     *  cambie de app, dado que un unico stack cubre RX y TX. */
    virtual void shutdown() = 0;

    /** Último error detallado del adaptador (si hubo). */
    virtual const char* getLastError() const { return ""; }
};

// ---------------------------------------------------------------------------
// Fachada del dominio (core)
// ---------------------------------------------------------------------------

/** Inyecta el adaptador de hardware. Última escritura gana, sin transferencia
 *  de propiedad (mismo contrato que hid::registerDriver). */
void registerAdapter(IBleAdapter* adapter);
IBleAdapter* getAdapter();

/** Estado actual sin tocar el adaptador (lectura cacheada). */
BleAdapterState getState();
bool isAvailable();

/** Notificación de cambios de estado (core → UI). */
void setStateCallback(BleAdapterStateCallback cb);

/** Notificación de cambios de estado (HAL → core/UI). El HAL lo invoca desde
 *  sus propios callbacks de stack para no obligar a la UI a sondear. */
void notifyStateChanged(BleAdapterState state);

} // namespace ble
} // namespace cbdos