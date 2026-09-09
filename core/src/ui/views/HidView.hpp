#pragma once
#include "BaseView.hpp"
#include <lvgl.h>
#include <string>

namespace cbdos {
namespace ui {

class HidView : public BaseView {
public:
    HidView();
    ~HidView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onThemeChanged(cbdos::theme::ThemeType theme,
                        const cbdos::theme::ThemePalette& palette) override;

private:
    void buildStatusBar(lv_obj_t* parent);
    void buildTabs(lv_obj_t* parent);
    void buildKeyboardTab(lv_obj_t* tab);
    void buildTouchpadTab(lv_obj_t* tab);
    void buildStreamDeckTab(lv_obj_t* tab);
    void refreshStatus();

    static void enableSwitchCb(lv_event_t* e);
    static void kbHidCb(lv_event_t* e);
    static void clearTextCb(lv_event_t* e);
    static void quickKeyCb(lv_event_t* e);
    static void mouseBtnCb(lv_event_t* e);
    static void padEventCb(lv_event_t* e);
    static void wheelCb(lv_event_t* e);
    static void deckBtnCb(lv_event_t* e);
    static void statusTimerCb(lv_timer_t* t);

    lv_obj_t* m_tabview = nullptr;
    lv_obj_t* m_statusLabel = nullptr;
    lv_obj_t* m_enableSwitch = nullptr;
    lv_obj_t* m_textarea = nullptr;
    lv_obj_t* m_keyboard = nullptr;
    lv_obj_t* m_pad = nullptr;
    lv_timer_t* m_timer = nullptr;

    int32_t m_lastPadX = -1;
    int32_t m_lastPadY = -1;
    int32_t m_accumX = 0;
    int32_t m_accumY = 0;
};

}  // namespace ui
}  // namespace cbdos
