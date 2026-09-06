# Diagnóstico Técnico: Subsistema Gráfico de Lua (cbdos.gfx) y Controlador MIPI-DSI ST7701S en ESP32-P4

**Fecha:** 28 de Agosto de 2026  
**Target:** ESP32-P4 (Guition JC4880P443C - Módulo JC-ESP32P4-M3 Rev 1.3)  
**Entorno:** ESP-IDF 5.5 / C++ / LVGL 9.5  
**Objetivo del documento:** Servir de insumo analítico para inspección y resolución en OpenCode.

---

## 1. Descripción del Problema Observado en Hardware

Al ejecutar scripts de Lua con modo gráfico directo fullscreen (como `demo_graficos.lua` o `tictactoe.lua`):
1. La primera pantalla se renderiza brevemente (se ve el gráfico 1 vez).
2. De inmediato, la pantalla entra en un ciclo de parpadeo continuo (flickering / reinicio visual de la imagen) o se congela.
3. Los scripts sin interfaz gráfica (consola, serial, audio, sistema) funcionan al 100%.

---

## 2. Arquitectura de Hardware de Pantalla (ESP32-P4)

* **Pantalla:** Panel IPS 4.3" ST7701S resolución 480x800.
* **Interfaz:** MIPI-DSI (2 data lanes) conectado internamente a un controlador de periférico DPI (RGB interface) con DMA de alta velocidad a 34 MHz pixel clock.
* **Configuración del Panel DPI (`DisplayHAL.cpp`):**
  ```c
  esp_lcd_dpi_panel_config_t dpi_config = {};
  dpi_config.num_fbs = 2; // Doble buffer hardware asignado por el driver DPI en PSRAM
  dpi_config.flags.use_dma2d = true;
  ```
* **Punteros de Framebuffer:**
  - `fb0` (Buffer 0 en PSRAM)
  - `fb1` (Buffer 1 en PSRAM)
  - El hardware DPI realiza un escaneo continuo (DMA streaming a 60 FPS) leyendo de uno de los dos buffers.

---

## 3. ¿Cómo Funciona la Interfaz de LVGL 9.5 (`LVGL_Port.cpp`)?

* LVGL está inicializado en modo `LV_DISPLAY_RENDER_MODE_FULL` compartiendo `fb0` y `fb1`:
  ```cpp
  lv_display_set_buffers(display, fb0, fb1, buffer_size_bytes, LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display, flushCallback);
  ```
* Tarea de LVGL (`lvgl_task` en Core 1):
  - Ejecuta `lv_timer_handler()` cada 5ms.
  - Cuando LVGL termina de dibujar un área o pantalla completa, dispara `flushCallback(disp, area, px_map)`.
* En `flushCallback`:
  ```cpp
  void LVGL_Port::flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
      if (!LuaBridge::isUIPaused()) {
          esp_cache_msync(px_map, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
          esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, w, h, px_map);
      }
      lv_display_flush_ready(disp);
  }
  ```

---

## 4. El Flujo de Ejecución de Scripts Gráficos de Lua

1. El script inicia con:
   ```lua
   cbdos.gfx.pause_ui(0) -- o pause_ui(30)
   ```
2. `LuaBridge::pauseUI()` pone la bandera `s_uiPausedIndefinite = true`.
3. El script procede a limpiar y dibujar en bucle:
   ```lua
   cbdos.gfx.clear(C_FONDO)
   cbdos.gfx.draw_rect(...)
   cbdos.gfx.draw_text(...)
   cbdos.gfx.flush()
   ```
4. Las funciones de dibujo en `LuaBridge.cpp` escriben directamente en memoria:
   ```cpp
   uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
   uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
   ```

---

## 5. Puntos Críticos de Falla Identificados para Análisis

### Falla A: Concurrencia entre `LuaRunnerTask` (Core 0) y `lvgl_task` (Core 1)
* Aunque `LuaBridge::isUIPaused()` es `true` y `flushCallback` no llama a `esp_lcd_panel_draw_bitmap`, **LVGL 9 sigue ejecutando `lv_timer_handler()` en Core 1 cada 5ms**.
* LVGL sigue intentando renderizar su escena en segundo plano sobre los buffers compartidos `fb0` o `fb1` porque sus timers y tareas de invalidación siguen activas.

### Falla B: Mecánica de Intercambio de Buffers (`esp_lcd_panel_draw_bitmap`) en DPI Panel
* En ESP-IDF, para un panel DPI con `num_fbs = 2`, la función `esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, w, h, buffer)` se utiliza para indicar al hardware: *"Cambia el buffer visible por este nuevo puntero en el siguiente VSYNC"*.
* Actualmente, `cbdos::display::flush()` en `hal_display_p4.cpp` hace:
  ```cpp
  void flush() {
      void* fb = DisplayHAL::getInstance().getFrameBuffer(0); // Siempre envía fb0
      esp_cache_msync(fb, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
      esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, w, h, fb);
  }
  ```
* Si el hardware DPI está escaneando `fb0` y se le ordena repetidamente hacer swap a `fb0`, o si no hay alternancia entre `fb0` y `fb1`, el DMA y la caché L2 entran en colisión (desgarro de pantalla y parpadeo continuo).

### Falla C: Coherencia de Memoria Caché L1/L2 del RISC-V en ESP32-P4
* Las escrituras directas de Lua (`setPixel`, `drawRect`, `drawLine`, `drawText`) modifican la memoria PSRAM pasando por la caché del Core 0.
* Si no se ejecuta `esp_cache_msync` sobre el buffer exacto antes de que el controlador DPI lea de la PSRAM física, el panel muestra datos antiguos, artefactos o frames en blanco.

---

## 6. Archivos Involucrados para Auditoría

1. `core/src/lua/LuaBridge.cpp` (Bindings de `cbdos.gfx.*`, `pause_ui`, `draw_*`, `clear`, `flush`).
2. `core/src/lua/LuaRunner.cpp` (Tarea FreeRTOS `luaTask`, hooks de interrupción y ciclo de vida).
3. `bsp/esp32_p4_jc4880/hal/hal_display_p4.cpp` (Implementación de `cbdos::display::flush()`, `getFramebuffer()`).
4. `bsp/esp32_p4_jc4880/hal/DisplayHAL.cpp` (Inicialización de MIPI-DSI, panel DPI ST7701S y asignación de `fb0`/`fb1`).
5. `bsp/esp32_p4_jc4880/hal/LVGL_Port.cpp` (Tarea `lvglTask`, `flushCallback` y sincronización).
6. `scripts/lua/demo_graficos.lua` y `scripts/lua/tictactoe.lua` (Scripts de prueba afectados).

---

## 7. Pregunta de Diseño para OpenCode

> ¿Cuál es la forma óptima en ESP-IDF 5.5 con panel DPI ST7701S (2 FBs en PSRAM) para permitir que una tarea externa (Lua) tome posesión exclusiva del framebuffer sin interferencia del bucle de LVGL 9 ni colisión de DMA/Caché en el ESP32-P4?
