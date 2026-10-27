#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lvgl.h>

class LVGL_Port {
public:
    static LVGL_Port& getInstance() {
        static LVGL_Port instance;
        return instance;
    }

    esp_err_t init(int h_res = 480, int v_res = 800);

    bool lock(uint32_t timeout_ms = portMAX_DELAY);
    void unlock();

    lv_display_t* getDisplay() const { return display; }
    lv_indev_t* getIndev() const { return indev; }
    lv_indev_t* getKeyboardIndev() const { return kbIndev; }
    lv_group_t* getDefaultGroup() const { return defaultGroup; }

private:
    LVGL_Port();
    ~LVGL_Port();

    LVGL_Port(const LVGL_Port&) = delete;
    LVGL_Port& operator=(const LVGL_Port&) = delete;

    static void lvglTask(void* arg);
    static void flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    static void touchReadCallback(lv_indev_t* indev, lv_indev_data_t* data);
    static void keyboardReadCallback(lv_indev_t* indev, lv_indev_data_t* data);

    lv_display_t* display = nullptr;
    lv_indev_t* indev = nullptr;
    lv_indev_t* kbIndev = nullptr;
    lv_group_t* defaultGroup = nullptr;
    SemaphoreHandle_t lvglMux = nullptr;
    TaskHandle_t taskHandle = nullptr;

    void* buf1 = nullptr;
    void* buf2 = nullptr;

    int width = 480;
    int height = 800;
    bool running = false;
};
