#include "cbdos/ble_transmitter.hpp"

#include "cbdos/ble_payloads.hpp"
#include "cbdos/rtos.hpp"
#include "cbdos/system.hpp"

namespace cbdos {
namespace ble {

static const char* TAG_BLE_TX = "BLE_TX";

// Task stack: el ciclo de TX llama a BlePayloadRegistry y al stack NimBLE/HCI.
// 8192 bytes previenen cualquier desbordamiento de pila en FreeRTOS.
static constexpr uint32_t kTxTaskStack = 8192;

BleTransmitter& BleTransmitter::getInstance() {
    static BleTransmitter instance;
    return instance;
}

int8_t BleTransmitter::powerToDbm(BleTxPower power) {
    switch (power) {
        case BleTxPower::Max:   return 21;
        case BleTxPower::High:  return 12;
        case BleTxPower::Medium: return 6;
        case BleTxPower::Low:   return 3;
    }
    return 21;
}

void BleTransmitter::setConfig(const BleTxConfig& config) {
    BleTxConfig applied = config;
    if (applied.intervalMs < 20) applied.intervalMs = 20;
    if (applied.intervalMs > 10240) applied.intervalMs = 10240;

    BleTxStatusCallback cb = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_config = applied;
        cb = m_statusCb;
    }
    // Si la categoría tiene variantes, el índice fuera de rango se deja pasar
    // como Random/All; lo normaliza el propio registro.
    if (cb) cb(getStatus());
}

BleTxConfig BleTransmitter::getConfig() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_config;
}

std::string BleTransmitter::getLastError() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_lastError;
}

void BleTransmitter::setStatusCallback(BleTxStatusCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_statusCb = std::move(cb);
}

BleTxStatus BleTransmitter::getStatus() const {
    BleTxStatus st;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        st.config = m_config;
        st.lastError = m_lastError;
    }
    st.transmitting = m_transmitting.load();
    st.adapterState = getState();
    st.stats.packetsSent = m_packetsSent.load();
    st.stats.elapsedMs = m_elapsedMs.load();
    if (st.stats.elapsedMs > 0) {
        st.stats.packetsPerSec =
            (static_cast<float>(st.stats.packetsSent) * 1000.0f) /
            static_cast<float>(st.stats.elapsedMs);
    }
    return st;
}

bool BleTransmitter::shouldRotateMac(uint32_t packetIndex) const {
    uint32_t every = 0;
    switch (m_config.macRotation) {
        case BleMacRotationMode::Off:         return false;
        case BleMacRotationMode::EveryPacket: every = 1;  break;
        case BleMacRotationMode::Every2:      every = 2;  break;
        case BleMacRotationMode::Every3:      every = 3;  break;
        case BleMacRotationMode::Every5:      every = 5;  break;
        case BleMacRotationMode::Every10:     every = 10; break;
        case BleMacRotationMode::Every25:     every = 25; break;
        case BleMacRotationMode::Every50:     every = 50; break;
    }
    if (every == 0) return false;
    return (packetIndex % every) == 0;
}

bool BleTransmitter::emitOnce() {
    IBleAdapter* adapter = getAdapter();
    if (!adapter) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "Sin adaptador BLE registrado";
        return false;
    }

    BleTxConfig cfg;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        cfg = m_config;
    }

    BleAdvPayload payload = BlePayloadRegistry::build(cfg.category, cfg.variantIndex);
    if (!payload.valid) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "Payload vacio o truncado";
        return false;
    }

    BleAdvParams params;
    params.data = payload.data;
    params.len = payload.len;
    params.intervalMs = cfg.intervalMs;
    params.txPowerDbm = powerToDbm(cfg.txPower);
    params.macRotation = shouldRotateMac(m_packetsSent.load()) ? cfg.macRotation : BleMacRotationMode::Off;

    if (!adapter->startAdvertising(params)) {
        std::lock_guard<std::mutex> lk(m_mutex);
        const char* halErr = adapter->getLastError();
        m_lastError = (halErr && halErr[0] != '\0') ? halErr : "El stack rechazo startAdvertising()";
        return false;
    }

    m_packetsSent.fetch_add(1);
    return true;
}

void BleTransmitter::txTask(void* param) {
    (void)param;
    IBleAdapter* adapter = getAdapter();

    uint32_t startMs = system::getTimeMs();
    uint32_t windowStart = startMs;
    uint32_t windowPackets = 0;
    uint32_t nextEmitMs = startMs;

    system::log(system::LogLevel::Info, TAG_BLE_TX, "Ciclo de transmision iniciado");

    while (m_transmitting.load()) {
        uint32_t now = system::getTimeMs();
        m_elapsedMs.store(now - startMs);

        if (now >= nextEmitMs) {
            if (emitOnce()) {
                windowPackets++;
                nextEmitMs = now + m_config.intervalMs;
            } else {
                // Error de construcción o de stack: no martillear el stack.
                system::log(system::LogLevel::Warn, TAG_BLE_TX, "Fallo al emitir: %s",
                            getLastError().c_str());
                system::sleepMs(200);
                nextEmitMs = system::getTimeMs() + 200;
            }

            // Publica métricas cada 500 ms para que la UI refresque la tasa.
            uint32_t windowElapsed = now - windowStart;
            if (windowElapsed >= 500) {
                BleTxStatusCallback cb;
                {
                    std::lock_guard<std::mutex> lk(m_mutex);
                    cb = m_statusCb;
                }
                if (cb) cb(getStatus());
                windowStart = now;
                windowPackets = 0;
            }
        }
        system::sleepMs(1);
    }

    if (adapter) adapter->stopAdvertising();

    system::log(system::LogLevel::Info, TAG_BLE_TX, "Ciclo de transmision detenido (%u paquetes)",
                static_cast<unsigned>(m_packetsSent.load()));

    // Restaura el escaneo si esta sesión lo interrumpió.
    if (m_restoreScan.exchange(false)) {
        if (adapter && adapter->startScan()) {
            system::log(system::LogLevel::Info, TAG_BLE_TX, "Escaneo GAP restaurado");
        }
    }

    BleTxStatusCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        cb = m_statusCb;
    }
    if (cb) cb(getStatus());

    m_taskHandle = nullptr;
    rtos::deleteTask(nullptr);
}

bool BleTransmitter::start() {
    if (m_transmitting.load()) return true;

    IBleAdapter* adapter = getAdapter();
    if (!adapter) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "Sin adaptador BLE registrado";
        return false;
    }
    if (!adapter->isAvailable()) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "Este target no tiene radio Bluetooth";
        return false;
    }
    if (!adapter->init("CBDos-Lab")) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "No se pudo inicializar el stack BLE";
        return false;
    }

    if (m_taskHandle != nullptr) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "Ya hay una sesion de transmision activa";
        return false;
    }

    // Arbitraje: la TX y el escaneo compiten por la misma radio. Si había un
    // escaneo activo (Air Recon), se pausa y se restaura al terminar.
    m_restoreScan.store(adapter->getState() == BleAdapterState::Scanning);
    if (m_restoreScan.load()) {
        adapter->stopScan();
        system::log(system::LogLevel::Info, TAG_BLE_TX,
                    "Escaneo pausado por inicio de transmision");
    }

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError.clear();
    }

    m_packetsSent.store(0);
    m_elapsedMs.store(0);
    m_transmitting.store(true);

    m_taskHandle = rtos::createTask(
        [](void* p) { static_cast<BleTransmitter*>(p)->txTask(p); },
        "ble_tx_task", kTxTaskStack, this, 4, 0);

    if (m_taskHandle == nullptr) {
        m_transmitting.store(false);
        m_restoreScan.store(false);
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastError = "No se pudo crear la tarea de transmision";
        return false;
    }
    return true;
}

void BleTransmitter::stop() {
    if (!m_transmitting.load()) return;
    m_transmitting.store(false);

    // Esperar hasta 500 ms a que la tarea complete la limpieza y se autodestruya
    for (int i = 0; i < 50 && m_taskHandle != nullptr; ++i) {
        system::sleepMs(10);
    }
}

void BleTransmitter::shutdown() {
    m_restoreScan.store(false);
    stop();
}

} // namespace ble
} // namespace cbdos