# Documento de Investigación Técnica y Diseño Arquitectónico Definitivo del Subsistema USB en CBDos

**Documento:** `docs/drafts/INVESTIGACION_Y_DISENO_DEFINITIVO_SUBSISTEMA_USB.md`  
**Versión:** 1.0.0  
**Fecha:** Septiembre 2026  
**Estado:** 🔍 Documento de Investigación y Análisis Formal  

---

## 🔬 1. Análisis Profundo del Hardware y Controladores USB en ESP32

Para comprender por qué los parches aislados fallan y cómo construir una arquitectura sólida y permanente, es necesario analizar el funcionamiento interno del silicio del ESP32-S3 y ESP32-P4:

```
                                ESP32-S3 PHY MULTIPLEXER
                              ┌───────────────────────────┐
                              │     Pines Físicos D+/D-   │
                              │     (Conector USB Type-C) │
                              └─────────────┬─────────────┘
                                            │
                       ┌────────────────────┴────────────────────┐
                       │                                         │
        [ ARDUINO_USB_MODE = 1 ]                  [ ARDUINO_USB_MODE = 0 ]
                       ▼                                         ▼
         ┌───────────────────────────┐             ┌───────────────────────────┐
         │ USB-Serial-JTAG Silicio   │             │   Pila USB-OTG (TinyUSB)  │
         │ • Hardwired en hardware   │             │ • Controlada por software │
         │ • Escucha DTR/RTS fija    │             │ • Soporta descriptores    │
         │ • Inmune a cuelgues       │             │   dinámicos y compuestos  │
         │ • Solo CDC/JTAG           │             │ • Clases: CDC, HID, MSC   │
         └───────────────────────────┘             └─────────────┬─────────────┘
                                                                 │
                                                   ┌─────────────┴─────────────┐
                                                   │                           │
                                                   ▼                           ▼
                                            ┌─────────────┐             ┌─────────────┐
                                            │ USBSerial   │             │  USBHID     │
                                            │ (CDC)       │             │ (FIDO2/Kbd) │
                                            └─────────────┘             └─────────────┘
```

---

## ⚠️ 2. Diagnóstico de los Problemas Previos

1. **Bifurcación Inconsistente de Modos:**
   * Al compilar con `ARDUINO_USB_MODE=1`, el SoC quedaba bloqueado exclusivamente en el hardware Serial-JTAG, impidiendo cualquier función HID (FIDO2 / BadUSB).
   * Al compilar con `ARDUINO_USB_MODE=0`, no se inicializaba `USBSerial` en conjunto con `USBHID`, lo que causaba la pérdida total del puerto serie de desarrollo (`/dev/ttyACM0`) y rompía la salida de logs de `cbdos::system::log`.

2. **Falta de Auto-Reset en Software OTG:**
   * En modo TinyUSB (`ARDUINO_USB_MODE=0`), el reseteo automático por software hacia el bootloader requiere que el endpoint CDC capture el evento de apertura a **1200 baudios** con `DTR=0 / RTS=1` y ejecute `esp_restart()`. Al no estar el CDC conectado, `esptool` no podía resetear la placa automáticamente.

3. **Inexistencia de una Capa de Abstracción `UsbManager`:**
   * Las vistas (`KerberosView`) intentaban interactuar directamente con primitivas del hardware en lugar de solicitar capacidades a un gestor centralizado del sistema operativo.

---

## 🏛️ 3. Especificación del Diseño Arquitectónico Definitivo

### 3.1. Principio Fundamental: Modelo Compuesto Base (`CDC + Dynamic Services`)
Para garantizar que el desarrollador y el usuario **NUNCA pierdan la consola serie ni el auto-flasheo**, la base del stack USB debe operar con **CDC Serial permanente como Interfaz 0/1**, mientras que las demás clases estándar (`HID`, `MSC`) se activan o desactivan **bajo demanda** como interfaces secundarias sin interrumpir la consola.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TABLA DE INTERFACES COMPUESTAS USB                       │
├──────────────┬───────────────────────────────┬──────────────────────────────┤
│  Interfaz #  │ Clase USB Estándar            │ Función                      │
├──────────────┼───────────────────────────────┼──────────────────────────────┤
│ Interfaz 0/1 │ CDC-ACM (Communications)      │ Consola CLI, Logs, Auto-Reset│
│ Interfaz 2   │ HID (Human Interface Device)  │ FIDO2 (0xF1D0), Teclado/Ratón│
│ Interfaz 3   │ MSC (Mass Storage Class)      │ Acceso a MicroSD en PC       │
└──────────────┴───────────────────────────────┴──────────────────────────────┘
```

---

### 3.2. Contrato de la API en el Núcleo (`core/include/cbdos/usb.hpp`)

```cpp
namespace cbdos {
namespace usb {

enum class UsbMode : uint8_t {
    CDC = 0,        // Solo Consola Serie / Flasheo
    HID,            // Teclado / Ratón / Llave FIDO
    MSC,            // Almacenamiento Masivo
    CompositeCdcHid // CDC + HID simultáneo (Modo Recomendado)
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
    
    // Consulta y cambio directo
    UsbMode getMode() const;
    bool setMode(UsbMode mode);

    // Pila de modos para aplicaciones y scripts (Push / Pop)
    void pushMode(UsbMode temporaryMode);
    void popMode();

    // Guardar preferencia de arranque en NVS
    void saveDefaultMode(UsbMode mode);
    UsbMode loadDefaultMode();
};

} // namespace usb
} // namespace cbdos
```

---

### 3.3. Ciclo de Vida en Aplicaciones (`KerberosView`, `FlasherView`, Scripts Lua)

```
                       ┌───────────────────────────────┐
                       │     ARRANQUE DEL SISTEMA      │
                       │     (Modo Base: CDC Serial)   │
                       └──────────────┬────────────────┘
                                      │
                       ┌──────────────▼────────────────┐
                       │   Usuario abre Kerberos FIDO  │
                       │   UsbManager::pushMode(HID)   │
                       └──────────────┬────────────────┘
                                      │
                       ┌──────────────▼────────────────┐
                       │  Interfaz HID FIDO2 Activa    │
                       │  (Consola CDC sigue viva)     │
                       └──────────────┬────────────────┘
                                      │
                       ┌──────────────▼────────────────┐
                       │  Usuario sale de Kerberos     │
                       │  UsbManager::popMode()        │
                       └──────────────┬────────────────┘
                                      │
                       ┌──────────────▼────────────────┐
                       │   PUERTO RESTAURADO A CDC     │
                       └───────────────────────────────┘
```

---

### 3.4. Interfaz de Usuario en Configuración (`ConfigView.cpp`)
* Ubicación: **Configuración -> Puertos y Conexiones**.
* Controles: Selector táctil de 4 opciones estándar (`CDC`, `HID`, `MSC`, `CDC+HID`).
* Visualización: Estado del enlace USB en tiempo real (Conectado / Desconectado / Tráfico activo).
