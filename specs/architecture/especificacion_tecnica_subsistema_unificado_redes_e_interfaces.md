# 📡 Especificación Técnica: Subsistema Unificado de Interfaces de Red, Coexistencia Multi-Radio y Capa de Ruteo (CBDos v0.2.1)

**Documento:** `docs/architecture/especificacion_tecnica_subsistema_unificado_redes_e_interfaces.md`  
**Versión:** 1.0.0 (RFC-CBDOS-NET-02)  
**Estado:** 💡 Propuesta de Arquitectura Formal para Evaluación y Aprobación  
**Autor:** Equipo de Arquitectura de Software CBDos & Usuario  
**Fecha:** Agosto 2026  

---

## 🏛️ 1. Resumen Ejecutivo y Justificación Arquitectónica

En las versiones iniciales de CBDos, el subsistema de comunicaciones presentaba ambigüedades conceptuales y técnicas:
1. **Confusión Terminológica (*Falso Mesh vs Enlace Físico*):** Se utilizaba el término `IMeshTransport` para referirse a la capa física de transmisión de paquetes por radio, cuando en realidad una radio no es una red en malla, sino un simple enlace de datos (*Data Link / PHY*).
2. **Falta de Abstracción Multi-Radio:** El hardware del Cyberdeck cuenta con capacidades heterogéneas avanzadas:
   * **Coprocesador ESP32-C6 / S3:** Transceptor de 2.4 GHz capaz de operar en **Wi-Fi 6 (802.11ax)**, **Bluetooth 5.0 (BLE)**, **802.15.4 (Zigbee 3.0 / Thread)** y **ESP-NOW**.
   * **Mochilas de Hardware (Backpacks NFC):** Transceptores LoRa de largo alcance (**Semtech SX1262** a 915/868 MHz) y alta velocidad (**SX1280 FLRC** a 2.4 GHz).
   * **Puertos USB / UART:** Conexión de módems de radio externos (**ESP32-C3**, T-Beam, dongles TNC/SLIP).
3. **Acoplamiento entre Ruteo e Interfaces:** La lógica de enrutamiento multi-salto debe vivir en la **Capa de Red (L3)**, desacoplada de las **Interfaces Físicas (L1/L2)** sobre las cuales se transportan los paquetes.

El objetivo de esta especificación es definir una **arquitectura de redes unificada, modular y formal** basada en el modelo estándar de capas (OSI/POSIX), soportando **conmutación dinámica de modos de radio**, **coexistencia RF** y **ruteo multi-interfaz** con **MeshCore**.

---

## 🌐 2. Modelo de Capas y Jerarquía de Red

El subsistema se divide en 3 capas estrictamente desacopladas:

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                       CAPA 3: APLICACIONES (UI / LVGL 9.5)                  │
│   • MeshChatView (Chat)   • Web Browser (HTTP)   • IoT SmartHome (Zigbee)   │
│   • Packet Sniffer (RF)   • Serial Terminal      • Weather Dashboard        │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (Consume servicios y sockets)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                  CAPA 2: RUTEO Y PROTOCOLOS DE RED (L3 / L4)                 │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        MESHCORE ROUTING ENGINE                      │   │
│   │  • Tablas de Nodos y Rutas       • Control de Saltos (TTL / Hops)   │   │
│   │  • Flooding & Direct Routing     • Bridge Multi-Radio (LoRa <-> BLE)│   │
│   └──────────────────────────────────┬──────────────────────────────────┘   │
│                                      │                                      │
│   ┌──────────────────────────────────┴──────────────────────────────────┐   │
│   │                  PILA TCP/IP & SOCKETS (LwIP)                       │   │
│   │  • HTTP Client / WebSockets      • MQTT Broker Client (JSON/Proto)  │   │
│   │  • DHCP / DNS Resolver           • BSD Sockets (Stream / Datagram)  │   │
│   └──────────────────────────────────┬──────────────────────────────────┘   │
│                                      │                                      │
│   ┌──────────────────────────────────┴──────────────────────────────────┐   │
│   │             ALTERNET CLIENT (Protocolo de Torres PTMP)              │   │
│   │  • Descubrimiento de Torres      • Peticiones TLVGL por el Aire     │   │
│   └──────────────────────────────────┬──────────────────────────────────┘   │
└──────────────────────────────────────┼──────────────────────────────────────┘
                                       │ (Envía/Recibe tramas crudas o IP)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│          CAPA 1: GESTOR DE INTERFACES DE RED (NetworkInterfaceManager)      │
│                                                                             │
│   ┌───────────────────┐ ┌───────────────────┐ ┌─────────────────────────┐  │
│   │    INTERFAZ 1     │ │    INTERFAZ 2     │ │       INTERFAZ 3        │  │
│   │ (Radio Integrada) │ │ (Mochila LoRa NFC)│ │   (Módem USB / Serial)  │  │
│   ├───────────────────┤ ├───────────────────┤ ├─────────────────────────┤  │
│   │ • ESP-NOW (2.4G)  │ │ • SX1262 LoRa 915 │ │ • USB-CDC Virtual COM   │  │
│   │ • Wi-Fi 6 STA/AP  │ │ • SX1280 FLRC 2.4 │ │ • UART Hardware (JP1)   │  │
│   │ • BLE 5.0 GATT    │ │ • CC1101 Sub-GHz  │ │ • Modo KISS TNC / SLIP  │  │
│   │ • 802.15.4 Zigbee │ │                   │ │                         │  │
│   └───────────────────┘ └───────────────────┘ └─────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 📻 3. Matriz de Interfaces Físicas y Transceptores

### 3.1. Ranura 1: Interfaz Integrada Multi-Modo (ESP32-C6 / ESP32-S3)
Opera en la banda de 2.4 GHz compartiendo la antena de radiofrecuencia. Cuenta con una **máquina de estados de modos operativos**:

| Modo Operativo (`RadioMode`) | Protocolo Subyacente | Pila de Red Asociada | Casos de Uso en CBDos |
| :--- | :--- | :--- | :--- |
| **`MODE_OFF`** | Ninguno (Radio apagada) | Ninguna (0 RAM, 0 CPU) | Modo Avión / Ahorro extremo de batería (**Offline-First**). |
| **`MODE_ESPNOW`** | ESP-NOW (Canales 1..13 @ 1Mbps)| `MeshCore` / `Alternet` | Malla local descentralizada de alta velocidad sin router. |
| **`MODE_WIFI_STA`** | 802.11 b/g/n/ax (Station) | TCP/IP (LwIP / Sockets)| Navegación Web, cliente MQTT, descargas OTA de firmware. |
| **`MODE_WIFI_AP`** | 802.11 SoftAP | TCP/IP (Servidor Web)  | Configuración local, portal cautivo de rescate. |
| **`MODE_BLE_GATT`** | Bluetooth Low Energy 5.0 | NimBLE / HCI Stack     | Conexión con teclado CardKB BLE, mandos o bridge móvil. |
| **`MODE_802154_ZIGBEE`**| IEEE 802.15.4 (Zigbee 3.0)| Zboss / Zigbee Stack   | Red de domótica, interruptores, bombillas y sensores IoT. |
| **`MODE_802154_THREAD`**| IEEE 802.15.4 (OpenThread)| 6LoWPAN / IPv6 Mesh    | Malla IPv6 industrial y domótica de bajo consumo. |
| **`MODE_COEX_WIFI_BLE`**| Coexistencia TDM (Wi-Fi + BLE)| LwIP + NimBLE          | Navegar por internet mientras se usa un teclado Bluetooth. |

### 3.2. Ranura 2: Interfaz Modular Externa (Mochilas Hot-Plug NFC)
Transceptores físicos acoplados al header de expansión JP1 con auto-configuración de pines por NFC:

* **Semtech SX1262:** Banda Sub-GHz (915 MHz América / 868 MHz Europa). Modulación LoRa para enlaces de más de **15 km**.
* **Semtech SX1280:** Banda 2.4 GHz. Modulación FLRC a **1.3 Mbps** para transferencia rápida de archivos o LoRa 2.4 GHz de largo alcance.
* **TI CC1101:** Analizador y transceptor Sub-GHz (315/433/868 MHz) para radios y sensores ISM.

### 3.3. Ranura 3: Interfaz USB-CDC / Módem Serial TNC
* **Dongle USB (ESP32-C3 / Seeed XIAO / Heltec):** Conectado al puerto USB Host de CBDos actuando como radio módem TNC o puente SLIP.
* **Cyberdeck como Módem:** El Cyberdeck conectado a un ordenador transmite tramas de malla hacia herramientas de escritorio (`meshcore-cli`, scripts en Python).

---

## 🧠 4. Capa de Ruteo Multi-Interfaz (MeshCore Routing Engine)

La capa de ruteo opera de forma agnóstica por encima de las interfaces registradas.

```text
                               ┌────────────────────────────────┐
                               │   MeshCore Multi-Radio Router  │
                               └───────────────┬────────────────┘
                                               │
               ┌───────────────────────────────┼───────────────────────────────┐
               ▼                               ▼                               ▼
       ┌───────────────┐               ┌───────────────┐               ┌───────────────┐
       │   IFACE 1     │               │   IFACE 2     │               │   IFACE 3     │
       │ (ESP-NOW 2.4G)│               │ (LoRa 915 MHz)│               │  (USB Módem)  │
       └───────────────┘               └───────────────┘               └───────────────┘
```

### Funciones del Ruteador:
1. **Conmutación Inteligente de Enlace (*Multipath & Interface Selection*):**
   * Paquetes locales hacia nodos cercanos (< 100 m): Se transmiten preferentemente por **ESP-NOW (Iface 1)** por su velocidad (1 Mbps, latencia < 2 ms).
   * Paquetes de larga distancia o sin respuesta directa: Se enrutan por **LoRa (Iface 2)** (Time-on-Air ~60 ms, rango > 10 km).
2. **Puente Transparente (*Bridging & Packet Forwarding*):**
   * Un paquete recibido por la radio LoRa externa puede ser retransmitido automáticamente por ESP-NOW hacia otros Cyberdecks locales y volcado por el puerto USB-CDC a un terminal conectado.
3. **Control de Bucle y Duplicados (*Deduplication Cache*):**
   * Cada trama contiene un `MsgID` (2 Bytes) y `SourceID` (2 Bytes). Una tabla circular en PSRAM evita el reenvío de paquetes ya procesados en la malla.

---

## 📜 5. Contratos de Software en C++ Puro (`core/include/cbdos/network/`)

### 5.1. Contrato Abstracto de Interfaz Física (`INetworkInterface.hpp`)

```cpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

namespace cbdos {
namespace net {

enum class InterfaceType : uint8_t {
    Disabled = 0,
    RadioPacket,   // ESP-NOW, LoRa SX1262, FLRC SX1280 (Tramas crudas)
    IpNetwork,     // Wi-Fi 6 TCP/IP (LwIP Sockets)
    BluetoothLe,   // BLE 5.0 (NimBLE GATT / Beacons)
    Zigbee802154,  // Zigbee 3.0 / Thread (802.15.4)
    SerialModem    // USB-CDC / UART TNC
};

enum class InterfaceMode : uint8_t {
    Off = 0,
    EspNow,
    WifiStation,
    WifiAccessPoint,
    BleGattServer,
    BleGattClient,
    ZigbeeCoordinator,
    ZigbeeRouter,
    LoRa_915MHz,
    LoRa_868MHz,
    FLRC_2400MHz,
    UsbCdcTnc
};

using PacketRecvCallback = std::function<void(const uint8_t* srcAddr, const uint8_t* payload, size_t len, int8_t rssi)>;

class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;

    virtual const char* getName() const = 0;
    virtual InterfaceType getType() const = 0;
    virtual InterfaceMode getMode() const = 0;
    virtual bool setMode(InterfaceMode mode) = 0;

    virtual bool isReady() const = 0;
    virtual bool sendPacket(const uint8_t* destAddr, const uint8_t* data, size_t len) = 0;
    virtual void setPacketRecvCallback(PacketRecvCallback cb) = 0;

    virtual uint8_t getChannel() const { return 1; }
    virtual bool setChannel(uint8_t channel) { return false; }
    virtual int8_t getTxPower() const { return 0; }
    virtual bool setTxPower(int8_t powerDbm) { return false; }
};

} // namespace net
} // namespace cbdos
```

### 5.2. Gestor de Interfaces de Red (`NetworkInterfaceManager.hpp`)

```cpp
#pragma once

#include "INetworkInterface.hpp"
#include <vector>
#include <memory>
#include <mutex>

namespace cbdos {
namespace net {

class NetworkInterfaceManager {
public:
    static NetworkInterfaceManager& getInstance();

    // Registro de ranuras de interfaz (Inyectadas por el BSP)
    void registerInterface(uint8_t slotId, std::shared_ptr<INetworkInterface> iface);
    std::shared_ptr<INetworkInterface> getInterface(uint8_t slotId);
    size_t getInterfaceCount() const;

    // Control global de conectividad (Offline-First)
    void setAllOffline();
    bool isAnyInterfaceActive() const;

private:
    NetworkInterfaceManager() = default;
    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<INetworkInterface>> m_interfaces;
};

} // namespace net
} // namespace cbdos
```

---

## 💾 6. Persistencia de Perfiles en NVS y MessagePack (`networks.msgpack`)

Para permitir la conmutación entre perfiles de red sin modificar código en Flash, las interfaces y redes se configuran en `networks.msgpack`:

```json
{
  "version": 1,
  "default_profile": "tactical_mesh",
  "profiles": [
    {
      "id": "tactical_mesh",
      "name": "Malla Táctica Híbrida",
      "interfaces": [
        { "slot": 0, "mode": "EspNow", "channel": 1, "enabled": true },
        { "slot": 1, "mode": "LoRa_915MHz", "freq": 915.0, "sf": 7, "bw": 250, "enabled": true },
        { "slot": 2, "mode": "UsbCdcTnc", "baud": 115200, "enabled": false }
      ],
      "router": { "engine": "meshcore", "auto_forward": true, "ttl_max": 7 }
    },
    {
      "id": "internet_home",
      "name": "Wi-Fi Hogar + BLE",
      "interfaces": [
        { "slot": 0, "mode": "WifiStation", "ssid": "CyberLab_5G", "psk": "...", "enabled": true },
        { "slot": 0, "mode": "BleGattClient", "target_device": "CardKB_BLE", "enabled": true }
      ]
    },
    {
      "id": "stealth_offline",
      "name": "Modo Sigilo (Offline Total)",
      "interfaces": [
        { "slot": 0, "mode": "Off" },
        { "slot": 1, "mode": "Off" },
        { "slot": 2, "mode": "Off" }
      ]
    }
  ]
}
```

---

## 📋 7. Hoja de Ruta de Implementación por Fases

| Fase | Título | Entregables Principales | Estado |
| :---: | :--- | :--- | :---: |
| **Fase 1** | **MeshCore sobre Radios de Paquetes** | Integración de `MeshCoreEngine` en `core/src/apps/meshcore/` y pruebas sobre ESP-NOW y SX1262. | ✅ **Completado** |
| **Fase 2** | **Refactorización de Contratos (`INetworkInterface`)** | Migración formal de `IMeshTransport` hacia `INetworkInterface` y creación de `NetworkInterfaceManager`. | ⏳ **Propuesto** |
| **Fase 3** | **Coexistencia RF en ESP32-C6** | Implementación de máquina de estados para conmutar Wi-Fi 6, BLE 5 y Zigbee en caliente. | ⏳ **Planificado** |
| **Fase 4** | **Soporte de Módem USB-CDC** | Integración del driver USB Host CDC para radio módems externos (ESP32-C3 / SLIP / KISS TNC). | ⏳ **Planificado** |
| **Fase 5** | **Gestor de Perfiles `networks.msgpack`** | Carga y guardado dinámico de perfiles de red desde la MicroSD y la UI. | ⏳ **Planificado** |
