#pragma once
#include "cbdos/ui.hpp"
#include "ThemeEngine.hpp"
#include "components/HeaderBar.hpp"
#include "components/QuickSettingsPanel.hpp"
#include "views/BaseView.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <lvgl.h>

namespace cbdos {
namespace ui {

struct KeyboardOptions {
    bool autoCloseOnSubmit = true;
    std::function<void(const char*)> onSubmit = nullptr;
    lv_obj_t* nextFocusTarget = nullptr;
};

class UIManager {
public:
    static UIManager& getInstance();

    bool init(lv_obj_t* rootScreen = nullptr);
    void update();

    void pushView(std::shared_ptr<BaseView> view);
    void popView();
    void switchView(std::shared_ptr<BaseView> view);
    std::shared_ptr<BaseView> getCurrentView() const;

    void openDashboard();
    void toggleQuickSettings() { closeKeyboard(); QuickSettingsPanel::toggle(); }
    bool isQuickSettingsOpen() const { return QuickSettingsPanel::isOpen(); }
    void showNotification(const char* message, uint32_t durationMs = 3000);
    static void showToast(const char* message, uint32_t durationMs = 2500) { getInstance().showNotification(message, durationMs); }
    static void attachKeyboard(lv_obj_t* ta, const KeyboardOptions& opts = {});
    static void openKeyboard(lv_obj_t* ta);
    static void closeKeyboard();
    static lv_obj_t* getActiveKeyboard();

    HeaderBar& getHeaderBar() { return m_headerBar; }
    lv_obj_t* getContentContainer() const { return m_contentContainer; }

private:
    UIManager();
    ~UIManager() = default;
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette);

    lv_obj_t* m_rootScreen;
    lv_obj_t* m_contentContainer;
    HeaderBar m_headerBar;

    std::vector<std::shared_ptr<BaseView>> m_viewStack;
    bool m_initialized;
};

} // namespace ui
} // namespace cbdos
