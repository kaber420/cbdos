# 📡 Arquitectura de Router Multi-Radio Distribuido USB Host (ESP32-P4 + C3/C6/H2)

## 1. Visión General del Sistema

El sistema convierte al **ESP32-P4** con **CBDos** en una estación base / concentrador / router de telecomunicaciones heterogéneo (**Multi-Radio SDR & RF Bridge**). A través de su puerto **USB OTG High-Speed (480 Mbps)** y un concentrador (**USB 2.0 Hub**), gestiona en paralelo múltiples coprocesadores de radio dedicados (**ESP32-C3, ESP32-C6 y ESP32-H2**).

Cada coprocesador actúa como un **Módem / Front-End de RF transparente (Zero-Copy Bridge)** responsable exclusivo de los tiempos críticos de radio y las modulaciones específicas, mientras que el ESP32-P4 centraliza el enrutamiento dinámico (QoS), la fragmentación, el almacenamiento de buffers en PSRAM (32 MB) y la interfaz gráfica de usuario en tiempo real.

---

## 2. Diagrama de Arquitectura de Hardware y Bus USB

```text
                                 ┌──────────────────────────────────────────────┐
                                 │           ESP32-P4 (CBDos Router)            │
                                 │  - CPU RISC-V Dual-Core @ 400 MHz            │
                                 │  - 32 MB Hexal-PSRAM @ 200 MHz               │
                                 │  - Dispatcher & Dynamic QoS Routing Engine   │
                                 │  - UI Terminal y Monitor Gráfico en Pantalla │
                                 └──────────────────────┬───────────────────────┘
                                                        │ USB 2.0 High-Speed
                                                        │ (480 Mbps / D+ D-)
                                               ┌────────┴────────┐
                                               │   HUB USB 2.0   │
                                               │ (Powered Hub 5V)│
                                               └──┬───┬───┬───┬──┘
                  ┌───────────────────────────────┘   │   │   └───────────────────────────────┐
                  │ 12 Mbps Bulk CDC                  │   │                  12 Mbps Bulk CDC │
                  ▼                                   │   │                                   ▼
       ┌─────────────────────┐                        │   │                        ┌─────────────────────┐
       │  ESP32-C3 (Port 1)  │                        │   │                        │ ESP32-C6/H2 (Port 4)│
       │  + SX1262 / LLCC68  │                        │   │                        │ (Radio 802.15.4 SoC)│
       ├─────────────────────┤                        │   │                        ├─────────────────────┤
       │ 📡 LoRa Sub-GHz     │                        │   │                        │ 📡 802.15.4 PHY     │
       │    868 / 915 MHz    │                        │   │                        │    Zigbee / Thread  │
       └─────────────────────┘                        │   │                        └─────────────────────┘
                                                      │   │
                               ┌──────────────────────┘   └──────────────────────┐
                               ▼                                                 ▼
                    ┌─────────────────────┐                           ┌─────────────────────┐
                    │  ESP32-C3 (Port 2)  │                           │  ESP32-C3 (Port 3)  │
                    │  + SX1280 / LR1121  │                           │  (Radio SoC Nativa) │
                    ├─────────────────────┤                           ├─────────────────────┤
                    │ 📡 2.4 GHz FLRC /   │                           │ 📡 ESP-NOW /        │
                    │    LoRa 2.4 GHz     │                           │    ESP-NOW LR       │
                    └─────────────────────┘                           └─────────────────────┘
```

---

## 3. Matriz de Nodos, Bandas y Modulaciones

| Puerto Hub | Coprocesador | Transceptor RF | Banda / Frecuencia | Modulación / Protocolo | Ancho de Canal (BW) | Throughput / Latencia | Caso de Uso Principal |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Port 1** | **ESP32-C3** | Semtech SX1262 / LLCC68 (vía SPI) | 868 MHz (EU) / 915 MHz (US) | **LoRa Sub-GHz** (CSS) | 125 kHz / 250 kHz / 500 kHz | ~0.5 - 22 kbps / Alta latencia | Alcance extremo (10-30 km), penetración en hormigón/muros y telemetría crítica. |
| **Port 2** | **ESP32-C3** | Semtech SX1280 / SX1281 (vía SPI) | 2.4 GHz ISM | **FLRC** (Fast Long Range) / **LoRa 2.4** | 2 MHz / 5 MHz (FLRC hasta 1.2-2.6 MHz BW) | **1 Mbps - 2 Mbps** / **< 1 ms** | Streaming de datos rápido, telemetría pesada, baja latencia y alta inmunidad con FEC. |
| **Port 3** | **ESP32-C3** | Radio Wi-Fi Integrada | 2.4 GHz (Canales 1-14) | **ESP-NOW** / **ESP-NOW LR** (Long Range) | 20 MHz (Estándar) / DSSS Narrow (LR) | ~250 kbps (LR) a 1 Mbps | Comunicación local multi-nodo sin router, broadcast y redes ad-hoc. |
| **Port 4** | **ESP32-C6 / H2** | Radio 802.15.4 Integrada | 2.4 GHz (Canales 11-26) | **IEEE 802.15.4 Raw MAC** / Thread / Zigbee | **5 MHz** (Espaciado estándar IEEE) | 250 kbps / Baja latencia | Redes mesh densas de sensores, domótica y transporte de paquetes estandarizados. |

---

## 4. Rendimiento del Enlace USB y Formato de Paquetes

### 4.1. Rendimiento del Bus USB Host
* **Enlace P4 ➔ Hub:** USB 2.0 High-Speed (**480 Mbps**).
* **Enlace Hub ➔ Nodos C3/C6/H2:** USB 2.0 Full-Speed (**12 Mbps** por puerto mediante Transaction Translator - TT).
* **Throughput Útil por Nodo:** **~8 Mbps (~1 MB/s)** sostenido.
* **Capacidad de Despacho:** A 8 Mbps, cada nodo puede recibir y transmitir entre **3.000 y 5.000 paquetes completos de 250 bytes por segundo**, superando ampliamente el ancho de banda del medio aéreo de cada radio.

### 4.2. Especificación de la Trama de Paquete Binario Dinámico (120 a 250 Bytes)

El intercambio de datos se realiza en formato binario puro utilizando tramas dinámicas optimizadas:

```text
┌──────────────┬──────────────┬────────────────────────┬────────────────────────────────┬─────────────────┐
│ Control (1B) │ Short ID (2B)│ Cabecera Extend. (0-7B)│       Payload Crudo            │ CRC16 / FCS (2B)│
│              │              │ (Ruta / QoS / Time)    │       (110 a 240 Bytes)        │                 │
└──────────────┴──────────────┴────────────────────────┴────────────────────────────────┴─────────────────┘
```

#### Estructura del Byte de Control (Byte 0):
* **Bits [7:6] - Destino / Tipo:**
  * `00`: Control interno y comandos al módem (cambio de canal, potencia TX, sleep).
  * `01`: Comunicación Interzona (entre nodos locales de la red de campo).
  * `10`: Enrutamiento hacia Internet / Gateway WAN (vía interfaz activa del P4).
  * `11`: Paquete de Sincronismo / Keepalive / ACK de transporte.
* **Bits [5:4] - Prioridad / Clase de Servicio (QoS):**
  * `00`: Baja prioridad (Telemetría de fondo).
  * `01`: Normal (Stream continuo).
  * `10`: Alta prioridad / Tiempo Real (Control de actuadores).
  * `11`: Urgente / Alarma (Interrupción inmediata).
* **Bits [3:0] - Tamaño de Cabecera Extendida:**
  * Longitud variable de metadatos de enrutamiento (de 0 a 7 bytes adicionales).

### 4.3. Compatibilidad de Tamaño de Paquete por Medio (Sweet Spot: 120 - 250 Bytes)
* **ESP-NOW:** Límite máximo de payload por paquete: **250 bytes** (*Zero-fragmentation*).
* **BLE 5.0 DLE (Data Length Extension):** Límite MTU: **251 bytes** (*Zero-fragmentation*).
* **FLRC 2.4 GHz (SX1280):** Payload máximo continuo: **254 bytes** (*Zero-fragmentation*).
* **IEEE 802.15.4 (C6/H2):** MTU de capa física PHY: **127 bytes**.
  * *Estrategia de fragmentación:* Para tramas > 120 bytes, el ESP32-P4 divide el paquete en 2 fragmentos contiguos con subcabecera de fragmento de 2 bytes (`SeqNum:8 + FragID:4/Total:4`), y el C6/H2 los transmite en ráfaga.

---

## 5. Arquitectura del Software en CBDos (ESP32-P4)

### 5.1. Módulos del Sistema
1. **`UsbHubManager` / `UsbCdcHostDriver`:**
   - Detecta la conexión del Hub USB y enumera dinámicamente cada interfaz CDC-ACM (`VID: 0x303A` / Espressif USB-Serial-JTAG).
   - Asigna colas RingBuffer individuales para RX y TX independientes en PSRAM.
2. **`MultiRadioRouter` (Dispatcher FreeRTOS Core 0):**
   - Inspecciona el byte de control y las cabeceras de 3-10 bytes.
   - Aplica tablas de rutas dinámicas según métricas de RSSI, SNR, tipo de mensaje y disponibilidad de banda.
3. **`SerialTerminalView` (Capa Gráfica LVGL 9.5):**
   - Permite al usuario monitorizar logs de depuración, estadísticas de paquetes/seg, gráficos de tráfico y consolas CLI interactivas de cada coprocesador de forma individual o agregada.
4. **`UsbFlasherSubsystem`:**
   - Permite flashear en caliente el firmware específico de cada módem (LoRa, FLRC, ESP-NOW o 802.15.4) descargándolo desde la MicroSD del P4 mediante comandos de auto-reset DTR/RTS por USB CDC.

---

## 6. Ventajas Técnicas del Diseño

* **Zero Pin Waste en Host:** Todo el sistema multi-banda se interconecta mediante **un único par diferencial USB (D+/D-)** del ESP32-P4.
* **Aislamiento de Tareas Críticas:** Las tareas con requisitos de tiempo real estricto a nivel de microsegundo (sincronismo de radio, SPI de RF, ACKs) son absorbidas por los coprocesadores C3/C6/H2, liberando al P4 para la GUI fluida a 60 FPS y el procesamiento de red.
* **Modularidad y Escalabilidad:** Es posible añadir o sustituir cualquier módem de radio desconectando y conectando el dongle al Hub USB sin reiniciar el sistema operativo CBDos.
