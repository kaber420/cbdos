#pragma once
#include <cstdint>

namespace cbdos {
namespace input {

struct TouchPoint {
    bool isPressed;
    uint16_t x;
    uint16_t y;
    uint8_t fingerCount;
};

bool init();
bool getTouch(TouchPoint& point);
bool hasPhysicalKeyboard();

} // namespace input
} // namespace cbdos
