#include "PowerManager.hpp"
#include "cbdos/system.hpp"
#include "cbdos/display.hpp"
#include "cbdos/config_manager.hpp"
#include "cbdos/security.hpp"
#include "../ui/modals/LockPinModal.hpp"
#include "cbdos/log.hpp"

namespace cbdos {
namespace system {

static const char* TAG = "PowerManager";

PowerManager::PowerManager()
    : m_state(PowerState::Active),
      m_idleTimeoutSec(60),
      m_lastActivityMs(0),
      m_savedBrightness(70),
      m_initialized(false) {
}

void PowerManager::init() {
    if (m_initialized) return;

    m_idleTimeoutSec = ConfigManager::getInstance().getIdleTimeoutSec();
    m_savedBrightness = ConfigManager::getInstance().getBrightness();
    m_lastActivityMs = cbdos::system::getTimeMs();
    m_state = PowerState::Active;

    // Escuchar eventos táctiles de LVGL para monitoreo de inactividad
    lv_display_t* disp = lv_display_get_default();
    if (disp) {
        lv_obj_t* scr = lv_display_get_screen_active(disp);
        if (scr) {
            lv_obj_add_event_cb(scr, touchInputEventCb, LV_EVENT_PRESSED, this);
            lv_obj_add_event_cb(scr, touchInputEventCb, LV_EVENT_KEY, this);
        }
    }

    m_initialized = true;
    CBD_LOG_I(TAG, "PowerManager inicializado (Auto-Standby: %u s)", (unsigned int)m_idleTimeoutSec);
}

void PowerManager::touchInputEventCb(lv_event_t* e) {
    auto* self = static_cast<PowerManager*>(lv_event_get_user_data(e));
    if (self) {
        self->notifyActivity();
    }
}

void PowerManager::notifyActivity() {
    uint32_t now = cbdos::system::getTimeMs();
    m_lastActivityMs = now;

    if (m_state != PowerState::Active) {
        wakeUp();
    }
}

void PowerManager::setIdleTimeoutSec(uint32_t sec) {
    m_idleTimeoutSec = sec;
    ConfigManager::getInstance().setIdleTimeoutSec(sec);
    notifyActivity();
    CBD_LOG_I(TAG, "Tiempo limite de inactividad ajustado a: %u s", (unsigned int)sec);
}

void PowerManager::update() {
    if (!m_initialized || m_idleTimeoutSec == 0) return;

    uint32_t now = cbdos::system::getTimeMs();
    uint32_t idleMs = now - m_lastActivityMs;
    uint32_t timeoutMs = m_idleTimeoutSec * 1000;

    // Etapa 1: Dimmed (50% del tiempo de inactividad)
    uint32_t dimTimeoutMs = timeoutMs / 2;

    if (m_state == PowerState::Active && idleMs >= dimTimeoutMs && idleMs < timeoutMs) {
        m_state = PowerState::Dimmed;
        uint8_t currentBright = ConfigManager::getInstance().getBrightness();
        if (currentBright > 20) {
            cbdos::display::setBrightness(20);
        }
        CBD_LOG_I(TAG, "Sistema atenuado por inactividad (Brillo 20%%)");
    }
    // Etapa 2: Auto-Apagado de Pantalla (100% del tiempo alcanzado)
    else if (m_state != PowerState::ScreenOff && m_state != PowerState::LightSleep && m_state != PowerState::DeepSleep && idleMs >= timeoutMs) {
        turnOffScreen();
    }
}

void PowerManager::turnOffScreen() {
    m_state = PowerState::ScreenOff;
    cbdos::display::setBrightness(0);
    CBD_LOG_I(TAG, "Pantalla apagada. Tareas de fondo activas.");
}

void PowerManager::enterLightSleep() {
    m_state = PowerState::LightSleep;
    cbdos::display::setBrightness(0);
    CBD_LOG_I(TAG, "Entrando a Suspension (Light Sleep)...");
    cbdos::system::sleepMs(100);
}

void PowerManager::enterDeepSleep() {
    m_state = PowerState::DeepSleep;
    cbdos::display::setBrightness(0);
    CBD_LOG_I(TAG, "Entrando a Suspension Profunda (Deep Sleep - LP Core)...");
    cbdos::system::sleepMs(200);
}

void PowerManager::wakeUp() {
    bool wasAsleep = (m_state == PowerState::ScreenOff || m_state == PowerState::LightSleep);
    m_state = PowerState::Active;
    uint8_t targetBright = ConfigManager::getInstance().getBrightness();
    if (targetBright == 0) targetBright = 70;
    cbdos::display::setBrightness(targetBright);
    CBD_LOG_I(TAG, "Sistema reactivado a brillo normal (%u%%)", (unsigned int)targetBright);

    if (wasAsleep && cbdos::security::LockService::getInstance().getPolicy() == cbdos::security::LockPolicy::Lockscreen) {
        cbdos::security::LockService::getInstance().lock();
        if (!cbdos::ui::LockPinModal::isOpen()) {
            cbdos::ui::LockPinModal::show(nullptr, nullptr, nullptr, true);
        }
    }
}

void PowerManager::restart() {
    CBD_LOG_I(TAG, "Reiniciando sistema...");
    cbdos::system::sleepMs(100);
    cbdos::system::restart();
}

} // namespace system
} // namespace cbdos
