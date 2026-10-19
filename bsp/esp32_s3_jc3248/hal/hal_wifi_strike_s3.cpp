#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_random.h>

#include <cstdio>

#include "cbdos/system.hpp"
#include "cbdos/wifi_radio.hpp"

// Override de enlazado sobre libnet80211.a: el blob original bloquea subtipos
// de gestion 0xC0 (Deauth) y 0xA0 (Disassoc) asi como MACs de origen
// suplantadas devolviendo ESP_ERR_INVALID_ARG (0x102). Devolver 0 autoriza a
// esp_wifi_80211_tx() a inyectar cualquier trama construida por el servicio.
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
    (void)arg;
    (void)arg2;
    (void)arg3;
    return 0;
}

namespace cbdos {
namespace bsp {

static const char* TAG_STRIKE_S3 = "HAL_STRIKE_S3";

using wifi::IWifiStrikeRadio;
using wifi::RadioResult;
using wifi::StrikeRadioState;
using wifi::TxFrameParams;

// ===========================================================================
// Radio Wi-Fi Strike sobre el controlador del ESP32-S3
// ===========================================================================
//
// La inyeccion usa esp_wifi_80211_tx() con la interfaz STA en modo broadcast
// de sistema desactivado: es la misma via que GhostESP y Marauder usan para
// el beacon spam y la unica que permite sacar del controlador una trama de
// gestion 802.11 completa tal cual la ha construido BeaconEmitter.
//
// Requisitos del controlador, aplicados en init():
//   - modo STA: esp_wifi_80211_tx() no acepta tramas crudas si la interfaz es
//     AP con cliente asociado, y el beacon spam corre sin SoftAP propia.
//   - promiscuo: sin el, el controlador descarta el paquete antes de emitirlo.
//   - almacenamiento en RAM: esp_wifi_80211_tx() escribe la trama en la
//     flash si el stack esta en WIFI_STORAGE_FLASH. Con el beacon spam eso son
//     decenas de escrituras por segundo y deja la flash inservible en minutos.
//     Por eso el modo de la cabecera de red se pone a RAM al arrancar y se
//     restaura en shutdown().
// ===========================================================================

class WifiStrikeRadioS3 : public IWifiStrikeRadio {
public:
    bool isAvailable() const override {
        // El ESP32-S3 siempre trae radio 2.4 GHz integrated; no hay un path de
        // compilacion donde este target no la tenga.
        return true;
    }

    RadioResult init() override {
        if (m_state == StrikeRadioState::Emitting) return RadioResult::Ok;

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t err = esp_wifi_init(&cfg);
        // ESP_ERR_INVALID_STATE = el stack ya lo arranco otro consumidor
        // (Air Recon, SoftAP del portal, cliente STA). Es el caso normal.
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return setError(RadioResult::HardwareError, "esp_wifi_init fallo");
        }
        m_initialized = true;

        err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            // Fallback a modo STA si APSTA falla por estado previo
            err = esp_wifi_set_mode(WIFI_MODE_STA);
            if (err != ESP_OK) {
                return setError(RadioResult::HardwareError, "esp_wifi_set_mode fallo");
            }
        }

        err = esp_wifi_start();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return setError(RadioResult::NotReady, "esp_wifi_start fallo");
        }

        // La trama se inyecta desde RAM: sin esto cada beacon acaba en la flash.
        if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
            return setError(RadioResult::HardwareError, "esp_wifi_set_storage(RAM) fallo");
        }
        m_storageIsRam = true;

        err = esp_wifi_set_promiscuous(true);
        if (err != ESP_OK) {
            return setError(RadioResult::HardwareError, "esp_wifi_set_promiscuous fallo");
        }
        m_promiscuous = true;

        setState(StrikeRadioState::Idle);
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_STRIKE_S3,
                           "Radio Wi-Fi Strike lista (APSTA + promiscuo, TX en RAM)");
        return RadioResult::Ok;
    }

    RadioResult txFrame(const uint8_t* data, size_t len,
                        const TxFrameParams& params) override {
        if (data == nullptr || len == 0) {
            return setError(RadioResult::InvalidArg, "trama vacia");
        }
        if (!m_initialized) {
            return setError(RadioResult::NotReady, "init() no llamado");
        }
        if (len > kMaxTxFrameSize) {
            return setError(RadioResult::InvalidArg, "trama mayor que el tope del controlador");
        }

        const wifi_interface_t primaryIface =
            (params.iface == 1) ? WIFI_IF_AP : WIFI_IF_STA;
        const wifi_interface_t fallbackIface =
            (primaryIface == WIFI_IF_STA) ? WIFI_IF_AP : WIFI_IF_STA;
        setState(StrikeRadioState::Emitting);

        esp_err_t err = esp_wifi_80211_tx(primaryIface, data, static_cast<int>(len), params.enSys);
        if (err == ESP_OK) return RadioResult::Ok;

        // Si la interfaz primaria fue rechazada por el controlador (ej. rol MAC), reintentar por la interfaz secundaria
        if (err != ESP_ERR_NO_MEM) {
            err = esp_wifi_80211_tx(fallbackIface, data, static_cast<int>(len), params.enSys);
            if (err == ESP_OK) return RadioResult::Ok;
        }

        if (err == ESP_ERR_NO_MEM) {
            // Sin buffers de TX: el emisor reintenta tras su pausa.
            return setError(RadioResult::NoMemory, "sin buffers de TX");
        }

        char errBuf[96];
        snprintf(errBuf, sizeof(errBuf), "esp_wifi_80211_tx fallo: %s (0x%x)",
                 esp_err_to_name(err), err);
        return setError(RadioResult::HardwareError, errBuf);
    }

    RadioResult setChannel(uint8_t channel) override {
        if (channel == 0) {
            return setError(RadioResult::InvalidArg, "canal 0");
        }
        if (!m_initialized) {
            return setError(RadioResult::NotReady, "init() no llamado");
        }
        // WIFI_SECOND_CHAN_NONE: sin HT40. El beacon spam necesita el canal
        // primario exacto que anuncia el DS Parameter Set de cada baliza.
        const esp_err_t chErr = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (chErr != ESP_OK) {
            char chBuf[96];
            snprintf(chBuf, sizeof(chBuf), "esp_wifi_set_channel(%u) fallo: %s (0x%x)",
                     channel, esp_err_to_name(chErr), chErr);
            return setError(RadioResult::HardwareError, chBuf);
        }
        m_channel = channel;
        return RadioResult::Ok;
    }

    uint8_t getChannel() const override {
        uint8_t primary = 0;
        wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
        if (esp_wifi_get_channel(&primary, &second) == ESP_OK) {
            return primary;
        }
        return m_channel;
    }

    RadioResult getMac(uint8_t* out) const override {
        if (out == nullptr) {
            return RadioResult::InvalidArg;
        }
        if (esp_wifi_get_mac(WIFI_IF_STA, out) != ESP_OK) {
            return setError(RadioResult::HardwareError, "esp_wifi_get_mac fallo");
        }
        return RadioResult::Ok;
    }

    RadioResult getRandom(uint8_t* out, size_t len) override {
        if (out == nullptr || len == 0) {
            return RadioResult::InvalidArg;
        }
        esp_fill_random(out, len);
        return RadioResult::Ok;
    }

    bool isStaConnected() const override {
        wifi_ap_record_t ap;
        return esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
    }

    StrikeRadioState getState() const override {
        return m_state;
    }

    void shutdown() override {
        if (m_promiscuous) {
            esp_wifi_set_promiscuous(false);
            m_promiscuous = false;
        }
        if (m_storageIsRam) {
            esp_wifi_set_storage(WIFI_STORAGE_FLASH);
            m_storageIsRam = false;
        }
        m_initialized = false;
        m_channel = 0;
        setState(StrikeRadioState::Idle);
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_STRIKE_S3,
                           "Radio Wi-Fi Strike devuelta a modo normal");
    }

    const char* getLastError() const override {
        return m_lastError;
    }

private:
    // El controlador ESP32 no admite tramas mas largas que el buffer de TX
    // interno (la trama beacon completa mide 104 bytes como maximo).
    static constexpr size_t kMaxTxFrameSize = 1600;

    void setState(StrikeRadioState state) {
        if (m_state == state) return;
        m_state = state;
        wifi::notifyStrikeRadioState(state);
    }

    RadioResult setError(RadioResult result, const char* reason) const {
        snprintf(m_lastError, sizeof(m_lastError), "%s (%s)", reason,
                 wifi::getRadioResultName(result));
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG_STRIKE_S3, "%s",
                           m_lastError);
        return result;
    }

    mutable char m_lastError[96] = {0};
    bool m_initialized = false;
    bool m_promiscuous = false;
    bool m_storageIsRam = false;
    uint8_t m_channel = 0;
    StrikeRadioState m_state = StrikeRadioState::Idle;
};

static WifiStrikeRadioS3 s_strikeRadioS3;

void initWifiStrikeS3() {
    wifi::registerStrikeRadio(&s_strikeRadioS3);
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_STRIKE_S3,
                       "Radio Wi-Fi Strike registrada (esp_wifi_80211_tx)");
}

} // namespace bsp
} // namespace cbdos