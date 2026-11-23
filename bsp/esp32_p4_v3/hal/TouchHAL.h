#pragma once

#include <cstdint>
#include <esp_err.h>
#include <esp_lcd_touch.h>

#include <driver/i2c_master.h>
#include "cbdos_device_tree.h"

class TouchHAL {
public:
    static TouchHAL& getInstance() {
        static TouchHAL instance;
        return instance;
    }

    esp_err_t init(int h_res = cbdos::board::display::WIDTH, int v_res = cbdos::board::display::HEIGHT);
    esp_lcd_touch_handle_t getTouchHandle() const { return touchHandle; }
    i2c_master_bus_handle_t getI2cBusHandle() const { return i2cBusHandle; }

    bool read(uint16_t* x, uint16_t* y, bool* pressed);

private:
    TouchHAL();
    ~TouchHAL();

    TouchHAL(const TouchHAL&) = delete;
    TouchHAL& operator=(const TouchHAL&) = delete;

    i2c_master_bus_handle_t i2cBusHandle = nullptr;
    esp_lcd_touch_handle_t touchHandle = nullptr;
    int width = cbdos::board::display::WIDTH;
    int height = cbdos::board::display::HEIGHT;
    bool initialized = false;
};
