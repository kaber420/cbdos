#include "SecurityConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../modals/LockPinModal.hpp"
#include "cbdos/language.hpp"
#include "cbdos/system.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

using cbdos::lang::tr;
using cbdos::lang::StrId;
using cbdos::security::LockPolicy;
using cbdos::security::LockService;

SecurityConfigView::SecurityConfigView()
    : BaseView(cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_POLICY_TITLE)) {
}

void SecurityConfigView::policyCardClickedCb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    auto* self = static_cast<SecurityConfigView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    int policyIdx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (policyIdx < 0 || policyIdx > 2) return;

    self->applySelectedPolicy(static_cast<LockPolicy>(policyIdx));
}

void SecurityConfigView::applySelectedPolicy(LockPolicy newPolicy) {
    LockPolicy current = LockService::getInstance().getPolicy();
    if (current == newPolicy) return;

    // Si ya existe política de seguridad activa, solicitar PIN antes de permitir el cambio
    if (current != LockPolicy::Disabled) {
        LockPinModal::show(
            tr(StrId::STR_SEC_ENTER_PIN),
            [this, newPolicy]() {
                LockService::getInstance().setPolicy(newPolicy);
                refreshPolicySelection();
                UIManager::showToast(tr(StrId::STR_SEC_TOAST_POL_SAVED));
            },
            [this]() {
                refreshPolicySelection();
            }
        );
    } else {
        LockService::getInstance().setPolicy(newPolicy);
        refreshPolicySelection();
        UIManager::showToast(tr(StrId::STR_SEC_TOAST_POL_SAVED));
    }
}

void SecurityConfigView::changePinClickedCb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    auto* self = static_cast<SecurityConfigView*>(lv_event_get_user_data(e));
    if (!self) return;

    // Paso A: Validar identidad con el PIN actual
    LockPinModal::show(
        tr(StrId::STR_SEC_ENTER_PIN),
        [self]() {
            // Paso B: Capturar nuevo código PIN
            LockPinModal::showCapture(
                tr(StrId::STR_SEC_NEW_PIN),
                [self](const std::string& newPin) {
                    if (newPin.length() == 4) {
                        LockService::getInstance().forceSetPin(newPin);
                        self->refreshPinStatus();
                        UIManager::showToast(tr(StrId::STR_SEC_TOAST_PIN_SAVED));
                    }
                }
            );
        }
    );
}

void SecurityConfigView::lockNowClickedCb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    LockService::getInstance().lock();
    LockPinModal::show(nullptr, nullptr, nullptr, true);
}

void SecurityConfigView::refreshPolicySelection() {
    LockPolicy current = LockService::getInstance().getPolicy();

    for (int i = 0; i < 3; i++) {
        bool isSelected = (static_cast<int>(current) == i);
        if (m_policyCards[i] && lv_obj_is_valid(m_policyCards[i])) {
            if (isSelected) {
                lv_obj_set_style_border_color(m_policyCards[i], DefaultTheme::getPrimaryAccent(), 0);
                lv_obj_set_style_border_width(m_policyCards[i], 2, 0);
            } else {
                lv_obj_set_style_border_color(m_policyCards[i], lv_color_hex(0x2A2E3D), 0);
                lv_obj_set_style_border_width(m_policyCards[i], 1, 0);
            }
        }

        if (m_policyRadios[i] && lv_obj_is_valid(m_policyRadios[i])) {
            if (isSelected) {
                lv_obj_set_style_bg_color(m_policyRadios[i], DefaultTheme::getPrimaryAccent(), 0);
                lv_obj_set_style_border_color(m_policyRadios[i], DefaultTheme::getPrimaryAccent(), 0);
            } else {
                lv_obj_set_style_bg_color(m_policyRadios[i], lv_color_hex(0x1E2230), 0);
                lv_obj_set_style_border_color(m_policyRadios[i], lv_color_hex(0x475569), 0);
            }
        }
    }
}

void SecurityConfigView::refreshPinStatus() {
    if (!m_pinStatusLabel || !lv_obj_is_valid(m_pinStatusLabel)) return;

    if (LockService::getInstance().hasCustomPin()) {
        lv_label_set_text(m_pinStatusLabel, tr(StrId::STR_SEC_STATUS_CUSTOM_PIN));
        lv_obj_set_style_text_color(m_pinStatusLabel, lv_color_hex(0x10B981), 0); // Verde esmeralda
    } else {
        lv_label_set_text(m_pinStatusLabel, tr(StrId::STR_SEC_STATUS_DEFAULT_PIN));
        lv_obj_set_style_text_color(m_pinStatusLabel, lv_color_hex(0xF59E0B), 0); // Ámbar
    }
}

void SecurityConfigView::onShow() {
    BaseView::onShow();
    refreshPolicySelection();
    refreshPinStatus();
}

bool SecurityConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 24, 0);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // ─── SECCIÓN 1: Política de Seguridad del Dispositivo ───
    lv_obj_t* sec1Lbl = lv_label_create(m_container);
    lv_label_set_text(sec1Lbl, tr(StrId::STR_SEC_POLICY_DESC));
    lv_obj_set_style_text_color(sec1Lbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sec1Lbl, &lv_font_montserrat_14, 0);

    struct PolicyMeta {
        StrId titleId;
        StrId subId;
    };

    const PolicyMeta policies[3] = {
        { StrId::STR_SEC_POLICY_DISABLED,   StrId::STR_SEC_POLICY_DISABLED_SUB },
        { StrId::STR_SEC_POLICY_LOCKSCREEN, StrId::STR_SEC_POLICY_LOCKSCREEN_SUB },
        { StrId::STR_SEC_POLICY_SETTINGS,   StrId::STR_SEC_POLICY_SETTINGS_SUB }
    };

    for (int i = 0; i < 3; i++) {
        lv_obj_t* card = lv_button_create(m_container);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        DefaultTheme::applyButton(card, 14);
        lv_obj_set_user_data(card, (void*)(intptr_t)i);
        lv_obj_add_event_cb(card, policyCardClickedCb, LV_EVENT_CLICKED, this);

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(card, 12, 0);
        lv_obj_set_style_pad_column(card, 12, 0);

        // Indicador de Radio Button
        lv_obj_t* radioOuter = lv_obj_create(card);
        lv_obj_set_size(radioOuter, 22, 22);
        lv_obj_set_style_radius(radioOuter, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(radioOuter, 2, 0);
        lv_obj_set_style_border_color(radioOuter, lv_color_hex(0x475569), 0);
        lv_obj_remove_flag(radioOuter, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(radioOuter, LV_OBJ_FLAG_SCROLLABLE);

        m_policyRadios[i] = radioOuter;

        // Contenedor de Texto
        lv_obj_t* textCont = lv_obj_create(card);
        lv_obj_set_flex_grow(textCont, 1);
        lv_obj_set_style_bg_opa(textCont, 0, 0);
        lv_obj_set_style_border_width(textCont, 0, 0);
        lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(textCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(textCont, 0, 0);
        lv_obj_remove_flag(textCont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(textCont, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* titleLbl = lv_label_create(textCont);
        lv_label_set_text(titleLbl, tr(policies[i].titleId));
        lv_obj_set_style_text_color(titleLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);
        lv_obj_remove_flag(titleLbl, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* subLbl = lv_label_create(textCont);
        lv_label_set_text(subLbl, tr(policies[i].subId));
        lv_obj_set_style_text_color(subLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);
        lv_obj_remove_flag(subLbl, LV_OBJ_FLAG_CLICKABLE);

        m_policyCards[i] = card;
    }

    // ─── SECCIÓN 2: Credenciales de Acceso ───
    lv_obj_t* sec2Lbl = lv_label_create(m_container);
    lv_label_set_text(sec2Lbl, tr(StrId::STR_SEC_CREDENTIALS_TITLE));
    lv_obj_set_style_text_color(sec2Lbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sec2Lbl, &lv_font_montserrat_14, 0);

    lv_obj_t* cardCred = lv_obj_create(m_container);
    lv_obj_set_width(cardCred, lv_pct(100));
    lv_obj_set_height(cardCred, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardCred, 14);
    lv_obj_set_flex_flow(cardCred, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardCred, 14, 0);
    lv_obj_set_style_pad_row(cardCred, 10, 0);

    m_pinStatusLabel = lv_label_create(cardCred);
    lv_obj_set_style_text_font(m_pinStatusLabel, &lv_font_montserrat_14, 0);

    lv_obj_t* btnChangePin = lv_button_create(cardCred);
    lv_obj_set_width(btnChangePin, lv_pct(100));
    lv_obj_set_height(btnChangePin, 44);
    DefaultTheme::applyButton(btnChangePin, 10);
    lv_obj_add_event_cb(btnChangePin, changePinClickedCb, LV_EVENT_CLICKED, this);

    lv_obj_set_flex_flow(btnChangePin, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnChangePin, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btnChangePin, 8, 0);

    lv_obj_t* iconPin = lv_label_create(btnChangePin);
    lv_label_set_text(iconPin, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(iconPin, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_remove_flag(iconPin, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* txtPin = lv_label_create(btnChangePin);
    lv_label_set_text(txtPin, tr(StrId::STR_SEC_CHANGE_PIN));
    lv_obj_set_style_text_color(txtPin, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(txtPin, &lv_font_montserrat_14, 0);
    lv_obj_remove_flag(txtPin, LV_OBJ_FLAG_CLICKABLE);

    // ─── SECCIÓN 3: Acciones Inmediatas ───
    lv_obj_t* sec3Lbl = lv_label_create(m_container);
    lv_label_set_text(sec3Lbl, "ACCIONES INMEDIATAS");
    lv_obj_set_style_text_color(sec3Lbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sec3Lbl, &lv_font_montserrat_14, 0);

    lv_obj_t* btnLockNow = lv_button_create(m_container);
    lv_obj_set_width(btnLockNow, lv_pct(100));
    lv_obj_set_height(btnLockNow, 48);
    DefaultTheme::applyButton(btnLockNow, 12);
    lv_obj_set_style_border_color(btnLockNow, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_border_width(btnLockNow, 1, 0);
    lv_obj_add_event_cb(btnLockNow, lockNowClickedCb, LV_EVENT_CLICKED, this);

    lv_obj_set_flex_flow(btnLockNow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnLockNow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btnLockNow, 8, 0);

    lv_obj_t* iconLock = lv_label_create(btnLockNow);
    lv_label_set_text(iconLock, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(iconLock, lv_color_hex(0xEF4444), 0);
    lv_obj_remove_flag(iconLock, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* txtLock = lv_label_create(btnLockNow);
    lv_label_set_text(txtLock, tr(StrId::STR_SEC_BTN_LOCK_NOW));
    lv_obj_set_style_text_color(txtLock, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(txtLock, &lv_font_montserrat_14, 0);
    lv_obj_remove_flag(txtLock, LV_OBJ_FLAG_CLICKABLE);

    // Refrescar estados iniciales
    refreshPolicySelection();
    refreshPinStatus();

    return true;
}

} // namespace ui
} // namespace cbdos
