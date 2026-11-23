#include "cbdos/system.hpp"
#include "cbdos/memory.hpp"
#include "cbdos/log.hpp"
#include "cbdos/rtos.hpp"
#include <driver/temperature_sensor.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_log.h>
#include <soc/lp_system_reg.h>
#include <soc/soc.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace cbdos {

namespace mem {
    void* alloc_psram(size_t size) {
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return ptr ? ptr : ::malloc(size);
    }
    void* realloc_psram(void* ptr, size_t size) {
        void* p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return p ? p : ::realloc(ptr, size);
    }
    void* alloc_dma(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    }
    void* alloc_internal(size_t size) {
        return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    void* realloc_internal(void* ptr, size_t size) {
        return heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    void free_mem(void* ptr) {
        if (ptr) ::free(ptr);
    }
}

namespace log {
    void write(LogLevel level, const char* tag, const char* format, ...) {
        esp_log_level_t esp_level = ESP_LOG_INFO;
        switch (level) {
            case LogLevel::Error:   esp_level = ESP_LOG_ERROR; break;
            case LogLevel::Warn:    esp_level = ESP_LOG_WARN;  break;
            case LogLevel::Info:    esp_level = ESP_LOG_INFO;  break;
            case LogLevel::Debug:   esp_level = ESP_LOG_DEBUG; break;
            case LogLevel::Verbose: esp_level = ESP_LOG_VERBOSE; break;
        }
        va_list args;
        va_start(args, format);
        esp_log_writev(esp_level, tag ? tag : "CBDos", format, args);
        va_end(args);
        printf("\n");
    }
}

namespace rtos {
    TaskHandle createTask(TaskFunction fn, const char* name, uint32_t stackSize, void* param, uint32_t priority, int coreId) {
        TaskHandle_t handle = nullptr;
        BaseType_t res;
        if (coreId >= 0) {
            res = xTaskCreatePinnedToCore((TaskFunction_t)fn, name, stackSize, param, priority, &handle, coreId);
        } else {
            res = xTaskCreate((TaskFunction_t)fn, name, stackSize, param, priority, &handle);
        }
        return (res == pdPASS) ? (TaskHandle)handle : nullptr;
    }

    void deleteTask(TaskHandle handle) {
        vTaskDelete((TaskHandle_t)handle);
    }

    void sleepMs(uint32_t ms) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    MutexHandle createMutex() {
        return (MutexHandle)xSemaphoreCreateMutex();
    }

    bool lockMutex(MutexHandle handle, uint32_t timeoutMs) {
        if (!handle) return false;
        TickType_t ticks = (timeoutMs == 0xFFFFFFFF) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        return xSemaphoreTake((SemaphoreHandle_t)handle, ticks) == pdTRUE;
    }

    void unlockMutex(MutexHandle handle) {
        if (handle) {
            xSemaphoreGive((SemaphoreHandle_t)handle);
        }
    }

    void deleteMutex(MutexHandle handle) {
        if (handle) {
            vSemaphoreDelete((SemaphoreHandle_t)handle);
        }
    }
}

namespace system {

uint32_t getTimeMs() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

uint64_t getTimeUs() {
    return (uint64_t)esp_timer_get_time();
}

void sleepMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void sleepUs(uint32_t us) {
    esp_rom_delay_us(us);
}

void yieldTask() {
    taskYIELD();
}

size_t getFreeHeap() {
    return heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
}

size_t getTotalHeap() {
    return heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
}



static temperature_sensor_handle_t s_temp_sensor = nullptr;
static bool s_temp_sensor_init = false;

static void init_temp_sensor_if_needed() {
    if (!s_temp_sensor_init) {
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
        if (temperature_sensor_install(&temp_sensor_config, &s_temp_sensor) == ESP_OK) {
            if (temperature_sensor_enable(s_temp_sensor) == ESP_OK) {
                s_temp_sensor_init = true;
            }
        }
    }
}

size_t getFreePsram() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

size_t getTotalPsram() {
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

float getCpuTemperature() {
    init_temp_sensor_if_needed();
    if (s_temp_sensor_init && s_temp_sensor) {
        float tsens_out = 0.0f;
        if (temperature_sensor_get_celsius(s_temp_sensor, &tsens_out) == ESP_OK) {
            return tsens_out;
        }
    }
    return 0.0f;
}

void restart() {
    esp_restart();
}

void restartToBootloader() {
    REG_SET_BIT(LP_SYSTEM_REG_SYS_CTRL_REG, LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}

void log(LogLevel level, const char* tag, const char* format, ...) {
    esp_log_level_t esp_level = ESP_LOG_INFO;
    switch (level) {
        case LogLevel::Debug: esp_level = ESP_LOG_DEBUG; break;
        case LogLevel::Info:  esp_level = ESP_LOG_INFO;  break;
        case LogLevel::Warn:  esp_level = ESP_LOG_WARN;  break;
        case LogLevel::Error: esp_level = ESP_LOG_ERROR; break;
    }
    
    va_list args;
    va_start(args, format);
    esp_log_writev(esp_level, tag ? tag : "CBDos", format, args);
    va_end(args);
    printf("\n");
}

} // namespace system
} // namespace cbdos
