# 📡 Plan de Arquitectura: Red Alternet TLVGL sobre ESP-NOW

Este plan describe la arquitectura completa para operar el **Navegador TLV Super Denso** a través del protocolo de radio **ESP-NOW (2.4 GHz)** sin depender de routers Wi-Fi, sin internet y con cero emparejamiento.

---

## 🗺️ 1. Topología del Ecosistema

```
                     ┌────────────────────────────────────────────────────────┐
                     │              NODOS CLIENTE (PANTALLA / CBDos)           │
                     │  1. ESP32-P4 (JC4880 - 480x800) [Vía Coprocesador C6]  │
                     │  2. ESP32-S3 (JC3248 - 320x480) [Radio Nativa]         │
                     └────────────────────────────────────────────────────────┘
                                              ▲     ▲
                        Petición REQ_URL (15B)│     │Respuesta TLV (1-2 Chunks)
                                              ▼     ▼
               ESP-NOW Broadcast (Canal 1) / MAC Directa (2.4 GHz)
                                              ▲     ▲
                                              │     │
                     ┌────────────────────────┴─────┴────────────────────────┐
                     │          NODOS SERVIDORES / SENSORES (HEADLESS)        │
                     │  3. ESP32-S3 Sin Pantalla (Gateway / Micro-Hosting)    │
                     │  4. ESP32-C3 Sin Pantalla (Nodo Sensor / Clima a Batería)
                     └────────────────────────────────────────────────────────┘
```

---

## 📦 2. Protocolo de Micro-Chunking (Fragmentación para Límite de 250B)

ESP-NOW impone un límite de **250 Bytes** por trama. Con el compilador super denso de CBDos:
- Páginas como `config.tlvgl` (**195B**) caben en **1 sola trama**.
- Páginas como `clima.tlvgl` (**321B**) o `bento.tlvgl` (**365B**) requieren solo **2 tramas**.

### Estructura del Paquete ESP-NOW (250 Bytes Máximo)

```
┌─────────────────┬─────────────────┬──────────────────────────────────────────┐
│ Micro-Header 2B │  MeshHeader 3B  │              Payload TLVGL               │
│ [Seq/Tot][MsgID]│  [Ctrl][DstID]  │   [0x10 Page, 0x1B Chart, 0x17 Slider...]│
└─────────────────┴─────────────────┴──────────────────────────────────────────┘
```

1. **Micro-Header de Fragmentación (2 Bytes):**
   - **Byte 0:** `[Chunk Index : 4 bits]` | `[Total Chunks : 4 bits]`
     - `0x00`: Trama única no fragmentada (Total = 1, Index = 0).
     - `0x01`: Fragmento 1 de 2.
     - `0x11`: Fragmento 2 de 2.
   - **Byte 1:** `Message ID (uint8_t)`: Identificador de correlación para asociar los fragmentos al mismo paquete.
2. **MeshHeader (3 Bytes DST_ONLY):**
   - `[Control 1B]` (`0x08` = Response, `0x07` = Request) + `[DstShortID 2B]`.
3. **Carga Útil TLVGL:**
   - Hasta **245 Bytes** de datos por fragmento.

---

## ⚙️ 3. Implementación en los Nodos Cliente (CBDos)

### Target ESP32-S3 (Radio Nativa)
1. **Inicialización:** Inicializa Wi-Fi en modo Station o AP sin conectar a ningún router (`WIFI_MODE_STA`).
2. **Registro ESP-NOW:**
   ```cpp
   esp_now_init();
   esp_now_register_recv_cb(onEspNowDataRecv);
   ```
3. **Envío de Petición:** Broadcast (`FF:FF:FF:FF:FF:FF`) en el canal Wi-Fi fijo (ej. Canal 1) con la trama `TYPE_REQ_URL` de 15 Bytes.
4. **Reensamblado y Renderizado:** Al recibir todos los fragmentos correspondientes a un `Message ID`, entrega el buffer a `TlvBrowserView::render(data, len)`.

### Target ESP32-P4 (Vía Coprocesador ESP32-C6)
1. El ESP32-P4 envía comandos de radio al C6 a través del enlace SDIO (ESP-Hosted).
2. El coprocesador C6 emite y recibe los paquetes ESP-NOW por el aire y los transfiere directamente a la memoria PSRAM del P4.

---

## 🔋 4. Firmware Ligero para Nodos Servidores (ESP32-C3 y ESP32-S3 Headless)

Para los nodos sin pantalla (C3 y S3 headless), se creará un micro-firmware ultra-compacto (< 400 KB de Flash):

```
firmware_sensor_c3/
├── main.cpp              # Inicializa I2C (BME280/AHT20), Wi-Fi y ESP-NOW
├── tlv_pages.h           # Páginas TLV pre-compiladas en memoria Flash (PROGMEM)
└── sensor_engine.cpp     # Lee temperatura/humedad y actualiza los valores del TLV en RAM
```

### Ciclo de Operación de un Sensor C3 a Batería:
1. **Arranque en 15 ms:** Se despierta de Deep Sleep o escucha en modo de bajo consumo.
2. **Escucha de Petición:** Al recibir `REQ_URL: "clima.mesh"` o `0x01`:
   - Lee el sensor I2C en 2 ms (ej. 24.5 °C, 62% humedad).
   - Inyecta los 4 bytes de telemetría y los 8 puntos de gráfica en la plantilla `clima.tlvgl`.
   - Envía los 2 fragmentos ESP-NOW a la dirección MAC del cliente solicitante.
3. **Reposo:** Vuelve a modo de ultra bajo consumo inmediatamente.

---

## 📱 5. Flujo de Usuario en el Navegador CBDos

1. El usuario abre la app **Navegador** en su pantalla táctil (P4 o S3).
2. En la barra de URL escribe `espnow://clima.mesh` o pulsa un marcador de acceso rápido.
3. El microcontrolador emite un pulso ESP-NOW en broadcast.
4. El nodo **ESP32-C3** que esté a menos de 100 metros responde en menos de 10 milisegundos.
5. El navegador dibuja la estación meteorológica con sus gráficas de líneas, barras de progreso y sliders interactivos, **completamente offline en medio del campo o sin red eléctrica**.

---

## 📋 6. Fases de Desarrollo Sugeridas

- [ ] **Fase 1:** Crear el proyecto firmware headless para ESP32-C3 / S3 con lectura de sensores y respuesta TLV por ESP-NOW.
- [ ] **Fase 2:** Implementar la capa de transporte `EspNowTransport` en `cbdos/core` con ensamblado de fragmentos de 250 bytes.
- [ ] **Fase 3:** Probar navegación bidireccional entre la pantalla táctil de CBDos y el nodo sensor autónomo.
