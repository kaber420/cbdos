# 📡 Especificación Técnica & Documento de Investigación: Red Malla, Descubrimiento Multicanal y Gestión Multi-Radio para CBDos

**Versión:** 1.0.0 (RFC-CBDOS-MESH-01)  
**Estado:** Propuesta de Arquitectura Formal  
**Autores:** Equipo de Arquitectura CBDos  
**Fecha:** Agosto 2026  

---

## 🏛️ 1. Resumen Ejecutivo y Objetivos

El objetivo de esta especificación es definir una arquitectura de red en malla descentralizada, robusta y determinista para **CBDos** que resuelva los siguientes desafíos fundamentales:

1. **Descubrimiento y Escaneo Multicanal (*Channel Hopping & Passive/Active Scanning*):** Descubrir torres y nodos sin fijar canales de forma estática (*hardcodeo*) y sin saturar el espectro de radio.
2. **Gestión de Perfiles y Credenciales Portables en MessagePack (`networks.msgpack`):** Almacenar y conmutar perfiles de redes (**CBDos Alternet**, **MeshCore**, **Meshtastic**, etc.) en un formato binario compacto y exportable.
3. **Matriz Multi-Radio y Abstracción de Enlace (HAL):** Operar fluidamente sobre **ESP-NOW Normal** (2.4 GHz @ 1 Mbps), **ESP-NOW Long Range** (802.11 LR @ 250 kbps), **SX1280 FLRC** (2.4 GHz @ 1.3 Mbps) y **SX1262 LoRa** (Sub-GHz @ 915/868 MHz).
4. **Algoritmo Adaptativo de Calidad de Enlace (*LQI & Link-State Probing*):** Selección automática (`AUTO`) basada en métricas físicas reales (RSSI, SNR, ToA, tasa de pérdida de paquetes).
5. **Autenticación Ligera y Handshake de Asociación a Torres (*Challenge-Response 3B-Auth*).**

---

## 📻 2. Capa Física y Modulaciones: Matriz de Radios

CBDos opera sobre múltiples capas físicas heterogéneas. Cada radio impone restricciones de MTU, velocidad y tiempo en el aire (*Time-on-Air - ToA*):

| Radio / Protocolo | Banda | Ancho de Banda | Tasa de Datos | MTU Máx | MTU Útil CBDos | Rango Típico | Sensibilidad | ToA por Fragmento |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ESP-NOW Normal** | 2.4 GHz | 20 MHz (OFDM/DSSS) | 1 Mbps - 2 Mbps | 250 B | **240 B** | ~80 - 120 m | -93 dBm | $\approx 1.8 \text{ ms}$ |
| **ESP-NOW LR** | 2.4 GHz | 20 MHz (DSSS CCK-LR) | 250 kbps / 500 kbps | 250 B | **240 B** | ~800 m - 1.2 km | -98 dBm | $\approx 7.2 \text{ ms}$ |
| **SX1280 (FLRC)** | 2.4 GHz | 1.2 MHz (GMSK/BT0.5) | **1.3 Mbps** / 1.04 Mbps | 128 / 255 B | **120 B** | ~3 - 8 km | -108 dBm | $\approx 0.9 \text{ ms}$ |
| **SX1262 (LoRa)** | 915 MHz | 250 kHz / 500 kHz | 10 kbps - 21 kbps (SF7-SF9)| 255 B | **80 B** | **> 15 km** | -130 dBm | $\approx 45 \text{ ms}$ |

---

## 🔍 3. Protocolo de Descubrimiento y Escaneo Multicanal

Para evitar el bloqueo en un único canal Wi-Fi, se define un mecanismo dual de escaneo: **Pasivo (*Beacon Sniffing*)** y **Activo (*Probe Request Sweep*)**.

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        ALGORITMO DE BARRIDO MULTICANAL (SWEEP)                         │
│                                                                                        │
│   Canal 1        Canal 6        Canal 11       Canales 2..5, 7..10, 12..13             │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌───────────────────────────┐            │
│  │ Dwell   │───▶│ Dwell   │───▶│ Dwell   │───▶│ Dwell Breve (15 ms c/u)   │───▶ FIN    │
│  │  35 ms  │    │  35 ms  │    │  35 ms  │    │                           │            │
│  └─────────┘    └─────────┘    └─────────┘    └───────────────────────────┘            │
│       │              │              │                       │                          │
│       ▼              ▼              ▼                       ▼                          │
│  [Emitir Probe  [Emitir Probe  [Emitir Probe          [Escuchar Beacons               │
│   y capturar]    y capturar]    y capturar]            en segundo plano]               │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### 3.1. Trama de Sondeo Activo (*Tower Probe Request*)
El cliente emite una micro-trama de 12 Bytes en broadcast (`FF:FF:FF:FF:FF:FF`):

```
┌───────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ MicroChunk│ Control Byte │ Dst Short ID │ Tag Servicio │ Client Short │ Client Caps  │
│  (2 Bytes)│   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │   (2 Bytes)  │
├───────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x01 0xAA │ 0x4F (Signal)│ 0xFFFF (Bcast│ 0x01 (PROBE) │ 0xXXXX       │ [Norm|LR|LoRa│
└───────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

### 3.2. Trama de Respuesta de Torre (*Tower Announcement / Probe Response*)
La torre o nodo de enlace responde con sus parámetros operativos:

```
┌───────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ MicroChunk│ Control Byte │ Dst Short ID │ Tag Servicio │ Tower ID     │ Canal & Mod  │ Longitud Nom │ Nombre Torre │
│  (2 Bytes)│   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │   (2 Bytes)  │   (1 Byte)   │ (N Bytes <=24│
├───────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x01 0xBB │ 0x4F (Signal)│ Client Short │ 0x02 (ANNOUN)│ 0x0001 (ASN1)│ Ch:1 | LR:1  │ 0x12 (18B)   │ "Torre Valle"│
└───────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 🗂️ 4. Estructura de Perfiles de Red en MessagePack (`networks.msgpack`)

Para desacoplar la configuración de la memoria volátil o claves planas de NVS, el sistema utiliza perfiles estructurados en formato **MessagePack**:

```json
[
  {
    "uuid": "a1b2c3d4-e5f6-7890-1234-56789abcdef0",
    "type": "cbdos_alternet",
    "name": "Red Comunitaria Valle",
    "asn": 1,
    "zone": 10,
    "tower_id": 1,
    "auth": {
      "type": "token_hmac",
      "token": "4f9b2d8e7a1c3e5f"
    },
    "allowed_radios": ["espnow", "espnow_lr", "flrc"],
    "preferred_channel": 1,
    "auto_hop": true,
    "priority": 100
  },
  {
    "uuid": "b2c3d4e5-f6a7-8901-2345-67890abcdef1",
    "type": "meshtastic_compat",
    "name": "Canal Comunitario LoRa 915",
    "frequency_mhz": 915.0,
    "bandwidth_khz": 250,
    "spreading_factor": 11,
    "coding_rate": 5,
    "channel_psk": "AQ==",
    "priority": 40
  }
]
```

---

## 🤖 5. Algoritmo de Decisión y Selección `AUTO`

Cuando el cliente opera en modo `RadioMode::Auto`, el motor `MeshEngine` evalúa periódicamente el estado de la comunicación y conmuta la modulación según la siguiente máquina de estados:

```
                            ┌────────────────────────┐
                            │    ESTADO INICIAL:     │
                            │    BÚSQUEDA AUTO       │
                            └───────────┬────────────┘
                                        │
                         Barrido Multicanal (1, 6, 11)
                                        │
                 ┌──────────────────────┴──────────────────────┐
                 ▼                                             ▼
       [ Torre Encontrada ]                          [ Sin Torres Cerca ]
                 │                                             │
                 ├───────────────────────────────┐             ▼
                 ▼                               ▼      Activar ESP-NOW LR
         RSSI > -70 dBm                  RSSI <= -72 dBm (+20dBm) Modo Baliza
                 │                               │
                 ▼                               ▼
       Fijar ESP-NOW Normal             Fijar ESP-NOW LR
        (1 Mbps, 240B MTU)              (250 kbps, +20dBm)
                 │                               │
                 └──────────────┬────────────────┘
                                │
                        Monitoreo LQI:
                 ¿Tasa de pérdida > 20%?
                 ├── Sí  ──▶ Degradar a LR / FLRC
                 └── No  ──▶ Mantener enlace
```

---

## 🔐 6. Handshake de Asociación y Seguridad (3-Byte Challenge)

Para evitar la suplantación de nodos (*MAC spoofing* o *ID collision*):

1. **Paso 1 (Cliente $\to$ Torre):** Envía `[ProbeReq]` con su `Short ID` temporal.
2. **Paso 2 (Torre $\to$ Cliente):** Responde con `[Challenge Nonce]` (4 Bytes aleatorios).
3. **Paso 3 (Cliente $\to$ Torre):** Envía `[AuthResponse]`:
   $$\text{Signature} = \text{Truncate3B}(\text{HMAC-SHA256}(\text{Token}, \text{Nonce} \parallel \text{ShortID}))$$
4. **Paso 4 (Torre):** Valida la firma en $< 1 \text{ ms}$ e inscribe el `Short ID` en la tabla **Pseudo-ARP** con tiempo de vida (*Lease Time*) de 300 segundos.

---

## 📋 7. Plan de Implementación por Fases

- [ ] **Fase A (HAL & Canales):** Implementar selector de canales $1..13$ y barrido multicanal (*Channel Hopping Sweep*) en `hal_mesh_s3.cpp` y `hal_mesh_p4.cpp`.
- [ ] **Fase B (Parser MessagePack):** Implementar la carga y persistencia de perfiles `networks.msgpack` en `core/src/mesh/MeshProfileManager.cpp`.
- [ ] **Fase C (Handshake & Sondeo):** Estandarizar las tramas `ProbeReq / ProbeResp` en todos los gateways y dongles.
- [ ] **Fase D (UI / Experiencia de Usuario):** Integrar la lista dinámica de escaneo con indicador de intensidad en dBm y selección de canal en `MeshConfigView`.
