# Corrección de Repetición de Teclas (Key Multi-press) en JC3248W535: Lectura AXS15231B de Ventana Completa + Latch en GhostESP

> **Ubicación:** `specs/hardware/ghostesp_jc3248_touch_release_debounce.md`
> **Objetivo:** Documentar el síntoma de repetición múltiple de teclas (tecla marcada 2-7 veces) en el teclado en pantalla de GhostESP para la placa **Guition JC3248W535 (AXS15231B in-cell)**, la causa raíz (protocolo de lectura I2C incorrecto, no debouncing), el fix definitivo (lectura de la ventana de reporte completa de 32 bytes + latch de 60 ms) y la guía para replicarlo en limpio para el futuro Pull Request.
>
> **Nota histórica:** una primera iteración implementó un debounce de release en `main/managers/display_manager.c` (`hardware_input_task`). Mejoró el síntoma parcialmente (2-4 repeticiones) pero no eliminó la causa; fue descartada. El fix final vive **única y exclusivamente** en el driver táctil y equivale al protocolo que ya funcionaba en el firmware CBDos.

---

## 1. Síntoma

Cada toque en una tecla del teclado en pantalla (SSID/password) inserta **2 a 7 veces** el mismo carácter. Navegación por menús apenas enmascara el problema (publish suave tras el evento PR).

---

## 2. Causa Raíz (instrumentada en hardware con logs `TOUCHDBG`)

### 2.1 Arquitectura de entrada (no es doble registro de indev)

GhostESP no registra un `lv_indev_t` para el touch (`disp_indev = NULL`); la única ruta es el polling de `hardware_input_task` (10 ms → `touch_driver_read_axs15231b` → `bsp_touch_read_raw`) y la máquina de estados de flancos PR/REL que encola eventos hacia `keyboard_screen.c`. Cada evento PR inserta un carácter: la multiplicación implica que **la capa de lectura emite varios ciclos PR/REL por un solo contacto**.

### 2.2 Lectura I2C desalineada del panel (causa real)

El código inicial leía el reporte táctil con:

- comando `b5 ab a5 5a 00 00 00 08` (8 bytes, sin los 3 ceros de cierre) y
- una lectura I2C de tan solo **8 bytes**.

Tracería con `ESP_LOGW("TOUCHDBG", ...)` en `bsp_touch_read_raw` (capturas serial en `/tmp/opencode/touch_raw3.log`...`touch_raw7.log`): durante un toque real el panel intercala frames de **reporte válido** (`00 01 80 3e 00 55 ...`: gesto=0, n_puntos=1, coords) con frames de **relleno idle** `46464646 46464646`, y algunos frames con el reporte pero **rotado** (ej. `46 46 46 00 01 81 36 01`: cabecera del reporte desplazada 3 bytes). La lectura corta de 8 bytes deja el puntero interno del chip desincronizado: cada ciclo I2C arranca un chunk de 8 bytes del reporte de 32 bytes en un offset distinto, no siempre en la cabecera de reporte → frames REL espurios a mitad de pulsación → flancos PR/REL múltiples → un carácter duplicado por cada burst.

Además, en reposo el contenido completo es estable `46464646 ...`: cualquier byte leído es "no-toque" (correcto en ese caso, pero los frames de presión quedan fuera de la ventana leída).

### 2.3 El protocolo correcto ya estaba en el firmware CBDos

`bsp/esp32_s3_jc3248/hal/esp_lcd_axs15231b.c:454-549` (referencia de la placa que **sí funciona**):

1. comando de **11 bytes**: `{0xb5,0xab,0xa5,0x5a, 0x00,0x00, (len)>>8,(len)&0xff, 0x00,0x00,0x00}` con `len = AXS_MAX_TOUCH_NUMBER*6+2 = 32` (los 3 ceros de cierre cierran la transacción);
2. lectura I2C de **32 bytes** (ventana completa: 2 bytes de cabecera + 5 puntos × 6 bytes);
3. parseo del primer punto: `x = ((data[2]&0x0F)<<8)|data[3]; y = ((data[4]&0x0F)<<8)|data[5];`
4. **sanity-check del contador de puntos**: válido solo si `1 <= data[1] <= 5` — descarta frames de relleno (`0x46`, `0xff`).

### 2.4 Gaps que quedaban tras alinear el protocolo (instrumentados, resueltos con latch)

Con la lectura de 32 bytes desde la cabecera, el panel AÚN intercala frames de relleno (`0x46`) mientras el dedo está puesto (y hay ráfagas de 2-3 seguidas cuando el puntero de ventana avanza). Con la regla ingenua "una muestra REL = release" el contacto se sigue fracturando. Tracería posterior (touch_raw7.log): sin latch el teclado repetía; con la interpretación de estado latching:

> **Estado de toque = "vimos un frame de reporte válido en los últimos ~60 ms" (latch). Solo un silencio de frames válidos más largo que eso es un release real.**

Cero réplicas confirmadas (para hacer rápido y lento) y release < 100 ms.

---

## 3. Fix Final Implementado (Validado en Hardware)

**Archivos:**

- `components/axs15231b/esp_bsp.c` — `bsp_touch_read_raw` (función implementada localmente; upstream GhostESP no la tiene): protocolo 11-byte + ventana 32-byte + latch.
- Nº tras el fix completo: display_manager NO se modifica (el debounce fue eliminado; sólo subsisten los cambios previos del PR soporte, como `enable_touch_polling = true`, `disp_indev = NULL` y la tarea de polling ya presentes).

### 3.1 Comando + lectura de ventana completa

```c
/* AXS15231B touch reports live in a 32-byte window (5 points x 6 bytes +
 * 2 header bytes). Reading a shorter window desyncs the internal pointer
 * and returns mid-record garbage chunks, which toggle press/release and
 * make keys register several times. Request the full window with a
 * trailing-zero command so every read starts at the report header. */
static const uint8_t cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00};
uint8_t data[32] = {0};

esp_err_t ret = i2c_master_transmit(s_touch_i2c_dev, cmd, sizeof(cmd), pdMS_TO_TICKS(20));
if (ret != ESP_OK) return false;
esp_rom_delay_us(50);
ret = i2c_master_receive(s_touch_i2c_dev, data, sizeof(data), pdMS_TO_TICKS(20));
if (ret != ESP_OK) return false;

bool press = (data[0] == 0 && data[1] >= 1 && data[1] <= 5);
```

### 3.2 Latch con ventana temporal (hold window)

```c
/* The panel interleaves valid touch reports with idle (0x46 filler)
 * frames even while a finger is held down, so a single read cannot be
 * trusted as the touch state. Latch the touch: it stays "pressed" for a
 * short window after the last valid report, and only a gap longer than
 * that (i.e. no valid report for that long) is a real release. */
#define AXS_TOUCH_HOLD_MS 60

static bool s_valid_seen = false;
static uint16_t s_hold_x = 0, s_hold_y = 0;
static TickType_t s_last_valid_tick = 0;

TickType_t now = xTaskGetTickCount();

if (press) {
    uint16_t x = ((data[2] & 0x0F) << 8) | data[3];
    uint16_t y = ((data[4] & 0x0F) << 8) | data[5];
    s_hold_x = x;
    s_hold_y = y;
    s_last_valid_tick = now;
    s_valid_seen = true;
} else if (s_valid_seen &&
           (now - s_last_valid_tick) <= pdMS_TO_TICKS(AXS_TOUCH_HOLD_MS)) {
    /* Inside the hold window of a recent valid report: keep reporting
     * the touch with the last known coordinates. */
    press = true;
} else {
    s_valid_seen = false;
    return false;
}

uint16_t x = s_hold_x;
uint16_t y = s_hold_y;
if (x >= EXAMPLE_LCD_QSPI_H_RES) x = EXAMPLE_LCD_QSPI_H_RES - 1;
if (y >= EXAMPLE_LCD_QSPI_V_RES) y = EXAMPLE_LCD_QSPI_V_RES - 1;
*out_x = x;
*out_y = y;
return true;
```

### 3.3 Propiedades del diseño

| Propiedad | Valor |
|---|---|
| Réplicas eliminadas | Sí: el latch no permite REL hasta 60 ms tras el último frame válido; la máquina de flancos se evalúa sobre un estado lógico limpio |
| Rápido tecleo preservado | Sí: la ventanilla de 60 ms permite 2 toques consecutivos rapidísimos (analizado en touch_raw7.log) |
| Drags/swipes | Sí: los frames válidos renuevan `s_last_valid_tick` (coords en vivo durante el drag) |
| Latencia añadida al release | ≤ 60 ms (solo retrasa la limpieza del press del key) |
| Costo | 3 variables estáticas + 1 define, cero asignaciones dinámicas |

---

## 4. Guía de Implementación Limpia para el PR (upstream GhostESP)

Para aplicar como commit limpio sobre la rama del PR de soporte JC3248 (ver `guition_jc3248w535_ghostesp_pr_guide.md`):

1. `git checkout -b fix/jc3248-touch-protocol-and-latch`.
2. Aplicar en `components/axs15231b/esp_bsp.c`:
   - si upstream ya con `disp_indev = NULL` (del PR soporte) y polling por `hardware_input_task`, únicamente reemplazar el cuerpo de lectura de `bsp_touch_read_raw` por el de §3.1 + §3.2 (con los comentarios tal cual);
   - `esp_rom_sys.h` ya incluido por PR soporte.
3. NO tocar `main/managers/display_manager.c` en este commit (el debounce de release se descartó).
4. Verificación:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   idf.py -C references/ghostesp build
   idf.py -C references/ghostesp -p /dev/ttyACM0 flash monitor
   ```
5. Casos de prueba (todos los que deben pasar): 1 carácter por toque (lento + rápido), swipes intactos, wake por tap, control-center gesture.

**Mensaje de commit sugerido:**

```
fix(jc3248): read full AXS15231B touch window and latch the touch state

Reading only 8 bytes of the touch report desyncs the panel's internal
read pointer and returns mid-record garbage chunks, which toggle
press/release and register each on-screen keyboard key several times.
Additionally, the panel interleaves idle (0x46 filler) frames with valid
reports while a finger is held.

Query the full 32-byte touch window with the proper trailing-zero
command, parse the first point with a point-count sanity check (1-5),
and latch the touch for a short hold window (60 ms) after the last
valid report so filler frames no longer emit spurious releases.
```

---

## 5. Validación en Hardware (estado: ✅ aprobado por usuario)

- **Dispositivo:** Guition JC3248W535 (ESP32-S3, /dev/ttyACM0).
- **Firmas del fix instrumentado primero (con `TOUCHDBG`)**: sin réplicas (touch_raw7.log: ciclos de latch limpios por contacto); usuario confirmó *"ya funciona bien"*.
- **Luego limpieza**: removidos todos los logs `TOUCHDBG` (esp_bsp.c + display_manager.c) y el experimento de debounce de release de `hardware_input_task`; reconstruido y reflasheado; boot limpio sin cambios funcionales.
- **Resultados:** escribiendo el SSID/password cada toque inserta exactamente 1 carácter.

---

## 6. Historial de Descartes

- **Experimento de debounce en display_manager.c** (3 frames REL consecutivos para commit + resolver release pendiente en la rama PR): mejoró el síntoma parcialmente (antes 4-7, luego 2-4) pero NO cubre la ráfaga real de ≥3 frames de relleno a mitad de pulsación → **eliminado**.
- **Protocolo de 8 bytes de la referencia Arduino** (`JC3248W535_Touch.cpp`): funciona en su contexto (polling a cadencia distinta sobre la ventana completa) pero reproducido tal cual en GhostESP resulta incompatible con el router de polling a 100 Hz — sustituido por el protocolo 11-byte/32-byte del HAL CBDos.

## 7. Relación con Otros Documentos

- `specs/hardware/axs15231b_touch_ghostesp_investigation_and_fix.md`: hace funcionar el táctil (causas 1-3). Este documento es la pieza 4: el fix que lo hace **utilizable para escribir**.
- `specs/hardware/guition_jc3248w535_ghostesp_pr_guide.md` / `plan_commits_pr_jc3248w535_clean.md`: orden de commits del PR (este fix como commit aparte).
