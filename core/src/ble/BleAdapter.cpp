#include "cbdos/ble.hpp"

#include <utility>

namespace cbdos {
namespace ble {

static IBleAdapter* s_adapter = nullptr;
static BleAdapterStateCallback s_stateCallback = nullptr;

void registerAdapter(IBleAdapter* adapter) {
    s_adapter = adapter;
}

IBleAdapter* getAdapter() {
    return s_adapter;
}

BleAdapterState getState() {
    if (!s_adapter) return BleAdapterState::Unavailable;
    return s_adapter->getState();
}

bool isAvailable() {
    return s_adapter != nullptr && s_adapter->isAvailable();
}

void setStateCallback(BleAdapterStateCallback cb) {
    s_stateCallback = std::move(cb);
}

// Invocado por el HAL cuando su estado interno cambia, para que el core y la
// UI se enteren sin consultar al hardware.
void notifyStateChanged(BleAdapterState state) {
    if (s_stateCallback) s_stateCallback(state);
}

} // namespace ble
} // namespace cbdos