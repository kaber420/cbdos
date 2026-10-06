#include "HeaderBar.hpp"
#include "ThemeEngine.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/system.hpp"
#include "cbdos/network.hpp"
#include "cbdos/time.hpp"
#include "cbdos_build_profile.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace cbdos {
namespace ui {

HeaderBar::HeaderBar()
    : m_container(nullptr),
      m_labelTitle(nullptr),
      m_btnBack(nullptr),
      m_labelClock(nullptr),
      m_labelWifi(nullptr),
      m_btnRightAction(nullptr),
      m_labelRightAction(nullptr),
      m_timer(nullptr),
      m_staffPinTimer(nullptr),
      m_onClickCb(nullptr),
      m_onBackCb(nullptr),
      m_onRightActionCb(nullptr),
      m_onStaffPinCb(nullptr),
      m_lastUpdateMs(0) {
}

HeaderBar::~HeaderBar() {
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
    if (m_staffPinTimer) {
        lv_timer_delete(m_staffPinTimer);
        m_staffPinTimer = nullptr;
    }
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_delete(m_container);
        m_container = nullptr;
    }
}

bool HeaderBar::init(lv_obj_t* parent) {
    if (!parent) return false;

    const auto& palette = ThemeEngine::getInstance().getPalette();

    // 1. Contenedor Isla Flotante (44px de altura, redondeada y translúcida)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(94), 44);
    lv_obj_align(m_container, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Estilo Glassmorphism translúcido original
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x1B1E29), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_70, 0);
    lv_obj_set_style_border_color(m_container, lv_color_hex(0x3B4252), 0);
    lv_obj_set_style_border_width(m_container, 1, 0);
    lv_obj_set_style_radius(m_container, 14, 0);
    lv_obj_set_style_pad_hor(m_container, 14, 0);
    lv_obj_set_style_pad_ver(m_container, 0, 0);

    // Layout Flex horizontal
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 2. Izquierda: Contenedor Izquierdo (Título o Botón Volver)
    lv_obj_t* leftBox = lv_obj_create(m_container);
    lv_obj_remove_flag(leftBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(leftBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftBox, 0, 0);
    lv_obj_set_style_pad_all(leftBox, 0, 0);
    lv_obj_set_size(leftBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(leftBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftBox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_btnBack = lv_button_create(leftBox);
    lv_obj_set_size(m_btnBack, 84, 30);
    DefaultTheme::applyButton(m_btnBack, 10);
    lv_obj_t* lblBack = lv_label_create(m_btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT " Volver");
    lv_obj_set_style_text_color(lblBack, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblBack, &lv_font_montserrat_14, 0);
    lv_obj_center(lblBack);
    lv_obj_add_event_cb(m_btnBack, backBtnEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(m_btnBack, LV_OBJ_FLAG_HIDDEN);

    m_labelTitle = lv_label_create(leftBox);
    lv_label_set_text(m_labelTitle, "CBDos");
    lv_obj_set_style_text_color(m_labelTitle, lv_color_hex(palette.textPrimary), 0);
    lv_obj_set_style_text_font(m_labelTitle, &lv_font_montserrat_14, 0);

    // 3. Centro: Contenedor táctil central (Solo tocar aquí abre los Accesos Rápidos si KIOSK_LOCK está apagado)
    lv_obj_t* centerBox = lv_obj_create(m_container);
    lv_obj_remove_flag(centerBox, LV_OBJ_FLAG_SCROLLABLE);
#if !CBDOS_FEATURE_KIOSK_LOCK
    lv_obj_add_flag(centerBox, LV_OBJ_FLAG_CLICKABLE);
#endif
    lv_obj_set_style_bg_opa(centerBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(centerBox, 0, 0);
    lv_obj_set_style_pad_hor(centerBox, 24, 0);
    lv_obj_set_style_pad_ver(centerBox, 0, 0);
    lv_obj_set_size(centerBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(centerBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(centerBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_labelClock = lv_label_create(centerBox);
    lv_label_set_text(m_labelClock, "--:--");
    lv_obj_set_style_text_color(m_labelClock, lv_color_hex(palette.textPrimary), 0);
    lv_obj_set_style_text_font(m_labelClock, &lv_font_montserrat_16, 0);

#if !CBDOS_FEATURE_KIOSK_LOCK
    // Evento de click EXCLUSIVO en la zona central
    lv_obj_add_event_cb(centerBox, eventHandler, LV_EVENT_CLICKED, this);
#endif

    // 4. Derecha: Contenedor derecho (WiFi Status o Botón de Acción Personalizado)
    lv_obj_t* rightBox = lv_obj_create(m_container);
    lv_obj_remove_flag(rightBox, LV_OBJ_FLAG_SCROLLABLE);
#if CBDOS_FEATURE_STAFF_PIN
    lv_obj_add_flag(rightBox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rightBox, staffPinEventHandler, LV_EVENT_ALL, this);
#endif
    lv_obj_set_style_bg_opa(rightBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rightBox, 0, 0);
    lv_obj_set_style_pad_all(rightBox, 0, 0);
    lv_obj_set_size(rightBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(rightBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rightBox, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_btnRightAction = lv_button_create(rightBox);
    lv_obj_set_size(m_btnRightAction, 95, 30);
    DefaultTheme::applyButton(m_btnRightAction, 10);
    lv_obj_add_flag(m_btnRightAction, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(m_btnRightAction, rightActionEventHandler, LV_EVENT_CLICKED, this);

    m_labelRightAction = lv_label_create(m_btnRightAction);
    lv_label_set_text(m_labelRightAction, "");
    lv_obj_set_style_text_font(m_labelRightAction, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_labelRightAction, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_center(m_labelRightAction);

    m_labelWifi = lv_label_create(rightBox);
    lv_label_set_text(m_labelWifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(m_labelWifi, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_text_font(m_labelWifi, &lv_font_montserrat_16, 0);

    m_timer = lv_timer_create(timerCallback, 15000, this);
    update();
    return true;
}

void HeaderBar::timerCallback(lv_timer_t* t) {
    auto* self = static_cast<HeaderBar*>(lv_timer_get_user_data(t));
    if (self) {
        self->update();
    }
}

void HeaderBar::setTitle(const char* title) {
    if (m_labelTitle && lv_obj_is_valid(m_labelTitle) && title) {
        lv_label_set_text(m_labelTitle, title);
    }
}

void HeaderBar::showBackButton(bool show, ClickCallback onBack) {
    m_onBackCb = onBack;
    if (m_btnBack && lv_obj_is_valid(m_btnBack)) {
        if (show) {
            lv_obj_remove_flag(m_btnBack, LV_OBJ_FLAG_HIDDEN);
            if (m_labelTitle) lv_obj_add_flag(m_labelTitle, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_btnBack, LV_OBJ_FLAG_HIDDEN);
            if (m_labelTitle) lv_obj_remove_flag(m_labelTitle, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void HeaderBar::showWifi(bool show) {
    if (m_labelWifi && lv_obj_is_valid(m_labelWifi)) {
        if (show) {
            lv_obj_remove_flag(m_labelWifi, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_labelWifi, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void HeaderBar::setRightAction(const char* label, ClickCallback onAction) {
    m_onRightActionCb = onAction;
    if (m_btnRightAction && lv_obj_is_valid(m_btnRightAction) && label) {
        if (m_labelRightAction) lv_label_set_text(m_labelRightAction, label);
        lv_obj_remove_flag(m_btnRightAction, LV_OBJ_FLAG_HIDDEN);
    }
}

void HeaderBar::clearRightAction() {
    m_onRightActionCb = nullptr;
    if (m_btnRightAction && lv_obj_is_valid(m_btnRightAction)) {
        lv_obj_add_flag(m_btnRightAction, LV_OBJ_FLAG_HIDDEN);
    }
}

void HeaderBar::eventHandler(lv_event_t* e) {
    auto* self = static_cast<HeaderBar*>(lv_event_get_user_data(e));
    if (self && self->m_onClickCb) {
        self->m_onClickCb();
    }
}

void HeaderBar::backBtnEventHandler(lv_event_t* e) {
    auto* self = static_cast<HeaderBar*>(lv_event_get_user_data(e));
    if (self && self->m_onBackCb) {
        self->m_onBackCb();
    }
}

void HeaderBar::rightActionEventHandler(lv_event_t* e) {
    auto* self = static_cast<HeaderBar*>(lv_event_get_user_data(e));
    if (self && self->m_onRightActionCb) {
        self->m_onRightActionCb();
    }
}

void HeaderBar::setOnClickCallback(ClickCallback cb) {
    m_onClickCb = cb;
}

void HeaderBar::setOnStaffPinRequestCallback(ClickCallback cb) {
    m_onStaffPinCb = cb;
}

void HeaderBar::staffPinEventHandler(lv_event_t* e) {
    auto* self = static_cast<HeaderBar*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        if (self->m_staffPinTimer) {
            lv_timer_delete(self->m_staffPinTimer);
            self->m_staffPinTimer = nullptr;
        }
        self->m_staffPinTimer = lv_timer_create(staffPinTimerCallback, 3000, self);
        lv_timer_set_repeat_count(self->m_staffPinTimer, 1);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (self->m_staffPinTimer) {
            lv_timer_delete(self->m_staffPinTimer);
            self->m_staffPinTimer = nullptr;
        }
    }
}

void HeaderBar::staffPinTimerCallback(lv_timer_t* t) {
    auto* self = static_cast<HeaderBar*>(lv_timer_get_user_data(t));
    if (self) {
        self->m_staffPinTimer = nullptr;
        if (self->m_onStaffPinCb) {
            self->m_onStaffPinCb();
        }
    }
}

void HeaderBar::update() {
    if (!m_container || !lv_obj_is_valid(m_container)) return;

    // Actualización y sincronización automática de tiempo (si NTP está activado)
    cbdos::time::update();

    // 1. Reloj NTP Real (HH:MM) - Actualizado cada 15 segundos
    std::time_t rawtime = cbdos::time::getEpoch();
    std::tm* timeinfo = std::localtime(&rawtime);

    char clockBuf[16];
    if (timeinfo && timeinfo->tm_year > (2020 - 1900)) {
        snprintf(clockBuf, sizeof(clockBuf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    } else {
        snprintf(clockBuf, sizeof(clockBuf), "--:--");
    }

    if (m_labelClock && lv_obj_is_valid(m_labelClock)) {
        const char* currentText = lv_label_get_text(m_labelClock);
        if (!currentText || strcmp(currentText, clockBuf) != 0) {
            lv_label_set_text(m_labelClock, clockBuf);
        }
    }

    // 2. Actualizar WiFi
    if (m_labelWifi && lv_obj_is_valid(m_labelWifi)) {
        bool connected = cbdos::network::isConnected();
        lv_label_set_text(m_labelWifi, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(m_labelWifi, lv_color_hex(connected ? 0x10B981 : 0x64748B), 0);
    }
}

void HeaderBar::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;

    if (m_labelTitle && lv_obj_is_valid(m_labelTitle)) {
        lv_obj_set_style_text_color(m_labelTitle, lv_color_hex(palette.textPrimary), 0);
    }
    if (m_labelClock && lv_obj_is_valid(m_labelClock)) {
        lv_obj_set_style_text_color(m_labelClock, lv_color_hex(palette.textPrimary), 0);
    }
}

} // namespace ui
} // namespace cbdos
