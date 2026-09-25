# Plan de Migración: Storage (SDMMC) hacia Device Tree (ESP32-P4)

> **Documento de Especificación y Plan Técnico**  
> **Ubicación:** `specs/architecture/plan_migracion_storage_a_device_tree.md`  
> **Fecha:** 2026-09-24  
> **Fase / Paso:** Fase A (Nivel 1) — Paso 5 del Plan de Migración CDT  
> **Objetivo:** Migrar los pines hardcodeados de la interfaz MicroSD (SDMMC 4-bit) en `hal_storage_p4.cpp` hacia el Device Tree autogenerado (`cbdos_device_tree.h`), preservando la alimentación LDO VO4 (3.3V) y los pull-ups internos.

---

## 1. Análisis del Código Actual en Funcionamiento

Al auditar la implementación actual en [`bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp), se identifican los siguientes elementos dependientes de la placa:

### A. Líneas de Señal SDMMC Slot 0 (Nativo ESP32-P4)
La tarjeta MicroSD opera mediante el periférico hardware SDMMC en Slot 0 (4-bit). Actualmente utiliza números de GPIO literales (`GPIO_NUM_39` a `GPIO_NUM_44`) en dos rutinas clave:

1. **Montaje Estándar (`mountSd()`, líneas 158–177):**
   - **Pull-ups de línea:**
     ```cpp
     gpio_pullup_en(GPIO_NUM_44); // CMD
     gpio_pullup_en(GPIO_NUM_39); // D0
     gpio_pullup_en(GPIO_NUM_40); // D1
     gpio_pullup_en(GPIO_NUM_41); // D2
     gpio_pullup_en(GPIO_NUM_42); // D3
     ```
   - **Asignación de Pines al Slot Config (`sdmmc_slot_config_t`):**
     ```cpp
     slot_config.clk = GPIO_NUM_43;
     slot_config.cmd = GPIO_NUM_44;
     slot_config.d0 = GPIO_NUM_39;
     slot_config.d1 = GPIO_NUM_40;
     slot_config.d2 = GPIO_NUM_41;
     slot_config.d3 = GPIO_NUM_42;
     ```

2. **Formateo Destructivo FAT32 (`formatSd()`, líneas 228–246):**
   - Replica idénticamente las llamadas a `gpio_pullup_en()` y la asignación a `slot_config` para inicializar el bus previo al formateo de clusters.

### B. Control de Alimentación LDO (VO4 / 3.3V)
En las líneas 23 y 39–48:
```cpp
#define BOARD_DISP_SD_LDO_CH 4
```
- El regulador interno LDO VO4 alimenta la ranura TF/MicroSD.
- **Alcance de preservación:** De acuerdo con la especificación `PLAN_MIGRACION_CDT_Y_DRIVERS.md`, los canales de reguladores LDO se mantienen como macro local en Fase A y se migrarán a parámetros en Fase B (Paso 7/8). No se modifica en este paso.

---

## 2. Correspondencia con el Device Tree (`cbdos_device_tree.h`)

En [`bsp/esp32_p4_jc4880/hal/cbdos_device_tree.h`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/cbdos_device_tree.h#L53-L61), el espacio de nombres `sdcard` ya contiene la definición exacta autogenerada a partir de `boards/jc4880p443.json`:

```cpp
// 4. Tarjeta MicroSD (SDMMC 4-bit)
namespace sdcard {
    constexpr int PIN_D0 = 39;
    constexpr int PIN_D1 = 40;
    constexpr int PIN_D2 = 41;
    constexpr int PIN_D3 = 42;
    constexpr int PIN_CLK = 43;
    constexpr int PIN_CMD = 44;
}
```

La correspondencia es 1:1 con el hardware activo y probado. No se requieren cambios en el archivo JSON ni regeneración del árbol.

---

## 3. Plan de Modificaciones Paso a Paso

### 3.1. Inclusión de Encabezado en `hal_storage_p4.cpp`
Agregar el encabezado del Device Tree al inicio de [`bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp):
```cpp
#include "cbdos_device_tree.h"
```

### 3.2. Sustitución de Pines Literales por Constantes del CDT
Sustituir cada referencia literal por `static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_*)`:

#### En `mountSd()`:
* Líneas 158–162:
  ```cpp
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3));
  ```
* Líneas 172–177:
  ```cpp
  slot_config.clk = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CLK);
  slot_config.cmd = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD);
  slot_config.d0 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0);
  slot_config.d1 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1);
  slot_config.d2 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2);
  slot_config.d3 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3);
  ```

#### En `formatSd()`:
* Líneas 228–232:
  ```cpp
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2));
  gpio_pullup_en(static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3));
  ```
* Líneas 241–246:
  ```cpp
  slot_config.clk = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CLK);
  slot_config.cmd = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_CMD);
  slot_config.d0 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D0);
  slot_config.d1 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D1);
  slot_config.d2 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D2);
  slot_config.d3 = static_cast<gpio_num_t>(cbdos::board::sdcard::PIN_D3);
  ```

---

## 4. Criterios de Aceptación y Validación

1. **Cero Números de GPIO Literales:**
   No debe quedar ninguna mención de `GPIO_NUM_39`..`44` en `hal_storage_p4.cpp`.
2. **Compilación Limpia:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
   ```
   *Criterio:* `Project build complete.` sin advertencias de tipos ni errores de link.
3. **Actualización del Tracker:**
   Actualizar `specs/architecture/PLAN_MIGRACION_CDT_Y_DRIVERS.md` marcando el Paso 5 como `✅`.

---

## 5. Propuesta de Mejora y Refactorización (Complementario)

Para eliminar la duplicación de código de configuración hardware en `mountSd()` y `formatSd()`, así como para corregir el bug de reporte de estado en el formateo destructivo, se encuentra documentada la propuesta técnica complementaria en:

👉 **[`specs/architecture/propuesta_refactor_storage_dry_y_formateo.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/propuesta_refactor_storage_dry_y_formateo.md)**

Asimismo, el plan integral para desacoplar el formateo del BSP hacia el Core del sistema operativo se especifica en:

👉 **[`specs/architecture/plan_rediseño_arquitectura_storage_core.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/plan_rediseño_arquitectura_storage_core.md)**


