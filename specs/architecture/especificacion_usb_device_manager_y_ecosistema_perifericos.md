# Especificación Técnica: Gestor Universal de Dispositivos USB (`UsbDeviceManager`) y Ecosistema de Periféricos en CBDos

**Fecha:** 2 de Septiembre, 2026  
**Documento:** `docs/architecture/especificacion_usb_device_manager_y_ecosistema_perifericos.md`  
**Estado:** Aprobado para Diseño e Implementación  
**Módulos Afectados:** `bsp/esp32_p4_jc4880/hal/`, `core/include/cbdos/`, `core/src/network/`, `core/src/flasher/`, `core/src/ui/`  

---

## 🎯 1. Visión General y Filosofía de Diseño

El puerto USB 2.0 High-Speed OTG del procesador **ESP32-P4** es el canal periférico externo de mayor ancho de banda y versatilidad de CBDos. Cuenta con auto-alimentación integrada de 5V (VBUS) y capacidad Host.

### Principios Rectores:
1. **Agnosticismo de Bus:** El puerto USB **NO pertenece** a un subsistema específico (ni a la radio mesh, ni exclusivamente al flasher). Es un bus universal dinámico del sistema operativo.
2. **Zero Polling & Zero Presunciones:** La detección de hardware opera 100% impulsada por eventos nativos e interrupciones del stack USB Host de ESP-IDF (`new_dev_cb` y `CDC_ACM_HOST_DEVICE_DISCONNECTED`).
3. **Identificación Estándar por Descriptores:** Todo dispositivo se identifica leyendo sus descriptores oficiales universales de hardware: **VID (Vendor ID)**, **PID (Product ID)**, **Clase de Dispositivo** y cadenas de texto (`iProduct`, `iManufacturer`, `iSerialNumber`).
4. **Soporte de Standalone Field Programmer:** Si se detecta un microcontrolador (ESP32, etc.) en modo bootloader o serial, el sistema lo expone de inmediato al programador de campo autónomo de CBDos para flashear cualquier binario `.bin` desde la MicroSD, sin forzarlo a ser un módem de radio.

---

## 🏛️ 2. Arquitectura del `UsbDeviceManager`

El `UsbDeviceManager` actúa como árbitro central del bus USB Host. Los diferentes servicios del sistema operativo se registran como controladores modulares (`IUsbDriver`) que compiten o se asignan según los descriptores del dispositivo enchufado.

```
                           ┌─────────────────────────────────────────┐
                           │       Puerto USB OTG (P4 Host)          │
                           └────────────────────┬────────────────────┘
                                                │ (Interrupción D+/D-)
                                                ▼
                           ┌─────────────────────────────────────────┐
                           │            UsbDeviceManager             │
                           │  - Enumeración y lectura VID/PID        │
                           │  - Máquina de estados de Hot-Plug       │
                           │  - Despachador de eventos a Drivers     │
                           └────────────────────┬────────────────────┘
                                                │
       ┌────────────────────┬───────────────────┼───────────────────┬────────────────────┐
       │                    │                   │                   │                    │
       ▼                    ▼                   ▼                   ▼                    ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     ┌──────────────┐
│  Driver 1:   │    │  Driver 2:   │    │  Driver 3:   │    │  Driver 4:   │     │  Driver 5:   │
│ Field        │    │ Interfaz     │    │ Serial VCP   │    │ Mass Storage │     │ HID Input    │
│ Programmer   │    │ Física Red   │    │ Genérico     │    │ (MSC)        │     │ (Teclado /   │
│ (ESP32 ROM / │    │ (Slot 2 en   │    │ (CP210x,     │    │ (Pendrives   │     │  Escáner)    │
│  Bootloader) │    │  NetworkMgr) │    │  CH340, GPS) │    │  en /usb/)   │     │              │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘     └──────────────┘
```

---

## 🧩 3. Interfaces de Software y Contratos

### 3.1. Estructura de Información de Dispositivo (`UsbDeviceInfo`)
```cpp
namespace cbdos {
namespace usb {

enum class DeviceClass {
    Unknown,
    CdcAcm,         // Serial CDC estándar
    VendorSpecific, // ESP32 USB-Serial-JTAG / CP210x / CH340
    MassStorage,    // Memorias USB
    Hid             // Teclados, lectores de código de barras
};

enum class DeviceRole {
    Unassigned,               // Recién conectado, sin driver asignado
    FieldTarget,              // Microcontrolador listo para programar con Flasher
    PhysicalNetworkInterface, // Interfaz física de red (L1/L2) para NetworkInterfaceManager
    SerialTerminal,           // Dispositivo serial para consola o GPS/TNC
    StorageDrive,             // Unidad de disco montada
    InputPeripheral           // Dispositivo de entrada
};

struct UsbDeviceInfo {
    uint16_t vid;
    uint16_t pid;
    DeviceClass devClass;
    DeviceRole role;
    char manufacturer[32];
    char product[32];
    char serialNumber[32];
    void* handle; // Handle opaco del driver CDC-ACM / USB Host
    bool isConnected;
};

} // namespace usb
} // namespace cbdos
```

### 3.2. Interfaz Abstracta de Controlador (`IUsbDriver`)
Cualquier módulo que desee consumir hardware del puerto USB debe implementar esta interfaz:

```cpp
namespace cbdos {
namespace usb {

class IUsbDriver {
public:
    virtual ~IUsbDriver() = default;

    // Nombre amigable del driver (ej. "FieldProgrammerDriver", "CbdosMeshModemDriver")
    virtual const char* getDriverName() const = 0;

    // Prioridad de sondeo (0 = baja, 100 = máxima)
    virtual uint8_t getPriority() const { return 10; }

    // Evalúa si este driver sabe manejar el dispositivo según sus descriptores VID/PID
    virtual bool match(const UsbDeviceInfo& dev) = 0;

    // Se invoca cuando el dispositivo es asignado a este driver
    virtual bool onAttach(UsbDeviceInfo& dev) = 0;

    // Se invoca cuando el dispositivo se desconecta físicamente del bus
    virtual void onDetach(const UsbDeviceInfo& dev) = 0;
};

} // namespace usb
} // namespace cbdos
```

---

## 🔄 4. Ciclo de Vida y Detección de Dispositivos (Event-Driven)

```
[ INSERCIÓN FÍSICA ]
        │
        ▼
1. Interrupción de hardware USB PHY (Líneas D+/D-)
        │
        ▼
2. ESP-IDF llama al callback nativo: cdc_acm_new_dev_callback(dev_hdl)
        │
        ▼
3. UsbDeviceManager lee descriptores:
   - VID / PID (ej. 0x303A:0x1001)
   - iProduct string (ej. "USB JTAG/serial debug unit")
   - DeviceClass
        │
        ▼
4. Ronda de "Match" entre drivers registrados por prioridad:
   │
   ├── CASO A: Es un ESP32 en modo Bootloader (ROM / Download mode)
   │   └──> Match: FieldProgrammerDriver
   │        └──> Notifica al sistema: "Target ESP32 conectado. Listo para Flashear."
   │
   ├── CASO B: Es un ESP32 en modo Normal / Firmware Activo
   │   └──> Se envía paquete ligero de sondeo (Probe 0xAA 0x55 con timeout 200ms):
   │        ├── ¿Responde con ACK de Módem de Radio?
   │        │   └──> Match: UsbNetworkInterfaceDriver
   │        │        └──> Registra una INTERFAZ FÍSICA DE RED (Slot 2) en el NetworkInterfaceManager.
   │        │             El sistema operativo reporta: "Nueva interfaz L1/L2 disponible (MAC, Canal, Potencia)".
   │        │             Cualquier servicio superior (Mesh, BBS, Sniffer, Routing) decide si usarla.
   │        └── ¿No responde o es firmware propio del usuario?
   │            └──> Match: GenericVcpSerialDriver / FieldProgrammer
   │                 └──> Queda disponible como Puerto Serie genérico y para reprogramación.
   │
   └── CASO C: Es un convertidor serie comercial (CP2102, CH340, FTDI)
       └──> Match: GenericVcpSerialDriver (Disponible para Terminal / TNC)

[ EXTRACCIÓN FÍSICA ]
        │
        ▼
1. Interrupción de hardware: CDC_ACM_HOST_DEVICE_DISCONNECTED
        │
        ▼
2. UsbDeviceManager invoca onDetach() en el driver activo.
        │
        ▼
3. Se liberan buffers y se ejecuta cdc_acm_host_close(dev).
        │
        ▼
4. Notificación de "Puerto USB Libre / Desconectado" hacia el NetworkInterfaceManager (baja de interfaz L1/L2) y el Kernel.
```

---

## 🛠️ 5. Los Roles Clave para el Usuario y el Sistema Operativo

### 5.1. Standalone Field Programmer (Programador de Campo Autónomo)
* **Objetivo:** Flashear en campo microcontroladores externos alimentándolos directamente con los 5V del puerto USB del P4.
* **Comportamiento:** Cuando se conecta un microcontrolador en modo bootloader:
  * El sistema no asume nada sobre qué archivo grabar.
  * Abre el target en la aplicación **Field Programmer**.
  * El usuario navega por su tarjeta MicroSD (`/sdcard/firmwares/`), selecciona cualquier binario `.bin` (firmware de sensores, sniffer, proyecto Arduino propio, módem o microPython) y ejecuta el flasheo a 460800 o 921600 baudios.

### 5.2. Interfaz Física de Red USB (Capa L1/L2 en NetworkInterfaceManager - Slot 2)
* **Objetivo:** Proporcionar al sistema operativo un adaptador de red físico externo (Capa 1 física / Capa 2 enlace de datos), exactamente igual que conectar un dongle Wi-Fi o Ethernet por USB.
* **Desacoplamiento Estricto:**
  * Esta interfaz **NO es un motor de malla** ni pertenece a ninguna app.
  * Solo expone la abstracción estándar `INetworkInterface` (enviar paquetes crudos L2, recibir tramas L2, reportar canal y MAC física real).
  * El `NetworkInterfaceManager` simplemente notifica al sistema: *"Interfaz Slot 2 conectada / activa"*.
  * Las aplicaciones o motores superiores (el motor de ruteo `RoutingEngine`, el tablón `cbdBBS`, una herramienta de auditoría/sniffer o un puente serial) son libres de transmitir o escuchar por este slot según la configuración del usuario.

### 5.3. Puerto Serial Genérico / Terminal / TNC
* **Objetivo:** Soporte para dispositivos que no corren firmware de CBDos (dispositivos comerciales con chips CH340, CP210x o FTDI, como antenas GPS USB, módems satelitales, radios TNC APRS o consolas seriales).
* **Comportamiento:** El usuario puede abrir la app de Terminal Serie o enviar comandos Lua para interactuar directamente con el dispositivo a la velocidad que configure.

---

## 🖥️ 6. Integración con la Interfaz de Usuario (LVGL 9.5)

El `UsbDeviceManager` emite eventos que son escuchados por las pantallas de CBDos sin polling:

* **En la HeaderBar:** Icono de USB dinámico:
  * Oculto si el puerto está vacío.
  * Azul si hay un dispositivo conectado y activo.
  * Ámbar si hay un microcontrolador listo para programar en el Flasher.
* **En NetworkManagerView (Slot 2):**
  * Si no hay módem de red conectado: Muestra el estado real del puerto (*"Puerto USB libre"* o *"Dispositivo conectado: ESP32-C3 (Modo Serial / Target Flasher)"*).
  * Si hay módem de red: Muestra MAC, Firmware, Canal y Potencia reales.
* **En Field Programmer (Flasher):**
  * Detecta automáticamente la conexión del target y muestra su chip exacto (ej. `ESP32-C3 Rev 0.3`) sin que el usuario tenga que presionar botones de reset manuales.

---

## 📋 7. Plan de Implementación por Fases

1. **Fase 1: Creación de `UsbDeviceManager` en el BSP P4:**
   * Archivos: `bsp/esp32_p4_jc4880/hal/usb_device_manager.hpp` y `.cpp`.
   * Integración de los callbacks nativos `cdc_acm_new_dev_callback` y `CDC_ACM_HOST_DEVICE_DISCONNECTED`.
   * Lectura de descriptores USB estándar (VID, PID, strings).

2. **Fase 2: Registro de Drivers:**
   * Desacoplar `hal_flasher_p4.cpp` para que use el `UsbDeviceManager` como fuente de dispositivos targets.
   * Modularizar la interfaz de red USB en un adaptador físico desacoplado (`UsbNetworkInterface` para Slot 2) que solo se activa y se registra en `NetworkInterfaceManager` cuando el `UsbDeviceManager` detecta una interfaz de radio compatible.

3. **Fase 3: Notificación Reactiva a la UI:**
   * Conectar eventos de inserción/extracción con las vistas de LVGL 9.5 (`NetworkManagerView`, `MeshCoreView` y `FlasherView`).
