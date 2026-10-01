#include "cbdos/system.hpp"
#include "cbdos/rtos.hpp"
#include "cbdos/memory.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>

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
    if (ptr) {
        heap_caps_free(ptr);
    }
}

} // namespace mem
namespace system {

uint32_t getTimeMs() {
    return millis();
}

uint64_t getTimeUs() {
    return micros();
}

void sleepMs(uint32_t ms) {
    delay(ms);
}

void sleepUs(uint32_t us) {
    delayMicroseconds(us);
}

void yieldTask() {
    yield();
}

size_t getFreeHeap() {
    return ESP.getFreeHeap();
}

size_t getTotalHeap() {
    return ESP.getHeapSize();
}

size_t getFreePsram() {
    return ESP.getFreePsram();
}

size_t getTotalPsram() {
    return ESP.getPsramSize();
}

float getCpuTemperature() {
    return temperatureRead();
}

void restart() {
    ESP.restart();
}

void log(LogLevel level, const char* tag, const char* format, ...) {
    const char* lvlStr = "INFO";
    switch (level) {
        case LogLevel::Debug: lvlStr = "DEBUG"; break;
        case LogLevel::Info:  lvlStr = "INFO";  break;
        case LogLevel::Warn:  lvlStr = "WARN";  break;
        case LogLevel::Error: lvlStr = "ERROR"; break;
    }
    
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    Serial.printf("[%s] [%s] %s\n", lvlStr, tag ? tag : "CBDos", buf);
}

} // namespace system

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

} // namespace rtos
} // namespace cbdos
