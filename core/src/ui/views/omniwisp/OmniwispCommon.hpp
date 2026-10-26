#pragma once

#include "cbdos/omniwisp_models.hpp"
#include "cbdos/airos_client.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

// Paleta de colores estándar para OmniWISP
namespace omni_colors {
    static constexpr uint32_t BG_DARK      = 0x07090E;
    static constexpr uint32_t CARD_BG      = 0x0E131F;
    static constexpr uint32_t BORDER       = 0x1E293B;
    static constexpr uint32_t TEAL_ACCENT  = 0x00F5D4;
    static constexpr uint32_t TEXT_MUTED   = 0x94A3B8;
    static constexpr uint32_t ALERT_RED    = 0xEF4444;
    static constexpr uint32_t WARN_YELLOW  = 0xF59E0B;
    static constexpr uint32_t HEALTH_GREEN = 0x10B981;
}

} // namespace ui
} // namespace cbdos
