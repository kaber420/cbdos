# Resumen Técnico: JC4880P443C_I_W (ESP32-P4 + C6)

Documento de referencia con el análisis del hardware, mapa de memoria, controladores confirmados y respaldo del firmware de fábrica.

---

## 1. Ficha Técnica del Hardware

* **Módulo Principal:** ESP32-P4-M3
  * **CPU:** Doble núcleo RISC-V @ 400 MHz + LP Core de bajo consumo.
  * **Memoria Flash:** 16 MB SPI Flash.
  * **Memoria PSRAM:** 16/32 MB PSRAM de alta velocidad (Octal/Hexal SPI @ 200 MHz).
  * **Aceleración Gráfica:** Acelerador de Procesamiento de Píxeles (PPA 2D Hardware Accelerator).
* **Co-procesador Inalámbrico:** ESP32-C6 (WiFi 6, Bluetooth 5 BLE, Zigbee / Thread vía SDIO/SPI).
* **Pantalla:** 4.3 pulgadas IPS, resolución nativa 480×800.
* **Interfaz de Pantalla:** MIPI-DSI (2-lane D-PHY).
* **Controlador del Panel LCD:** ST7701S.
* **Panel Táctil:** Capacitivo Goodix GT911 sobre bus I2C.

---

## 2. Mapa de Particiones de la Memoria Flash (16 MB)

A partir del volcado de la tabla de particiones oficial en `0x8000`:

| Nombre | Offset en Flash | Tamaño | Propósito |
| :--- | :--- | :--- | :--- |
| **`bootloader`** | `0x00000000` | 32 KB (`0x8000`) | Bootloader secundario ESP-IDF |
| **`partition_table`** | `0x00008000` | 4 KB (`0x1000`) | Definición de particiones |
| **`nvs`** | `0x00009000` | 32 KB (`0x8000`) | Non-Volatile Storage (calibración / config) |
| **`factory_app`** | `0x00020000` | 7.00 MB (`0x700000`) | Binario de la aplicación demo de fábrica |
| **`storage`** | `0x00720000` | 6.00 MB (`0x600000`) | Assets, fuentes, imágenes y recursos |

---

## 3. Respaldo de Seguridad Realizado (Backup)

Todos los binarios se encuentran almacenados de forma segura en: `firmware/backup/`

* `bootloader.bin` (32 KB)
* `partition_table.bin` (4 KB)
* `nvs.bin` (32 KB)
* `factory_app.bin` (4.43 MB binario de código ejecutable)
* `storage.bin` (6.00 MB de recursos y assets)
* `JC4880P443C_full_16MB.bin` (**Imagen Flash monolítica completa de 16 MB**)

### Comando para restaurar la placa al estado de fábrica:
```bash
esptool -p /dev/ttyACM0 write-flash 0x0 firmware/backup/JC4880P443C_full_16MB.bin
```

---

## 4. Stack de Software y Drivers Confirmados

Al inspeccionar los símbolos del firmware de fábrica, se identificó el stack exacto:

1. **LCD Driver:** Componente `espressif/esp_lcd_st7701` en modo MIPI-DSI 2-lanes.
2. **Touch Driver:** Componente `espressif/esp_lcd_touch_gt911` sobre I2C.
3. **Port LVGL:** `espressif/esp_lvgl_port` con doble búfer en PSRAM y aceleración PPA activada.
4. **UI Framework del Demo:** `espressif/esp-brookesia` (interfaz estilo smartphone: `phone_app`, `app_launcher`, `navigation_bar`, `gesture`).

---

## 5. Recomendación para el Proyecto CBDos

* **¿Usar ESP-Brookesia?** **No**. Brookesia impone un estilo visual de teléfono móvil/tablet.
* **Arquitectura recomendada:** Usar directamente **LVGL 9** con la capa `esp_lvgl_port` y abstracción en `firmware/src/HAL/` para construir la interfaz personalizada de CBDos con total libertad y rendimiento óptimo.
