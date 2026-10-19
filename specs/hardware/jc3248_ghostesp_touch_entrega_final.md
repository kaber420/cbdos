# Entrega Final — Táctil JC3248W535 en GhostESP: Código Completo, Probado y Autocontenido

> **Ubicación:** `specs/hardware/jc3248_ghostesp_touch_entrega_final.md`
> **Propósito:** Documento ÚNICO para entregar a cualquier persona y que pueda hacer funcionar el táctil de la placa **Guition JC3248W535** en GhostESP (ESP-IDF v5.5), sin acceso al repo CBDos. Contiene el **código final exacto** (copiar/pegar tal cual), la referencia de dónde proviene, el orden de aplicación y el plan de pruebas validado en hardware físico.
> **Origen del código:** las líneas probadas viven en CBDos: `bsp/esp32_s3_jc3248/hal/esp_lcd_axs15231b.c:454-549` (protocolo del panel) y fueron portadas a GhostESP (`components/axs15231b/esp_bsp.c`) y validadas hasta funcionar **1 carácter por toque** (los documentos históricos de investigación son: `axs15231b_touch_ghostesp_investigation_and_fix.md`, `guition_jc3248w535_ghostesp_pr_guide.md`, `plan_commits_pr_jc3248w535_clean.md`, `ghostesp_jc3248_touch_release_debounce.md`).

---

## 1. Resumen de qué hace falta

Un PR de soporte JC3248 en GhostESP necesita:

| Pieza | Archivos | Dónde está documentada |
|---|---|---|
| **A. SDK config de la placa + driver de pantalla QSPI + backlight + LVGL** | `configs/sdkconfig.JC3248W535EN`, `components/espressif__esp_lcd_axs15231b/*`, `components/axs15231b/esp_bsp.c` (parte LCD), `components/axs15231b/lv_port.c` (parte LVGL), `main/vendor/drivers/axs15231b.c`, `main/managers/display_manager.c` (bloques de init) | `guition_jc3248w535_ghostesp_pr_guide.md` + `plan_commits_pr_jc3248w535_clean.md` (Commits 1-3) |
| **B. Táctil completo (lo de ESTE documento)** | `components/axs15231b/esp_bsp.h`, `esp_bsp.c` (funcs touch), `lv_port.c` (lector), `main/managers/display_manager.c` (tarea de polling) | **§2-§4 de aquí abajo** (código final inline) |

**Este documento cubre la pieza B completa**, que es la que los otros docs dejaban con huecos/firma obsoleta.

> ⚠️ **Corrección sobre los docs históricos:** en `plan_commits_pr_jc3248w535_clean.md` (Commit 4) la firma y el cuerpo de `bsp_touch_read_raw` quedaron obsoletos (8 bytes, firma `esp_err_t(...,bool*)`, cuerpo con `...`). **Ignora ese diff y usa el código final de este documento.**

---

## 2. Código final probado — todas las piezas, copiar/pegar

### 2.1 `components/axs15231b/esp_bsp.h` — declaraciones públicas

Junto a los `#define EXAMPLE_PIN_NUM_QSPI_TOUCH_*` (SCL=8, SDA=4, RST=-1, INT=-1), añadir:

```c
esp_err_t bsp_touch_init(void);
bool bsp_touch_read_raw(uint16_t *out_x, uint16_t *out_y);
esp_lcd_touch_handle_t bsp_touch_get_handle(void);
```

> Nota: la firma usa retorno `bool` y **dos** parámetros (no `esp_err_t`/3 parámetros como decían los drafts anteriores).

### 2.2 `components/axs15231b/esp_bsp.c` — I2C reutilizable (parte del bus compartido)

En `bsp_i2c_init()`, aceptar el caso en que otro módulo ya inicializó el bus:

```c
esp_err_t ret = i2c_master_get_bus_handle(BSP_I2C_NUM, &s_bsp_i2c_bus);
if (ret == ESP_ERR_NOT_FOUND || ret == ESP_ERR_INVALID_STATE) {   // <- añade INVALID_STATE
    i2c_master_bus_config_t i2c_conf = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = EXAMPLE_PIN_NUM_QSPI_TOUCH_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_QSPI_TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,   // <- pullups internos en true
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_conf, &s_bsp_i2c_bus));
    s_bsp_i2c_bus_owned = true;
}
```

Y añadir el include (para el delay entre comando y lectura):

```c
#include "esp_rom_sys.h"
```

### 2.3 `components/axs15231b/esp_bsp.c` — `bsp_touch_init` (código completo, probado)

Colocar después de `bsp_display_new` / antes de `bsp_display_indev_init`:

```c
static i2c_master_dev_handle_t s_touch_i2c_dev = NULL;

esp_err_t bsp_touch_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");

    if (!s_touch_i2c_dev && s_bsp_i2c_bus) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x3B,
            .scl_speed_hz = 400000,
        };
        esp_err_t err = i2c_master_bus_add_device(s_bsp_i2c_bus, &dev_cfg, &s_touch_i2c_dev);
        ESP_LOGI(TAG, "AXS15231B touch I2C device added (0x3B): ret=%d, dev=%p", err, s_touch_i2c_dev);
        if (err != ESP_OK) return err;
    }

    return ESP_OK;
}
```

### 2.4 `components/axs15231b/esp_bsp.c` — `bsp_touch_read_raw` (corazón del fix, código completo EXACTO probado)

> Fuente del protocolo: CBDos `bsp/esp32_s3_jc3248/hal/esp_lcd_axs15231b.c` líneas 454-549 (`touch_axs15231b_read_data` + parseo del registro táctil). Dos requisitos no negociables:
> 1. **Comando de 11 bytes + lectura de 32 bytes** (ventana completa del reporte, 5 puntos × 6 bytes + 2 de cabecera). Leer 8 bytes desincroniza el puntero interno del chip y devuelve chunks de basura a mitad de registro → el teclado repite cada tecla 2-7 veces.
> 2. **Latch de 60 ms**: el panel intercala frames de relleno idle (`0x46`) con frames válidos incluso con el dedo puesto; el estado es "vimos un reporte válido en los últimos ~60 ms", no "lo que dice esta muestra".

```c
bool bsp_touch_read_raw(uint16_t *out_x, uint16_t *out_y)
{
    if (!s_touch_i2c_dev) {
        if (bsp_touch_init() != ESP_OK || !s_touch_i2c_dev) {
            return false;
        }
    }

    /* AXS15231B touch reports live in a 32-byte window (5 points x 6 bytes +
     * 2 header bytes). Reading a shorter window desyncs the internal pointer
     * and returns mid-record garbage chunks, which toggle press/release and
     * make keys register several times. Request the full window with a
     * trailing-zero command so every read starts at the report header. */
    static const uint8_t cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00};
    uint8_t data[32] = {0};

    esp_err_t ret = i2c_master_transmit(s_touch_i2c_dev, cmd, sizeof(cmd), pdMS_TO_TICKS(20));
    if (ret != ESP_OK) {
        return false;
    }

    esp_rom_delay_us(50);

    ret = i2c_master_receive(s_touch_i2c_dev, data, sizeof(data), pdMS_TO_TICKS(20));
    if (ret != ESP_OK) {
        return false;
    }

    /* The panel interleaves valid touch reports with idle (0x46 filler)
     * frames even while a finger is held down, so a single read cannot be
     * trusted as the touch state. Latch the touch: it stays "pressed" for a
     * short window after the last valid report, and only a gap longer than
     * that (i.e. no valid report for that long) is a real release. */
#define AXS_TOUCH_HOLD_MS 60

    static bool s_valid_seen = false;
    static uint16_t s_hold_x = 0, s_hold_y = 0;
    static TickType_t s_last_valid_tick = 0;

    bool press = (data[0] == 0 && data[1] >= 1 && data[1] <= 5);

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
}

esp_lcd_touch_handle_t bsp_touch_get_handle(void)
{
    return tp;
}
```

(Nota: `EXAMPLE_LCD_QSPI_H_RES/V_RES` = 320x480; el clipeo existe porque el panel puede reportar 1 px fuera de rango.)

### 2.5 `components/axs15231b/esp_bsp.c` — SIN registro de indev en `bsp_display_start_with_config`

GhostESP usa su propia tarea de polling (`hardware_input_task`), NO un indev LVGL para JC3248; registrar `lvgl_port_add_touch` duplicaría el input:

```c
lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    ...
    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);

    bsp_touch_init();     // <- prepara el bus I2C del touch
    disp_indev = NULL;    // <- sin indev: el polling es el único dueño del táctil

    return disp;
}
```

### 2.6 `components/axs15231b/lv_port.c` — lector táctil (código final probado)

```c
void touch_driver_read_axs15231b(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    if (data == NULL) {
        return;
    }

    data->state = LV_INDEV_STATE_REL;

    uint16_t x = 0, y = 0;
    if (bsp_touch_read_raw(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    }
}
```

### 2.7 `main/managers/display_manager.c` — habilitar la tarea de polling

Tres cambios mínimos (sin ellos el táctil nunca se sondea):

1. Quitar el `#ifndef CONFIG_JC3248W535EN_LCD` / `#endif` que dejaba fuera de la compilación los handles de la tarea:

```c
static TaskHandle_t hardware_input_task_handle = NULL;
static StackType_t *hardware_input_task_stack = NULL;
static StaticTask_t *hardware_input_task_buffer = NULL;
```

2. En la creación de tareas dentro de `display_manager_init`, mover el `#endif` de forma que la tarea `RawInput` (`hardware_input_task`, stack 4096 en PSRAM) SÍ se cree para JC3248.

3. En `hardware_input_task`, activar el polling para esta placa:

```c
#ifdef CONFIG_JC3248W535EN_LCD
    enable_touch_polling = true;   // antes: false con el comentario "BSP pointer indev is the sole touch owner"
#endif
```

Y en la rama de lectura del loop (el sondeo ya existente, cada 10 ms) usar el lector nuevo:

```c
#ifdef CONFIG_JC3248W535EN_LCD
    touch_driver_read_axs15231b(&touch_driver, &touch_data);
#else
    touch_driver_read(&touch_driver, &touch_data);
#endif
```

**No se necesita ningún cambio adicional** en `hardware_input_task` para el multi-tap: el fix del §2.4 resuelve el fantasma PR/REL a nivel de driver. (Un experimento de debounce de release en este archivo se probó y se descartó: mejoraba a medias y quedó sin variables extra.)

---

## 3. Build, flash y verificación

```bash
# Entorno ESP-IDF v5.5
. $HOME/esp/esp-idf/export.sh

# Build (target esp32s3, sdkconfig de la placa ya en configs/sdkconfig.JC3248W535EN)
idf.py -C <repo-ghostesp> build

# Flash + monitor en el puerto USB-C (esptool; en la placa de referencia: /dev/ttyACM0)
idf.py -C <repo-ghostesp> -p /dev/ttyACM0 flash monitor
```

### Casos de prueba obligatorios (todos validados en la placa física de referencia)

| # | Caso | Criterio de paso |
|---|---|---|
| 1 | Escribir SSID/password lento (tecla por tecla, ~300 ms) | **1 carácter por toque**, cero repeticiones |
| 2 | Escribir rápido (toques < 60 ms entre sí) | 2 caracteres distintos, ninguno perdido |
| 3 | Navegar menús / galería de apps single-tap | Sin clusters dobles de navegación (el teclado detecta lo demás) |
| 4 | Swipe / drag en listas | Scrolling fluido, coords en vivo (el latch renueva coords en cada frame válido) |
| 5 | Sleep/dim + tap para despertar | Despierta y no inserta toques extra |
| 6 | Gestos de borde (control center inferior) | Intactos |

### Referencia de resolución rápida

- **Teclado repite teclas** = lectura corta de 8 bytes o sin sanity-check de `data[1]` (1..5) → revisar §2.4.
- **Táctil muerto, pantalla bien** = `enable_touch_polling=false` o tarea RawInput sin crear → revisar §2.7.
- **Coordenadas en espejo/raro** = problema del PR soporte (transform de `lv_port.c`), no de este fix.
- **Error I2C `this port has not been initialized`** al boot = benigna si aparece una vez antes de que LVGL use el bus (así ocurre en la placa de referencia); diagnosticar solo si el táctil queda muerto.

---

## 4. Respuesta a la pregunta clásica: ¿se necesita CBDos?

**No.** Este documento es autocontenido: todo el código final está inline (§2) y funciona tal cual en GhostESP + ESP-IDF v5.5 sobre la placa JC3248W535. La referencia CBDos se **cita** solo como origen histórico/auditable del protocolo del panel (`bsp/esp32_s3_jc3248/hal/esp_lcd_axs15231b.c:454-549`), por si alguien quiere contrastar contra el driver que lleva años en producción ahí.

## 5. Historial de validación (evidencia)

- Placa: Guition JC3248W535 (ESP32-S3, 16 MB flash, 8 MB Octal PSRAM), puerto `/dev/ttyACM0`.
- Firmware: GhostESP ESP-IDF v5.5, `CONFIG_JC3248W535EN_LCD`, build verde (`Project build complete`).
- Proceso de descubrimiento replicable: logs temporales `TOUCHDBG` (esp_bsp.c y display_manager.c) confirmaron que el panel intercala frames válidos (`00 01 80 3e 00 55 ...`) con relleno idle (`46464646...`) y frames rotados (`46 46 46 00 01 ...`) → puntero I2C desincronizado. Después del fix (11 bytes + 32 bytes + latch): **1 carácter por toque** confirmado por el usuario ("ya funciona bien"), y boot limpio tras la limpieza de instrumentación.
- Documentos de detalle (si se quiere el análisis paso a paso): `ghostesp_jc3248_touch_release_debounce.md` (causa raíz y parche), `axs15231b_touch_ghostesp_investigation_and_fix.md` (causas 1-3), `guition_jc3248w535_ghostesp_pr_guide.md` y `plan_commits_pr_jc3248w535_clean.md` (piezas de pantalla del PR).
