// AUTO-GENERATED FILE. DO NOT EDIT.
// Generated from jc4880p443.json
// Board: JC4880P443C (v1.3)

#pragma once

#include <array>
#include <cstddef>

namespace cbdos {
namespace board {

// Hardware Firewall Whitelist (Allowed Pins)
constexpr size_t NUM_ALLOWED_PINS = 12;
constexpr std::array<int, NUM_ALLOWED_PINS> BOARD_ALLOWED_PINS = {
    28, 29, 30, 31, 32, 33, 34, 35, 49, 50, 51, 52
};

// Display Configuration
constexpr const char* DEVICE_TREE_DISPLAY_DRIVER = "st7701s";
constexpr int DEVICE_TREE_DISPLAY_WIDTH = 480;
constexpr int DEVICE_TREE_DISPLAY_HEIGHT = 800;
constexpr int DEVICE_TREE_DISPLAY_FPS = 60;
constexpr int DEVICE_TREE_DISPLAY_PIN_RST = 5;
constexpr int DEVICE_TREE_DISPLAY_PIN_BL = 23;

} // namespace board
} // namespace cbdos
