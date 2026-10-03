# 📐 Plan Técnico Definitivo: Descriptor HID Compuesto Nativo TinyUSB (Teclado + Ratón + FIDO2) y Conmutación Real USB en ESP32-S3

**Fecha:** Octubre 2026  
**Documento:** `specs/architecture/plan_tecnico_descriptor_compuesto_hid_s3.md`  
**Autor:** Desarrollador Senior de Sistemas Embebidos / Equipo de Arquitectura CBDos  
**Objetivo:** Implementación nativa de HID Device en `hal_hid_s3.cpp` sin librerías externas en conflicto, y conexión real de `UsbManager` en `main.cpp`.

---

## 🏛️ 1. Diagnóstico de Arquitectura

Actualmente, el ESP32-S3 sufre dos limitaciones técnicas en la rama activa:

1. **Stubs en la capa HAL de HID (`hal_hid_s3.cpp`):**
   Las funciones `sendReport()` y `sendMouseReport()` no generan paquetes USB hacia el Host (PC).
2. **Causa del choque previo al usar librerías de Arduino:**
   Al intentar incluir `USBHIDKeyboard` y `USBHIDMouse`, cada clase crea una subclase de `USBHIDDevice` y ejecuta su propio `m_hid.begin()`. Esto sobrescribe y corrompe los descriptores USB del dispositivo `s_fidoDevice` (Kerberos FIDO2), provocando un desbordamiento/pánico en TinyUSB que reinicia el microcontrolador.
3. **Conmutador de Ajustes desconectado en `main.cpp`:**
   En `bsp/esp32_s3_jc3248/src/main.cpp`, el arranque llamaba incondicionalmente a `initHidDriverS3()`, ignorando el estado persistido en NVS por `UsbManager`.

---

## 🛠️ 2. Solución de Ingeniería: Único Descriptor Compuesto Nativo

En lugar de crear múltiples instancias de dispositivos HID con librerías de terceros, implementaremos un **único descriptor de reporte HID compuesto** gestionado por una sola clase `USBHIDCompositeS3 : public USBHIDDevice`.

### 2.1. Estructura de Report IDs Unificados

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                  DESCRIPTOR HID COMPUESTO UNIFICADO S3                       │
├────────────────────────────────┬─────────────────────────────┬───────────────┤
│ Report ID 1 (Keyboard)         │ Report ID 2 (Mouse)         │ Report ID 3   │
│ - Standard 8-byte boot report  │ - Standard relative pointer │ (FIDO2/CTAP)  │
│   (Modifiers + 6 Keycodes)     │   (Buttons, dX, dY, wheel)  │ - 64-byte raw │
└────────────────────────────────┴─────────────────────────────┴───────────────┘
```

### 2.2. Definición Exacta del Descriptor C++

```cpp
#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_FIDO     3

static const uint8_t s_s3CompositeReportDesc[] = {
    // --- TECLADO (Report ID 1) ---
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),

    // --- RATÓN (Report ID 2) ---
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),

    // --- KERBEROS FIDO2 / CTAPHID (Report ID 3) ---
    0x06, 0xD0, 0xF1, // Usage Page (FIDO Alliance 0xF1D0)
    0x09, 0x01,       // Usage (CTAPHID)
    0xA1, 0x01,       // Collection (Application)
    0x85, REPORT_ID_FIDO, //   Report ID (3)
    0x09, 0x20,       //   Usage (Input Report Data)
    0x15, 0x00,       //   Logical Min 0
    0x26, 0xFF, 0x00, //   Logical Max 255
    0x75, 0x08,       //   Report Size 8
    0x95, 0x40,       //   Report Count 64
    0x81, 0x02,       //   Input (Data,Var,Abs)
    0x09, 0x21,       //   Usage (Output Report Data)
    0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, 0x40,
    0x91, 0x02,       //   Output (Data,Var,Abs)
    0xC0              // End Collection
};
```

---

## 📝 3. Plan de Acción Detallado en 3 Pasos

### Paso 1: Refactorización de `hal_hid_s3.cpp`
- Sustituir la clase `USBHIDFido` por `USBHIDCompositeS3` que registra el descriptor unificado compuesto.
- Implementar los métodos de envío en `USBHIDCompositeS3`:
  - `sendKeyboardReport(uint8_t modifiers, const uint8_t keycodes[6])` -> Envía paquete de 8 bytes con Report ID 1 (`tud_hid_n_report(0, REPORT_ID_KEYBOARD, ...)`).
  - `sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel)` -> Envía paquete de 5 bytes con Report ID 2 (`tud_hid_n_report(0, REPORT_ID_MOUSE, ...)`).
  - `sendFidoReport(const uint8_t report[64])` -> Envía paquete de 64 bytes con Report ID 3 (`tud_hid_n_report(0, REPORT_ID_FIDO, ...)`).
- Conectar los métodos virtuales `sendReport()` y `sendMouseReport()` de `Esp32S3HidDriver` a la clase unificada.

### Paso 2: Conexión Real de `UsbManager` en `bsp/esp32_s3_jc3248/src/main.cpp`
- Tras `initPersistenceBackend()`, invocar `cbdos::usb::UsbManager::getInstance().init()`.
- Consultar `getBootMode()`:
  - Si es `UsbMode::Host`: invocar `cbdos::bsp::initUsbHostBackendS3()`.
  - Si es `UsbMode::Hid`: invocar `cbdos::bsp::initHidDriverS3()`.
- Con esto, el conmutador de modo USB en la aplicación **Ajustes (ConfigView)** funcionará de verdad tras reiniciar el sistema.

### Paso 3: Validación y Pruebas
- Compilación del target S3 con `pio run -d bsp/esp32_s3_jc3248`.
- Confirmación de cero panics/bootloops en serial.
- Verificación funcional de la pantalla como Teclado/Mouse táctil en la PC.
