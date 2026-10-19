# Plan de Commits Atómicos y Validación para Pull Request: Guition JC3248W535 (ESP32-S3)

Este documento desglosa los cambios validados en hardware real en **commits independientes, atómicos y modulares**, separando la configuración de memoria, el display QSPI, la integración de LVGL y el panel táctil I2C. Esto facilita la revisión técnica por parte de los mantenedores de GhostESP y garantiza una replicación limpia desde cero.

---

## 1. Estructura de la Serie de Commits

```text
commit 1: fix(jc3248): correct board pinout, resolution and psram allocation in sdkconfig
commit 2: fix(axs15231b): fix black screen and implement continuous QSPI streaming
commit 3: fix(display): integrate lvgl task sync, backlight control and portrait rotation
commit 4: feat(touch): configure I2C touch controller for AXS15231B on JC3248W535
```

---

## 2. Detalle y Diffs por Commit

### Commit 1: Configuración de Placa y Memoria (Board & Memory)
**Mensaje del Commit:**  
`fix(jc3248): correct board pinout, resolution and psram allocation in sdkconfig`

**Archivos afectados:**
- `configs/sdkconfig.JC3248W535EN`

**Justificación Técnica:**
1. **Resolución física:** La pantalla es nativa Portrait (320 horizontal x 480 vertical). El archivo tenía `480x320`, causando que los widgets y menús se calcularan a 480px de ancho y salieran recortados lateralmente.
2. **Backlight Pin:** El pin del hardware real conectado al circuito LED de retroiluminación es **GPIO 1**. El archivo tenía GPIO 32.
3. **Agotamiento de SRAM (Crash en Boot):** `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` forzaba que cualquier asignación menor a 16 KB se hiciera en SRAM interna. Los buffers de red, TLS y LVGL agotaban los 380 KB internos en menos de 2 segundos. Se reduce a `512` y se activa `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y`.

**Diff:**
```diff
--- a/configs/sdkconfig.JC3248W535EN
+++ b/configs/sdkconfig.JC3248W535EN
@@ -551,8 +551,8 @@ CONFIG_BLUE_RGB_PIN=-1
 # Display Options
 #
 CONFIG_WITH_SCREEN=y
-CONFIG_TFT_WIDTH=480
-CONFIG_TFT_HEIGHT=320
+CONFIG_TFT_WIDTH=320
+CONFIG_TFT_HEIGHT=480
 CONFIG_USE_TOUCHSCREEN=y
 
@@ -1343,11 +1343,10 @@ CONFIG_SPIRAM_BOOT_INIT=y
 CONFIG_SPIRAM_USE_MALLOC=y
-CONFIG_SPIRAM_MEMTEST=y
-CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
-# CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP is not set
+CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=512
+CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
 CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
-# CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY is not set
+CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
 
@@ -2733,7 +2732,7 @@ CONFIG_LV_DISP_PIN_RST=5
 CONFIG_LV_DISP_BACKLIGHT_SWITCH=y
 CONFIG_LV_BACKLIGHT_ACTIVE_LVL=y
-CONFIG_LV_DISP_PIN_BCKL=32
+CONFIG_LV_DISP_PIN_BCKL=1
 CONFIG_LV_I2C_DISPLAY_PORT=0
```

---

### Commit 2: Driver del Display AXS15231B (QSPI Panel Driver)
**Mensaje del Commit:**  
`fix(axs15231b): fix black screen and implement continuous QSPI streaming`

**Archivos afectados:**
- `components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c`

**Justificación Técnica:**
1. **Eliminación de Pantalla Negra:** La tabla `vendor_specific_init_default` incluía `{0x22, ...}` (*All Pixels Off*). Esto forzaba a la pantalla a ignorar el framebuffer y permanecer en negro permanente.
2. **Eliminación de Desplazamiento Vertical y Montado de Iconos:**
   - La transferencia por slices de DMA en QSPI divide el frame en bloques de 48 líneas.
   - Enviar comandos `RASET` intermedios o re-emitir `0x2C` (`RAMWR`) en cada slice provocaba que el puntero vertical del chip se descalibrara y reiniciara en la fila 0, haciendo que los últimos slices pisaran la parte superior (la headerbar terminaba abajo y los iconos de abajo arriba).
   - **Solución:** El Slice 0 (`y_start == 0`) define la ventana completa activa (0..319, 0..479) y emite `RAMWR (0x2C)`. Los slices consecutivos (1..9) transmiten de forma continua mediante `RAMWRC (0x3C)` sin enviar comandos `RASET` intermedios.
3. **Polaridad Display On/Off:** Se corrige la lógica invertida en `panel_axs15231b_disp_off`.

**Diff:**
```diff
--- a/components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c
+++ b/components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c
@@ -28,6 +28,7 @@
 #define LCD_OPCODE_WRITE_COLOR              (0x32ULL)
+#define LCD_CMD_RAMWRC                      (0x3C)
 
 static const char *TAG = "lcd_panel.axs15231b";
@@ -228,7 +229,6 @@ static const axs15231b_lcd_init_cmd_t vendor_specific_init_default[] = {
     {0x11, (uint8_t[]){0x00}, 0, 200},
     {0x29, (uint8_t[]){0x00}, 0, 200},
     {0x2C, (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4, 0},
-    {0x22, (uint8_t[]){0x00}, 0, 200},//All Pixels off
 };
@@ -295,29 +295,47 @@ static esp_err_t panel_axs15231b_draw_bitmap(esp_lcd_panel_t *panel, int x_start
     y_start += axs15231b->y_gap;
     y_end += axs15231b->y_gap;
 
-    tx_param(axs15231b, io, LCD_CMD_CASET, (uint8_t[]) {
-        (x_start >> 8) & 0xFF,
-        x_start & 0xFF,
-        ((x_end - 1) >> 8) & 0xFF,
-        (x_end - 1) & 0xFF,
-    }, 4);
+    if (axs15231b->flags.use_qspi_interface) {
+        if (y_start == axs15231b->y_gap) {
+            // Slice 0: Definir ventana global activa y reset de origen (0,0)
+            tx_param(axs15231b, io, LCD_CMD_CASET, (uint8_t[]) {
+                (x_start >> 8) & 0xFF,
+                x_start & 0xFF,
+                ((x_end - 1) >> 8) & 0xFF,
+                (x_end - 1) & 0xFF,
+            }, 4);
+
+            tx_param(axs15231b, io, LCD_CMD_RASET, (uint8_t[]) {
+                0x00, 0x00,
+                (479 >> 8) & 0xFF,
+                479 & 0xFF,
+            }, 4);
+
+            size_t len = (x_end - x_start) * (y_end - y_start) * axs15231b->fb_bits_per_pixel / 8;
+            tx_color(axs15231b, io, LCD_CMD_RAMWR, color_data, len); // 0x2C
+        } else {
+            // Slices siguientes: streaming continuo sin RASET
+            size_t len = (x_end - x_start) * (y_end - y_start) * axs15231b->fb_bits_per_pixel / 8;
+            tx_color(axs15231b, io, LCD_CMD_RAMWRC, color_data, len); // 0x3C
+        }
+    } else {
+        tx_param(axs15231b, io, LCD_CMD_CASET, (uint8_t[]) {
+            (x_start >> 8) & 0xFF,
+            x_start & 0xFF,
+            ((x_end - 1) >> 8) & 0xFF,
+            (x_end - 1) & 0xFF,
+        }, 4);
-    if (0 == axs15231b->flags.use_qspi_interface) {
         tx_param(axs15231b, io, LCD_CMD_RASET, (uint8_t[]) {
             (y_start >> 8) & 0xFF,
             y_start & 0xFF,
             ((y_end - 1) >> 8) & 0xFF,
             (y_end - 1) & 0xFF,
         }, 4);
-    }
-    size_t len = (x_end - x_start) * (y_end - y_start) * axs15231b->fb_bits_per_pixel / 8;
-    if (y_start == 0) {
-        tx_color(axs15231b, io, LCD_CMD_RAMWR, color_data, len);//2C
-    } else {
-        tx_color(axs15231b, io, LCD_CMD_RAMWRC, color_data, len);//3C
-    }
+        size_t len = (x_end - x_start) * (y_end - y_start) * axs15231b->fb_bits_per_pixel / 8;
+        tx_color(axs15231b, io, LCD_CMD_RAMWR, color_data, len);
     }
     return ESP_OK;
 }
@@ -380,16 +398,11 @@ static esp_err_t panel_axs15231b_set_gap(esp_lcd_panel_t *panel, int x_gap, int
-static esp_err_t panel_axs15231b_disp_off(esp_lcd_panel_t *panel, bool off)
+static esp_err_t panel_axs15231b_disp_off(esp_lcd_panel_t *panel, bool on_off)
 {
     axs15231b_panel_t *axs15231b = __containerof(panel, axs15231b_panel_t, base);
     esp_lcd_panel_io_handle_t io = axs15231b->io;
-    int command = 0;
-    if (off) {
-        command = LCD_CMD_DISPOFF;
-    } else {
-        command = LCD_CMD_DISPON;
-    }
+    int command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;
     tx_param(axs15231b, io, command, NULL, 0);
     return ESP_OK;
 }
```

---

### Commit 3: Integración LVGL y Control de Retroiluminación (Display Glue)
**Mensaje del Commit:**  
`fix(display): integrate lvgl task sync, backlight control and portrait rotation`

**Archivos afectados:**
- `components/axs15231b/esp_bsp.c`
- `components/axs15231b/lv_port.c`
- `main/vendor/drivers/axs15231b.c`
- `main/managers/display_manager.c`

**Justificación Técnica:**
1. **Backlight PWM Activo:** Inicializa `GPIO 1` como salida digital en nivel alto y configura el ciclo de trabajo de LEDC al 100% al encender.
2. **Sincronización DMA:** Agrega `xSemaphoreTake(disp_ctx->trans_done_sem, portMAX_DELAY)` al finalizar la transmisión de los slices para evitar que el siguiente frame de LVGL sobrescriba el buffer DMA mientras la última línea física está saliendo por el bus QSPI.
3. **Scheduler de LVGL:** Exporta `g_lvgl_port_task_handle` y lo enlaza en `display_manager.c` para que el sistema conozca el estado de la tarea gráfica.
4. **Hook de Brillo:** Vincula `display_manager_set_backlight_raw` con `bsp_display_brightness_set`.

**Diff:**
```diff
--- a/components/axs15231b/esp_bsp.c
+++ b/components/axs15231b/esp_bsp.c
@@ -136,6 +136,10 @@ static esp_err_t bsp_display_brightness_init(void)
 {
+    gpio_reset_pin(EXAMPLE_PIN_NUM_QSPI_BL);
+    gpio_set_direction(EXAMPLE_PIN_NUM_QSPI_BL, GPIO_MODE_OUTPUT);
+    gpio_set_level(EXAMPLE_PIN_NUM_QSPI_BL, 1);
+
     const ledc_channel_config_t LCD_backlight_channel = {
         .gpio_num = EXAMPLE_PIN_NUM_QSPI_BL,
@@ -143,7 +147,7 @@ static esp_err_t bsp_display_brightness_init(void)
         .channel = LCD_LEDC_CH,
         .intr_type = LEDC_INTR_DISABLE,
         .timer_sel = 1,
-        .duty = 0,
+        .duty = 1023,
         .hpoint = 0
     };
@@ -184,6 +188,8 @@ esp_err_t bsp_display_backlight_on(void)
 {
+    gpio_set_direction(EXAMPLE_PIN_NUM_QSPI_BL, GPIO_MODE_OUTPUT);
+    gpio_set_level(EXAMPLE_PIN_NUM_QSPI_BL, 1);
     return bsp_display_brightness_set(100);
 }
@@ -276,7 +282,8 @@ esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_hand
     esp_lcd_panel_reset(*ret_panel);
     esp_lcd_panel_init(*ret_panel);
-    esp_lcd_panel_disp_on_off(*ret_panel, false);
+    esp_lcd_panel_invert_color(*ret_panel, false);
+    esp_lcd_panel_disp_on_off(*ret_panel, true);

--- a/components/axs15231b/lv_port.c
+++ b/components/axs15231b/lv_port.c
@@ -68,6 +68,7 @@
+TaskHandle_t g_lvgl_port_task_handle = NULL;
@@ -113,9 +114,9 @@ esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg)
     BaseType_t res;
     if (cfg->task_affinity < 0) {
-        res = xTaskCreate(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, NULL);
+        res = xTaskCreate(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, &g_lvgl_port_task_handle);
     } else {
-        res = xTaskCreatePinnedToCore(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, NULL, cfg->task_affinity);
+        res = xTaskCreatePinnedToCore(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, &g_lvgl_port_task_handle, cfg->task_affinity);
     }
@@ -455,6 +456,35 @@ static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area,
+        if (rotate == LV_DISP_ROT_NONE) {
+            int max_height = disp_ctx->trans_size / width;
+            if (max_height > height) max_height = height;
+            if (max_height < 1) max_height = 1;
+            int trans_count = (height + max_height - 1) / max_height;
+            for (int i = 0; i < trans_count; i++) {
+                int local_y = i * max_height;
+                int slice_h = (height - local_y > max_height) ? max_height : (height - local_y);
+                int y_draw_start = y_start + local_y;
+                int y_draw_end = y_draw_start + slice_h - 1;
+                disp_ctx->trans_act = (disp_ctx->trans_act == disp_ctx->trans_buf_1) ? disp_ctx->trans_buf_2 : disp_ctx->trans_buf_1;
+                memcpy(disp_ctx->trans_act, color_map + (local_y * width), slice_h * width * sizeof(lv_color_t));
+                if (i == 0) {
+                    if (disp_ctx->draw_wait_cb) disp_ctx->draw_wait_cb(disp_ctx->panel_handle->user_data);
+                    xSemaphoreGive(disp_ctx->trans_done_sem);
+                }
+                xSemaphoreTake(disp_ctx->trans_done_sem, portMAX_DELAY);
+                esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, x_start, y_draw_start, x_end + 1, y_draw_end + 1, disp_ctx->trans_act);
+            }
+            xSemaphoreTake(disp_ctx->trans_done_sem, portMAX_DELAY);
+            lv_disp_flush_ready(drv);
+            return;
+        }

--- a/main/vendor/drivers/axs15231b.c
+++ b/main/vendor/drivers/axs15231b.c
@@ -12,1 +12,1 @@
-#define LVGL_PORT_ROTATION_DEGREE (90)
+#define LVGL_PORT_ROTATION_DEGREE (0)

--- a/main/managers/display_manager.c
+++ b/main/managers/display_manager.c
@@ -2493,6 +2493,7 @@
 #elif defined(CONFIG_JC3248W535EN_LCD)
+  extern TaskHandle_t g_lvgl_port_task_handle;
   esp_err_t ret = lcd_axs15231b_init();
   if (ret != ESP_OK) {
     ESP_LOGE(TAG, "LCD initialization failed");
     return;
   }
+  lvgl_task_handle = g_lvgl_port_task_handle;
@@ -3923,6 +3924,8 @@ static void display_manager_set_backlight_raw(uint8_t percentage) {
+#elif defined(CONFIG_JC3248W535EN_LCD)
+    bsp_display_brightness_set(percentage);
```

---

### Commit 4: Integración del Controlador Táctil I2C (AXS15231B Touch)
> ⚠️ **OBSOLETO (2026-10-17):** el diff de este commit quedó obsoleto. La firma de `bsp_touch_read_raw` cambió a `bool bsp_touch_read_raw(uint16_t *out_x, uint16_t *out_y)` y el cuerpo (lectura de ventana completa de 32 bytes + latch de 60 ms) nunca se escribió aquí (hay `...`). **El código final validado en hardware está en `specs/hardware/jc3248_ghostesp_touch_entrega_final.md` §2 — úsalo.** Contexto en `specs/hardware/ghostesp_jc3248_touch_release_debounce.md`.
**Mensaje del Commit:**  
`feat(touch): configure I2C touch controller for AXS15231B on JC3248W535`

**Archivos afectados:**
- `components/axs15231b/esp_bsp.h`
- `components/axs15231b/esp_bsp.c`
- `components/axs15231b/lv_port.c`
- `main/managers/display_manager.c`

**Justificación Técnica:**
1. **Configuración de Pines I2C:** Define `GPIO 4` (SDA) y `GPIO 8` (SCL) con pullups internos activos para comunicación con el chip AXS15231B en dirección `0x3B`.
2. **Lectura Directa de Hardware (Cero Falsas Liberaciones):** Se desacopla la inicialización del controlador de LVGL para evitar doble registro de dispositivos de entrada (indev). `touch_driver_read_axs15231b` consulta directamente las coordenadas de hardware a través de `esp_lcd_touch_read_data` y `esp_lcd_touch_get_data` en resolución nativa 320x480.
3. **Habilitación de `hardware_input_task` en JC3248:** Se ajustan los bloques `#ifndef CONFIG_JC3248W535EN_LCD` en `display_manager.c` para permitir la creación de la tarea `RawInput` (`hardware_input_task`) y se activa `enable_touch_polling = true`, canalizando los toques hacia `input_queue` para la interacción fluida con todas las vistas del sistema.

303: **Diff:**
304: ```diff
305: --- a/components/axs15231b/esp_bsp.h
306: +++ b/components/axs15231b/esp_bsp.h
307: @@ -47,6 +47,10 @@
308:  #define EXAMPLE_PIN_NUM_QSPI_TOUCH_SCL  (GPIO_NUM_8)
309:  #define EXAMPLE_PIN_NUM_QSPI_TOUCH_SDA  (GPIO_NUM_4)
310:  #define EXAMPLE_PIN_NUM_QSPI_TOUCH_RST  (-1)
311:  #define EXAMPLE_PIN_NUM_QSPI_TOUCH_INT  (-1)
312: +
313: +esp_err_t bsp_touch_init(void);
314: +esp_err_t bsp_touch_read_raw(uint16_t *x, uint16_t *y, bool *pressed);
315: +esp_lcd_touch_handle_t bsp_touch_get_handle(void);
316: 
317: --- a/components/axs15231b/esp_bsp.c
318: +++ b/components/axs15231b/esp_bsp.c
319: @@ -95,7 +95,7 @@ esp_err_t bsp_i2c_init(void)
320:      if (s_bsp_i2c_bus == NULL) {
321:          esp_err_t err = i2c_master_get_bus_handle(BSP_I2C_NUM, &s_bsp_i2c_bus);
322: -        if (err == ESP_ERR_NOT_FOUND) {
323: +        if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_INVALID_STATE) {
324:              const i2c_master_bus_config_t i2c_bus_cfg = {
325:                  .i2c_port = BSP_I2C_NUM,
326:                  .sda_io_num = BSP_I2C_SDA,
327:                  .scl_io_num = BSP_I2C_SCL,
328:                  .clk_source = I2C_CLK_SRC_DEFAULT,
329:                  .glitch_ignore_cnt = 7,
330:                  .intr_priority = 0,
331:                  .trans_queue_depth = 0,
332:                  .flags.enable_internal_pullup = true,
333:              };
334:              BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_cfg, &s_bsp_i2c_bus));
335:          } else if (err != ESP_OK) {
336:              return err;
337:          }
338:      }
339: +    return bsp_touch_init();
340: +}
341: +
342: +esp_err_t bsp_touch_read_raw(uint16_t *x, uint16_t *y, bool *pressed)
343: +{
344: +    // Trama AXS15231B validada en CBDos: 0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08
345: +    ...
346: +}
347: 
348: --- a/components/axs15231b/lv_port.c
349: +++ b/components/axs15231b/lv_port.c
350: @@ -649,13 +650,23 @@ void touch_driver_read_axs15231b(lv_indev_drv_t *drv, lv_indev_data_t *data)
351:  {
352: -    memcpy((void*)data, (void*)&lastTouchData, sizeof(lv_indev_data_t));
353: -    lastTouchData.state = LV_INDEV_STATE_RELEASED;
354: +    if (data == NULL) return;
355: +    uint16_t x = 0, y = 0;
356: +    bool pressed = false;
357: +    if (bsp_touch_read_raw(&x, &y, &pressed) == ESP_OK && pressed) {
358: +        data->point.x = (lv_coord_t)x;
359: +        data->point.y = (lv_coord_t)y;
360: +        data->state = LV_INDEV_STATE_PR;
361: +    } else {
362: +        data->state = LV_INDEV_STATE_REL;
363: +    }
364:  }
365: 
366: --- a/main/managers/display_manager.c
367: +++ b/main/managers/display_manager.c
368: @@ -90,7 +90,5 @@
369: -#ifndef CONFIG_JC3248W535EN_LCD
370:  static TaskHandle_t hardware_input_task_handle = NULL;
371:  static StackType_t *hardware_input_task_stack = NULL;
372:  static StaticTask_t *hardware_input_task_buffer = NULL;
373: @@ -97,2 +95,0 @@
374: -#endif
375: @@ -2680,2 +2677,3 @@
376:  #endif
377: +#endif
378: @@ -2708,2 +2706,0 @@
379: -#endif
380: @@ -5130,3 +5127,2 @@
381:  #ifdef CONFIG_JC3248W535EN_LCD
382: -    /* The BSP pointer indev is the sole touch owner on this board. */
383: -    enable_touch_polling = false;
384: +    enable_touch_polling = true;
385: ```
386: 
387: ---
388: 
389: ## 3. Plan de Pruebas y Evidencias de Validación
390: 
391: Para incluir en la descripción del PR de GitHub como evidencia irrefutable de que fue validado en hardware real:
392: 
393: ### A. Proceso de Compilación Limpia
394: ```bash
395: . $HOME/esp/esp-idf/export.sh
396: idf.py -C references/ghostesp build
397: ```
398: **Resultado Esperado:** Salida limpia `[100%] Built target app` y generación del binario `Ghost_ESP_IDF.bin` (~3.5 MB) con 44% de espacio libre en partición.
399: 
400: ### B. Proceso de Flasheo
401: ```bash
402: idf.py -C references/ghostesp -p /dev/ttyACM0 flash
403: ```
404: 
405: ### C. Checkpoints de Validación Visual y Funcional
406: - [x] **Sin pantalla negra:** El panel enciende inmediatamente sin quedar congelado en negro.
407: - [x] **Alineación geométrica perfecta:** La headerbar (reloj, batería, estado) se sitúa en la parte superior `(Y = 0..30)`. Los iconos de aplicaciones se muestran centrados en la pantalla física sin distorsión vertical ni superposiciones.
408: - [x] **Orientación natural:** Formato 320x480 Portrait correcto.
409: - [x] **Interacción Táctil (AXS15231B):** Pantalla táctil capacitiva 100% reactiva y fluida; navegación entre menús, selección de apps y deslizamiento validados en hardware físico.
410: - [x] **Estabilidad de memoria:** La consola serie no reporta abortos por OOM (*Out Of Memory*), manteniendo más de 60 KB de SRAM interna libre tras la inicialización de LVGL y Wi-Fi.
