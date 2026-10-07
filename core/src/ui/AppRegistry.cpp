#include "AppRegistry.hpp"
#include "UIManager.hpp"
#include "cbdos/language.hpp"
#include "cbdos_build_profile.h"

// Vistas del sistema
#include "views/ConfigView.hpp"
#include "views/MusicPlayerView.hpp"
#include "views/AudioRecorderView.hpp"
#include "views/GalleryListView.hpp"
#include "views/RadioView.hpp"
#include "views/LuaRunnerView.hpp"
#include "views/TextEditorView.hpp"
#include "views/FileManagerView.hpp"
#include "views/HidView.hpp"
#include "views/UtilitiesView.hpp"
#include "views/TlvBrowserView.hpp"
#include "views/LuappView.hpp"
#include "views/LottieTestView.hpp"
#include "views/MeshCoreView.hpp"
#include "../apps/kerberos/KerberosView.hpp"
#include "../lua/LuappManager.hpp"

#if CBDOS_FEATURE_FLASHER
#include "views/FlasherView.hpp"
#endif
#if CBDOS_FEATURE_CARTRIDGE
#include "views/CartridgeView.hpp"
#endif
#if CBDOS_FEATURE_TERMINAL
#include "views/TerminalView.hpp"
#endif
#if CBDOS_FEATURE_LAN_RECON
#include "views/LanReconView.hpp"
#endif

#include <algorithm>

namespace cbdos {
namespace ui {

AppRegistry& AppRegistry::getInstance() {
    static AppRegistry instance;
    return instance;
}

void AppRegistry::initSystemApps() {
    if (m_initialized) return;
    m_initialized = true;

    using cbdos::lang::tr;
    using cbdos::lang::StrId;

    // 1. TlvBrowser
    registerApp({
        "browser",
        tr(StrId::STR_APP_BROWSER),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "browser", LV_SYMBOL_DIRECTORY },
        0x00E5FF,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<TlvBrowserView>()); }
    });

    // 2. Galería
    registerApp({
        "gallery",
        tr(StrId::STR_APP_GALLERY),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "gallery", LV_SYMBOL_IMAGE },
        0xF72585,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<GalleryListView>()); }
    });

    // 3. Administrador de Archivos
    registerApp({
        "files",
        tr(StrId::STR_APP_FILES),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "files", LV_SYMBOL_FILE },
        0xFFB703,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<FileManagerView>()); }
    });

    // 4. Utilidades
    registerApp({
        "utilities",
        tr(StrId::STR_APP_UTILITIES),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "utilities", LV_SYMBOL_SETTINGS },
        0x7209B7,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<UtilitiesView>()); }
    });

#if CBDOS_FEATURE_CARTRIDGE
    // 5. Motor de Cartuchos
    registerApp({
        "cartridge",
        tr(StrId::STR_APP_CARTRIDGE),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "cartridge", LV_SYMBOL_PLAY },
        0x4CC9F0,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<CartridgeView>()); }
    });
#endif

    // 6. Lua Runner
    registerApp({
        "lua",
        tr(StrId::STR_APP_LUA),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "lua", LV_SYMBOL_EDIT },
        0x4361EE,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>()); }
    });

    // 7. Editor de Texto
    registerApp({
        "editor",
        tr(StrId::STR_APP_EDITOR),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "editor", LV_SYMBOL_EDIT },
        0x3B82F6,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<TextEditorView>()); }
    });

    // 8. Radio Web
    registerApp({
        "radio",
        tr(StrId::STR_APP_RADIO),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "radio", LV_SYMBOL_AUDIO },
        0x8B5CF6,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<RadioView>()); }
    });

#if CBDOS_FEATURE_FLASHER
    // 9. Flasheador USB
    registerApp({
        "flasher",
        tr(StrId::STR_APP_FLASHER),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "flasher", LV_SYMBOL_DOWNLOAD },
        0xF59E0B,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<FlasherView>()); }
    });
#endif

#if CBDOS_FEATURE_TERMINAL
    // 10. Terminal Serie / SSH
    registerApp({
        "terminal",
        tr(StrId::STR_APP_TERMINAL),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "terminal", LV_SYMBOL_KEYBOARD },
        0x10B981,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<TerminalView>()); }
    });
#endif

    // 11. USB HID / BadUSB
    registerApp({
        "hid",
        tr(StrId::STR_APP_HID),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::LVGL_SYMBOL, "", LV_SYMBOL_KEYBOARD },
        0xFFD60A,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<HidView>()); }
    });

    // 12. MeshCore Companion
    registerApp({
        "meshcore",
        tr(StrId::STR_APP_MESHCORE),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::LVGL_SYMBOL, "", LV_SYMBOL_WIFI },
        0x00E5FF,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<MeshCoreView>()); }
    });

#if CBDOS_FEATURE_LAN_RECON
    // 13. Reconocimiento LAN
    registerApp({
        "recon",
        "LAN Recon",
        AppOrigin::SYSTEM_NATIVE,
        { IconType::LVGL_SYMBOL, "", LV_SYMBOL_WIFI },
        0x00F5D4,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<LanReconView>()); }
    });
#endif

    // 14. Grabadora de Audio
    registerApp({
        "recorder",
        tr(StrId::STR_APP_RECORDER),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "recorder", LV_SYMBOL_AUDIO },
        0xEF4444,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<AudioRecorderView>()); }
    });

    // 15. Reproductor de Música
    registerApp({
        "music",
        tr(StrId::STR_APP_MUSIC),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "music", LV_SYMBOL_AUDIO },
        0x00E5FF,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>()); }
    });

    // 16. Kerberos Vault
    registerApp({
        "kerberos",
        tr(StrId::STR_APP_KERBEROS),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::LVGL_SYMBOL, "", LV_SYMBOL_USB },
        0x10B981,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<KerberosView>()); }
    });

    // 17. Animaciones Lottie
    registerApp({
        "lottie",
        tr(StrId::STR_APP_LOTTIE),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::LVGL_SYMBOL, "", LV_SYMBOL_IMAGE },
        0x06D6A0,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<LottieTestView>()); }
    });

    // 18. Configuración del Sistema
    registerApp({
        "config",
        tr(StrId::STR_APP_CONFIG),
        AppOrigin::SYSTEM_NATIVE,
        { IconType::SYSTEM_BUILTIN, "config", LV_SYMBOL_SETTINGS },
        0x9D4EDD,
        true,
        []() { UIManager::getInstance().pushView(std::make_shared<ConfigView>()); }
    });
}

void AppRegistry::registerApp(const AppDescriptor& app) {
    auto it = std::find_if(m_apps.begin(), m_apps.end(), [&app](const AppDescriptor& existing) {
        return existing.id == app.id;
    });

    if (it != m_apps.end()) {
        *it = app;
    } else {
        m_apps.push_back(app);
    }
}

void AppRegistry::unregisterApp(const std::string& id) {
    m_apps.erase(
        std::remove_if(m_apps.begin(), m_apps.end(), [&id](const AppDescriptor& app) {
            return app.id == id;
        }),
        m_apps.end()
    );
}

const AppDescriptor* AppRegistry::findApp(const std::string& id) const {
    auto it = std::find_if(m_apps.begin(), m_apps.end(), [&id](const AppDescriptor& app) {
        return app.id == id;
    });
    if (it != m_apps.end()) {
        return &(*it);
    }
    return nullptr;
}

void AppRegistry::rescanExternalApps() {
    // 1. Eliminar apps dinámicas previas
    m_apps.erase(
        std::remove_if(m_apps.begin(), m_apps.end(), [](const AppDescriptor& app) {
            return app.origin == AppOrigin::LUA_SCRIPT;
        }),
        m_apps.end()
    );

    // 2. Escanear /sdcard/apps con LuappManager
    auto& luappMgr = cbdos::lua::LuappManager::getInstance();
    luappMgr.scanApps("/sdcard/apps");

    for (const auto& luaApp : luappMgr.getDiscoveredApps()) {
        AppDescriptor desc;
        desc.id = "luapp_" + luaApp.name;
        desc.title = luaApp.name;
        desc.origin = AppOrigin::LUA_SCRIPT;
        desc.accentColor = luaApp.accentColor;
        desc.visibleInDashboard = true;

        // Por ahora símbolo LVGL, en Fase 3/4 añadiremos soporte de icono FS
        desc.icon = { IconType::LVGL_SYMBOL, "", luaApp.iconSymbol };

        std::string filePath = luaApp.filePath;
        std::string title = luaApp.name;
        std::string iconSymbol = luaApp.iconSymbol;
        desc.launchAction = [filePath, title, iconSymbol]() {
            UIManager::getInstance().pushView(std::make_shared<LuappView>(filePath, title, iconSymbol));
        };

        registerApp(desc);
    }
}

void AppRegistry::clear() {
    m_apps.clear();
    m_initialized = false;
}

} // namespace ui
} // namespace cbdos
