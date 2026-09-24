# Plan de Corrección: Driver GT911 en TouchHAL (ESP32-P4)

> **Documento de Especificación y Plan**  
> **Ubicación:** `specs/architecture/plan_correccion_touch_gt911.md`  
> **Fecha:** 2026-09-24  
> **Objetivo:** Resolver el fallo de inicialización del controlador táctil Goodix GT911 (`ESP_ERR_INVALID_STATE` / `i2c transaction failed`) eliminando el pulso manual redundante y configurando adecuadamente el driver oficial `esp_lcd_touch_gt911` de Espressif.

---

## 1. Diagnóstico del Problema y Evidencia en Caliente

Al flashear el firmware en el ESP32-P4 tras la migración a los pines verificados de hardware (`RST=22`, `INT=21`), la consola serie reportó el siguiente error durante el arranque:

```text
I (1828) TouchHAL: === Inicializando TouchHAL (Goodix GT911 I2C SDA=7 SCL=8 RST=22 INT=21) ===
I (1890) TouchHAL: Pulso de Reset GT911 completado en GPIO 22
I (1890) TouchHAL: GT911 detectado en dirección 0x5D
I (1890) GT911: I2C address initialization procedure skipped - using default GT9xx setup
E (1911) lcd_panel.io.i2c: panel_io_i2c_rx_buffer(149): i2c transaction failed
E (1911) GT911: touch_gt911_read_cfg(419): GT911 read error!
E (1912) GT911: esp_lcd_touch_new_i2c_gt911(169): GT911 init failed
E (1912) GT911: Error (0x103)! Touch controller GT911 initialization failed!
E (1912) TouchHAL: Fallo al inicializar touch GT911: ESP_ERR_INVALID_STATE
Aviso: Touch no detectado o fallo inicializacion
```

### Causa Raíz

1. **Doble pulso de reset desincronizado:**
   * En `TouchHAL.cpp` (líneas 44 a 59), se realizaba un pulso de reset manual en GPIO 22 (`RST`), bajándolo a `0` por 10 ms y subiéndolo a `1`, pero dejando `INT` (GPIO 21) en estado no coordinado.
   * Milisegundos después, `TouchHAL.cpp` invoca a `esp_lcd_touch_new_i2c_gt911()`, el cual vuelve a forzar un ciclo de reset por hardware en GPIO 22.

2. **Omisión de `driver_data` en el componente oficial:**
   * Al inspeccionar `bsp/esp32_p4_jc4880/managed_components/espressif__esp_lcd_touch_gt911/esp_lcd_touch_gt911.c` (líneas 108–149), se evidencia que si `tp_cfg.driver_data == NULL`:
     * El driver omite la secuencia de sincronización de pines (`I2C address initialization procedure skipped`).
     * Realiza un reset básico con un tiempo de espera de solo **10 ms** (en lugar de los 60 ms mínimos requeridos por el datasheet del GT911 para que su microcontrolador interno levante el firmware).
     * Inmediatamente después intenta leer los registros de configuración en I2C (`touch_gt911_read_cfg`), fallando por timeout de respuesta (`i2c transaction failed`) y abortando con error `0x103` (`ESP_ERR_INVALID_STATE`).

---

## 2. Modificaciones Propuestas

### Archivo Afectado
* [`bsp/esp32_p4_jc4880/hal/TouchHAL.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/TouchHAL.cpp)

### Cambios Concretos

1. **Eliminar el pulso manual redundante (Paso 2 del init en TouchHAL.cpp):**
   Eliminar el bloque que configuraba manualmente GPIO 22 como salida y enviaba pulsos arbitrarios, delegando todo el ciclo de encendido y control físico al driver oficial.

2. **Inyectar la configuración del dispositivo en `driver_data`:**
   Declarar e inicializar la estructura oficial `esp_lcd_touch_io_gt911_config_t` y asignarla a `tp_cfg.driver_data`:
   ```cpp
   esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
       .dev_addr = tp_addr,
   };

   esp_lcd_touch_config_t tp_cfg = {
       .x_max = (uint16_t)width,
       .y_max = (uint16_t)height,
       .rst_gpio_num = (gpio_num_t)cbdos::board::touch::PIN_RST,
       .int_gpio_num = (gpio_num_t)cbdos::board::touch::PIN_INT,
       .levels = {
           .reset = 0,
           .interrupt = 0,
       },
       .flags = {
           .swap_xy = 0,
           .mirror_x = 0,
           .mirror_y = 0,
       },
       .driver_data = &tp_gt911_config,
   };
   ```

Con este ajuste, el driver de Espressif:
* Mantiene `INT` en nivel lógico bajo antes y durante el flanco ascendente de `RST`, fijando la dirección en `0x5D` de acuerdo a la especificación de Goodix.
* Aplica la temporización completa (10 ms en bajo + 10 ms en alto + 50 ms de estabilización de arranque).
* Conmuta `INT` a modo entrada de interrupciones de forma atómica.
* Lee el ID del panel (`0x39, 0x31, 0x31` = "911") exitosamente.

---

## 3. Plan de Verificación

1. **Compilación Limpia:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
   ```
   *Criterio de éxito:* `Project build complete.`

2. **Flasheo al Hardware:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 -p /dev/ttyACM0 flash
   ```
   *Criterio de éxito:* Flasheo completado y reinicio automático.

3. **Verificación en Caliente:**
   * Monitorear los logs de arranque en `/dev/ttyACM0` para confirmar:
     ```text
     TouchPad_ID: 0x39,0x31,0x31
     TouchHAL GT911 inicializado correctamente (480x800)
     ```
   * Probar interacción táctil sobre la interfaz gráfica LVGL para confirmar la captura de coordenadas y eventos de pulsación.
