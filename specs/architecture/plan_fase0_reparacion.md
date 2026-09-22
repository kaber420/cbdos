# Plan de Reparación de la Fase 0 (Saneamiento de Device Tree)

Debido a que la migración original de la Fase 0 se hizo de manera incompleta y generó archivos huérfanos y falta de información, este plan detalla paso a paso cómo restauraremos la "Fuente de la Verdad" antes de tocar ningún archivo de la arquitectura principal o intentar integrar la Fase 2.

## Objetivo
Unificar toda la configuración de hardware real (verificada contra el código fuente de los drivers en C++) dentro de un único archivo JSON, corregir el generador, y limpiar la basura autogenerada que estaba causando conflictos.

---

## Cambios Propuestos

### 1. Actualización de la Fuente de la Verdad (JSON)
**Archivo a modificar:** `boards/jc4880p443.json`
- Se extenderá el esquema actual para incluir todos los dispositivos que utilizan pines estáticos del sistema.
- Se añadirán (con base en la información verificada de los drivers):
  - **I2C_0 (Touch y compartidos):** SDA=7, SCL=8, RST=3, INT=4
  - **Audio I2S:** MCLK=13, BCLK=12, WS=10, DOUT=9, DIN=48, PA=11
  - **SDMMC:** D0=39, D1=40, D2=41, D3=42, CLK=43, CMD=44
  - **Console UART0:** TX=38, RX=37
  - **Power Control:** EN=36
  - **Strapping & Control C6:** 54, 14, 15, 16, 17, 18, 19

### 2. Corrección del Generador de Código
**Archivo a modificar:** `tools/cdtc.py`
- El script de Python será actualizado para escanear recursivamente todo el bloque `"devices"` del JSON.
- Recolectará todos los números de pin (ignorando valores inválidos o nulos) y generará un array consolidado sin duplicados: `constexpr std::array<int, NUM_SYSTEM_LOCKED_PINS> BOARD_SYSTEM_LOCKED_PINS`.
- Todo se imprimirá en el único archivo oficial: `cbdos_device_tree.h`.

### 3. Limpieza de Archivos Basura
- **Acción:** Se eliminará definitivamente el archivo obsoleto `bsp/esp32_p4_jc4880/hal/board_config_generated.h` para erradicar el problema de "split-brain" detectado por los evaluadores de código.

---

## Plan de Verificación

1. **Generación Manual:** Se ejecutará `python tools/cdtc.py boards/jc4880p443.json bsp/esp32_p4_jc4880/hal/cbdos_device_tree.h` de forma manual.
2. **Inspección Visual:** Se verificará que el nuevo `cbdos_device_tree.h` contenga exactamente 28 pines en `BOARD_SYSTEM_LOCKED_PINS` y los 12 pines de expansión en `BOARD_ALLOWED_PINS`.
3. **Prueba de Compilación (ESP-IDF):** Se ejecutará `. ~/esp/esp-idf/export.sh && idf.py build` para garantizar que la eliminación del archivo huérfano y la actualización del header no rompan nada en el código C++ actual.
