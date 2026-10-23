#include "SshControlBar.hpp"
#include "../../themes/DefaultTheme.h"

namespace cbdos {
namespace ui {

bool SshControlBar::create(lv_obj_t* parent,
                           ActionCallback onOpenModal,
                           ConnectToggleCallback onToggleConnect,
                           ActionCallback onHold,
                           ActionCallback onClear,
                           ActionCallback onSave) {
    if (!parent) return false;
    m_onOpenModal = onOpenModal;
    m_onToggleConnect = onToggleConnect;
    m_onHold = onHold;
    m_onClear = onClear;
    m_onSave = onSave;

    m_toolbar = lv_obj_create(parent);
    lv_obj_set_size(m_toolbar, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(m_toolbar, 8);
    lv_obj_set_style_pad_all(m_toolbar, 4, 0);
    lv_obj_set_flex_flow(m_toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(m_toolbar);

    // Etiqueta de Host (con marquesina circular para mantener 1 renglón estricto)
    m_lblHost = lv_label_create(m_toolbar);
    lv_label_set_text(m_lblHost, "SSH: Desconectado");
    lv_obj_set_style_text_font(m_lblHost, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(m_lblHost, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_flex_grow(m_lblHost, 1);
    lv_obj_set_style_max_height(m_toolbar, 40, 0);

    // Botón Dinámico Host / Salir
    m_btnConfig = lv_button_create(m_toolbar);
    DefaultTheme::applyButton(m_btnConfig, 6);
    lv_obj_set_size(m_btnConfig, 70, 30);
    lv_obj_set_style_bg_color(m_btnConfig, lv_palette_main(LV_PALETTE_TEAL), 0);
    m_lblConfig = lv_label_create(m_btnConfig);
    lv_label_set_text(m_lblConfig, LV_SYMBOL_SETTINGS " Host");
    lv_obj_set_style_text_font(m_lblConfig, &lv_font_montserrat_12, 0);
    lv_obj_center(m_lblConfig);
    lv_obj_add_event_cb(m_btnConfig, hostConfigBtnCb, LV_EVENT_CLICKED, this);

    // Botón Hold (solo icono táctil)
    m_btnHold = lv_button_create(m_toolbar);
    DefaultTheme::applyButton(m_btnHold, 6);
    lv_obj_set_size(m_btnHold, 34, 30);
    lv_obj_set_style_bg_color(m_btnHold, lv_palette_main(LV_PALETTE_GREY), 0);
    m_lblHold = lv_label_create(m_btnHold);
    lv_label_set_text(m_lblHold, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(m_lblHold, &lv_font_montserrat_12, 0);
    lv_obj_center(m_lblHold);
    lv_obj_add_event_cb(m_btnHold, holdBtnCb, LV_EVENT_CLICKED, this);

    // Botón Limpiar
    m_btnClear = lv_button_create(m_toolbar);
    DefaultTheme::applyButton(m_btnClear, 6);
    lv_obj_set_size(m_btnClear, 32, 30);
    lv_obj_t* lblClear = lv_label_create(m_btnClear);
    lv_label_set_text(lblClear, LV_SYMBOL_TRASH);
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(m_btnClear, clearBtnCb, LV_EVENT_CLICKED, this);

    // Botón Guardar en SD
    m_btnSave = lv_button_create(m_toolbar);
    DefaultTheme::applyButton(m_btnSave, 6);
    lv_obj_set_size(m_btnSave, 32, 30);
    lv_obj_t* lblSave = lv_label_create(m_btnSave);
    lv_label_set_text(lblSave, LV_SYMBOL_SD_CARD);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(m_btnSave, saveBtnCb, LV_EVENT_CLICKED, this);

    return true;
}

void SshControlBar::show(bool visible) {
    if (!m_toolbar) return;
    if (visible) lv_obj_remove_flag(m_toolbar, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(m_toolbar, LV_OBJ_FLAG_HIDDEN);
}

void SshControlBar::setConnected(bool connected, const std::string& hostInfo) {
    m_isConnected = connected;
    if (m_btnConfig && m_lblConfig) {
        if (connected) {
            lv_obj_set_style_bg_color(m_btnConfig, lv_palette_main(LV_PALETTE_RED), 0);
            lv_label_set_text(m_lblConfig, LV_SYMBOL_STOP " Salir");
        } else {
            lv_obj_set_style_bg_color(m_btnConfig, lv_palette_main(LV_PALETTE_TEAL), 0);
            lv_label_set_text(m_lblConfig, LV_SYMBOL_SETTINGS " Host");
        }
    }
    if (m_lblHost) {
        if (connected && !hostInfo.empty()) {
            lv_label_set_text_fmt(m_lblHost, "SSH: %s", hostInfo.c_str());
        } else {
            lv_label_set_text(m_lblHost, "SSH: Desconectado");
        }
    }
}

void SshControlBar::setHoldActive(bool active) {
    m_isHoldActive = active;
    if (m_btnHold && m_lblHold) {
        if (active) {
            lv_obj_set_style_bg_color(m_btnHold, lv_palette_main(LV_PALETTE_AMBER), 0);
            lv_label_set_text(m_lblHold, LV_SYMBOL_PLAY);
        } else {
            lv_obj_set_style_bg_color(m_btnHold, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_label_set_text(m_lblHold, LV_SYMBOL_PAUSE);
        }
    }
}

void SshControlBar::hostConfigBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshControlBar*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->m_isConnected) {
        if (self->m_onToggleConnect) self->m_onToggleConnect(false);
    } else {
        if (self->m_onOpenModal) self->m_onOpenModal();
    }
}

void SshControlBar::holdBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onHold) self->m_onHold();
}

void SshControlBar::clearBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onClear) self->m_onClear();
}

void SshControlBar::saveBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onSave) self->m_onSave();
}

} // namespace ui
} // namespace cbdos
