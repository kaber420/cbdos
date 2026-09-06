# 📡 Plan Técnico: Célula Multi-Radio USB CDC (Torre P4 + Hub + 3x C3 + SBC Gateway)

**Documento:** `docs/drafts/PLAN_USB_CDC_MULTI_RADIO_TORRE.md`
**Versión:** 1.0.0
**Estado:** 🔍 Borrador para Evaluación
**Fecha:** Agosto 2026

---

## 1. Resumen Ejecutivo

Este plan define la implementación de una **célula de radio multi-banda** donde un **ESP32-P4** actúa como hub central conectado por **USB 2.0 High-Speed** a un **USB Hub** alimentado, al cual se conectan **3 módulos ESP32-C3** como front-ends de radio dedicados. Un **SBC gateway** (Raspberry Pi, Orange Pi, etc.) se conecta por **red (Wi-Fi o Ethernet)** al P4 para proporcionar backhaul a Internet/LLM/MQTT.

> **Nota sobre el SBC:** El SBC se comunica con el P4 por TCP/IP, NO por USB. La conexión USB está reservada exclusivamente para los C3 radios. La opción de que el SBC hable directamente con los C3 por USB es un item de roadmap futuro (ver sección 13.1).

**Por qué USB CDC-ACM:**
- Un solo cable USB-C: alimentación + datos
- 12 Mbps por nodo (Full-Speed) → suficiente para ESP-NOW, LoRa, BLE
- Hot-plug con detección automática
- Ya existe infraestructura en el proyecto (`usb_cdc_loader_port.cpp`, `espnow_usb_bridge`)
- Escalable con USB Hub (4+ puertos)

---

## 2. Arquitectura de Hardware de la Célula

```text
┌─────────────────────────────────────────────────────────────────────┐
│                    SBC GATEWAY (Raspberry Pi / Orange Pi)            │
│  • Backhaul a Internet (Ethernet / Wi-Fi)                          │
│  • LLM local / Servidor MQTT / NAS                                  │
│  • Acceso SSH a CBDos P4                                            │
│                                                                     │
│  ⚠️  Conexión al P4: TCP/IP por red (NO USB)                       │
└───────────────────────────┬─────────────────────────────────────────┘
                            │ Ethernet / Wi-Fi (TCP/IP)
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    ESP32-P4 (CBDos Router / Hub)                     │
│  • CPU RISC-V Dual-Core @ 400 MHz                                   │
│  • 32 MB Hexal-PSRAM @ 200 MHz                                      │
│  • USB 2.0 OTG High-Speed (480 Mbps)                                │
│  • Display 4.3" IPS 480x800 (UI LVGL 9.5)                          │
│  • MicroSD (firmware C3, logs, configuración)                       │
│                                                                     │
│  Roles:                                                             │
│  1. Dispatcher QoS (routeo multi-radio)                             │
│  2. Monitor gráfico en pantalla                                     │
│  3. Flasher autónomo de C3 por USB CDC                              │
│  4. Terminal serie interactivo por USB CDC                           │
└───────────────────────────┬─────────────────────────────────────────┘
                            │ USB 2.0 High-Speed (480 Mbps)
                            │ Par diferencial D+/D-
                            ▼
               ┌────────────────────────┐
               │    USB 2.0 HUB         │
               │  (Powered Hub 5V/2A)  │
               └──┬──────┬──────┬──────┘
                  │      │      │
        ┌─────────┘      │      └─────────┐
        ▼                ▼                ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│  ESP32-C3 #1  │ │  ESP32-C3 #2  │ │  ESP32-C3 #3  │
│  + LoRa SX1262│ │  + ESP-NOW    │ │  + BLE/Sniffer│
│  868/915 MHz  │ │  2.4 GHz      │ │  2.4 GHz      │
├───────────────┤ ├───────────────┤ ├───────────────┤
│  USB-Serial/  │ │  USB-Serial/  │ │  USB-Serial/  │
│  JTAG nativo  │ │  JTAG nativo  │ │  JTAG nativo  │
│  (CDC-ACM)    │ │  (CDC-ACM)    │ │  (CDC-ACM)    │
└───────────────┘ └───────────────┘ └───────────────┘
  Puerto USB 1     Puerto USB 2     Puerto USB 3
  /dev/ttyACM0     /dev/ttyACM1     /dev/ttyACM2
```

### 2.1. Hardware Requerido

| Componente | Cantidad | Notas |
|:---|:---:|:---|
| ESP32-P4 JC4880P443C | 1 | Ya disponible |
| ESP32-C3 SuperMini / XIAO C3 | 3 | USB-Serial/JTAG nativo, sin USB externo |
| USB 2.0 Powered Hub (4+ puertos) | 1 | Alimentado externamente (5V/2A mínimo) |
| Cable USB-C a USB-C | 1 | P4 ↔ Hub |
| Cable USB-C a USB-A (o C a C) | 3 | Hub ↔ C3 |
| Módulo LoRa SX1262 (SPI) | 1 | Opcional, para C3 #1 |
| SBC Gateway (RPi/Opi) | 1 | Backhaul a Internet |

### 2.2. Velocidades del Bus USB

```text
ESP32-P4 ──USB 2.0 HS──▶ Hub ──USB 2.0 FS──▶ C3 #1    → 12 Mbps (~1 MB/s)
                         Hub ──USB 2.0 FS──▶ C3 #2    → 12 Mbps (~1 MB/s)
                         Hub ──USB 2.0 FS──▶ C3 #3    → 12 Mbps (~1 MB/s)
                         ─────────────────────────────
                         Total aggregate: 36 Mbps (4.5 MB/s)
                         P4 bandwidth: 480 Mbps (sobra)
```

**Throughput por nodo:** ~8 Mbps sostenido (1 MB/s) → **3.000-5.000 paquetes de 250 bytes/seg** por radio, superando el ancho de banda del medio aéreo de cada radio.

---

## 3. Protocolo de Comunicación P4 ↔ C3

### 3.1. Trama Binaria Existente (reutilizar `packet_framing.h`)

El firmware `tools/espnow_usb_bridge/src/main.cpp` ya implementa un framing binario eficiente:

```text
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┐
│ Magic 0  │ Magic 1  │ Dir (1B) │ Len (2B) │ Payload   │ CRC8 (1B)│
│ 0xAA     │ 0x55     │          │ (N)      │ (N bytes) │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┘
```

**Direcciones (Dir byte):**
- `0x01` PC→Dongle (ahora P4→C3): paquete para transmitir por radio
- `0x02` Dongle→PC (ahora C3→P4): paquete recibido del aire
- `0x03` Ctrl Command: comandos de control de radio
- `0x04` Ctrl Response: respuestas/ACK de radio

**Comandos de control existentes:**
- `0x01` GET_STATUS
- `0x02` SET_MODE (Normal/LR)
- `0x03` SET_CHANNEL
- `0x04` SET_POWER

### 3.2. Extensión del Protocolo para Multi-Radio

Añadir al byte Dir un campo de **Radio ID** para identificar a qué módulo C3 se dirige:

```text
Dir byte extendido:
  Bits [7:4] - Tipo de mensaje (ya existente: 0x01-0x04)
  Bits [3:0] - Radio ID / Slot (0=auto, 1=C3#1, 2=C3#2, 3=C3#3)
```

O alternativamente, mantener el byte Dir como está y usar un **wrapper de multiplexación** en el payload:

```text
Wrapper Byte 0: [RadioID:4][Reserved:4]
```

### 3.3. Handshake de Descubrimiento

Cuando un C3 se conecta al Hub, el P4 debe:
1. Detectar el evento USB de nuevo dispositivo
2. Abrir el canal CDC-ACM al VID:PID `0x303A:0x1001`
3. Enviar comando `GET_STATUS` (`0x03 0x01`)
4. El C3 responde con su MAC, modo de radio activo, canal, etc.
5. El P4 registra el C3 en la tabla de interfaces

---

## 4. Cambios Requeridos en Firmware

### 4.1. Lado ESP32-P4 (Host)

#### 4.1.1. Nuevo archivo: `hal/usb_cdc_radio_host.hpp`

```cpp
// Interfaz pública para el host USB CDC multi-radio
namespace cbdos {
namespace bsp {

struct UsbCdcRadioNode {
    uint8_t slot;               // 0-2 (índice del puerto Hub)
    cdc_acm_dev_hdl_t handle;  // Handle del dispositivo CDC-ACM
    uint8_t mac[6];            // MAC del C3 ( tras handshake)
    char name[32];             // Nombre del módem
    uint8_t radioType;         // Tipo de radio (LoRa, ESP-NOW, BLE)
    uint8_t channel;           // Canal RF activo
    bool connected;            // Estado de conexión
    QueueHandle_t rxQueue;     // Cola RX (RingBuffer en PSRAM)
    QueueHandle_t txQueue;     // Cola TX (RingBuffer en PSRAM)
};

bool usbCdcRadioHostInit();
bool usbCdcRadioHostStart();
size_t usbCdcRadioHostGetNodeCount();
UsbCdcRadioNode* usbCdcRadioHostGetNode(uint8_t slot);
bool usbCdcRadioHostSend(uint8_t slot, const uint8_t* data, size_t len);
bool usbCdcRadioHostReceive(uint8_t slot, uint8_t* buffer, size_t maxLen, size_t* outLen, uint32_t timeoutMs);

} // namespace bsp
} // namespace cbdos
```

#### 4.1.2. Nuevo archivo: `hal/usb_cdc_radio_host.cpp`

**Estructura general:**

```
usbCdcRadioHostInit()
  ├── usb_host_install() (una vez)
  ├── cdc_acm_host_install() (una vez)
  ├── Crear tarea usb_host_lib_task (Core 0, prio 5)
  └── Registrar new_dev_cb para detección automática

usbCdcRadioHostStart()
  ├── Iniciar escaneo de dispositivos
  └── Por cada dispositivo detectado:
      ├── cdc_acm_host_open(VID, PID, ...)
      ├── Crear UsbCdcRadioNode
      ├── Asignar rxQueue (RingBuffer PSRAM 8KB)
      ├── Asignar txQueue (RingBuffer PSRAM 8KB)
      ├── Enviar GET_STATUS handshake
      ├── Registrar en NetworkInterfaceManager (slot 2)
      └── Log: "C3 Radio Node conectado en puerto X"
```

**Task de recepción por nodo:**

```
usb_cdc_rx_task(void* arg)   // Un task por C3 conectado
  while (node->connected):
    ├── cdc_acm_host_data_rx_blocking(handle, buf, 512, timeout=100ms)
    ├── FrameParser_feed() byte a byte
    ├── Si frame completa:
    │   ├── Verificar CRC8
    │   ├── Extraer payload (paquete de radio)
    │   ├── Encolar en rxQueue
    │   └── Notificar a MultiRadioRouter
    └── Si timeout: seguir leyendo
```

#### 4.1.3. Integración con `usb_cdc_loader_port.cpp` existente

El archivo actual solo sirve para flashear. Necesita extenderse con un **modo dual**:

```cpp
enum class UsbCdcMode {
    LOADER,     // Modo actual: flasher bloqueante
    TERMINAL    // Modo nuevo: bidireccional continuo
};

// Nuevas funciones:
esp_loader_error_t loader_port_usb_cdc_open_terminal(uint32_t timeout_ms);
size_t loader_port_usb_cdc_terminal_read(uint8_t* buf, size_t maxLen, uint32_t timeoutMs);
size_t loader_port_usb_cdc_terminal_write(const uint8_t* buf, size_t len);
```

#### 4.1.4. Cambios en `main.cpp`

Añadir inicialización del host USB CDC radio:

```cpp
// Después de initHidDriverP4():
cbdos::bsp::initUsbCdcRadioHostP4();  // Nuevo
```

### 4.2. Lado ESP32-C3 (Modem)

#### 4.2.1. Firmware base: `tools/espnow_usb_bridge/src/main.cpp`

El firmware ya existe y funciona. Cambios necesarios:

1. **Auto-identificación al arrancar** — Responder a GET_STATUS con MAC, tipo de radio, canal
2. **Soporte multi-radio** — Añadir comandos para:
   - `SET_RADIO_TYPE`: seleccionar modo LoRa/ESP-NOW/BLE
   - `SET_FREQUENCY`: configurar frecuencia/canal específico
   - `GET_RADIO_CAPABILITIES`: reportar qué radios tiene conectados (SPI)
3. **Firmware compilable como ESP-IDF puro** (no solo Arduino) para máximo control

#### 4.2.2. Nuevo firmware: `tools/esp_c3_radio_modem/`

```text
esp_c3_radio_modem/
├── CMakeLists.txt
├── sdkconfig.defaults.esp32c3
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                    # Entry point
│   ├── usb_serial_jtag_driver.c  # USB-Serial/JTAG driver
│   ├── radio_manager.c           # Gestión de radios SPI (SX1262, etc.)
│   ├── packet_framing.c          # Reutilizar de espnow_usb_bridge
│   ├── cli_commands.c            # CLI interactivo
│   └──Kconfig.projbuild          # Configuración de radios
└── managed_components/
```

**Modos de operación del C3 modem:**

| Modo | Radio | Frecuencia | Protocolo | Throughput |
|:---|:---|:---|:---|:---|
| `ESP-NOW` | Wi-Fi integrada | 2.4 GHz | ESP-NOW / LR | ~250 kbps - 1 Mbps |
| `LoRa Sub-GHz` | SX1262 (SPI) | 868/915 MHz | LoRa CSS | 0.5-22 kbps |
| `LoRa 2.4` | SX1280 (SPI) | 2.4 GHz | LoRa 2.4 / FLRC | 1-2 Mbps |
| `BLE` | BLE integrada | 2.4 GHz | BLE 5.0 GATT | ~2 Mbps |
| `Sniffer` | Wi-Fi promiscuo | 2.4 GHz | Raw 802.11 | Captura pasiva |

---

## 5. Integración con la Arquitectura Existente

### 5.1. `NetworkInterfaceManager` (slot 2)

El `core/include/cbdos/network_interface.hpp` ya define:

```cpp
enum class InterfaceType {
    SerialModem    // USB-CDC / UART TNC  ← ¡Exactamente esto!
};

enum class InterfaceMode {
    UsbCdcTnc      // ← ¡Y esto!
};
```

**Implementación necesaria:**

```cpp
class UsbCdcRadioInterface : public network::INetworkInterface {
public:
    const char* getName() const override { return "USB-CDC Radio"; }
    InterfaceType getType() const override { return InterfaceType::SerialModem; }
    InterfaceMode getMode() const override { return InterfaceMode::UsbCdcTnc; }
    
    bool isReady() const override;
    int sendPacket(const uint8_t* buffer, size_t len) override;
    void setPacketRecvCallback(PacketRecvCallback cb, void* userCtx) override;
    
private:
    uint8_t m_activeSlot = 0;  // Puerto USB activo
    PacketRecvCallback m_callback = nullptr;
    void* m_userCtx = nullptr;
};
```

Registrar en `main.cpp`:

```cpp
static UsbCdcRadioInterface s_usbCdcRadio;
// ...
network::NetworkInterfaceManager::getInstance().registerInterface(2, &s_usbCdcRadio);
```

### 5.2. `MeshCoreTypes` (slot 2 ya definido)

```cpp
// core/src/apps/meshcore/MeshCoreTypes.hpp
enum class MeshInterfaceId : uint8_t {
    Interface1_Internal = 0, // Radio Interna (ESP-NOW)
    Interface2_Backpack = 1, // Mochila LoRa
    Interface3_USB      = 2, // ← USB-CDC / Modem Serial TNC  ✅ YA EXISTE
};
```

El slot 2 ya está reservado para USB-CDC. Solo hay que crear la implementación.

### 5.3. `SerialTerminalView` (UI LVGL)

Agregar en el selector de puertos:

```cpp
// En SerialTerminalView.cpp, al construir la lista de dispositivos:
auto usbDevices = cbdos::bsp::usbCdcRadioHostGetAvailableDevices();
for (auto& dev : usbDevices) {
    addItem(dev.displayName, PortType::USB_CDC_ACM, dev.slot);
}
```

### 5.4. `FlasherView` (flasheo en caliente)

El flasher ya soporta USB CDC (`FlasherTransport::USB_CDC_NATIVE`). Se puede reutilizar para flashear los C3 con el firmware de modem actualizado directamente desde la MicroSD del P4:

```cpp
// Nuevo preset:
FlasherPreset p_usb_modem;
p_usb_modem.id = "p4_usb_c3_modem";
p_usb_modem.name = "C3 Radio Modem (USB-CDC)";
p_usb_modem.description = "Flasheo de C3 como modem de radio via USB CDC";
p_usb_modem.transport = FlasherTransport::USB_CDC_NATIVE;
p_usb_modem.config.binPath = "/sdcard/firmware/c3_radio_modem.bin";
```

---

## 6. Diagrama de Tareas FreeRTOS (ESP32-P4)

```text
Core 0:
├── usb_host_lib_task          (prio 5)  ← Ya existe en usb_cdc_loader_port.cpp
├── usb_cdc_rx_task_c3_0      (prio 4)  ← Nuevo: lee frames de C3 #1
├── usb_cdc_rx_task_c3_1      (prio 4)  ← Nuevo: lee frames de C3 #2
├── usb_cdc_rx_task_c3_2      (prio 4)  ← Nuevo: lee frames de C3 #3
├── radio_dispatch_task        (prio 3)  ← Nuevo: QoS routing entre interfaces
└── auto_wifi_task             (prio 1)  ← Ya existe

Core 1:
├── LVGL_Port task             (prio 5)  ← Ya existe (render display)
├── usb_serial_jtag_cli        (prio 2)  ← Ya existe (debug CLI)
└── main_app                   (prio 1)  ← Ya existe
```

**Memoria PSRAM requerida por nodo C3:**
- RingBuffer RX: 8 KB
- RingBuffer TX: 8 KB
- FrameParser: 260 bytes
- UsbCdcRadioNode struct: ~256 bytes
- **Total por nodo: ~17 KB** → 3 nodos = **~51 KB** (trivial en 32 MB PSRAM)

---

## 7. Análisis de Rendimiento

### 7.1. Latencia P4 → C3 → Radio → Aire

| Paso | Latencia estimada |
|:---|:---|
| P4 encola paquete en TX queue | < 1 ms |
| USB CDC TX (Full-Speed bulk) | ~0.5 ms (512 bytes @ 12 Mbps) |
| C3 procesa frame + framing radio | ~2 ms |
| Transmisión por radio (ESP-NOW) | ~1 ms |
| **Total estimado** | **~4-5 ms** |

Comparado con:
- UART a 115200 baud: ~4.4 ms solo por 512 bytes → **~10x más lento**
- UART a 921600 baud: ~0.5 ms → similar a USB CDC pero sin hot-plug

### 7.2. Capacidad del Sistema Completo

| Métrica | Valor |
|:---|:---|
| Nodos simultáneos | 3 (expandible a 4-7 con Hub mayor) |
| Throughput aggregate | 3 × 1 MB/s = **3 MB/s** |
| Paquetes/seg por nodo | ~3.500 paquetes de 250 bytes |
| Total paquetes/seg (sistema) | **~10.500** |
| Memoria PSRAM usada (buffers) | ~51 KB / 32 MB = **0.16%** |
| CPU P4 usada (USB Host + dispatch) | ~5-8% estimado |

---

## 8. Casos de Uso de la Célula

### 8.1. Torre de Comunicaciones Descentralizada

```text
┌─────────────────────────────────────────────────┐
│  TORRE CBDos (Campamento / Evento / Emergencia)  │
│                                                   │
│  ESP32-P4 + Display 4.3"  ── UI de monitor       │
│  + USB Hub                                     │
│  ├── C3 #1 + SX1262  → LoRa 915 MHz (alcance 10-30 km)
│  ├── C3 #2 (ESP-NOW) → Red local mesh 2.4 GHz
│  └── C3 #3 (Sniffer) → Monitoreo de espectro
│                                                   │
│  SBC Gateway (RPi) ──→ Internet / MQTT / LLM     │
└─────────────────────────────────────────────────┘
```

### 8.2. Flujo de Datos

```
[Sensor remoto] ──LoRa 915MHz──▶ [C3 #1] ──USB CDC──▶ [P4 Router]
                                                              │
                              ┌───────────────────────────────┘
                              ▼
                    [MultiRadioRouter dispatch]
                     ├── Reenviar a C3 #2 (ESP-NOW mesh)
                     ├── Almacenar en MicroSD (log)
                     ├── Enviar a SBC Gateway (TCP/IP)
                     └── Mostrar en pantalla (LVGL)
```

### 8.3. Flashing Remoto de Modems

El P4 puede flashear cualquier C3 conectado al Hub sin necesidad de cables adicionales:

```
[MicroSD] → firmware_c3_lora.bin → [USB CDC Host] → [C3 en modo bootloader]
```

El usuario selecciona el firmware desde la UI FlasherView y el P4 ejecuta el auto-reset DTR/RTS por USB CDC (ya implementado en `usb_cdc_loader_port.cpp:200-224`).

---

## 9. Plan de Implementación por Fases

### Fase 1: Host USB CDC Bidireccional (P4)
**Archivos:** `hal/usb_cdc_radio_host.hpp`, `hal/usb_cdc_radio_host.cpp`
**Dependencias:** `espressif/usb_host_cdc_acm` (ya enlazado)
**Esfuerzo estimado:** 3-4 días

| Tarea | Prioridad |
|:---|:---|
| Extender `usb_cdc_loader_port.cpp` con modo terminal bidireccional | Alta |
| Implementar RingBuffer PSRAM para RX/TX continuo | Alta |
| Implementar detección hot-plug (new_dev_cb) | Alta |
| Añadir handshake GET_STATUS al conectar | Media |
| Multi-nodo:管理 3 dispositivos CDC simultáneos | Alta |
| Integrar en `main.cpp` (init) | Baja |

### Fase 2: Firmware C3 Radio Modem
**Archivos:** `tools/esp_c3_radio_modem/` (nuevo proyecto ESP-IDF)
**Dependencias:** Ninguna nueva
**Esfuerzo estimado:** 2-3 días

| Tarea | Prioridad |
|:---|:---|
| Crear proyecto ESP-IDF base para C3 | Alta |
| Implementar USB-Serial/JTAG driver (o reutilizar Arduino Serial) | Alta |
| Portar `packet_framing.h` desde `espnow_usb_bridge` | Alta |
| Añadir handshake de auto-identificación | Alta |
| Soporte multi-radio (ESP-NOW + LoRa SPI) | Media |
| CLI interactivo por USB CDC | Baja |

### Fase 3: Integración con Core (NetworkInterface + MeshCore)
**Archivos:** `core/include/cbdos/network_interface.hpp` (ya existe), nueva implementación BSP
**Dependencias:** Fase 1 completada
**Esfuerzo estimado:** 2-3 días

| Tarea | Prioridad |
|:---|:---|
| Implementar `UsbCdcRadioInterface` (INetworkInterface) | Alta |
| Registrar en slot 2 del `NetworkInterfaceManager` | Alta |
| Integrar con `MeshCoreEngine` (Interface3_USB) | Media |
| Integrar con `SerialTerminalView` (selector de puertos) | Media |
| Integrar con `FlasherView` (preset USB-CDC para C3) | Baja |

### Fase 4: Multi-Radio Router (Dispatcher QoS)
**Archivos:** Nuevo `hal/usb_cdc_router.cpp` o extensión de `hal_mesh_p4.cpp`
**Dependencias:** Fases 1-3 completadas
**Esfuerzo estimado:** 3-5 días

| Tarea | Prioridad |
|:---|:---|
| Implementar `MultiRadioRouter` con tablas de rutas | Alta |
| Byte de control QoS (prioridad, tipo, destino) | Alta |
| Bridge LoRa ↔ ESP-NOW ↔ USB-CDC | Media |
| Monitoreo de RSSI/SNR por interfaz | Media |
| UI de monitor multi-radio en LVGL | Baja |

### Fase 5: SBC Gateway y Backhaul
**Archivos:** Scripts en `tools/` o nuevo directorio
**Dependencias:** Red TCP/IP funcionando (WiFi o Ethernet)
**Esfuerzo estimado:** 2-3 días

El SBC se conecta al P4 por **red (Wi-Fi/Ethernet)**, NO por USB. El P4 actúa como bridge TCP↔radio.

| Tarea | Prioridad |
|:---|:---|
| Servidor MQTT en SBC (broker local) | Alta |
| Bridge TCP ↔ USB-CDC (SBC ↔ P4 por red) | Media |
| Dashboard web de monitoreo en SBC | Baja |
| Integración con LLM local (Ollama) | Baja |

---

## 13. Roadmap Futuro (Post-Fase 5)

> **Nota:** Los items de esta sección están **fuera del alcance** del plan actual. Se documentan aquí como referencia para desarrollo futuro.

### 13.1. SBC Gateway Directo a C3s (USB Hub Dual-Host)

Arquitectura futura donde el SBC reemplaza al P4 como host USB principal, o comparte un Hub dual con el P4:

```text
                    ┌──────────────────┐
                    │  USB HUB Dual    │
                    │  (USB OTG Mux)   │
                    └──┬────┬────┬────┘
                       │    │    │
              ┌────────┘    │    └────────┐
              ▼             ▼             ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ C3 #1    │ │ C3 #2    │ │ C3 #3    │
        └──────────┘ └──────────┘ └──────────┘

  Host A: ESP32-P4 (UI, routing, display)
  Host B: SBC Gateway (backhaul, LLM, MQTT)
```

**Ventajas:** El SBC puede acceder directamente a los C3 sin pasar por el P4, reduciendo latencia para tareas de backhaul.
**Complejidad:** Requiere un Hub con OTG multiplexing o un Hub que soporte dos hosts (hardware especializado).

### 13.2. USB 3.0 / High-Speed Hub

Si se necesita más throughput (por ejemplo con radios de alta velocidad como SX1280 FLRC a 2 Mbps), se puede migrar a un Hub USB 3.0 que soporte SuperSpeed (5 Gbps), aunque el ESP32-P4 está limitado a USB 2.0 HS (480 Mbps).

### 13.3. Ethernet sobre USB (CDC-NCM / RNDIS)

Los C3 podrían exponer una interfaz de red Ethernet por USB CDC-NCM en lugar de serial raw, permitiendo que el P4 o el SBC vean cada C3 como un dispositivo de red IP directamente. Esto simplifica el software pero añade overhead de TCP/IP sobre USB.

---

## 10. Dependencias ESP-IDF / Componentes

| Componente | Versión | Estado | Uso |
|:---|:---|:---|:---|
| `espressif/usb_host_cdc_acm` | ^2.4.1 | ✅ Ya enlazado | Host CDC driver |
| `espressif/esp_tinyusb` | ^1.7.0 | ✅ Ya enlazado | USB Device (HID) |
| `espressif/usb` | (core) | ✅ Ya enlazado | USB Host stack |
| `espressif/esp-serial-flasher` | ^1.3.0 | ✅ Ya enlazado | Flasher protocol |
| `espressif/usb_host_ch34x_vcp` | ^2.2.1 | ✅ Ya enlazado | CH34x support |
| `espressif/usb_host_cp210x_vcp` | ^2.2.0 | ✅ Ya enlazado | CP210x support |

**No se necesitan componentes adicionales.** Todo ya está enlazado.

---

## 11. Riesgos y Mitigaciones

| Riesgo | Probabilidad | Impacto | Mitigación |
|:---|:---:|:---:|:---|
| USB-Serial/JTAG del C3 no soporta alta velocidad | Baja | Medio | ya funciona a 115200; probar a 2 Mbps |
| Colisión de endpoints USB con TinyUSB HID | Baja | Baja | TinyUSB es Device, USB Host es stack separado |
| Timeout de detección hot-plug | Media | Baja | Implementar retry con exponential backoff |
| Memoria insuficiente en C3 para firmware completo | Baja | Media | C3 tiene 400 KB SRAM; suficiente para USB+radio+framing |
| USB Hub con TT introduce latencia | Baja | Baja | TT es transparente para bulk transfers |

---

## 12. Criterios de Aceptación

- [ ] P4 detecta automáticamente 3 C3 conectados al Hub USB
- [ ] Cada C3 responde a handshake GET_STATUS con MAC y tipo de radio
- [ ] Paquetes ESP-NOW se transmiten/received por USB CDC con < 10 ms latencia
- [ ] Se puede flashear un C3 desde la UI del P4 sin cables adicionales
- [ ] Terminal serie interactivo funciona para cada C3 individualmente
- [ ] MultiRadioRouter distribuye paquetes entre interfaces
- [ ] SBC Gateway recibe y reenvía tráfico mesh a Internet
- [ ] Sistema es estable por > 24 horas sin reinicios
