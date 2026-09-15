#include "TerminalCommandBar.hpp"
#include "../../themes/DefaultTheme.h"
#include "../UIManager.hpp"
#include <cstring>

namespace cbdos {
namespace ui {

struct QuickKey {
    const char* label;
    const char* bytes;
    size_t len;
};

static const QuickKey s_quickKeys[] = {
    {"ENTER", "\r\n", 2},
    {"^C", "\x03", 1},
    {"^Z", "\x1A", 1},
    {"SPACE", " ", 1},
    {"TAB", "\t", 1}
};

bool TerminalCommandBar::create(lv_obj_t* parent, SendCommandCallback onSendCmd, SendRawCallback onSendRaw) {
    if (!parent) return false;
    m_onSendCmd = onSendCmd;
    m_onSendRaw = onSendRaw;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_container, 4, 0);
    DefaultTheme::disableScroll(m_container);

    // Fila 1: Protocolo + Botones Rápidos
    lv_obj_t* quickRow = lv_obj_create(m_container);
    lv_obj_set_size(quickRow, LV_PCT(100), 32);
    lv_obj_set_style_bg_opa(quickRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(quickRow, 0, 0);
    lv_obj_set_style_pad_all(quickRow, 0, 0);
    lv_obj_set_flex_flow(quickRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(quickRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(quickRow, 4, 0);
    DefaultTheme::disableScroll(quickRow);

    m_ddLineEnding = lv_dropdown_create(quickRow);
    lv_dropdown_set_options(m_ddLineEnding, "CRLF\nLF\nCR\nNone");
    lv_dropdown_set_selected(m_ddLineEnding, 0);
    lv_obj_set_width(m_ddLineEnding, 74);
    lv_obj_set_style_text_font(m_ddLineEnding, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddLineEnding, lineEndingChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_btnEcho = lv_button_create(quickRow);
    DefaultTheme::applyButton(m_btnEcho, 6);
    lv_obj_set_size(m_btnEcho, 70, 28);
    lv_obj_set_style_bg_color(m_btnEcho, lv_palette_main(LV_PALETTE_GREY), 0);
    m_lblEcho = lv_label_create(m_btnEcho);
    lv_label_set_text(m_lblEcho, "Echo: OFF");
    lv_obj_set_style_text_font(m_lblEcho, &lv_font_montserrat_12, 0);
    lv_obj_center(m_lblEcho);
    lv_obj_add_event_cb(m_btnEcho, echoToggleBtnCb, LV_EVENT_CLICKED, this);

    for (const auto& qk : s_quickKeys) {
        lv_obj_t* btn = lv_button_create(quickRow);
        DefaultTheme::applyButton(btn, 6);
        lv_obj_set_size(btn, 48, 28);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, qk.label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        lv_obj_set_user_data(btn, (void*)&qk);
        lv_obj_add_event_cb(btn, quickKeyBtnCb, LV_EVENT_CLICKED, this);
    }

    // Fila 2: Input + Botón Enviar + Botón Teclado
    lv_obj_t* inputRow = lv_obj_create(m_container);
    lv_obj_set_size(inputRow, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(inputRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inputRow, 0, 0);
    lv_obj_set_style_pad_all(inputRow, 0, 0);
    lv_obj_set_flex_flow(inputRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inputRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(inputRow, 4, 0);
    DefaultTheme::disableScroll(inputRow);

    m_inputCmd = lv_textarea_create(inputRow);
    lv_obj_set_flex_grow(m_inputCmd, 1);
    lv_obj_set_height(m_inputCmd, 36);
    DefaultTheme::applyTextArea(m_inputCmd, 8);
    lv_textarea_set_one_line(m_inputCmd, true);
    lv_textarea_set_placeholder_text(m_inputCmd, "Comando...");
    lv_obj_set_style_text_font(m_inputCmd, &lv_font_montserrat_12, 0);

    m_btnSend = lv_button_create(inputRow);
    DefaultTheme::applyButton(m_btnSend, 8);
    lv_obj_set_size(m_btnSend, 50, 36);
    lv_obj_t* lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(lblSend, LV_SYMBOL_PLAY);
    lv_obj_center(lblSend);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);

    m_btnToggleKb = lv_button_create(inputRow);
    DefaultTheme::applyButton(m_btnToggleKb, 8);
    lv_obj_set_size(m_btnToggleKb, 42, 36);
    lv_obj_t* lblKb = lv_label_create(m_btnToggleKb);
    lv_label_set_text(lblKb, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lblKb);
    lv_obj_add_event_cb(m_btnToggleKb, kbToggleBtnCb, LV_EVENT_CLICKED, this);

    // Teclado Virtual
    UIManager::attachKeyboard(m_inputCmd, {
        .autoCloseOnSubmit = true,
        .onSubmit = [this](const char*) {
            this->executeCommand();
        }
    });

    return true;
}

void TerminalCommandBar::executeCommand() {
    if (!m_inputCmd) return;
    const char* txt = lv_textarea_get_text(m_inputCmd);
    if (txt && strlen(txt) > 0 && m_onSendCmd) {
        m_onSendCmd(txt, m_lineEnding, m_localEcho);
        lv_textarea_set_text(m_inputCmd, "");
    }
}

void TerminalCommandBar::lineEndingChangedCb(lv_event_t* e) {
    auto* self = static_cast<TerminalCommandBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddLineEnding) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddLineEnding);
    switch (sel) {
        case 0: self->m_lineEnding = cbdos::serial::LineEnding::CRLF; break;
        case 1: self->m_lineEnding = cbdos::serial::LineEnding::LF;   break;
        case 2: self->m_lineEnding = cbdos::serial::LineEnding::CR;   break;
        case 3: self->m_lineEnding = cbdos::serial::LineEnding::NONE; break;
        default: self->m_lineEnding = cbdos::serial::LineEnding::CRLF; break;
    }
}

void TerminalCommandBar::echoToggleBtnCb(lv_event_t* e) {
    auto* self = static_cast<TerminalCommandBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_btnEcho || !self->m_lblEcho) return;
    self->m_localEcho = !self->m_localEcho;
    if (self->m_localEcho) {
        lv_obj_set_style_bg_color(self->m_btnEcho, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text(self->m_lblEcho, "Echo: ON");
    } else {
        lv_obj_set_style_bg_color(self->m_btnEcho, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_label_set_text(self->m_lblEcho, "Echo: OFF");
    }
}

void TerminalCommandBar::sendBtnCb(lv_event_t* e) {
    auto* self = static_cast<TerminalCommandBar*>(lv_event_get_user_data(e));
    if (self) self->executeCommand();
}

void TerminalCommandBar::kbToggleBtnCb(lv_event_t* e) {
    auto* self = static_cast<TerminalCommandBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_inputCmd) return;
    if (UIManager::getActiveKeyboard() != nullptr) {
        UIManager::closeKeyboard();
    } else {
        UIManager::openKeyboard(self->m_inputCmd);
    }
}

void TerminalCommandBar::quickKeyBtnCb(lv_event_t* e) {
    auto* self = static_cast<TerminalCommandBar*>(lv_event_get_user_data(e));
    auto* target = (lv_obj_t*)lv_event_get_current_target(e);
    if (!self || !target) return;
    const auto* qk = static_cast<const QuickKey*>(lv_obj_get_user_data(target));
    if (self->m_onSendRaw && qk) {
        self->m_onSendRaw(qk->bytes, qk->len);
    }
}

} // namespace ui
} // namespace cbdos
