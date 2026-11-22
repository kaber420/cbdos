# Roadmap: Soporte ESP32-P4 v3+ (`bsp/esp32_p4_v3`) y Placa Guition JC1060P470C (CBDos 0.2.6)

> **Estado:** Planificación inicial en rama `bsp-p4-v3` (rumbo a `0.2.6`).
> **Documentos relacionados:**
> - [plan_bsp_generico_p4_multiplaca.md](plan_bsp_generico_p4_multiplaca.md) (propuesta previa; **no modificado**)
> - [CDT_CANONICAL.md](CDT_CANONICAL.md) y [PLAN_MIGRACION_CDT_Y_DRIVERS.md](PLAN_MIGRACION_CDT_Y_DRIVERS.md)
> - [plan_investigacion_hardware_jc4880.md](../hardware/plan_investigacion_hardware_jc4880.md) (metodología de auditoría de pines)

---

## 1. Punto de Partida Verificado

| Componente | Estado actual observado |
|---|---|
| **Placa objetivo inicial** | Guition **JC1060P470C** (7" 1024×600 MIPI-DSI) |
| **SoC detectado** | `ESP32-P4 (revision v3.2)`, cristal 40 MHz, USB-Serial/JTAG (`303a:1001`), MAC `80:f1:b2:d8:15:65` |
| **Toolchain** | **ESP-IDF v5.5.5** (`/home/kaber420/esp/esp-idf`) — ya soporta P4 v3.x |
| **BSP `<v3` existente** | `bsp/esp32_p4_jc4880` (`JC4880P443C`, ST7701S 480×800, `<v3`, 360 MHz) — **se mantiene intacto** |
| **Rama de trabajo** | `bsp-p4-v3` (hito **CBDos 0.2.6**) |

### Decisiones de Arquitectura
1. **BSP separado por generación de silicio:** `bsp/esp32_p4_jc4880/` permanece intacto para placas **`<v3`** (360 MHz). Se crea **`bsp/esp32_p4_v3/`** dedicado exclusivamente a **ESP32-P4 `v3+`** (`CONFIG_ESP32P4_REV_MIN_300=y`, 400 MHz).
2. **Desacoplado y multi-placa desde el día 1:** `bsp/esp32_p4_v3/` nace limpio de hardcodes; todo lo dependiente de placa/panel (pines, driver LCD, secuencia de init, timings DSI, LDOs, touch, audio, SD) viene del **CDT** (`tools/cdtc.py` + `boards/*.json` + `panels/*.h`) seleccionable vía `CBDOS_BOARD`.
3. **Mantener ESP-IDF v5.5.5 durante el porting:** No migrar de rama mayor de ESP-IDF (ej. v6.0) mientras se levanta el nuevo BSP y la placa JC1060P470C, evitando mezclar roturas de API con validación de hardware.

---

## 2. Fases de Ejecución

### Fase 0 — Auditoría de Hardware Real (Zero Cambios de Código)
*Objetivo: obtener el mapa eléctrico real de la unidad antes de compilar.*

1. **Captura de logs serie de fábrica:** Conectar monitor serie al arranque del demo actual para registrar:
   - Modelo exacto de panel y secuencia/timings reportados.
   - Dirección I2C y chip del touch (ej. GT911 en `0x5D` o `0x14`).
   - Códec de audio, pines SDIO del ESP32-C6 y canal LDO usado.
2. **Esquemático y revisión de panel:**
   - Conseguir el paquete oficial (`JC1060P470C_I_W_Y`) o contrastar con BSPs verificados y guardar los esquemáticos en `specs/hardware/schematics/jc1060p470/`.
   - **Punto crítico:** Confirmar si el panel **JD9165** es revisión **V1** o **V2** (unidades con marca V2 o date code `>= 2622` usan secuencia de inicialización/timings distintos; usar el incorrecto produce pantalla blanca con ruido vertical).
   - Verificar cada GPIO real: Display (`RST`, `BL`, LDO DSI), Touch (`SDA`, `SCL`, `RST`, `INT`, `I2C_ADDR`), Audio (`MCLK`, `BCLK`, `WS`, `DOUT`, `DIN`, `PA`), MicroSD (`SDMMC` + LDO) y ESP32-C6 (`SDIO` + `RESET` + `HANDSHAKE`).

**Criterio de salida:** Tabla de pines 100% contrastada con esquemático/hardware.

---

### Fase 1 — Ampliación del CDT (`tools/cdtc.py`, `boards/`, `panels/`)
*Objetivo: que el Device Tree describa completamente la placa y el panel sin constantes ocultas en C++.*

1. **Extender esquema JSON (`boards/*.json`):**
   - Añadir sección `display.dsi` (lanes, `dpi_clock_mhz`, `hsync`/`vsync` timings, `ldo_channel`, `ldo_voltage_mv`) y `display.panel_header`.
   - Añadir `sdcard.ldo_channel` (si aplica), `touch.i2c_port`, `touch.i2c_addr`, `audio.i2s_port`, `audio.codec`, `audio.codec_addr`.
2. **Actualizar `tools/cdtc.py`:**
   - Generar las nuevas constantes `constexpr` en `cbdos_device_tree.h` manteniendo retrocompatibilidad con `jc3248w535.json` (ESP32-S3).
   - Emitir la ruta/inclusión del header del panel correspondiente.
3. **Extraer secuencias de panel a `panels/`:**
   - Mover `ST7701_Init.h` de la JC4880 a `panels/st7701s_jc4880.h`.
   - Crear `panels/jd9165_jc1060.h` con la secuencia e init timings verificados en Fase 0.

**Criterio de salida:** `cdtc.py` genera `cbdos_device_tree.h` válido para `jc4880p443.json` sin romper la compilación actual.

---

### Fase 2 — Desacoplamiento de los Drivers HAL en `bsp/esp32_p4_v3/`
*Objetivo: crear la capa HAL de `v3+` libre de valores hardcodeados de JC4880.*

1. **Base inicial limpia:** Crear `bsp/esp32_p4_v3/` tomando la estructura HAL actual de P4 y limpiando todo acoplamiento de placa. (`bsp/esp32_p4_jc4880/` no se modifica).
2. **Display (`DisplayHAL.cpp`, `DisplayHAL.h`):**
   - Leer `WIDTH`, `HEIGHT`, lanes, timings, LDOs y panel driver desde `cbdos::board::display`.
   - Incluir dinámicamente el header del panel desde `panels/` y soportar `espressif/esp_lcd_jd9165` (y futuros drivers MIPI-DSI) en `idf_component.yml`.
3. **LVGL Port (`LVGL_Port.cpp`, `LVGL_Port.h`):**
   - Eliminar `480` y `800` hardcodeados (incluyendo `lv_draw_rotate` en `LVGL_Port.cpp`) y derivarlos de `cbdos::board::display::WIDTH/HEIGHT`.
4. **Touch (`TouchHAL.cpp`, `TouchHAL.h`):**
   - Eliminar `480x800` por defecto en `TouchHAL.h`.
   - Leer `DRIVER`, `I2C_ADDR` e `I2C_PORT` desde `cbdos::board::touch` en lugar de forzar `GT911` / `0x5D`.
5. **Storage y Audio (`hal_storage_p4.cpp`, `AudioHAL.cpp`):**
   - Mover `BOARD_DISP_SD_LDO_CH` al CDT (`cbdos::board::sdcard`).
   - Parametrizar dirección/puerto I2S del códec desde `cbdos::board::audio`.

**Criterio de salida:** `grep` sin dimensiones `480`/`800` ni direcciones/LDOs fijos en `bsp/esp32_p4_v3/hal/`.

---

### Fase 3 — Parametrización `CBDOS_BOARD` en CMake (`bsp/esp32_p4_v3/`)
*Objetivo: que `bsp/esp32_p4_v3/` compile cualquier placa `v3+` y cualquier perfil (`full`, `red`, `lite`, `kds`, `tabletop`).*

1. **CMake raíz (`bsp/esp32_p4_v3/CMakeLists.txt`):**
   - Aceptar `-DCBDOS_BOARD=<placa>` (default `jc1060p470`).
   - Componer `SDKCONFIG_DEFAULTS` combinando base común `v3+` + `sdkconfig.defaults.<board>` + override de perfil (`red`, `lite`, `kds`, `tabletop`) en `build_<board>_<perfil>/`.
2. **CMake de `main/`:**
   - Invocar `tools/cdtc.py boards/${CBDOS_BOARD}.json` escribiendo `cbdos_device_tree.h` en el directorio de build (no versionado dentro de `hal/`).

**Criterio de salida:** CMake resuelve `boards/${CBDOS_BOARD}.json` y genera `cbdos_device_tree.h` sin editar código fuente.

---

### Fase 4 — Perfil ESP32-P4 v3+ y Definición de `jc1060p470`
*Objetivo: habilitar compilación nativa para silicio `v3+` (400 MHz) y placa de 7".*

1. **Crear `boards/jc1060p470.json`:** Con el mapa de pines y panel auditados en Fase 0.
2. **Crear `bsp/esp32_p4_v3/sdkconfig.defaults` (base `v3+`):**
   - **Sin** `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`.
   - Configurar `CONFIG_ESP32P4_REV_MIN_300=y` (soporte `v3.0+` / `v3.2`).
   - `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_400=y` y `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=400`.
3. **Crear `sdkconfig.defaults.jc1060p470`:** Pines SDIO del ESP32-C6 y configuración PSRAM/MIPI-DSI específicos de la placa.

**Criterio de salida:** `idf.py -C bsp/esp32_p4_v3 -B build_jc1060p470 -DCBDOS_BOARD=jc1060p470 build` genera firmware limpio sin errores de enlace ni advertencias críticas.

---

### Fase 5 — Prueba de Humo en Hardware Real (Display + Touch)
*Objetivo: validar arranque real en la JC1060P470C v3.2.*

1. Flashear `build_jc1060p470` en `/dev/ttyACM0`.
2. Verificar por monitor serie que no haya `illegal instruction`, panics de revisión de silicio ni errores de PSRAM/LDO.
3. Validar patrón de color/LVGL en `1024×600` (sin pantalla blanca, sin desplazamiento ni tearing).
4. Validar coordenadas y calibración del touch `GT911` en las cuatro esquinas.

**Criterio de salida:** UI base de CBDos visible y táctil operativo en `1024×600` en la JC1060P470C.

---

### Fase 6 — Validación de Periféricos y Adaptación de `core/` a 1024×600
*Objetivo: paridad funcional completa con JC4880.*

1. **Periféricos:**
   - MicroSD (montaje FatFS/exFAT vía SDMMC).
   - Audio (reproducción y grabación I2S/códec + amplificador PA).
   - Coprocesador ESP32-C6 (`esp_hosted` SDIO: Wi-Fi scan/STA/AP y BLE).
   - USB Host High-Speed OTG (teclado HID / dispositivos CDC).
2. **Ajustes en `core/` para landscape nativo `1024×600`:**
   - Revisar constantes residuales (ej. `WALLPAPER_HEIGHT = 480` en `core/src/ui/WallpaperManager.cpp`).
   - Auditar vistas (`DashboardView`, `KvmClientView`, `HeaderBar`, modales) para aprovechar `1024×600` sin recortes ni espacios muertos.

**Criterio de salida:** Todos los subsistemas de CBDos operativos en la JC1060P470C v3.2 y compilación verde tanto en `jc4880p443` (v1.3) como en `jc1060p470` (v3.2) y `esp32_s3_jc3248`.
