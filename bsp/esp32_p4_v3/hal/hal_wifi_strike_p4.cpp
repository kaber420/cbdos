#include "cbdos/system.hpp"
#include "cbdos/wifi_radio.hpp"

namespace cbdos {
namespace bsp {

static const char* TAG_STRIKE_P4 = "HAL_STRIKE_P4";

using wifi::IWifiStrikeRadio;
using wifi::RadioResult;
using wifi::StrikeRadioState;
using wifi::TxFrameParams;

// ===========================================================================
// Stub de Wi-Fi Strike para el ESP32-P4
// ===========================================================================
//
// El P4 no tiene radio Wi-Fi integrada: el stack es un companion C6 aparte, y
// la API raw 802.11 que exige el beacon spam (esp_wifi_80211_tx) vive en el
// companion y no es alcanzable desde este SoC. En lugar de dejar el puntero a
// nullptr -que obliga a cada consumidor a comprobar null- se inyecta este stub:
// isAvailable() devuelve false y todas las operaciones responden
// RadioResult::Unsupported.
//
// Es el mismo patron documentado en wifi_radio.hpp para el caso "target sin
// radio": la UI puede preguntar por la disponibilidad sin compilar condicionales
// por plataforma.
// ===========================================================================

class WifiStrikeRadioP4 : public IWifiStrikeRadio {
public:
    bool isAvailable() const override {
        return false;
    }

    RadioResult init() override {
        return RadioResult::Unsupported;
    }

    RadioResult txFrame(const uint8_t* data, size_t len,
                        const TxFrameParams& params) override {
        (void)data;
        (void)len;
        (void)params;
        return RadioResult::Unsupported;
    }

    RadioResult setChannel(uint8_t channel) override {
        (void)channel;
        return RadioResult::Unsupported;
    }

    uint8_t getChannel() const override {
        return 0;
    }

    RadioResult getMac(uint8_t* out) const override {
        if (out) {
            for (int i = 0; i < wifi::WIFI_MAC_LEN; ++i) out[i] = 0;
        }
        return RadioResult::Unsupported;
    }

    RadioResult getRandom(uint8_t* out, size_t len) override {
        (void)len;
        if (out) out[0] = 0;
        return RadioResult::Unsupported;
    }

    bool isStaConnected() const override {
        return false;
    }

    StrikeRadioState getState() const override {
        return StrikeRadioState::Unavailable;
    }

    void shutdown() override {
        // Nada que apagar: el P4 no controla ningun controlador Wi-Fi.
    }

    const char* getLastError() const override {
        return "ESP32-P4 sin radio Wi-Fi integrada (requiere companion C6)";
    }
};

static WifiStrikeRadioP4 s_strikeRadioP4;

void initWifiStrikeP4() {
    wifi::registerStrikeRadio(&s_strikeRadioP4);
    cbdos::system::log(cbdos::system::LogLevel::Warn, TAG_STRIKE_P4,
                       "Wi-Fi Strike no disponible en este target (stub registrado)");
}

} // namespace bsp
} // namespace cbdos