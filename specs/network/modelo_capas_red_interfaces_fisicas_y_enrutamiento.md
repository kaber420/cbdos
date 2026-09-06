# Modelo de Capas de Red, Interfaces Físicas y Enrutamiento en CBDos

---

## 📌 1. Visión y Pureza Arquitectónica

Este documento define la arquitectura formal del subsistema de comunicaciones y redes de **CBDos**, estableciendo una separación estricta y ortodoxa basada en el modelo por capas (L1 a L7).

### ⚠️ Principio Fundamental de Desacoplamiento
* **Las interfaces físicas de radio NO son "mesh":** Un módem USB, un transceptor LoRa o una radio Wi-Fi/ESP-NOW son únicamente medios de transmisión y recepción de tramas crudas (L1/L2). No poseen ni deben poseer conocimiento sobre topologías de red, algoritmos de enrutamiento o protocolos multi-salto.
* **El enrutamiento "mesh" es un protocolo de Capa 3 (L3):** El algoritmo de enrutamiento (ej. protocolo PoP multi-salto, BATMAN-adv, Babel u OLSR) se ejecuta de forma desacoplada por encima de las interfaces físicas, utilizando las interfaces L2 disponibles para reenviar paquetes y mantener tablas de topología.

---

## 🧱 2. Pila de Protocolos de CBDos (L1 a L7)

```
 +-------------------------------------------------------------------------+
 |                      CAPA 7: APLICACIÓN (L7)                            |
 |   +--------------------------+  +-----------------------------------+   |
 |   |          cbdBBS          |  |            TlvBrowser             |   |
 |   | (Tablón / Foros Locales) |  |   (Navegador Micro-Web TLVGL)     |   |
 |   +--------------------------+  +-----------------------------------+   |
 +-------------------------------------------------------------------------+
                                     |
 +-------------------------------------------------------------------------+
 |                CAPA 5 - 6: SESIÓN Y PRESENTACIÓN (L5/L6)                |
 |   • TLVGL Payload Encoding (Type-Length-Value UI)                       |
 |   • Serialización binaria de estados y eventos de widgets               |
 +-------------------------------------------------------------------------+
                                     |
 +-------------------------------------------------------------------------+
 |                     CAPA 4: TRANSPORTE (L4)                             |
 |   • Microchunking & Reassembly (Segmentación de tramas grandes)         |
 |   • Control de flujo, reintentos y confirmación de entrega (ACKs)       |
 +-------------------------------------------------------------------------+
                                     |
 +-------------------------------------------------------------------------+
 |                  CAPA 3: RED Y ENRUTAMIENTO (L3)                        |
 |   • RoutingEngine / MeshProtocol: Cálculo de rutas y saltos             |
 |   • Descubrimiento de balizas de torre (PoP Broadcast)                  |
 |   • Pseudo-ARP: Mapeo de direcciones IPv4 Mesh (10.x.x.x) a MAC         |
 |   • Tabla de Reenvío Multi-Interfaz (Forwarding Information Base)       |
 +-------------------------------------------------------------------------+
                                     |
 +-------------------------------------------------------------------------+
 |                CAPA 2: ENLACE DE DATOS (L2 - Data Link)                 |
 |   • Contrato abstracto: INetworkInterface / IRadioDevice                |
 |   • Direccionamiento físico (MAC Hardware 6 bytes / Short ID 2 bytes)   |
 |   • Detección de errores (CRC8 / CRC16 / Checksum)                      |
 |   • Encolado de tramas crudas TX / RX (Zero Copy RingBuffers)           |
 +-------------------------------------------------------------------------+
                                     |
 +-------------------------------------------------------------------------+
 |                     CAPA 1: FÍSICA (L1 - Physical)                      |
 |                                                                         |
 |  [ Slot 0: Interno ]      [ Slot 1: Mochila ]     [ Slot 2: Módem USB ] |
 |   Radio 2.4 GHz 802.11     LoRa SX1262 915 MHz     ESP32-C3 CDC-ACM     |
 |   (ESP-NOW / LR)           (Bus SPI JP1)           (USB OTG 12 Mbps)    |
 +-------------------------------------------------------------------------+
```

---

## 🔌 3. Contratos de la Capa de Enlace (L2: Data Link)

Toda interfaz de comunicación en CBDos implementa la interfaz abstracta pura `INetworkInterface`. Esta interfaz es agnóstica al protocolo de red y se limita al intercambio de tramas crudas:

```cpp
namespace cbdos {
namespace network {

enum class InterfaceType : uint8_t {
    InternalRadio,  // ESP-NOW / Wi-Fi SoC
    BackpackLoRa,   // Transceptor LoRa SX1262
    UsbModemCdc,    // Dongle / Módem USB CDC-ACM
    Ethernet        // Puerto cableado RMII
};

class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;

    virtual bool init(uint8_t channel) = 0;
    virtual void stop() = 0;
    virtual bool isReady() const = 0;

    // Transmisión de trama L2 cruda al medio físico
    virtual bool sendFrame(const uint8_t* dest_mac, const uint8_t* payload, size_t len) = 0;

    // Callback de recepción de trama L2 cruda desde el medio físico
    using FrameRecvCallback = std::function<void(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi)>;
    virtual void setFrameRecvCallback(FrameRecvCallback cb) = 0;

    // Métricas de estado de la interfaz
    virtual bool getMacAddress(uint8_t out_mac[6]) const = 0;
    virtual uint8_t getChannel() const = 0;
    virtual bool setChannel(uint8_t channel) = 0;
    virtual InterfaceType getType() const = 0;
    virtual const char* getName() const = 0;
};

} // namespace network
} // namespace cbdos
```

---

## 🛰️ 4. Capa de Red y Enrutamiento (L3: Routing Engine)

La Capa 3 se encarga de recibir tramas crudas de cualquiera de las interfaces L2 registradas y aplicar las políticas de red:

1. **Ingreso de Paquete (Packet Ingress):**
   * Cuando la interfaz **Slot 2 (Módem USB C3)** recibe un paquete de radio del aire, genera una interrupción y pasa la trama L2 cruda a `RoutingEngine::onFrameReceived(interface_id, src_mac, data, len, rssi)`.
2. **Inspección de Cabecera de Red:**
   * **Baliza de Torre / PoP Broadcast:** Si el paquete contiene una baliza de presencia, se actualiza la tabla de torres visibles con su RSSI, canal e ID.
   * **Paquete Unicast para el Nodo Local:** Se desencapsula y se entrega a la Capa 4 (Transporte / Microchunking).
   * **Paquete en Tránsito (Forwarding):** Si el nodo actúa como repetidor o torre, consulta la tabla de rutas y reenvía la trama por la interfaz adecuada (incluso bridging entre USB y LoRa si corresponde).

---

## 🖥️ 5. Capa de Aplicación: Distinción de `cbdBBS`

### 5.1. Qué es `cbdBBS`
`cbdBBS` es una aplicación de Capa 7 para el usuario final. No gestiona frecuencias de radio ni enlaces USB directamente; utiliza los servicios provistos por la Capa de Transporte y Red:
* **Tablón de Anuncios Comunitario:** Almacenamiento y sincronización de mensajes públicos entre nodos cercanos.
* **Canales Temáticos:** Publicaciones organizadas por tópicos (`#general`, `#emergencias`, `#mercado`).
* **Radar de Vecinos:** Lista de identidades comunitarias y alias descubiertos a través de la red.

### 5.2. Gestor de Redes (`NetworkManagerView`)
Es el panel de administración de hardware del sistema operativo:
* Muestra el estado físico de los 3 slots de interfaz (**Slot 0: Radio Interna**, **Slot 1: Mochila LoRa**, **Slot 2: Módem USB**).
* Permite cambiar de canal, potencia y monitorear el tráfico crudo de cada interfaz de forma independiente.
