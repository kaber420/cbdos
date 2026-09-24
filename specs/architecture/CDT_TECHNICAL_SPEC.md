# Especificación Técnica de Implementación: CDT Binario y URM Core

> **Documento Complementario de Ingeniería**  
> Complementa a [`CDT_CANONICAL.md`](CDT_CANONICAL.md) (estructura, mapa de hardware, nombres) con las estructuras binarias de bajo nivel y las firmas de código C++.  
> **Jerarquía:** ver [`CDT_POR_QUE_Y_OBJETIVO.md`](CDT_POR_QUE_Y_OBJETIVO.md).  
> **Estado:** este documento define **contratos objetivo**. Cada sección indica si está **implementada**, **parcial** o **pendiente**.

---

## 1. Estructura Binaria Plana (`struct RawDeviceTree` para `board.cdt`)

**Estado: PENDIENTE — fuera de alcance actual.**

`board.cdt` **no existe hoy** y no se generará hasta que haya:
1. Partición en flash designada para el binario.
2. Loader en arranque que la lea y consuma.

Sin ambas cosas, el binario es un archivo huérfano (ver [`CDT_CANONICAL.md`](CDT_CANONICAL.md) §1.5). La **única salida implementada** de `tools/cdtc.py` es `cbdos_device_tree.h` (constexpr C++).

Si el requisito anterior se cumple, el formato será:

```c
#pragma pack(push, 1)
struct RawDeviceTree {
    uint32_t magic;                 // 0x43424454 ("CBDT")
    uint16_t version;               // 1
    char     board_name[32];        // "JC4880P443C"

    // Display (pines)
    int16_t  disp_width;            // 480
    int16_t  disp_height;           // 800
    int8_t   disp_pin_rst;          // 5
    int8_t   disp_pin_bl;           // 23

    // Display (config placa — Nivel 2)
    int8_t   disp_ldo_mipi_chan;    // 3
    int8_t   disp_ldo_sd_chan;      // 4
    uint16_t disp_ldo_mipi_mv;      // 2500
    uint16_t disp_ldo_sd_mv;        // 3300
    uint8_t  dsi_lanes;             // 2
    uint16_t dsi_lane_mbps;         // 500
    uint16_t dpi_clock_mhz;         // 34
    char     panel_ref[32];         // "st7701s_jc4880" (init sequence NO va aquí)

    // Touch
    int8_t   touch_pin_sda;         // 7
    int8_t   touch_pin_scl;         // 8
    int8_t   touch_pin_rst;         // 22 ✅ verificado esquemático (no usar 3 — NC)
    int8_t   touch_pin_int;         // 21 ✅ verificado esquemático (no usar 4 — NC)
    uint8_t  touch_i2c_port;        // 0

    // Audio
    int8_t   audio_pin_mclk;        // 13
    int8_t   audio_pin_bclk;        // 12
    int8_t   audio_pin_ws;          // 10
    int8_t   audio_pin_dout;        // 9
    int8_t   audio_pin_din;         // 48
    int8_t   audio_pin_pa;          // 11
    uint8_t  audio_i2s_port;        // 0
    uint8_t  audio_codec_addr;      // 0x30 write-addr = 0x18<<1 (7-bit 0x18) — sin conflicto
    uint8_t  audio_i2c_port;        // 0

    // SDMMC
    int8_t   sd_pin_d0;             // 39
    int8_t   sd_pin_d1;             // 40
    int8_t   sd_pin_d2;             // 41
    int8_t   sd_pin_d3;             // 42
    int8_t   sd_pin_clk;            // 43
    int8_t   sd_pin_cmd;            // 44

    // Consola UART0
    int8_t   uart0_pin_tx;          // 38
    int8_t   uart0_pin_rx;          // 37

    // Control de Alimentacion
    int8_t   pwr_pin_en;            // 36

    // Coprocesador ESP32-C6 (SDIO)
    int8_t   c6_sdio_pins[6];       // {14, 15, 16, 17, 18, 19}
    int8_t   c6_pin_reset;          // 54
    int8_t   c6_pin_handshake;      // 6

    // Sensores y Botones
    int8_t   bat_adc_pin;           // 53
    int8_t   boot_btn_pin;          // 35

    // Pines No Conectados
    uint8_t  num_nc_pins;           // 2
    int8_t   nc_pins[8];            // {1, 2, -1, ...}

    // Expansion JP1
    uint8_t  num_expansion_pins;    // 12
    int8_t   expansion_pins[16];    // {28..34, 49..52, -1...}
};
#pragma pack(pop)
```

**Nota:** la init sequence del panel (~100 comandos) **nunca** va en `board.cdt` — vive en `panels/*.h` (ver canónico §1.2).

---

## 2. Contrato C++ del Universal Resource Manager (URM)

**Estado: PENDIENTE — implementado parcialmente con API vieja.**

| Pieza | Estado |
|---|---|
| `core/include/cbdos/urm.hpp` | ⚠ Contiene API vieja (`claimGpio`, `SystemLocked`, `ResourceClaimMode`) — **purgar** (Paso 12) |
| `core/src/system/urm.cpp` | ⚠ Esqueleto sin consumidores; 5 símbolos renombrados en Paso 0 |
| Wiring en `main.cpp` | ❌ No existe |
| Delegación desde `isPinAvailable()` | ❌ Backend usa whitelist estática propia |

**Contrato objetivo** (este es el `.hpp` que debe quedar al terminar el Paso 12):

```cpp
#pragma once

#include <array>
#include <cstddef>

namespace cbdos {

enum class PinState {
    NotConnected,   // GPIO no ruteado en placa (GPIO 1, 2)
    InUse,          // Asignado al sistema base o a una app/módulo activo
    Available       // Disponible para asignación en JP1
};

class UniversalResourceManager {
public:
    static UniversalResourceManager& getInstance() {
        static UniversalResourceManager instance;
        return instance;
    }

    // Inicializa la tabla con los pines de cbdos_device_tree.h
    void init();

    // Consultas de estado
    PinState getPinState(int pin) const;
    const char* getPinOwner(int pin) const;
    bool isPinAvailable(int pin) const;

    // Solicitud de pin (Solo aprobado si el pin esta en EXPANSION_PINS y Available)
    bool claimPin(int pin, const char* owner);

    // Liberacion de pin
    bool releasePin(int pin, const char* owner);

    // Reseteo de pines de expansion
    void resetExpansionPins();

private:
    UniversalResourceManager();
    ~UniversalResourceManager() = default;

    UniversalResourceManager(const UniversalResourceManager&) = delete;
    UniversalResourceManager& operator=(const UniversalResourceManager&) = delete;

    struct PinEntry {
        PinState state;
        const char* owner;
    };

    std::array<PinEntry, 64> m_pinTable;
    bool m_initialized = false;
};

} // namespace cbdos
```

**Reglas de `init()`:**
- `SYSTEM_PINS` → `InUse` con owner literal (`"display"`, `"touch"`, `"audio"`, `"sdcard"`, `"console"`, `"power"`, `"coprocessor"`, `"sensors"`, `"buttons"`).
- `EXPANSION_PINS` → `Available`.
- `nc_pins` → `NotConnected`.

---

## 3. Guía de Migración de Drivers del BSP

Detalle de pasos, archivos y criterios de build: [`PLAN_MIGRACION_CDT_Y_DRIVERS.md`](PLAN_MIGRACION_CDT_Y_DRIVERS.md).  
Estado resumido:

| Driver | Paso plan | Estado | Qué queda |
|---|:---:|---|---|
| `DisplayHAL.cpp` (pines) | 0 | ✅ | — |
| `hal_uart_p4.cpp` (whitelist) | 0 | ✅ | consola 38/37 + defaults JP1 → Paso 4 |
| `TouchHAL` | 1 | ⬜ | macros → `touch::*` (mantener `I2C_PORT` hasta Paso 7) |
| `DisplayHAL.h` split-brain | 2 | ⬜ | `BOARD_DISP_*` → DT |
| `AudioHAL` | 3 | ⬜ | pines → `audio::*`; port/addr hasta Paso 7 |
| `hal_uart_p4` consola | 4 | ⬜ | `38/37` → `console::*` |
| `hal_storage_p4` | 5 | ⬜ | `GPIO_NUM_39..44` → `sdcard::*` |
| `hal_flasher_p4` | 6 | ⬜ | `GPIO_NUM_36` → `power::PIN_EN` |
| JSON + cdtc ampliación (LDO/DSI/I2S...) | 7-8 | ⬜ | Nivel 2 |
| Display profundo + `panels/` | 9-11 | ⬜ | multi-placa |

### 3.1 TouchHAL (`bsp/esp32_p4_jc4880/hal/TouchHAL.h` y `TouchHAL.cpp`) — Paso 1
- **Eliminar:**
  ```cpp
  #define BOARD_TOUCH_SDA_GPIO 7
  #define BOARD_TOUCH_SCL_GPIO 8
  #define BOARD_TOUCH_RST_GPIO 3
  #define BOARD_TOUCH_INT_GPIO 4
  ```
- **Reemplazar por:**
  ```cpp
  #include "cbdos_device_tree.h"
  // Uso: cbdos::board::touch::PIN_SDA, PIN_SCL, PIN_RST, PIN_INT
  ```
- **Conservar hasta Paso 7:** `BOARD_TOUCH_I2C_PORT` (lo usa `AudioHAL.cpp:44`).
- **Corrección en el mismo paso (esquemático):** RST/INT = **22/21**, no 3/4 (3,4 = NC).

### 3.2 DisplayHAL (`bsp/esp32_p4_jc4880/hal/DisplayHAL.cpp`) — pines ✅ / profundo ⬜
- **Pines (hecho en Paso 0):**
  - Reset: `cbdos::board::display::PIN_RST`
  - Backlight: `cbdos::board::display::PIN_BL`
  - Resolución: `cbdos::board::display::WIDTH`, `HEIGHT`
- **Pendiente (Pasos 2, 9-11):** macros `BOARD_DISP_*` en `.h`; LDO, lanes, timings, init sequence.

### 3.3 AudioHAL (`bsp/esp32_p4_jc4880/hal/AudioHAL.cpp`) — Paso 3
- **Reemplazar pines I2S:**
  - `cbdos::board::audio::PIN_MCLK`, `PIN_BCLK`, `PIN_WS`, `PIN_DOUT`, `PIN_DIN`, `PIN_PA`
- **Pendiente (Paso 7):** `i2s_port`, `codec_addr`, `i2c_port`.

### 3.4 Consola UART (`bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`) — Paso 4
- **Reemplazar pines UART0:**
  - `cbdos::board::console::PIN_TX`, `PIN_RX`
- Whitelist JP1 ya migrada (Paso 0).

### 3.5 Storage y Flasher — Pasos 5-6
- `hal_storage_p4.cpp`: `GPIO_NUM_39..44` → `sdcard::*`
- `hal_flasher_p4.cpp`: `GPIO_NUM_36` → `power::PIN_EN`
