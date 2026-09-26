#pragma once

#include <array>
#include <cstddef>

namespace cbdos {
namespace board {

// Pines de expansion disponibles para JC3248W535 (ESP32-S3)
constexpr size_t NUM_EXPANSION_PINS = 10;
constexpr std::array<int, NUM_EXPANSION_PINS> EXPANSION_PINS = {
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19
};

// Pines asignados a perifericos del sistema base (LCD, Touch, Audio)
constexpr size_t NUM_SYSTEM_PINS = 13;
constexpr std::array<int, NUM_SYSTEM_PINS> SYSTEM_PINS = {
    1, 2, 3, 4, 8, 21, 39, 40, 41, 42, 45, 47, 48
};

} // namespace board
} // namespace cbdos
