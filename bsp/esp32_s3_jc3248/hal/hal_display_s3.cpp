#include "cbdos/display.hpp"
#include "cbdos_device_tree.h"
#include <Arduino.h>
#include <JC3248W535.h>
#include <lvgl.h>

static JC3248W535_Display s_display;
static uint8_t s_brightness = 70;
static cbdos::display::Orientation s_orientation = cbdos::display::Orientation::Portrait;

namespace cbdos {
namespace display {

bool init() {
    if (!s_display.begin()) {
        return false;
    }
    if (cbdos::board::display::PIN_BL >= 0) {
        pinMode(cbdos::board::display::PIN_BL, OUTPUT);
        analogWrite(cbdos::board::display::PIN_BL, (s_brightness * 255) / 100);
    }
    return true;
}

DisplayCaps getCapabilities() {
    DisplayCaps caps;
    if (isLandscape()) {
        caps.width = cbdos::board::display::HEIGHT;
        caps.height = cbdos::board::display::WIDTH;
    } else {
        caps.width = cbdos::board::display::WIDTH;
        caps.height = cbdos::board::display::HEIGHT;
    }
    caps.hasHardware2D = false;  // Render por software en S3
    caps.targetFps = cbdos::board::display::FPS;
    caps.isTouchSupported = true;
    return caps;
}

void setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_brightness = percent;
    if (cbdos::board::display::PIN_BL >= 0) {
        analogWrite(cbdos::board::display::PIN_BL, (percent * 255) / 100);
    }
}

uint8_t getBrightness() {
    return s_brightness;
}

void* getFramebuffer(int index) {
    (void)index;
    if (s_display.getCanvas()) {
        return (void*)s_display.getCanvas()->getFramebuffer();
    }
    return nullptr;
}

void flush() {
    s_display.flush();
}

bool lock(uint32_t timeout_ms) {
    (void)timeout_ms;
    return true;
}

void unlock() {
}

bool setOrientation(Orientation orientation) {
    s_orientation = orientation;
    lv_display_t* disp = lv_display_get_default();
    if (disp) {
        lv_display_rotation_t lv_rot = LV_DISPLAY_ROTATION_0;
        uint8_t drv_rot = 0;
        switch (orientation) {
            case Orientation::Landscape:
                lv_rot = LV_DISPLAY_ROTATION_90;
                drv_rot = 1;
                break;
            case Orientation::PortraitInverted:
                lv_rot = LV_DISPLAY_ROTATION_180;
                drv_rot = 2;
                break;
            case Orientation::LandscapeInverted:
                lv_rot = LV_DISPLAY_ROTATION_270;
                drv_rot = 3;
                break;
            case Orientation::Portrait:
            default:
                lv_rot = LV_DISPLAY_ROTATION_0;
                drv_rot = 0;
                break;
        }
        lv_display_set_rotation(disp, lv_rot);
        s_display.setRotation(drv_rot);
        lv_obj_t* scr = lv_screen_active();
        if (scr && lv_obj_is_valid(scr)) {
            lv_obj_invalidate(scr);
        }
    }
    return true;
}

Orientation getOrientation() {
    return s_orientation;
}

bool isLandscape() {
    return s_orientation == Orientation::Landscape || s_orientation == Orientation::LandscapeInverted;
}

} // namespace display
} // namespace cbdos

JC3248W535_Display& get_s3_display_driver() {
    return s_display;
}
