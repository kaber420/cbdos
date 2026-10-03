# 🔌 Plan de Arquitectura: Soporte Completo de USB Host y Conmutación Dinámica en ESP32-S3 (JC3248W535)

**Fecha:** Octubre 2026  
**Documento:** `specs/architecture/plan_soporte_usb_host_esp32_s3.md`  
**Estado:** ✅ COMPLETADO y Validado con Compilación Dual Limpia (ESP32-S3 y ESP32-P4)  
**Autor:** Equipo de Arquitectura CBDos  
**Fecha de Cierre:** Octubre 2026  
- `bsp/esp32_s3_jc3248/platformio.ini`  
- `bsp/esp32_s3_jc3248/src/main.cpp`  
- `bsp/esp32_s3_jc3248/hal/hal_usb_s3.hpp` & `hal_usb_s3.cpp`  
- `bsp/esp32_s3_jc3248/hal/hal_usb_cdc_s3.hpp` & `hal_usb_cdc_s3.cpp`  
- `core/src/ui/views/ConfigView.cpp` (verificación de soporte en UI)  

---

## 🏛️ 1. Justificación y Objetivos

Actualmente, el ESP32-P4 cuenta con un subsistema completo de USB Host (`hal_usb_host_p4.cpp` y `hal_usb_cdc_p4.cpp`), mientras que en el ESP32-S3 (`JC3248W535`) el contrato [`IUsbHostBackend`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/usb_host.hpp) fue dejado como un stub vacío con `supportsHost() = false`.

El silicio del ESP32-S3 posee un controlador USB OTG 1.1 Full-Speed (12 Mbps) perfectamente capaz de actuar como **Host USB** mediante el stack nativo de ESP-IDF (`usb_host` y `cdc_acm_host`).

### Objetivos:
1. **Erradicar el Stub Vacío en S3:** Reemplazar el dummy de [`hal_usb_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp) por un backend funcional que inicialice `usb_host_install` y `cdc_acm_host_install`.
2. **Conmutación Determinista de Modos:** Integrar el arranque de S3 con [`UsbManager`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/usb_manager.hpp):
   - **Modo `Hid`:** Inicializa TinyUSB (emulación de teclado, ratón, BadUSB y FIDO2 Poseidon hacia la PC).
   - **Modo `Host`:** Inicializa el stack USB Host para conectar teclados, dongles o flashear otros microcontroladores.
3. **Canal CDC Unificado (`IUsbCdcChannel`):** Proveer al S3 el canal serial USB necesario para la app `Terminal` y la app `Flasher`.
4. **Cero Regresiones Multi-Target:** Mantener la compilación 100% limpia tanto en ESP-IDF (P4) como en PlatformIO (S3).

---

## ⚡ 2. Matriz de Hardware y Topología USB en ESP32-S3

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│                      TOPOLOGÍA USB DEL ESP32-S3 (JC3248W535)                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│ • 1 Único Controlador Físico USB OTG FS (Pines GPIO 19: D-, GPIO 20: D+)        │
│ • Exclusividad de Stack: TinyUSB (Device) y USB Host NO pueden coexistir a la vez│
│ • Conmutación: Seleccionada por NVS (`usb_mode`) + Fast Reboot (~300 ms)         │
└──────────────────────────────────────────────────────────────────────────────────┘
                                      │
                 ┌────────────────────┴────────────────────┐
                 ▼                                         ▼
        [ Modo Device / HID ]                     [ Modo USB Host ]
   • TinyUSB Activo                          • ESP-IDF USB Host Library Activo
   • BadUSB / DuckyScript                    • CDC-ACM Host Driver Activo
   • FIDO2 Poseidon                          • IUsbCdcChannel Activo
   • Conexión a PC                           • Conexión a Dongles / Microcontroladores
                                             • Requiere VBUS externo (cable Y / Hub)
```

> [!NOTE]
> **Alimentación Eléctrica en la Placa JC3248W535:**
> El conector USB-C de la placa JC3248W535 es un puerto receptor (Sink). Al operar como USB Host, los periféricos externos que no tengan batería propia deben recibir 5V mediante un adaptador OTG en "Y" con inyección de alimentación o un Hub USB autoalimentado.

---

## 🧠 3. Arquitectura del Ciclo de Vida y Conmutación

```
                          ┌──────────────────────────┐
                          │   Inicio de setup() S3   │
                          └─────────────┬────────────┘
                                        │
                                        ▼
                          ┌──────────────────────────┐
                          │ UsbManager::init() (NVS) │
                          └─────────────┬────────────┘
                                        │
                    ┌───────────────────┴───────────────────┐
                    ▼                                       ▼
       [ getBootMode() == Host ]               [ getBootMode() == Hid ]
                    │                                       │
                    ▼                                       ▼
       ┌────────────────────────┐              ┌────────────────────────┐
       │ Iniciar Backend Host   │              │ Iniciar TinyUSB        │
       │ - usb_host_install()   │              │ - initHidDriverS3()    │
       │ - cdc_acm_host_install │              │ - supportsHost()=false │
       │ - supportsHost()=true  │              │                        │
       └────────────────────────┘              └────────────────────────┘
```

---

## 📁 4. Plan de Archivos a Modificar y Crear

| Archivo | Acción | Descripción |
|---|---|---|
| [`bsp/esp32_s3_jc3248/platformio.ini`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/platformio.ini) | **MODIFICAR** | Cambiar `-DARDUINO_USB_CDC_ON_BOOT=0` para permitir que el código de CBDos controle el PHY USB condicionalmente en `setup()`. |
| [`bsp/esp32_s3_jc3248/src/main.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/src/main.cpp) | **MODIFICAR** | Añadir `UsbManager::getInstance().init()` al arranque y bifurcar la inicialización según `getBootMode()`. |
| [`bsp/esp32_s3_jc3248/hal/hal_usb_s3.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_s3.hpp) | **CREAR** | Declaración de `S3UsbHostBackend` con colas FreeRTOS, tareas gestoras y soporte de hot-plug. |
| [`bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp) | **REESCRIBIR** | Implementación completa de `S3UsbHostBackend` conectada al stack ESP-IDF. |
| [`bsp/esp32_s3_jc3248/hal/hal_usb_cdc_s3.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_cdc_s3.hpp) | **CREAR** | Declaración del canal `S3UsbCdcChannel` adaptado para S3. |
| [`bsp/esp32_s3_jc3248/hal/hal_usb_cdc_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_cdc_s3.cpp) | **CREAR** | Driver CDC ACM Host para S3 con `StreamBuffer` de 16 KB, preemption (Terminal vs Flasher) y secuencias de bootloader DTR/RTS. |

---

## 🛠️ 5. Fases de Ejecución Fix-Forward

### Fase 0: Respaldo y Comprobación de Línea Base
1. Preservar copia histórica del stub actual en `specs/history/hal_usb_s3_stub.cpp`.
2. Verificar compilación limpia actual en ambas plataformas.

### Fase 1: Control del PHY USB en S3 (`platformio.ini` y `main.cpp`)
1. En `platformio.ini`, deshabilitar la captura incondicional de Arduino: `-DARDUINO_USB_CDC_ON_BOOT=0`.
2. En `main.cpp`, enlazar `cbdos::usb::UsbManager::getInstance().init()`.
3. Bifurcar arranque:
   - Si `Host`: Inicializar `initUsbHostBackendS3()`.
   - Si `Hid`: Inicializar `initHidDriverS3()`.

### Fase 2: Implementación de `S3UsbHostBackend`
1. Crear `hal_usb_s3.hpp` y reescribir `hal_usb_s3.cpp`:
   - `usb_host_install` con configuración adecuada para el PHY interno del S3 (`skip_phy_setup = false`).
   - Tarea de eventos de FreeRTOS `s3_usb_host_lib_task` con `usb_host_lib_handle_events`.
   - Tarea de gestión de eventos `s3_usb_mgr_task`.
   - Detección de dispositivos (Hot-Plug reactivo) y notificación a callbacks.
   - Ventana de cuarentena durante reseteos de microcontroladores.

### Fase 3: Implementación de `S3UsbCdcChannel`
1. Crear `hal_usb_cdc_s3.hpp` y `hal_usb_cdc_s3.cpp`:
   - Driver CDC-ACM Host con `cdc_acm_host_install`.
   - Gestión de apertura / cierre de endpoints CDC.
   - Buffer circular / StreamBuffer de 16 KB para recepción de alta velocidad sin pérdida de datos.
   - Arbitraje con desalojo (Preemption): Terminal cede el canal a Flasher de forma ordenada.
   - Generación de secuencias de reset DTR/RTS (4 pasos para JTAG nativo ESP32, 2 pasos para CP210x/CH340).

### Fase 4: Validación y Compilación Cruzada
1. Compilar ESP32-S3 (`pio run -d bsp/esp32_s3_jc3248`).
2. Compilar ESP32-P4 (`. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`) para garantizar 0 regresiones.
3. Verificar conmutación de modos en la UI de Ajustes (`ConfigView`).
