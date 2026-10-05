#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace cbdos {
namespace system {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

uint32_t getTimeMs();
uint64_t getTimeUs();
void sleepMs(uint32_t ms);
void sleepUs(uint32_t us);
void yieldTask();
size_t getFreeHeap();
size_t getTotalHeap();
size_t getFreePsram();
size_t getTotalPsram();
float getCpuTemperature();
void restart();
void restartToBootloader();
void log(LogLevel level, const char* tag, const char* format, ...);

} // namespace system
} // namespace cbdos
