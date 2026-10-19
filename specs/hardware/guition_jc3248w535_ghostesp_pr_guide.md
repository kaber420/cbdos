# Guía de Cambios Mínimos para Pull Request: Soporte Pantalla JC3248W535 (AXS15231B QSPI) en GhostESP

Esta guía documenta quirúrgicamente los **únicos cambios necesarios y útiles** para hacer funcionar desde cero la pantalla Guition JC3248W535 (ESP32-S3 + AXS15231B QSPI 320x480) en el repositorio de **GhostESP**, sin código experimental ni ruido residual.

---

## 1. Resumen de Archivos a Modificar en el PR

| Archivo | Propósito del Cambio |
|---|---|
| `configs/sdkconfig.JC3248W535EN` | Corrección de resolución (320x480), pin de backlight (GPIO 1) y límite de PSRAM (512B) para evitar OOM de SRAM interna. |
| `components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c` | Eliminación de comando `0x22` (All Pixels Off), soporte de streaming continuo QSPI (`RAMWR 0x2C` + `RAMWRC 0x3C`) y corrección de `disp_on_off`. |
| `components/axs15231b/esp_bsp.c` | Inicialización de GPIO 1 (Backlight), encendido del panel tras init y exportación del handle de tarea LVGL. |
| `components/axs15231b/lv_port.c` | Sincronización estricta de semáforo DMA post-slices y exportación de `g_lvgl_port_task_handle`. |
| `main/vendor/drivers/axs15231b.c` | Orientación nativa portrait `LVGL_PORT_ROTATION_DEGREE (0)`. |
| `main/managers/display_manager.c` | Vinculación del handle de tarea LVGL y hook de brillo de pantalla para JC3248W535. |

---

## 2. Diffs Quirúrgicos por Archivo

### Archivo 1: `configs/sdkconfig.JC3248W535EN`
Corrige la resolución física real (Portrait 320x480), el pin de retroiluminación correcto (GPIO 1) y el umbral de asignación interna de PSRAM para evitar agotar la memoria SRAM de 380 KB en el arranque:

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
+CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=512
+CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
 CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
+CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
 
@@ -2733,7 +2732,7 @@ CONFIG_LV_DISP_PIN_RST=5
 CONFIG_LV_DISP_BACKLIGHT_SWITCH=y
 CONFIG_LV_BACKLIGHT_ACTIVE_LVL=y
-CONFIG_LV_DISP_PIN_BCKL=32
+CONFIG_LV_DISP_PIN_BCKL=1
 CONFIG_LV_I2C_DISPLAY_PORT=0
```

---

### Archivo 2: `components/espressif__esp_lcd_axs15231b/esp_lcd_axs15231b.c`
1. Define el opcode `LCD_CMD_RAMWRC` (`0x3C`).
2. Elimina el comando destructivo `0x22` (*All Pixels Off*) de la secuencia de inicialización del panel.
3. En `panel_axs15231b_draw_bitmap`: en modo QSPI, el Slice 0 inicializa la ventana global (0..319, 0..479) con `RAMWR` (`0x2C`). Los slices consecutivos (1..9) transmiten de forma continua mediante `RAMWRC` (`0x3C`) sin enviar comandos `RASET` intermedios (los cuales descalibraban el barrido vertical y montaban los iconos).
4. Corrige la polaridad invertida en `panel_axs15231b_disp_off`.

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
 
-    // define an area of frame memory where MCU can access
-    tx_param(axs15231b, io, LCD_CMD_CASET, (uint8_t[]) {
-        (x_start >> 8) & 0xFF,
-        x_start & 0xFF,
-        ((x_end - 1) >> 8) & 0xFF,
-        (x_end - 1) & 0xFF,
-    }, 4);
+    if (axs15231b->flags.use_qspi_interface) {
+        if (y_start == axs15231b->y_gap) {
+            // Primer slice: ventana activa completa (320x480) y ancla en (0,0)
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
+            // Slices siguientes: continuar la escritura sin alterar RASET
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
+    }
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

### Archivo 3: `components/axs15231b/esp_bsp.c`
Garantiza que la retroiluminación PWM/GPIO arranque encendida al 100% y activa la pantalla tras el `init`:

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
```

---

### Archivo 4: `components/axs15231b/lv_port.c`
1. Exporta `TaskHandle_t g_lvgl_port_task_handle` para que el gestor de vistas de GhostESP sepa cuándo LVGL está listo.
2. Evita la condición de carrera por DMA al esperar el semáforo `trans_done_sem` después del último slice antes de llamar a `lv_disp_flush_ready(drv)`:

```diff
--- a/components/axs15231b/lv_port.c
+++ b/components/axs15231b/lv_port.c
@@ -68,6 +68,7 @@ typedef struct {
 * Local variables
 *******************************************************************************/
+TaskHandle_t g_lvgl_port_task_handle = NULL;
 static lvgl_port_ctx_t lvgl_port_ctx;
 static int lvgl_port_timer_period_ms = 5;
@@ -113,9 +114,9 @@ esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg)
     BaseType_t res;
     if (cfg->task_affinity < 0) {
-        res = xTaskCreate(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, NULL);
+        res = xTaskCreate(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, &g_lvgl_port_task_handle);
     } else {
-        res = xTaskCreatePinnedToCore(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, NULL, cfg->task_affinity);
+        res = xTaskCreatePinnedToCore(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority, &g_lvgl_port_task_handle, cfg->task_affinity);
     }
     ESP_GOTO_ON_FALSE(res == pdPASS, ESP_FAIL, err, TAG, "Create LVGL task fail!");
@@ -455,6 +456,35 @@ static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area,
         disp_ctx->trans_act = disp_ctx->trans_buf_1;
         int rotate = disp_ctx->sw_rotate;
 
+        if (rotate == LV_DISP_ROT_NONE) {
+            int max_height = disp_ctx->trans_size / width;
+            if (max_height > height) max_height = height;
+            if (max_height < 1) max_height = 1;
+
+            int trans_count = (height + max_height - 1) / max_height;
+            for (int i = 0; i < trans_count; i++) {
+                int local_y = i * max_height;
+                int slice_h = (height - local_y > max_height) ? max_height : (height - local_y);
+                int y_draw_start = y_start + local_y;
+                int y_draw_end = y_draw_start + slice_h - 1;
+
+                disp_ctx->trans_act = (disp_ctx->trans_act == disp_ctx->trans_buf_1) ? disp_ctx->trans_buf_2 : disp_ctx->trans_buf_1;
+                memcpy(disp_ctx->trans_act, color_map + (local_y * width), slice_h * width * sizeof(lv_color_t));
+
+                if (i == 0) {
+                    if (disp_ctx->draw_wait_cb) {
+                        disp_ctx->draw_wait_cb(disp_ctx->panel_handle->user_data);
+                    }
+                    xSemaphoreGive(disp_ctx->trans_done_sem);
+                }
+
+                xSemaphoreTake(disp_ctx->trans_done_sem, portMAX_DELAY);
+                esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, x_start, y_draw_start, x_end + 1, y_draw_end + 1, disp_ctx->trans_act);
+            }
+            xSemaphoreTake(disp_ctx->trans_done_sem, portMAX_DELAY);
+            lv_disp_flush_ready(drv);
+            return;
+        }
```

---

### Archivo 5: `main/vendor/drivers/axs15231b.c`
Fija la orientación por defecto en Portrait (Rotación 0):

```diff
--- a/main/vendor/drivers/axs15231b.c
+++ b/main/vendor/drivers/axs15231b.c
@@ -12,1 +12,1 @@
-#define LVGL_PORT_ROTATION_DEGREE (90)
+#define LVGL_PORT_ROTATION_DEGREE (0)
```

---

### Archivo 6: `main/managers/display_manager.c`
Conecta la tarea de LVGL inicializada por el driver AXS15231B con el scheduler de GhostESP y añade el hook para control de brillo:

```diff
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

## 3. Comprobación y Verificación

Para compilar y validar que el PR genera el firmware limpio y sin errores:
```bash
. $HOME/esp/esp-idf/export.sh
idf.py -C references/ghostesp build
```
Y para flashear a la placa:
```bash
idf.py -C references/ghostesp -p /dev/ttyACM0 flash
```
