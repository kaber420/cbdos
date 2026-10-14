#include "../RestProberView.hpp"
#include "RestProberCommon.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"

namespace cbdos {
namespace ui {

void RestProberView::setupBodyTab(lv_obj_t* tab) {
    using namespace restprober;

    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 6, 0);

    // Barra de inserción rápida de payloads y limpieza
    lv_obj_t* btnRow = lv_obj_create(tab);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_style_pad_gap(btnRow, 8, 0);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    DefaultTheme::disableScroll(btnRow);

    lv_obj_t* btnTpl = lv_button_create(btnRow);
    lv_obj_set_size(btnTpl, 150, 34);
    lv_obj_set_style_radius(btnTpl, 8, 0);
    lv_obj_set_style_bg_color(btnTpl, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnTpl, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(btnTpl, 1, 0);
    lv_obj_add_event_cb(btnTpl, [](lv_event_t* e) {
        auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
        if (view && view->m_taBody) {
            lv_textarea_set_text(view->m_taBody, "{\n  \"turn\": \"toggle\"\n}");
        }
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* lblTpl = lv_label_create(btnTpl);
    lv_label_set_text(lblTpl, "{ } JSON Relay");
    lv_obj_set_style_text_color(lblTpl, lv_color_hex(0x00F5D4), 0);
    lv_obj_center(lblTpl);

    lv_obj_t* btnClr = lv_button_create(btnRow);
    lv_obj_set_size(btnClr, 90, 34);
    lv_obj_set_style_radius(btnClr, 8, 0);
    lv_obj_set_style_bg_color(btnClr, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnClr, lv_color_hex(kColRed), 0);
    lv_obj_set_style_border_width(btnClr, 1, 0);
    lv_obj_add_event_cb(btnClr, [](lv_event_t* e) {
        auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
        if (view && view->m_taBody) {
            lv_textarea_set_text(view->m_taBody, "");
        }
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* lblClr = lv_label_create(btnClr);
    lv_label_set_text(lblClr, "Borrar");
    lv_obj_set_style_text_color(lblClr, lv_color_hex(kColRed), 0);
    lv_obj_center(lblClr);

    // Text area multilínea para el cuerpo
    m_taBody = lv_textarea_create(tab);
    lv_obj_set_size(m_taBody, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_taBody, 1);
    lv_textarea_set_placeholder_text(m_taBody, "Cuerpo de la peticion (JSON, XML o texto plano)...");
    lv_obj_set_style_radius(m_taBody, 8, 0);
    lv_obj_set_style_bg_color(m_taBody, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_taBody, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(m_taBody, lv_color_hex(0xF1F5F9), 0);
    UIManager::attachKeyboard(m_taBody);
}

} // namespace ui
} // namespace cbdos
