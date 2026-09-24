#pragma once

#include <cstdint>
#include <esp_err.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_mipi_dsi.h>
#include "cbdos_device_tree.h"

#define BOARD_DISP_DSI_LANES   2
#define BOARD_DISP_DSI_LDO_CH  3
#define BOARD_DISP_SD_LDO_CH   4

class DisplayHAL {
public:
    static DisplayHAL& getInstance() {
        static DisplayHAL instance;
        return instance;
    }

    esp_err_t init(int h_res = cbdos::board::display::WIDTH, int v_res = cbdos::board::display::HEIGHT);
    
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return currentBrightness; }
    void turnOn();
    void turnOff();

    esp_lcd_panel_handle_t getPanelHandle() const { return panelHandle; }
    esp_lcd_dsi_bus_handle_t getDsiBusHandle() const { return dsiBusHandle; }

    void* getFrameBuffer(int index) const { return (index == 0) ? fb0 : fb1; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    DisplayHAL();
    ~DisplayHAL();

    DisplayHAL(const DisplayHAL&) = delete;
    DisplayHAL& operator=(const DisplayHAL&) = delete;

    esp_err_t initBacklight();
    esp_err_t initMipiDsi();

    esp_lcd_dsi_bus_handle_t dsiBusHandle = nullptr;
    esp_lcd_panel_io_handle_t ioHandle = nullptr;
    esp_lcd_panel_handle_t panelHandle = nullptr;

    void* fb0 = nullptr;
    void* fb1 = nullptr;

    int width = cbdos::board::display::WIDTH;
    int height = cbdos::board::display::HEIGHT;
    uint8_t currentBrightness = 70;
    bool initialized = false;
};
