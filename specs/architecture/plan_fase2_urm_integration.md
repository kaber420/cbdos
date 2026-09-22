# Plan de Ejecución Unificado: Reparación Fase 0 + Integración Fase 2 (URM)

Este documento unifica las dos etapas necesarias para asegurar la arquitectura de hardware del OS. Se ejecutará de forma secuencial: primero sanear los cimientos (Fase 0) y luego conectar el URM (Fase 2).

## Parte 1: Reparación de la Fase 0 (Saneamiento de Device Tree)

Debido a que la migración original generó un "split-brain" y omitió pines críticos, restauraremos la "Fuente de la Verdad".

### 1. Actualización del JSON (Fuente de la Verdad)
**Archivo:** `boards/jc4880p443.json`
- Inyectar los 28 pines verificados del sistema que fueron omitidos:
  - **I2C_0 (Touch):** SDA=7, SCL=8, RST=3, INT=4
  - **Audio I2S:** MCLK=13, BCLK=12, WS=10, DOUT=9, DIN=48, PA=11
  - **SDMMC:** D0=39, D1=40, D2=41, D3=42, CLK=43, CMD=44
  - **Console UART0:** TX=38, RX=37
  - **Power Control:** EN=36
  - **Strapping & Control C6:** 54, 14, 15, 16, 17, 18, 19

### 2. Corrección del Generador
**Archivo:** `tools/cdtc.py`
- Actualizar el script para escanear recursivamente todo el bloque `"devices"` del JSON.
- Generar el arreglo consolidado `BOARD_SYSTEM_LOCKED_PINS` dentro de `cbdos_device_tree.h`.

### 3. Limpieza
- Eliminar el archivo huérfano `bsp/esp32_p4_jc4880/hal/board_config_generated.h` para erradicar el conflicto.

---

## Parte 2: Integración de la Fase 2 (URM y Capa de Drivers)

Con el Device Tree unificado, procederemos a asegurar que ningún driver pueda usar pines sin el permiso del Universal Resource Manager (URM).

### 4. Refactorización del URM
**Archivos:** `core/include/cbdos/urm.hpp` y `core/src/system/urm.cpp`
- Validar `ResourceClaimMode::SystemLocked` exclusivamente contra `BOARD_SYSTEM_LOCKED_PINS` exigiendo que el `owner` comience con `"system:"`.
- Validar `ResourceClaimMode::Exclusive/Shared` contra `BOARD_ALLOWED_PINS`.

### 5. Integración en los Wrappers HAL
Para mantener la compatibilidad con drivers nativos de ESP-IDF u open source descargados de internet, los wrappers de CBDos actuarán como un guardia de seguridad antes de inicializar el driver real:

**A. UART y Serial (`hal_uart_p4.cpp`)**
- `P4SerialPort::open`: Llamar a `claimGpio()` ANTES de `uart_driver_install`. Si el URM deniega el permiso, la inicialización aborta (evitando configurar hardware en conflicto).
- El preset de **UART0 (38,37)** se reclamará como `SystemLocked`. El **JP1 (32,28)** como expansión.

**B. Pantalla (`hal_display_p4.cpp` / `DisplayHAL.cpp`)**
- El wrapper reclamará el pin RST y BL bajo `owner="system:display"` (`SystemLocked`) antes de delegar la configuración a la librería ST7701 original.

### 6. Bindings Seguros para Lua
**Archivo:** `core/src/lua/lua_gpio.cpp` (o integrado en `LuaBridge`)
- Las aplicaciones Lua jamás pasarán el parámetro `owner`. El sistema inyectará `owner="app:<nombre>"`.
- Solo podrán usar el modo `Exclusive/Shared`.

---

## Plan de Verificación (DoD)
1. **Inspección de Generados:** `cbdos_device_tree.h` contendrá ambos arreglos completos.
2. **Compilación Limpia:** Ejecutar `idf.py build` sin errores.
3. **Pruebas de Cortafuegos:**
   - Una aplicación Lua intenta pedir el pin de pantalla (5). **Resultado esperado: Denegado por el URM.**
   - El sistema arranca y el wrapper de pantalla pide el pin 5. **Resultado esperado: Aprobado e inicializado por el driver original de internet.**
