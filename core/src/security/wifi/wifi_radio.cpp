#include "cbdos/wifi_radio.hpp"

#include <utility>

// Fachada del dominio para el HAL de Wi-Fi Strike.
//
// Mismo patron que BleAdapter.cpp: un unico puntero global sin transferencia
// de propiedad y una variable de callback. El core nunca consulta al
// controlador directamente, de modo que un target sin radio Wi-Fi (ESP32-P4 con
// host C6) se resuelve inyectando un stub cuyo isAvailable() devuelve false:
// getStrikeRadio() nunca es nullptr en un BSP completo, pero
// isStrikeRadioAvailable() si lo es.

namespace cbdos {
namespace wifi {

static IWifiStrikeRadio* s_strikeRadio = nullptr;
static StrikeRadioStateCallback s_stateCallback = nullptr;

void registerStrikeRadio(IWifiStrikeRadio* radio) {
    s_strikeRadio = radio;
}

IWifiStrikeRadio* getStrikeRadio() {
    return s_strikeRadio;
}

bool isStrikeRadioAvailable() {
    return s_strikeRadio != nullptr && s_strikeRadio->isAvailable();
}

StrikeRadioState getStrikeRadioState() {
    if (!s_strikeRadio) return StrikeRadioState::Unavailable;
    return s_strikeRadio->getState();
}

void setStrikeRadioStateCallback(StrikeRadioStateCallback cb) {
    s_stateCallback = std::move(cb);
}

// Invocado por el HAL desde sus propios eventos para que el core y la UI se
// enteren del cambio de estado sin sondear el hardware.
void notifyStrikeRadioState(StrikeRadioState state) {
    if (s_stateCallback) s_stateCallback(state);
}

} // namespace wifi
} // namespace cbdos