# Borrador Técnico: Arquitectura de Hub USB 2.0 Multi-Dispositivo en ESP32-P4 (CBDos)

**Fecha:** 4 de Septiembre de 2026  
**Autor:** Antigravity AI & kaber420  
**Target:** ESP32-P4 JC4880P443C (Controlador DWC2 USB 2.0 High-Speed 480 Mbps)  
**Estado:** Borrador de Arquitectura y Especificación Técnica  

---

## 1. Justificación y Alcance del Sistema

El Cyberdeck **ESP32-P4 (JC4880P443C)** cuenta con una potencia de cómputo destacada (RISC-V Dual-Core @ 400 MHz y 32 MB de PSRAM), pero carece de radios RF integradas en su SoC principal. Para transformarlo en una estación base táctica, concentrador de redes heterogéneas o banco de depuración/flasheo múltiple, se requiere conectar **múltiples microcontroladores coprocesadores (ESP32-C3, ESP32-C6, ESP32-S3)** a través de un único puerto físico USB OTG mediante un **Hub USB 2.0**.

Este documento define la arquitectura para soportar un concentrador USB con múltiples clientes CDC-ACM en paralelo, garantizando:
1. **Soporte de Hubs en ESP-IDF 5.5.**
2. **Identificación unívoca de dispositivos** a pesar de compartir el mismo VID/PID.
3. **Multiplexación y enrutamiento en MeshCore / Serial Terminal.**
4. **Estabilidad eléctrica y aislamiento de reinicios (*Reset por RTS* individual).**

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      ESP32-P4 (CBDos Host Central)                     │
 │  [USB Host Lib] ── [usb_host_hub] ── [cdc_acm_host Multi-Instance]     │
 └───────────────────────────────────┬────────────────────────────────────┘
                                     │ USB 2.0 High-Speed (480 Mbps)
                       ┌─────────────┴─────────────┐
                       │     HUB USB 2.0 (TT)      │
                       │    (Powered Hub 5V/2A)    │
                       └─┬─────────┬─────────┬───┬─┘
                         │ 12 Mbps │ 12 Mbps │   │ 12 Mbps
          ┌──────────────┘         │         └─┐ └─────────────┐
          ▼                        ▼           ▼               ▼
 ┌─────────────────┐      ┌─────────────────┐ ┌─────────────────┐ ...
 │   ESP32-C3 #1   │      │   ESP32-C3 #2   │ │   ESP32-S3 #3   │
 │ (Canal 1 / Mesh)│      │(Canal 6/Sniffer)│ │ (Flasher / CLI) │
 └─────────────────┘      └─────────────────┘ └─────────────────┘
```

---

## 2. Soporte de Hubs en ESP-IDF 5.5 y Controlador DWC2

### 2.1. Configuración del Kernel USB Host
A diferencia de arquitecturas previas punto a punto, ESP-IDF 5.5 soporta concentradores USB externos mediante la librería `usb_host` y el componente de hubs:
* **Habilitación en `sdkconfig`:**
  ```kconfig
  CONFIG_USB_HOST_HUBS_SUPPORTED=y
  CONFIG_USB_HOST_MAX_NUM_DEVICES=8
  CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=1024
  ```
* **Topología de Bus:**
  * El enlace **P4 ↔ Hub** opera a **High-Speed (480 Mbps)**.
  * Los enlaces **Hub ↔ Dispositivos** operan a **Full-Speed (12 Mbps)** mediante el *Transaction Translator* (TT) del chip concentrador (ej. GL850G, FE1.1S, VL817).
  * El driver `usb_host_hub` se encarga de monitorizar el cambio de estado de los puertos del Hub (interrupciones periódicas en el Endpoint de Estado del Hub).

---

## 3. Identificación Unívoca de Múltiples Dispositivos

### 3.1. La Problemática de los Identificadores Duplicados
Al conectar 3 o más módulos ESP32-C3, todos los dispositivos responden con la misma identificación estándar:
* **VID:** `0x303A` (Espressif Systems)
* **PID:** `0x1001` (ESP32 USB-Serial-JTAG)
* **Clase:** `0x02` (CDC) o `0xFF` (Vendor-Specific)

Un sistema ingenuo colisionaría al no saber a qué dispositivo enviar los datos ni a cuál aplicar el reset.

### 3.2. Estrategia de Triangulación de Identidad
CBDos resuelve la diferenciación mediante 3 capas jerárquicas:

| Nivel | Identificador | Origen | Propósito |
| :--- | :--- | :--- | :--- |
| **Físico/Topológico** | `hub_port_num` (1, 2, 3, 4) | Descriptor de puerto del Hub | Saber en qué conector físico del Hub está enchufado cada cable. |
| **Silicio/Hardware** | `iSerialNumber` (MAC eFuse) | Descriptor de Cadena USB (String 3) | Dirección MAC única de fábrica inmutable (`84:F7:03:XX:XX:XX`). |
| **Lógico/Firmware** | `RadioIdentity` / `NodeId` | Handshake de paquete en arranque | Rol asignado (ej. "Sniffer", "ESP-NOW", "LoRa Bridge"). |

#### Extracción del Número de Serie en la Enumeración:
Cuando `usb_host` detecta un nuevo dispositivo conectado en un puerto del Hub:
```cpp
struct UsbClientDescriptor {
    uint8_t hubPort;             // Puerto físico 1..N
    uint8_t devAddr;             // Dirección en el bus asignada por USB Host (1..127)
    uint16_t vid;                // 0x303A
    uint16_t pid;                // 0x1001
    char serialMac[18];          // "84:F7:03:1A:2B:3C"
    cdc_acm_dev_hdl_t cdcHandle; // Puntero al driver CDC-ACM individual
    bool isAssigned;             // En uso por Terminal, Flasher o Mesh
};
```

---

## 4. Evolución de la Capa de Software en CBDos

### 4.1. Evolución de `UsbDeviceManager` a Multidispositivo
Actualmente `UsbDeviceManager` almacena un único `m_activeDevice`. Para el Hub, evoluciona hacia una tabla de clientes USB activos:

```cpp
namespace cbdos {
namespace usb {

class UsbDeviceManager {
public:
    static constexpr size_t MAX_USB_CLIENTS = 7;

    const std::vector<UsbDeviceInfo>& getConnectedDevices() const;
    const UsbDeviceInfo* getDeviceByPort(uint8_t hubPort) const;
    const UsbDeviceInfo* getDeviceBySerial(const std::string& serial) const;

    // Callbacks con slot de puerto downstream
    void registerHubEventCallback(std::function<void(uint8_t port, bool connected)> cb);

private:
    std::vector<UsbDeviceInfo> m_deviceTable;
};

} // namespace usb
} // namespace cbdos
```

### 4.2. Pool de Puertos Seriales (`P4UsbOtgPort`)
En `hal_uart_p4.cpp`, en lugar de un único `m_portUsbOtg`, se implementa un pool indexado:
```cpp
class P4UsbMultiPortPool {
public:
    cbdos::serial::ISerialPort* getPortBySlot(uint8_t slotIndex);
    size_t getActivePortsCount() const;
private:
    std::array<P4UsbOtgPort, 4> m_otgPorts;
};
```
Cada instancia `P4UsbOtgPort` retiene su propio handle `cdc_acm_dev_hdl_t`, lo que permite:
* Leer y escribir en el ESP32-C3 #1 mientras simultáneamente se reciben datos del ESP32-C3 #2.
* Enviar un pulso de reset independiente vía RTS (`pulseControlPin`) a un solo módulo sin afectar ni reiniciar a los demás coprocesadores.

---

## 5. Integración con la Interfaz Gráfica (LVGL 9.5)

### 5.1. Desplegable de Selección en `SerialTerminalView`
Al conectar el Hub con múltiples dispositivos, el menú de puertos de la terminal se actualiza dinámicamente:

```text
[ 🔌 USB #1: ESP32-C3 (Pto 1 - 84F703) ▼ ]  [ 115200 ▼ ]  [ Abrir ]  [ ⚡ RST ]
--------------------------------------------------------------------------------
> Terminal conectada a Puerto Hub #1 (ESP32-C3 - MAC: 84:F7:03:1A:2B:3C).
> Hardware listo.
```

* **Conmutación en caliente:** Al cambiar el dropdown al dispositivo `#2`, la terminal cambia el flujo de datos inmediatamente sin bloquear el bus.
* **Control de Reset Dedicado:** Pulsar `[ ⚡ RST ]` conmuta la línea RTS virtual únicamente en el handle del dispositivo seleccionado.

### 5.2. Flasheador Universal Multi-Target (`FlasherView`)
Permite seleccionar en la pantalla táctil a cuál de los ESP32-C3 programar el firmware desde la tarjeta MicroSD (`/sdcard/firmware.bin`), enviándolo a modo bootloader por software (secuencia DTR/RTS) sin desconectar los demás módulos de la red.

---

## 6. Integración con MeshCore y Capa de Malla Heterogénea

Para la arquitectura multi-radio descrita en `PLAN_USB_CDC_MULTI_RADIO_TORRE.md`:

```text
       ┌────────────────────────────────────────────────────────┐
       │             MeshEngine (CBDos Core Routing)            │
       └───────────┬────────────────┬────────────────┬──────────┘
                   │ Slot 0         │ Slot 1         │ Slot 2
                   ▼                ▼                ▼
          [C3 #1: ESP-NOW]  [C3 #2: Sniffer]  [C3 #3: Repetidor]
```

1. **Multiplexación de Paquetes:** Cada C3 corre el firmware puente transparente de bajo retardo. Los paquetes entrantes por USB se etiquetan internamente en `MeshEngine` con el identificador del slot físico (`uint8_t slot`).
2. **Priorización de Tráfico:** Las tramas urgentes (mensajes directos o comandos de control) se despachan por el C3 con mejor RSSI/canal, mientras que el tráfico de telemetría de fondo se enruta por canales alternativos.
3. **Resiliencia ante Desconexión:** Si se extrae físicamente un C3 del Hub, `cdcEventCb` dispara la desconexión únicamente en ese slot. `MeshEngine` redistribuye automáticamente las rutas por los nodos restantes sin que el sistema sufra cuelgues ni caídas de interfaz.

---

## 7. Requisitos Eléctricos y de Protección

| Parámetro | Valor Requerido | Justificación |
| :--- | :--- | :--- |
| **Tipo de Hub** | **Alimentado (*Self-Powered Hub*)** | Evitar sobrecargar el regulador 5V interno del JC4880P443C. |
| **Fuente de Alimentación Externa** | **5V @ 2.5A mínimo** | Cada ESP32-C3 en ráfaga de emisión Wi-Fi consume ~380 mA (+20 dBm). 3 nodos = ~1.2A solo en radios. |
| **Protección contra Retorno (*Backfeeding*)** | Diodo Schottky / Conexión VBUS aislada | Evitar que la alimentación externa de 5V del Hub inyecte corriente inversa hacia el puerto USB OTG del P4. |
| **Tierra Común (GND)** | Unificada en el chasis | Mantener la integridad de los niveles lógicos diferenciales D+/D- en High-Speed. |

---

## 8. Plan de Implementación por Fases

1. **Fase 1: Configuración del Kernel (`sdkconfig`)**
   - Habilitar `CONFIG_USB_HOST_HUBS_SUPPORTED=y` en `bsp/esp32_p4_jc4880`.
   - Validar compilación limpia de la pila con driver de concentradores activo.

2. **Fase 2: Extensión de `UsbDeviceManager` para Múltiples Descriptores**
   - Incorporar lista dinámica de clientes USB detectados en el Hub.
   - Implementar extracción del string descriptor `iSerialNumber` durante la enumeración.

3. **Fase 3: Multi-Instancia en `hal_uart_p4.cpp`**
   - Implementar pool de handles `cdc_acm_dev_hdl_t` (`usb_otg_0`, `usb_otg_1`, etc.).
   - Adaptar `P4SerialBackend::getAvailablePorts()` para generar un descriptor de puerto por cada dispositivo enumerado en el Hub.

4. **Fase 4: Adaptación de la UI y Pruebas en Hardware**
   - Validar conmutación entre 2 o más ESP32-C3 conectados al Hub en `SerialTerminalView`.
   - Validar reinicio selectivo mediante `[ ⚡ RST ]` para cada cliente individual.
