#include "../RestProberView.hpp"
#include "RestProberCommon.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"

namespace cbdos {
namespace ui {

void RestProberView::createControlBar(lv_obj_t* parent) {
    using namespace restprober;

    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_style_pad_gap(bar, 6, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColBgCard), 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 10, 0);
    DefaultTheme::disableScroll(bar);

    // 1. Selector de Método HTTP con resaltado cromático
    m_dropdownMethod = lv_dropdown_create(bar);
    lv_dropdown_set_options(m_dropdownMethod, "GET\nPOST\nPUT\nDELETE\nPATCH\nHEAD");
    lv_obj_set_width(m_dropdownMethod, 96);
    lv_obj_set_style_radius(m_dropdownMethod, 8, 0);
    lv_obj_set_style_bg_color(m_dropdownMethod, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_text_color(m_dropdownMethod, lv_color_hex(kColGreen), 0);
    lv_obj_set_style_border_color(m_dropdownMethod, lv_color_hex(kColGreen), 0);
    lv_obj_set_style_border_width(m_dropdownMethod, 1, 0);
    lv_obj_add_event_cb(m_dropdownMethod, methodDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    // 2. Campo de URL (Textarea con teclado reactivo)
    m_taUrl = lv_textarea_create(bar);
    lv_textarea_set_one_line(m_taUrl, true);
    lv_obj_set_flex_grow(m_taUrl, 1);
    lv_textarea_set_placeholder_text(m_taUrl, "http://192.168.1.100/...");
    std::string defaultUrl = m_initialUrl.empty() ? "http://" : m_initialUrl;
    lv_textarea_set_text(m_taUrl, defaultUrl.c_str());
    lv_obj_set_style_radius(m_taUrl, 8, 0);
    lv_obj_set_style_bg_color(m_taUrl, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_taUrl, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(m_taUrl, lv_color_hex(0xF1F5F9), 0);
    UIManager::attachKeyboard(m_taUrl);

    // 3. Botón de Disparo HTTP (SEND 🚀)
    m_btnSend = lv_button_create(bar);
    lv_obj_set_size(m_btnSend, 82, 40);
    lv_obj_set_style_radius(m_btnSend, 8, 0);
    lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x00F5D4), 0);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);

    m_lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(m_lblSend, "SEND 🚀");
    lv_obj_set_style_text_color(m_lblSend, lv_color_hex(0x0A0F1D), 0);
    lv_obj_center(m_lblSend);
}

void RestProberView::methodDropdownCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (!view || !view->m_dropdownMethod) return;

    uint16_t sel = lv_dropdown_get_selected(view->m_dropdownMethod);
    http::HttpMethod method = static_cast<http::HttpMethod>(sel);
    uint32_t col = restprober::getMethodColor(method);

    lv_obj_set_style_text_color(view->m_dropdownMethod, lv_color_hex(col), 0);
    lv_obj_set_style_border_color(view->m_dropdownMethod, lv_color_hex(col), 0);
}

void RestProberView::sendBtnCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (view) {
        view->sendRequest();
    }
}

} // namespace ui
} // namespace cbdos
