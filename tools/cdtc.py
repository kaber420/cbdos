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
        
    # Extraer pines permitidos (Whitelist)
    allowed_pins = data.get("expansion", {}).get("jp1_allowed", [])
    allowed_pins = sorted(list(set(allowed_pins)))
    
    # Extraer configuracion de pantalla y pines de sistema
    display = data.get("devices", {}).get("display", {})
    disp_driver = display.get("driver", "unknown")
    disp_res = display.get("resolution", [0, 0])
    disp_fps = display.get("fps", 0)
    disp_rst = display.get("pins", {}).get("rst", -1)
    disp_bl = display.get("pins", {}).get("bl", -1)
    
    # Recolectar todos los pines del sistema
    system_locked_pins = []
    for dev_name, dev_config in data.get("devices", {}).items():
        pins = dev_config.get("pins", {})
        for pin_name, pin_val in pins.items():
            if isinstance(pin_val, int) and pin_val >= 0:
                system_locked_pins.append(pin_val)
            elif isinstance(pin_val, list):
                system_locked_pins.extend([p for p in pin_val if isinstance(p, int) and p >= 0])
                
    system_locked_pins = sorted(list(set(system_locked_pins)))
    
    # Generar el archivo C++
    header_content = f"""// AUTO-GENERATED FILE. DO NOT EDIT.
// Generated from {os.path.basename(input_file)}
// Board: {data.get('board', 'Unknown')} (v{data.get('version', 'Unknown')})

#pragma once

#include <array>
#include <cstddef>

namespace cbdos {{
namespace board {{

// Hardware Firewall Whitelist (Allowed Pins for Expansion)
constexpr size_t NUM_ALLOWED_PINS = {len(allowed_pins)};
constexpr std::array<int, NUM_ALLOWED_PINS> BOARD_ALLOWED_PINS = {{
    {', '.join(map(str, allowed_pins))}
}};

// System Locked Pins (Reserved for Internal HAL)
constexpr size_t NUM_SYSTEM_LOCKED_PINS = {len(system_locked_pins)};
constexpr std::array<int, NUM_SYSTEM_LOCKED_PINS> BOARD_SYSTEM_LOCKED_PINS = {{
    {', '.join(map(str, system_locked_pins))}
}};

// Display Configuration
constexpr const char* DEVICE_TREE_DISPLAY_DRIVER = "{disp_driver}";
constexpr int DEVICE_TREE_DISPLAY_WIDTH = {disp_res[0]};
constexpr int DEVICE_TREE_DISPLAY_HEIGHT = {disp_res[1]};
constexpr int DEVICE_TREE_DISPLAY_FPS = {disp_fps};
constexpr int DEVICE_TREE_DISPLAY_PIN_RST = {disp_rst};
constexpr int DEVICE_TREE_DISPLAY_PIN_BL = {disp_bl};

}} // namespace board
}} // namespace cbdos
"""

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    with open(output_file, 'w') as f:
        f.write(header_content)
        
    print(f"Successfully generated {output_file} with {len(allowed_pins)} allowed pins.")

if __name__ == "__main__":
    main()
