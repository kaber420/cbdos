#include "UIManager.hpp"
#include "views/DashboardView.hpp"
#include "WallpaperManager.h"
#include "themes/DefaultTheme.h"
#include "assets/SystemIcons.hpp"
#include "cbdos/system.hpp"
#include "PowerManager.hpp"
#include "../../lua/LuaBridge.hpp"

namespace cbdos {

namespace ui {

static const char* TAG = "UIManager";

bool init() {
    return UIManager::getInstance().init();
}

void update() {
    UIManager::getInstance().update();
}

void openDashboard() {
    UIManager::getInstance().openDashboard();
}

void toggleQuickSettings() {
    UIManager::getInstance().toggleQuickSettings();
}

bool isQuickSettingsOpen() {
    return UIManager::getInstance().isQuickSettingsOpen();
}

void showNotification(const char* message, uint32_t durationMs) {
    UIManager::getInstance().showNotification(message, durationMs);
}

UIManager& UIManager::getInstance() {
    static UIManager instance;
    return instance;
}

UIManager::UIManager()
    : m_rootScreen(nullptr),
      m_contentContainer(nullptr),
      m_initialized(false) {
}

bool UIManager::init(lv_obj_t* rootScreen) {
    if (m_initialized) return true;

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Inicializando UIManager...");

    // 1. Inicializar ThemeEngine y SystemIcons
    ThemeEngine::getInstance().init();
    SystemIcons::init();
    const auto& palette = ThemeEngine::getInstance().getPalette();

    // 2. Pantalla Raíz
    m_rootScreen = rootScreen ? rootScreen : lv_screen_active();
    if (!m_rootScreen) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "No se encontró pantalla activa en LVGL");
        return false;
    }

    lv_obj_set_style_bg_color(m_rootScreen, lv_color_hex(palette.bg), 0);
    lv_obj_set_style_bg_opa(m_rootScreen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(m_rootScreen, 0, 0);
    // Texto base heredable: evita negro-sobre-oscuro en labels sin estilo propio
    lv_obj_set_style_text_color(m_rootScreen, lv_color_hex(palette.textPrimary), 0);
    lv_obj_remove_flag(m_rootScreen, LV_OBJ_FLAG_SCROLLABLE);

    // 3. Aplicar Fondo de Pantalla (Wallpaper de fábrica o personalizado)
    WallpaperManager::getInstance().init();
    WallpaperManager::getInstance().applyWallpaper(m_rootScreen);

    // 4. Crear HeaderBar fija/flotante en la parte superior
    if (!m_headerBar.init(m_rootScreen)) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando HeaderBar");
        return false;
    }
    m_headerBar.setOnClickCallback([this]() {
        this->toggleQuickSettings();
    });

    // 5. Crear Contenedor de Contenido (Debajo de la HeaderBar flotante de 44px + margen)
    m_contentContainer = lv_obj_create(m_rootScreen);
    lv_obj_set_pos(m_contentContainer, 0, 58);
    lv_obj_set_width(m_contentContainer, lv_pct(100));

    int32_t screenHeight = 800;
    lv_display_t* disp = lv_display_get_default();
    if (disp) {
        screenHeight = lv_display_get_vertical_resolution(disp);
    } else if (m_rootScreen) {
        screenHeight = lv_obj_get_height(m_rootScreen);
        if (screenHeight <= 0) screenHeight = 800;
    }
    lv_obj_set_height(m_contentContainer, screenHeight - 58);
    lv_obj_set_style_bg_opa(m_contentContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_contentContainer, 0, 0);
    lv_obj_set_style_pad_all(m_contentContainer, 0, 0);
    lv_obj_set_style_radius(m_contentContainer, 0, 0);
    // Propaga texto claro a todas las vistas hijas
    lv_obj_set_style_text_color(m_contentContainer, lv_color_hex(palette.textPrimary), 0);
    lv_obj_remove_flag(m_contentContainer, LV_OBJ_FLAG_SCROLLABLE);

    // 6. Suscribir a cambios de tema
    ThemeEngine::getInstance().registerCallback([this](cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& pal) {
        this->onThemeChanged(theme, pal);
    });

    // 7. Inicializar gestor de energía
    cbdos::system::PowerManager::getInstance().init();

    // 8. Cargar vista inicial directa (Dashboard)
    openDashboard();

    m_initialized = true;
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "UIManager inicializado exitosamente.");
    return true;
}

void UIManager::update() {
    if (!m_initialized) return;

    cbdos::system::PowerManager::getInstance().update();

    if (LuaBridge::checkAndClearNeedsRefresh()) {
        lv_obj_t* scr = lv_screen_active();
        if (scr && lv_obj_is_valid(scr)) {
            lv_obj_invalidate(scr);
            lv_refr_now(NULL);
        }
    }

    // Actualizar barra de estado
    m_headerBar.update();

    // Actualizar vista activa
    auto currentView = getCurrentView();
    if (currentView && currentView->isVisible()) {
        currentView->onUpdate();
    }
}

static lv_obj_t* s_activeKeyboard = nullptr;

void UIManager::closeKeyboard() {
    if (s_activeKeyboard && lv_obj_is_valid(s_activeKeyboard)) {
        lv_keyboard_set_textarea(s_activeKeyboard, nullptr);
        lv_obj_t* kb = s_activeKeyboard;
        s_activeKeyboard = nullptr;
        lv_obj_delete_async(kb);
    } else {
        s_activeKeyboard = nullptr;
    }
}

static void kb_event_cb(lv_event_t* ev) {
    lv_event_code_t c = lv_event_get_code(ev);
    lv_obj_t* kb = (lv_obj_t*)lv_event_get_target(ev);
    if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) {
        UIManager::closeKeyboard();
    } else if (c == LV_EVENT_DELETE) {
        if (s_activeKeyboard == kb) {
            s_activeKeyboard = nullptr;
        }
    }
}

static void ta_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* targetTa = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        if (!s_activeKeyboard || !lv_obj_is_valid(s_activeKeyboard)) {
            lv_obj_t* topLayer = lv_layer_top();
            s_activeKeyboard = lv_keyboard_create(topLayer);
            lv_obj_set_size(s_activeKeyboard, lv_pct(100), lv_pct(45));
            lv_obj_align(s_activeKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(s_activeKeyboard, lv_color_hex(0x11131A), 0);
            lv_obj_set_style_border_color(s_activeKeyboard, lv_color_hex(0x2E3444), 0);
            lv_obj_set_style_border_width(s_activeKeyboard, 1, 0);
            lv_obj_set_style_text_color(s_activeKeyboard, lv_color_white(), LV_PART_ITEMS);
            lv_obj_set_style_bg_color(s_activeKeyboard, lv_color_hex(0x1E2230), LV_PART_ITEMS);
            lv_obj_set_style_border_color(s_activeKeyboard, lv_color_hex(0x3B4252), LV_PART_ITEMS);
            lv_obj_set_style_border_width(s_activeKeyboard, 1, LV_PART_ITEMS);
            lv_obj_set_style_radius(s_activeKeyboard, 6, LV_PART_ITEMS);
            lv_obj_set_style_bg_color(s_activeKeyboard, lv_color_hex(0x00F5D4), (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);
            lv_obj_set_style_text_color(s_activeKeyboard, lv_color_hex(0x0F172A), (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(s_activeKeyboard, lv_color_hex(0x2E3444), (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
            lv_obj_set_style_text_color(s_activeKeyboard, lv_color_white(), (lv_style_selector_t)LV_PART_ITEMS | (lv_style_selector_t)LV_STATE_CHECKED);
            lv_obj_add_event_cb(s_activeKeyboard, kb_event_cb, LV_EVENT_ALL, NULL);
        } else {
            lv_obj_remove_flag(s_activeKeyboard, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_to_index(s_activeKeyboard, -1);
        }
        lv_keyboard_set_textarea(s_activeKeyboard, targetTa);
    } else if (code == LV_EVENT_DELETE) {
        if (s_activeKeyboard && lv_obj_is_valid(s_activeKeyboard)) {
            if (lv_keyboard_get_textarea(s_activeKeyboard) == targetTa) {
                UIManager::closeKeyboard();
            }
        }
    }
}

void UIManager::attachKeyboard(lv_obj_t* ta) {
    if (!ta) return;
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, NULL);
}

void UIManager::pushView(std::shared_ptr<BaseView> view) {
    if (!view) return;

    closeKeyboard();
    m_headerBar.clearRightAction();
    m_headerBar.showWifi(true);

    if (!m_viewStack.empty()) {
        m_viewStack.back()->onHide();
    }

    m_viewStack.push_back(view);
    view->onCreate(m_contentContainer);
    view->onShow();

    if (m_viewStack.size() > 1) {
        m_headerBar.setTitle(view->getViewName().c_str());
        m_headerBar.showBackButton(true, [this]() {
            this->popView();
        });
    } else {
        m_headerBar.setTitle("CBDos");
        m_headerBar.showBackButton(false);
    }
}

void UIManager::popView() {
    if (m_viewStack.size() <= 1) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "No se puede hacer pop de la vista raíz");
        return;
    }

    closeKeyboard();
    m_headerBar.clearRightAction();
    m_headerBar.showWifi(true);

    auto topView = m_viewStack.back();
    topView->onHide();
    topView->onDestroy();
    m_viewStack.pop_back();

    if (!m_viewStack.empty()) {
        auto current = m_viewStack.back();
        current->onShow();
        if (m_viewStack.size() == 1) {
            m_headerBar.setTitle("CBDos");
            m_headerBar.showBackButton(false);
        } else {
            m_headerBar.setTitle(current->getViewName().c_str());
            m_headerBar.showBackButton(true, [this]() {
                this->popView();
            });
        }
    }
}

void UIManager::switchView(std::shared_ptr<BaseView> view) {
    if (!view) return;

    closeKeyboard();

    while (!m_viewStack.empty()) {
        auto v = m_viewStack.back();
        v->onHide();
        v->onDestroy();
        m_viewStack.pop_back();
    }

    pushView(view);
}

std::shared_ptr<BaseView> UIManager::getCurrentView() const {
    if (m_viewStack.empty()) return nullptr;
    return m_viewStack.back();
}

void UIManager::openDashboard() {
    closeKeyboard();
    if (m_headerBar.getContainer() && lv_obj_is_valid(m_headerBar.getContainer())) {
        lv_obj_remove_flag(m_headerBar.getContainer(), LV_OBJ_FLAG_HIDDEN);
    }
    switchView(std::make_shared<DashboardView>());
}

void UIManager::showNotification(const char* message, uint32_t durationMs) {
    if (!message || !m_rootScreen) return;

    const auto& palette = ThemeEngine::getInstance().getPalette();

    // Toast flotante animado
    lv_obj_t* toast = lv_obj_create(m_rootScreen);
    lv_obj_set_size(toast, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(toast, lv_color_hex(palette.panel), 0);
    lv_obj_set_style_border_color(toast, lv_color_hex(palette.primary), 0);
    lv_obj_set_style_border_width(toast, 1, 0);
    lv_obj_set_style_radius(toast, 8, 0);
    lv_obj_set_style_pad_all(toast, 10, 0);

    lv_obj_t* lbl = lv_label_create(toast);
    lv_label_set_text(lbl, message);
    lv_obj_set_style_text_color(lbl, lv_color_hex(palette.textPrimary), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    // Auto-destrucción tras timeout
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, toast);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_delay(&a, durationMs);
    lv_anim_set_exec_cb(&a, [](void* var, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(var), (lv_opa_t)v, 0);
    });
    lv_anim_set_completed_cb(&a, [](lv_anim_t* anim) {
        auto* obj = static_cast<lv_obj_t*>(anim->var);
        if (obj && lv_obj_is_valid(obj)) {
            lv_obj_delete(obj);
        }
    });
    lv_anim_start(&a);
}

void UIManager::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_rootScreen && lv_obj_is_valid(m_rootScreen)) {
        lv_obj_set_style_bg_color(m_rootScreen, lv_color_hex(palette.bg), 0);
        lv_obj_set_style_text_color(m_rootScreen, lv_color_hex(palette.textPrimary), 0);
    }
    if (m_contentContainer && lv_obj_is_valid(m_contentContainer)) {
        lv_obj_set_style_text_color(m_contentContainer, lv_color_hex(palette.textPrimary), 0);
    }

    m_headerBar.onThemeChanged(theme, palette);

    for (auto& view : m_viewStack) {
        if (view) {
            view->onThemeChanged(theme, palette);
        }
    }
}

} // namespace ui
} // namespace cbdos
