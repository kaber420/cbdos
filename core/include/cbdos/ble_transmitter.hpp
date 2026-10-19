#pragma once

// Orquestador de emisión publicitaria BLE (BLE Lab).
//
// Es la máquina de estados que mueve el ciclo de TX. Deliberadamente agnóstico:
// no conoce NimBLE ni FreeRTOS, solo habla con cbdos::ble::IBleAdapter.
//
// Responsabilidades:
//   - Construir payloads vía BlePayloadRegistry y entregarlos al adaptador.
//   - Arbitrar el conflicto escaneo/emisión: al arrancar la TX detiene el
//     escaneo, y al detener la TX lo restaura. Un solo stack cubre ambos modos.
//   - Rotar la MAC según la política configurada para evitar la deduplicación
//     que iOS, Android y Windows aplican a los paquetes repetidos.
//   - Publicar métricas (paquetes, paquetes/s) para la UI.

#include "cbdos/ble.hpp"
#include "cbdos/ble_payloads.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace cbdos {
namespace ble {

/** Niveles de potencia TX seleccionables. El techo depende del SoC: el
 *  ESP32-S3 llega a +21 dBm, los chips C6/H2/C5 a +20 dBm. */
enum class BleTxPower : uint8_t {
    Max = 0,   // +21 dBm
    High,      // +12 dBm
    Medium,    // +6 dBm
    Low        // +3 dBm
};

/** Configuración completa de una sesión de emisión. */
struct BleTxConfig {
    BlePayloadCategory category = BlePayloadCategory::ApplePairing;
    int variantIndex = 0;                          // -1 = Random / All
    uint16_t intervalMs = 100;
    BleTxPower txPower = BleTxPower::Max;
    BleMacRotationMode macRotation = BleMacRotationMode::EveryPacket;
};

/** Snapshot inmutable del estado de la sesión. */
struct BleTxStatus {
    bool transmitting = false;
    BleTxConfig config;
    BleAdvStats stats;
    BleAdapterState adapterState = BleAdapterState::Unavailable;
    std::string lastError;
};

/** Callback de actualización de estado (core → UI). */
using BleTxStatusCallback = std::function<void(const BleTxStatus& status)>;

/**
 * @brief Orquestador singleton de emisión BLE
 */
class BleTransmitter {
public:
    static BleTransmitter& getInstance();

    /** Aplica la configuración. No arranca transmisión; llámalo antes de start(). */
    void setConfig(const BleTxConfig& config);
    BleTxConfig getConfig() const;

    /** Arranca la emisión. Si el escaneo está activo, lo detiene y lo restaurará
     *  al terminar. Devuelve false con motivo en getLastError(). */
    bool start();

    /** Detiene la emisión y restaura el escaneo si estaba activo antes. */
    void stop();

    /** Detiene y descarta la emission pendiente sin restaurar el escaneo
     *  (usado al salir de la vista). */
    void shutdown();

    bool isTransmitting() const { return m_transmitting.load(); }
    BleTxStatus getStatus() const;
    std::string getLastError() const;

    void setStatusCallback(BleTxStatusCallback cb);

    /** Traduce el nivel de potencia al valor en dBm esperado por el stack. */
    static int8_t powerToDbm(BleTxPower power);

private:
    BleTransmitter() = default;
    ~BleTransmitter() = default;
    BleTransmitter(const BleTransmitter&) = delete;
    BleTransmitter& operator=(const BleTransmitter&) = delete;

    void txTask(void* param);
    bool emitOnce();
    bool shouldRotateMac(uint32_t packetIndex) const;

    mutable std::mutex m_mutex;
    BleTxConfig m_config;
    std::atomic<bool> m_transmitting{false};
    std::atomic<bool> m_restoreScan{false};
    std::atomic<uint32_t> m_packetsSent{0};
    std::atomic<uint32_t> m_elapsedMs{0};
    std::string m_lastError;
    BleTxStatusCallback m_statusCb;

    void* m_taskHandle = nullptr;
};

} // namespace ble
} // namespace cbdos