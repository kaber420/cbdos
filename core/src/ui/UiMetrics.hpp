#pragma once

#include "cbdos/display.hpp"
#include <algorithm>
#include <cstdint>
#include <lvgl.h>

namespace cbdos {
namespace ui {

enum class WidthClass : uint8_t {
    Compact = 0, // < 480 px (ej. 320x480)
    Medium  = 1, // 480..799 px (ej. 480x800, 480x320)
    Wide    = 2  // >= 800 px (ej. 800x480, 1024x600)
};

enum class HeightClass : uint8_t {
    Short = 0,   // < 640 px (ej. 320, 480, 600)
    Tall  = 1    // >= 640 px (ej. 800, 1024)
};

struct UiMetrics {
    uint16_t width;
    uint16_t height;
    WidthClass widthClass;
    HeightClass heightClass;

    bool isCompact() const { return widthClass == WidthClass::Compact; }
    bool isMedium()  const { return widthClass == WidthClass::Medium; }
    bool isWide()    const { return widthClass == WidthClass::Wide; }
    bool isShort()   const { return heightClass == HeightClass::Short; }

    int32_t modalWidthPct() const {
        switch (widthClass) {
            case WidthClass::Compact: return 92;
            case WidthClass::Medium:  return 85;
            case WidthClass::Wide:    return 65;
        }
        return 85;
    }

    int32_t maxModalWidth(int32_t wideLimit = 520) const {
        return std::min<int32_t>(static_cast<int32_t>(width) - 24, wideLimit);
    }

    int32_t maxModalHeight(int32_t tallLimit = 640) const {
        return std::min<int32_t>(static_cast<int32_t>(height) - 24, tallLimit);
    }

    int32_t keyboardHeight() const {
        int32_t h = (static_cast<int32_t>(height) * 35) / 100;
        return std::clamp<int32_t>(h, 140, 260);
    }

    int32_t keyboardMaxWidth() const {
        return isWide() ? std::min<int32_t>(width, 820) : static_cast<int32_t>(width);
    }

    static UiMetrics current() {
        auto caps = cbdos::display::getCapabilities();
        uint16_t w = caps.width > 0 ? caps.width : 480;
        uint16_t h = caps.height > 0 ? caps.height : 800;

        UiMetrics m{};
        m.width = w;
        m.height = h;
        if (w < 480) {
            m.widthClass = WidthClass::Compact;
        } else if (w < 800) {
            m.widthClass = WidthClass::Medium;
        } else {
            m.widthClass = WidthClass::Wide;
        }
        m.heightClass = (h < 640) ? HeightClass::Short : HeightClass::Tall;
        return m;
    }
};

/**
 * @brief Aplica ancho y alto adaptativo con límites máximos seguros a un modal LVGL.
 */
inline void applyAdaptiveModalSize(lv_obj_t* modal, const UiMetrics& m,
                                   int32_t maxW = 520, int32_t maxH = 640) {
    if (!modal) return;
    lv_obj_set_width(modal, lv_pct(m.modalWidthPct()));
    lv_obj_set_style_max_width(modal, m.maxModalWidth(maxW), 0);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(modal, m.maxModalHeight(maxH), 0);
}

} // namespace ui
} // namespace cbdos
