# Especificación Técnica: Ejecución XIP desde PSRAM y Caché L2 de 256 KB / 128 B en ESP32-P4

**Fecha:** 29 de Octubre de 2026  
**Ubicación:** `specs/architecture/plan_esp32_p4_xip_psram_y_cache_l2_256kb.md`  
**Estado:** Documentado para Prueba en Rama Dedicada  
**Target Exclusivo:** `bsp/esp32_p4_jc4880` (Guition JC4880P443C — ESP32-P4 + ESP32-C6)  
**Impacto en `core/` y `bsp/esp32_s3_jc3248`:** Nulo (0 archivos modificados fuera de `bsp/esp32_p4_jc4880`)

---

## 1. Objetivo Técnico

Evaluar en una rama Git aislada la configuración de memoria de alto rendimiento para el **ESP32-P4**, combinando tres mecanismos complementarios:
1. **Pool TLSF de LVGL 9.5 en PSRAM (4 MB):** Aislar el 100% de los widgets y cachés de LVGL fuera de la SRAM interna (`L2MEM`).
2. **Ejecución en Sitio desde PSRAM (`CONFIG_SPIRAM_XIP_FROM_PSRAM=y`):** Cargar y ejecutar todo el firmware (`.flash.text` + `.flash.rodata`, ~3.4 MB) desde los **32 MB de PSRAM Hexal DDR a 200 MHz (~400 MB/s)** en lugar de la Flash QSPI (~80 MB/s).
3. **Expansión de Caché L2 a 256 KB con Línea de 128 Bytes (`CONFIG_CACHE_L2_CACHE_256KB=y` + `CONFIG_CACHE_L2_CACHE_LINE_128B=y`):** Duplicar el tamaño de la caché L2 en hardware y el ancho de ráfaga hacia el controlador Hexal PSRAM (`MSPI0`).

---

## 2. Fundamento de Hardware en el ESP32-P4 (`JC4880P443C`)

### 2.1. Buses Físicos Independientes (`MSPI0` vs. `MSPI1`)
* **PSRAM Hexal (`MSPI0`):** 32 MB conectados mediante bus de 16 bits DDR a 200 MHz (**~400 MB/s**).
* **Flash Quad SPI (`MSPI1`):** 16 MB conectados mediante bus de 4 bits SDR (**~80 MB/s**).
* Al activar `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, el Bootloader de 2da etapa copia las secciones `.flash.text` y `.flash.rodata` de `MSPI1` a `MSPI0` durante el arranque (~60 ms) y remapea la MMU.
* **Efecto directo:** El procesador RISC-V Dual-Core alimenta sus cachés a 400 MB/s y las operaciones de escritura/borrado en Flash (`nvs_commit`, SPIFFS, OTA) dejan de suspender la caché de instrucciones y constantes gráficas.

### 2.2. Balance de SRAM Interna (`L2MEM` de 768 KB) al Subir la Caché L2 a 256 KB
La memoria `L2MEM` del ESP32-P4 comparte sus 768 KB físicos entre la Caché L2 de hardware y la SRAM interna del sistema:

| Concepto en `L2MEM` | Estado Actual (`L2 = 128 KB`) | Con esta Especificación (`L2 = 256 KB` + `XIP` + `LVGL PSRAM`) |
| :--- | :--- | :--- |
| **Caché L2 Hardware** | 128 KB (`0x20000`) | **256 KB (`0x40000`)** (-128 KB de SRAM) |
| **Línea de Caché L2** | 64 Bytes | **128 Bytes** (ráfagas de 128 B en PSRAM Hexal) |
| **Código `.iram` de SPI Flash / Sistema** | ~98 KB en SRAM interna | **Se reduce en ~30–40 KB** (funciones movidas a PSRAM XIP) |
| **Consumo de Widgets LVGL 9.5 (`< 16 KB`)** | Alojados en SRAM interna (`CLIB_MALLOC`) | **0 KB en SRAM interna** (aislados en Pool de 4 MB en PSRAM) |
| **Reserva DMA (`SDIO esp_hosted` + `MIPI-DSI`)** | Intacta en SRAM interna | **Intacta en SRAM interna** |

Al sacar primero a LVGL de la SRAM interna y reducir `.iram` gracias a `XIP_FROM_PSRAM`, el sistema compensa la reducción de 128 KB al ampliar la Caché L2 a **256 KB**, manteniendo margen seguro para los anillos DMA de `esp_hosted` (coprocesador C6) y `MIPI-DSI`.

---

## 3. Cambios Exactos de Implementación (Rama de Prueba)

### 3.1. `bsp/esp32_p4_jc4880/CMakeLists.txt`
Redirigir la creación del pool TLSF de LVGL a `malloc` (que al pedir 4 MB > 16 KB se asigna directamente en PSRAM):
```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../../core"
)

add_compile_definitions(LV_MEM_POOL_ALLOC=malloc)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cbdos_p4)
```

### 3.2. `bsp/esp32_p4_jc4880/sdkconfig`
Aplicar los tres bloques de configuración:

#### A) Pool LVGL 9.5 de 4 MB en PSRAM (2 MB `lv_image_cache` + 2 MB widgets)
```ini
CONFIG_LV_USE_BUILTIN_MALLOC=y
# CONFIG_LV_USE_CLIB_MALLOC is not set
CONFIG_LV_USE_STDLIB_MALLOC=0
CONFIG_LV_MEM_SIZE=4194304
```

#### B) Ejecución XIP desde PSRAM (`MSPI0` @ 200 MHz Hexal)
En la sección `PSRAM config` (línea ~1511), reemplazar `# CONFIG_SPIRAM_XIP_FROM_PSRAM is not set` por:
```ini
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_SPIRAM_FLASH_LOAD_TO_PSRAM=y
```

#### C) Caché L2 de 256 KB y Línea de 128 Bytes
En la sección `Cache config` (líneas ~1558–1564), reemplazar la configuración de 128 KB / 64 B por:
```ini
# CONFIG_CACHE_L2_CACHE_128KB is not set
CONFIG_CACHE_L2_CACHE_256KB=y
# CONFIG_CACHE_L2_CACHE_512KB is not set
CONFIG_CACHE_L2_CACHE_SIZE=0x40000
# CONFIG_CACHE_L2_CACHE_LINE_64B is not set
CONFIG_CACHE_L2_CACHE_LINE_128B=y
CONFIG_CACHE_L2_CACHE_LINE_SIZE=128
```

### 3.3. Alineación de Respaldo en `bsp/esp32_p4_jc4880/hal/LVGL_Port.cpp`
En `LVGL_Port::init` (líneas 148–149), cambiar el alineamiento de respaldo de `64` a `CONFIG_CACHE_L2_CACHE_LINE_SIZE` (`128`) para que coincida siempre con el tamaño de línea de la Caché L2 si llegara a ejecutarse el fallback:
```cpp
buf1 = heap_caps_aligned_alloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE, buffer_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
buf2 = heap_caps_aligned_alloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE, buffer_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
```
*(Nota: `DisplayHAL.cpp` y `LVGL_Port::flushCallback` ya utilizan `ESP_CACHE_MSYNC_FLAG_UNALIGNED` en `esp_cache_msync`, y `esp_hosted` ajusta automáticamente su `HOSTED_MEM_ALIGNMENT` a `CONFIG_CACHE_L2_CACHE_LINE_SIZE` = 128).*

---

## 4. Procedimiento de Prueba y Validación en Rama Aislada

1. **Creación de rama de prueba:**
   ```bash
   git checkout -b exp/p4-xip-psram-l2-256k
   ```
2. **Compilación limpia del target ESP32-P4:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
   ```
   * **Verificación de Bootloader:** Confirmar que `build/bootloader/bootloader.bin` no supere el límite del offset `0x9000` (28 KB desde `0x2000`) definido en `bsp/esp32_p4_jc4880/partitions.csv`. Si supera los 28 KB, ajustar `nvs` / `otadata` / `app0` o `CONFIG_PARTITION_TABLE_OFFSET`.
3. **Flasheo Completo (Bootloader + App):**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 -p /dev/ttyACM0 flash monitor
   ```
4. **Métricas a Auditar en Consola Serie (`/dev/ttyACM0`):**
   * Tiempo de arranque del bootloader copiando los 3.4 MB a PSRAM.
   * `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` y `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` en reposo y durante un escaneo Wi-Fi/BLE o `LAN Recon` intenso con `esp_hosted`.
   * Fluidez de renderizado a 480×800 en fondos animados (`AnimatedWallpaper`) y apertura de vistas pesadas.
5. **Criterio de Contingencia (Fallback Instantáneo):**
   * Si bajo carga máxima de `esp_hosted` (SDIO) se observa que el margen de SRAM interna es demasiado ajustado con `L2 = 256 KB`, se mantiene `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` + `CONFIG_CACHE_L2_CACHE_LINE_128B=y` con `CONFIG_CACHE_L2_CACHE_128KB=y`, recuperando 128 KB de SRAM interna al instante.

