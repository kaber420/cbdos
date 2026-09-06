# 📡 Especificación de Direccionamiento Jerárquico IPv4 Mesh, Tabla Pseudo-ARP y Control de Acceso Proxy

**Documento:** `docs/network/especificacion_direccionamiento_ipv4_mesh_y_pseudo_arp.md`  
**Estado:** Propuesta de Arquitectura Formal  
**Versión:** 1.0.0 (RFC-CBDOS-NET-02)  
**Fecha:** Agosto 2026  
**Ámbito:** Firmware ESP32-S3/P4 (C++), Gateway-Router (`gateway_router.py`), Servidores de Contenido y Proxy.

---

## 🏛️ 1. Principio y Motivación

Para dotar a la red malla **CBDos** de una estructura de ruteo determinista, escalable y compatible con herramientas estándar de red (OSPF, tablas de ruteo de kernel Linux, firewalls), se adopta el esquema de direccionamiento **IPv4 Privado RFC 1918 (`10.0.0.0/8`)** empaquetado en enteros nativos de **4 Bytes (32 bits)**.

Este modelo combina:
1. **Dirección Lógica Global (4 Bytes):** Legible como una IP privada (`10.Zona.Torre.Nodo`) con enrutamiento natural inter-torres.
2. **Identidad Inmutable de Silicio (6 Bytes):** MAC física obtenida a nivel de radio (Capa 2) para auditoría y autenticación.
3. **Tráfico Operativo Ultra-Ligero (3 Bytes):** Cabecera de 3 Bytes (`DST_ONLY` con `Short ID` de 2 Bytes) para no penalizar el tiempo de aire (*Time-on-Air*) en transmisiones ESP-NOW / FLRC / LoRa.

---

## 🗺️ 2. Estructura del UUID de 4 Bytes (`10.Zona.Torre.Nodo`)

Cualquier nodo dentro del ecosistema posee un identificador de 32 bits con la siguiente distribución de octetos:

```
  0                   1                   2                   3
  0 1 2 3 4 5 6 7     0 1 2 3 4 5 6 7     0 1 2 3 4 5 6 7     0 1 2 3 4 5 6 7
 ┌───────────────┬───┬───────────────┬───┬───────────────┬───┬───────────────┐
 │   Prefijo     │ . │    Zona       │ . │    Torre      │ . │    Nodo       │
 │ 0x0A (10 dec) │   │ (Área OSPF)   │   │  (Gateway ID) │   │  (Cliente ID) │
 └───────────────┴───┴───────────────┴───┴───────────────┴───┴───────────────┘
```

| Octeto | Campo | Rango | Descripción |
| :---: | :--- | :---: | :--- |
| **1** | **Prefijo de Red** | `10` (`0x0A`) | Identificador constante del espacio privado de la malla CBDos. |
| **2** | **Zona / Región** | `0 .. 255` | Comunidad, valle o área geográfica (Área OSPF). |
| **3** | **Torre / Gateway** | `0 .. 255` | Identificador de la torre de radio base o gateway local. |
| **4** | **Nodo / Cliente** | `1 .. 254` | Identificador del terminal de usuario (asignado por la Torre). |

> **Ejemplo de Dirección:**
> - Terminal conectado a la Torre 3 de la Zona 1: **`10.1.3.42`** (`0x0A01032A`).
> - Dirección de la propia Torre/Gateway: **`10.1.3.1`** (`0x0A010301`).
> - Broadcast local de Torre: **`10.1.3.255`** (`0x0A0103FF`).

---

## 🌲 3. Ruteo Jerárquico OSPF / BGP con Prefijos `/24`

Cada Torre o Gateway de radio física administra un bloque de subred **/24** (254 direcciones cliente):

```
                                  [ RED GLOBAL / INTER-ASN ]
                                              │ (Pseudo-BGP)
                                              ▼
                             ┌──────────────────────────────────┐
                             │       ROUTER FRONTERA / SBC      │
                             │  Área OSPF: Zona 1 (10.1.0.0/16) │
                             └────────────────┬─────────────────┘
                                              │
                     ┌────────────────────────┴────────────────────────┐
                     ▼                                                 ▼
        ┌──────────────────────────┐                      ┌──────────────────────────┐
        │   TORRE 1 (10.1.1.0/24)  │                      │   TORRE 2 (10.1.2.0/24)  │
        │ • Anuncia: 10.1.1.0/24   │ ◄─── Enlace FLRC ──► │ • Anuncia: 10.1.2.0/24   │
        └────────────┬─────────────┘      (Troncal 1.3M)  └────────────┬─────────────┘
                     │ (ESP-NOW Local)                                 │ (ESP-NOW Local)
           ┌─────────┴─────────┐                             ┌─────────┴─────────┐
           ▼                   ▼                             ▼                   ▼
    [Nodo 10.1.1.10]    [Nodo 10.1.1.11]              [Nodo 10.1.2.5]     [Nodo 10.1.2.6]
```

### ⚡ Algoritmo de Enrutamiento en O(1):
1. Si el destino es `10.Zona_Actual.Torre_Actual.X` $\rightarrow$ **Entrega Local Directa** (Capa 2).
2. Si el destino es `10.Zona_Actual.Otra_Torre.X` $\rightarrow$ Reenviar por troncal OSPF a la Torre destino.
3. Si el destino es `10.Otra_Zona.X.X` $\rightarrow$ Reenviar al Router Frontera de Zona.
4. Si el destino es fuera de `10.0.0.0/8` $\rightarrow$ Reenviar al Proxy Gateway de Internet.

---

## 🗃️ 4. La Tabla Pseudo-ARP en la Torre / Gateway

El Gateway mantiene en memoria una tabla dinámica de traducción entre la dirección lógica IPv4, la dirección física de radio y el token de sesión rápida, persistida en una base de datos **SQLite3 (`clients.db`)** con modo WAL (*Write-Ahead Logging*):

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────┐
│                             TABLA PSEUDO-ARP (SQLite3 - clients.db)                              │
├─────────────┬───────────┬───────────────────┬──────────────┬──────────────┬─────────┬────────────┤
│ IPv4 (UUID) │ Short ID  │     MAC Física    │ Permiso Proxy│ Enlace Radio │  RSSI   │ Last Seen  │
├─────────────┼───────────┼───────────────────┼──────────────┼──────────────┼─────────┼────────────┤
│ 10.1.3.1    │  0x0001   │ Gateway Local     │ N/A (Root)   │ UART/Local   │  0 dBm  │ Activo     │
│ 10.77.127.216│ 0x7FD8   │ 14:C1:9F:4D:7F:D8 │ AUTORIZADO   │ ESP-NOW Norm │ -29 dBm │ Hace 1s    │
│ 10.124.12.148│ 0x0C94   │ 9C:CC:01:7C:0C:94 │ AUTORIZADO   │ ESP-NOW LR   │ -48 dBm │ Hace 5s    │
└─────────────┴───────────┴───────────────────┴──────────────┴──────────────┴─────────┴────────────┘
```

### Funciones de la Tabla Pseudo-ARP:
1. **Resolución Inversa Inmediata:** Convierte tramas entrantes de 3 Bytes (`Short ID 0x7FD8`) a su identidad completa `10.77.127.216` para logs, auditoría y enrutamiento hacia la red exterior.
2. **Control de Acceso (ACL):** Determina si el cliente puede hacer consultas web a internet (`proxy_acl = 1`) o únicamente acceder a recursos locales de la malla.
3. **Persistencia ACID & Concurrencia:** Utiliza transacciones SQLite3 en modo WAL para evitar cualquier corrupción en caso de apagón del Gateway y permitir consultas concurrentes desde dashboards web o bots de telemetría.
4. **Mapeo a la Capa 2:** Al responder, asocia el `Short ID` con la dirección MAC física de hardware para la API de transmisión del driver de radio.

---

## 🤝 5. Protocolo de Aprovisionamiento y Registro (Pseudo-DHCP)

Cuando un terminal CBDos entra en cobertura de una Torre, se ejecuta el siguiente handshake de 2 pasos:

```
  ESP32-S3 (Cliente)                                        Torre / Gateway
        │                                                          │
        │─── [1] PROBE_ASSOC_REQ ─────────────────────────────────►│
        │    (MAC: en Capa 2 + eFuse Serial + Caps de Radio)       │
        │                                                          │
        │                                            [Consulta ACL / Asigna IP]
        │                                            IP: 10.1.3.42
        │                                            ShortID: 0x002A
        │                                            Proxy: AUTORIZADO
        │                                                          │
        │◄── [2] PROBE_ASSOC_ACK ──────────────────────────────────│
        │    (IPv4 Asignada + Short ID + Permisos de Red)          │
        │                                                          │
        │─── [3] Tráfico Operativo Normal (Cabecera de 3 Bytes) ──►│
```

### 5.1. Trama de Asociación del Cliente (`PROBE_ASSOC_REQ`):
- **Capa 2 Radio:** Contiene la MAC física de 6 Bytes en el preámbulo de hardware (`info->src_addr`).
- **Payload:**
  - `Tag: 0x01` (`TYPE_ASSOC_REQUEST`)
  - `Capabilities (2B):` Modos soportados (ESP-NOW, LR, FLRC, LoRa).
  - `Hostname (String):` Nombre descriptivo del terminal (ej: `"CBDos S3"`).

### 5.2. Trama de Confirmación de la Torre (`PROBE_ASSOC_ACK`):
- **Payload:**
  - `Tag: 0x02` (`TYPE_ASSOC_ACK`)
  - `Assigned IPv4 (4B):` `0x0A 0x01 0x03 0x2A` (`10.1.3.42`)
  - `Assigned Short ID (2B):` `0x002A`
  - `Gateway IPv4 (4B):` `10.1.3.1`
  - `Proxy Flags (1B):` `0x01` = Proxy Internet Habilitado, `0x00` = Solo Malla.

---

## 🛡️ 6. Políticas de Control de Acceso Proxy (Internet vs Malla)

El Gateway-Router aplica dos reglas estrictas de reenvío:

```
                               Petición Entrante del Nodo (10.1.3.42)
                                                 │
                                                 ▼
                                     ¿Destino está en 10.0.0.0/8?
                                                 │
                               ┌─────────────────┴─────────────────┐
                               ▼                                   ▼
                            [ SÍ ]                               [ NO ] (Internet / URLs públicas)
                               │                                   │
                               ▼                                   ▼
                    [ Tráfico Interno Malla ]             ¿`proxy_enabled == true`?
                    • Páginas `.mesh` locales                      │
                    • Chat entre nodos                   ┌─────────┴─────────┐
                    • Telemetría de sensores             ▼                   ▼
                                                      [ SÍ ]               [ NO ]
                                                         │                   │
                                                         ▼                   ▼
                                              [ Transcodificar Web ]   [ DROP / Error 403 ]
                                              • Open-Meteo, RSS, etc.  "Acceso a Internet
                                              • Compilar a TLVGL        No Autorizado"
```

---

## 🚀 7. Roadmap de Implementación

1. **Fase 1: Gateway-Router (`gateway_router.py`):**
   - Integrar la clase `PseudoArpTable` con almacenamiento transaccional en **SQLite3 (`clients.db`)**.
   - Implementar el despachador de asociación `handle_assoc_request()`.
2. **Fase 2: Firmware CBDos (`MeshEngine.cpp`):**
   - Añadir la máquina de estados de asociación para registrar el UUID/IPv4 y persistir el `Short ID` en NVS.
3. **Fase 3: Proxy de Transcodificación Dinámica:**
   - Atender peticiones dinámicas (`clima`, `noticias`, `sensores`) verificando las banderas ACL de la tabla Pseudo-ARP.
