#include "UtilitiesView.hpp"
#include "utilities/TodoApp.hpp"
#include "utilities/CalculatorApp.hpp"
#include "utilities/StopwatchApp.hpp"
#include "utilities/PomodoroApp.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/language.hpp"

namespace cbdos {
namespace ui {

UtilitiesView::UtilitiesView()
    : BaseView("Utilidades"), m_tabview(nullptr) {
}

void UtilitiesView::onDestroy() {
    TodoApp::cleanup();
    CalculatorApp::cleanup();
    StopwatchApp::cleanup();
    PomodoroApp::cleanup();
    m_tabview = nullptr;
    BaseView::onDestroy();
}

bool UtilitiesView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Configurar cabecera
    UIManager::getInstance().getHeaderBar().setTitle(cbdos::lang::tr(cbdos::lang::StrId::STR_UTIL_TITLE));
    UIManager::getInstance().getHeaderBar().showWifi(false);

    // Contenedor principal de la vista (transparente para ver el fondo)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 6, 0);
    DefaultTheme::disableScroll(m_container);

    // Tabview principal con 4 pestañas
    m_tabview = lv_tabview_create(m_container);
    lv_tabview_set_tab_bar_position(m_tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(m_tabview, 38);
    lv_obj_set_size(m_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_tabview, 0, 0);
    lv_obj_set_style_border_width(m_tabview, 0, 0);

    // Estilizar barra de pestañas
    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_pad_column(tab_bar, 4, 0);

    lv_obj_t* tab_todo = lv_tabview_add_tab(m_tabview, cbdos::lang::tr(cbdos::lang::StrId::STR_UTIL_NOTES));
    lv_obj_t* tab_calc = lv_tabview_add_tab(m_tabview, cbdos::lang::tr(cbdos::lang::StrId::STR_UTIL_CALC));
    lv_obj_t* tab_sw   = lv_tabview_add_tab(m_tabview, cbdos::lang::tr(cbdos::lang::StrId::STR_UTIL_SW));
    lv_obj_t* tab_pomo = lv_tabview_add_tab(m_tabview, cbdos::lang::tr(cbdos::lang::StrId::STR_UTIL_POMO));

    // Desactivar scroll en el contenedor principal de pestañas
    lv_obj_t* content = lv_tabview_get_content(m_tabview);
    DefaultTheme::disableScroll(content);
    lv_obj_set_scroll_snap_x(content, LV_SCROLL_SNAP_NONE);
    lv_obj_set_scroll_snap_y(content, LV_SCROLL_SNAP_NONE);
    lv_obj_set_style_pad_all(content, 0, 0);

    // Desactivar scroll directo en contenedores de pestaña
    DefaultTheme::disableScroll(tab_todo);
    DefaultTheme::disableScroll(tab_calc);
    DefaultTheme::disableScroll(tab_sw);
    DefaultTheme::disableScroll(tab_pomo);

    // Estilizar botones individuales de las pestañas
    uint32_t btn_cnt = lv_obj_get_child_count(tab_bar);
    for (uint32_t i = 0; i < btn_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(tab_bar, i);
        if (btn) {
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1B1E29), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_60, 0);
            lv_obj_set_style_text_color(btn, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_border_width(btn, 0, 0);

            // Tipografía de pestaña
            lv_obj_t* lbl = lv_obj_get_child(btn, 0);
            if (lbl) {
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            }

            // Estado activo
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x242838), LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
            lv_obj_set_style_text_color(btn, DefaultTheme::getPrimaryAccent(), LV_STATE_CHECKED);
            lv_obj_set_style_border_color(btn, DefaultTheme::getPrimaryAccent(), LV_STATE_CHECKED);
            lv_obj_set_style_border_width(btn, 1, LV_STATE_CHECKED);
            lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
        }
    }

    // Construir cada módulo
    TodoApp::build(tab_todo);
    CalculatorApp::build(tab_calc);
    StopwatchApp::build(tab_sw);
    PomodoroApp::build(tab_pomo);

    return true;
}

void UtilitiesView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
