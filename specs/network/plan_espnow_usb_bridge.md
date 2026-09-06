# 📡 Plan de Implementación: Puente USB Serial ↔ ESP-NOW (Dongle Gateway)

Este plan especifica la creación del **Dongle USB ESP-NOW**, un dispositivo puente basado en **ESP32-C3** o **ESP32-S3 sin pantalla** que se conecta por puerto USB a tu computadora y permite que los servidores en la PC (`tlvgl_server.py`, microservicios locales o proxies) transmitan y reciban tramas TLV por el aire vía **ESP-NOW (2.4 GHz)** hacia dispositivos con **CBDos** sin necesidad de routers Wi-Fi ni internet.

---

## 🏛️ 1. Arquitectura del Sistema

```
┌────────────────────────────────────────────────────────────┐
│                       TU COMPUTADORA                       │
│                                                            │
│   Servidores Locales (Python / Go / C++):                  │
│   • tlvgl_server.py (Hosting Alternet .mesh - 0x07)        │
│   • web_proxy_gateway (Proxy Web Internet - 0x05)          │
│   • Broker de Mensajería / Telemetría                      │
│                                                            │
│   Conector: espnow_serial_bridge.py (PySerial)             │
└─────────────────────────────┬──────────────────────────────┘
                              │ Puerto USB (/dev/ttyACM0 o /dev/ttyUSB0)
                              │ Trama Serial Enmarcada (SLIP / Header 0xAA 0x55)
                              │
┌─────────────────────────────▼──────────────────────────────┐
│        DONGLE USB (ESP32-C3 o ESP32-S3 Headless)           │
│                                                            │
│   Firmware: tools/espnow_usb_bridge                        │
│   • Recibe trama por USB Serial  ──▶ Emite por ESP-NOW     │
│   • Recibe trama por ESP-NOW     ──▶ Envía por USB Serial  │
│   • Indicador LED de actividad TX / RX                     │
└─────────────────────────────▲──────────────────────────────┘
                              │
                       Radio ESP-NOW (2.4 GHz - Canal Fijo)
                       (Alcance hasta 100m, Cero Router)
                              │
┌─────────────────────────────▼──────────────────────────────┐
│               DISPOSITIVOS PORTÁTILES CBDos                │
│   • ESP32-P4 (JC4880P443C 480x800) [Vía Coprocesador C6]   │
│   • ESP32-S3 (JC3248W535 320x480) [Radio Wi-Fi Nativa]     │
└────────────────────────────────────────────────────────────┘
```

---

## 📐 2. Protocolo de Enmarcado Serial (PC ↔ Dongle USB)

Para garantizar integridad y evitar colisiones de bytes entre la PC y el Dongle USB, se utiliza un protocolo de enmarcado ligero con suma de verificación (Checksum):

```
┌───────────┬──────────────┬───────────────┬──────────────────────────────┬──────────┐
│ Magic 2B  │ Dirección 1B │ Longitud 2B   │ Payload (MeshHeader + TLV)   │ CRC8 1B  │
│ 0xAA 0x55 │ 0x01 TX/0x02 │ [Len_H][Len_L]│ Hasta 250 Bytes              │ Checksum │
└───────────┴──────────────┴───────────────┴──────────────────────────────┴──────────┘
```

- **`Magic (0xAA 0x55)`:** Marcador de inicio de paquete.
- **`Dirección (1 Byte)`:**
  - `0x01`: De PC a Dongle (para emitir por el aire).
  - `0x02`: De Dongle a PC (paquete capturado en el aire).
- **`Longitud (2 Bytes)`:** Tamaño del payload útil (1 a 250 Bytes).
- **`Payload (N Bytes)`:** Trama completa (incluyendo Micro-Header de fragmentación + MeshHeader + TLV).
- **`CRC8 (1 Byte)`:** Checksum XOR/CRC8 para validar que la transmisión USB no sufrió corrupción.

---

## 🛠️ 3. Componentes a Desarrollar

### A. Micro-Firmware del Dongle (`tools/espnow_usb_bridge/`)
- Proyecto multiplataforma (ESP-IDF y PlatformIO) compatible con:
  - **ESP32-C3** (SuperMini / Xiao C3 / NodeMCU C3).
  - **ESP32-S3** (Zero / DevKit / Headless con USB-CDC nativo).
- **Características:**
  - Inicialización Wi-Fi en modo Station sin conexión (Canal 1 por defecto).
  - Inicialización de `esp_now` con par broadcast (`FF:FF:FF:FF:FF:FF`) y unicast automático.
  - Tarea UART/USB-CDC de lectura en búfer circular.
  - Callback de recepción ESP-NOW con reenvió instantáneo hacia el puerto Serial.
  - LED parpadeante en actividad de paquetes.

### B. Módulo de Transporte Serial en Python (`tools/tlvgl_gateway/serial_transport.py`)
- Clase `SerialTransport` en Python usando `pyserial`:
  - Detección automática del puerto serie (`/dev/ttyACM*` o `/dev/ttyUSB*`).
  - Hilo en segundo plano que escucha tramas del Dongle USB.
  - Al recibir una petición `TYPE_REQ_URL` desde el aire:
    - La entrega al servidor `tlvgl_server.py` o al proxy.
    - El servidor genera la respuesta en TLV super denso.
    - Divide la respuesta en micro-chunks de $\le 240$ bytes si es necesario.
    - Las envía por el puerto serie al Dongle para su emisión inmediata.

### C. Soporte en el Navegador de CBDos (`TlvBrowserView`)
- Integración del modo `espnow://`:
  - Si la URL empieza por `espnow://...` o se activa el modo radio en la HeaderBar, el navegador conmuta el transporte hacia el módulo ESP-NOW de la capa HAL de red.

---

## 📁 4. Estructura de Archivos del Proyecto

```
cbdos/
├── tools/
│   ├── espnow_usb_bridge/             # Proyecto de firmware para el Dongle USB
│   │   ├── platformio.ini             # Configuración para ESP32-C3 y ESP32-S3
│   │   └── src/
│   │       ├── main.cpp               # Lógica del puente Serial <-> ESP-NOW
│   │       └── packet_framing.h       # Enmarcado y cálculo de CRC
│   └── tlvgl_gateway/
│       ├── tlvgl_server.py            # Servidor actualizado con soporte dual (TCP + Serial)
│       └── serial_transport.py        # Driver de comunicación con el Dongle USB
└── docs/
    └── network/
        ├── plan_tlvgl_esp_now.md      # Protocolo de fragmentación ESP-NOW
        └── plan_espnow_usb_bridge.md  # Este plan de implementación del Dongle
```

---

## 🧪 5. Plan de Verificación y Pruebas

1. **Prueba de Enmarcado Serial:**
   - Conectar el ESP32-C3 por USB a la PC.
   - Ejecutar un script de prueba en Python que envíe tramas de ping/pong al Dongle y verificar respuesta con CRC correcto.
2. **Prueba de Emisión de Radio:**
   - Poner el Dongle a emitir una trama periódica `clima.tlvgl`.
   - Con el ESP32-P4 o ESP32-S3 con CBDos, capturar los paquetes en el aire y verificar que el navegador renderice la estación meteorológica con sus gráficas de líneas y barras.
3. **Prueba Bidireccional Completa:**
   - Desde la pantalla táctil de CBDos, tocar el enlace "Bento Grid".
   - El cliente emite `TYPE_REQ_LINK_CLICK` por ESP-NOW.
   - El Dongle USB lo recibe y lo entrega por Serial a la PC.
   - `tlvgl_server.py` en la PC despacha `bento.tlvgl` en 2 ms hacia el Dongle.
   - El Dongle lo emite por el aire y la pantalla de CBDos cambia de vista automáticamente.
