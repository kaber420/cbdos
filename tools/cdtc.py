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
    soc = data.get("soc", "unknown")
    devices = data.get("devices", {})
    
    # 1. Extraer pines de expansion (JP1)
    expansion_pins = sorted(list(set(data.get("expansion", {}).get("jp1_allowed", []))))
    
    # 2. Extraer pines prohibidos (ej. Flash/PSRAM OPI)
    prohibited_pins = sorted(list(set(data.get("prohibited_pins", []))))
    
    # 3. Recolectar todos los pines del sistema base
    system_pins = []
    for dev_name, dev_config in devices.items():
        pins = dev_config.get("pins", {})
        for p_name, p_val in pins.items():
            if isinstance(p_val, int) and p_val >= 0:
                system_pins.append(p_val)
            elif isinstance(p_val, list):
                system_pins.extend([p for p in p_val if isinstance(p, int) and p >= 0])
    system_pins = sorted(list(set(system_pins)))
    
    # 4. Validaciones de integridad y solapamiento de hardware
    sys_set = set(system_pins)
    exp_set = set(expansion_pins)
    proh_set = set(prohibited_pins)
    
    collision_sys_exp = sys_set & exp_set
    if collision_sys_exp:
        sys.stderr.write(f"[CDTc Error] Colision critica de hardware: Pines compartidos entre sistema y expansion: {sorted(list(collision_sys_exp))}\n")
        sys.exit(1)
        
    collision_exp_proh = exp_set & proh_set
    if collision_exp_proh:
        sys.stderr.write(f"[CDTc Error] Colision critica: Pines prohibidos declarados en expansion: {sorted(list(collision_exp_proh))}\n")
        sys.exit(1)
        
    collision_sys_proh = sys_set & proh_set
    if collision_sys_proh:
        sys.stderr.write(f"[CDTc Error] Colision critica: Pines de sistema asignados a lineas prohibidas: {sorted(list(collision_sys_proh))}\n")
        sys.exit(1)
    
    # 5. Extraccion periferico por periferico
    # Display
    disp = devices.get("display", {})
    disp_driver = disp.get("driver", "unknown")
    disp_bus = disp.get("bus", "mipi_dpi")
    disp_res = disp.get("resolution", [480, 800])
    disp_fps = disp.get("fps", 60)
    disp_pins = disp.get("pins", {})
    disp_rst = disp_pins.get("rst", -1)
    disp_bl = disp_pins.get("bl", -1)
    disp_cs = disp_pins.get("cs", -1)
    disp_sclk = disp_pins.get("sclk", -1)
    disp_d0 = disp_pins.get("d0", -1)
    disp_d1 = disp_pins.get("d1", -1)
    disp_d2 = disp_pins.get("d2", -1)
    disp_d3 = disp_pins.get("d3", -1)
    
    # Touchscreen
    touch = devices.get("touch_i2c0", {})
    touch_pins = touch.get("pins", {})
    touch_sda = touch_pins.get("sda", -1)
    touch_scl = touch_pins.get("scl", -1)
    touch_rst = touch_pins.get("rst", -1)
    touch_int = touch_pins.get("int", -1)
    touch_port = touch.get("i2c_port", 0)
    touch_freq = touch.get("freq", 400000)
    touch_addr_str = str(touch.get("addr", "0x3B"))
    
    # Audio
    audio = devices.get("audio_i2s", {})
    audio_pins = audio.get("pins", {})
    audio_mclk = audio_pins.get("mclk", -1)
    audio_bclk = audio_pins.get("bclk", -1)
    audio_ws = audio_pins.get("ws", -1)
    audio_dout = audio_pins.get("dout", -1)
    audio_din = audio_pins.get("din", -1)
    audio_pa = audio_pins.get("pa", -1)
    has_mclk = audio.get("has_mclk", audio_mclk >= 0)
    
    # Storage (SDMMC / SPI)
    storage = devices.get("storage", devices.get("sdmmc", {}))
    storage_bus = storage.get("bus", "sdmmc" if "sdmmc" in devices else "spi")
    storage_pins = storage.get("pins", {})
    # Pines SDMMC
    sd_d0 = storage_pins.get("d0", -1)
    sd_d1 = storage_pins.get("d1", -1)
    sd_d2 = storage_pins.get("d2", -1)
    sd_d3 = storage_pins.get("d3", -1)
    sd_clk = storage_pins.get("clk", -1)
    sd_cmd = storage_pins.get("cmd", -1)
    # Pines SPI
    sd_cs = storage_pins.get("cs", -1)
    sd_mosi = storage_pins.get("mosi", -1)
    sd_sck = storage_pins.get("sck", -1)
    sd_miso = storage_pins.get("miso", -1)
    
    if storage_bus == "spi":
        if sd_clk == -1 and sd_sck >= 0:
            sd_clk = sd_sck
        if sd_cmd == -1 and sd_mosi >= 0:
            sd_cmd = sd_mosi
        if sd_d0 == -1 and sd_miso >= 0:
            sd_d0 = sd_miso
            
    # Console & USB
    console_pins = devices.get("console_uart0", {}).get("pins", {})
    console_tx = console_pins.get("tx", -1)
    console_rx = console_pins.get("rx", -1)
    
    usb_pins = devices.get("usb_cdc", {}).get("pins", {})
    usb_dm = usb_pins.get("dm", -1)
    usb_dp = usb_pins.get("dp", -1)
    
    flasher_pins = devices.get("flasher", {}).get("pins", {})
    flasher_tx = flasher_pins.get("tx", -1)
    flasher_rx = flasher_pins.get("rx", -1)
    
    # Coprocesador C6
    has_coprocessor = "coprocessor_c6" in devices
    c6 = devices.get("coprocessor_c6", {})
    c6_pins = c6.get("pins", {})
    c6_sdio = c6_pins.get("sdio", []) if has_coprocessor else []
    c6_reset = c6_pins.get("reset", -1) if has_coprocessor else -1
    c6_handshake = c6_pins.get("handshake", -1) if has_coprocessor else -1
    
    # Sensores y Botones
    battery = devices.get("battery_sensor", {}).get("pins", {})
    buttons = devices.get("system_buttons", {}).get("pins", {})
    bat_adc = battery.get("bat_adc", -1)
    btn_boot = buttons.get("boot", -1)
    
    # Generacion del Header C++
    header_content = f"""// AUTO-GENERATED FILE. DO NOT EDIT.
// Generated by tools/cdtc.py from {os.path.basename(input_file)}
// Board: {board_name} (v{version}) - SoC: {soc}

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cbdos {{
namespace board {{

// 0. Informacion General de Placa
constexpr const char* BOARD_NAME = "{board_name}";
constexpr const char* BOARD_VERSION = "{version}";
constexpr const char* SOC_TYPE = "{soc}";

// Pines de expansion disponibles (Cabecera JP1 Whitelist)
constexpr size_t NUM_EXPANSION_PINS = {len(expansion_pins)};
constexpr std::array<int, NUM_EXPANSION_PINS> EXPANSION_PINS = {{
    {', '.join(map(str, expansion_pins))}
}};

// Pines asignados a perifericos del sistema base
constexpr size_t NUM_SYSTEM_PINS = {len(system_pins)};
constexpr std::array<int, NUM_SYSTEM_PINS> SYSTEM_PINS = {{
    {', '.join(map(str, system_pins))}
}};

// Pines estrictamente prohibidos (Flash/PSRAM OPI)
constexpr size_t NUM_PROHIBITED_PINS = {len(prohibited_pins)};
constexpr std::array<int, NUM_PROHIBITED_PINS> PROHIBITED_PINS = {{
    {', '.join(map(str, prohibited_pins))}
}};

// 1. Pantalla
namespace display {{
    constexpr const char* DRIVER = "{disp_driver}";
    constexpr const char* BUS = "{disp_bus}";
    constexpr int WIDTH = {disp_res[0]};
    constexpr int HEIGHT = {disp_res[1]};
    constexpr int FPS = {disp_fps};
    constexpr int PIN_RST = {disp_rst};
    constexpr int PIN_BL = {disp_bl};
    // Pines bus QSPI (si aplica)
    constexpr int PIN_CS = {disp_cs};
    constexpr int PIN_SCLK = {disp_sclk};
    constexpr int PIN_D0 = {disp_d0};
    constexpr int PIN_D1 = {disp_d1};
    constexpr int PIN_D2 = {disp_d2};
    constexpr int PIN_D3 = {disp_d3};
}}

// 2. Touchscreen (I2C)
namespace touch {{
    constexpr int PIN_SDA = {touch_sda};
    constexpr int PIN_SCL = {touch_scl};
    constexpr int PIN_RST = {touch_rst};
    constexpr int PIN_INT = {touch_int};
    constexpr int I2C_PORT = {touch_port};
    constexpr uint32_t I2C_FREQ = {touch_freq};
    constexpr uint8_t I2C_ADDR = {touch_addr_str};
}}

// 3. Audio (I2S + Codec)
namespace audio {{
    constexpr bool HAS_MCLK = {'true' if has_mclk else 'false'};
    constexpr int PIN_MCLK = {audio_mclk};
    constexpr int PIN_BCLK = {audio_bclk};
    constexpr int PIN_WS = {audio_ws};
    constexpr int PIN_DOUT = {audio_dout};
    constexpr int PIN_DIN = {audio_din};
    constexpr int PIN_PA = {audio_pa};
}}

// 4. Tarjeta MicroSD (SDMMC / SPI)
namespace sdcard {{
    constexpr const char* BUS = "{storage_bus}";
    // Pines SDMMC
    constexpr int PIN_D0 = {sd_d0};
    constexpr int PIN_D1 = {sd_d1};
    constexpr int PIN_D2 = {sd_d2};
    constexpr int PIN_D3 = {sd_d3};
    constexpr int PIN_CLK = {sd_clk};
    constexpr int PIN_CMD = {sd_cmd};
    // Pines SPI
    constexpr int PIN_CS = {sd_cs};
    constexpr int PIN_MOSI = {sd_mosi};
    constexpr int PIN_SCK = {sd_sck};
    constexpr int PIN_MISO = {sd_miso};
}}

// 5. Consola Serie (UART0) & USB Nativo
namespace console {{
    constexpr int PIN_TX = {console_tx};
    constexpr int PIN_RX = {console_rx};
    constexpr int PIN_USB_DM = {usb_dm};
    constexpr int PIN_USB_DP = {usb_dp};
}}

// 6. Flasher Externo
namespace flasher {{
    constexpr int PIN_TX = {flasher_tx};
    constexpr int PIN_RX = {flasher_rx};
}}

// 7. Coprocesador ESP32-C6 (SDIO)
namespace coprocessor {{
    constexpr bool HAS_COPROCESSOR = {'true' if has_coprocessor else 'false'};
    constexpr size_t NUM_SDIO_PINS = {len(c6_sdio)};
    constexpr std::array<int, NUM_SDIO_PINS> PINS_SDIO = {{{', '.join(map(str, c6_sdio))}}};
    constexpr int PIN_RESET = {c6_reset};
    constexpr int PIN_HANDSHAKE = {c6_handshake};
}}

// 8. Sensores
namespace sensors {{
    constexpr int PIN_BATTERY_ADC = {bat_adc};
}}

// 9. Botones Fisicos
namespace buttons {{
    constexpr int PIN_BOOT = {btn_boot};
}}

}} // namespace board
}} // namespace cbdos
"""

    os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
    with open(output_file, 'w') as f:
        f.write(header_content)
        
    print(f"Successfully generated {output_file} ({len(expansion_pins)} expansion pins, {len(system_pins)} system pins, bus={storage_bus}).")

if __name__ == "__main__":
    main()
