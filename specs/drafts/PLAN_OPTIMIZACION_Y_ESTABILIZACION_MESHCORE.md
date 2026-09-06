# 📋 ESPECIFICACIÓN TÉCNICA Y PLAN DE TRABAJO: SUBSISTEMA MESHCORE (CBDos v0.2.1)

**Estado:** Borrador de Arquitectura y Plan Maestro de Implementación  
**Módulos Afectados:** `core/src/apps/meshcore/`, `core/include/cbdos/`, `bsp/esp32_s3_jc3248/`, `bsp/esp32_p4_jc4880/`  
**Autor:** Antigravity / CBDos Core Team  
**Objetivo:** Definir de forma exhaustiva la arquitectura de datos, sincronización thread-safe, libreta de contactos en MessagePack, protocolo P2P con confirmación de entrega (ACK) e interfaz gráfica adaptada a pantallas de 3.5" y 4.3".

---

## 🧭 1. Guía Integral: ¿Cómo Opera MeshCore (Sin Internet)?

MeshCore es un protocolo de red de malla (*Ad-Hoc Tactical Mesh*) que opera de forma totalmente descentralizada sobre capas de enlace de radio (ESP-NOW 2.4 GHz, LoRa SX1262 Sub-GHz y módems USB).

```
 ┌────────────────┐          ESP-NOW / LoRa           ┌────────────────┐
 │  CYBERDECK S3  │ ◄───────────────────────────────► │  LAPTOP BASE   │
 │   ID: 0x1337   │                                   │   ID: 0xCAFE   │
 └───────┬────────┘                                   └────────────────┘
         │
         │ Reenvío Multi-salto (Managed Flooding, Max 7 Hops)
         ▼
 ┌────────────────┐
 │ NODO REPETIDOR │
 │   ID: 0x88AA   │
 └────────────────┘
```

### 1.1. Identidades Locales y Direccionamiento
- **Short ID (2 Bytes / uint16_t):** Dirección hexadecimal única generada a partir de los últimos 2 bytes de la MAC de hardware o elegida por el usuario (ej: `0x1337`).
- **Node Name (Cadena UTF-8, máx 24 bytes):** Nombre legible del nodo (ej: `"Cyberdeck-S3"`).
- **Broadcast Address (`0xFFFF`):** Dirección especial que indica que el paquete debe ser procesado por todos los nodos en el canal.
- **Unicast Address (`0xXXXX`):** Dirección específica para comunicación privada P2P.

---

### 1.2. Protocolo Binario: Estructura de Tramas (`'MC'` = 0x4D43)

Todas las tramas viajan con cabecera binaria compacta para minimizar el *airtime* y consumo energético:

| Tipo de Trama | Código | Propósito |
| :--- | :---: | :--- |
| **`PKT_BEACON`** | `0x01` | Anuncio de presencia periódica y descubrimiento en radar. |
| **`PKT_CHAT`** | `0x02` | Mensajes de texto (Broadcast a canal o Unicast P2P). |
| **`PKT_ACK`** | `0x03` | Confirmación de recepción con telemetría de retorno. |
| **`PKT_ROUTE`** | `0x04` | Anuncio de rutas y calidad de enlace entre nodos. |

#### A. Trama `PKT_BEACON` (0x01) - Longitud: 11 + N bytes
```
┌───────────┬──────────┬──────────┬──────────┬──────────┬──────────┬─────────────┬────────────────┐
│ Magic(2B) │ Type(1B) │ Hops(1B) │ SrcId(2B)│ DstId(2B)│ Reserv(2)│ NameLen(1B) │ Nombre (N B)   │
│  0x4D43   │   0x01   │  0..7    │  0x1337  │  0xFFFF  │   0x00   │     12      │ "Cyberdeck-S3" │
└───────────┴──────────┴──────────┴──────────┴──────────┴──────────┴─────────────┴────────────────┘
```

#### B. Trama `PKT_CHAT` (0x02) - Longitud: 16 + N bytes
```
┌───────────┬──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┬────────────┬──────────────┐
│ Magic(2B) │ Type(1B) │ Hops(1B) │ SrcId(2B)│ DstId(2B)│ ChanId(2B)│ MsgId(4B)│ Flags(1B)│ TextLen(1B)│ Payload (N)  │
│  0x4D43   │   0x02   │  0..7    │  0x1337  │ 0xFFFF/ID│  0/Canal  │ 0x000001 │  Bitmask │     14     │ "Hola Mesh!" │
└───────────┴──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┴────────────┴──────────────┘
```
- **Flags (1 Byte):**
  - `Bit 0 (0x01):` Cifrado con contraseña de canal (PSK).
  - `Bit 1 (0x02):` Requiere confirmación de entrega (`ACK_REQUEST`).
  - `Bit 2 (0x04):` Paquete fragmentado (Microchunk).

#### C. Trama `PKT_ACK` (0x03) - Longitud: 14 bytes
```
┌───────────┬──────────┬──────────┬──────────┬──────────┬───────────┬──────────────┬───────────┐
│ Magic(2B) │ Type(1B) │ Hops(1B) │ SrcId(2B)│ DstId(2B)│ MsgId(4B) │ AckedRSSI(1B)│ Reserv(1) │
│  0x4D43   │   0x03   │  0..7    │  0xCAFE  │  0x1337  │ 0x000001  │   -45 dBm    │   0x00    │
└───────────┴──────────┴──────────┴──────────┴──────────┴───────────┴──────────────┴───────────┘
```

---

## 🗄️ 2. Persistencia: Libreta de Contactos en MessagePack (`contacts.msgpack`)

### 2.1. Política de Almacenamiento
- **NVS (Flash Key-Value):** Queda estrictamente reservada para flags mínimas de arranque del microcontrolador (Canal RF por defecto, Modo de radio activo).
- **Almacenamiento de Archivos (Flash Interna / `IStorageBackend`):** La libreta de contactos, alias e historial se serializan en binario **MessagePack** en la partición interna de archivos (`/spiffs/meshcore/contacts.msgpack` o `/storage/meshcore/contacts.msgpack`).
- **Autonomía Total:** Funciona de forma 100% independiente sin requerir una tarjeta MicroSD externa.

### 2.2. Estructura del Esquema MessagePack

El archivo `contacts.msgpack` contiene un mapa binario con metadatos de versión y la lista de registros:

```json
{
  "version": 1,
  "last_updated": 1725163200,
  "contacts": [
    {
      "id": 51966,
      "name": "Base-Laptop",
      "alias": "Laptop Taller Principal",
      "fav": true,
      "last_seen": 1725163195,
      "rssi": -42,
      "hops": 0,
      "iface": 0,
      "notes": "Estación base conectada a antena exterior"
    }
  ]
}
```

### 2.3. Estructura en C++ (`MeshContactRecord`)
```cpp
struct MeshContactRecord {
    uint16_t shortId;
    std::string announcedName;
    std::string customAlias;
    bool isFavorite;
    uint32_t lastSeenEpoch;
    int8_t lastRssi;
    uint8_t lastHops;
    uint8_t preferredInterface;
    std::string notes;
};
```

---

## 🎨 3. Especificación de UI (LVGL 9.5 en Pantallas 3.5" y 4.3")

### 3.1. Rediseño de Tarjetas en Radar (Fix de Texto Encimado)
En pantallas de 3.5 pulgadas (320x480 de ancho), colocar 4 datos en una sola línea horizontal colapsa el texto. Se especifica un layout limpio de **2 renglones verticales** por tarjeta:

```
┌─────────────────────────────────────────────────────────────┐
│ 📡 [0xCAFE] Base-Laptop-Estacion-Taller-Sur                 │ ◄── Renglón 1 (Blanco / Cyan): Nombre + ID
│ 📶 -45 dBm  •  0 saltos (Directo)  •  ESP-NOW Slot 0        │ ◄── Renglón 2 (#94A3B8): Telemetría
└─────────────────────────────────────────────────────────────┘
```

#### Especificación de Estilos LVGL:
- **Renglón 1 (Encabezado del Nodo):**
  - Fuente: `montserrat_14` (Bold / Resaltado).
  - Color: `0x00E5FF` (Cyan) si es favorito / `0xFFFFFF` (Blanco) estándar.
  - Modo marquesina: `lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR)`. Si el nombre supera el ancho de pantalla, rota suavemente sin cortarse.
- **Renglón 2 (Telemetría de Enlace):**
  - Fuente: `montserrat_12` (Compacta).
  - Color: `#94A3B8` (Gris azulado).
  - Formato: `📶 %d dBm  •  %s  •  %s` (`0 saltos (Directo)` / `N saltos`).

---

### 3.2. Modal de Acciones Rápidas al Tocar un Nodo
Al pulsar sobre cualquier tarjeta de la pestaña **Radar**, se abre una ventana modal con botones táctiles de gran tamaño:

```
┌──────────────────────────────────────────┐
│      NODO [0xCAFE] Base-Laptop           │
├──────────────────────────────────────────┤
│  [ 💬 Iniciar Chat Privado P2P ]         │
│  [ ⭐ Marcar como Favorito ]             │
│  [ ✏️ Cambiar Alias Personalizado ]      │
│  [ 📡 Enviar Ping de Alcance ]           │
│  [ ✕ Cerrar ]                            │
└──────────────────────────────────────────┘
```

---

### 3.3. Selector de Contexto en Chat (P2P vs Canales)
En la parte superior de la pestaña **Chat**, el desplegable de canales incluirá tanto los canales públicos como las conversaciones directas activas:

```
┌──────────────────────────────────────────┐
│ Contexto: [ 🌐 #general               ▼] │
│           │ 🌐 #general                  │
│           │ 🔒 #tactico                  │
│           │ 👤 @Base-Laptop (0xCAFE)    │
│           │ 👤 @Repetidor-Norte (0x88AA) │
└──────────────────────────────────────────┘
```
- Si se selecciona `🌐 #general`: `targetId = 0xFFFF`.
- Si se selecciona `👤 @Base-Laptop`: `targetId = 0xCAFE` (Chat privado P2P).

---

## 🔄 4. Máquina de Estados de Mensajería y Confirmaciones (`PKT_ACK`)

Cada mensaje saliente en la interfaz de chat cuenta con un indicador de estado visual:

```
┌───────────┐      Envío RF      ┌───────────┐     Recepción ACK     ┌─────────────┐
│ PENDIENTE │ ─────────────────► │  ENVIADO  │ ────────────────────► │  ENTREGADO  │
│  (Reloj)  │                    │ (1 Check) │                       │ (2 Checks)  │
└───────────┘                    └─────┬─────┘                       └─────────────┘
                                       │
                                       │ Timeout (3 seg) x 3 Reintentos
                                       ▼
                                ┌─────────────┐
                                │   FALLIDO   │
                                │  (Icono ⚠️) │
                                └─────────────┘
```

1. **Envío con Bandera ACK:** Cuando se envía a un `targetId != 0xFFFF`, el bit `ACK_REQUEST (0x02)` se activa en los flags.
2. **Recepción del Receptor:** El nodo destino recibe `PKT_CHAT`, extrae el `MsgId` y emite de vuelta un paquete `PKT_ACK` hacia el `SrcId` con su RSSI de recepción.
3. **Actualización en Pantalla:** El Cyberdeck emisor recibe el `PKT_ACK`, actualiza el mensaje a `isAcked = true` y dibuja el doble check `✓✓` en color Cyan.

---

## 🧩 5. Plan de Ejecución por Fases

```
┌────────────────────────────────────────────────────────────────────────┐
│                   CRONOGRAMA DE TRABAJO Y ENTREGABLES                  │
├────────────────────────────────────────────────────────────────────────┤
│  FASE 1: Arquitectura Thread-Safe (Buzón C++ Radio ↔ UI)      [✅ LISTA]│
│  FASE 2: Optimización del Chat (Inserción Incremental)        [✅ LISTA]│
│  FASE 3: Control Explícito de Radios (Botón Guardar + HAL)    [✅ LISTA]│
│  FASE 4: Rediseño Visual de Tarjetas en Radar (2 Renglones)   [PENDIENTE]│
│  FASE 5: Libreta de Contactos MessagePack (contacts.msgpack)   [PENDIENTE]│
│  FASE 6: Chat Directo P2P y Selector de Contextos             [PENDIENTE]│
│  FASE 7: Protocolo de Confirmaciones PKT_ACK y Retransmisión  [PENDIENTE]│
│  FASE 8: Validación de Estrés y Pruebas Cruzadas S3 / P4      [PENDIENTE]│
└────────────────────────────────────────────────────────────────────────┘
```

---

## 🧪 6. Protocolo de Pruebas y Criterios de Aceptación

| ID | Prueba | Procedimiento de Validación | Criterio de Aprobación |
| :---: | :--- | :--- | :--- |
| **T-01** | **Legibilidad Radar 3.5"** | Descubrir 5 nodos con nombres largos. | Formato de 2 renglones legible; marquesina circular activa sin colapsos de texto. |
| **T-02** | **Persistencia Flash** | Descubrir nodo `0xCAFE`, apagar S3, encender S3. | El nodo aparece en la lista con su alias y datos intactos desde `contacts.msgpack`. |
| **T-03** | **Mensajería P2P** | Enviar mensaje privado a `0xCAFE`. | El paquete viaja con `DstId = 0xCAFE`; solo ese nodo abre el mensaje. |
| **T-04** | **Confirmación ACK** | Enviar mensaje con solicitud de ACK. | El receptor devuelve `PKT_ACK`; la UI emisora marca doble check `✓✓`. |
| **T-05** | **Ráfaga Multicanal** | Transmitir 30 mensajes rápidos desde la laptop. | 0 fugas de memoria, 0 reinicios, render fluido en pantalla. |
