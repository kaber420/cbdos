import sys
import json
import os

def main():
    if len(sys.argv) < 3:
        print("Usage: python cdtc.py <input.json> <output.h>")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    with open(input_file, 'r') as f:
        data = json.load(f)
        
    board_name = data.get("board", "Unknown")
    version = data.get("version", "Unknown")
    devices = data.get("devices", {})
    
    # 1. Extraer pines de expansion (JP1)
    expansion_pins = sorted(list(set(data.get("expansion", {}).get("jp1_allowed", []))))
    
    # 2. Recolectar todos los pines del sistema base
    system_pins = []
    for dev_name, dev_config in devices.items():
        pins = dev_config.get("pins", {})
        for p_name, p_val in pins.items():
            if isinstance(p_val, int) and p_val >= 0:
                system_pins.append(p_val)
            elif isinstance(p_val, list):
                system_pins.extend([p for p in p_val if isinstance(p, int) and p >= 0])
    system_pins = sorted(list(set(system_pins)))
    
    # 3. Datos especificos de cada periferico
    disp = devices.get("display", {})
    disp_driver = disp.get("driver", "unknown")
    disp_res = disp.get("resolution", [480, 800])
    disp_fps = disp.get("fps", 60)
    disp_rst = disp.get("pins", {}).get("rst", -1)
    disp_bl = disp.get("pins", {}).get("bl", -1)
    
    touch = devices.get("touch_i2c0", {}).get("pins", {})
    audio = devices.get("audio_i2s", {}).get("pins", {})
    sdmmc = devices.get("sdmmc", {}).get("pins", {})
    console = devices.get("console_uart0", {}).get("pins", {})
    power = devices.get("power_control", {}).get("pins", {})
    c6 = devices.get("coprocessor_c6", {}).get("pins", {})
    battery = devices.get("battery_sensor", {}).get("pins", {})
    buttons = devices.get("system_buttons", {}).get("pins", {})
    
    c6_sdio = c6.get("sdio", [14, 15, 16, 17, 18, 19])
    
    header_content = f"""// AUTO-GENERATED FILE. DO NOT EDIT.
// Generated from {os.path.basename(input_file)}
// Board: {board_name} (v{version})

#pragma once

#include <array>
#include <cstddef>

namespace cbdos {{
namespace board {{

// Pines de expansion disponibles (Cabecera JP1)
constexpr size_t NUM_EXPANSION_PINS = {len(expansion_pins)};
constexpr std::array<int, NUM_EXPANSION_PINS> EXPANSION_PINS = {{
    {', '.join(map(str, expansion_pins))}
}};

// Pines asignados a perifericos del sistema base
constexpr size_t NUM_SYSTEM_PINS = {len(system_pins)};
constexpr std::array<int, NUM_SYSTEM_PINS> SYSTEM_PINS = {{
    {', '.join(map(str, system_pins))}
}};

// 1. Pantalla
namespace display {{
    constexpr const char* DRIVER = "{disp_driver}";
    constexpr int WIDTH = {disp_res[0]};
    constexpr int HEIGHT = {disp_res[1]};
    constexpr int FPS = {disp_fps};
    constexpr int PIN_RST = {disp_rst};
    constexpr int PIN_BL = {disp_bl};
}}

// 2. Touchscreen (I2C)
namespace touch {{
    constexpr int PIN_SDA = {touch.get('sda', -1)};
    constexpr int PIN_SCL = {touch.get('scl', -1)};
    constexpr int PIN_RST = {touch.get('rst', -1)};
    constexpr int PIN_INT = {touch.get('int', -1)};
}}

// 3. Audio (I2S + Codec)
namespace audio {{
    constexpr int PIN_MCLK = {audio.get('mclk', -1)};
    constexpr int PIN_BCLK = {audio.get('bclk', -1)};
    constexpr int PIN_WS = {audio.get('ws', -1)};
    constexpr int PIN_DOUT = {audio.get('dout', -1)};
    constexpr int PIN_DIN = {audio.get('din', -1)};
    constexpr int PIN_PA = {audio.get('pa', -1)};
}}

// 4. Tarjeta MicroSD (SDMMC 4-bit)
namespace sdcard {{
    constexpr int PIN_D0 = {sdmmc.get('d0', -1)};
    constexpr int PIN_D1 = {sdmmc.get('d1', -1)};
    constexpr int PIN_D2 = {sdmmc.get('d2', -1)};
    constexpr int PIN_D3 = {sdmmc.get('d3', -1)};
    constexpr int PIN_CLK = {sdmmc.get('clk', -1)};
    constexpr int PIN_CMD = {sdmmc.get('cmd', -1)};
}}

// 5. Consola Serie (UART0)
namespace console {{
    constexpr int PIN_TX = {console.get('tx', -1)};
    constexpr int PIN_RX = {console.get('rx', -1)};
}}

// 6. Control de Alimentacion
namespace power {{
    constexpr int PIN_EN = {power.get('en', -1)};
}}

// 7. Coprocesador ESP32-C6 (SDIO)
namespace coprocessor {{
    constexpr size_t NUM_SDIO_PINS = {len(c6_sdio)};
    constexpr std::array<int, NUM_SDIO_PINS> PINS_SDIO = {{{', '.join(map(str, c6_sdio))}}};
    constexpr int PIN_RESET = {c6.get('reset', -1)};
    constexpr int PIN_HANDSHAKE = {c6.get('handshake', -1)};
}}

// 8. Sensores
namespace sensors {{
    constexpr int PIN_BATTERY_ADC = {battery.get('bat_adc', -1)};
}}

// 9. Botones Fisicos
namespace buttons {{
    constexpr int PIN_BOOT = {buttons.get('boot', -1)};
}}

}} // namespace board
}} // namespace cbdos
"""

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    with open(output_file, 'w') as f:
        f.write(header_content)
        
    print(f"Successfully generated {output_file} ({len(expansion_pins)} expansion pins, {len(system_pins)} system pins).")

if __name__ == "__main__":
    main()
