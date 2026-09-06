# 🔌 Especificación Técnica: Subsistema Gestor de Puertos USB, Conmutación Dinámica de Modos y API para BadUSB / DuckyScript (CBDos v0.2.1)

**Documento:** `docs/architecture/especificacion_tecnica_gestor_usb_modos_y_api_badusb.md`  
**Versión:** 1.0.0 (RFC-CBDOS-USB-01)  
**Estado:** 💡 Especificación de Arquitectura Formal  
**Autor:** Equipo de Arquitectura de Software CBDos & Usuario  
**Fecha:** Agosto 2026  

---

## 🏛️ 1. Justificación y Objetivos

En sistemas portátiles tipo Cyberdeck, los puertos USB desempeñan múltiples roles críticos que cambian según la tarea:
1. **Consola y Depuración (USB-CDC Serial):** Comunicación interactiva CLI, logs del sistema y flasheo.
2. **Inyección de Teclas e Interacción HID (USB-HID):** Emulación de teclado y ratón para ejecución de scripts **DuckyScript / BadUSB** y herramientas de auditoría.
3. **Módem y Malla de Radio (USB TNC / SLIP):** Interfaz para aplicaciones como `MeshCore` sobre dongles de radio externos o conexión a PC.
4. **Host de Periféricos (USB-Host):** Conexión de teclados externos físicos, dongles LoRa/Zigbee y almacenamiento masivo (Pendrives).
5. **Mantenimiento y Flasheo de ROM:** Acceso al bootloader de fábrica sin necesidad de manipular botones físicos de hardware (`BOOT` + `RESET`).

El objetivo de esta especificación es definir una **capa de abstracción y gestión de puertos USB (`UsbManager`)**, con soporte multi-target (ESP32-S3 y ESP32-P4) y una **API en C++ y Lua** que permita a las aplicaciones y scripts de ataque conmutar temporalmente el modo USB y restaurarlo a su estado original al finalizar.

---

## ⚡ 2. Matriz de Hardware USB por Microcontrolador

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                            MATRIZ DE PUERTOS USB                            │
├──────────────────────────────────────┬──────────────────────────────────────┤
│       TARGET ESP32-S3 (JC3248W535)   │      TARGET ESP32-P4 (JC4880P443C)   │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ • 1 Puerto USB OTG Físico (FullSpeed)│ • 2 Puertos USB Físicos Independientes│
│ • Multiplexado por software (TinyUSB)│   1. Puerto 0: USB 2.0 High-Speed 480M│
│ • Modos: CDC, HID, Composite, Host   │      (Host para periféricos / Device) │
│                                      │   2. Puerto 1: USB-Serial-JTAG 12Mbps│
│                                      │      (Consola y Flasheo permanente)   │
└──────────────────────────────────────┴──────────────────────────────────────┘
```

### 2.1. ESP32-S3 (1 Puerto Multiplexado)
El puerto USB nativo del S3 se conmuta dinámicamente:
* **Modo `CDC_SERIAL`:** El dispositivo se enumera como puerto serie virtual (`/dev/ttyACM0` en Linux).
* **Modo `HID_KEYBOARD_MOUSE`:** Se enumera como teclado/ratón estándar sin puerto serie visible.
* **Modo `COMPOSITE_CDC_HID`:** Expone dos interfaces simultáneas (Serial + HID).
* **Modo `USB_HOST`:** Cambia la PHY a modo anfitrión para alimentar y leer periféricos.
* **Acción `REBOOT_BOOTLOADER`:** Salta por software a la ROM de flasheo (`esp_restart()`).

### 2.2. ESP32-P4 (2 Puertos Independientes)
* **Puerto 1 (Serial-JTAG):** Permanece siempre activo e inmune a los cambios de software, garantizando que el usuario **nunca pierda el acceso a la consola ni la capacidad de flashear**.
* **Puerto 0 (High-Speed):** Se utiliza preferentemente como **USB Host de 480 Mbps** (dongles LoRa, teclados, tarjetas de red) o se conmuta a emulación de dispositivo de alta velocidad.

---

## 🧠 3. Máquina de Estados y Modos de Operación (`UsbMode`)

```text
                  ┌───────────────────────────────┐
                  │       MODO PREDETERMINADO     │
                  │       (CDC Serial / Módem)    │
                  └──────────────┬────────────────┘
                                 │
           ┌─────────────────────┼─────────────────────┐
           │                     │                     │
           ▼                     ▼                     ▼
┌────────────────────┐ ┌────────────────────┐ ┌────────────────────┐
│   USB_HID_DEVICE   │ │   USB_COMPOSITE    │ │     USB_HOST       │
│  (BadUSB / Ducky)  │ │   (CDC + HID)      │ │ (Periféricos Ext)  │
└──────────┬─────────┘ └─────────┬──────────┘ └─────────┬──────────┘
           │                     │                      │
           └─────────────────────┼──────────────────────┘
                                 │ (Restaurar / Pop Mode)
                                 ▼
                  ┌───────────────────────────────┐
                  │    ESTADO PREVIO RESTAURADO   │
                  └───────────────────────────────┘
```

### 3.1. Regla de Aislamiento: Desacoplamiento Dinámico con el Gestor de Redes
Para evitar saturar la interfaz de usuario con opciones fantasma o estados confusos:
1. **Ocultamiento por Defecto en Redes:** Si el puerto USB se encuentra en modo **HID (Teclado/Ratón)**, **Consola Serial CLI** o **Almacenamiento MSC**, el puerto **NO aparecerá en el panel de Redes (`NetworkManager`)**.
2. **Registro Dinámico Exclusivo bajo Modo Módem:** Únicamente cuando el puerto USB se configure explícitamente en **Modo Módem (KISS TNC / SLIP)** o cuando se conecte un adaptador de radio externo al puerto USB Host (ej. un **ESP32-C3** actuando como radio módem para el P4), el subsistema USB notificará al Gestor de Redes para que registre dinámicamente una interfaz de paquetes por cable (*"Interfaz USB / Módem"*).
3. **Responsabilidad Única:** El subsistema de Redes solo consume paquetes de datos, mientras que `UsbManager` tiene el control absoluto de la PHY, la emulación y los modos de hardware.

---


## 🛠️ 4. Arquitectura de Software y Contratos C++ (`core/include/cbdos/usb/`)

### 4.1. Tipos de Modo USB (`usb_types.hpp`)
```cpp
namespace cbdos {
namespace usb {

enum class UsbPortId : uint8_t {
    Port0_Native = 0, // S3: Puerto Único OTG | P4: Puerto High-Speed 480 Mbps
    Port1_Debug  = 1, // P4: Puerto USB-Serial-JTAG dedicado
    MaxPorts     = 2
};

enum class UsbMode : uint8_t {
    Off = 0,
    CdcSerial,          // Puerto serie virtual / CLI interactiva / TNC
    HidKeyboardMouse,   // Teclado y ratón USB (Inyección BadUSB)
    CompositeCdcHid,    // Ambos simultáneos (Serial + HID)
    UsbHost,            // Anfitrión OTG (Dongles, teclados externos)
    MscStorage          // Almacenamiento masivo (Exponer MicroSD a PC)
};

} // namespace usb
} // namespace cbdos
```

### 4.2. Gestor Singleton `UsbManager`
```cpp
namespace cbdos {
namespace usb {

class UsbManager {
public:
    static UsbManager& getInstance();

    bool init();
    
    UsbMode getMode(UsbPortId port = UsbPortId::Port0_Native) const;
    bool setMode(UsbMode mode, UsbPortId port = UsbPortId::Port0_Native);

    // ────────────────────────────────────────────────────────────
    // Pila de Modos para Scripts (Push / Pop)
    // ────────────────────────────────────────────────────────────
    void pushMode(UsbMode temporaryMode, UsbPortId port = UsbPortId::Port0_Native);
    void popMode(UsbPortId port = UsbPortId::Port0_Native);

    // ────────────────────────────────────────────────────────────
    // Acciones Especiales de Hardware
    // ────────────────────────────────────────────────────────────
    void rebootToBootloader(); // Salta al bootloader ROM para flasheo
    void resetUsbPhy();        // Re-enumera el bus USB forzando reconexión
};

} // namespace usb
} // namespace cbdos
```

---

## 📜 5. API para Scripts Lua / BadUSB (`cbdos.usb.*`)

Los scripts de **Rubber Ducky / BadUSB** (`.ducky`, `.luapp`, `.lua`) pueden cambiar el modo del puerto automáticamente antes de lanzar el payload y restaurarlo inmediatamente al finalizar:

```lua
-- Ejemplo: Script BadUSB con auto-conmutación de modo
local usb = require("cbdos.usb")
local hid = require("cbdos.hid")

-- 1. Guardar modo actual y cambiar a HID
usb.push_mode(usb.MODE_HID)
sys.sleep_ms(500) -- Esperar a que el host reconozca el teclado

-- 2. Ejecutar payload DuckyScript
hid.press_gui("r")
sys.sleep_ms(200)
hid.type_text("cmd.exe\n")
sys.sleep_ms(300)
hid.type_text("echo CBDos BadUSB Payload Ejecutado!\n")

-- 3. Restaurar automáticamente el puerto al modo que estaba (ej. Serial CLI)
usb.pop_mode()
print("Modo USB restaurado exitosamente.")
```

### Tabla de Métodos en Lua:

| Función Lua | Descripción |
| :--- | :--- |
| **`cbdos.usb.get_mode()`** | Retorna el modo actual (`"cdc"`, `"hid"`, `"composite"`, `"host"`). |
| **`cbdos.usb.set_mode(mode)`** | Establece el modo del puerto USB de forma permanente. |
| **`cbdos.usb.push_mode(mode)`** | Guarda el modo actual en la pila y activa el modo temporal. |
| **`cbdos.usb.pop_mode()`** | Restaura el modo que estaba activo antes de la última llamada `push`. |
| **`cbdos.usb.bootloader()`** | Reinicia el Cyberdeck directamente en modo ROM Bootloader para flasheo. |

---

## 🖥️ 6. Interfaz Gráfica de Usuario (Panel `UsbManagerView`)

Ubicada en **Ajustes -> Puertos y Conexión USB**:

```text
┌──────────────────────────────────────────────────────────┐
│                 🔌 AJUSTES: PUERTOS USB                  │
├──────────────────────────────────────────────────────────┤
│ Puerto USB OTG:                                          │
│   Modo: [ 📟 USB-CDC Serial (Consola CLI)            ▾ ] │
│   Estado: Conectado a Host PC (Enumerado a 12 Mbps)      │
│                                                          │
│ Opciones de Conmutación:                                 │
│   [ ] Habilitar Emulación HID Permanente                 │
│   [x] Permitir a Scripts Lua conmutar modo (Push/Pop)    │
│                                                          │
│ ──────────────────────────────────────────────────────── │
│ ⚡ Mantenimiento:                                         │
│   [ 🔄 Reiniciar en Modo Bootloader (Flasheo) ]          │
│   (Permite flashear firmware sin presionar botones)      │
└──────────────────────────────────────────────────────────┘
```

---

## 📅 7. Fases de Implementación

| Fase | Tarea | Componentes Afectados |
| :--- | :--- | :--- |
| **Fase 1** | Definición de contratos `usb_types.hpp` y `UsbManager` en `core/`. | `core/include/cbdos/usb/` |
| **Fase 2** | Implementación del backend HAL en ESP32-S3 (TinyUSB / CDC / HID). | `bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp` |
| **Fase 3** | Implementación del backend HAL en ESP32-P4 (High-Speed Host + Serial-JTAG). | `bsp/esp32_p4_jc4880/hal/hal_usb_p4.cpp` |
| **Fase 4** | Exposición de la API `cbdos.usb.*` a LuaBridge y DuckyScript Engine. | `core/src/lua/LuaBridge.cpp` |
| **Fase 5** | Creación de la vista `UsbManagerView` en el menú de Ajustes LVGL 9.5. | `core/src/ui/views/UsbManagerView.cpp` |
