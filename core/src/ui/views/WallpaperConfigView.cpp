#include "WallpaperConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../WallpaperManager.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

WallpaperConfigView::WallpaperConfigView()
    : BaseView("Fondo de Pantalla") {
}

void WallpaperConfigView::default_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Sin RGB embebido: "predeterminado" = animado Constellation + borra /wallpaper.bin
        WallpaperManager::getInstance().restoreDefault();
        UIManager::showToast("Fondo animado por defecto aplicado");
        UIManager::getInstance().popView();
    }
}

void WallpaperConfigView::animated_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        const char* mode = (const char*)lv_obj_get_user_data(btn);
        if (!mode) mode = "animated_constellation";
        WallpaperManager::getInstance().setWallpaper(mode);
        UIManager::showToast("Fondo Animado Vectorial activado");
        UIManager::getInstance().popView();
    }
}

void WallpaperConfigView::wallpaper_select_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        const char* path = (const char*)lv_obj_get_user_data(btn);
        if (path) {
            bool ok = WallpaperManager::getInstance().setWallpaper(path);
            if (ok) {
                UIManager::showToast("Fondo copiado a Flash y aplicado");
            } else {
                UIManager::showToast("Error: Formato no compatible");
            }
            UIManager::getInstance().popView();
        }
    }
}

bool WallpaperConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);

    // 1. Botón Fondo Animado: Constelación Neón
    lv_obj_t* animConstCard = lv_button_create(m_container);
    lv_obj_set_width(animConstCard, lv_pct(100));
    lv_obj_set_height(animConstCard, 54);
    DefaultTheme::applyButton(animConstCard, 14);
    lv_obj_set_user_data(animConstCard, (void*)"animated_constellation");
    lv_obj_add_event_cb(animConstCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animConstCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animConstCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animConstCard, 16, 0);

    lv_obj_t* iconConst = lv_label_create(animConstCard);
    lv_label_set_text(iconConst, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(iconConst, lv_color_hex(0x00e5ff), 0);
    lv_obj_set_style_margin_right(iconConst, 12, 0);

    lv_obj_t* lblConst = lv_label_create(animConstCard);
    lv_label_set_text(lblConst, "Animado: Constelación Neón");
    lv_obj_set_style_text_color(lblConst, DefaultTheme::getTextColor(), 0);

    // 2. Botón Fondo Animado: Ondas Neón
    lv_obj_t* animWavesCard = lv_button_create(m_container);
    lv_obj_set_width(animWavesCard, lv_pct(100));
    lv_obj_set_height(animWavesCard, 54);
    DefaultTheme::applyButton(animWavesCard, 14);
    lv_obj_set_user_data(animWavesCard, (void*)"animated_waves");
    lv_obj_add_event_cb(animWavesCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animWavesCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animWavesCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animWavesCard, 16, 0);

    lv_obj_t* iconWaves = lv_label_create(animWavesCard);
    lv_label_set_text(iconWaves, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(iconWaves, lv_color_hex(0x9d4edd), 0);
    lv_obj_set_style_margin_right(iconWaves, 12, 0);

    lv_obj_t* lblWaves = lv_label_create(animWavesCard);
    lv_label_set_text(lblWaves, "Animado: Ondas Neón");
    lv_obj_set_style_text_color(lblWaves, DefaultTheme::getTextColor(), 0);

    // 3. Botón Fondo Animado: Carrito Cómic 2D
    lv_obj_t* animComicCard = lv_button_create(m_container);
    lv_obj_set_width(animComicCard, lv_pct(100));
    lv_obj_set_height(animComicCard, 54);
    DefaultTheme::applyButton(animComicCard, 14);
    lv_obj_set_user_data(animComicCard, (void*)"animated_comic");
    lv_obj_add_event_cb(animComicCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animComicCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animComicCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animComicCard, 16, 0);

    lv_obj_t* iconComic = lv_label_create(animComicCard);
    lv_label_set_text(iconComic, LV_SYMBOL_DRIVE);
    lv_obj_set_style_text_color(iconComic, lv_color_hex(0xef233c), 0);
    lv_obj_set_style_margin_right(iconComic, 12, 0);

    lv_obj_t* lblComic = lv_label_create(animComicCard);
    lv_label_set_text(lblComic, "Animado: Carrito Cómic 2D");
    lv_obj_set_style_text_color(lblComic, DefaultTheme::getTextColor(), 0);

    // 4. Botón Fondo Animado: Luciérnagas en el Bosque
    lv_obj_t* animFfCard = lv_button_create(m_container);
    lv_obj_set_width(animFfCard, lv_pct(100));
    lv_obj_set_height(animFfCard, 54);
    DefaultTheme::applyButton(animFfCard, 14);
    lv_obj_set_user_data(animFfCard, (void*)"animated_fireflies");
    lv_obj_add_event_cb(animFfCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animFfCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animFfCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animFfCard, 16, 0);

    lv_obj_t* iconFf = lv_label_create(animFfCard);
    lv_label_set_text(iconFf, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(iconFf, lv_color_hex(0xd4ff00), 0);
    lv_obj_set_style_margin_right(iconFf, 12, 0);

    lv_obj_t* lblFf = lv_label_create(animFfCard);
    lv_label_set_text(lblFf, "Animado: Luciérnagas Bosque");
    lv_obj_set_style_text_color(lblFf, DefaultTheme::getTextColor(), 0);

    // 5. Botón Fondo Animado: Touch Swarm Magnético
    lv_obj_t* animTouchCard = lv_button_create(m_container);
    lv_obj_set_width(animTouchCard, lv_pct(100));
    lv_obj_set_height(animTouchCard, 54);
    DefaultTheme::applyButton(animTouchCard, 14);
    lv_obj_set_user_data(animTouchCard, (void*)"animated_touch");
    lv_obj_add_event_cb(animTouchCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animTouchCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animTouchCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animTouchCard, 16, 0);

    lv_obj_t* iconTouch = lv_label_create(animTouchCard);
    lv_label_set_text(iconTouch, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(iconTouch, lv_color_hex(0x00f5d4), 0);
    lv_obj_set_style_margin_right(iconTouch, 12, 0);

    lv_obj_t* lblTouch = lv_label_create(animTouchCard);
    lv_label_set_text(lblTouch, "Animado: Partículas Touch Magnet");
    lv_obj_set_style_text_color(lblTouch, DefaultTheme::getTextColor(), 0);

    // 6. Botón Fondo Animado: Synthwave 80s Grid 3D
    lv_obj_t* animSynthCard = lv_button_create(m_container);
    lv_obj_set_width(animSynthCard, lv_pct(100));
    lv_obj_set_height(animSynthCard, 54);
    DefaultTheme::applyButton(animSynthCard, 14);
    lv_obj_set_user_data(animSynthCard, (void*)"animated_synthwave");
    lv_obj_add_event_cb(animSynthCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animSynthCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animSynthCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animSynthCard, 16, 0);

    lv_obj_t* iconSynth = lv_label_create(animSynthCard);
    lv_label_set_text(iconSynth, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(iconSynth, lv_color_hex(0xff007f), 0);
    lv_obj_set_style_margin_right(iconSynth, 12, 0);

    lv_obj_t* lblSynth = lv_label_create(animSynthCard);
    lv_label_set_text(lblSynth, "Animado: Synthwave 80s Grid");
    lv_obj_set_style_text_color(lblSynth, DefaultTheme::getTextColor(), 0);

    // 7. Botón Fondo Animado: Matrix Code Rain
    lv_obj_t* animMatrixCard = lv_button_create(m_container);
    lv_obj_set_width(animMatrixCard, lv_pct(100));
    lv_obj_set_height(animMatrixCard, 54);
    DefaultTheme::applyButton(animMatrixCard, 14);
    lv_obj_set_user_data(animMatrixCard, (void*)"animated_matrix");
    lv_obj_add_event_cb(animMatrixCard, animated_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(animMatrixCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(animMatrixCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(animMatrixCard, 16, 0);

    lv_obj_t* iconMatrix = lv_label_create(animMatrixCard);
    lv_label_set_text(iconMatrix, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(iconMatrix, lv_color_hex(0x00ff41), 0);
    lv_obj_set_style_margin_right(iconMatrix, 12, 0);

    lv_obj_t* lblMatrix = lv_label_create(animMatrixCard);
    lv_label_set_text(lblMatrix, "Animado: Matrix Code Rain");
    lv_obj_set_style_text_color(lblMatrix, DefaultTheme::getTextColor(), 0);

    // 8. Boton Restaurar animado por defecto (borra RGB de SPIFFS si existia)
    lv_obj_t* defaultCard = lv_button_create(m_container);
    lv_obj_set_width(defaultCard, lv_pct(100));
    lv_obj_set_height(defaultCard, 54);
    DefaultTheme::applyButton(defaultCard, 14);
    lv_obj_add_event_cb(defaultCard, default_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_flex_flow(defaultCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(defaultCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(defaultCard, 16, 0);

    lv_obj_t* iconDef = lv_label_create(defaultCard);
    lv_label_set_text(iconDef, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(iconDef, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_margin_right(iconDef, 12, 0);

    lv_obj_t* lblDef = lv_label_create(defaultCard);
    lv_label_set_text(lblDef, "Restaurar animado por defecto");
    lv_obj_set_style_text_color(lblDef, DefaultTheme::getTextColor(), 0);

    // 2. Separador de sección
    lv_obj_t* secLbl = lv_label_create(m_container);
    lv_label_set_text(secLbl, "Fondos en MicroSD (/wallpapers/):");
    lv_obj_set_style_text_color(secLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(secLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_top(secLbl, 10, 0);

    // 3. Obtener lista de wallpapers desde la MicroSD
    std::vector<std::string> wallpapers = WallpaperManager::getInstance().getAvailableWallpapers();

    if (wallpapers.empty()) {
        lv_obj_t* emptyCard = lv_obj_create(m_container);
        lv_obj_set_width(emptyCard, lv_pct(100));
        DefaultTheme::applyRaisedCard(emptyCard, 12);
        lv_obj_set_style_pad_all(emptyCard, 14, 0);

        lv_obj_t* emptyLbl = lv_label_create(emptyCard);
        lv_label_set_text(emptyLbl, "No se encontraron imagenes en\n/wallpapers/ (*.bin, *.bmp, *.jpg)");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (size_t i = 0; i < wallpapers.size(); i++) {
            const std::string& fullPath = wallpapers[i];
            
            size_t slashPos = fullPath.find_last_of('/');
            std::string filename = (slashPos != std::string::npos) ? fullPath.substr(slashPos + 1) : fullPath;

            lv_obj_t* card = lv_button_create(m_container);
            lv_obj_set_width(card, lv_pct(100));
            lv_obj_set_height(card, 54);
            DefaultTheme::applyButton(card, 14);

            char* pathCopy = strdup(fullPath.c_str());
            lv_obj_set_user_data(card, (void*)pathCopy);
            lv_obj_add_event_cb(card, wallpaper_select_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(card, 16, 0);

            lv_obj_t* icon = lv_label_create(card);
            lv_label_set_text(icon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(icon, DefaultTheme::getSecondaryAccent(), 0);
            lv_obj_set_style_margin_right(icon, 12, 0);

            lv_obj_t* lbl = lv_label_create(card);
            lv_label_set_text(lbl, filename.c_str());
            lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
        }
    }

    return true;
}

} // namespace ui
} // namespace cbdos
