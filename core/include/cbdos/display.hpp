#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace display {

struct DisplayCaps {
    uint16_t width;
    uint16_t height;
    bool hasHardware2D;     // true si cuenta con PPA / DMA2D / GPU 2D
    uint8_t targetFps;      // 60 en P4, 30 en S3
    bool isTouchSupported;
};

enum class Orientation : uint8_t {
    Portrait          = 0, // 0°: Vertical estándar (480x800 P4 / 320x480 S3)
    Landscape         = 1, // 90°: Horizontal panorámico (800x480 P4 / 480x320 S3)
    PortraitInverted  = 2, // 180°: Vertical invertido
    LandscapeInverted = 3  // 270°: Horizontal invertido
};

bool init();
DisplayCaps getCapabilities();
void setBrightness(uint8_t percent);
uint8_t getBrightness();
void* getFramebuffer(int index);
void flush();
bool lock(uint32_t timeout_ms = 100);
void unlock();

bool setOrientation(Orientation orientation);
Orientation getOrientation();
bool isLandscape();

} // namespace display
} // namespace cbdos
