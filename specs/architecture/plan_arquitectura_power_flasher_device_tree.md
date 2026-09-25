# Plan de Arquitectura y Auditoría de Hardware: Power Control y Flasher en Device Tree (ESP32-P4)

> **Documento de Arquitectura y Especificación Técnica**  
> **Ubicación:** `specs/architecture/plan_arquitectura_power_flasher_device_tree.md`  
> **Fecha:** 2026-09-24  
> **Fase / Paso:** Fase A (Nivel 1) — Paso 6 del Plan de Migración CDT  
> **Fuentes de Verdad:**
> - Esquemáticos oficiales de ingeniería Guition: [`specs/hardware/schematics/1_PWR.png`](../hardware/schematics/1_PWR.png) y [`3_ESP32-P4.png`](../hardware/schematics/3_ESP32-P4.png).
> - Hoja de especificación técnica oficial Espressif: *ESP32-P4 SoC Technical Reference Manual v0.4.1*, Apéndice A (Págs. 81 y 82).
> - Especificación de pines oficial del proyecto: [`specs/hardware/pinouts_and_ports.md`](../hardware/pinouts_and_ports.md).

---

## 1. Diagnóstico de Hardware y Hallazgo de Auditoría

En la arquitectura previa de CBDos, se asumía la existencia de un pin de control de energía del sistema asignado a `GPIO 36`:
- `bsp/esp32_p4_jc4880/hal/hal_flasher_p4.cpp:271-280` contenía código activo para configurar `GPIO_NUM_36` como salida digital y forzarlo a nivel alto (`1`), bajo el comentario: *"Asegurar que el carril ESP_3V3 (GPIO 36) esté energizado"*.
- El archivo de Device Tree [`boards/jc4880p443.json`](../../boards/jc4880p443.json) definía el nodo ficticio:
  ```json
  "power_control": {
    "pins": {
      "en": 36
    }
  }
  ```
- El compilador [`tools/cdtc.py`](../../tools/cdtc.py) generaba en consecuencia:
  ```cpp
  namespace power {
      constexpr int PIN_EN = 36;
  }
  ```

### Realidad Comprobada en el Hardware:
1. **Alimentación Principal (3.3V):**
   - Según el esquemático de potencia ([`1_PWR.png`](../hardware/schematics/1_PWR.png), bloque `电源3V3`), el convertidor reductor integrado es un **TLV62569 (AP1)**.
   - Su pin `EN` (Enable, pin 1) se encuentra soldado físicamente a `VIN` mediante una resistencia pull-up fija de 10 kΩ (`R23`) con desacoplo `C25`.
   - El carril `ESP_3V3` / `VCC3V3` se genera directamente a través del inductor `L1` (2.2 µH) de forma continua en cuanto la placa recibe alimentación de batería o USB. **Ningún GPIO conmuta ni puede apagar la alimentación de 3.3V.**

2. **Verdadera Función de GPIO 36 en el Silicio (ESP32-P4):**
   - Según la tabla de pines oficial de Espressif (Apéndice A, pág. 82, pin 68):
     - Pertenece al dominio de alimentación **`VDD_IO_4`** (recibe energía, no la entrega).
     - Tras el encendido se inicializa por hardware en **`IE, WPU`** (Input Enable, Weak Pull-Up).
     - Integra el grupo oficial de **Pines de Strapping y Consola ROM de Arranque** (`GPIO 34..38`).
   - En el esquemático de Guition ([`3_ESP32-P4.png`](../hardware/schematics/3_ESP32-P4.png)), el pin 55 del módulo (`GPIO36`) está conectado únicamente a una resistencia SMD **`R44` (10 kΩ) hacia `ESP_3V3`**.
   - Dicha resistencia cumple exactamente la recomendación de diseño de Espressif para fijar el nivel lógico de arranque seguro de la ROM del SoC. No está conectado a ningún MOSFET, no sale a la cabecera JP1, ni a ningún conector externo (es inaccesible físicamente).

---

## 2. Los 4 Puntos de Acción de la Migración

Para alinear el software con la realidad física y eliminar código muerto y definiciones espurias:

### Punto 1: Saneamiento de `hal_flasher_p4.cpp`
* **Acción:** Eliminar por completo el bloque huérfano de líneas 271 a 280:
  ```cpp
  // Asegurar que el carril ESP_3V3 (GPIO 36) este energizado
  gpio_config_t pwr_conf = {};
  pwr_conf.intr_type = GPIO_INTR_DISABLE;
  pwr_conf.mode = GPIO_MODE_OUTPUT;
  pwr_conf.pin_bit_mask = (1ULL << GPIO_NUM_36);
  pwr_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  pwr_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&pwr_conf);
  gpio_set_level(GPIO_NUM_36, 1);
  ```
* **Justificación:** Poner en `1` un pin que ya es entrada con pull-up pasivo de 10 kΩ soldado a 3.3V no tiene ningún efecto y confunde la lógica del flasher.

### Punto 2: Depuración de `boards/jc4880p443.json`
* **Acción:** Retirar el bloque:
  ```json
  "power_control": {
    "pins": {
      "en": 36
    }
  },
  ```
* **Clasificación de GPIO 36:** Mantener `GPIO 36` fuera de la lista de expansión `expansion.jp1_allowed` y asignarlo como pin reservado del sistema / strapping de arranque.

### Punto 3: Actualización de `tools/cdtc.py` y `cbdos_device_tree.h`
* **Acción en compilador:** Eliminar la extracción y generación de código del namespace:
  ```python
  # Retirar de tools/cdtc.py:
  power = devices.get("power_control", {}).get("pins", {})
  ...
  namespace power {{
      constexpr int PIN_EN = ...;
  }}
  ```
* **Acción en header:** Regenerar [`bsp/esp32_p4_jc4880/hal/cbdos_device_tree.h`](../../bsp/esp32_p4_jc4880/hal/cbdos_device_tree.h) sin el espacio de nombres `power`.

### Punto 4: Actualización del Plan Maestro y Especificaciones
* **Acción en `specs/architecture/PLAN_MIGRACION_CDT_Y_DRIVERS.md`:**
  - Actualizar la tabla de estado global: Paso 6 completado mediante saneamiento y eliminación de ficción de hardware.
  - Actualizar la sección descriptiva del Paso 6 documentando que no se requiere sustitución por macro DT, sino la erradicación del bloque de código no funcional.
* **Consistencia documental:** [`specs/hardware/pinouts_and_ports.md`](../hardware/pinouts_and_ports.md) ya refleja `GPIO 36` como pin de *strapping* de arranque y `GPIO 54` (`C6_CHIP_PU`) como el verdadero control de reset/bajo consumo del coprocesador C6.

---

## 3. Estado de Ejecución

| Tarea | Componente Afectado | Tipo de Operación | Estado |
| :--- | :--- | :--- | :---: |
| Documentación Técnica de Auditoría | `specs/architecture/` | Creación de especificación | ✅ Completado |
| Punto 1: Eliminar código muerto | `hal_flasher_p4.cpp` | Edición de código | ✅ Completado (2026-09-24) |
| Punto 2: Eliminar nodo JSON | `boards/jc4880p443.json` | Edición de configuración | ✅ Completado (2026-09-24) |
| Punto 3: Actualizar cdtc y header | `tools/cdtc.py` & `.h` | Edición de script y generación | ✅ Completado (2026-09-24) |
| Punto 4: Actualizar Plan CDT | `PLAN_MIGRACION_CDT_Y_DRIVERS.md` | Actualización de bitácora | ✅ Completado (2026-09-24) |
