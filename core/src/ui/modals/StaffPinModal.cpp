#include "StaffPinModal.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

static const char* TAG = "StaffPinModal";
static const char* EXPECTED_PIN = "1234";

lv_obj_t* StaffPinModal::s_mask = nullptr;
lv_obj_t* StaffPinModal::s_card = nullptr;
lv_obj_t* StaffPinModal::s_dotsContainer = nullptr;
lv_obj_t* StaffPinModal::s_dots[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* StaffPinModal::s_errorLabel = nullptr;
lv_obj_t* StaffPinModal::s_btnMatrix = nullptr;
std::string StaffPinModal::s_enteredPin = "";
StaffPinModal::SuccessCallback StaffPinModal::s_onSuccessCb = nullptr;

static const char* const s_btnmMap[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    LV_SYMBOL_CLOSE, "0", LV_SYMBOL_BACKSPACE, ""
};

bool StaffPinModal::isOpen() {
    return s_mask != nullptr && lv_obj_is_valid(s_mask);
}

void StaffPinModal::hide() {
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
}

void StaffPinModal::resetInput() {
    s_enteredPin.clear();
    updateDots();
}

void StaffPinModal::updateDots() {
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

void StaffPinModal::checkPin() {
    if (s_enteredPin == EXPECTED_PIN) {
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "PIN de supervisor autenticado con éxito");
        auto successCb = s_onSuccessCb;
        hide();
        UIManager::showToast("Acceso concedido");
        if (successCb) {
            successCb();
        }
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "PIN de supervisor incorrecto");
        if (s_errorLabel && lv_obj_is_valid(s_errorLabel)) {
            lv_label_set_text(s_errorLabel, "PIN Incorrecto");
            lv_obj_remove_flag(s_errorLabel, LV_OBJ_FLAG_HIDDEN);
        }
        resetInput();
    }
}

void StaffPinModal::btnMatrixEventCb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t btnId = lv_btnmatrix_get_selected_btn(obj);
        const char* txt = lv_btnmatrix_get_btn_text(obj, btnId);
        if (!txt) return;

        if (strcmp(txt, LV_SYMBOL_CLOSE) == 0) {
            hide();
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

void StaffPinModal::show(SuccessCallback onSuccess) {
    hide();
    s_onSuccessCb = onSuccess;
    s_enteredPin.clear();

    auto caps = cbdos::display::getCapabilities();

    s_mask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_mask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_mask, 0, 0);
    lv_obj_set_style_bg_color(s_mask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_mask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_mask, 0, 0);
    lv_obj_set_style_pad_all(s_mask, 0, 0);
    lv_obj_remove_flag(s_mask, LV_OBJ_FLAG_SCROLLABLE);

    // Tarjeta Modal Glassmorphism
    s_card = lv_obj_create(s_mask);
    int32_t cardWidth = (caps.width >= 480) ? 340 : 280;
    lv_obj_set_width(s_card, cardWidth);
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
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " PIN de Supervisor");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    // Subtítulo explicativo
    lv_obj_t* sub = lv_label_create(s_card);
    lv_label_set_text(sub, "Acceso restringido para ajustes");
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
    lv_label_set_text(s_errorLabel, "PIN Incorrecto");
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
