# 📡 Arquitectura Definitiva de Redes, Malla y Ruteo por Capas (CBDos v0.2.1)

**Documento:** `docs/architecture/arquitectura_definitiva_redes_malla_y_ruteo_por_capas.md`  
**Versión:** 1.0.0 (Especificación Técnica Final Consolidada)  
**Estado:** 🎯 Documento Canónico de Implementación  
**Target Hardware:** ESP32-P4 (JC4880P443C) + ESP32-C6 + Mochilas SPI (SX1262 / SX1280) + Módems USB-CDC  

---

## 🏛️ 1. Filosofía de Red y Resumen Ejecutivo

Este documento define la arquitectura canónica y definitiva del subsistema de comunicaciones de CBDos. Se rige por los siguientes principios de ingeniería embebida:

1. **Cero Desperdicio de Recursos (Airtime & Memory Budget):**
   - No se utilizan identificadores arbitrarios pesados ni protocolos no optimizados para microcontroladores.
   - El direccionamiento se resuelve exclusivamente mediante **MAC física (6 Bytes)**, **Short IDs locales (2 Bytes)** y direccionamiento jerárquico **Pseudo-IP `10.0.0.0/8` (4 Bytes)**.
   - Las tramas locales utilizan una cabecera ultracompacta de **3 Bytes** para maximizar el alcance de radio, minimizar el tiempo de aire (*Airtime*) y extender la vida de la batería.

2. **Separación Estricta de Responsabilidades en 3 Capas:**
   - La **Aplicación** solo produce y consume datos útiles de usuario (*Payload*).
   - El **Motor de Ruteo (`MeshCoreEngine`)** calcula saltos, agrega cabeceras mínimas y elige por qué slot/antena despachar el paquete.
   - El **HAL de Radio (`INetworkInterface`)** modula y emite los bytes ciegamente según la frecuencia y parámetros físicos configurados.

3. **Arquitectura Offline-First:**
   - El sistema arranca con todas las radios apagadas por defecto. La activación de interfaces es 100% bajo demanda desde la UI o scripts del sistema.

---

## 📐 2. Modelo de 3 Capas y Flujo de Paquetes

```text
┌───────────────────────────────────────────────────────────────────────────────────┐
│                          CAPA 3: APLICACIONES & SERVICIOS                         │
│     (MeshChat, Telemetría, Audio Streaming, Terminal Lua, TLV Browser)           │
│                                                                                   │
│  • Solo manipula datos útiles hiperdensos (texto, binario, audio comprimido).   │
│  • NO conoce frecuencias, radios físicas ni saltos de red.                       │
└────────────────────────────────────────┬──────────────────────────────────────────┘
                                         │  Payload Útil: [ Datos (N Bytes) ]
                                         ▼
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    CAPA 2: MOTOR DE RUTEO Y MALLA (MeshCoreEngine)                │
│                                                                                   │
│  • Agrega la cabecera dinámica de red (3B a 9B según el destino).                │
│  • Gestiona tablas de vecinos, detección de duplicados (Nonce/SeqNum) y ACKs.     │
│  • Selecciona la interfaz física de salida (Slot 0, Slot 1 o Slot 2).            │
└────────────────────────────────────────┬──────────────────────────────────────────┘
                                         │  Trama Lista: [ Cabecera + Payload ]
                                         ▼
┌───────────────────────────────────────────────────────────────────────────────────┐
│                  CAPA 1: GESTOR DE INTERFACES HAL (NetworkInterfaceManager)       │
│                                                                                   │
│   [ Slot 0: ESP32-C6 ]          [ Slot 1: Semtech SX1262 ]   [ Slot 2: Semtech SX1280 ]   │
│   • ESP-NOW (2.4 GHz)           • LoRa 915 / 868 MHz         • FLRC 2.4 GHz (1.3 Mbps)   │
│   • Wi-Fi 6 STA/AP              • SF7-SF12, BW 125 kHz       • Ráfagas de alta tasa      │
│   • BLE 5.0 GATT                • Alcance: 10 - 30 km        • Streaming / Archivos      │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 📦 3. Estructura de Cabeceras y Tramas en el Aire

La cabecera muta dinámicamente según la distancia y el alcance del destino para no desperdiciar bytes en enlaces locales.

### 3.1. Niveles de Cabecera Dinámica

```text
Nivel 0: Salto Local (Mismo PoP / Mismo Enlace Directo de Radio)
┌──────────────┬───────────────────┬────────────────────────────────┬─────────────────┐
│ Control (1B) │ Short Node ID (2B)│     Payload Crudo de App       │ CRC16 / FCS (2B)│
└──────────────┴───────────────────┴────────────────────────────────┴─────────────────┘
==> Longitud de Cabecera: 3 Bytes (Máxima densidad de datos)

Nivel 1: Inter-PoP (Cruza entre Gateways dentro de la misma Zona)
┌──────────────┬───────────────────┬──────────────┬───────────────────────────────────┐
│ Control (1B) │ Short Node ID (2B)│  PoP ID (2B) │     Payload Crudo                 │
└──────────────┴───────────────────┴──────────────┴───────────────────────────────────┘
==> Longitud de Cabecera: 5 Bytes

Nivel 2: Inter-Zona (Cruza regiones dentro del mismo ASN)
┌──────────────┬───────────────────┬──────────────┬──────────────┬────────────────────┐
│ Control (1B) │ Short Node ID (2B)│  PoP ID (2B) │ Zone ID (2B) │   Payload Crudo    │
└──────────────┴───────────────────┴──────────────┴──────────────┴────────────────────┘
==> Longitud de Cabecera: 7 Bytes

Nivel 3: Inter-ASN / Backbone Global
┌──────────────┬───────────────────┬──────────────┬──────────────┬──────────────┬─────┐
│ Control (1B) │ Short Node ID (2B)│  PoP ID (2B) │ Zone ID (2B) │  ASN ID (2B) │ ... │
└──────────────┴───────────────────┴──────────────┴──────────────┴──────────────┴─────┘
==> Longitud de Cabecera: 9 Bytes
```

### 3.2. Detalle de Bits del Byte de Control (Byte 0)

| Bits | Campo | Valores | Descripción |
| :--- | :--- | :--- | :--- |
| **[7:6]** | **Scope (Alcance)** | `00`: Local PoP (3B) <br> `01`: Inter-PoP (5B) <br> `10`: Inter-Zona (7B) <br> `11`: Inter-ASN (9B) | Indica cuántos bytes de ruta siguen al Short Node ID. |
| **[5:4]** | **QoS / Prioridad** | `00`: Baja (Telemetría/Beacon) <br> `01`: Normal (Chat/Texto) <br> `10`: Alta (Control/Comandos) <br> `11`: Urgente / Alarma SOS | Prioriza el encolamiento y selección de antena física. |
| **[3:2]** | **Flags de Transporte** | `00`: Unidireccional sin ACK <br> `01`: Requiere ACK a nivel enlace <br> `10`: Paquete Fragmentado <br> `11`: Reservado | Control de entrega y reensamblado. |
| **[1:0]** | **Versión Protocolo** | `00`: Versión 1.0 CBDos | Compatibilidad y evolución futura. |

---

## 🪪 4. Sistema de Identidades, Pseudo-IP `10.0.0.0/8` y DAD

El direccionamiento combina la inmutabilidad del silicio con la compatibilidad total con herramientas de red estándar:

### 4.1. Mapeo de Identidades
| Nivel | Identificador | Tamaño | Formato / Derivación | Propósito |
| :--- | :--- | :---: | :--- | :--- |
| **Hardware** | **MAC Física de Fábrica** | **6 Bytes** | `MAC[0..5]` (Quemada en eFuse de fábrica) | Identidad física permanente e inmutable. |
| **Pseudo-IP Link-Local** | **IPv4 SLAAC Hardware-Bound** | **4 Bytes** | **`10.MAC[3].MAC[4].MAC[5]`** <br>*(ej: MAC `..:01:7C:0C:94` ➔ `10.1.124.148`)* | Interoperabilidad 1:1 con sockets BSD, SSH, web y ping sin servidor DHCP. |
| **Local RF** | **Short Node ID** | **2 Bytes** | **`MAC[4..5]`** `(uint16_t)` <br>*(ej: `0x0C94`)* | Transmisión por radio ultracompacta en cabecera de 3 Bytes. |
| **Topológico / Mesh** | **IPv4 Jerárquica** | **4 Bytes** | **`10.Zona.PoP.Nodo`** *(ej: `10.1.3.42`)* | Enrutamiento estructurado en mallas con Gateways/Torres. |
| **Global Inter-ASN** | **Tupla Jerárquica** | **8 Bytes** | **`[ASN:2B \| Zone:2B \| PoP:2B \| ShortID:2B]`** | Ruteo troncal en federaciones de radio de larga distancia. |

---

### 4.2. Autoconfiguración y DAD (Duplicate Address Detection)

Para operar de forma **Zero-Config (Plug & Play)** con garantía de unicidad:

```text
   ESP32 (MAC: ..:01:7C:0C:94)                               Gateway / Torre / Malla
         │                                                             │
 Paso 1: │─── [PROBE DAD: ShortID propuesto 0x0C94, IP 10.1.124.148]──►│
 Sondeo  │                                                             │
         │                                              [Consulta Tabla Pseudo-ARP]
         │                                              ¿0x0C94 ocupado por otra MAC?
         │                                                             │
         │                                      ┌──────────────────────┴──────────────────────┐
         │                                      ▼ (Sin colisión - 98.2% de los casos)         ▼ (Colisión detectada - 1.8%)
 Paso 2: │◄── [ACK: Aprobado ShortID 0x0C94]────│                              ◄── [REASSIGN: Usa ShortID 0x0C95]───│
 Valida  │                                      │                                                                   │
         ▼                                      ▼                                      ▼                            ▼
   [Opera en 0x0C94]                      [Registrado]                           [Actualiza a 0x0C95]         [Registrado]
```

1. **Propuesta Optimista:** El nodo toma los últimos 2 bytes de su MAC (`MAC[4..5]`) como `Short ID` y sus 3 bytes bajos como `10.MAC[3].MAC[4].MAC[5]`.
2. **Sondeo DAD:** Al encenderse, emite una ráfaga broadcast rápida de sondeo.
3. **Resolución:**
   - **Caso Normal (>98%):** No hay colisión; el nodo opera inmediatamente con su identidad nativa derivada de silicio.
   - **En Caso de Colisión:** El Gateway o vecino emite un paquete de reasignación con un `Short ID` libre del pool local.

---

### 4.3. Inmunidad a Colisiones por Semántica de Scope (P2P vs Global)

Una ventaja crítica del **Byte de Control (`Scope [7:6]`)** es que aísla completamente los contextos de red:

1. **Cero Colisiones Inter-Región:**
   - Si dos Cyberdecks en ciudades diferentes tienen por coincidencia el mismo Short ID `0x0C94`, **jamás colisionan**.
   - En el aire local (`Scope = 00`), el paquete solo tiene significado dentro de su radio de cobertura inmediata (P2P o con su propio Gateway local).
   - Cuando el paquete sale hacia el backbone (`Scope = 11`), el router le antepone el prefijo global `[ASN:Zona:PoP]`, convirtiéndolo en una dirección globalmente única y determinista.

2. **Comunicación P2P Directa de Tú a Tú (Offline):**
   - Para dos usuarios en el campo sin torres ni internet que quieran chatear por ESP-NOW o LoRa, la **Pseudo-IP Link-Local (`10.MAC[3].MAC[4].MAC[5]`)** o el **Short ID (`MAC[4..5]`)** permiten enlace directo instantáneo de tú a tú.
   - La identidad fija también es la que usa el nodo para presentarse directamente de tú a tú ante el Gateway y poblar la tabla Pseudo-ARP.

---

## 🔄 5. Autoconfiguración y Estrategia de Gateway (Modo Híbrido)

Para que los nodos satélites se conecten a la salida exterior (Inter-PoP / WAN):

```text
       [ NODO SATÉLITE ]                                       [ GATEWAY P4 / REPETIDOR ]
              │                                                            │
  Paso 1:     │─── [Probe Anycast a 0x0000: "Busco Gateway"]──────────────►│
  Descubrimiento                                                           │
              │                                                            │
  Paso 2:     │◄── [Gateway Announcement: "Soy GW 0x0001 en PoP 0x0012"]───│
  Registro    │                                                            │
              │ (Almacena ID 0x0001 en RAM)                                │
              │                                                            │
  Paso 3:     │═══ [Tráfico Operativo Unicast a 0x0001 (Con ACK HW)]══════►│
  Operación   │                                                            │
              │                                                            │
  Paso 4:     │ (Si falla 3 veces seguidas el ACK unicast)                │
  Fallback    │─── [Regresa a Anycast 0x0000 para redescubrir]────────────►│
```

---

## 🧭 6. Servicio de Directorio de Ubicación entre Torres (Pseudo-DDNS)

Para resolver la movilidad de los usuarios sin saturar las radios de campo, las Torres y Gateways implementan un servicio de **Directorio de Ubicación Federado (Pseudo-DDNS / Map-Server)**:

```text
                               ┌────────────────────────────────────────────────────────┐
                               │  DIRECTORIO DE UBICACIÓN FEDERADO (Torres / DDNS)      │
                               │  - Tablas distribuidas entre Gateways P4 / Servidores  │
                               └─────────────────────────▲──────────────────────────────┘
                                                         │
                     1. Registro al entrar a nuevo PoP   │  2. Consulta de Ubicación
                        "Alice (10.124.12.148) está      │     "¿Dónde está Bob (10.5.2.1)?"
                         ahora en ASN:1, Zone:3, PoP:18" │     Respuesta: "Está en ASN:5, Zone:2, PoP:88"
                                                         │
                     ┌───────────────────────────────────┴───────────────────────────────────┐
                     │                                                                       │
      ┌──────────────┴──────────────┐                                         ┌──────────────┴──────────────┐
      │  TORRE / GATEWAY ORIGEN     │                                         │  TORRE / GATEWAY DESTINO    │
      │  (PoP 0x0018 / ASN 0x0001)  │ ══════════════════════════════════════> │  (PoP 0x0088 / ASN 0x0005)  │
      ├─────────────────────────────┤     3. Tráfico Ruteado Directo          ├─────────────────────────────┤
      │ • Guarda en Cache Local     │        [ASN:5|Zone:2|PoP:88|ShortID]    │ • Entrega a Bob por radio   │
      └──────────────▲──────────────┘                                         └──────────────▲──────────────┘
                     │                                                                       │
           (Trama 3B │ Local)                                                      (Trama 3B │ Local)
                     ▼                                                                       ▼
             [ ALICE (Nodo C3) ]                                                     [ BOB (Nodo C3) ]
             IP Fija: 10.124.12.148                                                  IP Fija: 10.5.2.1
```

### 6.1. Flujo de Localización y Presencia Opt-In:
1. **Activación de Presencia (Opt-In):** Si el usuario tiene habilitado el servicio de localización en su Cyberdeck, al conectarse a una nueva Torre se registra con su **Identidad Fija** (`MAC Física (6B)` / `Pseudo-IP Fija (4B)`).
2. **Registro Topológico en la Torre:** La Torre local anota en su tabla:
   `[Identidad Fija] ➔ [ASN:0, Zona:23, PoP:198, ShortID: 23145]`.
   *(En más del 98% de los casos, el `Short ID` es el natural de su MAC porque DAD no detectó repetidos).*
3. **Consulta Inter-Torres Bajo Demanda:** Si un nodo en otra Torre/Zona quiere enviarle un mensaje a su identidad fija, su Torre de origen consulta el directorio de presencia:
   * **Pregunta:** *"¿Dónde está la identidad fija `10.124.12.148`?"*
   * **Respuesta de la Torre Destino:** *"Está aquí en `ASN:0`, `Zona:23`, `PoP:198`, con `ShortID:23145`"*.
4. **Caché y Entrega Directa:**
   * La Torre emisora almacena la ruta en su caché en RAM/PSRAM.
   * Encapsula los paquetes con la tupla ruteable de 8 bytes y los despacha por el enlace troncal (LoRa/WAN).
   * La Torre receptora retira la cabecera troncal y entrega el mensaje a la antena local en una **trama corta de 3 bytes** al `Short ID 23145`.

---

## 📡 7. Micro-Broadcast PMP de 7 Bytes (NTP Pasivo, Hash de Sitio y Estado)

Para anuncios periódicos de presencia sin saturar el aire, las Torres y Nodos emiten una trama **Punto a Multipunto (PMP)** de **exactamente 7 Bytes (Little-Endian)** cada 60s (0 TX del cliente):

```text
┌────────────────────────────────────────────────────────────────────────┐
│               MICRO-BROADCAST DE NODO / TORRE (7 BYTES)                │
├────────────────────────┬───────────────────────┬───────────────────────┤
│  Unix Epoch (4 Bytes)  │  Cover Hash (2 Bytes) │  Status Code (1 Byte) │
│    [ Segundos Unix ]   │   [ CRC16 de Sitio ]  │   [ Bitmask Servicios]│
│       Bytes 0..3       │       Bytes 4..5      │         Byte 6        │
└────────────────────────┴───────────────────────┴───────────────────────┘
```

1. **NTP Pasivo (Bytes 0..3 — `uint32_t epoch`):** Sincroniza el reloj del sistema/RTC del Cyberdeck de forma 100% pasiva y silenciosa sin emitir ráfagas de sincronización.
2. **Hash de Sitio/Portada (Bytes 4..5 — `uint16_t cover_hash`):** CRC16 de la página o tablón local (`index.bcml` / `index.html`). El navegador TLV verifica si el hash cambió antes de solicitar contenido, ahorrando ancho de banda.
3. **Bitmask de Servicios (Byte 6 — `uint8_t status_code`):**
   * `0x01` (`POP_STATUS_INTERNET_UP`): Salida a Internet WAN activa (1) vs Red local (0).
   * `0x02` (`POP_STATUS_PROXY_OPEN`): Proxy libre vs Requiere autenticación.
   * `0x04` (`POP_STATUS_BAAS_BUSY`): Servidor digestor ocupado vs Disponible.
   * `0x08` (`POP_STATUS_ALERT_ACTIVE`): Alerta o mensaje de emergencia en el tablón local.
   * `0xF0`: Bits reservados para futuros servicios.

---

## 🛠️ 8. Contratos de Software HAL en C++ (`core/`)

### 8.1. Contrato `INetworkInterface`

```cpp
namespace cbdos {
namespace network {

enum class InterfaceType {
    Disabled,
    RadioPacket,   // LoRa SX1262, FLRC SX1280, ESP-NOW
    IpNetwork,     // Wi-Fi STA/AP con stack TCP/IP
    BluetoothLe,   // BLE 5.0 GATT
    SerialModem    // USB-CDC / UART TNC
};

enum class InterfaceMode {
    Off,
    EspNow,
    WifiStation,
    WifiAccessPoint,
    BleGattServer,
    LoRa_915MHz,
    LoRa_868MHz,
    FLRC_2400MHz,
    UsbCdcTnc
};

typedef void (*PacketRecvCallback)(const uint8_t* payload, size_t len, int rssi, int snr, void* userCtx);

class INetworkInterface {
public:
    virtual ~INetworkInterface() = default;

    virtual const char* getName() const = 0;
    virtual InterfaceType getType() const = 0;
    virtual InterfaceMode getMode() const = 0;
    virtual bool setMode(InterfaceMode mode) = 0;

    virtual bool isReady() const = 0;
    virtual int sendPacket(const uint8_t* buffer, size_t len) = 0;
    virtual void setPacketRecvCallback(PacketRecvCallback cb, void* userCtx) = 0;
};

} // namespace network
} // namespace cbdos
```

### 8.2. Gestor Singleton `NetworkInterfaceManager`

```cpp
namespace cbdos {
namespace network {

#define MAX_NETWORK_SLOTS 4

class NetworkInterfaceManager {
public:
    static NetworkInterfaceManager& getInstance();

    bool registerInterface(uint8_t slot, INetworkInterface* iface);
    INetworkInterface* getInterface(uint8_t slot);

    void setAllOffline();
    bool isAnyInterfaceActive() const;

private:
    INetworkInterface* m_slots[MAX_NETWORK_SLOTS] = {nullptr};
};

} // namespace network
} // namespace cbdos
```

---

## 🔒 9. Reserva de Espacio para la Futura Capa Criptográfica

El diseño reserva explícitamente el espacio en el payload para cuando se implemente la capa de seguridad una vez consolidado el enrutamiento:

```text
┌───────────────────────────────────────────────────────────────────────────────────┐
│                           ESTRUCTURA DE TRAMA SEGURA (FUTURO)                     │
├───────────────────┬───────────────────────────────────────────────────────────────┤
│ Cabecera Ruteo    │ Payload Criptográfico Seguro                                  │
│ (3 a 9 Bytes)     │ • Nonce / IV de Sesión (8-12 Bytes)                           │
│                   │ • Datos Útiles Cifrados (AES-256-CTR / ChaCha20)              │
│                   │ • Tag de Autenticación / HMAC (8-16 Bytes)                     │
└───────────────────┴───────────────────────────────────────────────────────────────┘
```

Al mantener la cabecera de ruteo en solo **3 Bytes** locales, queda más del **90% del MTU disponible** para el cifrado y los datos del usuario sin riesgo de fragmentación.

---

## 📋 10. Estado y Plan de Ejecución

1. **Fase 1 (Completada):** Sockets y streams limpios en `core/` (`ISocketStream`).
2. **Fase 2 (En Curso):** Implementación formal de `INetworkInterface` y `NetworkInterfaceManager` en `core/` y drivers BSP (`hal_radio_p4.cpp`, `hal_radio_s3.cpp`).
3. **Fase 3:** Integración con la UI de Ajustes (pantalla de gestión de slots y Modo Avión).
4. **Fase 4:** Incorporación de la capa criptográfica (cifrado simétrico E2EE y autenticación).
