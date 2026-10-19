#pragma once

// Contratos de datos de la suite Wi-Fi Strike.
//
// Este header es puramente declarativo: enums, estructuras y funciones inline
// sin ninguna dependencia de plataforma. Ni esp_wifi.h ni FreeRTOS aparecen
// aquí; el core solo describe "que" se va a emitir y "como se mide", mientras
// el acceso a la radio se inyecta via cbdos::wifi::IWifiStrikeRadio
// (ver wifi_radio.hpp) y la construccion de bytes vive en BeaconEmitter.
//
// La semántica de cada modo y cada campo está tomada de la suite GhostESP
// (references/ghostesp/main/attacks/wifi/beacon_spam.c).

#include "cbdos/wifi_types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cbdos {
namespace wifi {

// ---------------------------------------------------------------------------
// Límites 802.11 y de dominio
// ---------------------------------------------------------------------------

/** Longitud máxima del campo SSID en una trama 802.11 (SSID element, octeto 1
 *  de longitud). Cualquier SSID mas largo se trunca en el frame builder. */
static constexpr size_t STRIKE_SSID_MAX = 32;

/** Longitud de una direccion MAC en bytes (campos Addr1/Addr2/Addr3). */
static constexpr size_t WIFI_MAC_LEN = 6;

/** Maximo de APs clonables por sesion (replica BEACON_LIST_MAX / poblado de
 *  Air Recon). */
static constexpr size_t STRIKE_AP_LIST_MAX = 64;

/** Maximo de SSIDs de la lista personalizada (BEACON_LIST_MAX en GhostESP). */
static constexpr size_t STRIKE_CUSTOM_LIST_MAX = 16;

/** Longitud por defecto del SSID generado en modo aleatorio. */
static constexpr uint8_t STRIKE_RANDOM_SSID_LEN = 8;

/** Rafaga de tramas por salto de canal en modo aleatorio: amortiza el coste
 *  del cambio de canal, ya que en ese modo cada trama lleva un SSID distinto. */
static constexpr uint8_t STRIKE_DEFAULT_BURST = 4;

/** Intervalo por defecto entre iteraciones del bucle de emision, en ms. */
static constexpr uint16_t STRIKE_DEFAULT_INTERVAL_MS = 100;

// ---------------------------------------------------------------------------
// Modos de emision de beacons
// ---------------------------------------------------------------------------

/**
 * @brief Familias de beacons que el emisor sabe construir
 */
enum class BeaconMode : uint8_t {
    SingleSsid = 0,       // Un SSID fijo, una baliza por ciclo
    Random,               // SSID alfanumerico aleatorio + BSSID rotativa
    Rickroll,             // Coro rotativo de Rick Astley, un versiculo por ciclo
    CloneAccessPoints,    // Clona cada AP descubierto en Air Recon
    CustomList            // Rota por una lista de SSIDs indicada por el usuario
};

/**
 * @brief Etiqueta legible del modo, en el mismo idioma que el resto de la UI
 */
inline const char* getBeaconModeName(BeaconMode mode) {
    switch (mode) {
        case BeaconMode::SingleSsid:      return "SSID UNICO";
        case BeaconMode::Random:          return "SSID ALEATORIO";
        case BeaconMode::Rickroll:        return "RICKROLL";
        case BeaconMode::CloneAccessPoints: return "CLONAR APs";
        case BeaconMode::CustomList:      return "LISTA PERSONALIZADA";
        default:                          return "?";
    }
}

/**
 * @brief Estado de la maquina de emision
 */
enum class StrikeState : uint8_t {
    Idle = 0,     // Sin sesion; la radio esta libre para otros consumidores
    Running,      // Emision activa
    Stopping,     // Parada solicitada, la tarea esta drenando la rafaga
    Faulted        // Parada por error de radio; ver StrikeStats::lastError
};

/**
 * @brief Etiqueta legible del estado
 */
inline const char* getStrikeStateName(StrikeState state) {
    switch (state) {
        case StrikeState::Idle:     return "INACTIVO";
        case StrikeState::Running:  return "EMITIENDO";
        case StrikeState::Stopping: return "PARANDO";
        case StrikeState::Faulted:  return "ERROR";
        default:                    return "?";
    }
}

// ---------------------------------------------------------------------------
// Configuracion de sesion
// ---------------------------------------------------------------------------

/**
 * @brief Configuracion completa de una sesion de Wi-Fi Strike
 *
 * Solo se materializa lo que el modo seleccionado necesita; el resto de campos
 * se ignora. Es una copia en memoria: la UI puede editarla sin tocar la sesion
 * en curso hasta que se aplique con start().
 */
struct StrikeConfig {
    BeaconMode mode = BeaconMode::SingleSsid;

    /** SSID a emitir en SingleSsid (y SSID de la AP propia en CustomList si la
     *  lista se deja vacia). */
    std::string ssid;

    /** Lista personalizada para CustomList, en orden de rotacion. */
    std::vector<std::string> ssidList;

    /** APs a clonar en CloneAccessPoints, normalmente alimentada desde el
     *  Air Recon de WifiShieldService. */
    std::vector<radio::WifiApRecord> apList;

    /** Longitud del SSID generado en modo Random. */
    uint8_t randomSsidLength = STRIKE_RANDOM_SSID_LEN;

    /** Canales de salto. Vacio = usa la lista de TX del plan de canal activo
     *  que resuelva el HAL. */
    std::vector<uint8_t> channels;

    /** Espera entre iteraciones del bucle de emision, en ms. */
    uint16_t intervalMs = STRIKE_DEFAULT_INTERVAL_MS;

    /** Tramas por salto de canal. Solo aplica a Random: en los modos de SSID
     *  fijo cada iteracion emite una sola trama. */
    uint8_t burstSize = STRIKE_DEFAULT_BURST;

    /** Rotar la BSSID por trama (MAC localmente administrada) para evitar que
     *  el cliente descarte balizas repetidas. */
    bool rotateMac = true;

    /** Anadir el bloque HE Capabilities (Wi-Fi Alliance OUI 00:50:6f). */
    bool includeHeCapabilities = true;

    /** Semilla del PRNG del frame builder. 0 = sin fijar (el servicio lo
     *  siembra con entropia real de la radio al arrancar). */
    uint32_t seed = 0;

    /** Vacia todas las listas manteniendo el resto de ajustes. */
    void clear() {
        ssid.clear();
        ssidList.clear();
        apList.clear();
        channels.clear();
    }
};

/**
 * @brief Comprueba que la sesion tiene material suficiente para emitir
 *
 * No valida la radio ni el canal: solo las invariantes que el frame builder no
 * puede resolver por si mismo (SSID no vacio, listas con contenido, longitudes
 * dentro del limite 802.11).
 */
inline bool validate(const StrikeConfig& cfg) {
    switch (cfg.mode) {
        case BeaconMode::SingleSsid:
            return !cfg.ssid.empty() && cfg.ssid.size() <= STRIKE_SSID_MAX;
        case BeaconMode::Random:
            return cfg.randomSsidLength > 0 && cfg.randomSsidLength <= STRIKE_SSID_MAX;
        case BeaconMode::Rickroll:
            return true;
        case BeaconMode::CloneAccessPoints:
            return !cfg.apList.empty();
        case BeaconMode::CustomList:
            if (!cfg.ssidList.empty()) return true;
            return !cfg.ssid.empty() && cfg.ssid.size() <= STRIKE_SSID_MAX;
    }
    return false;
}

/**
 * @brief Motivo por el que validate() devuelve false, para la UI
 */
inline const char* describeInvalid(const StrikeConfig& cfg) {
    switch (cfg.mode) {
        case BeaconMode::SingleSsid:
            if (cfg.ssid.empty()) return "SSID vacio";
            if (cfg.ssid.size() > STRIKE_SSID_MAX) return "SSID > 32 bytes";
            return nullptr;
        case BeaconMode::Random:
            if (cfg.randomSsidLength == 0) return "Longitud de SSID aleatorio 0";
            if (cfg.randomSsidLength > STRIKE_SSID_MAX) return "SSID aleatorio > 32 bytes";
            return nullptr;
        case BeaconMode::Rickroll:
            return nullptr;
        case BeaconMode::CloneAccessPoints:
            if (cfg.apList.empty()) return "Sin APs en Air Recon";
            return nullptr;
        case BeaconMode::CustomList:
            if (cfg.ssidList.empty() && cfg.ssid.empty()) return "Lista de SSIDs vacia";
            return nullptr;
    }
    return "Modo desconocido";
}

// ---------------------------------------------------------------------------
// Metricas
// ---------------------------------------------------------------------------

/**
 * @brief Contadores en vivo de la sesion
 *
 * Snapshot inmutable que la UI lee desde la vista; el servicio publica los
 * valores bajo lock. `framesSent` cuenta tramas que el HAL acepto para TX (no
 * confirmaciones de radiodifusion: 802.11 no las ofrece).
 */
struct StrikeStats {
    uint32_t framesSent = 0;     // Tramas entregadas al HAL sin error
    uint32_t framesFailed = 0;    // Tramas rechazadas (sin memoria de TX, etc.)
    uint32_t bytesSent = 0;       // Bytes utiles-oxigeno inyectados en el aire
    uint32_t channelHops = 0;     // Cambios de canal solicitados
    uint32_t elapsedMs = 0;       // Duracion de la sesion activa
    float framesPerSec = 0.0f;    // framesSent / elapsedMs
    uint8_t lastChannel = 0;      // Ultimo canal usado
    uint8_t lastBurstSize = 0;    // Ultima rafaga completa
    std::string lastError;        // Vacio si la sesion va bien
};

/**
 * @brief Estado inmutable de la sesion, tal y como lo consume la UI
 */
struct StrikeStatus {
    bool active = false;
    StrikeState state = StrikeState::Idle;
    BeaconMode mode = BeaconMode::SingleSsid;
    StrikeStats stats;
    std::string lastError;
};

// ---------------------------------------------------------------------------
// Arbitraje de radio
// ---------------------------------------------------------------------------

/**
 * @brief Consumidores que pueden exigir la radio Wi-Fi en exclusiva
 */
enum class RadioClaimOwner : uint8_t {
    None = 0,     // Radio libre
    WifiStrike,   // Emision de tramas 802.11
    WifiShield,   // Recon pasivo e IDS
    LanRecon,
    Network,      // STA conectada con IP
    Portal,       // Evil Portal (SoftAP + DNS)
    Pmkid         // Sniffer de handshake
};

/**
 * @brief Etiqueta legible del propietario de la radio
 */
inline const char* getRadioClaimOwnerName(RadioClaimOwner owner) {
    switch (owner) {
        case RadioClaimOwner::None:       return "LIBRE";
        case RadioClaimOwner::WifiStrike: return "WIFI STRIKE";
        case RadioClaimOwner::WifiShield: return "WIFI SHIELD";
        case RadioClaimOwner::LanRecon:   return "LAN RECON";
        case RadioClaimOwner::Network:    return "RED (STA)";
        case RadioClaimOwner::Portal:     return "EVIL PORTAL";
        case RadioClaimOwner::Pmkid:      return "PMKID SNIFFER";
        default:                          return "?";
    }
}

/**
 * @brief Titularidad de la radio Wi-Fi durante una sesion de emision
 *
 * El beacon spam satura la radio: mientras inyecta tramas crudas el escaneo
 * pierde practicamente todos sus resultados y el enlace STA se cae. Por eso la
 * sesion toma la radio en exclusiva y la devuelve al terminar.
 *
 * `token` es opaco y lo emite el servicio al adquirir la radio; sirve para que
 * el liberador compruebe que la reserva sigue siendo suya y no libere la de
 * otro consumidor.
 */
struct RadioClaim {
    RadioClaimOwner owner = RadioClaimOwner::None;
    uint32_t token = 0;        // 0 = sin reserva
    uint32_t acquiredMs = 0;   // Marca temporal de adquisicion (ms)

    bool active() const { return owner != RadioClaimOwner::None; }

    /** Devuelve la reserva al estado libre. Idempotente. */
    void release() {
        owner = RadioClaimOwner::None;
        token = 0;
        acquiredMs = 0;
    }
};

} // namespace wifi
} // namespace cbdos
