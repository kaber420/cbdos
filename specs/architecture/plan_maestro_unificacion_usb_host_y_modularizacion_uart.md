# Plan Maestro: Unificación de USB Host y Modularización del Subsistema UART/Serial en ESP32-P4

**Fecha:** 25 de Septiembre, 2026  
**Documento:** `specs/architecture/plan_maestro_unificacion_usb_host_y_modularizacion_uart.md`  
**Estado:** Aprobado y Enriquecido tras Evaluación con Muse Spark 1.3  
**Módulos Afectados:** `bsp/esp32_p4_jc4880/hal/`, `bsp/esp32_p4_jc4880/main/CMakeLists.txt`, `core/include/cbdos/`  

---

## 🔍 1. Diagnóstico Forense y Estado Actual

Tras la auditoría del código fuente activo en el BSP del ESP32-P4 (`bsp/esp32_p4_jc4880/hal/`), se ha detectado una severa fragmentación arquitectónica y acoplamiento monolítico:

### 1.1. Fragmentación en 3 Silos Independientes de USB Host / CDC
Actualmente, tres componentes diferentes compiten o manipulan directamente el stack USB Host de ESP-IDF:

1. **`usb_device_manager.cpp` (Gestor Central del Sistema):**
   * Inicializa la librería `usb_host_install()` y el driver `cdc_acm_host_install()`.
   * Expone la interfaz de plugins [`IUsbDriver`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/usb_device_manager.hpp#L40) y una cola de eventos de conexión/desconexión (`s_usb_queue`).
   * **Problema:** Ningún subsistema activo se registra formalmente como un `IUsbDriver`. Su diseño quedó como cascarón sin consumir.

2. **`hal_uart_p4.cpp` (Implementación `P4UsbOtgPort`):**
   * Contiene una clase privada [`P4UsbOtgPort`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp#L116) que implementa `ISerialPort`.
   * En lugar de recibir un handle del gestor, llama **directamente** a `cdc_acm_host_open()` y `cdc_acm_host_open_vendor_specific()` ([líneas 161-163](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp#L161-L163)).
   * Solo consulta a `UsbDeviceManager` de manera pasiva para leer el VID/PID detectado. Maneja por su cuenta los callbacks nativos de datos (`cdcRxCb`) y eventos (`cdcEventCb`).

3. **`usb_cdc_loader_port.cpp` (Capa de Transporte para el Flasher de ESP-IDF):**
   * Es completamente ajeno al sistema operativo.
   * Contiene sus propias variables estáticas `s_usb_host_installed` y `s_cdc_driver_installed`, levanta su propia tarea privada `usb_host_lib_task` ([línea 23](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/usb_cdc_loader_port.cpp#L23)) e intenta volver a instalar el driver CDC.
   * Llama directamente a `cdc_acm_host_open(0x303A, 0x1001, ...)` ([línea 116](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/usb_cdc_loader_port.cpp#L116)) con VID/PID fijos para el bootloader ROM.

### 1.2. El Monolito de `hal_uart_p4.cpp` (816 líneas)
El archivo [`hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp) agrupa indebidamente 6 responsabilidades dispares:
1. `P4UsbNativePort`: Puerto USB-Serial-JTAG (consola PC nativa).
2. `P4UsbOtgPort`: Puerto USB Host CDC-ACM (periféricos y target externos).
3. `P4SerialPort`: Hardware UART físico (JP1, UART0, configuración manual de pines).
4. `P4SerialBackend`: Gestor de puertos serie del sistema (`ISerialBackend`).
5. `P4UartBackend`: Capa legacy de UART simple (`IUartBackend`).
6. **`P4GpioBackend`:** ¡Toda la implementación del HAL de GPIO de CBDos ([líneas 737-797](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp#L737-L797)) está incrustada dentro de este archivo UART! (Y se confirmó que en ESP32-S3 se cometió el mismo error arquitectónico en [`hal_uart_s3.cpp:352`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_uart_s3.cpp#L352)).

---

## ⚠️ 2. Riesgos Críticos Identificados por Muse Spark 1.3

1. **Doble Tarea Bombeando Eventos USB (`usb_host_lib_task`):**
   Tener dos tareas concurrentes llamando a `usb_host_lib_handle_events()` (`usb_device_manager.cpp` y `usb_cdc_loader_port.cpp`) es una **condición de carrera severa** prohibida por ESP-IDF v5.x. Debe existir una sola tarea de bombeo en todo el sistema.
2. **Imposibilidad de "Pausar" Callbacks en ESP-IDF CDC-ACM:**
   Los callbacks `data_cb` y `event_cb` se fijan en `cdc_acm_host_open()` y no pueden cambiarse en tiempo de ejecución. No es posible "pausar el buffer" de la Terminal para prestarle el handle al Flasher. La solución real es un **callback único demultiplexor** según el dueño activo (`CdcOwner`).
3. **Re-enumeración y Falso Hot-Plug durante el Reset del Target:**
   Al enviar el pulso DTR/RTS a un chip con USB-Serial-JTAG nativo (`0x303A:0x1001`), el dispositivo se resetea y desconecta por hardware. Si `UsbDeviceManager` procesa la desconexión de inmediato, cerrará el handle CDC (`cdc_acm_host_close`) en mitad del flasheo. Se requiere una **ventana de cuarentena/debounce de desconexión** mientras el rol sea `Flashing`.
4. **Conflicto de Secuencias de Bootloader DTR/RTS:**
   * Bridges USB-UART externos (CP210x, CH340, FTDI): Requieren la secuencia clásica de 2 pasos controlando transistores hacia EN e IO0.
   * Chips con JTAG nativo (`0x303A:0x1001` - C3/S3/C6): No tienen pines físicos; el firmware ROM emulado requiere la secuencia de 4 pasos con retardos de 100 ms estilo `esptool.py`.
5. **Colisión de Símbolos Globales de `esp-serial-flasher`:**
   `usb_cdc_loader_port.cpp` define `loader_port_write` y `loader_port_read`. Si el flasher de UART (JP1) también los define, con `WHOLE_ARCHIVE` en CMake habrá conflicto de enlace. Se requiere un despacho unificado.
6. **Ineficiencia en Buffer de Flasheo:**
   El buffer actual de 1 KB con `memmove` en `usb_cdc_loader_port.cpp` es O(n²) e insuficiente para flashear a 921600 bps. Se requiere un buffer de al menos 4 KB dedicado para el flasheo.

---

## 🏛️ 3. Arquitectura Objetivo Refinada

```
                                  ┌─────────────────────────────┐
                                  │      UsbDeviceManager       │
                                  │  - Único usb_host_install   │
                                  │  - Única usb_host_lib_task  │
                                  │  - Cuarentena de Hot-Plug   │
                                  └──────────────┬──────────────┘
                                                 │
                                                 ▼
                                  ┌─────────────────────────────┐
                                  │     UsbCdcHostDriver        │
                                  │   (Implementa IUsbDriver)   │
                                  │  - Único dueño de m_cdcDev  │
                                  │  - Demux atómico de rxCb    │
                                  └──────┬───────────────┬──────┘
                                         │               │
                            (CdcOwner::Terminal)   (CdcOwner::Flasher)
                                         │               │
                                         ▼               ▼
                                 [RingBuf 2KB]    [StreamBuf 4KB]
                                         │               │
                                         ▼               ▼
                                   P4UsbOtgPort    UsbCdcLoaderPort
                                (Terminal Serial) (Flasher Target)
```

### 3.1. Máquina de Estados del Driver CDC (`CdcOwner`)
```cpp
enum class CdcOwner {
    Idle,       // Sin consumidor activo
    Terminal,   // Consumo asíncrono para Terminal Serial / Shell / Lua
    Flasher     // Consumo exclusivo síncrono para esp-serial-flasher
};
```

* **Demux en el callback de recepción (Cero Bloqueos):**
```cpp
static bool cdcRxDemuxCallback(const uint8_t *data, size_t len, void *arg) {
    auto *driver = static_cast<UsbCdcHostDriver*>(arg);
    if (driver->getOwner() == CdcOwner::Flasher) {
        driver->pushFlasherData(data, len); // Hacia StreamBuffer de 4KB
    } else {
        driver->pushTerminalData(data, len); // Hacia RingBuffer de 2KB
    }
    return true;
}
```

* **Gestión de Reset / Bootloader Adaptativa por VID:**
```cpp
bool setTargetBootMode(bool enterBootloader) {
    if (m_activeVid == 0x303A) {
        // Secuencia estricta de 4 pasos para USB-Serial-JTAG nativo
        return sendNativeJtagBootSequence(enterBootloader);
    } else {
        // Secuencia estándar para CP210x, CH340, FTDI
        return sendStandardBridgeBootSequence(enterBootloader);
    }
}
```

---

## 📂 4. Plan de Modularización de Archivos

| Archivo | Responsabilidad | Tamaño Estimado |
|---|---|---|
| [`hal_gpio_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_gpio_p4.cpp) | Implementación aislada de `IGpioBackend` (`P4GpioBackend`) y función `initGpioBackendP4()`. | ~100 líneas |
| [`hal_serial_native_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_serial_native_p4.cpp) | Implementación de `ISerialPort` para USB-Serial-JTAG (Consola PC nativa) con cálculo real en `available()`. | ~85 líneas |
| [`hal_uart_hw_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_hw_p4.cpp) | Driver de UART físico (JP1, UART0, manual) con control de reset C6/S3 en GPIO 54/34. | ~210 líneas |
| [`usb_cdc_driver_p4.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/usb_cdc_driver_p4.hpp) / `.cpp` | Controlador que implementa `IUsbDriver`, gobierna `m_cdcDev`, demultiplexa RX por `CdcOwner` y maneja cambio de baudrate real. | ~240 líneas |
| [`hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp) | Orquestador de backends del sistema: `P4SerialBackend` (`ISerialBackend`) y adaptador de compatibilidad legacy `IUartBackend`. | ~140 líneas |
| [`usb_cdc_loader_port.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/usb_cdc_loader_port.cpp) | Capa limpia que conecta `esp-serial-flasher` con `UsbCdcHostDriver::acquire(CdcOwner::Flasher)`, eliminando tareas y dobles inits. | ~100 líneas |

---

## 🛠️ 5. Plan de Ejecución por Fases (Fix-Forward)

### Fase 1: Extracción Segura del HAL de GPIO
1. Crear [`hal_gpio_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_gpio_p4.cpp) trasladando la clase `P4GpioBackend` y `initGpioBackendP4()`.
2. Registrar el nuevo archivo en `bsp/esp32_p4_jc4880/main/CMakeLists.txt`.
3. Validar compilación limpia de ESP32-P4 (`idf.py build`).

### Fase 2: Extracción de UART Físico y Consola Nativa JTAG
1. Crear [`hal_serial_native_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_serial_native_p4.cpp) con `P4UsbNativePort` (corrigiendo `available()` real).
2. Crear [`hal_uart_hw_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_hw_p4.cpp) con `P4SerialPort` (hardware UART).
3. Mantener `hal_uart_p4.cpp` limpio únicamente con el registro de backends.
4. Validar compilación dual (P4 y S3).

### Fase 2.5: Implementación de `UsbCdcHostDriver` y Migración de Terminal
1. Crear `usb_cdc_driver_p4.hpp` / `.cpp` con soporte para `IUsbDriver`, mutex de arbitraje y demux RX.
2. Migrar `P4UsbOtgPort` para consumir este driver.
3. Probar en caliente la Terminal Serial por USB OTG (T4 y T5) sin tocar aún el código del Flasher.

### Fase 3: Integración del Flasher USB y Cuarentena de Desconexión
1. Conectar `usb_cdc_loader_port.cpp` a `UsbCdcHostDriver::acquire(CdcOwner::Flasher)`.
2. Añadir en `UsbDeviceManager` una ventana de cuarentena de desconexión (1000 ms) durante el flasheo de targets `0x303A:0x1001`.
3. Eliminar la tarea duplicada `usb_host_lib_task` y los dobles `usb_host_install` de `usb_cdc_loader_port.cpp`.
4. Implementar cambio de baudrate real (`cdc_acm_host_line_coding_set`) en `loader_port_change_transmission_rate()`.
5. Ejecutar la batería completa de pruebas T1 a T8.

---

## 🧪 6. Matriz de Pruebas de Campo Obligatorias (Test Matrix)

| # | Prueba | Target / Procedimiento | Criterio de Éxito |
|---|---|---|---|
| **T1** | **Consola Nativa PC** | Conectar P4 por cable USB-C nativo a PC (`idf.py monitor`). | Logs de arranque limpios a 115200 bps, REPL responde sin busy-loops. |
| **T2** | **Terminal Serial UART** | Conectar módulo a JP1 (Pines 32/28/34/54). Abrir Serial Terminal. | Comunicación fluida bidireccional. |
| **T3** | **Flasheo por UART (JP1)** | Flashear firmware `.bin` hacia ESP32-C6 / S3 conectado a JP1. | Secuencia Reset/Boot en GPIO 54/34 entra en bootloader; 100% flasheado. |
| **T4** | **Hot-Plug USB OTG** | Enchufar target USB al puerto OTG del P4. | `UsbDeviceManager` detecta VID/PID por interrupción sin polling. |
| **T5** | **Terminal Serial USB CDC** | Abrir sesión serie en `usb_otg` desde la UI a 115200 bps. | Envío y recepción de datos sin corrupción. |
| **T6a** | **Flasheo USB Target Nativo** | Conectar ESP32-C3/S3/C6 nativo (`0x303A:0x1001`) por USB. Flashear `.bin`. | Secuencia de 4 pasos entra en bootloader ROM; no hay disconnect erróneo; 100% flasheado. |
| **T6b** | **Flasheo USB Bridge Externo** | Conectar placa con CP210x o CH340 por USB. Flashear `.bin`. | Salto a alta velocidad (921600 bps) exitoso; 100% flasheado. |
| **T7** | **Compilación Dual Limpia** | Compilar P4 (ESP-IDF) y S3 (PlatformIO). | 0 errores de enlazado (`WHOLE_ARCHIVE` limpio) en ambas arquitecturas. |
| **T8** | **Stress Hot-Plug & Concurrencia** | Tener Terminal Serial abierta en USB OTG, iniciar Flasher, desconectar cable a la mitad y reconectar. | El SO no hace panic ni cuelga el bus USB; el driver se recupera limpiamente. |
