# 🌐 Protocolo de Enrutamiento Jerárquico Dinámico y Pseudo-BGP para CBDos (MeshCore / Pseudo-IP)

## 1. Visión General del Sistema y Filosofía de Red

Este protocolo implementa un modelo de **enrutamiento jerárquico de longitud variable (Variable-Length Hierarchical Routing)** diseñado para optimizar al extremo el ancho de banda en enlaces de radio heterogéneos (LoRa Sub-GHz, FLRC 2.4 GHz, ESP-NOW LR, IEEE 802.15.4).

### 🎯 Principios Fundamentales:
1. **Nodos Satélite / RF Bridges Tontos (ESP32-C3 / C6 / H2):**
   - **Cero cómputo de rutas:** Los nodos de campo y transceptores USB no mantienen tablas de rutas, ni conocen la topología global, ni calculan caminos.
   - Solo reciben datos por antena/USB y los escupen al aire o al host en tramas ultracompactas de **3 bytes** de cabecera local.
2. **Gateways Inteligentes (ESP32-P4 / SBCs ARM64 Linux):**
   - Son los **únicos responsables** de mantener las tablas de rutas (BGP/OSPF jerárquico), resolver identidades criptográficas (UUIDs / Claves Públicas) a direcciones topológicas, y mutar dinámicamente el tamaño de la cabecera:
     - **Encapsulado (Agregar 2B/4B/6B):** Cuando un paquete sale de su PoP, Zona o ASN hacia el exterior.
     - **Desencapsulado (Quitar bytes):** Cuando un paquete llega a su destino local, entregando a la antena final solo la cabecera mínima de 3 bytes para ahorrar tiempo de aire (Airtime) y batería.

---

## 2. Dualidad de Direccionamiento: Identidad Global vs Dirección Ruteable (Modelo LISP / IPv6)

Para responder a la arquitectura de red sin fricciones, el sistema opera con una **separación estricta entre Identidad (Quién eres) y Ubicación (Dónde estás)**, idéntico al modelo de **IPv6 (Link-Local vs Global Unicast)** y la arquitectura **LISP (Locator/ID Separation Protocol)**:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ 1. IDENTIDAD GLOBAL INMUTABLE (Quién es el Nodo - Hardware MAC de Fábrica)             │
│    • EUI-48 / MAC Física de Fábrica (6 Bytes) -> ej. 24:DC:C3:4A:12:F0                │
│    • UUID de Nodo / IPv4 Mesh (4 Bytes)       -> ej. 10.1.18.240 o MAC[2..5]          │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 2. PSEUDO-IP LINK-LOCAL (En el PoP Local - Airtime ultra-reducido)                     │
│    • Short Node ID: MAC[4..5] (2 Bytes)       -> ej. 0x12F0                            │
│    • Pseudo-IP Local: 10.[PoP_Local].[MAC_High].[MAC_Low] (Ámbito del PoP)             │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 3. DIRECCIÓN GLOBAL RUTEABLE (Dónde está en la Federación Mundial - Cambia al viajar)  │
│    • Tupla Topológica de 8 Bytes: [ ASN (2B) : Zone (2B) : PoP (2B) : ShortID (2B) ]   │
│    • Equivalente Global: ej. 0x0001:0x0003:0x0012:0x12F0                               │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1. ¿Cuál es la IP Global y cómo se manejan?

1. **La Dirección Global Ruteable es: `[ASN : Zone : PoP : ShortID]` (8 Bytes):**
   - Es el equivalente exacto a una **IPv6 Global Unicast (`2001:db8::/32`)** o a una IP pública BGP en Internet.
   - Es la que usan los Gateways P4/SBCs para enrutar el paquete a través de enlaces de larga distancia (LoRa, microondas, fibra o VPN).
   - **Ventaja de agregación:** Los routers centrales BGP solo enrutan por `ASN` y `Zone`; no necesitan conocer las millones de IPs individuales de los nodos de campo (*Cero saturación de tablas BGP*).

2. **La Identidad Global del Nodo es su `MAC Física (6B) / UUID IPv4 (4B)`:**
   - Permite que si un nodo se desplaza físicamente de una ciudad a otra (del `PoP 0x0012` al `PoP 0x0099`), su identidad de hardware permanezca invariable.
   - El nodo se conecta al nuevo PoP, obtiene un nuevo **Short ID / Dirección Ruteable** local (`10.0x0099.x.y`), y el Gateway emite una actualización BGP/MeshCore asociando su MAC/UUID a su nueva ubicación.

3. **La Pseudo-IP `10.0.0.0/8` como Capa de Compatibilidad de Aplicaciones:**
   - Dentro de cada ASN/Zona, `10.PoP.MAC_High.MAC_Low` permite abrir un túnel SSH, un socket UDP, un ping ICMP o una terminal web en CBDos directamente usando sockets estándar sin modificar el código de Linux/FreeRTOS.

---

## 3. Direccionamiento Jerárquico: De Local a Federaciones Inter-ASN

El espacio de direcciones emula una red **Pseudo-IP `10.0.0.0/8`** y federaciones BGP divididas en 4 niveles de 2 bytes (16 bits) cada uno:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        MAPA DE JERARQUÍA TOPOLÓGICA DE 8 BYTES                         │
├────────────────────┬────────────────────┬────────────────────┬─────────────────────────┤
│  ASN ID (2 Bytes)  │ Zone ID (2 Bytes)  │  PoP ID (2 Bytes)  │   Short Node ID (2B)    │
│  (Federación BGP)  │  (Región / Estado) │ (Subzona / Gateway)│ (Nodo de Campo Local)   │
│  [65.535 ASNs]     │  [65.535 Zonas]    │  [65.535 PoPs]     │ [65.535 Nodos por PoP]  │
└────────────────────┴────────────────────┴────────────────────┴─────────────────────────┘
```

> **Capacidad Total:** $65.535^4 \approx 1.84 \times 10^{19}$ direcciones únicas sin saturar el aire con cabeceras fijas pesadas.

---

## 3. Estructura de Cabeceras Dinámicas en Vuelo

El primer byte (**Byte de Control**) define el nivel de anidamiento y la longitud total de la cabecera:

```text
Nivel 0: Salto Local (Mismo PoP / Mismo Enlace de Radio Directo)
┌──────────────┬───────────────────┬────────────────────────────────┬─────────────────┐
│ Control (1B) │ Short Node ID (2B)│   Payload Crudo (MeshCore/Data)  │ CRC16 / FCS (2B)│
└──────────────┴───────────────────┴────────────────────────────────┴─────────────────┘
==> Cabecera mínima: 3 Bytes (Máximo ahorro de Airtime)

Nivel 1: Inter-PoP (Cruza entre Gateways dentro de la misma Zona)
┌──────────────┬───────────────────┬──────────────┬───────────────────────────────────┐
│ Control (1B) │ Short Node ID (2B)│  PoP ID (2B) │     Payload Crudo                 │
└──────────────┴───────────────────┴──────────────┴───────────────────────────────────┘
==> Cabecera: 5 Bytes

Nivel 2: Inter-Zona (Cruza regiones dentro del mismo ASN)
┌──────────────┬───────────────────┬──────────────┬──────────────┬────────────────────┐
│ Control (1B) │ Short Node ID (2B)│  PoP ID (2B) │ Zone ID (2B) │   Payload Crudo    │
└──────────────┴───────────────────┴──────────────┴──────────────┴────────────────────┘
==> Cabecera: 7 Bytes

Nivel 3: Inter-ASN / Federaciones BGP Globales
┌──────────────┬───────────────────┬──────────────┬──────────────┬──────────────┬─────┐
│ Control (1B) │ Short Node ID (2B)│  PoP ID (2B) │ Zone ID (2B) │  ASN ID (2B) │ ... │
└──────────────┴───────────────────┴──────────────┴──────────────┴──────────────┴─────┘
==> Cabecera: 9 a 10 Bytes
```

### 3.1. Detalle de Bits del Byte de Control (Byte 0)

| Bits | Campo | Valores | Significado |
| :--- | :--- | :--- | :--- |
| **[7:6]** | **Scope / Alcance** | `00`: Local (PoP) <br> `01`: Inter-PoP <br> `10`: Inter-Zona <br> `11`: Inter-ASN (BGP) | Determina cuántos bytes de cabecera siguen inmediatamente al `Short Node ID`. |
| **[5:4]** | **QoS / Prioridad** | `00`: Baja (Telemetría de fondo) <br> `01`: Normal (Chat/Stream) <br> `10`: Alta (Control/Mando) <br> `11`: Urgente / Alarma | El Gateway P4/SBC selecciona qué antena física usar (FLRC, LoRa o 802.15.4). |
| **[3:2]** | **Flags de Transporte** | `01`: Requiere ACK de Gateway <br> `10`: Paquete Fragmentado <br> `11`: MeshCore Native Frame | Metadatos de control para el router P4. |
| **[1:0]** | **Versión Protocolo** | `00`: Versión 1.0 | Compatibilidad futura. |

---

## 4. Topología y Flujo de Tráfico en Puerta de Enlace (Gateway PoP)

```text
 [ NODO FINAL C3 ]
 (Emite paquete local)
   │
   │ Trama 3 Bytes: [Ctrl: Local | Node: 0x004A | Payload]
   ▼
 📡 [ ANTENA ESP-NOW / C3 BRIDGE ] (USB CDC)
   │
   ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │           GATEWAY LOCAL: ESP32-P4 / SBC ARM64 (PoP 0x0012)             │
 │                                                                        │
 │  1. Recibe trama de 3 bytes por USB.                                   │
 │  2. Consulta Tabla de Enrutamiento BGP / MeshCore:                      │
 │     • Destino final: Nodo en ASN 0x0005, Zona 0x0002, PoP 0x0088.      │
 │  3. ENCAPSULADO DINÁMICO:                                              │
 │     • Reescribe Byte de Control a Scope = Inter-ASN (`11`).            │
 │     • Agrega los 6 bytes de ruta [PoP:0x0088 | Zone:0x0002 | ASN:0x0005]│
 │  4. Selección de Salida:                                               │
 │     • Despacha el paquete de 9 bytes por el Módem LoRa Sub-GHz o WAN.  │
 └───────────────────────────────────┬────────────────────────────────────┘
                                     │
                                     │ Trama de 9 Bytes (Inter-ASN)
                                     ▼
                      📡 [ ENLACE DE LARGA DISTANCIA ]
                      (LoRa 915 MHz / Microondas / Fibra)
                                     │
                                     ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │        GATEWAY DESTINO: ESP32-P4 / SBC ARM64 (PoP 0x0088)              │
 │                                                                        │
 │  1. Recibe trama Inter-ASN de 9 bytes.                                 │
 │  2. Comprueba que el ASN, Zona y PoP coinciden con su propia ID local. │
 │  3. DESENCAPSULADO:                                                    │
 │     • Retira los 6 bytes externos [ASN, Zone, PoP].                    │
 │     • Reescribe el Byte de Control a Scope = Local (`00`).             │
 │  4. Entrega la trama limpia de 3 bytes al ESP32-C3 transmisor local.   │
 └───────────────────────────────────┬────────────────────────────────────┘
                                     │
                                     │ Trama 3 Bytes (Local)
                                     ▼
                        [ NODO DESTINO FINAL C3 ]
```

---

## 5. Mapeo de Identidad a Topología Jerárquica (Pseudo-ARP / Node-ARP)

1. **Identidad Nativa de Nodo en CBDos:**
   - **MAC Física (6 Bytes):** Identificador universal de fábrica del transceptor.
   - **UUID / Dirección IPv4 Mesh (4 Bytes):** Formato `10.Zona.PoP.Nodo` (32 bits) o `MAC[2..5]`.
2. **Tabla de Resolución Topológica en Gateways (Pseudo-ARP):**
   - Los Gateways (P4/SBCs) mantienen una tabla en RAM/PSRAM que asocia los nodos activos con su ubicación topológica en la malla:
     ```text
     ┌──────────────────────┬──────────────────────┬──────────────────────────────────────┐
     │ MAC Física (6 Bytes) │ UUID / IPv4 (4 Bytes)│ Dirección Topológica (ASN:Zone:PoP:ID)│
     ├──────────────────────┼──────────────────────┼──────────────────────────────────────┤
     │ 24:DC:C3:4A:12:F0    │ 10.1.18.240          │ 0x0001 : 0x0003 : 0x0012 : 0x004A    │
     │ 30:AE:A4:05:88:01    │ 10.5.2.1             │ 0x0005 : 0x0002 : 0x0088 : 0x0001    │
     └──────────────────────┴──────────────────────┴──────────────────────────────────────┘
     ```
3. **Mapeo Pseudo-IP (`10.0.0.0/8`):**
   - Si se requiere compatibilidad con herramientas IP convencionales (Ping, SSH, sockets BSD):
     - `10.[Zone].[PoP].[NodeID]` representa la dirección IPv4 virtual del host.
     - El gateway P4 hace traducción NAT/ARP instantánea hacia la cabecera dinámica de radio.

---

## 6. Autoconfiguración de Nodos (MAC ➔ Short ID), DAD y Gateway por Defecto (0x0000)

### 6.1. Generación del Short Node ID desde la MAC
Para que los nodos ESP32-C3 satélites operen de forma **Zero-Config (Plug & Play)**:
1. El nodo toma los últimos 2 bytes de su dirección MAC física de fábrica:
   $$\text{Short ID Inicial} = \text{MAC}[4] \ll 8 \mid \text{MAC}[5]$$
2. **Pseudo-IP Local Automática:** `10.[PoP_Local].[MAC_High].[MAC_Low]`.

### 6.2. DAD (Duplicate Address Detection - Detección de Colisiones)
- Al encenderse, el nodo emite un broadcast local de sondeo (Probe DAD) con su Short ID derivado.
- Si el Gateway P4 o algún nodo existente en el PoP responde indicando colisión:
  - El Gateway P4 le asigna dinámicamente un ID libre en el PoP (mediante un mensaje rápido de asignación de 4 bytes) o el nodo aplica un hash pseudoaleatorio $CRC16(\text{MAC} + \text{Seed})$.
- El Gateway P4 actualiza de inmediato su **Tabla Pseudo-ARP**:
  `[Short ID Local (2B)] ⟷ [UUID IPv4 (4B)] ⟷ [MAC Física (6B)]`.

### 6.3. Modos de Direccionamiento hacia el Gateway (Pros y Contras)

Para que los nodos satélites envíen tráfico hacia el exterior del PoP (Inter-PoP / Inter-Zona / Inter-ASN), se definen y comparan tres estrategias de direccionamiento en los 2 bytes de destino:

```text
┌───────────────────────────────────────────────────────────────────────────────────────┐
│                      MODOS DE DIRECCIONAMIENTO AL GATEWAY                             │
├──────────────────────────────┬─────────────────────────────┬──────────────────────────┤
│ Modo A: Virtual Anycast      │ Modo B: Unicast Fijo        │ Modo C: Híbrido Dinámico │
│         (0x0000)             │         (0x0001 / MAC)      │   (0x0000 -> 0x0001)     │
└──────────────────────────────┴─────────────────────────────┴──────────────────────────┘
```

#### 🅰️ Modo A: Dirección Virtual Anycast (`0x0000`)
El nodo satélite siempre envía todos los paquetes destinados al exterior al ID genérico `0x0000`. Cualquier Gateway que escuche el paquete lo captura y lo enruta.

* **👍 Pros:**
  - **Cero configuración:** El nodo no necesita descubrir ni guardar la ID de ningún Gateway.
  - **Redundancia instantánea (Failover automático):** Si el Gateway primario cae y hay un Gateway secundario de respaldo en la misma zona, este toma el tráfico de inmediato sin interrupción de servicio.
  - **Movilidad transparente:** Un nodo móvil que cambia de PoP sigue transmitiendo a `0x0000` y es atendido por el Gateway más cercano.
* **👎 Contras:**
  - **Riesgo de duplicación de tramas:** Si dos Gateways están en el mismo canal RF, ambos capturarán el paquete y lo inyectarán a la red externa (requiere deduplicación por `Nonce` o `SeqNum` en el router P4).
  - **ACKs promiscuos:** No hay ACK a nivel de enlace unicast dedicado, salvo que el Gateway responda con una trama de confirmación explícita.

#### 🅱️ Modo B: Unicast Estricto / ID Fija (`0x0001` o 2 Bytes de MAC)
El Gateway se autoasigna una dirección fija conocida en cada PoP (por convención `0x0001`, análogo a la IP `.1` de un router `10.x.y.1`) o su ID derivada de MAC. El nodo le envía exclusivamente a esa ID.

* **👍 Pros:**
  - **Cero duplicaciones:** Solo el Gateway especificado procesa la trama.
  - **ACKs por Hardware a Nivel MAC:** Protocolos como ESP-NOW o 802.15.4 pueden emitir el ACK automático por hardware de capa 2 al coincidir la ID/MAC exacta.
  - **Control de métricas de enlace:** El nodo conoce el RSSI/SNR exacto de su Gateway primario y puede medir pérdida de paquetes con precisión.
* **👎 Contras:**
  - **Acoplamiento:** Si el Gateway `0x0001` se apaga o sufre interferencias, el nodo queda aislado hasta que cambie manualmente o reciba un nuevo anuncio.

#### 🅲 Modo C: Esquema Híbrido Dinámico (Recomendado por CBDos)
Combina la robustez del Anycast en el arranque con la eficiencia del Unicast durante la operación normal:

1. **Arranque / Descubrimiento:** El nodo satélite recién encendido envía una solicitud de registro (*PoP Discovery Probe*) a la dirección Anycast `0x0000`.
2. **Respuesta del Gateway:** El Gateway más cercano responde con un *Gateway Announcement* indicando su ID real (`0x0001` o ID derivada de su MAC), el `PoP_ID` local y los canales disponibles.
3. **Operación Normal:** El nodo almacena la ID del Gateway en RAM y conmuta a **Unicast directo** a esa ID para beneficiarse de ACKs por hardware y cero duplicaciones.
4. **Fallback Reactivo por Pérdida de Enlace:** Si tras $N$ reintentos (ej. 3 fallos consecutivos de ACK) el Gateway no responde, el nodo satélite invalida la ID en RAM y regresa automáticamente al modo Anycast `0x0000` para descubrir un nuevo Gateway disponible.
5. **Reemplazo Rápido Proactivo / Hot-Swap (Gratuitous Gateway Takeover):**
   - Si un Gateway principal se daña o se apaga y encendemos un nuevo Gateway de reemplazo (otro ESP32-P4 o SBC ARM64), este emite inmediatamente un broadcast local:
     `[Control: Broadcast | Code: GATEWAY_TAKEOVER | New_Gateway_ShortID: 0x0042 | PoP_ID: 0x0012]`
   - Todos los nodos satélites del PoP que escuchan este anuncio **actualizan en caliente su puntero de Gateway al instante** hacia `0x0042`, redirigiendo todo el tráfico hacia el nuevo Gateway sin necesidad de esperar fallos ni reiniciar ningún nodo de campo.

---

### 6.4. Matriz Comparativa de Estrategias de Gateway

| Criterio de Evaluación | Modo A: Anycast (`0x0000`) | Modo B: Unicast Fijo (`0x0001`) | Modo C: Híbrido Dinámico (CBDos) |
| :--- | :--- | :--- | :--- |
| **Sobrecarga de Tráfico Inicial** | **Nula (0 paquetes)** | Nula (Asume `0x0001`) | Mínima (1 paquete Probe inicial) |
| **Eficiencia Energética en Nodos** | Media (Sin ACK hardware) | **Máxima (ACK hardware L2)** | **Máxima (ACK hardware L2)** |
| **Resiliencia ante Caída de Gateway** | **Instantánea (0 ms)** | Nula (Requiere intervención) | **Alta (Fallback tras 3 reintentos)** |
| **Inmunidad a Paquetes Duplicados** | Baja (Requiere filtro en P4) | **Absoluta** | **Absoluta** |
| **Complejidad de Firmware en C3** | **Ultra Baja (~50 líneas)** | Baja | Media-Baja |

---

---

## 7. Resolución de Movilidad y Servidores de Ubicación (Pseudo-DDNS / LISP Map-Server / DHT)

### 7.1. El Desafío de la Movilidad
Cuando un usuario/nodo con **Identidad Fija** (MAC física o IPv4 Mesh fija `10.Zona.PoP.Nodo`) se traslada físicamente a otra Zona o ASN:
- Su dirección ruteable cambia (ej. de `0x0001:0x0002:0x0012:0x12F0` a `0x0005:0x0001:0x0088:0x12F0`).
- **Problema:** Los routers de campo y nodos satélites no pueden almacenar millones de entradas dinámicas de cada persona que se mueve en el mundo porque saturaría la RAM y el ancho de banda de radio.

---

### 7.2. Arquitectura de Resolución de Ubicación: LISP Map-Server / Pseudo-DDNS

Para resolver este problema con cero saturación de radio, se implementa una capa de **Servidores de Mapeo de Ubicación (Map-Servers / Location Directory)** federados:

```text
                               ┌────────────────────────────────────────────────────────┐
                               │  SERVIDORES DE UBICACIÓN FEDERADOS (Map-Servers / DDNS)│
                               │  - Ubicados en Gateways P4 / SBCs / Servidores WAN     │
                               │  - Base de Datos Distribuida (Kademlia DHT / SQLite)   │
                               └─────────────────────────▲──────────────────────────────┘
                                                         │
                     1. Registro al entrar a nuevo PoP   │  2. Consulta de Ubicación
                        "Alice (MAC/10.x.y.z) está       │     "¿Dónde está Bob (MAC_Bob)?"
                         ahora en ASN:5, Zone:1, PoP:88" │     Respuesta: "Está en ASN:2, Zone:4, PoP:15"
                                                         │
                     ┌───────────────────────────────────┴───────────────────────────────────┐
                     │                                                                       │
      ┌──────────────┴──────────────┐                                         ┌──────────────┴──────────────┐
      │  GATEWAY P4 / SBC ORIGEN    │                                         │  GATEWAY P4 / SBC DESTINO   │
      │  (PoP 0x0088 / ASN 0x0005)  │ ══════════════════════════════════════> │  (PoP 0x0015 / ASN 0x0002)  │
      ├─────────────────────────────┤     3. Tráfico Directo Ruteable         ├─────────────────────────────┤
      │ • Guarda en Cache Local     │        [ASN:2|Zone:4|PoP:15|ShortID]    │ • Entrega a Bob por radio   │
      └──────────────▲──────────────┘                                         └──────────────▲──────────────┘
                     │                                                                       │
           (Trama 3B │ Local)                                                      (Trama 3B │ Local)
                     ▼                                                                       ▼
             [ ALICE (Nodo C3) ]                                                     [ BOB (Nodo C3) ]
             MAC: 24:DC:C3:4A:12:F0                                                  MAC: 30:AE:A4:05:88:01
```

---

### 7.3. Flujo Paso a Paso de una Comunicación Inter-Zona / Inter-ASN

#### Paso 1: Registro en Caliente al Llegar a un Nuevo PoP (Update)
1. Alice llega a una nueva ciudad y su nodo C3 se asocia al Gateway P4 del `PoP 0x0088`.
2. El Gateway P4 detecta la MAC de Alice y envía un paquete ligero de registro a su Map-Server asignado:
   `[MAP_REGISTER | MAC: 24:DC:C3:4A:12:F0 | IP_Fija: 10.1.18.240 | Location: ASN=5, Zone=1, PoP=88, ShortID=0x12F0]`
3. El Map-Server actualiza su entrada con un tiempo de expiración (TTL ej. 2 horas).

#### Paso 2: Consulta de Ubicación por Demanda (Map-Request)
1. Bob (en otro ASN) quiere enviarle un mensaje o abrir un socket hacia la IP fija o MAC de Alice.
2. El Gateway P4 de Bob no tiene a Alice en su caché local:
   - Envía un paquete ultra-compacto al Map-Server más cercano:
     `[MAP_REQUEST | Target_MAC: 24:DC:C3:4A:12:F0]`
3. El Map-Server responde al Gateway de Bob:
   `[MAP_REPLY | Target_MAC: 24:DC:C3:4A:12:F0 | Route: ASN:5, Zone:1, PoP:88, ShortID:0x12F0 | TTL: 3600s]`

#### Paso 3: Comunicación Directa Ruteada (Data Flow)
1. El Gateway de Bob almacena la ruta en su **Caché de Enrutamiento (Fast-Path Table)** en PSRAM.
2. A partir de ese instante, todos los paquetes de Bob hacia Alice se encapsulan directamente con los 8 bytes `[0x0005:0x0001:0x0088:0x12F0]` **sin volver a consultar al servidor**.
3. El tráfico fluye directamente de Gateway a Gateway por el camino más corto.

---

### 7.4. Modos de Implementación de los Map-Servers

| Nivel de Red | Mecanismo del Servidor | Implementación Técnica | Caso de Uso |
| :--- | :--- | :--- | :--- |
| **Red Offline / Local Mesh** | **Distributed Hash Table (DHT Kademlia)** | Embebido en la PSRAM de los ESP32-P4 / SBCs. | Comunidades aisladas sin internet ni servidores centrales. |
| **Red Federada / Híbrida** | **LISP Map-Server / Pseudo-DDNS** | Microservicio en C++/Go corriendo en VPS, Raspberry Pi o Gateways P4 de cabecera. | Redes amplias con enlaces de radio punto a punto + túneles IP. |
| **Red de Malla Pura (MeshCore)** | **Announcements con Path Propagation** | Paquetes de baliza nativos de MeshCore con Rate-Limiting. | Enrutamiento reactivo/proactivo en redes ad-hoc aisladas. |

---

## 8. Ventajas Clave del Esquema Global

1. **Eficiencia Máxima de Espectro:** Los enlaces locales de alta densidad no sufren por cabeceras globales gigantescas; la cabecera solo crece cuando el paquete realmente viaja lejos.
2. **Nodos de Campo Baratos y Eficientes:** Los ESP32-C3 satélites no necesitan memoria RAM para tablas gigantes de enrutamiento ni procesamiento de grafos de red.
3. **Escalabilidad Global Federada:** Múltiples comunidades, ciudades o redes tácticas pueden crear sus propios ASNs (sistemas autónomos) de 2 bytes y hacer *peering* mutuo sin colisión de direcciones.
4. **Cero Configuración Manual (Zero-Touch Deployment):** Los nodos derivan su ID de su MAC, resuelven colisiones mediante DAD y gestionan la salida al exterior mediante el esquema híbrido Anycast/Unicast sin intervención humana.
5. **Movilidad Transparente sin Saturación:** Los servidores de ubicación (Map-Servers / DHT) resuelven la IP ruteable bajo demanda; las tablas de los nodos nunca se llenan y el tráfico viaja directo una vez resuelto.

