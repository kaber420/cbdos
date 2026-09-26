# Plan de Arquitectura: Migración ESP32-S3 (JC3248W535) a CDT y Adaptación de Aplicaciones

> **Fecha:** 2026-09-26  
> **Estado:** Propuesta técnica / En evaluación  
> **Objetivo:** Integrar formalmente la placa **Guition JC3248W535 (ESP32-S3)** en la arquitectura de *Compiled Device Tree* (CDT), unificando la fuente de verdad de hardware con `boards/jc3248w535.json`, generalizando el compilador `tools/cdtc.py` y estableciendo la hoja de ruta para la adaptación de UI y aplicaciones.

---

## 1. Contexto y Diagnóstico del Hardware S3

El target `bsp/esp32_s3_jc3248` compila limpiamente hoy en día con PlatformIO (`pio run -d bsp/esp32_s3_jc3248`), pero padece del problema original de dispersión de hardware que se resolvió en el P4:

1. **Pines Dispersos y Desincronizados:**
   - La MicroSD SPI usa `12, 13, 11, 10` (en `hal_storage_s3.cpp`).
   - El archivo temporal `bsp/esp32_s3_jc3248/include/cbdos_device_tree.h` listaba esos mismos pines como pines libres de expansión (`EXPANSION_PINS`).
   - La pantalla QSPI AXS15231B y el Audio I2S tienen números mágicos en código fuente.
2. **Compilador CDT Acoplado:**
   - `tools/cdtc.py` asume exclusivamente la topología del ESP32-P4 (espera coprocesador C6 vía SDIO, bus MIPI DSI, LDOs VO3/VO4). Si se le alimenta una placa S3, falla o genera definiciones inconsistentes.
3. **Apps y Pantalla (320x480):**
   - El Core C++ y el runtime Lua 5.4 son agnósticos al procesador.
   - El factor determinante para las aplicaciones es la resolución: el S3 es **320x480** (o 480x320) frente a los **480x800** del P4.

---

## 2. Mapa Canónico de Hardware: Guition JC3248W535

Auditoría directa del código activo y drivers (`JC3248W535 Driver`):

| Periférico | Función / Net | Pin GPIO | Notas |
|---|---|:---:|---|
| **Pantalla QSPI (AXS15231B)** | Chip Select (CS) | **45** | QSPI Bus |
| | Serial Clock (SCLK) | **47** | QSPI Clock |
| | Data 0 (SDIO0) | **21** | Bus de 4 bits |
| | Data 1 (SDIO1) | **48** | |
| | Data 2 (SDIO2) | **40** | |
| | Data 3 (SDIO3) | **39** | |
| | Reset (RST) | **-1** | Conectado a Chip Reset o Pull-up |
| | Backlight (BL) | **1** | Control PWM / Brillo |
| **Touch I2C (AXS15231B)** | SDA | **4** | I2C Bus |
| | SCL | **8** | I2C Bus |
| | INT | **3** | Interrupción táctil |
| | RST | **-1** | Compartido con display |
| | Dirección I2C | `0x3B` | AXS15231B touch controller |
| **Audio I2S (MAX98357A)** | Bit Clock (BCK) | **42** | I2S Master TX |
| | Word Select (WS/LRC) | **2** | Left/Right Clock |
| | Data Out (DOUT) | **41** | Datos de audio mono/stereo |
| | Data In (DIN) / PA | **-1** | Sin micrófono en PCB |
| **MicroSD SPI (HSPI)** | CS (SS) | **10** | Bus SPI |
| | MOSI | **11** | SPI Master Out |
| | SCK | **12** | SPI Clock |
| | MISO | **13** | SPI Master In |
| **Consola UART / USB** | UART0 TX | **43** | Serial default |
| | UART0 RX | **44** | Serial default |
| | USB D- | **19** | USB-CDC Nativo OTG |
| | USB D+ | **20** | USB-CDC Nativo OTG |
| **Flasher Externo** | Flasher TX / RX | **15 / 16** | Conexión externa |
| **Pines Prohibidos (OPI Flash/RAM)** | Flash + PSRAM OPI | **26..37** | **Bajo ningún concepto reasignar** |

---

## 3. Plan de Ejecución por Fases

### Fase 1: Generalización del CDT y Soporte Multi-SoC

- [ ] **Paso 1.1: Creación de `boards/jc3248w535.json`**
  - Ubicación canónica en la raíz: `boards/jc3248w535.json`.
  - Definición completa de `display` (tipo QSPI, 320x480, 30 fps), `touch_i2c0`, `audio_i2s`, `sd_spi`, `console_uart0`, `usb_cdc` y whitelist estricta de `expansion`.
- [ ] **Paso 1.2: Generalización de `tools/cdtc.py`**
  - Hacer opcionales los bloques no universales (ej. `coprocessor_c6` solo si existe en el JSON).
  - Soportar tipos de bus de pantalla (`mipi_dpi` para P4 vs `qspi` para S3).
  - Añadir flags `constexpr bool HAS_COPROCESSOR`, `constexpr bool HAS_MIPI_DSI`, etc., para permitir condicionales en tiempo de compilación.
  - Asegurar que `boards/jc4880p443.json` continúe generando su header idéntico sin regresiones en P4.
- [ ] **Paso 1.3: Integración de Codegen en PlatformIO**
  - Crear script pre-build en PlatformIO (`bsp/esp32_s3_jc3248/scripts/gen_device_tree.py` o hook `extra_scripts`) que ejecute `tools/cdtc.py boards/jc3248w535.json bsp/esp32_s3_jc3248/include/cbdos_device_tree.h`.
- [ ] **Paso 1.4: Migración de HALs del S3 al Device Tree**
  - `hal_storage_s3.cpp`: reemplazar pines mágicos `12, 13, 11, 10` por `cbdos::board::sdcard::PIN_*`.
  - `hal_audio_s3.cpp`: migrar `42, 2, 41` a `cbdos::board::audio::PIN_*`.
  - `hal_display_s3.cpp`: migrar resolución y backlight a `cbdos::board::display::*`.
  - `S3GpioBackend::isPinAvailable`: reemplazar la lista manual por validación contra `cbdos::board::SYSTEM_PINS` y `EXPANSION_PINS`.
- [ ] **Paso 1.5: Verificación Dual-Target**
  - Compilar P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
  - Compilar S3: `pio run -d bsp/esp32_s3_jc3248`
  - Ambos deben compilar con cero warnings y cero regresiones.

---

### Fase 2: Adaptación de la UI y Motor de Apps al S3 (320x480)

- [ ] **Paso 2.1: Detección y Métricas de UI Responsivas**
  - Auditar `BaseView` y `HeaderBar` en 320x480.
  - Validar que el padding, ancho de iconos y barras de estado se ajusten automáticamente según `lv_display_get_horizontal_resolution(lv_display_get_default())`.
- [ ] **Paso 2.2: Entorno de Ejecución Lua (`LuappView`) en S3**
  - Verificar que `LuappView` inicialice y cargue scripts en el target S3.
  - Comprobar que los bindings de `Network` (HTTP, Sockets) utilicen el backend nativo de Wi-Fi del S3 sin pasar por el protocolo C6.
- [ ] **Paso 2.3: Validación de la Aplicación Roku Remote en S3**
  - Probar la carga de `resources/apps/roku_remote.luapp` en la pantalla de 320x480.
  - Ajustar coordenadas/botones de la app de Roku para que su diseño encaje ergonómicamente tanto en 480x800 como en 320x480.

---

## 4. Criterios de Aceptación

1. **Fuente de Verdad Única:** Todos los pines de la Guition JC3248W535 se leen exclusivamente desde `cbdos_device_tree.h`, generado desde `boards/jc3248w535.json`.
2. **Cero Conflictos de Pines:** La MicroSD y el bus QSPI están protegidos en `isPinAvailable()` sin posibilidad de que una app o usuario los corrompa vía GPIO.
3. **Compilación Multi-Target Limpia:**
   - ESP32-P4: `idf.py build` -> `SUCCESS`
   - ESP32-S3: `pio run` -> `SUCCESS`
4. **Ejecución de Apps:** La aplicación `roku_remote.luapp` abre y renderiza correctamente en el entorno del S3.
