# Especificación y Plan Técnico: Optimización de Memoria en ESP32-P4 (Homologación del Pool LVGL 9.5 en PSRAM y Ejecución XIP desde PSRAM)

**Fecha:** 29 de Octubre de 2026  
**Ubicación:** `specs/architecture/plan_homologacion_lvgl_psram_pool_p4_s3.md`  
**Estado:** Documentado / Listo para Implementación  
**Targets:** `bsp/esp32_p4_jc4880` (ESP32-P4) y `bsp/esp32_s3_jc3248` (ESP32-S3)

---

## 1. Diagnóstico Real de la Memoria SRAM Interna (ESP32-S3 vs. ESP32-P4)

Aunque la hoja de datos del **ESP32-P4** anuncia **768 KB de SRAM L2MEM** frente a los **512 KB de SRAM** del **ESP32-S3**, el análisis del mapa de enlazado real compilado (`bsp/esp32_p4_jc4880/build/cbdos_p4.map`) demuestra que el heap interno libre disponible para la aplicación es del mismo orden de magnitud en ambos procesadores (~256 KB a ~320 KB):

### 1.1. Distribución Real de la L2MEM en el ESP32-P4 (`cbdos_p4.map`)
* **-128 KB (`0x4ffa0000 .. 0x4ffc0000`):** Reservados por hardware para la **Caché L2** (`CONFIG_CACHE_L2_CACHE_128KB=y`).
* **-77 KB (`0x4ff2cbd0 .. 0x4ff40000`):** Reservados por la ROM y el Bootloader del SoC.
* **-149 KB (`0x4ff00000 .. 0x4ff24780` en `sram_low`):** Ocupados por código crítico `.iram`, tablas `.data` y variables estáticas `.bss` (~50 KB de `.bss` total en todo el firmware).
* **-32 KB (`CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768`):** Reserva obligatoria para descriptores DMA.
* **Pilas y Descriptores de Periféricos de Alta Velocidad:** Anillos DMA del controlador **MIPI-DSI (480×800)**, colas DMA de **SDIO (`esp_hosted`)** hacia el coprocesador ESP32-C6 y pilas de tareas FreeRTOS.

### 1.2. Elementos en SRAM Interna del P4 que SÍ son Útiles (No Tocar)
1. **Caché L2 de 128 KB:** Indispensable para que el CPU Dual-Core a 400 MHz acceda a la Flash y a los 32 MB de PSRAM Hexal a 200 MHz sin cuellos de botella.
2. **Pila de `lvgl_task` (32 KB) en SRAM Interna (`LVGL_Port.cpp`):** Cuando el usuario guarda ajustes en NVS/Flash desde un evento de la interfaz gráfica, ESP-IDF suspende momentáneamente la caché de memoria externa durante la escritura SPI Flash. Mantener la pila de `lvgl_task` en SRAM interna previene errores de tipo *Cache Error panic*.
3. **Descriptores DMA de MIPI-DSI y SDIO:** Requisito físico de los controladores DMA para sostener 60 FPS y el caudal de red del C6.
4. **Asignador Híbrido de Lua (`LuaEngine::psramAlloc` en `core/src/lua/LuaEngine.cpp`):** Ya desvía automáticamente a PSRAM cualquier bloque `>= 256 bytes` (con fallback a PSRAM para bloques menores si la SRAM interna se llena).

---

## 2. El Único Desperdicio Detectado: Asimetría del Heap de LVGL 9.5

Con `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384`, cualquier llamada a `malloc()` estándar menor a **16 KB (16,384 bytes)** es forzada a la **SRAM interna**.

Al comparar ambos BSPs se detectó una asimetría en cómo LVGL 9.5 aloja sus objetos (`lv_obj_t`, estilos, etiquetas, animaciones y descriptores, que miden entre **60 y 300 bytes** cada uno):

| Característica | ESP32-S3 (`bsp/esp32_s3_jc3248/include/lv_conf.h`) | ESP32-P4 (`bsp/esp32_p4_jc4880/sdkconfig`) — Estado Actual |
| :--- | :--- | :--- |
| **Asignador de LVGL** | `LV_STDLIB_BUILTIN` (TLSF propio de LVGL) | `CONFIG_LV_USE_CLIB_MALLOC=y` (`malloc` estándar de C) |
| **Origen de Memoria** | `LV_MEM_POOL_ALLOC` -> **2 MB en PSRAM** (`MALLOC_CAP_SPIRAM`) | `malloc()` sujeto a `ALWAYSINTERNAL=16384` -> **SRAM Interna** |
| **Consumo de SRAM Interna por Widgets UI** | **0 Bytes** (100% aislado en PSRAM) | **100% en SRAM Interna** (por medir `< 16 KB` cada widget) |
| **Fragmentación de SRAM Interna** | **Nula** al cambiar de vistas (`BaseView`) | **Continua** al crear y destruir árboles de widgets |

---

## 3. Solución Propuesta: Homologar el Pool de LVGL en PSRAM en el ESP32-P4

Para que ambas placas operen bajo la misma arquitectura, reaccionen igual ante vistas complejas o apps `.luapp` pesadas y dejen el **100% del margen de SRAM interna libre** para cuando el sistema realmente lo necesite (DMA, pilas FreeRTOS, SSH/TLS, buffers de radio):

### 3.1. Dimensionamiento del Pool en ESP32-P4 (4 MB de sus 32 MB PSRAM)
* En `bsp/esp32_p4_jc4880/hal/LVGL_Port.cpp` (línea 137) se inicializa la caché de imágenes de LVGL en **2 MB**:
  ```cpp
  lv_image_cache_resize(2 * 1024 * 1024, true);
  ```
* Por lo tanto, al activar `LV_STDLIB_BUILTIN` en el P4, el pool debe fijarse en **4 MB (`4194304` bytes)**:
  * **2 MB** para cubrir la caché de imágenes decodificadas (`lv_image_cache`).
  * **2 MB** libres para árboles de widgets, estilos y animaciones (idéntico al pool de 2 MB que tiene el ESP32-S3).

### 3.2. Mecanismo Limpio de Alocación en PSRAM (`LV_MEM_POOL_ALLOC`)
En el componente oficial `bsp/esp32_p4_jc4880/managed_components/lvgl__lvgl/src/stdlib/builtin/lv_mem_core_builtin.c` (líneas 75–81):
```c
#ifdef LV_MEM_POOL_ALLOC
    state.tlsf = lv_tlsf_create_with_pool((void *)LV_MEM_POOL_ALLOC(LV_MEM_SIZE), LV_MEM_SIZE);
#else
    static MEM_UNIT work_mem_int[LV_MEM_SIZE / sizeof(MEM_UNIT)] LV_ATTRIBUTE_LARGE_RAM_ARRAY;
    state.tlsf = lv_tlsf_create_with_pool((void *)work_mem_int, LV_MEM_SIZE);
#endif
```
* Si no se define `LV_MEM_POOL_ALLOC`, LVGL intenta crear un arreglo estático `work_mem_int` en `.bss` (SRAM interna).
* Definiendo globalmente `-DLV_MEM_POOL_ALLOC=malloc` en el `CMakeLists.txt` raíz del P4, `lv_mem_init()` hace una única llamada a `malloc(4194304)`.
* Como `4,194,304 bytes > 16,384 bytes` (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`), ESP-IDF adjudica ese bloque único de 4 MB **directamente en la PSRAM Hexal a 200 MHz** durante `lv_init()`, dejando **0 bytes** en `.bss` y **0 bytes** de gasto en SRAM interna.

---

## 4. Cambios Exactos para cuando se Ejecute la Implementación

### Paso 1: `bsp/esp32_p4_jc4880/CMakeLists.txt`
Añadir la definición `LV_MEM_POOL_ALLOC=malloc` antes de `project(cbdos_p4)`:
```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../../core"
)

add_compile_definitions(LV_MEM_POOL_ALLOC=malloc)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cbdos_p4)
```

### Paso 2: `bsp/esp32_p4_jc4880/sdkconfig`
Cambiar el asignador de LVGL de `CLIB` a `BUILTIN` con un pool de 4 MB (`4194304` bytes):
```ini
CONFIG_LV_USE_BUILTIN_MALLOC=y
# CONFIG_LV_USE_CLIB_MALLOC is not set
CONFIG_LV_USE_STDLIB_MALLOC=0
CONFIG_LV_MEM_SIZE=4194304
```

---

## 5. Fase Adicional Exclusiva para ESP32-P4: Ejecución Completa en PSRAM (`CONFIG_SPIRAM_XIP_FROM_PSRAM`)

Además de mover el heap de LVGL a PSRAM, el **ESP32-P4 (`JC4880P443C`)** cuenta con **32 MB de PSRAM Hexal (16-bit DDR a 200 MHz = ~400 MB/s)** en un controlador físico independiente (`MSPI0`), separado del bus Quad SPI de la Flash (`MSPI1` a ~80 MB/s).

Dado que el binario compilado de CBDos para P4 (`cbdos_p4.bin`) pesa **3.4 MB**, cabe holgadamente en los 32 MB de PSRAM.

### 5.1. ¿Qué hace `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` en ESP-IDF?
Definido en `/home/kaber420/esp/esp-idf/components/esp_psram/esp32p4/Kconfig.spiram`, activar `CONFIG_SPIRAM_XIP_FROM_PSRAM=y` selecciona automáticamente tres mecanismos a nivel de Bootloader y MMU sin modificar una sola línea de C++:
1. **`CONFIG_SPIRAM_FLASH_LOAD_TO_PSRAM=y`:** Durante el arranque (Bootloader de 2da etapa, antes de entrar a `app_main()`), las secciones de código y constantes se copian desde la Flash QSPI hacia la PSRAM Hexal (~60 ms adicionales en el boot).
2. **`CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y`:** El procesador RISC-V Dual-Core a 400 MHz ejecuta todas las instrucciones `.flash.text` desde la PSRAM a 400 MB/s en lugar de leerlas de la Flash QSPI (~80 MB/s).
3. **`CONFIG_SPIRAM_RODATA=y`:** Todas las constantes `.flash.rodata` (fuentes de LVGL, iconos, tablas de estilos, strings y bytecode) se leen a velocidad de PSRAM Hexal, y las escrituras en Flash (`nvs_commit`, SPIFFS, OTA) en `MSPI1` dejan de suspender la caché de código/rodata en `MSPI0`.

### 5.2. Prerrequisitos Ya Cumplidos en `bsp/esp32_p4_jc4880/sdkconfig`
El `sdkconfig` actual del P4 (líneas 1185–1196) **ya tiene habilitados todos los prerrequisitos**:
```ini
CONFIG_SOC_SPIRAM_XIP_SUPPORTED=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_BOOT_HW_INIT=y
CONFIG_SPIRAM_BOOT_INIT=y
```
Por lo tanto, activarlo consiste únicamente en cambiar en `bsp/esp32_p4_jc4880/sdkconfig`:
```ini
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
```

### 5.3. Consideración Importante al Flashear en Rama de Prueba
* Al activar `CONFIG_SPIRAM_XIP_FROM_PSRAM=y`, el **Bootloader de 2da etapa (`bootloader.bin`)** incorpora la lógica de inicialización de PSRAM y copia de particiones (`SPIRAM_FLASH_LOAD_TO_PSRAM`).
* Por ello, la primera vez que se flashee en la placa P4 se debe ejecutar el flasheo completo (`idf.py -C bsp/esp32_p4_jc4880 -p /dev/ttyACM0 flash`), y no solo `app-flash`, para que el nuevo `bootloader.bin` se grabe en `0x2000`.
* Verificar en el primer `build` que el tamaño de `bootloader.bin` encaje antes del offset `0x9000` donde inicia la tabla de particiones/NVS en `bsp/esp32_p4_jc4880/partitions.csv` (de ser necesario, `CONFIG_PARTITION_TABLE_OFFSET` puede subirse a `0x10000` si el bootloader supera los ~28 KB).

---

## 6. Resultado Esperado Conjunto
1. **Homogeneidad Total S3 / P4 en LVGL:** Ambos targets gestionan toda la memoria dinámica de LVGL 9.5 mediante el asignador TLSF interno (`LV_STDLIB_BUILTIN`) alojado en un bloque continuo de PSRAM (2 MB en S3, 4 MB en P4).
2. **Máximo Margen de SRAM Interna en P4:** Todos los widgets, contenedores, tablas y etiquetas de las vistas dejan de consumir y fragmentar la SRAM interna del ESP32-P4, reservándola exclusivamente para DMA, pilas de tareas, criptografía y subsistemas críticos.
3. **Ejecución Ultra-Rápida desde PSRAM en P4 (`XIP_FROM_PSRAM`):** El firmware completo del P4 corre desde su bus Hexal de 400 MB/s sin congelamientos durante escrituras en NVS/SPIFFS y sin tocar en absoluto el código compartido (`core/`) ni el BSP del ESP32-S3.


