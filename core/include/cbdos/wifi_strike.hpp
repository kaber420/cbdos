#pragma once

// Contratos de datos de la suite Wi-Fi Strike.
//
// Este header es puramente declarativo: enums, estructuras y funciones inline
// sin ninguna dependencia de plataforma. Ni esp_wifi.h ni FreeRTOS aparecen
// aquí; el core solo describe "que" se va a emitir y "como se mide", mientras
// este header, el core solo describe "que" se va a emitir y "como se mide", mientras
// el acceso a la radio se inyecta via cbdos::wifi::IWifiStrikeRadio
// (ver wifi_radio.hpp) y la construccion de bytes vive en BeaconEmitter (balizas)
// y MgmtEmitter (desautenticacion y desasociacion).
//
// La semantica de cada modo y cada campo esta tomada de la suite GhostESP
// (references/ghostesp/main/attacks/wifi/beacon_spam.c y deauth.c).

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

/**
 * @brief Intervalo por defecto del bucle de tramas de gestion, en ms
 *
 * Una trama de gestion son 26 bytes frente a los ~104 de un beacon, asi que el
 * ciclo de deauth puede ser mucho mas agresivo. Aun asi se conserva un minimo
 * de 10 ms: por debajo esp_wifi_80211_tx() empieza a devolver NoMemory en vez de
 * emitir mas, y la sesion solo acumularia fallos.
 */
static constexpr uint16_t STRIKE_DEFAULT_MGMT_INTERVAL_MS = 50;

/** Rafaga de tramas de gestion por salto de canal. */
static constexpr uint8_t STRIKE_DEFAULT_MGMT_BURST = 8;

/**
 * @brief Lineas de log de sesion que la UI puede retener
 *
 * El log no es un historico infinito: son las ultimas decisiones y errores de la
 * sesion en curso, suficiente para que el operador entienda que esta pasando sin
 * que el core acumule memoria ni copie cadenas en cada trama.
 */
static constexpr size_t STRIKE_LOG_LINES = 8;

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

// ---------------------------------------------------------------------------
// Modos de emision de tramas de gestion (deauth / disassoc)
// ---------------------------------------------------------------------------

/**
 * @brief Subtipo de trama de gestion que se inyecta
 *
 * Deauthentication (0x0C) y Disassociation (0x0A) se construyen igual salvo
 * en los dos bits de subtipo del Frame Control, pero no son lo mismo para un
 * cliente: la deauth invalida la asociacion y obliga a reautenticar, mientras
 * que la disasociacion solo cierra el enlace y la reasociacion es inmediata.
 */
enum class MgmtSubtype : uint8_t {
    Deauth = 0,      // Desautenticacion (0x0C)
    Disassoc,        // Desasociacion (0x0A)
    Mixed            // Alterna una y otra trama a trama
};

/**
 * @brief Etiqueta legible del subtipo de gestion
 */
inline const char* getMgmtSubtypeName(MgmtSubtype subtype) {
    switch (subtype) {
        case MgmtSubtype::Deauth:      return "DESAUTENTICACION 0x0C";
        case MgmtSubtype::Disassoc:    return "DESASOCIACION 0x0A";
        case MgmtSubtype::Mixed:       return "ALTERNAR 0x0C/0x0A";
        default:                       return "?";
    }
}

/**
 * @brief Sentido en el que se falsifica la identidad de la trama
 *
 *  - ApToSta: la trama aparenta salir de la AP (Addr2 = BSSID) hacia sus
 *    clientes. Es el caso clasico de deauth flood.
 *  - StaToAp: la trama aparenta salir de una STA suplantada (Addr2 = MAC local
 *    administrada) y acusa a la AP, que es lo que comprueba si el cliente valida
 *    el origen antes de desconectarse.
 *  - Both: alterna ambos sentidos trama a trama.
 */
enum class MgmtDirection : uint8_t {
    ApToSta = 0,
    StaToAp,
    Both
};

/**
 * @brief Etiqueta legible del sentido de la inyeccion
 */
inline const char* getMgmtDirectionName(MgmtDirection direction) {
    switch (direction) {
        case MgmtDirection::ApToSta: return "AP -> STA";
        case MgmtDirection::StaToAp: return "STA -> AP";
        case MgmtDirection::Both:    return "AMBOS SENTIDOS";
        default:                     return "?";
    }
}

/**
 * @brief Seleccion de objetivos de la inyeccion
 *
 * Broadcast inunda todos los BSSID del canal con identidades rotativas;
 * AccessPoints va contra los BSSID concretos descubiertos en Air Recon, uno por
 * uno y en el canal real de cada uno. Nunca se inventan clientes: sin lista de
 * APs el modo AccessPoints no es valido, y la razon aparece en describeInvalid.
 */
enum class MgmtTarget : uint8_t {
    Broadcast = 0,
    AccessPoints
};

/**
 * @brief Etiqueta legible del modo de objetivo
 */
inline const char* getMgmtTargetName(MgmtTarget target) {
    switch (target) {
        case MgmtTarget::Broadcast:    return "BROADCAST";
        case MgmtTarget::AccessPoints: return "APs ESCANEADOS";
        default:                      return "?";
    }
}

/**
 * @brief Codigos de razon estandar de desautenticacion y desasociacion
 *
 * Subconjunto de la Tabla 10-18 del 802.11 con los valores que un cliente
 * real acepta y que un auditor puede distinguir en un captura. El 0x0007 es el
 * que usan GhostESP y Bruce por defecto.
 */
enum class MgmtReason : uint16_t {
    Unspecified = 1,             // Razon no especificada
    PreviousAuthNotValid = 2,    // Autenticacion previa ya no valida
    LeavingOrDisabling = 3,      // Abandona la BSS o la desactiva
    Inactivity = 4,              // Inactividad
    ApOverload = 5,              // AP sin recursos
    SasFailure = 6,              // Fallo de autenticacion con la SA
    NotAuthed = 7,               // Trama clase 3 de una STA no asociada
    BadAuthState = 8,            // Trama recibida en estado de auth invalido
    AuthTimeout = 9,             // Timeout del temporizador de autenticacion
};

/**
 * @brief Etiqueta corta del codigo de razon, con su valor en hexadecimal
 */
inline const char* getMgmtReasonName(MgmtReason reason) {
    switch (reason) {
        case MgmtReason::Unspecified:          return "0x01 sin especificar";
        case MgmtReason::PreviousAuthNotValid: return "0x02 auth previa invalida";
        case MgmtReason::LeavingOrDisabling:   return "0x03 abandona la BSS";
        case MgmtReason::Inactivity:           return "0x04 inactividad";
        case MgmtReason::ApOverload:           return "0x05 AP sin recursos";
        case MgmtReason::SasFailure:           return "0x06 fallo de secuencia";
        case MgmtReason::NotAuthed:            return "0x07 STA no asociada";
        case MgmtReason::BadAuthState:         return "0x08 estado de auth invalido";
        case MgmtReason::AuthTimeout:          return "0x09 timeout de auth";
        default:                               return "?";
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

/**
 * @brief Familia de la sesion en curso
 *
 * El servicio expone una unica maquina de estados y una unica tarea de emision:
 * el tipo de trama que sale al aire lo decide la familia con la que arranco la
 * sesion. La UI lo necesita para no leer un contador de balizas como si fueran
 * tramas de desautenticacion.
 */
enum class StrikeKind : uint8_t {
    Beacon = 0,     // Balizas 802.11 (subtipo 0x08)
    Mgmt             // Gestion: deauth (0x0C) y disassoc (0x0A)
};

/**
 * @brief Etiqueta legible de la familia de sesion
 */
inline const char* getStrikeKindName(StrikeKind kind) {
    switch (kind) {
        case StrikeKind::Beacon: return "BEACON SPAM";
        case StrikeKind::Mgmt:   return "DEAUTH/DISASSOC";
        default:                 return "?";
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

/**
 * @brief Configuracion de una sesion de tramas de gestion
 *
 * Contraparte de StrikeConfig para deauth y disassoc. Igual que alli, solo se
 * materializa lo que el modo seleccionado necesita y es una copia en memoria:
 * la UI la edita sin tocar la sesion en curso.
 */
struct MgmtStrikeConfig {
    MgmtSubtype subtype = MgmtSubtype::Deauth;

    /** Sentido en el que se falsifica la identidad de la trama. */
    MgmtDirection direction = MgmtDirection::Both;

    /** Codigo de razon anunciado en el cuerpo de la trama. */
    MgmtReason reason = MgmtReason::NotAuthed;   // 0x0007

    /** Como se eligen los BSSID afectados. */
    MgmtTarget target = MgmtTarget::Broadcast;

    /** BSSID a atacar en modo AccessPoints, alimentada desde el Air Recon de
     *  WifiShieldService. En modo Broadcast se usa solo como lista de referencia
     *  para ordenar por potencia, no como objetivo. */
    std::vector<radio::WifiApRecord> apList;

    /** Canales de salto. Vacio = 1..13, o el canal de la STA si esta asociada. */
    std::vector<uint8_t> channels;

    /** Espera entre iteraciones del bucle de emision, en ms. */
    uint16_t intervalMs = STRIKE_DEFAULT_MGMT_INTERVAL_MS;

    /** Tramas por iteracion y salto de canal. */
    uint8_t burstSize = STRIKE_DEFAULT_MGMT_BURST;

    /** Marcar la trama como cifrada (bit Protected Frame, 802.11w) para
     *  comprobar si la AP de destino aplica Management Frame Protection. */
    bool protectedFrame = false;

    /** Semilla del PRNG de secuencias y MACs suplantadas. 0 = el servicio la
     *  siembra con entropia real de la radio al arrancar. */
    uint32_t seed = 0;

    /** Vacia las listas manteniendo el resto de ajustes. */
    void clear() {
        apList.clear();
        channels.clear();
    }
};

/**
 * @brief Numero de BSSID a los que la sesion va a lanzar tramas
 *
 * En modo Broadcast no hay objetivos concretos: el unico valor honesto es 1
 * (todo el espectro), porque inventar un numero de "victimas" seria falso.
 */
inline size_t mgmtTargetCount(const MgmtStrikeConfig& cfg) {
    return (cfg.target == MgmtTarget::AccessPoints) ? cfg.apList.size() : 1u;
}

/**
 * @brief Comprueba que la sesion de gestion tiene material suficiente
 *
 * Mismas reglas que validate() pero para el otro frame builder: no valida la
 * radio ni el canal, solo las invariantes que MgmtEmitter no puede resolver.
 */
inline bool validateMgmt(const MgmtStrikeConfig& cfg) {
    if (cfg.target == MgmtTarget::AccessPoints && cfg.apList.empty()) return false;
    return true;
}

/**
 * @brief Motivo por el que validateMgmt() devuelve false, para la UI
 */
inline const char* describeInvalidMgmt(const MgmtStrikeConfig& cfg) {
    if (cfg.target == MgmtTarget::AccessPoints && cfg.apList.empty()) {
        return "Sin APs en Air Recon";
    }
    return nullptr;
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
    /** Tramas dirigidas a un BSSID concreto. Siempre 0 en beacon spam, cuyo
     *  destino es el broadcast por definicion. */
    uint32_t framesTargeted = 0;
    /** Numero de BSSID objetivo de la sesion (ver mgmtTargetCount). */
    uint32_t targetsTracked = 0;
    std::string lastError;        // Vacio si la sesion va bien
};

/**
 * @brief Estado inmutable de la sesion, tal y como lo consume la UI
 */
struct StrikeStatus {
    bool active = false;
    StrikeState state = StrikeState::Idle;
    StrikeKind kind = StrikeKind::Beacon;
    /** Modos de beacon activos. Meaningful solo si kind == Beacon. */
    BeaconMode mode = BeaconMode::SingleSsid;
    /** Modos de gestion activos. Meaningful solo si kind == Mgmt. */
    MgmtSubtype mgmtSubtype = MgmtSubtype::Deauth;
    MgmtDirection mgmtDirection = MgmtDirection::Both;
    StrikeStats stats;
    std::string lastError;
    /** Ultimas decisiones y errores de la sesion, como mucho STRIKE_LOG_LINES. */
    std::vector<std::string> log;
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
