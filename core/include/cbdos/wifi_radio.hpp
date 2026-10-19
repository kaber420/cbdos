#pragma once

// Contrato HAL de la suite Wi-Fi Strike.
//
// Define la frontera entre el core agnóstico de CBDos y el controlador Wi-Fi
// real. El core nunca incluye esp_wifi.h, esp_random.h ni freertos: unica-
// mente habla con IWifiStrikeRadio.
//
// El contrato cubre lo mínimo imprescindible para inyectar tramas 802.11
// crudas (Beacon Spam, Deauth/Disassoc, Karma) y vigilar el estado del enlace:
//   - Injection de tramas de gestión crudas (esp_wifi_80211_tx en el BSP S3).
//   - Salto de canal (esp_wifi_set_channel con 20 MHz, sin HT40).
//   - Entropia real y MAC de la interfaz, para BSSIDs rotativas y SSIDs
//     aleatorios sin inventar datos.
//   - La SoftAP oculta que exige el controlador para poder emitir frames
//     arbitrarios.
//
// Mismo contrato que ble::IBleAdapter y radio::IRadioBackend: la inyeccion es
// un puntero sin transferencia de propiedad, la ultima escritura gana, y en
// targets sin radio Wi-Fi se inyecta un stub con isAvailable() == false.

#include "cbdos/wifi_strike.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace cbdos {
namespace wifi {

/**
 * @brief Estado de la radio desde el punto de vista de Wi-Fi Strike
 */
enum class StrikeRadioState : uint8_t {
    Unavailable = 0,  // El target no tiene radio Wi-Fi utilizable
    Idle,             // Radio lista, sin sesion de emision
    Emitting          // Tramas crudas en curso
};

/**
 * @brief Parametros de una transmision de trama cruda
 */
struct TxFrameParams {
    /** Interfaz de salida. 0 = STA (modo en el que el ESP32-S3 acepta tramas
     *  crudas con destino broadcast), 1 = AP. */
    uint8_t iface = 0;

    /** system=false para trama de gestion con destino broadcast. */
    bool enSys = false;
};

/**
 * @brief Resultado de una operacion de radio
 *
 * Sustituye a esp_err_t para que el core no dependa del enum de IDF.
 */
enum class RadioResult : uint8_t {
    Ok = 0,
    InvalidArg,
    Unsupported,   // El target no soporta la operacion (stub de P4)
    Busy,          // Radio ocupada por otro consumidor
    NoMemory,      // Sin buffers de TX: reintentar tras unos ms
    NotReady,      // Stack no inicializado
    HardwareError  // Fallo del controlador
};

/**
 * @brief Etiqueta legible del resultado
 */
inline const char* getRadioResultName(RadioResult r) {
    switch (r) {
        case RadioResult::Ok:            return "OK";
        case RadioResult::InvalidArg:    return "ARG INVALIDO";
        case RadioResult::Unsupported:    return "NO SOPORTADO";
        case RadioResult::Busy:          return "RADIO OCUPADA";
        case RadioResult::NoMemory:      return "SIN MEMORIA TX";
        case RadioResult::NotReady:      return "RADIO NO LISTA";
        case RadioResult::HardwareError: return "ERROR HARDWARE";
        default:                         return "?";
    }
}

/**
 * @brief Notificacion de cambio de estado de la radio (HAL -> core/UI)
 *
 * La invoca el HAL desde sus propios eventos para que la UI no tenga que
 * sondear el hardware (prohibido el polling ciclico).
 */
using StrikeRadioStateCallback = std::function<void(StrikeRadioState state)>;

/**
 * @brief Interfaz abstracta de la radio para Wi-Fi Strike (HAL)
 *
 * Implementaciones previstas:
 *   - ESP32-S3 (bsp/esp32_s3_jc3248/hal/hal_wifi_strike_s3.cpp) sobre
 *     esp_wifi_80211_tx() con la interfaz STA en modo promiscuo.
 *   - ESP32-P4 (host Wi-Fi C6) mediante stub: isAvailable() == false y todas
 *     las operaciones devuelven Unsupported.
 */
class IWifiStrikeRadio {
public:
    virtual ~IWifiStrikeRadio() = default;

    /** ¿Existe radio Wi-Fi utilizable para inyectar en este target? */
    virtual bool isAvailable() const = 0;

    /**
     * @brief Prepara el controlador para emitir tramas crudas
     *
     * En el ESP32-S3 implica arrancar el stack en modo STA y activar el modo
     * promiscuo: sin el, el controlador descarta el paquete antes de emitirlo.
     * El almacenamiento de la cabecera de red pasa a RAM para que las tramas
     * inyectadas no se escriban en la flash. Idempotente.
     */
    virtual RadioResult init() = 0;

    /**
     * @brief Inyecta una trama 802.11 ya construida
     *
     * No es bloqueante en el sentido de esperar confirmacion: 802.11 no la
     * ofrece. Devuelve NoMemory si el controlador no tiene buffers libres, en
     * cuyo caso el emisor reintenta tras una pausa corta.
     */
    virtual RadioResult txFrame(const uint8_t* data, size_t len,
                                const TxFrameParams& params) = 0;

    /**
     * @brief Fija el canal de salida (20 MHz, sin HT40)
     *
     * El salto de canal es la parte cara del beacon spam: amortiza el cambio
     * emitiendo varios beacons por canal (ver StrikeConfig::burstSize).
     */
    virtual RadioResult setChannel(uint8_t channel) = 0;

    /** Canal primario actual. 0 si se desconoce. */
    virtual uint8_t getChannel() const = 0;

    /**
     * @brief MAC de la interfaz de salida (6 bytes)
     *
     * Necesario para los modos que clonan la identidad de una AP real en lugar
     * de rotar una MAC localmente administrada.
     */
    virtual RadioResult getMac(uint8_t* out) const = 0;

    /**
     * @brief Rellena `len` bytes con entropia del hardware
     *
     * Es la unica fuente de azar del core: ni rand() ni un PRNG sembrado por
     * tiempo de actividad, para no repetir SSIDs ni BSSIDs entre arranques.
     */
    virtual RadioResult getRandom(uint8_t* out, size_t len) = 0;

    /** ¿Hay una STA conectada con IP? En ese caso la sesion debe emitir
     *  unicamente en el canal asociado y no hacer hopping. */
    virtual bool isStaConnected() const = 0;

    /** Estado de la radio. */
    virtual StrikeRadioState getState() const = 0;

    /** Devuelve la radio al estado normal (fin de SoftAP oculta). */
    virtual void shutdown() = 0;

    /** Detalle del último error, para diagnostico. */
    virtual const char* getLastError() const { return ""; }
};

// ---------------------------------------------------------------------------
// Fachada del dominio (core)
// ---------------------------------------------------------------------------

/**
 * @brief Inyecta el HAL de Wi-Fi Strike
 *
 * Sin transferencia de propiedad: el HAL es un singleton estatico con vida
 * mayor que el core. Ultima escritura gana.
 */
void registerStrikeRadio(IWifiStrikeRadio* radio);

/**
 * @brief HAL registrado, o nullptr si el target no tiene radio
 *
 * Todo consumidor debe comprobar nullptr antes de usarlo; es la forma natural
 * de detectar un target sin Wi-Fi (ESP32-P4 con host C6) sin tocar la UI.
 */
IWifiStrikeRadio* getStrikeRadio();

/** Atajo: hay radio y esta disponible. */
bool isStrikeRadioAvailable();

/** Estado cacheado; Unavailable si no hay HAL registrado. */
StrikeRadioState getStrikeRadioState();

/** Registro de callback de cambio de estado (core -> UI). */
void setStrikeRadioStateCallback(StrikeRadioStateCallback cb);

/**
 * @brief Notifica un cambio de estado de la radio
 *
 * Lo invoca el HAL; no es para uso del core.
 */
void notifyStrikeRadioState(StrikeRadioState state);

} // namespace wifi
} // namespace cbdos
