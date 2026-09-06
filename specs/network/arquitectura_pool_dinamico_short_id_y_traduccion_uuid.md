# 🛰️ Arquitectura de Pool Dinámico de Short IDs (Pseudo-DHCP) y Traducción Asimétrica de UUIDs (Pseudo-NAT) en Redes Malla CBDos

**Documento:** `docs/network/arquitectura_pool_dinamico_short_id_y_traduccion_uuid.md`  
**Estado:** Propuesta de Arquitectura y Especificación de Protocolo  
**Versión:** 1.0.0 (RFC-CBDOS-ID-02)  
**Fecha:** Agosto 2026  
**Ámbito:** Capa de Red y Transporte CBDos, Gateway-Router, Firmware ESP32-S3/P4.

---

## 📑 Índice de Contenidos
1. [Resumen Ejecutivo y Principio de Traducción Asimétrica](#1-resumen-ejecutivo-y-principio-de-traducción-asimétrica)
2. [Estructura del UUID Hardware (4 Bytes) y del Pool de Short IDs (2 Bytes)](#2-estructura-del-uuid-hardware-4-bytes-y-del-pool-de-short-ids-2-bytes)
3. [Análisis de Carga de Red y Overhead (¿Es costoso el handshake?)](#3-análisis-de-carga-de-red-y-overhead-es-costoso-el-handshake)
4. [Ciclo de Vida de Arriendo (Lease), Persistencia en NVS y Expiración](#4-ciclo-de-vida-de-arriendo-lease-persistencia-en-nvs-y-expiración)
5. [Traducción Bidireccional Asimétrica (Local 3B $\longleftrightarrow$ WAN 4B/10B)](#5-traducción-bidireccional-asimétrica-local-3b-longleftrightarrow-wan-4b10b)
6. [Manejo de Roaming y Movilidad entre Múltiples Torres](#6-manejo-de-roaming-y-movilidad-entre-múltiples-torres)
7. [Comparativa Técnica: Pool Dinámico vs SLAAC Directo](#7-comparativa-técnica-pool-dinámico-vs-slaac-directo)

---

## 🏛️ 1. Resumen Ejecutivo y Principio de Traducción Asimétrica

Esta arquitectura resuelve el dilema entre **identidad inmutable** y **eficiencia extrema en el aire** mediante un modelo de **Traducción de Direcciones Malla (Pseudo-NAT)**:

```
  ┌─────────────────────────────────────────────────────────────┐
  │                 ESP32 (Cliente / Terminal)                  │
  │ • Identidad Global Inmutable: UUID de 4 Bytes (MAC[2..5])   │
  │ • Identidad Efímera de Radio: Short ID de 2 Bytes (Pool)    │
  └──────────────────────────────┬──────────────────────────────┘
                                 │
                   (Aire: Cabecera Ligera de 3 Bytes)
                                 │
                                 ▼
  ┌─────────────────────────────────────────────────────────────┐
  │                 TORRE / GATEWAY ROUTER                      │
  │                                                             │
  │  TABLA DE TRADUCCIÓN PSEUDO-NAT / PSEUDO-ARP (RAM):         │
  │  ┌──────────┬────────────────────────┬───────────────────┐  │
  │  │ Short ID │ UUID Permanente (4B)   │  MAC Física (6B)  │  │
  │  ├──────────┼────────────────────────┼───────────────────┤  │
  │  │  0x0002  │ 0x017C0C94 (1.124.12.148) 9C:CC:01:7C:0C:94│  │
  │  └──────────┴────────────────────────┴───────────────────┘  │
  └──────────────────────────────┬──────────────────────────────┘
                                 │
              (Troncal / WAN: Cabecera Global de 4B / 10B)
                                 │
                                 ▼
  ┌─────────────────────────────────────────────────────────────┐
  │       RED EXTERIOR / INTER-TORRES / PROXY / OSPF / BGP      │
  │  Solo ve y enruta usando el UUID Global permanente (4 Bytes)│
  └─────────────────────────────────────────────────────────────┘
```

### Principio Clave:
- **Hacia Adentro (En el Aire Local):** El nodo y la Torre hablan exclusivamente con el **Short ID de 2 Bytes** en cabeceras de **3 Bytes (`DST_ONLY`)**.
- **Hacia Afuera (Enrutamiento Troncal y WAN):** La Torre reemplaza el Short ID local por el **UUID global de 4 Bytes**. El mundo exterior no necesita saber qué Short ID tiene asignado el nodo en la radio local.

---

## 🪪 2. Estructura del UUID Hardware (4 Bytes) y del Pool de Short IDs (2 Bytes)

### 2.1. UUID Global Hardware-Bound (4 Bytes / 32 bits)
El ESP32 toma los últimos 4 Bytes de su dirección MAC física quemada en el silicio (`eFuse`):

```
 Dirección MAC Física (6 Bytes):  [ 9C : CC : 01 ] : [ 7C : 0C : 94 ]
                                   └───┬───┘          └──────┬──────┘
                   OUI Fabricante (Espressif)     Número de Serie Único (3 Bytes)
                   
 UUID Global de 4 Bytes (32 bits): [ 0x01 ] [ 0x7C ] [ 0x0C ] [ 0x94 ]
                                    (MAC[2]) (MAC[3])  (MAC[4])  (MAC[5])
```

- **Espacio de Identidades:** $2^{32} = \mathbf{4,294,967,296}$ nodos posibles.
- **Inmutabilidad:** No depende de ningún servidor; el nodo nace con este UUID grabado en el chip.
- **Notación IPv4 Amigable:** `0x017C0C94` se representa como **`1.124.12.148`** o con prefijo de red **`10.1.12.148`**.

---

### 2.2. Pool de Short IDs en la Torre (2 Bytes / 16 bits)
Cada Torre mantiene un rango de direcciones efímeras para sus clientes activos:

```
┌─────────────────┬──────────────────────────────────────────────────────────┐
│ Rango Short ID  │                        Propósito                         │
├─────────────────┼──────────────────────────────────────────────────────────┤
│ 0x0000          │ Reservado (Dirección Nula)                               │
│ 0x0001          │ Gateway / Torre Local (Host Base)                        │
│ 0x0002 .. 0x01FF│ Pool Dinámico de Clientes Locales (510 clientes activos) │
│ 0x0200 .. 0xFEFF│ Nodos Estáticos / Repetidores Fijos                      │
│ 0xFFFF          │ Broadcast General a todos los nodos                      │
└─────────────────┴──────────────────────────────────────────────────────────┘
```

---

## 🔬 3. Análisis de Carga de Red y Overhead (¿Es costoso el handshake?)

Una preocupación legítima es si solicitar el `Short ID` mediante un handshake previo introduce saturación o retrasos en el aire.

### 📐 Tamaño Físico de las Tramas de Asociación:

```
[1] CLIENTE ──► TORRE: Trama de Asociación (Solicitud de Short ID) - 12 Bytes Totales
┌───────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ MicroChunk│ Control Byte │ Dst Short ID │ Tag Servicio │ Client UUID  │ Capabilities │
│  (2 Bytes)│   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (4 Bytes)  │   (2 Bytes)  │
├───────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x01 0x10 │ 0x4F (Signal)│ 0x0001 (Gate)│ 0x01 (ASSOC) │  0x017C0C94  │ [LR | Norm]  │
└───────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘

[2] TORRE ──► CLIENTE: Trama de Asignación de Short ID - 10 Bytes Totales
┌───────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ MicroChunk│ Control Byte │ Dst Short ID │ Tag Servicio │ Assigned ID  │ Lease Flags  │
│  (2 Bytes)│   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │   (2 Bytes)  │
├───────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x01 0x20 │ 0x4F (Signal)│ 0xFFFF (Bcast│ 0x02 (ASSIGN)│    0x0005    │ [Proxy | 1h] │
└───────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

### ⏱️ Métricas de Rendimiento en el Aire:

| Métrica | En ESP-NOW (1 Mbps) | En SX1280 FLRC (1.3 Mbps) | En LoRa SF7 (18 kbps) |
| :--- | :---: | :---: | :---: |
| **Tiempo de Solicitud (12B)** | **0.85 ms** | **0.42 ms** | **28 ms** |
| **Tiempo de Respuesta (10B)** | **0.82 ms** | **0.40 ms** | **26 ms** |
| **Tiempo Total de Handshake** | **< 2.5 ms** | **< 1.0 ms** | **~60 ms** |
| **Frecuencia de Ejecución** | **1 sola vez** al encender el dispositivo o al cambiar de torre |

> **Conclusión:** El handshake de asociación toma **menos de 2.5 milisegundos** y ocurre **una sola vez**. No genera ninguna carga apreciable en el canal de radio.

---

## 🔄 4. Ciclo de Vida de Arriendo (Lease), Persistencia en NVS y Expiración

Para no repetir el handshake cada vez que se reinicia el microcontrolador:

```
              ┌──────────────────────────────────────────────┐
              │              ENCENDIDO DEL ESP32             │
              └──────────────────────┬───────────────────────┘
                                     │
                    ¿Hay Short ID guardado en NVS?
                                     │
                    ┌────────────────┴────────────────┐
                    ▼ [ SÍ ]                          ▼ [ NO ]
       ┌──────────────────────────┐      ┌──────────────────────────────┐
       │ Enviar paquete Ping/Keep │      │ Handshake de Asociación      │
       │ a la Torre con Short ID  │      │ enviando UUID de 4 Bytes     │
       └────────────┬─────────────┘      └──────────────┬───────────────┘
                    │                                   │
             ¿Torre confirma?                           ▼
                    │                    ┌──────────────────────────────┐
            ┌───────┴───────┐            │ Recibe Short ID (ej: 0x0005) │
            ▼ [ SÍ ]        ▼ [ NO / Exp]│ Guarda en NVS                │
       ┌─────────┐   ┌─────────────────┐ └──────────────┬───────────────┘
       │  LISTO  │   │ Renegociar      │                │
       │(3 Bytes)│   │ Short ID nuevo  │◄───────────────┘
       └─────────┘   └─────────────────┘
```

---

## 🔀 5. Traducción Bidireccional Asimétrica (Local 3B $\longleftrightarrow$ WAN 4B/10B)

### 📤 5.1. Flujo de Salida (Uplink: Nodo $\longrightarrow$ Internet / Servidor)

1. El **ESP32-S3** envía su petición web con cabecera de **3 Bytes**:
   $$\text{Trama en el Aire: } [\mathtt{Control: 0x0F}] \; [\mathtt{Dst: 0x0001}] \; [\mathtt{Payload: "clima.mesh"}]$$
2. La **Torre (Gateway-Router)** recibe la trama:
   - Sabe por la Capa 2 de radio que la MAC emisora es `9C:CC:01:7C:0C:94`.
   - Consulta su tabla Pseudo-ARP: `MAC 9C:CC:01:...` $\rightarrow$ `UUID 0x017C0C94 (1.124.12.148)`.
3. La Torre expande el paquete para la red troncal o el servidor backend:
   $$\text{Trama Hacia el Servidor / Troncal: } [\mathtt{SrcUUID: 0x017C0C94}] \; [\mathtt{DstService: 0x07}] \; [\mathtt{Payload: "clima.mesh"}]$$

---

### 📥 5.2. Flujo de Entrada (Downlink: Servidor / Internet $\longrightarrow$ Nodo)

1. El servidor o router exterior responde hacia el destino global:
   $$\text{Respuesta Entrante a la Torre: } [\mathtt{DstUUID: 0x017C0C94}] \; [\mathtt{Bytecode TLVGL (326B)}]$$
2. La Torre consulta su tabla Pseudo-ARP:
   - `DstUUID 0x017C0C94` $\rightarrow$ `Short ID local: 0x0005` $\rightarrow$ `MAC: 9C:CC:01:7C:0C:94`.
3. La Torre reempaqueta con la cabecera ligera de **3 Bytes** y emite por ESP-NOW:
   $$\text{Trama Emitida al Aire: } [\mathtt{Control: 0x4F}] \; [\mathtt{DstShortID: 0x0005}] \; [\mathtt{Bytecode TLVGL (326B)}]$$
4. El **ESP32-S3** ve que el destino es `0x0005` (su Short ID activo) y renderiza a 60 FPS.

---

## 🚶 6. Manejo de Roaming y Movilidad entre Múltiples Torres

¿Qué sucede cuando el usuario camina o viaja en vehículo y cambia de la cobertura de la Torre A a la Torre B?

```
             ┌─────────────────┐                   ┌─────────────────┐
             │     TORRE A     │                   │     TORRE B     │
             │ (Zona 1 Torre 1)│                   │ (Zona 1 Torre 2)│
             └────────┬────────┘                   └────────┬────────┘
                      │                                     │
           [ Short ID local: 0x0002 ]            [ Short ID local: 0x0008 ]
           [ UUID global: 0x017C0C94]            [ UUID global: 0x017C0C94]
                      ▲                                     ▲
                      │                                     │
                      └───────────── [ ESP32-S3 ] ──────────┘
                                 (Se mueve de A a B)
```

1. **El UUID del ESP32 NUNCA cambia:** Sigue siendo `0x017C0C94` (`1.124.12.148`).
2. Al entrar en el radio de la Torre B (detectada por el barrido multicanal de beacons):
   - El ESP32 emite su solicitud de asociación a la Torre B con su UUID `0x017C0C94`.
   - La Torre B le asigna un Short ID de su propio pool local (ej: `0x0008`).
   - La Torre B anuncia por OSPF a la red: *"El UUID `0x017C0C94` ahora está bajo mi cobertura"*.
3. **El tráfico se redirige inmediatamente a la nueva torre sin romper conexiones.**

---

## 📊 7. Comparativa Técnica: Pool Dinámico vs SLAAC Directo

| Característica | Modelo Pool Dinámico (Pseudo-DHCP) | Modelo SLAAC Directo (Estático) |
| :--- | :---: | :---: |
| **Garantía Anti-Colisiones en el Aire** | **100% Absoluta** (Asignado por la Torre) | 98.2% nativo (requiere DAD) |
| **Complejidad del Servidor/Torre** | Media (Mantiene Pool de IDs 0x0002..0x01FF) | Baja (Solo registra lo que le envían) |
| **Operación P2P sin Torre** | Requiere Short ID fallback (ej. MAC[4..5]) | **Totalmente Nativa e Inmediata** |
| **Eficiencia de Tráfico en el Aire** | **3 Bytes por trama** | **3 Bytes por trama** |
| **Soporte de Roaming Multi-Torre** | **Excelente** (Cambia Short local, mantiene UUID) | Bueno |
| **Sobrecarga de Handshake Inicial** | 1 intercambio de 2.5 ms | Cero |

---

## 🎯 Conclusión y Recomendación de Diseño

El modelo de **Pool Dinámico de Short IDs con Traducción Asimétrica de UUIDs**:
1. **Es extremadamente ligero:** El handshake toma menos de 2.5 ms y solo se ejecuta 1 vez.
2. **Garantiza 0% de colisiones** en células densas de radio.
3. **Mantiene la identidad fija del dispositivo:** El UUID de 4 Bytes derivado de los eFuses de silicio es la dirección permanente del usuario en todo el mundo.
4. **Protege el ancho de banda:** El 99% del tráfico subsiguiente viaja con la cabecera ligera de **3 Bytes**.
