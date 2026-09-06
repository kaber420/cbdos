# 📻 Borrador de Diseño: Protocolo de Paquetes Crudos sobre IEEE 802.15.4 (`Raw802154`)

## 1. Justificación Técnica: Canal Estrecho vs Wi-Fi
Para enlaces de comunicación de bajo consumo, largo alcance y telemetría ligera en CBDos:

| Métrica | ESP-NOW (Wi-Fi 2.4 GHz) | **Raw IEEE 802.15.4 (ESP32-C6)** |
| :--- | :--- | :--- |
| **Ancho de Banda de Canal** | 20 MHz | **2 MHz (Canal Estrecho @ 5 MHz espaciado)** |
| **Canales RF** | Canales 1 a 13 (2412 - 2472 MHz) | **Canales 11 a 26 (2405 - 2480 MHz)** |
| **Sensibilidad de Recepción**| ~ -97 dBm | **-104 dBm (+7 dBm de ganancia / ~2x alcance)** |
| **Inmunidad a Ruido Wi-Fi** | Vulnerable a saturación de routers | **Excelente (Usa los valles entre canales Wi-Fi 1, 6 y 11)** |
| **Consumo TX/RX** | ~120 - 240 mA | **~30 - 55 mA (Ahorro masivo de batería)** |
| **Tasa de Bits en el Aire** | 1 Mbps (DSSS) o 6-54 Mbps (OFDM) | **250 kbps (O-QPSK DSSS robusto)** |
| **MTU Máxima por Trama** | 250 bytes | **127 bytes (PSDU Máxima)** |

---

## 2. Ventajas del Modo Crudo (`esp_ieee802154`)
1. **Zero Overhead:** No se requiere stack de Thread, ni Zigbee, ni LwIP/IPv6.
2. **Direccionamiento Flexible:** Permite emitir tramas con dirección corta de 16 bits (`0xFFFF` para broadcast universal) o dirección extendida IEEE de 64 bits (EUI-64).
3. **ACK por Hardware:** El transceptor del C6 soporta confirmación de entrega por hardware (Auto-ACK) en menos de 192 microsegundos.
4. **Energy Detect (ED) / CCA:** El C6 mide la energía del canal antes de transmitir (Clear Channel Assessment) para evitar colisiones.

---

## 3. Estructura de Trama Cruda (`Raw802154Frame`)

Las tramas IEEE 802.15.4 estándar se limitan a 127 bytes totales:

```text
 ┌──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
 │ Frame Ctrl   │ Sequence Num │ PAN ID Dest  │ Dest Address │ CBDos Payload│ Frame Check  │
 │  (2 bytes)   │   (1 byte)   │  (2 bytes)   │  (2 bytes)   │  (1-118 B)   │ (FCS: 2 B)   │
 └──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

### Formato del Payload de CBDos (Dentro de la trama 802.15.4):
- **Magic Byte:** `0xCB` (CyBerDeck).
- **Tipo de Mensaje:** Telemetría rápida (7 bytes), Mensaje de Texto P2P o Chunk de Página.
- **Payload Útil:** Hasta **118 bytes por trama**.

---

## 4. Integración en la Arquitectura P4 ↔ C6 (Vía SDIO)

```text
 ┌───────────────────────────┐                     ┌───────────────────────────┐
 │   ESP32-P4 (Host CBDos)   │                     │   ESP32-C6 (Coprocesador) │
 └─────────────┬─────────────┘                     └─────────────┬─────────────┘
               │                                                 │
               │── [CMD_802154_INIT: Canal 15, PAN 0xCAFE] ─────>│ 1. Inicializa esp_ieee802154
               │<── [ACK: READY, MAC: 16-bit / 64-bit] ──────────│
               │                                                 │
               │── [CMD_802154_TX: Dest=0xFFFF, Data (7B)] ─────>│ 2. Emite trama en 250 kbps
               │                                                 │    (Modulación O-QPSK)
               │                                                 │
               │<── [EVT_802154_RX: Src, RSSI, LQI, Data] ───────│ 3. Captura trama del aire
               │                                                 │    y sube datos por SDIO
```

---

## 5. Casos de Uso en CBDos

1. **Beacon de Telemetría y Tiempo:** Transmitir la hora de la red y presencia en micro-ráfagas de 7 bytes con alcance extendido en exteriores o edificios densos.
2. **Chat P2P de Emergencia / Respaldo:** Mensajería de texto en canal estrecho cuando el espectro Wi-Fi 2.4 GHz está completamente saturado.
3. **Control Remoto / Nodos Sensores:** Comunicación con placas satélites basadas en ESP32-C6 / ESP32-H2 operadas con pilas de botón.
