#include "LockPinModal.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include "cbdos/security.hpp"
#include "cbdos/language.hpp"
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

static const char* TAG = "LockPinModal";

lv_obj_t* LockPinModal::s_mask = nullptr;
lv_obj_t* LockPinModal::s_card = nullptr;
lv_obj_t* LockPinModal::s_dotsContainer = nullptr;
lv_obj_t* LockPinModal::s_dots[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* LockPinModal::s_errorLabel = nullptr;
lv_obj_t* LockPinModal::s_btnMatrix = nullptr;
std::string LockPinModal::s_enteredPin = "";
LockPinModal::SuccessCallback LockPinModal::s_onSuccessCb = nullptr;
LockPinModal::CancelCallback LockPinModal::s_onCancelCb = nullptr;
LockPinModal::PinEnteredCallback LockPinModal::s_onCaptureCb = nullptr;
bool LockPinModal::s_fullScreenMode = false;

static const char* const s_btnmMap[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    LV_SYMBOL_CLOSE, "0", LV_SYMBOL_BACKSPACE, ""
};

bool LockPinModal::isOpen() {
    return s_mask != nullptr && lv_obj_is_valid(s_mask);
}

void LockPinModal::hide() {
    if (s_mask && lv_obj_is_valid(s_mask)) {
        lv_obj_delete_async(s_mask);
        s_mask = nullptr;
        s_card = nullptr;
        s_dotsContainer = nullptr;
        for (int i = 0; i < 4; i++) {
            s_dots[i] = nullptr;
        }
        s_errorLabel = nullptr;
        s_btnMatrix = nullptr;
    }
    s_enteredPin.clear();
    s_onSuccessCb = nullptr;
    s_onCancelCb = nullptr;
    s_onCaptureCb = nullptr;
    s_fullScreenMode = false;
}

void LockPinModal::resetInput() {
    s_enteredPin.clear();
    updateDots();
}

void LockPinModal::updateDots() {
    size_t count = s_enteredPin.length();
    for (int i = 0; i < 4; i++) {
        if (!s_dots[i] || !lv_obj_is_valid(s_dots[i])) continue;
        if (i < static_cast<int>(count)) {
            lv_obj_set_style_bg_color(s_dots[i], DefaultTheme::getPrimaryAccent(), 0);
            lv_obj_set_style_border_color(s_dots[i], DefaultTheme::getPrimaryAccent(), 0);
        } else {
            lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(0x2E3444), 0);
            lv_obj_set_style_border_color(s_dots[i], lv_color_hex(0x475569), 0);
        }
    }
}

void LockPinModal::checkPin() {
    if (s_onCaptureCb) {
        auto captureCb = s_onCaptureCb;
        std::string pin = s_enteredPin;
        hide();
        if (captureCb) {
            captureCb(pin);
        }
        return;
    }

    bool ok = cbdos::security::LockService::getInstance().verifyPin(s_enteredPin);
    if (ok) {
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Autenticacion con PIN exitosa");
        auto successCb = s_onSuccessCb;
        hide();
        UIManager::showToast(cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_ACCESS_GRANTED));
        if (successCb) {
            successCb();
        }
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "PIN incorrecto");
        if (s_errorLabel && lv_obj_is_valid(s_errorLabel)) {
            lv_label_set_text(s_errorLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_PIN_INCORRECT));
            lv_obj_remove_flag(s_errorLabel, LV_OBJ_FLAG_HIDDEN);
        }
        resetInput();
    }
}

void LockPinModal::btnMatrixEventCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t btnId = lv_btnmatrix_get_selected_btn(obj);
        const char* txt = lv_btnmatrix_get_btn_text(obj, btnId);
        if (!txt) return;

        if (strcmp(txt, LV_SYMBOL_CLOSE) == 0) {
            // Si está en modo lockscreen completo y bloqueado, no se puede simplemente cerrar sin PIN
            if (s_fullScreenMode && cbdos::security::LockService::getInstance().isLocked()) {
                resetInput();
                return;
            }
            auto cancelCb = s_onCancelCb;
            hide();
            if (cancelCb) {
                cancelCb();
            }
            return;
        } else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
            if (!s_enteredPin.empty()) {
                s_enteredPin.pop_back();
                updateDots();
                if (s_errorLabel && lv_obj_is_valid(s_errorLabel)) {
                    lv_obj_add_flag(s_errorLabel, LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else if (txt[0] >= '0' && txt[0] <= '9' && txt[1] == '\0') {
            if (s_enteredPin.length() < 4) {
                s_enteredPin.push_back(txt[0]);
                updateDots();
                if (s_errorLabel && lv_obj_is_valid(s_errorLabel)) {
                    lv_obj_add_flag(s_errorLabel, LV_OBJ_FLAG_HIDDEN);
                }
                if (s_enteredPin.length() == 4) {
                    checkPin();
                }
            }
        }
    }
}

void LockPinModal::showCapture(const char* customTitle,
                               PinEnteredCallback onPinEntered,
                               CancelCallback onCancel) {
    show(customTitle, nullptr, onCancel, false);
    s_onCaptureCb = onPinEntered;
}

void LockPinModal::show(const char* customTitle,
                        SuccessCallback onSuccess,
                        CancelCallback onCancel,
                        bool fullScreen) {
    hide();
    s_onSuccessCb = onSuccess;
    s_onCancelCb = onCancel;
    s_onCaptureCb = nullptr;
    s_fullScreenMode = fullScreen;
    s_enteredPin.clear();

    auto caps = cbdos::display::getCapabilities();

    s_mask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_mask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_mask, 0, 0);
    lv_obj_set_style_bg_color(s_mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_mask, fullScreen ? LV_OPA_90 : LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_mask, 0, 0);
    lv_obj_set_style_pad_all(s_mask, 0, 0);
    lv_obj_remove_flag(s_mask, LV_OBJ_FLAG_SCROLLABLE);

    // Tarjeta Modal Glassmorphism
    s_card = lv_obj_create(s_mask);
    int32_t cardWidth = (caps.width >= 480) ? 340 : 280;
    if (fullScreen) {
        lv_obj_set_width(s_card, (caps.width >= 480) ? 380 : 300);
    } else {
        lv_obj_set_width(s_card, cardWidth);
    }
    lv_obj_set_height(s_card, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(s_card, 16);
    lv_obj_center(s_card);
    lv_obj_set_flex_flow(s_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_card, 16, 0);
    lv_obj_set_style_pad_row(s_card, 8, 0);
    lv_obj_remove_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

    // Titulo
    lv_obj_t* title = lv_label_create(s_card);
    if (customTitle && customTitle[0] != '\0') {
        lv_label_set_text(title, customTitle);
    } else if (fullScreen) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %s", LV_SYMBOL_WARNING, cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_LOCKED));
        lv_label_set_text(title, buf);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %s", LV_SYMBOL_SETTINGS, cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_ENTER_PIN));
        lv_label_set_text(title, buf);
    }
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    // Subtítulo explicativo
    lv_obj_t* sub = lv_label_create(s_card);
    lv_label_set_text(sub, cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_POLICY_DESC));
    lv_obj_set_style_text_color(sub, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);

    // Contenedor de puntos (4 indicadores)
    s_dotsContainer = lv_obj_create(s_card);
    lv_obj_set_size(s_dotsContainer, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_bg_opa(s_dotsContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dotsContainer, 0, 0);
    lv_obj_set_style_pad_all(s_dotsContainer, 0, 0);
    lv_obj_set_flex_flow(s_dotsContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_dotsContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_dotsContainer, 14, 0);
    lv_obj_remove_flag(s_dotsContainer, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 4; i++) {
        s_dots[i] = lv_obj_create(s_dotsContainer);
        lv_obj_set_size(s_dots[i], 16, 16);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(0x2E3444), 0);
        lv_obj_set_style_border_color(s_dots[i], lv_color_hex(0x475569), 0);
        lv_obj_set_style_border_width(s_dots[i], 2, 0);
        lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // Label de error (oculto por defecto)
    s_errorLabel = lv_label_create(s_card);
    lv_label_set_text(s_errorLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_PIN_INCORRECT));
    lv_obj_set_style_text_color(s_errorLabel, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_font(s_errorLabel, &lv_font_montserrat_12, 0);
    lv_obj_add_flag(s_errorLabel, LV_OBJ_FLAG_HIDDEN);

    // Teclado Numérico con lv_btnmatrix
    s_btnMatrix = lv_btnmatrix_create(s_card);
    lv_btnmatrix_set_map(s_btnMatrix, s_btnmMap);
    lv_obj_set_size(s_btnMatrix, lv_pct(100), (caps.height >= 800) ? 230 : 180);
    lv_obj_set_style_bg_opa(s_btnMatrix, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_btnMatrix, 0, 0);
    lv_obj_set_style_pad_all(s_btnMatrix, 2, 0);
    lv_obj_set_style_pad_row(s_btnMatrix, 6, 0);
    lv_obj_set_style_pad_column(s_btnMatrix, 6, 0);

    // Estilos de los botones del keypad
    lv_obj_set_style_radius(s_btnMatrix, 10, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_btnMatrix, lv_color_hex(0x1E2230), LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_btnMatrix, lv_color_hex(0x3B4252), LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_btnMatrix, 1, LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_btnMatrix, DefaultTheme::getTextColor(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_btnMatrix, &lv_font_montserrat_16, LV_PART_ITEMS);

    // Estado presionado
    lv_obj_set_style_bg_color(s_btnMatrix, DefaultTheme::getPrimaryAccent(), (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);
    lv_obj_set_style_text_color(s_btnMatrix, lv_color_hex(0x0F172A), (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);

    lv_obj_add_event_cb(s_btnMatrix, btnMatrixEventCb, LV_EVENT_VALUE_CHANGED, nullptr);

    updateDots();
}

} // namespace ui
} // namespace cbdos
