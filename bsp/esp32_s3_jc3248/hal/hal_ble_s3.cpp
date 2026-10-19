#include <Arduino.h>

#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEAddress.h>
#include <NimBLEUtils.h>

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/ble.h"

#include <esp_random.h>

#include "cbdos/ble.hpp"
#include "cbdos/system.hpp"
#include "security/BleShieldService.hpp"

namespace cbdos {
namespace bsp {

static const char* TAG_BLE = "HAL_BLE_S3";

using ble::BleAdapterState;
using ble::BleAdvParams;
using ble::BleMacRotationMode;
using ble::IBleAdapter;

// ===========================================================================
// Callbacks de escaneo GAP
// ===========================================================================

namespace {

class S3BleScanCallbacks : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* device) override {
        radio::BleRawPacket pkt;

        // val[] de NimBLE esta en orden nativo (little-endian): index 0 es el
        // byte menos significativo, que es el que se emite primero.
        const uint8_t* mac = device->getAddress().getBase()->val;
        if (mac != nullptr) {
            memcpy(pkt.mac, mac, 6);
        }

        // Los dos bits altos del ultimo byte marcan direccion aleatoria.
        pkt.isRandomMac = (mac != nullptr) && ((mac[5] & 0xC0) != 0);
        pkt.rssi = static_cast<int8_t>(device->getRSSI());

        const std::vector<uint8_t>& payload = device->getPayload();
        if (!payload.empty()) {
            size_t len = payload.size();
            if (len > sizeof(pkt.payload)) {
                len = sizeof(pkt.payload);
            }
            memcpy(pkt.payload, payload.data(), len);
            pkt.payloadLen = len;
        }

        pkt.timestampMs = millis();

        security::BleShieldService::getInstance().processRawPacket(pkt);
    }
};

} // namespace

// ===========================================================================
// Adaptador NimBLE
// ===========================================================================

class S3BleAdapter : public IBleAdapter {
public:
    bool isAvailable() const override {
#if defined(CONFIG_BT_ENABLED) || defined(CONFIG_BT_NIMBLE_ENABLED)
        return true;
#else
        return false;
#endif
    }

    bool init(const char* deviceName) override {
        if (m_initialized) return true;

        if (!NimBLEDevice::init(deviceName != nullptr ? deviceName : "CBDos-S3")) {
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG_BLE,
                               "NimBLEDevice::init() fallo");
            return false;
        }

        NimBLEDevice::setPower(21, NimBLETxPowerType::All);

        m_pScan = NimBLEDevice::getScan();
        m_pScan->setScanCallbacks(&s_scanCallbacks, false);
        m_pScan->setActiveScan(true);
        m_pScan->setInterval(50);
        m_pScan->setWindow(45);
        m_pScan->setDuplicateFilter(false);

        m_initialized = true;
        m_state = BleAdapterState::Idle;

        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_BLE,
                           "Stack NimBLE inicializado");
        return true;
    }

    bool startScan() override {
        if (!m_initialized && !init("CBDos-S3")) return false;
        if (m_pScan == nullptr) return false;

        // duration 0 = BLE_HS_FOREVER en NimBLE: escaneo continuo hasta que
        // BleShieldService lo detenga. El filtrado de duplicados lo hace el
        // host (wantDuplicates=false en setScanCallbacks), asi que la memoria
        // queda acotada por el numero de dispositivos unicos, no por el tiempo.
        m_pScan->clearResults();
        if (!m_pScan->start(0)) {
            cbdos::system::log(cbdos::system::LogLevel::Warn, TAG_BLE,
                               "NimBLEScan::start() fallo");
            return false;
        }

        m_state = BleAdapterState::Scanning;
        cbdos::ble::notifyStateChanged(m_state);

        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_BLE,
                           "Escaner GAP BLE (NimBLE) iniciado");
        return true;
    }

    bool stopScan() override {
        if (m_pScan != nullptr) {
            m_pScan->stop();
            m_pScan->clearResults();
        }
        if (m_state == BleAdapterState::Scanning) {
            m_state = BleAdapterState::Idle;
            cbdos::ble::notifyStateChanged(m_state);
        }
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_BLE,
                           "Escaner GAP BLE detenido");
        return true;
    }

    bool startAdvertising(const BleAdvParams& params) override {
        m_lastError.clear();
        if (!m_initialized && !init("CBDos-Lab")) {
            m_lastError = "Fallo init NimBLE";
            return false;
        }
        if (params.data == nullptr || params.len == 0 || params.len > 31) {
            m_lastError = "Parametros o len invalido";
            return false;
        }

        // 1. Detener cualquier transmisión activa previa (patrón GhostESP)
        if (ble_gap_adv_active()) {
            ble_gap_adv_stop();
        }

        // 2. Rotación de dirección MAC nativa directa en el Host de NimBLE
        uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
        if (params.macRotation != BleMacRotationMode::Off) {
            uint8_t rnd_addr[6];
            esp_fill_random(rnd_addr, 6);
            rnd_addr[5] |= 0xC0; // Static random address (top two bits = 11)
            if ((rnd_addr[0] | rnd_addr[1] | rnd_addr[2] | rnd_addr[3] | rnd_addr[4]) == 0) {
                rnd_addr[0] = 0x01;
            }
            int rc = ble_hs_id_set_rnd(rnd_addr);
            if (rc != 0) {
                cbdos::system::log(cbdos::system::LogLevel::Warn, TAG_BLE,
                                   "ble_hs_id_set_rnd fallo: %d", rc);
            }
            own_addr_type = BLE_OWN_ADDR_RANDOM;
        }

        // 3. Cargar el payload crudo directamente al controlador GAP sin manipulaciones
        int rc = ble_gap_adv_set_data(params.data, params.len);
        if (rc != 0) {
            m_lastError = "set_data fallo (" + std::to_string(rc) + ")";
            cbdos::system::log(cbdos::system::LogLevel::Warn, TAG_BLE,
                               "ble_gap_adv_set_data fallo: %d", rc);
            return false;
        }

        // 4. Parámetros de emisión nativos (GhostESP / Bruce: no-conectible, descubrible)
        struct ble_gap_adv_params adv_params;
        memset(&adv_params, 0, sizeof(adv_params));
        adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
        adv_params.channel_map = 0x07; // Canales 37, 38, 39

        uint32_t intervalMs = (params.intervalMs < 20) ? 20 : params.intervalMs;
        if (intervalMs > 10240) intervalMs = 10240;
        uint16_t slots = static_cast<uint16_t>((intervalMs * 8) / 5);
        if (slots < 0x0020) slots = 0x0020;
        if (slots > 0x4000) slots = 0x4000;

        adv_params.itvl_min = slots;
        adv_params.itvl_max = slots;

        if (params.txPowerDbm != 127) {
            NimBLEDevice::setPower(params.txPowerDbm, NimBLETxPowerType::Advertise);
        }

        // 5. Iniciar la emisión permanente controlada por CBDos
        rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
        if (rc != 0) {
            m_lastError = "start fallo (" + std::to_string(rc) + ")";
            cbdos::system::log(cbdos::system::LogLevel::Warn, TAG_BLE,
                               "ble_gap_adv_start fallo: %d", rc);
            return false;
        }

        if (m_state != BleAdapterState::Advertising) {
            m_state = BleAdapterState::Advertising;
            cbdos::ble::notifyStateChanged(m_state);
        }
        return true;
    }

    bool stopAdvertising() override {
        if (ble_gap_adv_active()) {
            ble_gap_adv_stop();
        }

        if (m_state == BleAdapterState::Advertising) {
            m_state = BleAdapterState::Idle;
            cbdos::ble::notifyStateChanged(m_state);
        }
        return true;
    }

    bool setTxPower(int8_t dbm) override {
        return NimBLEDevice::setPower(dbm, NimBLETxPowerType::Advertise);
    }

    BleAdapterState getState() const override {
        return m_state;
    }

    void shutdown() override {
        if (!m_initialized) return;
        stopAdvertising();
        stopScan();
        // El stack se deja vivo a proposito: NimBLEHost::deinit() cuesta
        // segundos y fragmenta el heap, y un unico stack cubre RX y TX.
        // Cambiar de app no lo requiere.
        m_initialized = false;
        m_state = BleAdapterState::Idle;
        cbdos::ble::notifyStateChanged(m_state);
    }

    const char* getLastError() const override {
        return m_lastError.c_str();
    }

private:
    static S3BleScanCallbacks s_scanCallbacks;

    NimBLEScan* m_pScan = nullptr;
    BleAdapterState m_state = BleAdapterState::Idle;
    std::string m_lastError;
    bool m_initialized = false;
};

S3BleScanCallbacks S3BleAdapter::s_scanCallbacks;

} // namespace bsp
} // namespace cbdos

// ===========================================================================
// Registro en el core
// ===========================================================================

namespace cbdos {
namespace bsp {

static S3BleAdapter s_s3BleAdapter;

// Air Recon (BleShieldService) conserva su contrato intacto: sigue recibiendo
// un handler (start)->bool. Lo unico que cambia es quien lo ejecuta: NimBLE
// en lugar de Bluedroid.
static bool s3BleScanControl(bool start) {
    return start ? s_s3BleAdapter.startScan() : s_s3BleAdapter.stopScan();
}

void initBleDriverS3() {
    security::BleShieldService::getInstance().registerScanControl(s3BleScanControl);
    cbdos::ble::registerAdapter(&s_s3BleAdapter);
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_BLE,
                       "Driver BLE (NimBLE) registrado");
}

} // namespace bsp
} // namespace cbdos