# Investigación y Plan de Reparación: Controlador Táctil AXS15231B en GhostESP

> **Ubicación:** `specs/hardware/axs15231b_touch_ghostesp_investigation_and_fix.md`  
> **Objetivo:** Documentar de manera aislada y limpia la auditoría técnica del panel táctil capacitivo AXS15231B en la placa **Guition JC3248W535 (ESP32-S3)**, comparando la implementación funcional y probada en hardware de **CBDos** con la arquitectura de entrada de **GhostESP**, definiendo la solución definitiva.

---

## 1. Fuente de la Verdad en Hardware Real: Implementación CBDos

En `cbdos`, el controlador táctil está probado y 100% operativo en las siguientes capas de código:

### A. Ubicación de Archivos de Referencia en CBDos
- Driver de bajo nivel: [`bsp/esp32_s3_jc3248/.pio/libdeps/esp32s3/JC3248W535 Driver/src/JC3248W535_Touch.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/.pio/libdeps/esp32s3/JC3248W535%20Driver/src/JC3248W535_Touch.cpp)
- Encabezado de pines y registros: [`bsp/esp32_s3_jc3248/.pio/libdeps/esp32s3/JC3248W535 Driver/src/JC3248W535_Touch.h`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/.pio/libdeps/esp32s3/JC3248W535%20Driver/src/JC3248W535_Touch.h)
- HAL de entrada del sistema: [`bsp/esp32_s3_jc3248/hal/hal_input_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_input_s3.cpp)
- Tareas de ejecución: [`bsp/esp32_s3_jc3248/src/main.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/src/main.cpp#L80-L95) y [`bsp/esp32_s3_jc3248/src/LuaLauncher.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/src/LuaLauncher.cpp#L99-L153)

### B. Especificación del Hardware Verificado en CBDos

| Parámetro | Valor Verificado en CBDos | Justificación Técnica |
|---|---|---|
| **Pin SDA** | `GPIO 4` | Conectado a la línea SDA del FPC del panel |
| **Pin SCL** | `GPIO 8` | Conectado a la línea SCL del FPC del panel |
| **Pin INT** | `GPIO 3` (Opcional) | Interrupción de toque; CBDos usa sondeo periódico estable a 400 kHz |
| **Pin RST** | `-1` (NC) | No conectado físicamente (reset integrado con el panel o por soft) |
| **Dirección I2C** | `0x3B` (7-bit) | Dirección del chip esclavo AXS15231B |
| **Velocidad I2C** | `400 kHz` (Fast Mode) | Garantiza latencia mínima en transferencias de 8 bytes |
| **Trama de Consulta** | `0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08` | Comando de 8 bytes para solicitar el reporte de 1 punto táctil |
| **Detección de Toque** | `data[0] == 0 && data[1] != 0` | `data[0]` = gesto (0 = tap/touch), `data[1]` = cantidad de puntos |
| **Coordenada X** | `((data[2] & 0x0F) << 8) \| data[3]` | 12 bits brutos directamente mapeados a 0..319 px |
| **Coordenada Y** | `((data[4] & 0x0F) << 8) \| data[5]` | 12 bits brutos directamente mapeados a 0..479 px |
| **Resolución** | `320 x 480` (Portrait nativo) | Sin escalas artificiales; resolución física 1:1 |

---

## 2. Auditoría Arquitectónica: ¿Cómo funciona el Toque en GhostESP?

Al auditar [`display_manager.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/main/managers/display_manager.c) y las vistas de GhostESP ([`main_menu_screen.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/main/managers/views/main_menu_screen.c), `app_gallery_screen.c`, etc.), se evidencia la arquitectura real del proyecto:

### A. El Sistema Reactivo de GhostESP (Queue vs. LVGL indev)
1. GhostESP **NO utiliza el subsistema de dispositivos de entrada (`lv_indev_t`) de LVGL** para la navegación de vistas, botones, selección de tarjetas, menús ni teclado.
2. Posee una tarea dedicada llamada **`hardware_input_task`** (`RawInput`, prioridad 3, periodo 10 ms = 100 Hz).
3. Cada 10 ms, `hardware_input_task` llama a la función de lectura del driver táctil (`touch_driver_read_axs15231b` en JC3248).
4. Si detecta presión, emite una estructura `InputEvent` con `event.type = INPUT_TYPE_TOUCH`, cargando `event.data.touch_data.point` y `event.data.touch_data.state = LV_INDEV_STATE_PR`.
5. Ese evento se envía a **`input_queue`**.
6. En el bucle principal de renderizado (`lvgl_tick_task` o `processEvent`), se extrae el evento de `input_queue` y se entrega a la vista activa:
   ```c
   dm.current_view->handle_input(dm.current_view, &event);
   ```
7. En [`display_manager.c` (líneas 2597–2608)](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/main/managers/display_manager.c#L2597-L2608), los propios desarrolladores de GhostESP explican que **está expresamente prohibido registrar un indev nativo en LVGL** simultáneamente, porque causa carreras donde un toque es procesado dos veces por dos sistemas desacoplados (*Banshee TSC2007 issue*).

---

## 3. Las Causas del Fallo en GhostESP para JC3248W535

Al revisar por qué el táctil no funcionaba en JC3248 en GhostESP, encontramos tres desconexiones críticas:

### Causa 1: `hardware_input_task` estaba totalmente desactivada en JC3248
En [`display_manager.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/main/managers/display_manager.c#L2652-L2708):
```c
#ifndef CONFIG_JC3248W535EN_LCD
    // Creación de lvgl_tick_task...
    ...
    // Creación de hardware_input_task (RawInput)...
    hardware_input_task_handle = xTaskCreateStatic(hardware_input_task, "RawInput", 4096, ...);
#endif
```
El `#ifndef` que pretendía evitar crear `lvgl_tick_task` (porque JC3248 usa `lvgl_port_task`) abarcó hasta la línea 2708, **apagando la creación de `hardware_input_task`**. Ningún toque podía ser leído ni enviado a `input_queue`.

### Causa 2: `enable_touch_polling` forzado a `false`
En [`display_manager.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/main/managers/display_manager.c#L5130-L5133):
```c
#ifdef CONFIG_JC3248W535EN_LCD
    /* The BSP pointer indev is the sole touch owner on this board. */
    enable_touch_polling = false;
#elif defined(CONFIG_USE_TOUCHSCREEN)
    enable_touch_polling = true;
```
Quien portó la placa asumió erróneamente que un puntero indev de LVGL manejaría la interfaz, desactivando el sondeo hacia `input_queue`.

### Causa 3: Puente roto en `touch_driver_read_axs15231b`
En [`lv_port.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/components/axs15231b/lv_port.c#L649-L661):
```c
void touch_driver_read_axs15231b(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    memcpy((void*)data, (void*)&lastTouchData, sizeof(lv_indev_data_t));
    lastTouchData.state = LV_INDEV_STATE_RELEASED;
}
```
`lastTouchData` sólo se actualizaba si el timer de LVGL llamaba a su indev (~30 ms). Luego `touch_driver_read_axs15231b` leía `lastTouchData` a 100 Hz y la borraba instantáneamente (`RELEASED`), provocando pérdida de toques y rebotes constantes.

---

94: ## 4. Solución Quirúrgica y Limpia (Implementada y Validada)
95: 
96: La solución alinea a JC3248 con el estándar del resto de pantallas táctiles de GhostESP y con los valores probados en CBDos:
97: 
98: 1. **Corrección de I2C Master en ESP-IDF v5 ([`esp_bsp.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/components/axs15231b/esp_bsp.c)):**
99:    - En ESP-IDF v5, `i2c_master_get_bus_handle(BSP_I2C_NUM, ...)` retorna `ESP_ERR_INVALID_STATE` cuando el bus no existe aún (el código upstream solo chequeaba `ESP_ERR_NOT_FOUND` y abortaba con error).
100:    - Se maneja `ESP_ERR_INVALID_STATE` permitiendo crear el bus con SDA=GPIO 4, SCL=GPIO 8, Pullup activado a 400 kHz.
101: 
102: 2. **Driver Táctil AXS15231B Nativo I2C:**
103:    - Se evitó el wrapper `esp_lcd_panel_io_i2c` (el cual introducía un byte `0xFF` de registro espurio al rechazar comandos planos de 8 bytes).
104:    - Se crea un dispositivo I2C esclavo directo `s_touch_i2c_dev` en dirección `0x3B`.
105:    - Lectura atómica vía `bsp_touch_read_raw`: transmisión de la trama de comando `0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08`, retardo `esp_rom_delay_us(50)` y lectura directa de 8 bytes.
106:    - Se desacopla `lvgl_port_add_touch` para prevenir carreras de eventos.
107: 
108: 3. **Lectura directa y sin latencia en [`lv_port.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/components/axs15231b/lv_port.c):**
109:    - `touch_driver_read_axs15231b` invoca directamente `bsp_touch_read_raw(&x, &y, &pressed)` cada 10 ms:
110:      ```c
111:      void touch_driver_read_axs15231b(lv_indev_drv_t *drv, lv_indev_data_t *data) {
112:          if (!data) return;
113:          uint16_t x = 0, y = 0;
114:          bool pressed = false;
115:          if (bsp_touch_read_raw(&x, &y, &pressed) == ESP_OK && pressed) {
116:              data->point.x = (lv_coord_t)x;
117:              data->point.y = (lv_coord_t)y;
118:              data->state = LV_INDEV_STATE_PR;
119:          } else {
120:              data->state = LV_INDEV_STATE_REL;
121:          }
122:      }
123:      ```
124: 
125: 4. **Habilitación de tarea en [`display_manager.c`](file:///home/kaber420/Documentos/proyectos/cbdos/references/ghostesp/main/managers/display_manager.c):**
126:    - Se limitó el `#ifndef CONFIG_JC3248W535EN_LCD` únicamente a la creación de `lvgl_tick_task`.
127:    - `hardware_input_task` (`RawInput`) se crea y ejecuta normalmente a 100 Hz.
128:    - Se configuró `enable_touch_polling = true;` para `CONFIG_JC3248W535EN_LCD`.
129: 
130: ---
131: 
132: ## 5. Estado de Validación en Hardware
133: 
134: - **Dispositivo:** Guition JC3248W535 (ESP32-S3 QFN56, 16MB Flash, 8MB Octal PSRAM).
135: - **Firmware:** GhostESP (ESP-IDF v5.5, FreeRTOS).
136: - **Resultado:** **APROBADO Y 100% OPERATIVO**. La pantalla responde a toques, deslizamientos y navegación fluida entre menús y aplicaciones.

