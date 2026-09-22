# Plan de Implementación: Fase 0 para ESP32-P4 (Extracción de Pines y Codegen)

Este plan detalla los pasos de ejecución técnica para implementar la **Fase 0** de la arquitectura en la placa `ESP32-P4 (JC4880P443C)`. 

El objetivo es extraer las listas de pines hardcodeados actualmente en `hal_uart_p4.cpp` y convertirlos en un archivo declarativo (YAML) que genere código C++ de forma automática durante la compilación.

## Propuesta de Integración con CMake

Para que este proceso sea transparente, se añadirá una regla personalizada (Custom Command) en el `CMakeLists.txt` del proyecto o del BSP. Esto hará que cada vez que se ejecute `idf.py build`, el sistema llame a `python tools/cdtc.py boards/jc4880p443.yaml` automáticamente antes de compilar el código C++.

---

## 1. Definición del Hardware (YAML)

### Archivo: `boards/jc4880p443.yaml`
Se creará este archivo con la extracción de los pines encontrados en el código actual (basado en `hal_uart_p4.cpp:785`):

```yaml
board: "JC4880P443C"
version: "1.3"

resources:
  system_locked:
    display_and_touch: [5, 23, 3, 4] # RST, BL PWM, Touch RST, Touch INT
    i2c_bus_0: [7, 8] # Compartido entre Touch, Códec de Audio y cabecera JP1
    audio_i2s: [13, 12, 10, 9, 48, 11]
    sdmmc: [39, 40, 41, 42, 43, 44]
    sdio_and_strapping: [18, 19, 14, 15, 16, 17, 54]
    console_uart: [37, 38] # UART0 nativo de debug
    power_control: [36] # GPIO 36 controla la alimentación ESP_3V3 del C6
  
  expansion:
    jp1_allowed: [28, 29, 30, 31, 32, 33, 34, 35, 49, 50, 51, 52]
```

---

## 2. Script de Generación (Device Tree Compiler)

### Archivo: `tools/cdtc.py`
Un script en Python puro (idealmente sin dependencias complejas más allá de `pyyaml`, o usando JSON puro para evitar instalar paquetes extra si se prefiere) que lee el archivo YAML y genera código C++:
- Generará un archivo en el directorio de compilación: `build/generated/board_config_generated.h` (o dentro de `bsp/esp32_p4_jc4880/include/`).
- Contendrá un arreglo consolidado `constexpr int BOARD_SYSTEM_LOCKED_PINS[] = {...}` que fusiona todas las listas de `system_locked`.
- Adicionalmente, será capaz de exportar la misma estructura como un archivo `.cdt` en formato **Flat Binary (C-Struct crudo)**, permitiendo su futura carga directa en la memoria flash (memory mapping) sin necesidad de librerías de parsing como JSON o MsgPack.

---

## 3. Limpieza del Código BSP

### Archivo a modificar: `bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`
Se modificará la función `isPinAvailable(int pin)` para eliminar los números mágicos y depender del archivo generado:

```cpp
#include "board_config_generated.h"
// ...
bool isPinAvailable(int pin) const override {
    if (pin < 0 || pin > 54) return false;
    
    // Verificar si está en la lista de pines bloqueados del sistema
    for (int locked_pin : BOARD_SYSTEM_LOCKED_PINS) {
        if (pin == locked_pin) return false;
    }
    return true;
}
```

### Archivo a modificar: `bsp/esp32_p4_jc4880/CMakeLists.txt`
Se agregará un `add_custom_command` que invoque a `python ../../tools/cdtc.py` y genere el header antes de que `hal_uart_p4.cpp` sea compilado.

---

## Plan de Verificación

1. Ejecutar `. ~/esp/esp-idf/export.sh` y luego `idf.py build` en la carpeta `bsp/esp32_p4_jc4880`.
2. Observar en los logs de CMake que el script de Python se ejecuta correctamente sin errores.
3. Verificar que la compilación es exitosa.
4. (Opcional) Flashear y validar que el arranque de CBDos sigue siendo normal y funcional, garantizando que no hemos roto nada en el proceso de limpieza de código.
