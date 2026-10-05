#pragma once
#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class PowerConfigView : public BaseView {
public:
    PowerConfigView();
    ~PowerConfigView() override = default;

    bool onCreate(lv_obj_t* parent) override;

private:
    static void screen_off_btn_cb(lv_event_t* e);
    static void light_sleep_btn_cb(lv_event_t* e);
    static void deep_sleep_btn_cb(lv_event_t* e);
    static void restart_btn_cb(lv_event_t* e);
    static void bootloader_btn_cb(lv_event_t* e);
    static void timeout_dropdown_cb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
