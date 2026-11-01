#pragma once
#include <lvgl.h>

namespace cbdos {
namespace ui {

class QuickSettingsPanel {
public:
    static void toggle();
    static void hide();
    static bool isOpen() { return panelObj != nullptr && lv_obj_is_valid(panelObj); }

private:
    static lv_obj_t* panelObj;

    static void mask_click_cb(lv_event_t* e);
    static void volume_slider_cb(lv_event_t* e);
    static void brightness_slider_cb(lv_event_t* e);
    static void wifi_switch_cb(lv_event_t* e);
    static void ble_switch_cb(lv_event_t* e);
    static void radio_mode_dropdown_cb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
