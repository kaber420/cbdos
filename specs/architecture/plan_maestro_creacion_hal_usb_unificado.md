# Plan Maestro: Creación del HAL USB Host Unificado y Erradicación de Código Duplicado

**Fecha:** 25 de Septiembre, 2026  
**Documento:** `specs/architecture/plan_maestro_creacion_hal_usb_unificado.md`  
**Estado:** Dictamen Aprobado por Muse Spark 1.3 con Modificaciones Obligatorias Incorporadas  
**Módulos Afectados:**  
- `core/include/cbdos/` (`usb_host.hpp`, `usb_manager.hpp`, `serial.hpp`)  
- `core/src/system/`  
- `bsp/esp32_p4_jc4880/hal/` (`hal_usb_host_p4.cpp`, `hal_usb_cdc_p4.cpp`, `hal_gpio_p4.cpp`, `hal_uart_p4.cpp`, `usb_cdc_loader_port.cpp`, `hal_flasher_p4.cpp`)  
- `bsp/esp32_p4_jc4880/main/CMakeLists.txt`  
- `bsp/esp32_s3_jc3248/hal/` (`hal_usb_s3.cpp`)  

---

## 🔍 1. Diagnóstico Forense y Auditoría de Código Duplicado

Tras la auditoría del código fuente activo en el BSP del ESP32-P4 y la validación cruzada con **Muse Spark 1.3**, se confirma fehacientemente la sospecha del usuario: **el código USB está disperso, duplicado en 3 archivos independientes del BSP y carece de abstracción HAL formal en el `core/`**.

### 1.1. Matriz de Código Duplicado y Conflictos

| Operación / Responsabilidad | Componente 1: `usb_device_manager.cpp` | Componente 2: `hal_uart_p4.cpp` (`P4UsbOtgPort`) | Componente 3: `usb_cdc_loader_port.cpp` | Diagnóstico de Conflicto |
|---|---|---|---|---|
| **Instalación USB Host** (`usb_host_install`) | Línea 121 | No | Línea 81 | **Doble llamada conflictiva**. Dos hilos inicializando el hardware USB Host. |
| **Tarea de Bombeo de Eventos** (`usb_host_lib_task`) | Líneas 36-42 (`usb_host_lib`) | No | Líneas 23-29 (`usb_host_task`) | **Condición de carrera crítica**. Dos tareas concurrentes llamando a `usb_host_lib_handle_events()`, prohibido por ESP-IDF v5.x. |
| **Instalación CDC-ACM Host** (`cdc_acm_host_install`) | Línea 137 | No | Línea 97 | **Doble registro del driver CDC**. |
| **Apertura de Dispositivo CDC** (`cdc_acm_host_open` / `vendor_specific`) | No | Líneas 161-164 | Líneas 116-119 | **Competencia destructiva**. Dos clientes abren el mismo hardware en lugar de compartir un handle gestionado. |
| **Control de Líneas DTR / RTS** (`cdc_acm_host_set_control_line_state`) | No | Líneas 146, 173, 183, 247, 254-267 | Líneas 127, 146-149, 220-232 | **Lógica repetida y descoordinada**. Ambas clases implementan sus propios toggles DTR/RTS para resetear microcontroladores. |
| **Transmisión de Datos** (`cdc_acm_host_data_tx_blocking`) | No | Línea 224 | Línea 160 | **Código redundante**. |
| **Recepción y Buffers** | No | `RingbufferHandle_t` (2 KB) en callback `cdcRxCb` | Buffer plano con `memmove` (1 KB) + semáforo en `cdc_rx_callback` | **Inconsistencia**. La terminal serial usa RingBuffer; el flasher usa buffer estático ineficiente de 1 KB con copia O(n²). |
| **Cierre de Conexión** (`cdc_acm_host_close`) | No | Líneas 123, 276 | Línea 135 | **Cierre destructivo cruzado**. Cuando el flasher termina, destruye el handle de hardware dejando colgada la terminal serial. |

---

## 🏛️ 2. Arquitectura del HAL USB Host de CBDos

Para evitar colisiones de nomenclatura con `core/include/cbdos/usb_manager.hpp` (que gestiona exclusivamente el modo de arranque PHY `Hid` vs `Host`), el nuevo contrato se define en:  
👉 **`core/include/cbdos/usb_host.hpp`**

```
                    ┌───────────────────────────────────────────────┐
                    │               APLICACIONES Y UI               │
                    │  (Serial Terminal, Flasher, Red Mesh, Shell)  │
                    └───────────────────────┬───────────────────────┘
                                            │
                                            ▼
                    ┌───────────────────────────────────────────────┐
                    │       CORE HAL: <cbdos/usb_host.hpp>          │
                    │  - IUsbHostBackend (Gestión Central Host)     │
                    │  - IUsbDriver (Plugins: CDC, MSC, HID)        │
                    │  - IUsbCdcChannel (Hereda de ISerialPort)     │
                    │  - UsbIoResult, UsbDeviceInfo, DeviceClass    │
                    └───────────────────────┬───────────────────────┘
                                            │
                                            ▼
                    ┌───────────────────────────────────────────────┐
                    │          BSP IMPLEMENTATION (ESP32-P4)        │
                    ├───────────────────────┬───────────────────────┤
                    │    hal_usb_host_p4    │    hal_usb_cdc_p4     │
                    │  - Único usb_host_*   │  - Único cdc_acm_*    │
                    │  - Única tarea bombeo │  - Demux ISR/Task-safe│
                    │  - Hot-Plug & Cuarent.│  - StreamBuffer 16 KB │
                    └───────────────────────┴───────────────────────┘
                                            │
                                            ▼
                               [ Hardware USB OTG PHY ]
```

### 2.1. Contrato del HAL en `core/include/cbdos/usb_host.hpp`

```cpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include "cbdos/serial.hpp"

namespace cbdos {
namespace usb {

// Códigos de resultado explícitos para I/O robusto (vital para Flasher a 921600 bps)
enum class UsbIoResult {
    Ok,
    Timeout,
    Disconnected,
    BufferOverflow,
    HardwareError
};

enum class DeviceClass : uint8_t {
    Unknown,
    CdcAcm,         // Serial CDC estándar
    VendorSpecific, // ESP32 USB-Serial-JTAG / CP210x / CH340 / FTDI
    MassStorage,    // Pendrives / Discos USB MSC
    Hid             // Teclados, ratones, lectores barcode
};

enum class DeviceRole : uint8_t {
    Unassigned,               // Conectado, sin arbitrar
    FieldTarget,              // Microcontrolador externo para programar
    PhysicalNetworkInterface, // Módem de radio / LoRa / SLIP
    SerialTerminal,           // Consola interactiva UART/CDC
    StorageDrive,             // Almacenamiento masivo montado en /usb/
    InputPeripheral           // Periférico de entrada
};

// Causa del evento de conexión/desconexión
enum class UsbEventCause : uint8_t {
    PhysicalHotPlug,          // Conexión/desconexión física de cable
    TargetResetInduced,       // Desconexión temporal provocada por secuencia DTR/RTS
    DriverError               // Fallo en bus o protocolo
};

struct UsbDeviceInfo {
    uint16_t vid = 0;
    uint16_t pid = 0;
    DeviceClass devClass = DeviceClass::Unknown;
    DeviceRole role = DeviceRole::Unassigned;
    char manufacturer[32] = {0};
    char product[32] = {0};
    char serialNumber[32] = {0};
    bool isConnected = false;
};

// Arbitraje y desalojo prioritario
enum class CdcOwner : uint8_t {
    None,       // Libre
    Terminal,   // Consumo por terminal interactiva
    Flasher     // Consumo exclusivo y prioritario por el programador
};

// Interfaz unificada del Canal CDC (Hereda de ISerialPort para evitar duplicar el HAL Serial)
class IUsbCdcChannel : public cbdos::serial::ISerialPort {
public:
    virtual ~IUsbCdcChannel() = default;

    // Arbitraje con soporte de preemption (Flasher puede desalojar a Terminal con aviso)
    virtual bool tryAcquire(CdcOwner owner, uint32_t timeoutMs = 1000) = 0;
    virtual void release(CdcOwner owner) = 0;
    virtual CdcOwner getCurrentOwner() const = 0;

    // I/O de alta velocidad con discriminación de errores
    virtual UsbIoResult read(uint8_t* buffer, size_t maxLen, size_t& outLen, uint32_t timeoutMs) = 0;
    virtual UsbIoResult write(const uint8_t* data, size_t len, size_t& outWritten, uint32_t timeoutMs) = 0;
    virtual void purge() = 0;

    // Configuración completa de línea
    virtual bool setLineCoding(uint32_t baud, uint8_t dataBits = 8, uint8_t parity = 0, uint8_t stopBits = 1) = 0;

    // Secuencia de Reset adaptativa según VID/PID (4 pasos para JTAG nativo, 2 para puentes USB-UART)
    virtual bool resetTarget(bool enterBootloader) override = 0;
};

// Interfaz de Plugin de Driver USB (para CDC, MSC, HID Host)
class IUsbDriver {
public:
    virtual ~IUsbDriver() = default;
    virtual const char* getDriverName() const = 0;
    virtual uint8_t getPriority() const { return 10; }
    virtual bool match(const UsbDeviceInfo& dev) = 0;
    virtual bool onAttach(const UsbDeviceInfo& dev) = 0;
    virtual void onDetach(const UsbDeviceInfo& dev) = 0;
};

// Evento de hot-plug con causa explícita
using UsbEventCallback = std::function<void(const UsbDeviceInfo& dev, bool connected, UsbEventCause cause)>;

// Interfaz Central del Backend USB Host
class IUsbHostBackend {
public:
    virtual ~IUsbHostBackend() = default;

    virtual bool init() = 0;
    virtual void deinit() = 0;
    virtual bool supportsHost() const = 0; // Consulta explícita de hardware (P4 true, S3 false)

    virtual bool registerDriver(IUsbDriver* driver) = 0;
    virtual bool unregisterDriver(IUsbDriver* driver) = 0;

    virtual bool isDeviceConnected() const = 0;
    virtual bool getActiveDevice(UsbDeviceInfo& outInfo) const = 0; // Copia segura, no puntero dangling

    virtual void registerEventCallback(UsbEventCallback callback) = 0;
    virtual IUsbCdcChannel* getCdcChannel() = 0;
};

// Inyección e instanciación
IUsbHostBackend* getUsbHostBackend();
void setUsbHostBackend(IUsbHostBackend* backend);

} // namespace usb
} // namespace cbdos
```

---

## 🧩 3. Erradicación de Código Duplicado: Mapeo Funcional

### 3.1. Única Tarea de Bombeo y Máquina de Estados de Cuarentena (`hal_usb_host_p4.cpp`)
* **Unificación de Tarea:** Se elimina la tarea duplicada de `usb_cdc_loader_port.cpp`. Una sola tarea `usb_host_lib_task` en `hal_usb_host_p4.cpp` llama a `usb_host_lib_handle_events()`.
* **Máquina de Estados de Cuarentena:**
  ```
  [ Connected ] ──(DTR/RTS Reset Target)──► [ Quarantined (Debounce 1000ms) ]
        │                                                     │
  (Desconexión real)                                   (Re-enumeración ROM)
        ▼                                                     ▼
  [ Disconnected ]                                      [ InBootloader ]
  ```
  Evita falsos eventos de desconexión durante el reset a bootloader de chips con USB-Serial-JTAG (`0x303A:0x1001`).

### 3.2. Fusión CDC, Demux Task-Safe y Buffer Dimensionado a 16 KB (`hal_usb_cdc_p4.cpp`)
* **Buffer Escalado para 921600 bps:** El flasher a 921600 bps transfiere ~92 KB/s. Un buffer de 4 KB daba apenas 43 ms de margen. Se dimensiona un `StreamBuffer` de 16 KB con nivel de agua (*watermark*) para garantizar flasheos continuos de binarios de varios megabytes.
* **Demultiplexor Libre de Bloqueos:** El callback nativo corre en contexto USB; utiliza `xStreamBufferSendFromISR` o `xRingbufferSendFromISR` sin invocaciones a `malloc`, locks ni logs bloqueantes.

### 3.3. Integración con `ISerialPort` (Cero HALs Paralelos)
* Como `IUsbCdcChannel` hereda de `ISerialPort`, `P4UsbOtgPort` en `hal_uart_p4.cpp` se convierte en un adaptador transparente o delegador directo. La Terminal Serial y el Flasher comparten exactamente el mismo objeto sin bifurcaciones de código.

---

## 📁 4. Plan de Archivos a Crear y Reorganizar

| Archivo | Acción | Responsabilidad |
|---|---|---|
| [`core/include/cbdos/usb_host.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/usb_host.hpp) | **CREAR** | Contrato universal del HAL USB Host: `IUsbHostBackend`, `IUsbCdcChannel`, `UsbIoResult`, `IUsbDriver`. |
| [`bsp/esp32_p4_jc4880/hal/hal_usb_host_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_usb_host_p4.cpp) | **CREAR** | Backend USB Host central para ESP32-P4. Único `usb_host_install` y gestor de cuarentena. Reemplaza a `usb_device_manager.cpp`. |
| [`bsp/esp32_p4_jc4880/hal/hal_usb_cdc_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_usb_cdc_p4.cpp) | **CREAR** | Driver CDC ACM Host y canal I/O unificado. Demux RX seguro, StreamBuffer 16 KB, arbitraje con preemption. |
| [`bsp/esp32_p4_jc4880/hal/hal_gpio_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_gpio_p4.cpp) | **CREAR** | Extracción del HAL de GPIO fuera de `hal_uart_p4.cpp` (limpieza modular previa). |
| [`bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp) | **REFACTORIZAR** | Se remueven GPIO y código CDC duplicado; delega el puerto OTG en `IUsbCdcChannel`. |
| [`bsp/esp32_p4_jc4880/hal/usb_cdc_loader_port.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/usb_cdc_loader_port.cpp) | **REFACTORIZAR** | Se eliminan tareas y dobles handles; se conecta limpiamente a `IUsbCdcChannel`. |
| [`bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp) | **CREAR** | Implementación de `IUsbHostBackend` para ESP32-S3 con `supportsHost() = false` explícito. |

---

## 🛠️ 5. Fases de Ejecución Fix-Forward

### Fase 0: Línea Base y Respaldo Físico (Zero Trust)
1. Comprobar compilación limpia actual de ambas plataformas:
   - ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
2. Preservar archivos históricos en `specs/history/`.

### Fase 1: Extracción Modular Aislada de GPIO
1. Mover `P4GpioBackend` fuera de `hal_uart_p4.cpp` a [`hal_gpio_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_gpio_p4.cpp).
2. Registrar en CMakeLists de P4 y compilar.

### Fase 2: Creación de la Interfaz HAL en Core
1. Crear [`core/include/cbdos/usb_host.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/usb_host.hpp).
2. Crear stub en ESP32-S3 [`hal_usb_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_usb_s3.cpp) con `supportsHost() = false`.
3. Validar compilación dual.

### Fase 3: Implementación del Backend Host y Driver CDC en P4
1. Crear `hal_usb_host_p4.cpp` con tarea única de bombeo y cuarentena de reset.
2. Crear `hal_usb_cdc_p4.cpp` con `IUsbCdcChannel`, preemption y `StreamBuffer` de 16 KB.
3. Conectar `P4UsbOtgPort` para consumir `IUsbCdcChannel`.
4. Validar Terminal Serial por puerto USB OTG.

### Fase 4: Migración del Flasher hacia el HAL USB
1. Conectar `usb_cdc_loader_port.cpp` directamente a `IUsbCdcChannel::tryAcquire(CdcOwner::Flasher)`.
2. Eliminar del loader port todas las variables estáticas duplicadas, tareas concurrentes y llamadas a `usb_host_install`.
3. Probar flasheo completo de ESP32-C3 / S3 y placas con CP210x/CH340.

---

## 🧪 6. Matriz de Pruebas de Aceptación (Test Matrix)

| Test ID | Escenario | Procedimiento | Criterio de Éxito |
|---|---|---|---|
| **USB-T01** | Compilación Dual Limpia | Compilar ESP32-P4 (ESP-IDF) y ESP32-S3 (PlatformIO). | 0 errores de enlazado; 0 símbolos duplicados (`WHOLE_ARCHIVE` limpio). |
| **USB-T02** | Hot-Plug Reactivo | Enchufar y desenchufar dispositivo USB al puerto OTG del P4. | Detección inmediata en logs sin polling; evento reactivo hacia observadores. |
| **USB-T03** | Terminal Serial CDC | Abrir `usb_otg` desde la app Terminal a 115200 bps. | Envío y recepción bidireccional fluido sin desbordamientos de buffer. |
| **USB-T04** | Arbitraje y Preemption | Abrir Terminal en `usb_otg`, luego iniciar Flasher USB. | Flasher desaloja limpiamente a Terminal; al terminar el flasheo, la Terminal recupera el canal sin reiniciar el hardware. |
| **USB-T05** | Flasheo Target Nativo JTAG | Conectar ESP32-C3 o S3 (`0x303A:0x1001`) y flashear `.bin`. | Secuencia de 4 pasos entra en bootloader; cuarentena evita disconnect falso; 100% flasheado. |
| **USB-T06** | Flasheo Bridge USB-UART | Conectar placa con CP210x o CH340 y flashear a 921600 bps. | Secuencia de 2 pasos entra en bootloader; salto a 921600 bps sin pérdidas de bytes; 100% flasheado. |
| **USB-T07** | Resiliencia ante Desconexión | Desconectar cable USB durante flasheo o sesión serial. | No hay Kernel Panic; recursos liberados ordenadamente. |
| **USB-T08** | Stress & Stack High Watermark | 20 ciclos de hot-plug/flasheo continuo. | `uxTaskGetStackHighWaterMark` > 512 bytes en todas las tareas USB; cero memory leaks. |
