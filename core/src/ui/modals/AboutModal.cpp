#include "AboutModal.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* AboutModal::s_modalMask = nullptr;

void AboutModal::close_btn_cb(lv_event_t* e) {
    (void)e;
    hide();
}

void AboutModal::hide() {
    if (s_modalMask && lv_obj_is_valid(s_modalMask)) {
        lv_obj_delete_async(s_modalMask);
        s_modalMask = nullptr;
    }
}

void AboutModal::show(lv_obj_t* parent) {
    (void)parent;
    hide();

    auto caps = cbdos::display::getCapabilities();

    s_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_modalMask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_modalMask, 0, 0);
    lv_obj_set_style_bg_color(s_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_modalMask, 0, 0);
    lv_obj_set_style_pad_all(s_modalMask, 10, 0);

    // Tarjeta Modal con Glassmorphism
    lv_obj_t* card = lv_obj_create(s_modalMask);
    lv_obj_set_width(card, (caps.width >= 480) ? 430 : 304);
    lv_obj_set_height(card, (caps.width >= 480) ? 620 : 440);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_center(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);

    // 1. Icono Insignia
    lv_obj_t* iconBox = lv_obj_create(card);
    lv_obj_set_size(iconBox, 54, 54);
    lv_obj_set_style_bg_color(iconBox, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(iconBox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(iconBox, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_border_width(iconBox, 2, 0);
    lv_obj_set_style_radius(iconBox, 27, 0);
    lv_obj_set_style_pad_all(iconBox, 0, 0);
    lv_obj_remove_flag(iconBox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblIcon = lv_label_create(iconBox);
    lv_label_set_text(lblIcon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(lblIcon, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
    lv_obj_center(lblIcon);

    // 2. Título y Versión
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "CyBerDeck OS");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    lv_obj_t* verLbl = lv_label_create(card);
    lv_label_set_text(verLbl, "Version 0.2.3-dev (Universal Core)");
    lv_obj_set_style_text_color(verLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(verLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_bottom(verLbl, 6, 0);

    // Helper para añadir filas de información
    auto addInfoBox = [](lv_obj_t* p, const char* title, const char* value, const char* highlight = nullptr) {
        lv_obj_t* box = lv_obj_create(p);
        lv_obj_set_width(box, lv_pct(100));
        lv_obj_set_height(box, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x131722), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x232936), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_radius(box, 8, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(box, 8, 0);
        lv_obj_set_style_pad_row(box, 2, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* t = lv_label_create(box);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);

        lv_obj_t* v = lv_label_create(box);
        lv_label_set_text(v, value);
        lv_obj_set_style_text_color(v, highlight ? lv_color_hex(0x38BDF8) : DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_12, 0);
        lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(v, lv_pct(100));
    };

    // 3. Cajas de Información
    addInfoBox(card, "Autor / Mantenedor", "kaber420");
    addInfoBox(card, "Repositorio Oficial", "https://github.com/kaber420/cbdos", "url");
    addInfoBox(card, "Sitio Web", "https://kaber420.github.io/cbdos/", "url");
    addInfoBox(card, "Licencia de Software", "GNU General Public License v3.0 (GPLv3)");
    addInfoBox(card, "Enlace de la Licencia", "https://www.gnu.org/licenses/gpl-3.0.html", "url");

    if (caps.width >= 480) {
        addInfoBox(card, "Hardware Target", "ESP32-P4 RISC-V Dual-Core @ 400MHz\n480x800 MIPI-DPI | 32MB Hexal-PSRAM");
    } else {
        addInfoBox(card, "Hardware Target", "ESP32-S3 Xtensa Dual-Core @ 240MHz\n320x480 QSPI | 8MB Octal-PSRAM");
    }

    // Botón Cerrar
    lv_obj_t* btnClose = lv_button_create(card);
    lv_obj_set_size(btnClose, lv_pct(100), 38);
    DefaultTheme::applyButton(btnClose, 10);
    lv_obj_set_style_bg_color(btnClose, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_margin_top(btnClose, 4, 0);

    lv_obj_t* lblC = lv_label_create(btnClose);
    lv_label_set_text(lblC, "Cerrar");
    lv_obj_set_style_text_color(lblC, lv_color_hex(0x0F172A), 0);
    lv_obj_center(lblC);

    lv_obj_add_event_cb(btnClose, close_btn_cb, LV_EVENT_CLICKED, nullptr);
}

} // namespace ui
} // namespace cbdos
