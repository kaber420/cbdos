# Plan de Ejecución: CDT Multi-Placa (Nivel 2) y Drivers

> **Documento de Ejecución**  
> Objetivo real: soportar nuevas placas del mismo SoC cambiando **solo datos** (JSON + panel), sin reescribir drivers.  
> Basado en [CDT_CANONICAL.md](CDT_CANONICAL.md) — *requiere actualización de su §1/§4 para reflejar la estructura boards/ + panels/*.

---

## Arquitectura Objetivo

```
boards/                         ← datos de LA PLACA (PCB)
  jc4880p443.json               pines, LDO, qué panel usa, expansion
  otrop4.json                   nueva placa = nuevo JSON

panels/                         ← datos DEL PANEL (fabricante)
  st7701s_jc4880.h              init sequence + timings DSI
  axs15231b.h                   (si se porta S3-style a P4)

tools/cdtc.py                   JSON → cbdos_device_tree.h (constexpr)
                                + referencia a panel header

bsp/esp32_p4_jc4880/hal/
  DisplayHAL.cpp                genérico: lee TODO del DT/panel ref
  AudioHAL.cpp                  genérico: lee pins/ports/addrs del DT
  TouchHAL.cpp                  idem
```

**Criterio de pertenencia:**

| Dato | Va en... | Ejemplo |
|---|---|---|
| Cambia por PCB (pines, LDO, expansion) | `boards/*.json` | rst=5, ldo_chan=3 |
| Cambia por panel (init, timings, clock) | `panels/*.h` | hsync=12, init DCS |
| No cambia nunca (lógica driver) | `*.cpp` | secuencia de llamadas IDF |

---

## Metodología

- Un paso a la vez; tras cada uno: `idf.py build` → `Project build complete`.
- Sin cambios fuera de los archivos listados.
- **Verificación por sección (decisión 2026-09-23):** antes de migrar cada paso, verificar sus pines/datos contra el esquemático (`specs/hardware/schematics/`) y corregir JSON/docs si hay discrepancia — *no* se migra un dato sin comprobarlo. (Ejemplo ya hecho: Paso 1, Touch 3/4 → 22/21.)
- Registrar resultado en §Registro de Cambios.

---

## Estado Global

| Paso | Descripción | Estado |
|:---|:---|:---:|
| 0 | Reparación build (rename símbolos) | ✅ 2026-09-23 |
| 1 | TouchHAL → DT | ✅ 2026-09-23 |
| 2 | DisplayHAL.h split-brain + DT | ✅ 2026-09-23 |
| 3 | AudioHAL → DT (pins + port + addr) | ⬜ |
| 4 | hal_uart consola/defaults → DT | ⬜ |
| 5 | hal_storage SDMMC → DT | ⬜ |
| 6 | hal_flasher power → DT | ⬜ |
| **7** | **Ampliar JSON: LDO, DSI, I2S port, codec addr, i2c_port** | ⬜ |
| **8** | **Ampliar cdtc.py: namespaces display::dsi, display::ldo, audio::i2s...** | ⬜ |
| **9** | **DisplayHAL profundo: timings/LDO/lanes desde DT** | ⬜ |
| **10** | **Extraer init sequence a `panels/st7701s_jc4880.h`** | ⬜ |
| **11** | **DisplayHAL incluye panel referenciado por JSON** | ⬜ |
| 12 | URM: purga API vieja + init + wiring | ⬜ |
| 13 | NC pins en JSON + URM `NotConnected` | ⬜ |
| 14 | `board.cdt` binario (solo si hay loader/partición) | ⬜ |
| 15 | S3: JSON CDT + cdtc compartido | ⬜ |
| 16 | Purga specs viejos + docs hardware | ⬜ |

---

## 🔹 Paso 0: Reparación Build ✅ (2026-09-23)

Símbolos renombrados en `cdtc.py`/header sin actualizar consumidores.  
**Corregido:** `DisplayHAL.cpp` (6 refs), `hal_uart_p4.cpp` (2), `urm.cpp` (5).  
**Verificación:** `idf.py build` → `Project build complete` ✅

---

## Fase A — Migración de pines (Nivel 1 completo)

### 🔹 Paso 1: TouchHAL ✅ (2026-09-23)
> **⚠ Corrección de pines incluida (verificada contra esquemático `JC4880P443_V1.0`):**
> - `3_ESP32-P4.png` borde inferior: nets `TOUCH_INT`/`TOUCH_RST` tras `GPIO20` → **INT=GPIO21, RST=GPIO22** (LCD_PWM=23 ✓).
> - `2_LCD&CSI.png` FPC1: pin 23=`TOUCH_RST`, pin 26=`TOUCH_INT`.
> - `pinouts_and_ports.md:75` y NC: GPIO 1,2,3,4 **no ruteados** → JSON/código actuales (3,4) son **incorrectos**.

1. **Corregir `boards/jc4880p443.json`** (no regenerar header todavía hasta el final del paso):
   ```diff
   "touch_i2c0": {
     "pins": {
       "sda": 7, "scl": 8,
   -   "rst": 3, "int": 4
   +   "rst": 22, "int": 21
     }
   }
   ```
2. `TouchHAL.h`: + `#include "cbdos_device_tree.h"`; eliminar `BOARD_TOUCH_SDA/SCL/RST/INT_GPIO`; **conservar** `BOARD_TOUCH_I2C_PORT` hasta Paso 7.
3. `TouchHAL.cpp`: → `touch::PIN_SDA/SCL/RST/INT` (RST/INT ahora 22/21 vía DT regenerado).
4. Regenerar header: `python3 tools/cdtc.py boards/jc4880p443.json` (o comando equivalente del repo).
5. **Check dependencia:** `AudioHAL.cpp:44` usa `BOARD_TOUCH_I2C_PORT` — no romper.
6. Borrar/corregir nota falsa `pinouts_and_ports.md:2` (dice GPIO 3/4; debe decir 22/21 y apuntar al esquemático).
7. Actualizar `CDT_CANONICAL.md` §2.1 (quitar ⚠ de RST/INT, valor confirmado).

- **Éxito:** `idf.py build` → `Project build complete`; JSON y header con 22/21; nota pinouts corregida.

### 🔹 Paso 2: DisplayHAL.h ✅ (2026-09-23)
- Eliminar `BOARD_DISP_H_RES/V_RES/BL_GPIO/RST_GPIO`.
- Defaults de firma/miembros → `display::WIDTH/HEIGHT` (+ include DT).
- **Conservar** `DSI_LANES/LDO_CH/SD_LDO_CH` hasta Paso 7-9.
- **Éxito:** build OK; cero `#define BOARD_DISP_*_GPIO`.

### 🔹 Paso 3: AudioHAL ✅ (2026-09-24)
- `AudioHAL.h`: eliminadas macros `BOARD_AUDIO_MCLK...PA_GPIO`; include `cbdos_device_tree.h`.
- `AudioHAL.cpp`: migrado a `cbdos::board::audio::PIN_*`.
- **Conservados para Paso 7:** `I2S_PORT`, `CODEC_ADDR` (0x30 write-addr 8-bit).
- **Dependencia I2C compartida:** bus preservado mediante `TouchHAL`.
- **Éxito:** build limpio en ESP-IDF (`Project build complete`). Cero referencias a `BOARD_AUDIO_*_GPIO`.

### 🔹 Paso 4: hal_uart_p4 consola ⬜
- Líneas 506, 539-542, 614: `38/37` → `console::PIN_TX/RX`.
- Defaults 720-728 (`32/28`): documentar como JP1-defaults o migrar según decisión.
- Whitelist ya hecha en Paso 0.
- **Éxito:** build OK; serie en 38/37.

### 🔹 Paso 5: hal_storage SDMMC ⬜
- Líneas 158-162, 172-177, 228-232, 241-246: `GPIO_NUM_39..44` → `sdcard::PIN_D0..D3/CLK/CMD`.
- **Éxito:** build OK.

### 🔹 Paso 6: hal_flasher power ⬜
- Líneas 275, 279: `GPIO_NUM_36` → `power::PIN_EN`.
- **Éxito:** build OK.

---

## Fase B — Nivel 2 (multi-placa real)

### 🔹 Paso 7: Ampliar JSON ⬜
**Archivo:** `boards/jc4880p443.json`

Añadir (valores verificados en código actual):

```json
"display": {
  "panel": "st7701s_jc4880",
  "dsi": {
    "lanes": 2,
    "lane_bit_rate_mbps": 500,
    "dpi_clock_mhz": 34,
    "hsync_pulse": 12, "hsync_back": 42, "hsync_front": 42,
    "vsync_pulse": 2,  "vsync_back": 8,  "vsync_front": 166
  },
  "ldo": { "mipi_chan": 3, "mipi_mv": 2500, "sd_chan": 4, "sd_mv": 3300 }
},
"touch_i2c0": { "i2c_port": 0, "pins": { "sda": 7, "scl": 8, "rst": 22, "int": 21 } },
"audio_i2s":  { "i2s_port": 0, "codec_addr": "0x18", "codec_addr_write": "0x30", "i2c_port": 0 },
"nc_pins": [1, 2]
```

**Éxito:** JSON válido; `cdtc.py` actual (sin cambiar aún) no rompe.

### 🔹 Paso 8: Ampliar cdtc.py ⬜
**Archivo:** `tools/cdtc.py`

Generar nuevos namespaces:
- `display::dsi::*` (lanes, clock, timings)
- `display::ldo::*` (chans, mv)
- `display::PANEL` (string ref)
- `touch::I2C_PORT`, `audio::I2S_PORT`, `audio::CODEC_ADDR`, `audio::I2C_PORT`
- `NC_PINS` / `NUM_NC_PINS`

**Éxito:** regenerar header; build sigue OK (solo se añaden constants).

### 🔹 Paso 9: DisplayHAL profundo ⬜
**Archivo:** `DisplayHAL.cpp` (+ `.h` si queda macro LDO)

Reemplazar hardcode:
- `ldo_cfg.chan_id/voltage` → `display::ldo::*` (líneas 108-109, 120-121)
- `num_data_lanes`, `lane_bit_rate` → `display::dsi::*` (129, 131)
- `dpi_clock`, hsync/vsync → `display::dsi::*` (153, 160-165)
- Eliminar `BOARD_DISP_DSI_*` del `.h` si ya no se usan.

**Éxito:** build OK; pantalla enciende (smoke test en hardware si es posible).

### 🔹 Paso 10: Init sequence a panels/ ⬜
**Acción:**
1. Crear `panels/st7701s_jc4880.h` moviendo contenido de `ST7701_Init.h`.
2. `ST7701_Init.h` → wrapper o eliminar (decidir).
3. JSON ya tiene `"panel": "st7701s_jc4880"` (Paso 7).

**Éxito:** build OK; init sequence intacta (comparar binario o smoke test).

### 🔹 Paso 11: DisplayHAL incluye panel ref ⬜
- DisplayHAL.cpp selecciona header de panel según constante DT (`display::PANEL`).
- Mecanismo simple: `#include` directo del panel activo vía macro generada o en el `.h` de placa.
- **Éxito:** build OK; misma pantalla que antes.

---

## Fase C — Recursos y multi-target

### 🔹 Paso 12: URM Core ⬜
- **Borrar** API vieja: `ResourceClaimMode`, `claimGpio/releaseGpio` con `SystemLocked`.
- **Implementar** contrato `CDT_TECHNICAL_SPEC §2`: `PinState`, `init()`, `claimPin/releasePin`, `getPinOwner`, etc.
- `init()` en `main.cpp` antes de backends: 31 SYSTEM_PINS → `InUse` con owners; 12 EXPANSION → `Available`.
- `P4GpioBackend::isPinAvailable` → delegar al URM.
- **Éxito:** log `URM Inicializado`; claim sobre pin sistema = false.

### 🔹 Paso 13: NC pins ⬜
- JSON `nc_pins: [1,2]` (hecho en Paso 7) → URM marca `NotConnected`.
- **Éxito:** claim/getPinState(1) = NotConnected.

### 🔹 Paso 14: board.cdt binario ⬜ *(solo si hay caso de uso)*
- **Prerequisito:** partición + loader en arranque. Sin eso, **no hacer** — genera huérfano.
- `cdtc.py`: `struct.pack` conforme a `RawDeviceTree`.
- **Éxito:** magic/size OK **y** sistema lo consume o descarta a favor del `.h`.

### 🔹 Paso 15: S3 en el sistema ⬜
- Crear `boards/jc3248w535.json` con pines reales de `S3GpioBackend` + pinouts.
- Ejecutar `cdtc.py` → header S3.
- `S3GpioBackend::isPinAvailable` → whitelist del DT (eliminar blacklist inline).
- **Éxito:** `pio run` S3 OK; mismos 2 targets verificados.

### 🔹 Paso 16: Purga de docs ⬜
- Specs viejos → `specs/history/` (verificar git status).
- `pinouts_and_ports.md`: Touch RST/INT **ya verificado → 22/21** (esquemático; nota línea 2 se corrige en Paso 1). Codec addr: documentar 0x18 (7-bit) y, si hace falta, 0x30 como write-addr IDF.
- Cabecera `OBSOLETO → CDT_CANONICAL.md` en residual.
- Actualizar `CDT_CANONICAL.md` §1/§4: estructura boards/panels, Nivel 2, sin promesas de "sin recompilar" sin loader.
- **Éxito:** grep `DEVICE_TREE_DISPLAY|BOARD_ALLOWED_PINS|SystemLocked` en specs vigentes = 0.

---

## Criterio de "nueva placa P4" al terminar Fase B

Para soportar otra placa ESP32-P4 con otro panel:

1. Escribir `boards/nueva.json` (pines, LDO, expansion, panel ref)
2. Escribir `panels/nuevo_panel.h` (init sequence + timings)
3. Apuntar build al nuevo JSON (`cdtc.py` input)
4. **Cero cambios** en `DisplayHAL.cpp`, `AudioHAL.cpp`, `TouchHAL.cpp`

Si algún paso obliga a tocar esos .cpp para una placa nueva, la Fase B no está completa.

---

## Registro de Cambios

| Fecha | Paso | Resultado |
|:---|:---|:---:|
| 2026-09-23 | 0 — Reparación build | ✅ `idf.py build` OK |
| | 2 parcial — símbolos DisplayHAL.cpp | ✅ en Paso 0 |
| | 4 parcial — whitelist uart | ✅ en Paso 0 |
| | Verificación Touch RST/INT = 22/21 | ✅ esquemático `JC4880P443_V1.0`; codec 0x18=0x30<<1 resuelto; docs actualizados |
| | 1 — TouchHAL → DT | ✅ `idf.py build` OK; TouchHAL migrado a `cbdos::board::touch::PIN_*`; RST/INT=22/21 en JSON y DT |
| | 2 — DisplayHAL.h → DT | ✅ `idf.py build` OK; `BOARD_DISP_H_RES/V_RES/BL/RST` eliminados; defaults migrados a DT |
| 2026-09-24 | 3 — AudioHAL → DT | ✅ `idf.py build` OK; AudioHAL migrado a `cbdos::board::audio::PIN_*`; eliminados `BOARD_AUDIO_*_GPIO` |
