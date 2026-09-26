# Plan de Arquitectura: Migración ESP32-S3 (JC3248W535) a CDT y Adaptación de Aplicaciones

> **Fecha:** 2026-09-26  
> **Estado:** Aprobado tras evaluación técnica crítica (Muse Spark 1.3)  
> **Objetivo:** Integrar formalmente la placa **Guition JC3248W535 (ESP32-S3)** en la arquitectura de *Compiled Device Tree* (CDT), unificando la fuente de verdad de hardware con `boards/jc3248w535.json`, generalizando el compilador `tools/cdtc.py`, protegiendo los pines críticos y estableciendo la hoja de ruta para la adaptación de UI y aplicaciones.

---

## 1. Contexto y Diagnóstico del Hardware S3

El target `bsp/esp32_s3_jc3248` compila limpiamente hoy en día con PlatformIO (`pio run -d bsp/esp32_s3_jc3248`), pero presenta inconsistencias estructurales y colisiones latentes de hardware:

1. **Inconsistencias y Colisiones de Pines Identificadas:**
   - La MicroSD SPI usa `12, 13, 11, 10` (en `hal_storage_s3.cpp`). El archivo borrador `bsp/esp32_s3_jc3248/include/cbdos_device_tree.h` listaba esos mismos pines erróneamente como libres de expansión (`EXPANSION_PINS`).
   - El rango `10..19` que figuraba como expansión incluía `GPIO 19`, que es la línea `USB D-` nativa de la consola OTG. Exponerlo como GPIO usuario corrompe el USB-CDC.
   - `GPIO 15 y 16` se usan simultáneamente como defaults de UART de flasheo externo y expansión sin política de posesión clara.
   - `GPIO 45` (CS del panel LCD) y `GPIO 3` (INT del touch) son pines de *strapping* en el ESP32-S3 que requieren protección estricta en el arranque.
   - Macros `-DI2S_*` duplicadas en los entornos `env:gbc`, `env:doom` y `env:lua` de `platformio.ini`, constituyendo segundas fuentes de verdad.
2. **Compilador CDT Acoplado (`tools/cdtc.py`):**
   - **Bug crítico de fallback:** `cdtc.py:50` asigna por defecto `[14, 15, 16, 17, 18, 19]` a `PINS_SDIO` aunque el bloque `coprocessor_c6` no exista en el JSON.
   - Solo soporta `sdmmc` (D0..D3, CLK, CMD) y no contempla bus `spi` para MicroSD (`cs, mosi, miso, sck`).
   - Solo soporta pantallas MIPI DSI y no contempla `qspi` ni parámetros de bus/panel (`cs, sclk, sdio0..3`).
   - Recolección simple de pines sin validación de solapamiento entre `SYSTEM_PINS` y `EXPANSION_PINS`.
3. **Apps y Pantalla (320x480):**
   - El Core C++ y el runtime Lua 5.4 son agnósticos a la CPU.
   - El factor determinante para las aplicaciones es la resolución: el S3 es **320x480** (o 480x320) frente a los **480x800** del P4. El ajuste debe apoyarse en layout responsivo relativo/scroll y no en coordenadas fijas.

---

## 2. Mapa Canónico de Hardware: Guition JC3248W535

Auditoría verificada contra código activo y drivers (`JC3248W535 Driver`):

| Sub-sistema | Señal / Función | Pin GPIO | Restricción / Nota Técnica |
|---|---|:---:|---|
| **Pantalla QSPI (AXS15231B)** | Chip Select (CS) | **45** | Pin de strapping `VDD_SPI`. Prohibido toggle en boot. |
| | Serial Clock (SCLK) | **47** | QSPI Clock |
| | Data 0 (SDIO0) | **21** | Bus de datos QSPI 4-bit |
| | Data 1 (SDIO1) | **48** | |
| | Data 2 (SDIO2) | **40** | |
| | Data 3 (SDIO3) | **39** | |
| | Reset (RST) | **-1** | Conectado a reset de placa / Pull-up |
| | Backlight (BL) | **1** | PWM LEDC (frecuencia y canal definidos) |
| **Touch I2C (AXS15231B)** | SDA | **4** | Bus I2C0 |
| | SCL | **8** | Bus I2C0 |
| | INT | **3** | Pin de strapping/JTAG. Requiere pull-up pasivo. |
| | RST | **-1** | Compartido con display |
| | Config I2C | — | Puerto: 0, Frecuencia: 400kHz, Dirección: `0x3B` |
| **Audio I2S (MAX98357A)** | Bit Clock (BCK) | **42** | I2S Master TX (sin MCLK en MAX98357A) |
| | Word Select (WS/LRC) | **2** | Left/Right Clock |
| | Data Out (DOUT) | **41** | Mono/Stereo DAC Data |
| | Data In (DIN) / PA | **-1** | Sin micrófono en PCB |
| **MicroSD SPI (HSPI)** | Chip Select (CS) | **10** | Bus SPI MicroSD dedicado |
| | MOSI | **11** | SPI Master Out |
| | SCK | **12** | SPI Clock |
| | MISO | **13** | SPI Master In |
| **Consola y Comunicaciones** | UART0 TX | **43** | Serial0 Hardware TX |
| | UART0 RX | **44** | Serial0 Hardware RX |
| | USB OTG D- | **19** | Consola USB-CDC Nativa (`Serial`) |
| | USB OTG D+ | **20** | Consola USB-CDC Nativa (`Serial`) |
| | Flasher Externo | **15 / 16** | UART dedicada a flasheo secundario |
| **Pines Prohibidos (OPI Flash/RAM)** | Flash + PSRAM OPI | **26..37** | **Bajo ningún concepto reasignar** |
| **Pines Libres Expansión (JP1 Whitelist)** | Expansión Segura | **5, 6, 7, 9, 14, 17, 18, 38** | Sin colisión con buses ni strappings |

---

## 3. Plan de Ejecución por Fases

### Fase 1: Generalización del CDT y Soporte Multi-SoC

- [ ] **Paso 1.1: Creación de `boards/jc3248w535.json`**
  - Ubicación canónica: `boards/jc3248w535.json`.
  - Definición explícita del esquema:
    - `"soc": "esp32s3"`
    - `"display": { "driver": "axs15231b", "bus": "qspi", "resolution": [320, 480], "fps": 30, "pins": { "cs": 45, "sclk": 47, "d0": 21, "d1": 48, "d2": 40, "d3": 39, "bl": 1 } }`
    - `"touch_i2c0": { "i2c_port": 0, "freq": 400000, "addr": "0x3B", "pins": { "sda": 4, "scl": 8, "int": 3 } }`
    - `"audio_i2s": { "codec": "max98357a", "has_mclk": false, "pins": { "bclk": 42, "ws": 2, "dout": 41 } }`
    - `"storage": { "bus": "spi", "pins": { "cs": 10, "mosi": 11, "sck": 12, "miso": 13 } }`
    - `"console": { "usb_cdc": { "dm": 19, "dp": 20 }, "uart0": { "tx": 43, "rx": 44 } }`
    - `"flasher": { "pins": { "tx": 15, "rx": 16 } }`
    - `"prohibited_pins": [26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37]`
    - `"expansion": { "jp1_allowed": [5, 6, 7, 9, 14, 17, 18, 38] }`

- [ ] **Paso 1.2: Refactorización y Blindaje de `tools/cdtc.py`**
  - **Corrección Bug SDIO:** `coprocessor_c6` se emite **únicamente** si la clave existe en `devices`; si no, emite `constexpr bool HAS_COPROCESSOR = false;`.
  - **Soporte Multi-Bus para Storage:**
    - Si `devices.storage.bus == "spi"`, genera `namespace sdcard` con `PIN_CS, PIN_MOSI, PIN_SCK, PIN_MISO` y `BUS = "spi"`.
    - Si `devices.sdmmc` existe, genera `PIN_D0..D3, PIN_CLK, PIN_CMD` y `BUS = "sdmmc"`.
  - **Soporte Multi-Bus para Display:**
    - Generar constantes según el bus (`qspi` emite `PIN_CS, PIN_SCLK, PIN_D0..D3`; `mipi_dpi` emite timings/rst/bl).
  - **Validación de Solapamiento en Codegen:**
    - El script en Python debe validar `set(system_pins) & set(expansion_pins) == empty` y `set(expansion_pins) & set(prohibited_pins) == empty`. Si hay colisión, aborta con `sys.exit(1)`.
  - **Prueba Golden P4:**
    - Verificar que regenerar `boards/jc4880p443.json` mantenga compatibilidad exacta con el ESP32-P4 sin romper headers existentes.

- [ ] **Paso 1.3: Hook de Automatización en PlatformIO**
  - Crear `bsp/esp32_s3_jc3248/scripts/gen_device_tree.py` invocable vía `extra_scripts = pre:scripts/gen_device_tree.py` en `platformio.ini`.
  - El script invoca `tools/cdtc.py boards/jc3248w535.json bsp/esp32_s3_jc3248/include/cbdos_device_tree.h` usando rutas absolutas resueltas desde `PROJECT_DIR`.
  - Limpiar segundas fuentes de verdad: retirar las macros redundantes `-DI2S_*` en `env:gbc`, `env:doom` y `env:lua`.

- [ ] **Paso 1.4: Migración de HALs del S3 al Device Tree**
  - `hal_storage_s3.cpp`: migrar `s_sdSPI->begin(12, 13, 11, 10)` a `cbdos::board::sdcard::PIN_*`.
  - `hal_audio_s3.cpp`: migrar pines I2S `42, 2, 41` a `cbdos::board::audio::PIN_*`.
  - `hal_display_s3.cpp`: migrar resolución y pin de backlight a `cbdos::board::display::*`.
  - `S3GpioBackend::isPinAvailable`: reemplazar la lista manual hardcodeada por la validación de `cbdos::board::EXPANSION_PINS` y rechazo estricto de `cbdos::board::SYSTEM_PINS`.

- [ ] **Paso 1.5: Verificación Dual-Target**
  - Build S3: `pio run -d bsp/esp32_s3_jc3248` -> `SUCCESS`
  - Build P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build` -> `Project build complete`

---

### Fase 2: Adaptación de la UI y Motor de Apps al S3 (320x480)

- [ ] **Paso 2.1: Directrices de UI Responsiva para 320x480**
  - Prohibir píxeles absolutos en vistas y widgets de aplicaciones.
  - Asegurar que `BaseView` y `HeaderBar` obtengan dimensiones vía `lv_display_get_horizontal_resolution()` y `lv_display_get_vertical_resolution()`.
- [ ] **Paso 2.2: Validación de `LuappView` en S3**
  - Validar que el entorno de ejecución Lua inicialice en el target S3 y enlace con el backend de red nativo (Wi-Fi directo).
- [ ] **Paso 2.3: Adaptación y Prueba de `roku_remote.luapp`**
  - Ajustar el layout del control remoto de Roku con proporciones relativas / flexbox de LVGL para que encaje de forma ergonómica en 320x480 y en 480x800.

---

## 4. Criterios de Aceptación

1. **Fuente Única de Verdad:** Ningún driver ni HAL de S3 contiene números mágicos de pines; todos se leen desde `cbdos_device_tree.h`.
2. **Cero Conflictos de Hardware:** Los pines SPI de MicroSD, USB nativo, strappings y bus QSPI están protegidos de accesos ilegítimos.
3. **Cero Regresiones Multi-Target:** Ambos objetivos (ESP32-P4 en ESP-IDF y ESP32-S3 en PlatformIO) compilan limpiamente.
4. **Validación Funcional:** La aplicación `roku_remote.luapp` renderiza correctamente en la pantalla de 320x480 del S3.
