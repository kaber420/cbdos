#include "DefaultTheme.h"

void DefaultTheme::disableScroll(lv_obj_t* obj) {
    if(!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

void DefaultTheme::applyFlatBg(lv_obj_t* obj) {
    if(!obj) return;
    lv_obj_set_style_bg_color(obj, getBgColor(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    // Herencia: todo label/hijo sin color propio queda legible sobre fondo oscuro
    lv_obj_set_style_text_color(obj, getTextColor(), 0);
    disableScroll(obj);
}

void DefaultTheme::applyRaisedCard(lv_obj_t* obj, int32_t radius) {
    if(!obj) return;
    disableScroll(obj);
    
    // Fondo translúcido con efecto acrílico (Glassmorphism)
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1B1E29), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    
    // Borde nítido para resaltar los bordes sobre el wallpaper
    lv_obj_set_style_border_color(obj, lv_color_hex(0x3B4252), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);

    // Texto heredable: evita negro-sobre-oscuro en labels olvidados
    lv_obj_set_style_text_color(obj, getTextColor(), 0);
}

void DefaultTheme::applySunkenCard(lv_obj_t* obj, int32_t radius) {
    if(!obj) return;
    disableScroll(obj);
    
    // Tono hendido translúcido
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x11131A), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_60, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    
    // Bisel interno nítido
    lv_obj_set_style_border_color(obj, lv_color_hex(0x2E3444), 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);

    // Texto heredable
    lv_obj_set_style_text_color(obj, getTextColor(), 0);

    lv_obj_set_style_bg_color(obj, lv_color_hex(0x11131A), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_80, LV_STATE_PRESSED);
}

void DefaultTheme::applyButton(lv_obj_t* obj, int32_t radius) {
    if(!obj) return;
    applyRaisedCard(obj, radius);

    // Asegura herencia tambien en el boton (sobreescribe cualquier default LVGL)
    lv_obj_set_style_text_color(obj, getTextColor(), 0);

    // Estado presionado con feedback visual luminoso
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x242838), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_90, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x00F5D4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(obj, getTextColor(), LV_STATE_PRESSED);
}

void DefaultTheme::applyTextArea(lv_obj_t* obj, int32_t radius) {
    applyRaisedCard(obj, radius);
    lv_obj_set_style_text_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x94A3B8), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x00F5D4), LV_PART_CURSOR);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x00F5D4), LV_STATE_FOCUSED);
}

