#pragma once

// Orquestador de la suite Wi-Fi Strike (Beacon Spam y Deauth/Disassoc).
//
// Es la maquina de estados que mueve el ciclo de emision de tramas 802.11.
// Sigue el patron de WifiShieldService y BleTransmitter: singleton con
// puros datos de dominio, logica 100% agnostica de plataforma. No conoce
// esp_wifi.h ni esp_random(); solo habla con cbdos::wifi::IWifiStrikeRadio y
// construye los bytes con BeaconEmitter y MgmtEmitter.
//
// Responsabilidades:
//   - Validar la StrikeConfig y arrancar una sesion de emision de balizas.
//   - Validar la MgmtStrikeConfig y arrancar una sesion de tramas de gestion
//     (deauth 0x0C y disassoc 0x0A), en los dos sentidos AP->STA y STA->AP.
//   - Arbitrar la radio: la emision satura el controlador, asi que la
//     sesion toma la radio en exclusiva (RadioClaim) y la devuelve al terminar.
//   - Iterar los canales configurados y amortizar el salto de canal con rafagas
//     (StrikeConfig::burstSize), que es la parte cara de la emision.
//   - Construir la trama de cada iteracion segun el modo y entregarla al HAL.
//   - Publicar StrikeStats (tramas, bytes, saltos de canal, tasa) y un log
//     acotado de sesion para la UI.
//
// La emision corre en una tarea FreeRTOS creada via cbdos::rtos, de modo que la
// UI nunca se congela: el bucle duerme intervalMs entre iteraciones.

#include "cbdos/wifi_radio.hpp"
#include "cbdos/wifi_strike.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace cbdos {
namespace security {

/** Callback de actualizacion de estado (core -> UI). */
using StrikeStatusCallback = std::function<void(const wifi::StrikeStatus& status)>;

/**
 * @brief Servicio central de Wi-Fi Strike: emision de tramas 802.11
 */
class WifiStrikeService {
public:
    static WifiStrikeService& getInstance();

    /**
     * @brief Arranca una sesion de emision de balizas
     *
     * Valida la configuracion, toma la radio en exclusiva, inicializa el HAL y
     * crea la tarea de emision. Devuelve false con motivo en getLastError():
     * configuracion invalida, target sin radio, radio ocupada por otro
     * consumidor, o fallo del stack Wi-Fi.
     */
    bool startBeacon(const wifi::StrikeConfig& cfg);

    /**
     * @brief Arranca una sesion de tramas de gestion (deauth / disassoc)
     *
     * Misma maquinaria que startBeacon() con la diferencia de que las tramas
     * las construye MgmtEmitter y de que el plan de objetivos son BSSID reales
     * del Air Recon o el broadcast, no SSID inventados.
     */
    bool startMgmt(const wifi::MgmtStrikeConfig& cfg);

    /** Detiene la sesion y devuelve la radio. Idempotente. */
    void stop();

    /** ¿Hay una sesion emitiendo ahora mismo? */
    bool isActive() const;

    /** Snapshot de contadores, con la tasa ya calculada. */
    wifi::StrikeStats getStats() const;

    /** Snapshot completo que consume la UI. */
    wifi::StrikeStatus getStatus() const;

    /** Configuracion vigente de beacons (la ultima aplicada o la de startBeacon). */
    wifi::StrikeConfig getConfig() const;

    /** Guarda la configuracion de beacons para la proxima sesion sin arrancarla. */
    void setConfig(const wifi::StrikeConfig& cfg);

    /** Configuracion vigente de tramas de gestion. */
    wifi::MgmtStrikeConfig getMgmtConfig() const;

    /** Guarda la configuracion de gestion para la proxima sesion sin arrancarla. */
    void setMgmtConfig(const wifi::MgmtStrikeConfig& cfg);

    std::string getLastError() const;

    void setStatusCallback(StrikeStatusCallback cb);

    // --- Arbitraje de radio ------------------------------------------------
    //
    // El beacon spam monopoliza el controlador: mientras inyecta tramas
    // crudas el escaneo pierde todos sus resultados y el enlace STA se cae. La
    // sesion reserva la radio con un token opaco y solo lo libera si sigue
    // siendo suyo, para no devolver la reserva de otro consumidor.

    /** Estado actual de la reserva de radio. */
    static wifi::RadioClaim getRadioClaim();

    /**
     * @brief Intenta reservar la radio para `owner`
     *
     * Devuelve false si ya la tiene otro propietario. Reentrante: volver a
     * pedirla con el mismo owner devuelve true sin regenerar el token.
     */
    static bool claimRadio(wifi::RadioClaimOwner owner);

    /** Devuelve la radio al estado libre si `token` sigue siendo valido. */
    static void releaseRadio(uint32_t token);

private:
    WifiStrikeService() = default;
    ~WifiStrikeService() = default;
    WifiStrikeService(const WifiStrikeService&) = delete;
    WifiStrikeService& operator=(const WifiStrikeService&) = delete;

    /** Bucle de emision: recorre canales, construye tramas y las inyecta. */
    void emitTask(void* param);

    /**
     * @brief Arranca la tarea de emision para una familia de sesion
     *
     * startBeacon() y startMgmt() comparten toda la parte comum: arbitrar la
     * radio, inicializar el HAL, pausar el Air Recon, sembrar el PRNG con
     * entropia real y crear la tarea. Lo que cambia son la validacion previa y
     * que configuracion se guarda, asi que esa parte se parametriza.
     */
    bool startSession(wifi::StrikeKind kind);

    /** Publica el estado a la UI (tarea o hilo de control). */
    void publishStatusLocked();

    /** Guarda el ultimo error y actualiza el estado a Faulted. */
    void fault(const char* reason);

    /**
     * @brief Anade una linea al log acotado de la sesion
     *
     * El texto va ya formateado: el servicio no decide el idioma, solo conserva
     * el orden y recorta el numero de lineas. Solo deduplica si el llamante lo
     * pide explicitamente con `repeat`, para no llenar el log con el mismo error
     * de TX miles de veces.
     */
    void pushLog(const std::string& line, bool dedup = false);

    mutable std::mutex m_mutex;
    wifi::StrikeConfig m_config;
    wifi::MgmtStrikeConfig m_mgmtConfig;
    std::string m_lastError;
    StrikeStatusCallback m_statusCb;
    std::vector<std::string> m_log;

    std::atomic<bool> m_running{false};
    std::atomic<wifi::StrikeState> m_state{wifi::StrikeState::Idle};
    std::atomic<wifi::StrikeKind> m_kind{wifi::StrikeKind::Beacon};
    std::atomic<void*> m_taskHandle{nullptr};

    std::atomic<uint32_t> m_framesSent{0};
    std::atomic<uint32_t> m_framesFailed{0};
    std::atomic<uint32_t> m_bytesSent{0};
    std::atomic<uint32_t> m_channelHops{0};
    std::atomic<uint32_t> m_elapsedMs{0};
    std::atomic<uint32_t> m_lastChannel{0};
    std::atomic<uint32_t> m_lastBurst{0};
    std::atomic<uint32_t> m_framesTargeted{0};
    std::atomic<uint32_t> m_targetsTracked{0};

    /** Token de la reserva de radio en poder de esta sesion. */
    std::atomic<uint32_t> m_claimToken{0};
};

} // namespace security
} // namespace cbdos