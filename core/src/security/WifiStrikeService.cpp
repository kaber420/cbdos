#include "WifiStrikeService.hpp"

#include "WifiShieldService.hpp"
#include "wifi/BeaconEmitter.hpp"

#include "cbdos/rtos.hpp"
#include "cbdos/system.hpp"

#include <cstring>
#include <vector>

namespace cbdos {
namespace security {

using wifi::BeaconMode;
using wifi::RadioClaim;
using wifi::RadioClaimOwner;
using wifi::RadioResult;
using wifi::StrikeConfig;
using wifi::StrikeState;
using wifi::StrikeStats;
using wifi::StrikeStatus;
using wifi::TxFrameParams;

static const char* TAG_STRIKE = "WIFI_STRIKE";

namespace {

// Pila de la tarea: el ciclo construye tramas (BeaconFrame son 104 bytes) y
// copia la StrikeConfig, que incluye vectores de SSIDs y de APs.
constexpr uint32_t kEmitTaskStack = 8192;
constexpr uint32_t kEmitTaskPriority = 4;
constexpr int kEmitTaskCore = 0;

// Cadencia del bucle. Por debajo de 10 ms la radio se satura sin ganar nada
// util; por encima de 10 s la UI percibe la sesion como congelada.
constexpr uint16_t kMinIntervalMs = 10;
constexpr uint16_t kMaxIntervalMs = 10000;

// Fallas consecutivas de radio que convierten la sesion en Faulted. Un NoMemory
// transitorio (sin buffers de TX) no cuenta: se reintenta tras una pausa.
constexpr uint32_t kMaxConsecutiveFailures = 8;

// Espera maxima de stop() a que la tarea drene la rafaga en curso.
constexpr uint32_t kStopTimeoutMs = 1500;

// Periodo de publicacion de estado hacia la UI.
constexpr uint32_t kStatusPeriodMs = 500;

// Plan de canales por defecto cuando StrikeConfig::channels viene vacio:
// los 13 canales de la banda de 2.4 GHz, igual que GhostESP.
constexpr uint8_t kFirstChannel = 1;
constexpr uint8_t kLastChannel = 13;

// Rafaga maxima por salto de canal (amortiza esp_wifi_set_channel, que es la
// parte cara del beacon spam).
constexpr uint8_t kMaxBurst = 32;

// Slice de espera entre iteraciones: dormir el intervalo entero haria que stop()
// tardase hasta 10 s en responder.
constexpr uint32_t kIdleSliceMs = 10;

// Sequence Control inicial (mismo valor por defecto que BeaconEmitter::Params).
constexpr uint16_t kInitialSeqControl = 0x6CC0;

/**
 * @brief Estado mutable de la sesion que solo pertenece a la tarea
 *
 * Vive en la pila de la tarea de emision: el indice del coro de Rickroll, el
 * cursor de la lista de SSIDs o de APs clonadas y la semilla del PRNG son
 * propios del hilo que emite, asi que no necesitan lock ni member variables.
 */
struct SessionState {
    StrikeConfig cfg;
    BeaconEmitter::Rng rng;
    uint8_t rickIndex = 0;
    size_t listIndex = 0;
    size_t apIndex = 0;
    uint16_t seqCounter = 0;
    uint8_t currentChannel = 0;
    uint8_t staMac[wifi::WIFI_MAC_LEN] = {0};
    bool haveStaMac = false;
    bool lockToStaChannel = false;

    /** Siguiente Sequence Control (12 bits de secuencia, fragmento a 0). */
    uint16_t nextSeq() {
        const uint16_t seq = static_cast<uint16_t>(
            kInitialSeqControl + static_cast<uint16_t>(seqCounter++) * 2);
        return static_cast<uint16_t>(seq & 0xFFF0);
    }
};

// Reserva de radio compartida por todos los consumidores Wi-Fi Strike.
std::mutex s_claimMutex;
RadioClaim s_claim;
uint32_t s_claimTokenSeed = 0x5746534Bu;   // 'WFSK'

} // namespace

WifiStrikeService& WifiStrikeService::getInstance() {
    static WifiStrikeService instance;
    return instance;
}

// ────────────────────────────────────────────────────────────────────────────
// Arbitraje de radio
// ────────────────────────────────────────────────────────────────────────────

RadioClaim WifiStrikeService::getRadioClaim() {
    std::lock_guard<std::mutex> lock(s_claimMutex);
    return s_claim;
}

bool WifiStrikeService::claimRadio(RadioClaimOwner owner) {
    std::lock_guard<std::mutex> lock(s_claimMutex);
    if (s_claim.active()) {
        // Reentrante: el mismo propietario puede refrescar la sesion sin
        // tumbar la reserva que ya tiene.
        return s_claim.owner == owner;
    }
    s_claim.owner = owner;
    s_claim.token = ++s_claimTokenSeed;
    if (s_claim.token == 0) s_claim.token = ++s_claimTokenSeed;
    s_claim.acquiredMs = system::getTimeMs();
    return true;
}

void WifiStrikeService::releaseRadio(uint32_t token) {
    std::lock_guard<std::mutex> lock(s_claimMutex);
    // Solo el dueno del token devuelve la radio: si otra sesion la ha
    // tomado despues, liberarla seria steals de una reserva ajena.
    if (!s_claim.active() || s_claim.token != token) return;
    s_claim.release();
}

// ────────────────────────────────────────────────────────────────────────────
// Configuracion y estado
// ────────────────────────────────────────────────────────────────────────────

StrikeConfig WifiStrikeService::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void WifiStrikeService::setConfig(const StrikeConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = cfg;
}

std::string WifiStrikeService::getLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

bool WifiStrikeService::isActive() const {
    return m_state.load() == StrikeState::Running;
}

void WifiStrikeService::setStatusCallback(StrikeStatusCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statusCb = std::move(cb);
}

void WifiStrikeService::fault(const char* reason) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = (reason && reason[0] != '\0') ? reason : "Fallo de radio";
    }
    m_state.store(StrikeState::Faulted);
    m_running.store(false);
}

StrikeStats WifiStrikeService::getStats() const {
    StrikeStats stats;
    stats.framesSent = m_framesSent.load();
    stats.framesFailed = m_framesFailed.load();
    stats.bytesSent = m_bytesSent.load();
    stats.channelHops = m_channelHops.load();
    stats.elapsedMs = m_elapsedMs.load();
    stats.lastChannel = static_cast<uint8_t>(m_lastChannel.load());
    stats.lastBurstSize = static_cast<uint8_t>(m_lastBurst.load());
    stats.lastError = getLastError();
    if (stats.elapsedMs > 0) {
        stats.framesPerSec = (static_cast<float>(stats.framesSent) * 1000.0f) /
                             static_cast<float>(stats.elapsedMs);
    }
    return stats;
}

StrikeStatus WifiStrikeService::getStatus() const {
    StrikeStatus status;
    status.state = m_state.load();
    status.active = (status.state == StrikeState::Running);
    status.mode = getConfig().mode;
    status.stats = getStats();
    status.lastError = status.stats.lastError;
    return status;
}

void WifiStrikeService::publishStatusLocked() {
    StrikeStatusCallback cb;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_statusCb;
    }
    if (cb) cb(getStatus());
}

// ────────────────────────────────────────────────────────────────────────────
// Ciclo de vida de la sesion
// ────────────────────────────────────────────────────────────────────────────

bool WifiStrikeService::startBeacon(const StrikeConfig& cfg) {
    if (m_state.load() != StrikeState::Idle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = "Ya hay una sesion de Wi-Fi Strike activa";
        return false;
    }

    if (!wifi::validate(cfg)) {
        const char* why = wifi::describeInvalid(cfg);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = std::string("Configuracion invalida: ") + (why ? why : "motivo desconocido");
        return false;
    }

    wifi::IWifiStrikeRadio* radio = wifi::getStrikeRadio();
    if (radio == nullptr || !radio->isAvailable()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = "Este target no tiene radio Wi-Fi para inyectar";
        return false;
    }

    // Arbitraje: la emision satura el controlador, asi que se toma la radio en
    // exclusiva antes de tocar el stack.
    if (!claimRadio(RadioClaimOwner::WifiStrike)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = std::string("Radio ocupada por ") +
                      wifi::getRadioClaimOwnerName(getRadioClaim().owner);
        return false;
    }
    const uint32_t token = getRadioClaim().token;

    StrikeConfig applied = cfg;
    if (applied.intervalMs < kMinIntervalMs) applied.intervalMs = kMinIntervalMs;
    if (applied.intervalMs > kMaxIntervalMs) applied.intervalMs = kMaxIntervalMs;
    if (applied.burstSize == 0) applied.burstSize = 1;
    if (applied.burstSize > kMaxBurst) applied.burstSize = kMaxBurst;
    if (applied.apList.size() > wifi::STRIKE_AP_LIST_MAX) {
        applied.apList.resize(wifi::STRIKE_AP_LIST_MAX);
    }
    if (applied.mode == BeaconMode::Random) {
        if (applied.randomSsidLength == 0 ||
            applied.randomSsidLength > wifi::STRIKE_SSID_MAX) {
            applied.randomSsidLength = wifi::STRIKE_RANDOM_SSID_LEN;
        }
    }

    // El PRNG se siembra con entropia real de la radio: los SSIDs y BSSIDs
    // generados tienen que diferir entre arranques, no repetir una secuencia
    // fija que cualquier cliente podria Learnear.
    if (applied.seed == 0) {
        uint8_t entropy[4] = {0};
        if (radio->getRandom(entropy, sizeof(entropy)) == RadioResult::Ok) {
            applied.seed = static_cast<uint32_t>(entropy[0]) |
                           (static_cast<uint32_t>(entropy[1]) << 8) |
                           (static_cast<uint32_t>(entropy[2]) << 16) |
                           (static_cast<uint32_t>(entropy[3]) << 24);
        } else {
            applied.seed = system::getTimeMs();
        }
    }

    if (radio->init() != RadioResult::Ok) {
        const char* halErr = radio->getLastError();
        releaseRadio(token);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = (halErr && halErr[0] != '\0') ? halErr
                                                    : "No se pudo preparar la radio para TX cruda";
        return false;
    }

    // El Air Recon comparte el mismo stack: si estaba escaneando se pausa y se
    // restaura al terminar, igual que hace BleTransmitter con el escaneo GAP.
    m_restoreScan.store(WifiShieldService::getInstance().isScanning());
    if (m_restoreScan.load()) {
        WifiShieldService::getInstance().stopScan();
        system::log(system::LogLevel::Info, TAG_STRIKE,
                    "Air Recon pausado por inicio de emision");
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = applied;
        m_lastError.clear();
    }

    m_framesSent.store(0);
    m_framesFailed.store(0);
    m_bytesSent.store(0);
    m_channelHops.store(0);
    m_elapsedMs.store(0);
    m_lastChannel.store(0);
    m_lastBurst.store(0);
    m_claimToken.store(token);
    m_state.store(StrikeState::Running);
    m_running.store(true);

    void* handle = rtos::createTask(
        [](void* p) { static_cast<WifiStrikeService*>(p)->emitTask(p); },
        "wifi_strike_tx", kEmitTaskStack, this, kEmitTaskPriority, kEmitTaskCore);

    if (handle == nullptr) {
        m_running.store(false);
        m_state.store(StrikeState::Idle);
        m_claimToken.store(0);
        radio->shutdown();
        if (m_restoreScan.exchange(false)) {
            WifiShieldService::getInstance().startScan();
        }
        releaseRadio(token);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = "No se pudo crear la tarea de emision";
        return false;
    }

    m_taskHandle.store(handle);
    system::log(system::LogLevel::Info, TAG_STRIKE, "Emision iniciada: modo=%s intervalo=%u ms rafaga=%u",
                wifi::getBeaconModeName(applied.mode),
                static_cast<unsigned>(applied.intervalMs),
                static_cast<unsigned>(applied.burstSize));
    return true;
}

void WifiStrikeService::stop() {
    const StrikeState state = m_state.load();
    if (state == StrikeState::Idle) return;
    // Un Faulted ya no tiene tarea viva que drenar: parar da el error por
    // reconocido y devuelve el servicio a Idle para otra sesion.
    if (state == StrikeState::Running) m_state.store(StrikeState::Stopping);
    else if (state == StrikeState::Faulted) m_state.store(StrikeState::Idle);
    m_running.store(false);

    // La tarea drena la rafaga en curso, libera la radio y se autodestruye.
    for (uint32_t waited = 0; waited < kStopTimeoutMs; waited += kIdleSliceMs) {
        if (m_taskHandle.load() == nullptr) break;
        system::sleepMs(kIdleSliceMs);
    }

    publishStatusLocked();
    system::log(system::LogLevel::Info, TAG_STRIKE, "Emision detenida (%u balizas)",
                static_cast<unsigned>(m_framesSent.load()));
}

// ────────────────────────────────────────────────────────────────────────────
// Tarea de emision
// ────────────────────────────────────────────────────────────────────────────

void WifiStrikeService::emitTask(void* param) {
    (void)param;

    wifi::IWifiStrikeRadio* radio = wifi::getStrikeRadio();

    SessionState st;
    st.cfg = getConfig();
    st.rng = BeaconEmitter::Rng(st.cfg.seed);

    // Plan de canales. Con la STA conectada el beacon spam no puede saltar de
    // canal: el enlace se caeria en cada hop, asi que se emite solo en el
    // canal ya asociado.
    st.lockToStaChannel = radio && radio->isStaConnected();
    if (!st.cfg.channels.empty()) {
        // El plan de canales lo fija el usuario: se respeta tal cual.
    } else if (st.lockToStaChannel) {
        uint8_t current = radio->getChannel();
        st.cfg.channels.push_back(current != 0 ? current : kFirstChannel);
    } else {
        for (uint8_t ch = kFirstChannel; ch <= kLastChannel; ++ch) {
            st.cfg.channels.push_back(ch);
        }
    }

    TxFrameParams txParams;
    txParams.iface = 0;        // interfaz STA: la que el controlador deja libre
    txParams.enSys = false;     // trama de gestion con destino broadcast

    uint32_t consecutiveFailures = 0;
    const uint32_t startMs = system::getTimeMs();
    uint32_t lastStatusMs = startMs;

    // Identidad fija de la AP propia cuando el usuario desactiva la rotacion
    // de BSSID; con rotacion activa cada trama lleva una MAC nueva.
    auto resolveSourceMac = [&](BeaconEmitter::Params& params) {
        if (st.cfg.rotateMac) {
            BeaconEmitter::fillRandomMac(params.bssid, st.rng);
            return;
        }
        if (!st.haveStaMac && radio) {
            if (radio->getMac(st.staMac) == RadioResult::Ok) {
                st.haveStaMac = true;
            }
        }
        if (st.haveStaMac) {
            std::memcpy(params.bssid, st.staMac, wifi::WIFI_MAC_LEN);
        } else {
            BeaconEmitter::fillRandomMac(params.bssid, st.rng);
        }
    };

    // Construye e inyecta una baliza ya construida.
    auto emitOne = [&](const BeaconFrame& frame) {
        if (!frame.valid) {
            m_framesFailed.fetch_add(1);
            return false;
        }
        const RadioResult result = radio->txFrame(frame.data, frame.len, txParams);
        if (result == RadioResult::Ok) {
            m_framesSent.fetch_add(1);
            m_bytesSent.fetch_add(static_cast<uint32_t>(frame.len));
            consecutiveFailures = 0;
            return true;
        }
        m_framesFailed.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastError = std::string("TX rechazada: ") + wifi::getRadioResultName(result);
        }
        // NoMemory y Busy son transitorios (buffers de TX ocupados o cola llena):
        // cuentan como fallo pero no tumban la sesion.
        if (result != RadioResult::NoMemory && result != RadioResult::Busy) {
            ++consecutiveFailures;
        }
        return false;
    };

    auto buildAndEmit = [&](uint8_t channel, uint32_t nowMs) {
        BeaconEmitter::Params params;
        params.channel = channel;
        params.timestamp = nowMs;          // TSF de la AP: ms desde el arranque
        params.seqControl = st.nextSeq();
        params.includeHeCapabilities = st.cfg.includeHeCapabilities;

        BeaconFrame frame;
        switch (st.cfg.mode) {
            case BeaconMode::Random: {
                // Cada trama lleva SSID y BSSID nuevos: es lo que distingue el
                // modo aleatorio del resto.
                BeaconEmitter::fillRandomMac(params.bssid, st.rng);
                frame = BeaconEmitter::buildRandomSsid(params, st.cfg.randomSsidLength, st.rng);
                break;
            }
            case BeaconMode::Rickroll: {
                resolveSourceMac(params);
                frame = BeaconEmitter::build(params, BeaconEmitter::rickrollLine(st.rickIndex));
                st.rickIndex = BeaconEmitter::nextRickrollIndex(st.rickIndex);
                break;
            }
            case BeaconMode::CloneAccessPoints: {
                if (st.cfg.apList.empty()) return false;
                const radio::WifiApRecord& ap = st.cfg.apList[st.apIndex % st.cfg.apList.size()];
                st.apIndex++;
                frame = BeaconEmitter::buildCloneAp(ap, st.cfg.includeHeCapabilities);
                break;
            }
            case BeaconMode::CustomList: {
                resolveSourceMac(params);
                if (!st.cfg.ssidList.empty()) {
                    const std::string& ssid = st.cfg.ssidList[st.listIndex % st.cfg.ssidList.size()];
                    st.listIndex++;
                    frame = BeaconEmitter::build(params, ssid);
                } else {
                    frame = BeaconEmitter::build(params, st.cfg.ssid);
                }
                break;
            }
            case BeaconMode::SingleSsid:
            default: {
                resolveSourceMac(params);
                frame = BeaconEmitter::build(params, st.cfg.ssid);
                break;
            }
        }
        return emitOne(frame);
    };

    // Salta de canal solo cuando hace falta: es la operacion cara del ciclo.
    auto hopTo = [&](uint8_t channel) -> bool {
        if (st.currentChannel == channel) return true;
        const RadioResult result = radio->setChannel(channel);
        if (result != RadioResult::Ok) {
            m_framesFailed.fetch_add(1);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastError = std::string("Cambio a canal ") + std::to_string(channel) +
                              " rechazado: " + wifi::getRadioResultName(result);
            }
            ++consecutiveFailures;
            return false;
        }
        st.currentChannel = channel;
        m_channelHops.fetch_add(1);
        m_lastChannel.store(channel);
        return true;
    };

    while (m_running.load()) {
        const uint32_t nowMs = system::getTimeMs();
        m_elapsedMs.store(nowMs - startMs);

        if (st.cfg.mode == BeaconMode::CloneAccessPoints) {
            // Evil Twin: cada clon se emite en el canal real de su AP, asi que
            // la iteracion recorre la lista en vez del plan de canales.
            for (size_t i = 0; i < st.cfg.apList.size() && m_running.load(); ++i) {
                const uint8_t apChannel = st.cfg.apList[i].channel != 0
                                              ? st.cfg.apList[i].channel
                                              : st.currentChannel;
                if (!hopTo(apChannel)) break;
                m_lastBurst.store(1);
                buildAndEmit(apChannel, system::getTimeMs());
            }
        } else {
            // Solo el modo aleatorio amortiza el hop: en los modos de SSID fijo
            // cada iteracion del bucle emite una sola trama.
            const uint8_t burst =
                (st.cfg.mode == BeaconMode::Random && st.cfg.burstSize > 0)
                    ? st.cfg.burstSize
                    : 1;
            m_lastBurst.store(burst);
            for (uint8_t channel : st.cfg.channels) {
                if (!m_running.load()) break;
                if (!hopTo(channel)) break;
                for (uint8_t i = 0; i < burst && m_running.load(); ++i) {
                    buildAndEmit(channel, system::getTimeMs());
                    if (consecutiveFailures >= kMaxConsecutiveFailures) break;
                }
                if (consecutiveFailures >= kMaxConsecutiveFailures) break;
            }
        }

        if (consecutiveFailures >= kMaxConsecutiveFailures) {
            fault("La radio rechazo la emision de forma consecutiva");
            break;
        }

        if ((nowMs - lastStatusMs) >= kStatusPeriodMs) {
            lastStatusMs = nowMs;
            publishStatusLocked();
        }

        // Espera del intervalo en trozos: dormirlo entero retrasaria la parada
        // hasta 10 s y mantendria la radio reservada sin emitir nada.
        uint32_t waited = 0;
        while (m_running.load() && waited < st.cfg.intervalMs) {
            const uint32_t slice = (st.cfg.intervalMs - waited < kIdleSliceMs)
                                       ? (st.cfg.intervalMs - waited)
                                       : kIdleSliceMs;
            system::sleepMs(slice);
            waited += slice;
        }
    }

    // --- Drenado y devolucion de la radio ---------------------------------
    const uint32_t token = m_claimToken.load();
    if (radio) {
        radio->shutdown();
        wifi::notifyStrikeRadioState(wifi::StrikeRadioState::Idle);
    }

    if (m_restoreScan.exchange(false)) {
        WifiShieldService::getInstance().startScan();
        system::log(system::LogLevel::Info, TAG_STRIKE, "Air Recon restaurado");
    }

    releaseRadio(token);
    m_claimToken.store(0);
    m_elapsedMs.store(system::getTimeMs() - startMs);
    // El estado Faulted se conserva: es el unico rastro de por que se cayo la
    // sesion una vez que la tarea ya ha soltado la radio.
    if (m_state.load() != StrikeState::Faulted) {
        m_state.store(StrikeState::Idle);
    }

    publishStatusLocked();
    m_taskHandle.store(nullptr);
    rtos::deleteTask(nullptr);
}

} // namespace security
} // namespace cbdos