// AUTO-GENERATED FILE. DO NOT EDIT.
// Generated from jc4880p443.json
// Board: JC4880P443C (v1.3)

#pragma once

#include <array>
#include <cstddef>

namespace cbdos {
namespace board {

// Pines de expansion disponibles (Cabecera JP1)
constexpr size_t NUM_EXPANSION_PINS = 11;
constexpr std::array<int, NUM_EXPANSION_PINS> EXPANSION_PINS = {
    28, 29, 30, 31, 32, 33, 34, 49, 50, 51, 52
};

// Pines asignados a perifericos del sistema base
constexpr size_t NUM_SYSTEM_PINS = 30;
constexpr std::array<int, NUM_SYSTEM_PINS> SYSTEM_PINS = {
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 35, 37, 38, 39, 40, 41, 42, 43, 44, 48, 53, 54
};

// 1. Pantalla
namespace display {
    constexpr const char* DRIVER = "st7701s";
    constexpr int WIDTH = 480;
    constexpr int HEIGHT = 800;
    constexpr int FPS = 60;
    constexpr int PIN_RST = 5;
    constexpr int PIN_BL = 23;
}

// 2. Touchscreen (I2C)
namespace touch {
    constexpr int PIN_SDA = 7;
    constexpr int PIN_SCL = 8;
    constexpr int PIN_RST = 22;
    constexpr int PIN_INT = 21;
}

// 3. Audio (I2S + Codec)
namespace audio {
    constexpr int PIN_MCLK = 13;
    constexpr int PIN_BCLK = 12;
    constexpr int PIN_WS = 10;
    constexpr int PIN_DOUT = 9;
    constexpr int PIN_DIN = 48;
    constexpr int PIN_PA = 11;
}

// 4. Tarjeta MicroSD (SDMMC 4-bit)
namespace sdcard {
    constexpr int PIN_D0 = 39;
    constexpr int PIN_D1 = 40;
    constexpr int PIN_D2 = 41;
    constexpr int PIN_D3 = 42;
    constexpr int PIN_CLK = 43;
    constexpr int PIN_CMD = 44;
}

// 5. Consola Serie (UART0)
namespace console {
    constexpr int PIN_TX = 38;
    constexpr int PIN_RX = 37;
}

// 6. Coprocesador ESP32-C6 (SDIO)
namespace coprocessor {
    constexpr size_t NUM_SDIO_PINS = 6;
    constexpr std::array<int, NUM_SDIO_PINS> PINS_SDIO = {14, 15, 16, 17, 18, 19};
    constexpr int PIN_RESET = 54;
    constexpr int PIN_HANDSHAKE = 6;
}

// 7. Sensores
namespace sensors {
    constexpr int PIN_BATTERY_ADC = 53;
}

// 8. Botones Fisicos
namespace buttons {
    constexpr int PIN_BOOT = 35;
}

} // namespace board
} // namespace cbdos
