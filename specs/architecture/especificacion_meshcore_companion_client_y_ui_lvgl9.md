# 📡 Especificación Técnica: Cliente MeshCore, Interfaz Táctica LVGL 9.5 y Subprotocolo de Aplicaciones P2P (Ajedrez Off-Grid)

**Versión:** 1.0.0 (RFC-CBDOS-MESHCORE-COMPANION)  
**Estado:** Propuesta Técnica / En Desarrollo por Fases  
**Target:** ESP32-P4 (JC4880P443C) / ESP32-S3 (JC3248W535)  
**Módulos Afectados:** `core/include/cbdos/meshcore/*`, `core/src/meshcore/*`, `core/src/ui/views/MeshCoreView.*`, `core/src/games/chess/*`, `core/include/cbdos/serial.hpp`

---

## 📌 1. Visión General y Objetivos Arquitectónicos

El objetivo de esta especificación es integrar el ecosistema de comunicaciones descentralizadas por radio LoRa **MeshCore** dentro del sistema operativo **CBDos**, permitiendo el uso de "mochilas" de radio (*backpacks*) tanto fijas (vía conector GPIO JP1) como externas (vía USB Host CDC-ACM).

A diferencia de las arquitecturas tradicionales basadas en smartphones con enlaces BLE o Wi-Fi pesados, CBDos opera como una **estación táctica autónoma e independiente (Standalone Communicator)**.

### Principios Fundamentales del Subsistema:
1. **Consumo Estricto de la HAL Unificada de Sistema (Regla 8):**  
   El cliente de MeshCore opera enteramente en `core/` y no interactúa con drivers de bajo nivel. Consume de forma exclusiva la API abstracta [`cbdos::serial`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/serial.hpp).
2. **Intercambiabilidad de Transporte Transparente:**  
   Funciona idénticamente si el backpack se conecta a la UART física interna (`"jp1"`) o por conector USB-C Host (`"usb_otg"`).
3. **Arquitectura Reactiva sin Sondeo (Regla 12):**  
   Cero *polling* en la interfaz de usuario. Las actualizaciones de mensajes, métricas y descubrimiento de nodos disparan callbacks reactivos (`m_statusDirty = true`).
4. **Desacoplamiento Total y Offline-First (Regla 7):**  
   No requiere conexión a Internet, coprocesador C6 ni dependencias externas. El sistema arranca y funciona 100% desconectado.
5. **Multiplexación de Tráfico (Chat vs Datos de Apps):**  
   Soporte nativo para separar conversaciones de texto humano de cargas útiles binarias de aplicaciones y juegos por turnos (como Ajedrez P2P off-grid).

---

## 🏗️ 2. Diagrama de Arquitectura del Sistema

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                   CBDos Core                                     │
│                                                                                  │
│   ┌───────────────────────────────┐            ┌─────────────────────────────┐   │
│   │     MeshCoreView (LVGL 9.5)   │            │   ChessView / P2P Games     │   │
│   │  - Chats / Canales / DMs      │            │  - Tablero Ajedrez LVGL 9   │   │
│   │  - Lista de Nodos Malla       │            │  - Movimientos UCI / Bin    │   │
│   │  - Config Radio & Métricas    │            │  - Estado de Partida        │   │
│   └───────────────┬───────────────┘            └──────────────┬──────────────┘   │
│                   │                                           │                  │
│                   │ (App ID: 0x00 - Chat)                     │ (App ID: 0x10)   │
│                   └─────────────────────┬─────────────────────┘                  │
│                                         ▼                                        │
│                   ┌───────────────────────────────────────────┐                  │
│                   │      MeshCoreDispatcher (Enrutador)       │                  │
│                   │  - Multiplexa Chat vs Datos de Apps       │                  │
│                   │  - Gestión de ACKs y Cola de Prioridad    │                  │
│                   └─────────────────────┬─────────────────────┘                  │
│                                         ▼                                        │
│                   ┌───────────────────────────────────────────┐                  │
│                   │          MeshCoreClient (Parser)          │                  │
│                   │  - Framing binario Little-Endian          │                  │
│                   │  - Serialización / Deserialización        │                  │
│                   │  - Máquina de Estados del Protocolo       │                  │
│                   └─────────────────────┬─────────────────────┘                  │
│                                         ▼                                        │
│                   ┌───────────────────────────────────────────┐                  │
│                   │             API cbdos::serial             │                  │
│                   │  - open() / close() / read() / write()    │                  │
│                   └─────────────────────┬─────────────────────┘                  │
└─────────────────────────────────────────┼────────────────────────────────────────┘
                                          │
┌─────────────────────────────────────────▼────────────────────────────────────────┐
│                                 BSP Hardware                                     │
│   ┌─────────────────────────────────────┐  ┌─────────────────────────────────┐   │
│   │     Puerto Físico JP1 (UART)        │  │     Puerto USB Host (CDC-ACM)   │   │
│   │   - ESP32-P4: TX:32 RX:28 RST:54    │  │   - Detección Hotplug Dinámica  │   │
│   │   - ESP32-S3: TX:15 RX:16           │  │   - UsbDeviceManager            │   │
│   └─────────────────────────────────────┘  └─────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 📦 3. Protocolo Binario: MeshCore Companion Protocol

El protocolo de comunicación entre CBDos y el firmware del módulo LoRa es binario y orientado a paquetes con codificación **Little-Endian**.

### 3.1. Estructura General de Trama (Framing)

```
┌──────────┬──────────┬──────────┬───────────────────────────┬──────────┐
│  PRE_1   │  PRE_2   │  CMD_ID  │      PAYLOAD_LEN (LE)     │ PAYLOAD  │
│  (0x55)  │  (0xAA)  │  (1 Byte)│          (2 Bytes)        │ (N Bytes)│
└──────────┴──────────┴──────────┴───────────────────────────┴──────────┘
```

- **Preámbulo de Sincronización:** `0x55 0xAA` (2 bytes).
- **ID de Comando (`CMD_ID`):** 1 byte que indica el tipo de instrucción o evento.
- **Longitud (`PAYLOAD_LEN`):** `uint16_t` (Little-Endian) con el tamaño exacto del cuerpo de datos.
- **Cuerpo (`PAYLOAD`):** Datos estructurados dependientes del comando.
- **Checksum / Validación:** Integrado en la cabecera o mediante CRC16 al final de tramas de datos críticos.

### 3.2. Tabla de Comandos Principales

| CMD_ID | Nombre | Dirección | Descripción |
| :---: | :--- | :---: | :--- |
| `0x01` | `CMD_DEVICE_QUERY` | CBDos $\to$ Radio | Solicita versión de firmware, UUID del nodo, batería y estado. |
| `0x81` | `RESP_DEVICE_INFO` | Radio $\to$ CBDos | Respuesta con información de telemetría y configuración del radio. |
| `0x02` | `CMD_GET_CONTACTS` | CBDos $\to$ Radio | Solicita la tabla de nodos vecinos descubiertos. |
| `0x82` | `RESP_CONTACT_ITEM` | Radio $\to$ CBDos | Emite información de un contacto (ID, Alias, SNR, RSSI, Hops). |
| `0x03` | `CMD_SEND_PACKET` | CBDos $\to$ Radio | Envía una trama de datos hacia la malla (Broadcast o Direct). |
| `0x83` | `RESP_PACKET_ACK` | Radio $\to$ CBDos | Confirmación de transmisión de paquete o recepción de ACK remoto. |
| `0x84` | `EVENT_PACKET_RECV` | Radio $\to$ CBDos | Notificación reactiva de paquete entrante desde la malla. |
| `0x05` | `CMD_SET_RADIO_CFG` | CBDos $\to$ Radio | Ajusta frecuencia, canal, ancho de banda (BW) y Spreading Factor (SF). |
| `0x85` | `RESP_RADIO_CFG` | Radio $\to$ CBDos | Parámetros activos del transceptor LoRa. |

---

## 🎮 4. Subprotocolo de Aplicaciones y Juegos P2P (MeshApp Framing)

Para evitar la saturación de los canales de chat humano con datos binarios y permitir una interacción fluida con minijuegos y utilidades, toda carga útil enviada a través de `CMD_SEND_PACKET` incorpora una cabecera de despacho de aplicación:

```
┌────────────┬────────────┬────────────────────────┬──────────────────────┐
│  APP_ID    │  MSG_SEQ   │    FLAGS / SUB-TYPE    │     APP_PAYLOAD      │
│  (1 Byte)  │  (1 Byte)  │        (1 Byte)        │      (N Bytes)       │
└────────────┴────────────┴────────────────────────┴──────────────────────┘
```

### 4.1. Catálogo de Identificadores de Aplicación (`APP_ID`)

- `0x00`: **Text Messaging (Chat)**: Mensajes estándar en UTF-8 visualizados en `MeshCoreView`.
- `0x10`: **P2P Chess (CBDos-Chess)**: Partidas de ajedrez asíncronas punto a punto.
- `0x11`: **Battleship (Hundir la Flota)**: Coordenadas de disparo y confirmaciones de impacto.
- `0x20`: **Telemetry & Sensors**: Posición GNSS, baliza SOS y telemetría ambiental.
- `0x30`: **Remote Shell / Admin**: Comandos de administración remota autenticados.

### 4.2. Especificación de Carga Útil para Ajedrez P2P (`APP_ID = 0x10`)

Dadas las limitaciones de transmisión de LoRa (~1% Duty Cycle en frecuencias ISM), el subprotocolo de ajedrez está optimizado para consumir **entre 3 y 6 bytes por jugada**.

#### Estructura del Paquete de Ajedrez:

| Campo | Tamaño | Descripción |
| :--- | :---: | :--- |
| `GameSessionID` | 1 Byte | ID de la partida activa entre dos nodos (soporta partidas concurrentes). |
| `TurnNumber` | 1 Byte | Contador incremental de turnos para detectar saltos o pérdida de paquetes. |
| `ActionType` | 4 Bits | `0`: Movimiento normal, `1`: Enroque, `2`: Promoción, `3`: Oferta Tablas, `4`: Acepta Tablas, `5`: Rendición. |
| `SourceSquare` | 6 Bits | Casilla inicial ($0$ a $63$, donde $0=\text{a1}$, $63=\text{h8}$). |
| `TargetSquare` | 6 Bits | Casilla destino ($0$ a $63$). |
| `PromotionPiece`| 2 Bits | `0`: Dama, `1`: Torre, `2`: Alfil, `3`: Caballo. |
| `BoardHash` | 2 Bytes | Hash CRC16 del estado del tablero (FEN condensado) para verificar sincronismo. |

**Tamaño total por movimiento:** **4 a 6 bytes**. Esto garantiza que un movimiento de ajedrez genere un pulso LoRa de apenas ~40 ms en el aire, imperceptible para la congestión de la malla.

---

## 🗺️ 5. Hoja de Ruta de Implementación por Fases

El desarrollo se organiza en 5 fases secuenciales e incrementales:

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           PLAN DE EJECUCIÓN POR FASES                           │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│   FASE 1: Motor del Protocolo y Abstracción de Enlace                           │
│   - Parser de tramas Little-Endian en C++ puro (core/src/meshcore/)             │
│   - Integración con cbdos::serial (Puertos "jp1" y "usb_otg")                   │
│   - Cola de recepción reactiva FreeRTOS y manejo de ACKs                        │
│                                                                                 │
│   FASE 2: Interfaz Táctica de Mensajería en LVGL 9.5                            │
│   - Vista principal MeshCoreView (Tabs: Chats, Nodos Malla, Radio Config)       │
│   - Burbujas de chat reactivas y entrada con teclado en pantalla / físico       │
│   - Libreta de contactos con métricas de señal (RSSI, SNR, Hops)                │
│                                                                                 │
│   FASE 3: Dispatcher de Aplicaciones y Ajedrez P2P Off-Grid                     │
│   - Router de payloads por APP_ID (Chat vs Datos)                               │
│   - Implementación de la vista de tablero de Ajedrez en LVGL 9.5                │
│   - Motor de validación de movimientos y serialización ultra-compacta (4 bytes) │
│                                                                                 │
│   FASE 4: Persistencia Local y Gestión de Caché                                 │
│   - Historial de mensajes y conversaciones en LittleFS / MicroSD                │
│   - Caché de topología de nodos vecinos mediante IStorageBackend                │
│                                                                                 │
│   FASE 5: Bindings de Automatización y Bots (Headless en Lua)                   │
│   - Métodos en cbdos.meshcore.* para scripts automáticos                        │
│   - Bots de respuesta táctica, repetidores de software y pasarelas             │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

### Detalle de las Fases

#### 🔹 Fase 1: Motor del Protocolo y Abstracción de Enlace
- **Entregables:**
  - `core/include/cbdos/meshcore/meshcore_types.hpp`: Enums de comandos, estructuras de paquetes y descriptores de nodo.
  - `core/include/cbdos/meshcore/meshcore_client.hpp` y `.cpp`: Parser de máquina de estados por bytes entrantes.
  - Conexión con `cbdos::serial`: Selección automática o manual de `"jp1"` (UART mochila) o `"usb_otg"` (USB Host CDC-ACM).
  - Tarea de escucha reactiva en segundo plano con semáforo y callbacks para eventos entrantes.

#### 🔹 Fase 2: Interfaz Táctica de Mensajería en LVGL 9.5
- **Entregables:**
  - `core/src/ui/views/MeshCoreView.hpp` y `.cpp`: Hereda de `BaseView`, utiliza el contenedor `m_container` y se integra con `HeaderBar`.
  - **Tab 1 - Mensajería:** Historial con burbujas diferenciadas (remoto/local), estado de ACK (reloj, entregado, fallido), `lv_textarea` inferior y teclado de CBDos.
  - **Tab 2 - Nodos y Topología:** Lista interactiva con indicador visual de batería, calidad de enlace (SNR/RSSI) y selector de nodo para iniciar DM.
  - **Tab 3 - Configuración y Enlace:** Estado del enlace serie, selector de baudrate/puerto y configuración de canal de radio.
  - Aplicación estricta de la **Regla 12**: Cero *polling*. La UI se actualiza únicamente cuando se activa la bandera `m_statusDirty`.

#### 🔹 Fase 3: Dispatcher de Aplicaciones y Ajedrez P2P Off-Grid
- **Entregables:**
  - `core/include/cbdos/meshcore/meshcore_dispatcher.hpp`: Enrutador que inspecciona el primer byte de la carga útil y la entrega a la aplicación suscripta.
  - `core/src/games/chess/ChessView.hpp` y `.cpp`: Tablero interactivo en LVGL 9.5 con selección táctil de casillas y renderizado de piezas SVG/Bitmap.
  - Lógica de sincronización P2P: Envío de jugadas empaquetadas (4 bytes), validación de turno y verificación de hash del tablero para evitar desincronizaciones por paquetes perdidos.

#### 🔹 Fase 4: Persistencia Local y Gestión de Caché
- **Entregables:**
  - Serialización de conversaciones en formato binario compacto o JSON ligero en `/sdcard/meshcore/chats/` o LittleFS mediante `IStorageBackend`.
  - Almacenamiento persistente de contactos conocidos (Alias, NodeID, clave pública).

#### 🔹 Fase 5: Bindings de Automatización y Bots (Headless en Lua)
- **Entregables:**
  - Registro en `LuaBridge.cpp` del módulo `cbdos.meshcore.*`:
    - `cbdos.meshcore.send_text(node_id, text)`
    - `cbdos.meshcore.on_message(callback)`
    - `cbdos.meshcore.get_nodes()`
  - Scripts de ejemplo para bots desatendidos (ej. consulta meteorológica offline, eco-bot de alcance de malla o contestador automático).

---

## 🔒 6. Cumplimiento de Reglas de CBDos

| Regla | Mecanismo de Cumplimiento |
| :--- | :--- |
| **Regla 7 (Offline-First)** | El sistema no utiliza sockets IP, DNS ni coprocesador C6; la comunicación es 100% directa por puerto serie a la radio LoRa física. |
| **Regla 8 (Pureza de `core/`)** | El cliente MeshCore y la UI están escritos en C++ estándar y LVGL 9.5 sin incluir cabeceras de IDF o Arduino. El hardware se consume mediante `cbdos::serial`. |
| **Regla 10 (No destructivo)** | Este documento reside en un archivo nuevo sin truncar ninguna especificación preexistente de `specs/`. |
| **Regla 12 (Cero Polling)** | La recepción de paquetes genera eventos en colas de FreeRTOS en el BSP y activa la bandera `m_statusDirty` en el Core para refrescar la UI. |
