# 🔌 Plan Arquitectónico: Extirpación y Centralización del Bus I2C en BSP

**Estado:** Pendiente / Deuda Técnica Prioritaria  
**Categoría:** Arquitectura BSP / Desacoplamiento de Hardware  
**Target:** ESP32-P4 (`bsp/esp32_p4_jc4880`) y extensible a ESP32-S3 (`bsp/esp32_s3_jc3248`)  
**Fecha:** 15 de Octubre de 2026  

---

## 🎯 1. Diagnóstico del Problema y Justificación

Actualmente, en el BSP del ESP32-P4 ([`bsp/esp32_p4_jc4880/hal/`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/)), el bus de comunicaciones I2C opera bajo un **acoplamiento temporal y de responsabilidades indebido**:

```mermaid
graph TD
    subgraph Arquitectura Actual (Acoplada)
        A["TouchHAL (P4)"] -->|1. Inicializa Bus Maestro i2c_master_bus_t| B["Bus I2C Hardware (SDA=IO7, SCL=IO8)"]
        A -->|2. Expone getI2cBusHandle()| C["Handle I2C"]
        D["AudioHAL (ES8311)"] -->|3. Depende de TouchHAL::getInstance()| C
    end
```

### Problemas arquitectónicos identificados:
1. **Violación de Responsabilidad Única (SRP):** `TouchHAL` no debe ser el administrador de un bus de comunicaciones general del SoC. Su única responsabilidad debe ser leer coordenadas e interrupciones del chip táctil Goodix GT911.
2. **Acoplamiento Temporal Rígido:** `AudioHAL` asume ciegamente que `TouchHAL` fue inicializado antes. Si el orden de inicialización cambia o se deshabilita la pantalla en un perfil headless o de prueba, el audio crashea con un puntero nulo al bus.
3. **Dificultad de Expansión:** Agregar futuros periféricos I2C (sensores ambientales en puertos de expansión, RTC externo, PMIC o expansores de pines) requeriría seguir encadenando dependencias cruzadas absurdas contra `TouchHAL`.

---

## 🏗️ 2. Arquitectura Objetivo: `I2cMasterBus` Centralizado

La solución consiste en **extirpar el control del bus I2C de `TouchHAL`** y delegarlo en un componente singleton o gestor autónomo dentro del BSP:

```mermaid
graph TD
    subgraph Arquitectura Desacoplada Objetivo
        DT["cbdos_device_tree.h"] -->|Define Pines y Frecuencia| BUS["I2cMasterBus (BSP)"]
        BUS -->|Crea y Custodia| HW["Bus I2C Hardware (i2c_master_bus_handle_t)"]
        
        TOUCH["TouchHAL (GT911)"] -->|Solicita Device Handle (0x5D)| BUS
        AUDIO["AudioHAL (ES8311)"] -->|Solicita Device Handle (0x18)| BUS
        EXT["Futuros Sensores / Expansion|Qwiic"] -->|Solicita Device Handle| BUS
    end
```

### Principios de diseño del nuevo modelo:
1. **Totalmente Aislado en BSP:** El Core de CBDos (`core/`) no se entera ni cambia en nada; la abstracción HAL se mantiene intacta.
2. **Basado en ESP-IDF 5.x Nativo:** Utiliza la API moderna `i2c_new_master_bus()` e `i2c_master_bus_add_device()`, garantizando que el propio driver de Espressif maneje los mutex de hardware por transacción.
3. **Orden de Arranque Determinista:** `I2cMasterBus::init()` se invoca durante la secuencia temprana del BSP (`board_init()`), antes de que cualquier HAL específico (Touch, Audio o Sensores) despierte.

---

## 📋 3. Plan de Implementación Quirúrgica

### Paso 1: Creación de `I2cMasterBus` en BSP P4
* **Archivos:** `bsp/esp32_p4_jc4880/hal/I2cMasterBus.h` y `.cpp`.
* **Responsabilidades:**
  * Leer la configuración del puerto desde `cbdos_device_tree.h` (`I2C_PORT`, `I2C_SDA_PIN`, `I2C_SCL_PIN`, `I2C_FREQ`).
  * Llamar a `i2c_new_master_bus()` una sola vez durante el arranque del sistema.
  * Proveer métodos de registro de periféricos:
    ```cpp
    esp_err_t registerDevice(const i2c_device_config_t* dev_cfg, i2c_master_dev_handle_t* dev_handle);
    i2c_master_bus_handle_t getBusHandle() const;
    ```

### Paso 2: Limpieza y Extirpación en `TouchHAL`
* Eliminar `i2cBusHandle`, `i2c_bus_config` y la llamada a `i2c_new_master_bus()` de [`TouchHAL.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/TouchHAL.cpp).
* Eliminar el método `getI2cBusHandle()` de [`TouchHAL.h`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/TouchHAL.h).
* `TouchHAL` solicita su panel IO I2C directamente al nuevo `I2cMasterBus`.

### Paso 3: Limpieza y Desacoplamiento en `AudioHAL`
* En [`AudioHAL.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/AudioHAL.cpp#L46), reemplazar:
  ```cpp
  // ANTES (Acoplado):
  i2c_cfg.bus_handle = TouchHAL::getInstance().getI2cBusHandle();
  
  // DESPUÉS (Desacoplado):
  i2c_cfg.bus_handle = I2cMasterBus::getInstance().getBusHandle();
  ```
* Eliminar la inclusión de `#include "TouchHAL.h"` en `AudioHAL.cpp`.

---

## 🛡️ 4. Criterios de Aceptación
1. **Independencia de Compilación:** `AudioHAL.cpp` compila limpio sin incluir `TouchHAL.h`.
2. **Independencia en Ejecución:** Desactivar temporalmente la inicialización de `TouchHAL` no impide que `AudioHAL` inicialice el códec ES8311 y reproduzca sonido.
3. **Cero Regresiones:** El panel táctil Goodix GT911 (60 FPS) y el audio ES8311 continúan funcionando simultáneamente sin bloqueos ni contención de bus.
