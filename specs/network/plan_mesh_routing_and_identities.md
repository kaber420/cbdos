# 🗺️ Plan de Arquitectura: Ruteo Jerárquico por Capas y Asignación Dinámica de Cabeceras

Este documento describe el modelo de **Enrutamiento Jerárquico Asimétrico (Pseudo-BGP / Pseudo-OSPF / Pseudo-ARP)** optimizado para operar tanto en computadoras de placa reducida (SBC) como en microcontroladores de alto rendimiento (**ESP32-P4 con radio FLRC SX1280**).

---

## 🏛️ 1. Principio Fundamental: Inteligencia en la Torre, Simplicidad en el Cliente

El nodo cliente (tu dispositivo portátil CBDos o sensor) **no procesa tablas de ruteo pesadas ni calcula rutas globales**. Su trabajo es hablar únicamente con su Torre local con la menor cantidad de bytes posible.

```
┌─────────────────────────┐
│     CLIENTE FINAL       │ ─── Envía SIEMPRE en 3 Bytes ──▶ ┌─────────────────────────────────┐
│   (ESP32-P4 / S3 / C3)  │                                   │          ROUTER / TORRE         │
│                         │ ◀── Recibe SIEMPRE en 3 Bytes ── │  (ESP32-P4 @ 400MHz / SBC Linux)│
└─────────────────────────┘                                   └────────────────┬────────────────┘
                                                                               │
                                                       ¿Hacia dónde va el paquete?
                                                                               │
                 ┌─────────────────────────────────────────────────────────────┼─────────────────────────────────────────┐
                 ▼                                                             ▼                                         ▼
   ┌───────────────────────────┐                                 ┌───────────────────────────┐             ┌───────────────────────────┐
   │    NIVEL 1: LOCAL         │                                 │    NIVEL 2: INTRA-ZONA    │             │    NIVEL 3: PSEUDO-BGP    │
   │    (Misma Torre / ESP-NOW)│                                 │    (Entre Torres - OSPF)  │             │    (Inter-ASN / Frontera) │
   │                           │                                 │                           │             │                           │
   │ • Cabecera: 3 Bytes       │                                 │ • Cabecera: 9 Bytes       │             │ • Cabecera: 21 Bytes      │
   │ • Control + ShortID Dst   │                                 │ • Control + Torre Src/Dst │             │ • Lee SOLO 2 Bytes de ASN │
   │ • Cero cálculo de ruta    │                                 │   + ShortIDs de zona      │             │ • Despacho en < 25 ns     │
   └───────────────────────────┘                                 └───────────────────────────┘             └───────────────────────────┘
```

---

## ⚡ 2. Eficiencia Extrema de Pseudo-BGP en un ESP32-P4

El enrutador de frontera (Pseudo-BGP) no requiere un servidor grande; un **ESP32-P4 (Dual-Core RISC-V @ 400 MHz)** con un módulo de radio **Semtech SX1280 (FLRC @ 1.3 Mbps)** y antena direccional (Yagi / Parabólica) puede actuar como un **nodo de frontera inter-ASN de larguísimo alcance (> 10 km)**:

```
Paquete entrante de 21 Bytes (Inter-ASN):
┌──────────┬──────────┬──────────┬──────────┬──────────┬─────────────────────────────┐
│ Control  │ ASN Dst  │ Zona Dst │ Torre Dst│ UUID Dst │     Dirección Origen...     │
│   1B     │    2B    │    2B    │    2B    │    4B    │            10B              │
└──────────┴──────────┴──────────┴──────────┴──────────┴─────────────────────────────┘
                ▲
                │ El Router BGP de frontera lee ÚNICAMENTE estos 2 Bytes de ASN Destino
```

### ¿Por qué es ultra-rápido en un ESP32-P4?
1. **Decisión en O(1) (< 25 nanosegundos):**
   - El router lee los bytes `[1..2]` (`ASN Dst`).
   - Hace una búsqueda directa en un array plano en RAM `next_hop = bgp_table[asn_dst]`.
   - Reenvía el paquete por DMA hacia la radio SX1280 direccional del siguiente salto.
2. **Origen Opaco e Ignorado:**
   - La dirección de origen no se parsea ni se toca durante el tránsito normal.
   - **Solo en caso de error** (ej. ruta inalcanzable / TTL expirado), el router lee la dirección de origen para enviar un paquete de señalización de error de vuelta.

---

## 🌲 3. Pseudo-OSPF (Nivel 2: Carga Mediana Intra-Zona)

Dentro de una misma Zona (comunidad, valle o ciudad con múltiples torres):
- La cabecera se mantiene en **9 Bytes** (`[Ctrl: 1B][Torre Src: 2B][Torre Dst: 2B][ShortID Src: 2B][ShortID Dst: 2B]`).
- Los routers de torre mantienen el estado de enlaces (*Link-State*) y calculan la ruta más corta entre torres de la zona.

---

## 📱 4. Última Milla Local (Nivel 1: Cabecera de 3 Bytes en ESP-NOW / Wi-Fi)

- Cuando el paquete llega a la torre donde está el usuario final:
  - La torre retira la cabecera BGP (21B) o de Zona (9B).
  - Consulta su **Tabla Pseudo-ARP local** en RAM (`UUID -> Short ID`).
  - Emite el paquete por **ESP-NOW** al dispositivo del usuario con la cabecera ultra-corta de **3 Bytes** (`[Control][Short ID Dst]`).
- **El cliente CBDos recibe el payload íntegro y renderiza a 60 FPS sin sufrir overhead.**

---

## 🪪 5. Resumen de Roles de Hardware

| Rol en la Red | Hardware Ideal | Tipo de Enlace | Capa de Protocolo |
| :--- | :--- | :--- | :--- |
| **Cliente / Terminal** | ESP32-P4 / S3 con pantalla CBDos | ESP-NOW (2.4 GHz) / Wi-Fi | Local (3 Bytes) |
| **Torre Local / Gateway** | ESP32-S3 / C3 Dongle + PC o Raspberry Pi | ESP-NOW / UART | Local (3B) $\leftrightarrow$ Pseudo-ARP |
| **Router de Zona** | ESP32-P4 / ESP32-S3 | SX1280 FLRC (1.3 Mbps) | Pseudo-OSPF (9 Bytes) |
| **Router de Frontera BGP** | ESP32-P4 con Antena Direccional / SBC | SX1280 FLRC / LoRa 2.4G | Pseudo-BGP (21 Bytes, lectura 2B) |
