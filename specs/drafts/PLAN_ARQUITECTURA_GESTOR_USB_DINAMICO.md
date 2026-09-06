# Especificación de Arquitectura: Gestor Dinámico de Modos USB Estándar para CBDos

**Documento:** `docs/drafts/PLAN_ARQUITECTURA_GESTOR_USB_DINAMICO.md`  
**Versión:** 1.0.0  
**Estado:** 📋 En Planificación y Revisión de Arquitectura  

---

## 🏛️ 1. Objetivos del Módulo
Permitir la conmutación **100% dinámica en tiempo de ejecución (Hot-Plug / Hot-Switching)** entre las clases oficiales del estándar USB, directamente desde la pantalla táctil de CBDos y a través de scripts Lua, sin necesidad de recompilar el firmware ni alterar configuraciones manuales.

---

## 🔌 2. Clases USB Oficiales Soportadas

El sistema implementa exclusivamente la nomenclatura y estándares oficiales de la especificación USB:

| Modo USB | Clase Estándar | Función en CBDos |
| :--- | :--- | :--- |
| **`CDC`** | Communications Device Class | Puerto Serie Virtual / Consola CLI / Flasheo / Logs. |
| **`HID`** | Human Interface Device | Interacción de interfaz humana (Teclado, Ratón, Llave FIDO). |
| **`MSC`** | Mass Storage Class | Acceso directo a la tarjeta MicroSD desde la PC como unidad USB. |
| **`CDC + HID`** | Composite Device | Puerto Serie y periférico HID funcionando simultáneamente. |
| **`OFF`** | PHY Desactivada | Puerto USB en reposo (bajo consumo / aislamiento). |

---

## 🧩 3. Arquitectura del Núcleo (`core/`)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           CAPA DE APLICACIÓN                            │
│           • Selector de Modo en Configuracion       │
│           • Scripts Lua: cbdos.usb.push_mode() / pop_mode()             │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
┌────────────────────────────────────▼────────────────────────────────────┐
│                       NÚCLEO USB (Agnóstico C++)                        │
│                 core/include/cbdos/usb.hpp                              │
│                 core/src/usb/UsbManager.cpp                             │
│                                                                         │
│   • Máquina de estados: getMode() / setMode(UsbMode)                   │
│   • Pila dinámica: pushMode(UsbMode) / popMode()                       │
│   • Persistencia de modo predeterminado en NVS                          │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ Interfaces HAL IUsbDriver
┌────────────────────────────────────▼────────────────────────────────────┐
│                              CAPA BSP / HAL                             │
│                                                                         │
│   ESP32-S3 (bsp/.../hal_usb_s3.cpp):                                    │
│   • Pila TinyUSB OTG con re-enumeración dinámica al vuelo               │
│                                                                         │
│   ESP32-P4 (bsp/.../hal_usb_p4.cpp):                                    │
│   • Puerto 0: High-Speed 480 Mbps (Host / Device conmutable)            │
│   • Puerto 1: USB-Serial-JTAG (Consola y Flasheo permanente)            │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## ⚙️ 4. Definición de Contratos en C++ (`core/include/cbdos/usb.hpp`)

```cpp
namespace cbdos {
namespace usb {

enum class UsbMode : uint8_t {
    Off = 0,
    CDC,            // Puerto Serie / Consola
    HID,            // Dispositivo de Interfaz Humana
    MSC,            // Almacenamiento Masivo
    CompositeCdcHid // Compuesto CDC + HID
};

enum class UsbPortId : uint8_t {
    Port0_Native = 0, // S3 OTG / P4 High-Speed
    Port1_Debug  = 1  // P4 Serial-JTAG dedicado
};

class IUsbDriver {
public:
    virtual ~IUsbDriver() = default;
    virtual bool setMode(UsbMode mode) = 0;
    virtual UsbMode getMode() const = 0;
    virtual bool isConnected() = 0;
    virtual void rebootToBootloader() = 0;
};

class UsbManager {
public:
    static UsbManager& getInstance();

    bool init();
    
    UsbMode getMode(UsbPortId port = UsbPortId::Port0_Native) const;
    bool setMode(UsbMode mode, UsbPortId port = UsbPortId::Port0_Native);

    // Pila para conmutación temporal en scripts/apps
    void pushMode(UsbMode tempMode, UsbPortId port = UsbPortId::Port0_Native);
    void popMode(UsbPortId port = UsbPortId::Port0_Native);

    void rebootToBootloader();
};

} // namespace usb
} // namespace cbdos
```

---

## 📜 5. Integración con Scripts Lua (`cbdos.usb.*`)

Los scripts pueden cambiar temporalmente el modo USB para una tarea y restaurarlo automáticamente al terminar:

```lua
local usb = require("cbdos.usb")

-- Guardar modo actual y cambiar a HID
usb.push_mode("hid")
sys.sleep_ms(300)

-- (Ejecución de tareas)

-- Restaurar modo anterior (ej. CDC)
usb.pop_mode()
```

---

## 🖥️ 6. Interfaz de Usuario (LVGL 9.5)
* **Ubicación:** `ConfigView` -> Sección "Puertos y Conexiones" y widget en `QuickSettingsPanel`.
* **Componente:** Selector de 4 botones estilo radio (`CDC`, `HID`, `MSC`, `CDC+HID`).
* **Comportamiento:** Al pulsar una opción, el bus USB se re-enumera inmediatamente en caliente y la opción elegida se almacena en la NVS.
