# Especificación Técnica: Desacoplamiento y Modularización del Subsistema PTMP y Tabla de Pares (PeerTable)

**Documento:** `specs/network/especificacion_desacoplamiento_ptmp_y_peertable.md`  
**Fecha:** 14 de Octubre de 2026  
**Estado:** Propuesta de Arquitectura Formal  
**Autor:** Antigravity AI & kaber420  
**Ámbito:** `core/src/mesh/` ➔ `core/src/ptmp/`, `core/include/cbdos/ptmp/`, BSPs ESP32-S3 y ESP32-P4  

---

## 1. Justificación y Diagnóstico de la Deuda Técnica

El componente actual denominado históricamente `MeshEngine` (`core/src/mesh/MeshEngine.cpp`) arrastra una severa deuda arquitectónica:

1. **Ambigüedad y Falso Nombre ("Mesh"):**
   El código actual no implementa un protocolo de enrutamiento en malla dinámico multi-salto (como BATMAN-adv, Babel o AODV). En su lugar, implementa un **enlace de radio Punto a Multipunto (PTMP) ad-hoc** optimizado para transmisiones de baja latencia entre terminales y torres base mediante tramas broadcast/unicast de radio.
2. **Confusión Conceptual entre ARP y Resolución de Pares de Radio:**
   Se utilizaba el término "ARP" y "Pseudo-ARP" para una simple tabla asociativa de pares de radio (`ShortID ➔ MAC`). Esto colisiona conceptualmente con el protocolo estándar ARP (RFC 826) de la pila TCP/IP (LwIP). En radio ad-hoc no existe tráfico broadcast de sondeo ARP; la asociación se aprende pasivamente a coste cero (*zero-overhead*) de las tramas entrantes.
3. **Monolito en Archivo Gigante ("God Class"):**
   En lugar de estar descompuesto en componentes independientes, `MeshEngine.cpp` concentra más de 470 líneas con todas las responsabilidades apiñadas en un único archivo:
   - Control físico de radio (modos Wi-Fi/ESP-NOW, canales RF).
   - Tabla asociativa de pares y algoritmos de expiración.
   - Algoritmo de fragmentación (*Micro-chunking* de 240B) y reensamblado con bitmask.
   - Parser de cabeceras de red binarias.
   - Lógica de aplicación cliente (construcción de tramas URL para TLVGL).
   - Descubrimiento de balizas de torres y extracción del Epoch horario.

---

## 2. Nuevo Modelo Arquitectónico en Capas

El subsistema se reorganiza de acuerdo con el modelo de capas canónico de CBDos:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        CAPA 7: APLICACIÓN                              │
│                                                                        │
│   [ TlvBrowserView ]          [ RadioConfigView ]     [ Diagnostics ]  │
│   • Genera tramas TLV         • UI de radio y torres  • Métricas RX/TX │
└───────────────────┬───────────────────┬───────────────────┬────────────┘
                    │                   │                   │
                    ▼                   ▼                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│                 CAPA 5-6: SERVICIOS DE SISTEMA                         │
│                                                                        │
│   [ TowerService ]                                                     │
│   • Sondeo periódico de torres (Tower Probes)                          │
│   • Ingesta de balizas PoP (PoPBroadcastFrame)                         │
│   • Inyección de Epoch horaria a cbdos::time (offline-first)           │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│             CAPA 3-4: TRANSPORTE Y ENLACE (cbdos::ptmp)                │
│                                                                        │
│   [ PtmpLink ] (Orquestador de Enlace)                                 │
│   • Despachador de servicios por ServiceId                             │
│   • Coordinación entre Framer, PeerTable y Transporte                  │
│                                                                        │
│   [ PtmpFramer ]                                                       │
│   • Serialización de cabeceras PtmpHeader                              │
│   • MicroChunker: Segmentación (hasta 8x240B) y reensamblado L4        │
│                                                                        │
│   [ PeerTable ] (Caché de Pares)                                       │
│   • Mapeo: uint16_t short_id ➔ uint8_t mac[6]                          │
│   • Métricas por peer: RSSI, timestamp último visto (last_seen_ms)     │
│   • Aprendizaje pasivo L2 (Zero airtime overhead)                      │
│   • Aislado completamente de LwIP / TCP-IP ARP                         │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                     CAPA 1-2: RADIO HAL & TRANSPORTE                   │
│                                                                        │
│   [ cbdos::radio::IRadioBackend ]       [ IPtmpTransport ]             │
│   • Control de hardware (canales, RF)   • S3: ESP-NOW nativo           │
│   • Encendido / Apagado / Potencia TX   • P4: USB CDC Dongle           │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Descomposición Modular: Estructura de Archivos

Para eliminar el archivo monolítico gigante, cada responsabilidad se separa en su propio par de archivos `.hpp` y `.cpp` con dependencias mínimas:

```
core/
├── include/cbdos/ptmp/
│   ├── ptmp_types.hpp        // PODs, Enums, ServiceId, PoPBroadcastFrame, MicroChunkHeader
│   ├── peer_table.hpp        // Clase PeerTable y estructura PeerInfo
│   ├── ptmp_framer.hpp       // Segmentador / Reensamblador MicroChunker y serialización
│   ├── ptmp_transport.hpp    // Interfaz abstracta IPtmpTransport
│   ├── ptmp_link.hpp         // Motor de enlace PtmpLink (Singleton)
│   └── tower_service.hpp     // Servicio de descubrimiento de torres y sync de Epoch
└── src/ptmp/
    ├── PeerTable.cpp         // Implementación de la tabla de pares (Thread-safe)
    ├── PtmpFramer.cpp        // Implementación de segmentación y reensamblado
    ├── PtmpLink.cpp          // Implementación del motor de enlace L3/L4
    └── TowerService.cpp      // Implementación del cliente de torres y balizas
```

---

## 4. Especificación de Módulos e Interfaces

### 4.1. Módulo `PeerTable` (`peer_table.hpp` / `PeerTable.cpp`)
Responsabilidad única: Gestión del registro de pares de radio en memoria.

```cpp
namespace cbdos {
namespace ptmp {

struct PeerInfo {
    uint16_t short_id = 0;
    uint8_t  mac[6] = {0};
    int8_t   last_rssi = -127;
    uint32_t last_seen_ms = 0;
};

class PeerTable {
public:
    static PeerTable& getInstance();

    void updatePeer(uint16_t short_id, const uint8_t mac[6], int8_t rssi, uint32_t now_ms);
    bool lookupMac(uint16_t short_id, uint8_t out_mac[6]) const;
    bool getPeer(uint16_t short_id, PeerInfo& out_peer) const;
    void pruneExpired(uint32_t now_ms, uint32_t timeout_ms = 300000);
    void clear();

    size_t getPeerCount() const;
    std::vector<PeerInfo> getAllPeers() const;

private:
    PeerTable() = default;
    std::unordered_map<uint16_t, PeerInfo> m_peers;
    mutable std::mutex m_mutex;
};

} // namespace ptmp
} // namespace cbdos
```

### 4.2. Módulo `PtmpFramer` (`ptmp_framer.hpp` / `PtmpFramer.cpp`)
Responsabilidad única: Fragmentación de paquetes en micro-chunks de hasta 240 bytes y reensamblado por bitmask con temporizador de descarte.

```cpp
namespace cbdos {
namespace ptmp {

struct ChunkReassembly {
    uint8_t msg_id;
    uint8_t total_chunks;
    uint8_t received_mask;
    uint32_t start_time_ms;
    std::vector<std::vector<uint8_t>> chunks;
    uint8_t src_mac[6];
    int8_t rssi;
};

class PtmpFramer {
public:
    PtmpFramer();

    // Divide un buffer en tramas listas para transmisión física
    bool fragmentPacket(uint8_t msg_id, const uint8_t* full_packet, size_t full_len,
                        std::vector<std::vector<uint8_t>>& out_frames);

    // Ingesta un frame recibido y retorna true si se completó un paquete entero
    bool processIncomingFrame(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi,
                             std::vector<uint8_t>& out_completed_packet);

    void pruneExpiredReassemblies(uint32_t now_ms, uint32_t timeout_ms = 2500);
    void reset();

private:
    std::vector<ChunkReassembly> m_reassemblies;
};

} // namespace ptmp
} // namespace cbdos
```

### 4.3. Módulo `PtmpLink` (`ptmp_link.hpp` / `PtmpLink.cpp`)
Responsabilidad única: Orquestar el transporte L2/L3, resolver destinos usando `PeerTable`, fragmentar con `PtmpFramer` y despachar payloads a servicios registrados.

```cpp
namespace cbdos {
namespace ptmp {

class PtmpLink {
public:
    static PtmpLink& getInstance();

    void setTransport(IPtmpTransport* transport);
    IPtmpTransport* getTransport() const;

    bool init();
    void stop();
    bool isRunning() const;

    void setLocalIdentity(uint16_t short_id, uint32_t uuid);
    uint16_t getLocalShortId() const;
    uint32_t getLocalUuid() const;

    // Envío segmentado con resolución automática de MAC mediante PeerTable
    bool sendPacket(ServiceId svc, uint16_t dst_id, const uint8_t* payload, size_t len,
                    bool dst_only = true, const uint8_t* target_mac = nullptr);

    void registerServiceHandler(ServiceId svc, PtmpServiceCallback cb);
    void unregisterServiceHandler(ServiceId svc);

    void tick(uint32_t now_ms);

    uint32_t getRxPackets() const;
    uint32_t getTxPackets() const;
    int8_t getLastRssi() const;

private:
    PtmpLink();
    ~PtmpLink();

    void onRawDataReceived(const uint8_t* src_mac, const uint8_t* data, size_t len, int8_t rssi);
    void dispatchCompletedPacket(const uint8_t* src_mac, const uint8_t* full_data, size_t full_len, int8_t rssi);

    IPtmpTransport* m_transport = nullptr;
    PtmpFramer m_framer;
    bool m_running = false;
    uint8_t m_currentMsgId = 1;
    uint16_t m_localShortId = 0;
    uint32_t m_localUuid = 0;

    std::unordered_map<uint8_t, PtmpServiceCallback> m_serviceHandlers;
    uint32_t m_rxPackets = 0;
    uint32_t m_txPackets = 0;
    int8_t m_lastRssi = -127;
};

} // namespace ptmp
} // namespace cbdos
```

### 4.4. Módulo `TowerService` (`tower_service.hpp` / `TowerService.cpp`)
Responsabilidad única: Manejo de balizas de torre PoP, sondeo periódicos y sincronización horaria del sistema.

```cpp
namespace cbdos {
namespace ptmp {

class TowerService {
public:
    static TowerService& getInstance();

    void init();
    void sendTowerProbe();
    const std::vector<DiscoveredTower>& getDiscoveredTowers() const;
    void clearDiscoveredTowers();
    void setDiscoveredTowersCallback(std::function<void()> cb);
    void setEpochReceivedCallback(std::function<void(time_t)> cb);

private:
    TowerService();
    void handleRoutingControlPacket(const PtmpPacket& packet);

    std::vector<DiscoveredTower> m_discoveredTowers;
    std::function<void()> m_towersCb;
    std::function<void(time_t)> m_epochReceivedCb;
};

} // namespace ptmp
} // namespace cbdos
```

---

## 5. Plan de Implementación y Migración Modular (Fix-Forward)

1. **Fase 1: Creación de Tipos y Framer Autónomo**
   - Implementar `ptmp_types.hpp` y `ptmp_transport.hpp`.
   - Implementar `peer_table.hpp` y `PeerTable.cpp` (con tests/compilación limpia).
   - Implementar `ptmp_framer.hpp` y `PtmpFramer.cpp` (lógica de chunking aislada y testeable).
2. **Fase 2: Motor de Enlace y Servicio de Torres**
   - Implementar `ptmp_link.hpp` y `PtmpLink.cpp`.
   - Implementar `tower_service.hpp` y `TowerService.cpp`.
3. **Fase 3: Migración de Clientes de Aplicación y HAL**
   - [TlvBrowserView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TlvBrowserView.cpp): La construcción del TLV `TYPE_REQ_URL` se aloja en el propio navegador y envía vía `PtmpLink::sendPacket()`.
   - [MeshConfigView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/MeshConfigView.cpp): Renombrar / adaptar para consultar `TowerService`, `PeerTable` y `cbdos::radio`.
   - [hal_mesh_s3.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_mesh_s3.cpp) y [hal_mesh_p4.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_mesh_p4.cpp): Conectar `s_s3Transport` y `s_usbRadioTransport` a `PtmpLink`.
   - [main.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/main/main.cpp) (P4 y S3): Iniciar `TowerService`.
4. **Fase 4: Deprecación Segura de `MeshEngine` y Purga**
   - Proveer cabecera puente/adaptador temporal para evitar rupturas mientras se migra, y luego retirar el código muerto sin tocar nada externo.
5. **Fase 5: Validación de Compilación Dual**
   - ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
