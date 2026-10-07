#include "DashboardView.hpp"
#include "../AppRegistry.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../assets/SystemIcons.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include "cbdos/language.hpp"
#include <cstring>

namespace cbdos {
namespace ui {

DashboardView::DashboardView()
    : BaseView(cbdos::lang::tr(cbdos::lang::StrId::STR_DASHBOARD)) {
}

bool DashboardView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Asegurar inicialización y refresco dinámico de apps externas (MicroSD)
    auto& registry = AppRegistry::getInstance();
    registry.initSystemApps();
    registry.rescanExternalApps();

    // Contenedor principal con scroll vertical suave (Fondo transparente para ver el Wallpaper)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    setupLayout();
    createCards();
    return true;
}

void DashboardView::onDestroy() {
    m_cardObjs.clear();
    BaseView::onDestroy();
}

void DashboardView::setupLayout() {
    auto caps = cbdos::display::getCapabilities();

    // Configurar Flex Wrap para distribución automática de 3 columnas
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    if (caps.width >= 480) {
        // ESP32-P4 (480x800): 3 columnas amplias
        lv_obj_set_style_pad_row(m_container, 20, 0);
        lv_obj_set_style_pad_column(m_container, 8, 0);
        lv_obj_set_style_pad_hor(m_container, 16, 0);
        lv_obj_set_style_pad_ver(m_container, 16, 0);
    } else {
        // ESP32-S3 (320x480): 3 columnas compactas
        lv_obj_set_style_pad_row(m_container, 14, 0);
        lv_obj_set_style_pad_column(m_container, 6, 0);
        lv_obj_set_style_pad_hor(m_container, 10, 0);
        lv_obj_set_style_pad_ver(m_container, 12, 0);
    }
}

void DashboardView::createCards() {
    auto caps = cbdos::display::getCapabilities();
    const auto& apps = AppRegistry::getInstance().getVisibleApps();

    int32_t itemWidth = (caps.width >= 480) ? 140 : 96;
    int32_t iconSize = (caps.width >= 480) ? 84 : 64;
    int32_t iconRadius = (caps.width >= 480) ? 24 : 18;

    m_cardObjs.clear();

    for (size_t i = 0; i < apps.size(); ++i) {
        const auto& app = apps[i];
        if (!app.visibleInDashboard) continue;

        // 1. Contenedor vertical transparente de la app (Ícono + Título)
        lv_obj_t* appItem = lv_obj_create(m_container);
        lv_obj_set_size(appItem, itemWidth, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(appItem, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(appItem, 0, 0);
        lv_obj_set_style_pad_all(appItem, 0, 0);
        DefaultTheme::disableScroll(appItem);

        lv_obj_set_flex_flow(appItem, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(appItem, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // 2. Botón Squircle del Icono (iOS / Android style)
        lv_obj_t* btnIcon = lv_button_create(appItem);
        lv_obj_set_size(btnIcon, iconSize, iconSize);
        lv_obj_set_style_radius(btnIcon, iconRadius, 0);
        lv_obj_set_style_bg_color(btnIcon, lv_color_hex(0x1E2230), 0);
        lv_obj_set_style_bg_opa(btnIcon, LV_OPA_60, 0);
        lv_obj_set_style_border_color(btnIcon, lv_color_hex(app.accentColor), 0);
        lv_obj_set_style_border_width(btnIcon, 1, 0);
        lv_obj_set_style_border_opa(btnIcon, LV_OPA_60, 0);
        lv_obj_set_style_shadow_width(btnIcon, 12, 0);
        lv_obj_set_style_shadow_color(btnIcon, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(btnIcon, LV_OPA_40, 0);
        lv_obj_set_style_pad_all(btnIcon, 0, 0);

        // --- EFECTO: Reacción Táctil Cinematográfica (LV_STATE_PRESSED) ---
        lv_obj_set_style_border_width(btnIcon, 2, LV_STATE_PRESSED);
        lv_obj_set_style_border_opa(btnIcon, LV_OPA_100, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(btnIcon, 20, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(btnIcon, lv_color_hex(app.accentColor), LV_STATE_PRESSED);
        lv_obj_set_style_shadow_opa(btnIcon, LV_OPA_80, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btnIcon, lv_color_hex(0x2A3045), LV_STATE_PRESSED);

        // 3. Icono de la App
        lv_obj_t* iconObj = nullptr;
        if (app.icon.type == IconType::SYSTEM_BUILTIN && !app.icon.source.empty()) {
            iconObj = SystemIcons::createIcon(btnIcon, app.icon.source, (caps.width >= 480) ? 48 : 36);
        }

        if (iconObj) {
            lv_obj_center(iconObj);
            lv_obj_remove_flag(iconObj, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Icono estándar por símbolo LVGL
            const char* symbolStr = !app.icon.fallbackSymbol.empty() ? app.icon.fallbackSymbol.c_str() : LV_SYMBOL_FILE;
            lv_obj_t* lblIcon = lv_label_create(btnIcon);
            lv_label_set_text(lblIcon, symbolStr);
            lv_obj_set_style_text_color(lblIcon, lv_color_hex(app.accentColor), 0);
            lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
            lv_obj_center(lblIcon);
        }

        // 4. Título de la App situado debajo del ícono
        lv_obj_t* lblTitle = lv_label_create(appItem);
        lv_label_set_text(lblTitle, app.title.c_str());
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(lblTitle, 6, 0);
        lv_obj_set_style_text_align(lblTitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lblTitle, itemWidth);

        // Evento de Click pasando el índice de la App en AppRegistry
        lv_obj_set_user_data(btnIcon, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(btnIcon, cardClickedEventCb, LV_EVENT_CLICKED, this);

        m_cardObjs.push_back(btnIcon);
    }
}

void DashboardView::cardClickedEventCb(lv_event_t* e) {
    auto* view = static_cast<DashboardView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!view || !target) return;

    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    const auto& apps = AppRegistry::getInstance().getVisibleApps();
    if (idx >= apps.size()) return;

    const auto& app = apps[idx];
    if (app.launchAction) {
        app.launchAction();
    }
}

void DashboardView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
