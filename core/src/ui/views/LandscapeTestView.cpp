#include "LandscapeTestView.hpp"
#include "../themes/DefaultTheme.h"
#include "../ThemeEngine.hpp"
#include "cbdos/display.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

LandscapeTestView::LandscapeTestView()
    : BaseView("Panoramica", ViewMode::Solid, ViewOrientation::Landscape),
      m_clickCount(0) {
}

LandscapeTestView::~LandscapeTestView() {
    onDestroy();
}

void LandscapeTestView::onDestroy() {
    BaseView::onDestroy();
    m_lblResolution = nullptr;
    m_btnTouch = nullptr;
    m_lblTouch = nullptr;
}

void LandscapeTestView::touchTestBtnCb(lv_event_t* e) {
    auto* self = static_cast<LandscapeTestView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_clickCount++;
    char buf[64];
    snprintf(buf, sizeof(buf), LV_SYMBOL_OK " Touch calibrado! Clics: %d", self->m_clickCount);
    if (self->m_lblTouch) {
        lv_label_set_text(self->m_lblTouch, buf);
    }
    if (self->m_btnTouch) {
        lv_obj_set_style_bg_color(self->m_btnTouch, lv_color_hex(0x10B981), 0); // Verde esmeralda
    }
}

bool LandscapeTestView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    const auto& palette = ThemeEngine::getInstance().getPalette();

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(palette.bg), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 16, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Título distintivo
    lv_obj_t* lblTitle = lv_label_create(m_container);
    lv_label_set_text(lblTitle, LV_SYMBOL_IMAGE " MODO PANORÁMICO ACTIVO");
    lv_obj_set_style_text_color(lblTitle, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_16, 0);

    // 2. Consulta y muestra de resolución en vivo
    int32_t w = 800, h = 480;
    lv_display_t* disp = lv_display_get_default();
    if (disp) {
        w = lv_display_get_horizontal_resolution(disp);
        h = lv_display_get_vertical_resolution(disp);
    }

    char resBuf[64];
    snprintf(resBuf, sizeof(resBuf), "Resolución lógica: %ld × %ld px (Horizontal)", (long)w, (long)h);
    m_lblResolution = lv_label_create(m_container);
    lv_label_set_text(m_lblResolution, resBuf);
    lv_obj_set_style_text_color(m_lblResolution, lv_color_hex(palette.textSecondary), 0);
    lv_obj_set_style_text_font(m_lblResolution, &lv_font_montserrat_14, 0);

    // 3. Botón de prueba de Touch interactivo
    m_btnTouch = lv_button_create(m_container);
    lv_obj_set_size(m_btnTouch, 320, 48);
    DefaultTheme::applyButton(m_btnTouch, 12);
    lv_obj_add_event_cb(m_btnTouch, touchTestBtnCb, LV_EVENT_CLICKED, this);

    m_lblTouch = lv_label_create(m_btnTouch);
    lv_label_set_text(m_lblTouch, LV_SYMBOL_BELL " Toca aquí para probar el Touch");
    lv_obj_set_style_text_font(m_lblTouch, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblTouch, lv_color_hex(palette.textPrimary), 0);
    lv_obj_center(m_lblTouch);

    // 4. Instrucción de navegación
    lv_obj_t* lblHint = lv_label_create(m_container);
    lv_label_set_text(lblHint, "Toca '" LV_SYMBOL_LEFT " Volver' en la barra superior para regresar al Dashboard vertical.");
    lv_obj_set_style_text_color(lblHint, lv_color_hex(palette.textSecondary), 0);
    lv_obj_set_style_text_font(lblHint, &lv_font_montserrat_12, 0);

    return true;
}

} // namespace ui
} // namespace cbdos
