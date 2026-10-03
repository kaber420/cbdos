# 🔍 Auditoría Técnica Forense y Dictamen Definitivo: Estado Real del Subsistema USB (HID Device, USB Host y Periféricos Físicos) en CBDos

**Fecha:** Octubre 2026  
**Documento:** `specs/architecture/auditoria_tecnica_y_plan_definitivo_usb_hid.md`  
**Estado:** 📋 Especificación Técnica Definitiva y Auditoría de Código  
**Autor:** Desarrollador Senior de Sistemas Embebidos / Equipo de Arquitectura CBDos  
**Módulos Auditados:**  
- `core/src/ui/views/HidView.cpp` (Aplicación "HID Control" en pantalla)  
- `bsp/esp32_s3_jc3248/hal/hal_hid_s3.cpp` (Driver HID Device S3)  
- `bsp/esp32_p4_jc4880/hal/hal_hid_p4.cpp` (Driver HID Device P4)  
- `bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp` & `hal_usb_cdc_s3.cpp` (Backend Host S3)  
- `bsp/esp32_p4_jc4880/hal/hal_usb_host_p4.cpp` & `hal_usb_cdc_p4.cpp` (Backend Host P4)  

---

## 🏛️ 1. Resumen Ejecutivo y Matriz de la Verdad

Tras realizar una auditoría forense línea por línea sobre el código fuente activo de ambos objetivos (ESP32-P4 y ESP32-S3), se documenta fehacientemente el **estado real** de cada función USB para erradicar cualquier suposición o falsa expectativa.

### 📊 Matriz de Estado Real por Funcionalidad

| Funcionalidad USB | Target ESP32-P4 | Target ESP32-S3 | Diagnóstico de Arquitectura |
|---|---|---|---|
| **Poder usar la pantalla como Teclado USB para PC** | ✅ REAL y Funcional | ❌ **FALSO / STUB** | En P4 envía scancodes por TinyUSB (`tud_hid_report`). En S3 las funciones `sendReport()` eran stubs vacíos con `(void)keycodes;`. |
| **Poder usar la pantalla como Touchpad USB para PC** | ✅ REAL y Funcional | ❌ **FALSO / STUB** | En P4 envía reportes de mouse (`tud_hid_report`). En S3 las funciones `sendMouseReport()` eran stubs vacíos con `(void)x;`. |
| **App "HID Control" (`HidView.cpp`)** | ⚠️ Funcional con Bug | ⚠️ **Inoperativa / Con Bug** | En `padEventCb` (línea 431), tocar la pantalla fuerza `cbdos::hid::enable()` incondicionalmente, ignorando el switch en OFF y disparando toasts de error. |
| **Arranque por defecto del sistema** | `UsbMode::Hid` | `UsbMode::Hid` | Por defecto NVS carga `UsbMode::Hid` (modo Device hacia PC). Si no se cambia en Ajustes, el modo Host no inicia al bootear. |
| **USB Host CDC-ACM (Módems / Flasher / Terminal)** | ✅ REAL y Funcional | ✅ REAL y Funcional | Ambos targets compilan el stack `usb_host_install` y `cdc_acm_host_install` para comunicarse con otros microcontroladores. |
| **Conectar Teclado / Mouse USB Físico al puerto** | ❌ **INEXISTENTE** | ❌ **INEXISTENTE** | **Ningún target** tiene un driver `UsbHidHostDriver` (`usb_host_hid`). Si se conecta un teclado o mouse físico al puerto USB-C, la clase `0x03` es ignorada y no llega a LVGL. |

---

## 🔬 2. Auditoría Detallada del Código Fuente

### 2.1. El Caso del ESP32-S3: Por qué la aplicación "HID Control" no enviaba nada a la PC

En [`bsp/esp32_s3_jc3248/hal/hal_hid_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_hid_s3.cpp#L142-L152), la clase `Esp32S3HidDriver` fue implementada con métodos stub vacíos:

```cpp
// CÓDIGO INCOMPLETO EN S3 (hal_hid_s3.cpp):
void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) override {
    (void)modifiers;
    (void)keycodes; // <-- STUB DUMMY: No genera reporte USB
}

void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) override {
    (void)buttons;
    (void)x;
    (void)y;
    (void)wheel;    // <-- STUB DUMMY: No mueve el puntero de la PC
}
```

#### Comparativa con el ESP32-P4 (Código Real):
En [`bsp/esp32_p4_jc4880/hal/hal_hid_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_hid_p4.cpp#L125), la clase envía paquetes reales a través del stack TinyUSB Device:

```cpp
// CÓDIGO REAL EN P4 (hal_hid_p4.cpp):
hid_mouse_report_t report;
report.buttons = buttons;
report.x = x;
report.y = y;
report.wheel = wheel;
tud_hid_report(REPORT_ID_MOUSE, &report, sizeof(report)); // <-- Envía reporte HID real a la PC
```

### 2.2. El Bug de Interfaz en `HidView.cpp` (Activación Forzada e Incondicional)

En [`core/src/ui/views/HidView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/HidView.cpp#L424-L432), la función que gestiona los eventos táctiles sobre la zona del touchpad comete una violación de control de flujo:

```cpp
void HidView::padEventCb(lv_event_t* e) {
    ...
    if (code == LV_EVENT_PRESSED) {
        ...
        cbdos::hid::enable(); // <-- BUG CRÍTICO: Fuerza la activación del stack HID
                              // aunque m_enableSwitch esté en OFF o el sistema no esté listo.
    }
```

Esto provoca que, en cuanto el usuario roza la pantalla en la vista del Touchpad, el sistema intente habilitar el driver HID a la fuerza y dispare mensajes emergentes como *"Sin PC: revisa cable USB-OTG"*, arruinando la experiencia de usuario.

---

## 🛠️ 3. Plan de Acción Técnico y Correcciones Definitivas

Para convertir las implementaciones incompletas en un sistema 100% real, funcional y predecible, se establecen las siguientes 3 fases de ingeniería:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PLAN DEFINITIVO DE CORRECCIÓN USB                        │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. CORRECCIÓN DE BUGS EN UI (`HidView.cpp`)                                 │
│    - Eliminar la llamada incondicional `cbdos::hid::enable()` en padEventCb.│
│    - Verificar la propiedad `isEnabled()` antes de procesar eventos.        │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. IMPLEMENTACIÓN REAL DE HID DEVICE EN ESP32-S3 (`hal_hid_s3.cpp`)          │
│    - Sustituir stubs dummies por instanciación de `USBHIDKeyboard` y        │
│      `USBHIDMouse` de TinyUSB.                                              │
│    - Conectar `sendReport()` y `sendMouseReport()` a las llamadas reales.   │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. DRIVER USB HID HOST (TECLADOS Y MOUSE FÍSICOS EXTERNOS)                  │
│    - Crear `S3UsbHidHostDriver` y `P4UsbHidHostDriver` (`usb_host_hid`).     │
│    - Conectar reportes de entrada físicos a `LV_INDEV_TYPE_KEYPAD` y        │
│      `LV_INDEV_TYPE_POINTER` en LVGL 9.5.                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Especificación de la Fase 1: Corrección de UI (`HidView.cpp`)
* Respetar de forma estricta el estado del conmutador `m_enableSwitch`.
* Si el usuario no ha encendido manualmente el conmutador de habilitación, los eventos táctiles de la pantalla no dispararán llamadas a `cbdos::hid::enable()`.

### Especificación de la Fase 2: HID Device Real en ESP32-S3 (`hal_hid_s3.cpp`)
* Integrar los componentes `USBHIDKeyboard` y `USBHIDMouse` de Arduino/TinyUSB.
* En `sendReport(modifiers, keycodes)`: enviar el reporte HID de teclado estándar de 8 bytes hacia el PC.
* En `sendMouseReport(buttons, x, y, wheel)`: enviar el reporte HID de ratón relativo (botones, deltas X/Y y rueda) hacia el PC.

### Especificación de la Fase 3: USB HID Host para Periféricos Físicos
* Incorporar el driver `espressif/usb_host_hid` en el stack USB Host.
* Cuando un teclado físico sea detectado en el puerto USB-C: inyectar las teclas en el grupo por defecto de LVGL 9.5 (`LV_INDEV_TYPE_KEYPAD`).
* Cuando un mouse o touchpad físico sea detectado: mover el puntero de LVGL en pantalla (`LV_INDEV_TYPE_POINTER`).
