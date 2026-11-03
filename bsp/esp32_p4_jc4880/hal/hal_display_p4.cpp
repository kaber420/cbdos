#include "cbdos/display.hpp"
#include "DisplayHAL.h"
#include "LVGL_Port.h"
#include <esp_cache.h>

namespace cbdos {
namespace display {

static uint8_t s_brightness = 70;
static int s_active_fb = 0;

bool init() {
    return (DisplayHAL::getInstance().init() == ESP_OK);
}

DisplayCaps getCapabilities() {
    DisplayCaps caps;
    if (isLandscape()) {
        caps.width = DisplayHAL::getInstance().getHeight();
        caps.height = DisplayHAL::getInstance().getWidth();
    } else {
        caps.width = DisplayHAL::getInstance().getWidth();
        caps.height = DisplayHAL::getInstance().getHeight();
    }
    caps.hasHardware2D = true;  // ESP32-P4 PPA / DMA2D
    caps.targetFps = 60;
    caps.isTouchSupported = true;
    return caps;
}

void setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_brightness = percent;
    DisplayHAL::getInstance().setBrightness(percent);
}

uint8_t getBrightness() {
    return DisplayHAL::getInstance().getBrightness();
}

void* getFramebuffer(int index) {
    return DisplayHAL::getInstance().getFrameBuffer(index);
}

void flush() {
    esp_lcd_panel_handle_t panel_handle = DisplayHAL::getInstance().getPanelHandle();
    if (panel_handle) {
        int w = DisplayHAL::getInstance().getWidth();
        int h = DisplayHAL::getInstance().getHeight();
        size_t fb_bytes = (size_t)w * h * sizeof(uint16_t);
        void* fb = DisplayHAL::getInstance().getFrameBuffer(s_active_fb);
        if (fb) {
            esp_cache_msync(fb, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, w, h, fb);
            s_active_fb = (s_active_fb + 1) % 2;
        }
    }
}

bool lock(uint32_t timeout_ms) {
    return LVGL_Port::getInstance().lock(timeout_ms);
}

void unlock() {
    LVGL_Port::getInstance().unlock();
}

bool setOrientation(Orientation orientation) {
    LVGL_Port::getInstance().setOrientation(orientation);
    return true;
}

Orientation getOrientation() {
    return LVGL_Port::getInstance().getOrientation();
}

bool isLandscape() {
    return LVGL_Port::getInstance().isLandscape();
}

} // namespace display
} // namespace cbdos
