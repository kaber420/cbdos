# Plan: BSP Genérico ESP32-P4, Multi-Placa y Soporte Multi-Revisión

> **Documento de Ejecución**
> Basado en [CDT_CANONICAL.md](CDT_CANONICAL.md) (estructura y mapa de hardware) y en el
> [PLAN_MIGRACION_CDT_Y_DRIVERS.md](PLAN_MIGRACION_CDT_Y_DRIVERS.md) (pasos atómicos del CDT).
> **Estado:** propuesta, no ejecutada. Ningún paso se da por hecho sin `idf.py build` limpio.

---

## 1. Por qué existe este plan

El firmware **no es solo para la JC4880P443C**. Hoy CBDos corre en:

| Placa | SoC | Display | BSP |
|---|---|---|---|
| Guition JC4880P443C | ESP32-P4 v1.3 | ST7701S 480×800 MIPI-DSI | `bsp/esp32_p4_jc4880` |
| Guition JC3248W535 | ESP32-S3 | AXS15231B 320×480 QSPI | `bsp/esp32_s3_jc3248` |
| Guition JC1060P470C_I_W_Y | ESP32-P4 v1.0 | JD9165 1024×600 MIPI-DSI | futuro |

Si el BSP P4 conserva el nombre de la placa y los datos hardcodeados, cada placa nueva
significa copiar el BSP entero y cazar números mágicos archivo a archivo. Ese es exactamente
el problema que el CDT quiere eliminar.

**Objetivo único:**

> Un solo **código fuente** BSP para ESP32-P4, con una sola `boards/*.json` por placa y un
> `panels/*.h` por panel. Añadir una placa P4 = datos, no drivers.

**Matiz importante:** "un BSP para todos los P4" **no** implica un único `.bin` universal.
El firmware compilado para revisión de silicio anterior a v3 no arranca en v3.x y viceversa.
Existe una sola *fuente* y una *configuración por placa/perfil*.

**Alcance actual:** solo `<v3` (v0/v1/v2, 360 MHz). v3.x queda **pospuesto** hasta que exista
una placa real con esa revisión; no se prepara ni se prueba en hardware inexistente.

---

## 2. Definición de éxito

1. `bsp/esp32_p4/` compila para JC4880P443C y JC1060P470C cambiando solo `CBDOS_BOARD`.
2. Cero referencias a `esp32_p4_jc4880` fuera de `specs/history/`.
3. Cero `if (board == ...)` en `DisplayHAL.cpp`, `AudioHAL.cpp`, `TouchHAL.cpp`.
4. Un `sdkconfig.defaults` base + overrides por placa; sin fork de BSP.
5. Documentación canónica actualizada y sin contradicciones.

---

## 3. Estructura objetivo

```
boards/                         ← DATOS DE LA PLACA (PCB)
  jc4880p443.json               pines, LDO, DSI, qué panel usa, expansion
  jc1060p470.json               futura placa 7"
  jc3248w535.json               (S3; fuera del alcance P4 de este plan)

panels/                         ← DATOS DEL PANEL (init sequence + timings)
  st7701s_jc4880.h
  jd9165_jc1060.h

tools/cdtc.py                   ← JSON → cbdos_device_tree.h (constexpr) + panel ref

bsp/esp32_p4/                   ← BSP GENÉRICO (sin nombre de placa)
  CMakeLists.txt                lee CBDOS_BOARD, compone sdkconfig.defaults.<board>
  sdkconfig.defaults            base común (revisión, CPU, PSRAM, LVGL, C6)
  sdkconfig.defaults.jc4880p443 override menor (si hace falta)
  sdkconfig.defaults.jc1060p470 override (rev v1.0, Ethernet, etc.)
  hal/                          drivers genéricos: LEEN del DT, no hardcodean
  main/CMakeLists.txt           invoca cdtc.py con el JSON de CBDOS_BOARD
```

**Criterio de pertenencia:**

| Dato | Vive en | Ejemplo |
|---|---|---|
| Cambia por PCB | `boards/*.json` | rst=5, rst=27, ldo_chan=3 |
| Cambia por panel | `panels/*.h` | init DCS, dpi clock, hsync |
| Cambia por revisión/silicon | `sdkconfig.defaults.<board>` | `REV_LESS_V3`, CPU 360 |
| No cambia | `*.cpp` | lógica del driver, llamadas IDF |

---

## 4. Revisiones de silicio ESP32-P4

| Revisión | Estado | CPU máx. | Notas |
|---|---|---|---|
| v0.5 / v1.0 / v1.1 / v1.2 / v1.3 | `<v3` | 360 MHz | JC4880 es v1.3 (engineering sample); JC1060 es v1.0 |
| v2.x | `<v3` | 360 MHz | aplica el mismo `SELECTS_REV_LESS_V3` |
| v3.0 / v3.1 / v3.2 | `v3.x` | 400 MHz | **fuera de alcance**: requiere `REV_MIN_3` y no `SELECTS_REV_LESS_V3` |

**Opciones de Kconfig (ESP-IDF):**

- `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` → binario para v0/v1/v2 (**único alcance hoy**).
- `CONFIG_ESP32P4_REV_MIN_0` / `CONFIG_ESP32P4_REV_MAX_FULL` → rango aceptado.
- `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360` → CPU para `<v3`.

**Regla:** el BSP base se queda en `<v3` / 360 MHz. v3.x solo se activará cuando exista
una placa con esa revisión, con su propio `sdkconfig.defaults.<board>` y su propia prueba
en hardware. No se mantiene soporte especulativo.

**Problema detectado hoy:** `bsp/esp32_p4_jc4880/sdkconfig.defaults:7-8` pide 400 MHz,
pero `sdkconfig:1553-1554` usa 360 MHz. Si un build regenera desde defaults puros, la
configuración es incoherente. Corregir en la Fase 2.

---

## 5. Fases de ejecución

### Fase 0 — Completar el CDT Nivel 2 (requisito previo)

Es el `PLAN_MIGRACION_CDT_Y_DRIVERS.md` Fase B, pasos 7-11. Sin esto no hay multi-placa.

| Paso | Descripción | Estado |
|:---|:---|:---|
| 7 | Ampliar JSON: `panel`, `dsi`, `ldo`, `i2s_port`, `codec_addr`, `i2c_port`, `nc_pins` | pendiente |
| 8 | Ampliar `cdtc.py`: namespaces `display::dsi`, `display::ldo`, `touch::I2C_PORT`, `audio::I2S_PORT`, `NC_PINS` | pendiente |
| 9 | `DisplayHAL` profundo: lanes, LDO, timings desde el DT | pendiente |
| 10 | Init sequence a `panels/st7701s_jc4880.h` | pendiente |
| 11 | `DisplayHAL` incluye el panel referenciado por el JSON | pendiente |

**Criterio:** `idf.py build` OK en JC4880; ninguna constante de timings/LDO/lanes en `DisplayHAL.cpp`.

### Fase 1 — Parametrizar el BSP y seleccionar placa

1. **Selección de board.** Añadir en `bsp/esp32_p4/CMakeLists.txt`:
   - `set(CBDOS_BOARD "jc4880p443" CACHE STRING "Placa CBDos")`.
   - Componer `SDKCONFIG_DEFAULTS` con `sdkconfig.defaults` + `sdkconfig.defaults.${CBDOS_BOARD}`.
2. **Generación del DT.** `main/CMakeLists.txt` ejecuta `tools/cdtc.py boards/${CBDOS_BOARD}.json`
   y escribe el header en el **build dir** (no en `hal/`).
3. **Panel.** `cdtc.py` emite la referencia de panel; el HAL la consume sin `if` por nombre de placa.
4. **Build por board:** `idf.py -B build_jc4880p443 -D CBDOS_BOARD=jc4880p443 build` (o script wrapper).

**Criterio:** dos builds con distinto `CBDOS_BOARD` compilan sin editar HALs.

### Fase 2 — BSP genérico y referencias

1. `git mv bsp/esp32_p4_jc4880 bsp/esp32_p4`.
2. Actualizar referencias:
   - `scripts/release_firmware.sh:49,50,59`
   - `.agents/AGENTS.md:23,24`
   - `manual/getting-started/flashing.md:30`
   - `specs/ROADMAP.md:12`
   - `site/src/lib/boards.ts:22` (`bspPath`)
   - `site/src/lib/HardwareExplorer.svelte:17-18` (`bspPath`, `buildCmd`)
   - `site/src/lib/FlashingGuide.svelte:84-85`
   - `docs/assets/index-DOcRDXAH.js` (build precompilado; campo `bspPath`)
3. Mantener `project(cbdos_p4)` (ya es genérico).

**Criterio:** `grep -R "esp32_p4_jc4880"` = 0 fuera de `specs/history/`.

### Fase 3 — Configuración de revisión por placa

1. `sdkconfig.defaults` base: `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` y CPU **360 MHz**
   (coherente con v1.x; eliminar el 400 de `defaults:7-8`).
2. `sdkconfig.defaults.<board>` solo para diferencias de placa (PSRAM, Ethernet, etc.),
   no para revisión: todas las placas actuales son `<v3`.
3. Verificar que PPA/LVGL/DSI compilan en `<v3`.
4. v3.x no se toca: cuando llegue la placa, se añade un override propio y su prueba real.

**Criterio:** build con `<v3` OK; no existe intención de binario mixto ni de soporte v3 especulativo.

### Fase 4 — Nueva placa JC1060P470C (prueba de aceptación)

1. `boards/jc1060p470.json`: pines, `panel`, `dsi`, `ldo`, `i2c_port`, `i2s_port`,
   `codec_addr`, `nc_pins`, `expansion`.
2. `panels/jd9165_jc1060.h`: init sequence + timings del JD9165.
3. Dependencia `espressif/esp_lcd_jd9165` en `main/idf_component.yml`.
4. `sdkconfig.defaults.jc1060p470`: revisión v1.0, 360 MHz, Ethernet si aplica.
5. Verificar pines contra el esquemático (`5-Schematic/`) antes de dar el JSON por bueno.

**Criterio:** build + arranque con la 7"; cero cambios en `DisplayHAL.cpp`/`AudioHAL.cpp`/`TouchHAL.cpp`.

### Fase 5 — Purga documental

- Actualizar `CDT_CANONICAL.md` (estructura y mapa), `specs/hardware/*`, `ROADMAP.md`.
- Mover specs de hardware obsoletos a `specs/history/`.

---

## 6. Riesgos y decisiones abiertas

| Decisión | Contexto | Resolución pendiente |
|---|---|---|
| Nombre del BSP | `esp32_p4` vs `esp32_p4_common` | recomiendo `esp32_p4` |
| `CBDOS_BOARD` | CMake cache vs variable de entorno vs Kconfig | recomiendo cache + script |
| Timings JD9165 | vendor 51.2 MHz vs BSP Guition 58 MHz | verificar con el panel real |
| Pines JC1060 | datos de comunidad (reset 27, bl 23, touch 7/8/21/22) | contrastar esquemático |
| Binario `board.cdt` | sin partición ni loader | no aplica (CDT_CANONICAL §1.5) |
| Binario universal P4 | imposible entre `<v3` y v3.x | builds separados; v3.x pospuesto sin hardware |

---

## 7. Registro de cambios

| Fecha | Paso | Resultado |
|:---|:---|:---|
| — | — | plan redactado; sin ejecución |

---

## 8. Referencias

- [CDT_CANONICAL.md](CDT_CANONICAL.md) — estructura `boards/` + `panels/`, mapa de hardware.
- [CDT_POR_QUE_Y_OBJETIVO.md](CDT_POR_QUE_Y_OBJETIVO.md) — objetivo multi-placa.
- [CDT_TECHNICAL_SPEC.md](CDT_TECHNICAL_SPEC.md) — `RawDeviceTree` y contrato URM.
- [PLAN_MIGRACION_CDT_Y_DRIVERS.md](PLAN_MIGRACION_CDT_Y_DRIVERS.md) — pasos 0-16 del CDT.
- [plan_investigacion_hardware_jc4880.md](../hardware/plan_investigacion_hardware_jc4880.md) — metodología de auditoría de pines.
