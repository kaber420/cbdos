#include "DashboardView.hpp"
#include "ConfigView.hpp"
#include "MusicPlayerView.hpp"
#include "AudioRecorderView.hpp"
#include "GalleryListView.hpp"
#include "FlasherView.hpp"
#include "RadioView.hpp"
#include "LuaRunnerView.hpp"
#include "TextEditorView.hpp"
#include "CartridgeView.hpp"
#include "FileManagerView.hpp"
#include "TerminalView.hpp"
#include "HidView.hpp"
#include "UtilitiesView.hpp"
#include "TlvBrowserView.hpp"
#include "LuappView.hpp"
#include "LottieTestView.hpp"
#include "MeshCoreView.hpp"
#include "../../apps/kerberos/KerberosView.hpp"
#include "../../lua/LuappManager.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../assets/SystemIcons.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include <cstring>

namespace cbdos {
namespace ui {

DashboardView::DashboardView()
    : BaseView("Dashboard") {
}

bool DashboardView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Resetear lista de aplicaciones
    m_apps = {
        {"browser", "Navegador", LV_SYMBOL_EYE_OPEN, 0x00B4D8, false, ""},
        {"gallery", "Galeria", LV_SYMBOL_IMAGE, 0xEC4899, false, ""},
        {"files", "Archivos", LV_SYMBOL_DIRECTORY, 0xF77F00, false, ""},
        {"utilities", "Utilidades", LV_SYMBOL_LIST, 0x00F5D4, false, ""},
        {"cartridge", "Cartuchos", LV_SYMBOL_PLAY, 0x3F68D9, false, ""},
        {"lua", "Lua Runner", LV_SYMBOL_FILE, 0x06B6D4, false, ""},
        {"editor", "Editor", LV_SYMBOL_EDIT, 0x3B82F6, false, ""},
        {"radio", "Radio Online", LV_SYMBOL_WIFI, 0x10B981, false, ""},
        {"flasher", "Flasheador", LV_SYMBOL_DOWNLOAD, 0xF59E0B, false, ""},
        {"terminal", "Terminal", LV_SYMBOL_KEYBOARD, 0x10B981, false, ""},
        {"hid", "HID Control", LV_SYMBOL_KEYBOARD, 0xFFD60A, false, ""},
        {"meshcore", "MeshCore", LV_SYMBOL_WIFI, 0x00E5FF, false, ""},
        {"recorder", "Grabadora", LV_SYMBOL_AUDIO, 0xEF4444, false, ""},
        {"music", "Musica", LV_SYMBOL_AUDIO, 0x00E5FF, false, ""},
        {"kerberos", "Kerberos FIDO", LV_SYMBOL_USB, 0x10B981, false, ""},
        {"lottie", "Lottie Test", LV_SYMBOL_IMAGE, 0x06D6A0, false, ""},
        {"config", "Configuracion", LV_SYMBOL_SETTINGS, 0x9D4EDD, false, ""}
    };

    // Escanear dinámicamente aplicaciones .luapp en la MicroSD
    auto& luappMgr = cbdos::lua::LuappManager::getInstance();
    luappMgr.scanApps("/sdcard/apps");
    for (const auto& app : luappMgr.getDiscoveredApps()) {
        m_apps.push_back({
            "luapp_" + app.name,
            app.name,
            app.iconSymbol,
            app.accentColor,
            true,
            app.filePath
        });
    }

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

    // Dimensiones para 3 iconos por fila: más grandes y cómodos al tacto
    int32_t itemWidth = (caps.width >= 480) ? 140 : 96;
    int32_t iconSize = (caps.width >= 480) ? 84 : 64;
    int32_t iconRadius = (caps.width >= 480) ? 24 : 18;

    m_cardObjs.clear();

    for (size_t i = 0; i < m_apps.size(); ++i) {
        const auto& app = m_apps[i];

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

        // --- EFECTO 1: Reacción Táctil Cinematográfica (LV_STATE_PRESSED) ---
        // 1. Borde y Glow Neón intensos al contacto
        lv_obj_set_style_border_width(btnIcon, 2, LV_STATE_PRESSED);
        lv_obj_set_style_border_opa(btnIcon, LV_OPA_100, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(btnIcon, 20, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(btnIcon, lv_color_hex(app.accentColor), LV_STATE_PRESSED);
        lv_obj_set_style_shadow_opa(btnIcon, LV_OPA_80, LV_STATE_PRESSED);
        // 3. Fondo reactivo más brillante al pulsar
        lv_obj_set_style_bg_color(btnIcon, lv_color_hex(0x2A3045), LV_STATE_PRESSED);

        // 2. Icono de la App (Cargado limpiamente desde SystemIcons / SVG o fallback por símbolo)
        lv_obj_t* iconObj = SystemIcons::createIcon(btnIcon, app.id, (caps.width >= 480) ? 48 : 36);
        if (iconObj) {
            lv_obj_center(iconObj);
            lv_obj_remove_flag(iconObj, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Icono estándar por símbolo LVGL
            lv_obj_t* lblIcon = lv_label_create(btnIcon);
            lv_label_set_text(lblIcon, app.icon.c_str());
            lv_obj_set_style_text_color(lblIcon, lv_color_hex(app.accentColor), 0);
            lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_24, 0);
            lv_obj_center(lblIcon);
        }

        // 3. Título de la App situado debajo del ícono (como en un OS)
        lv_obj_t* lblTitle = lv_label_create(appItem);
        lv_label_set_text(lblTitle, app.title.c_str());
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(lblTitle, 6, 0);
        lv_obj_set_style_text_align(lblTitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lblTitle, itemWidth);

        // Evento de Click pasando el índice de la App
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
    if (idx >= view->m_apps.size()) return;
    const auto& app = view->m_apps[idx];

    if (app.isLuapp) {
        UIManager::getInstance().pushView(std::make_shared<LuappView>(app.luappPath, app.title, app.icon));
        return;
    }

    if (app.id == "browser") {
        UIManager::getInstance().pushView(std::make_shared<TlvBrowserView>());
    } else if (app.id == "gallery") {
        UIManager::getInstance().pushView(std::make_shared<GalleryListView>());
    } else if (app.id == "files") {
        UIManager::getInstance().pushView(std::make_shared<FileManagerView>());
    } else if (app.id == "utilities") {
        UIManager::getInstance().pushView(std::make_shared<UtilitiesView>());
    } else if (app.id == "cartridge") {
        UIManager::getInstance().pushView(std::make_shared<CartridgeView>());
    } else if (app.id == "lua") {
        UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>());
    } else if (app.id == "editor") {
        UIManager::getInstance().pushView(std::make_shared<TextEditorView>());
    } else if (app.id == "radio") {
        UIManager::getInstance().pushView(std::make_shared<RadioView>());
    } else if (app.id == "config") {
        UIManager::getInstance().pushView(std::make_shared<ConfigView>());
    } else if (app.id == "recorder") {
        UIManager::getInstance().pushView(std::make_shared<AudioRecorderView>());
    } else if (app.id == "music") {
        UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>());
    } else if (app.id == "kerberos") {
        UIManager::getInstance().pushView(std::make_shared<KerberosView>());
    } else if (app.id == "flasher") {
        UIManager::getInstance().pushView(std::make_shared<FlasherView>());
    } else if (app.id == "terminal") {
        UIManager::getInstance().pushView(std::make_shared<TerminalView>());
    } else if (app.id == "hid") {
        UIManager::getInstance().pushView(std::make_shared<HidView>());
    } else if (app.id == "meshcore") {
        UIManager::getInstance().pushView(std::make_shared<MeshCoreView>());
    } else if (app.id == "lottie") {
        UIManager::getInstance().pushView(std::make_shared<LottieTestView>());
    }
}

void DashboardView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
