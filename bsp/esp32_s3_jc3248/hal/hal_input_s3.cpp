#include "cbdos/input.hpp"
#include <Arduino.h>
#include <JC3248W535.h>

static JC3248W535_Touch s_touch;

namespace cbdos {
namespace input {

bool init() {
    bool ok = s_touch.begin();
    if (!ok) {
        Serial.println("[TouchHAL-S3] Error: No se pudo comunicar con el controlador AXS15231B por I2C");
    } else {
        Serial.println("[TouchHAL-S3] Controlador táctil AXS15231B detectado correctamente");
    }
    return ok;
}

bool getTouch(TouchPoint& point) {
    ::TouchPoint tp;
    if (s_touch.read(tp) && tp.touched) {
        point.isPressed = true;
        point.x = tp.x;
        point.y = tp.y;
        point.fingerCount = 1;
        return true;
    }
    point.isPressed = false;
    point.x = 0;
    point.y = 0;
    point.fingerCount = 0;
    return false;
}

bool hasPhysicalKeyboard() {
    return false;
}

} // namespace input
} // namespace cbdos

JC3248W535_Touch& get_s3_touch_driver() {
    return s_touch;
}
