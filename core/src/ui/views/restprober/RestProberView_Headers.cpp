#include "../RestProberView.hpp"
#include "RestProberCommon.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"

namespace cbdos {
namespace ui {

void RestProberView::setupHeadersTab(lv_obj_t* tab) {
    using namespace restprober;

    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 6, 0);

    // Barra de acciones de cabeceras (+ Cabecera, Limpiar)
    lv_obj_t* actRow = lv_obj_create(tab);
    lv_obj_set_size(actRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(actRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(actRow, 0, 0);
    lv_obj_set_style_pad_gap(actRow, 8, 0);
    lv_obj_set_style_bg_opa(actRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actRow, 0, 0);
    DefaultTheme::disableScroll(actRow);

    lv_obj_t* btnAdd = lv_button_create(actRow);
    lv_obj_set_size(btnAdd, 140, 36);
    lv_obj_set_style_radius(btnAdd, 8, 0);
    lv_obj_set_style_bg_color(btnAdd, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnAdd, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(btnAdd, 1, 0);
    lv_obj_add_event_cb(btnAdd, addHeaderBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblAdd = lv_label_create(btnAdd);
    lv_label_set_text(lblAdd, "+ Cabecera");
    lv_obj_set_style_text_color(lblAdd, lv_color_hex(0x00F5D4), 0);
    lv_obj_center(lblAdd);

    lv_obj_t* btnClear = lv_button_create(actRow);
    lv_obj_set_size(btnClear, 100, 36);
    lv_obj_set_style_radius(btnClear, 8, 0);
    lv_obj_set_style_bg_color(btnClear, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnClear, lv_color_hex(kColRed), 0);
    lv_obj_set_style_border_width(btnClear, 1, 0);
    lv_obj_add_event_cb(btnClear, clearHeadersBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblClear = lv_label_create(btnClear);
    lv_label_set_text(lblClear, "Limpiar");
    lv_obj_set_style_text_color(lblClear, lv_color_hex(kColRed), 0);
    lv_obj_center(lblClear);

    // Contenedor scrollable de filas clave-valor
    m_headersListCont = lv_obj_create(tab);
    lv_obj_set_size(m_headersListCont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_headersListCont, 1);
    lv_obj_set_flex_flow(m_headersListCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_headersListCont, 4, 0);
    lv_obj_set_style_pad_gap(m_headersListCont, 6, 0);
    lv_obj_set_style_bg_color(m_headersListCont, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_headersListCont, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(m_headersListCont, 1, 0);
    lv_obj_set_style_radius(m_headersListCont, 8, 0);

    // Cabecera por defecto para agilizar APIs REST
    addHeaderRow("Content-Type", "application/json");
}

void RestProberView::addHeaderRow(const std::string& key, const std::string& val) {
    if (!m_headersListCont) return;
    using namespace restprober;

    HeaderRowWidget row;
    row.rowObj = lv_obj_create(m_headersListCont);
    lv_obj_set_size(row.rowObj, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row.rowObj, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row.rowObj, 2, 0);
    lv_obj_set_style_pad_gap(row.rowObj, 6, 0);
    lv_obj_set_style_bg_opa(row.rowObj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row.rowObj, 0, 0);
    DefaultTheme::disableScroll(row.rowObj);

    row.taKey = lv_textarea_create(row.rowObj);
    lv_textarea_set_one_line(row.taKey, true);
    lv_obj_set_flex_grow(row.taKey, 1);
    lv_textarea_set_placeholder_text(row.taKey, "Clave (Header)");
    if (!key.empty()) lv_textarea_set_text(row.taKey, key.c_str());
    lv_obj_set_style_radius(row.taKey, 6, 0);
    lv_obj_set_style_bg_color(row.taKey, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(row.taKey, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(row.taKey, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_style_text_color(row.taKey, lv_color_hex(kColTextMuted), LV_PART_TEXTAREA_PLACEHOLDER);
    UIManager::attachKeyboard(row.taKey);

    row.taVal = lv_textarea_create(row.rowObj);
    lv_textarea_set_one_line(row.taVal, true);
    lv_obj_set_flex_grow(row.taVal, 1);
    lv_textarea_set_placeholder_text(row.taVal, "Valor");
    if (!val.empty()) lv_textarea_set_text(row.taVal, val.c_str());
    lv_obj_set_style_radius(row.taVal, 6, 0);
    lv_obj_set_style_bg_color(row.taVal, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(row.taVal, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(row.taVal, lv_color_hex(0xF1F5F9), 0);
    lv_obj_set_style_text_color(row.taVal, lv_color_hex(kColTextMuted), LV_PART_TEXTAREA_PLACEHOLDER);
    UIManager::attachKeyboard(row.taVal);

    lv_obj_t* btnDel = lv_button_create(row.rowObj);
    lv_obj_set_size(btnDel, 32, 32);
    lv_obj_set_style_radius(btnDel, 6, 0);
    lv_obj_set_style_bg_color(btnDel, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnDel, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(btnDel, 1, 0);
    lv_obj_add_event_cb(btnDel, [](lv_event_t* e) {
        lv_obj_t* parentRow = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
        if (parentRow && lv_obj_is_valid(parentRow)) {
            lv_obj_delete(parentRow);
        }
    }, LV_EVENT_CLICKED, row.rowObj);
    lv_obj_t* lblDel = lv_label_create(btnDel);
    lv_label_set_text(lblDel, "✕");
    lv_obj_set_style_text_color(lblDel, lv_color_hex(kColRed), 0);
    lv_obj_center(lblDel);

    m_headerRows.push_back(row);
}

void RestProberView::clearHeaders() {
    if (m_headersListCont) {
        lv_obj_clean(m_headersListCont);
    }
    m_headerRows.clear();
}

void RestProberView::addHeaderBtnCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (view) {
        view->addHeaderRow();
    }
}

void RestProberView::clearHeadersBtnCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (view) {
        view->clearHeaders();
    }
}

} // namespace ui
} // namespace cbdos
