# Propuesta Técnica: Analizador de Espectro y Sniffer SDR en ESP32-P4 vía ESP32-C6 por SDIO

> **Estado:** Propuesta Técnica / Plan de Arquitectura  
> **Fecha:** 5 de Octubre de 2026  
> **Target Primario:** Guition JC4880P443C (ESP32-P4 RISC-V @ 400 MHz + ESP32-C6 por SDIO)  
> **Memoria:** 32 MB Hexal-PSRAM @ 200 MHz (800 MB/s pico)  
> **Framework Gráfico:** LVGL v9.5 estricto sobre panel ST7701S (MIPI-DPI 480×800)  

---

## 1. Visión General del Proyecto

Aprovechar el descubrimiento del proyecto de código abierto **ESP-SDR** (desarrollado por el equipo de ESPARGOS) para dotar a **CBDos** en el **ESP32-P4** de una suite de análisis de radiofrecuencia (SDR) en la banda de 2.4 GHz, **sin añadir un solo componente de hardware adicional**.

La placa base oficial de CBDos (**Guition JC4880P443C**) ya incluye físicamente en su diseño un coprocesador **ESP32-C6 (U3)** soldado e interconectado directamente con el ESP32-P4 mediante un bus **SDIO de alta velocidad** y líneas de control de reset y arranque. 

Al combinar el bypass de ADC de radio de ESP-SDR en el C6 con el rendimiento masivo del P4 (Dual-Core RISC-V @ 400 MHz, bus SDIO 50 MHz y 32 MB de Hexal-PSRAM @ 200 MHz), CBDos se convierte en el **primer Cyberdeck autónomo con analizador de espectro por barrido a ráfagas y sniffer 2.4 GHz por duty-cycle**.

---

## 2. Topología de Hardware y Pines Verificados (JC4880P443C)

Todos los pines necesarios ya están soldados y ruteados en el PCB de la placa (verificados contra `specs/hardware/pinouts_and_ports.md`):

```
┌───────────────────────────────────────┐             ┌────────────────────────────────────────┐
│             ESP32-P4                  │             │          ESP32-C6 (Módulo U3)          │
│   (Host Master - 400 MHz RISC-V)     │             │     (Front-End RF 2.4 GHz SDR)         │
│                                       │             │                                        │
│   GPIO 14 ────────────────────────────┼─ SDIO_D0 ───┼─ GPIO 18 (SDIO Data 0)                 │
│   GPIO 15 ────────────────────────────┼─ SDIO_D1 ───┼─ GPIO 19 (SDIO Data 1)                 │
│   GPIO 16 ────────────────────────────┼─ SDIO_D2 ───┼─ GPIO 20 (SDIO Data 2)                 │
│   GPIO 17 ────────────────────────────┼─ SDIO_D3 ───┼─ GPIO 21 (SDIO Data 3)                 │
│   GPIO 18 ────────────────────────────┼─ SDIO_CLK ──┼─ GPIO 17 (SDIO Clock 50 MHz)           │
│   GPIO 19 ────────────────────────────┼─ SDIO_CMD ──┼─ GPIO 16 (SDIO Command)                │
│                                       │             │                                        │
│   GPIO 54 ────────────────────────────┼─ CHIP_PU ───┼─ EN (Reset C6 / Conmutador de Modo)   │
│   GPIO 6  ────────────────────────────┼─ IRQ/HS ────┼─ GPIO 2 (Interrupción de Datos)        │
│                                       │             │                                        │
│   [32 MB Hexal-PSRAM @ 200 MHz]      │             │   [Antena Cerámica ANT1 (2.4 GHz)]     │
│   [ST7701S MIPI-DPI 480x800 @ 60fps]  │             │   [Bypass ADC RF @ 80 MSa/s]           │
└───────────────────────────────────────┘             └────────────────────────────────────────┘
```

### Ancho de Banda y Rendimiento del Bus (modelo por ráfagas)
* **Bus SDIO:** 4 bits @ 50 MHz $\rightarrow$ Ancho de banda teórico de 200 Mbps (~25 MB/s). Rendimiento sostenido con DMA en ESP-IDF: **~18 a 20 MB/s para vaciado de ráfagas**.
* **Formato I/Q de Muestras:** 8 bits I + 8 bits Q = 2 bytes por muestra.
* **Captura en C6:** ráfagas de ~0.5-1.6 ms a 80 MSa/s nominal a SRAM interna (~512 KB). Sin tasa continua: duty-cycle 5-10%.
* **Transferencia C6 $\to$ P4:** vaciado de cada ráfaga por SDIO (~15-25 ms por ráfaga típica). El P4 procesa por DMA mientras el C6 captura la siguiente.
* **Ancho de Banda RF Instantáneo:** **hasta ~12-54 MHz durante la ráfaga** (según ESP-SDR en C6). Entre ráfagas hay tiempo ciego por vaciado/procesado.
* **Impacto en Hexal-PSRAM:** tráfico a ráfagas, media <2 MB/s $\rightarrow$ **<1% del ancho de banda de la RAM**.

---

## 3. Arquitectura del Sistema por Capas

```
┌────────────────────────────────────────────────────────────────────────┐
│                      CAPA 4: PRESENTACIÓN (LVGL 9.5)                   │
│   - RadioSpectrumView (Gráfica dBm vs Frecuencia 2.400 - 2.483 GHz)    │
│   - WaterfallView (Cascada de calor cromática a 30-60 FPS)             │
│   - ProtocolLoggerView (Decodificación de paquetes nRF24 y 802.15.4)   │
└──────────────────────────────────▲─────────────────────────────────────┘
                                   │ Notificaciones / Eventos
┌──────────────────────────────────┴─────────────────────────────────────┐
│                 CAPA 3: MOTOR DSP EN CORE 1 DEL ESP32-P4               │
│   - Tarea aislada en Core 1: sdr_dsp_task (FreeRTOS)                   │
│   - FFT de 512/1024 puntos con ventana Hanning/Blackman                │
│   - Correlador DSSS para preámbulos Zigbee/Thread (802.15.4)           │
│   - Discriminador de frecuencia para preámbulos GFSK (nRF24L01)        │
└──────────────────────────────────▲─────────────────────────────────────┘
                                   │ Lectura RingBuffer (Zero-Copy)
┌──────────────────────────────────┴─────────────────────────────────────┐
│                   CAPA 2: DRIVER HAL SDIO EN ESP32-P4                  │
│   - HalSdrSdio: SDIO Host Controller con descriptores DMA enlazados    │
│   - Doble buffer ping-pong para ráfagas asignado en Hexal-PSRAM (2x 512 KB) │
│   - Control de alimentación/reset del C6 (GPIO 54) y handshake (GPIO 6)│
└──────────────────────────────────▲─────────────────────────────────────┘
                                   │ Bus Físico SDIO (vaciado de ráfagas ~20 MB/s pico)
┌──────────────────────────────────┴─────────────────────────────────────┐
│                 CAPA 1: FIRMWARE SDR DEL ESP32-C6                      │
│   - Modificación de ESP-SDR adaptado para slave SDIO                   │
│   - Bypass de módem mediante adctrig / librftest a 80 MSa/s nominal    │
│   - Captura a ráfagas a SRAM interna (trigger -> dump -> send -> repeat)│
│   - Sin decimación en C6: se envía ráfaga cruda u espectro pre-calculado │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Las Dos Opciones de Arquitectura y Despliegue

Para adaptarse tanto a un dispositivo de uso diario como a una estación de radio táctica dedicada, CBDos contempla dos modelos de operación claramente diferenciados:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        OPCIÓN A: CYBERDECK DE USO MIXTO (HÍBRIDO)                     │
│  - Un único firmware fusionado en el C6 (ESP-Hosted + ESP-SDR en ~1.5 MB Flash).       │
│  - Conmutación en RAM vía comandos de control SDIO (< 50 ms, sin reinicio ni flasheo). │
│  - Confirmación explícita del usuario mediante diálogo modal en LVGL 9.5.              │
└────────────────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────────────────┐
│                   OPCIÓN B: ESTACIÓN SIGINT / SDR TÁCTICA DEDICADA                     │
│  - C6 dedicado 100% como SDR Front-End por ráfagas en 2.4 GHz (vaciado por SDIO).│
│  - Cero overhead de Wi-Fi; máxima sensibilidad y tasa de muestreo.                     │
│  - Comunicaciones y TX cubiertos por módulos Semtech en cabecera JP1 (SPI):           │
│      • SX1262 (Sub-GHz 433/868/915 MHz LoRa/FSK)                                      │
│      • SX1280 (2.4 GHz LoRa / FLRC 1.3 Mbps / Ranging)                                │
│      • LR1121 (Multi-Banda Sub-GHz + 2.4 GHz + Satelital S-Band 2.1 GHz)               │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

### Opción A: Firmware Híbrido Unificado (ESP-Hosted + ESP-SDR)

* **¿Cómo opera?**
  1. En la memoria Flash del C6 (4 MB) se compila un único binario que integra la pila de **ESP-Hosted** (1.2 MB) y el núcleo de **ESP-SDR** (250 KB).
  2. En el bus SDIO se define una nueva interfaz multiplexada: `IF_TYPE_SDR`.
  3. Al abrir la app *RF Scanner*, el P4 detecta si el C6 está en modo Wi-Fi y despliega una ventana modal en LVGL 9.5:
     ```text
     ┌─────────────────────────────────────────────────────────┐
     │ ⚠️  Coprocesador en Modo Wi-Fi Estándar                 │
     ├─────────────────────────────────────────────────────────┤
     │ El módulo ESP32-C6 está operando como tarjeta de red.   │
     │ Para usar el Analizador SDR es necesario conmutar el    │
     │ modo de la radio interna.                               │
     │                                                         │
     │ • Se suspenderá la conexión Wi-Fi actual.               │
     │ • Conmutación en RAM instantánea (~30 ms).              │
     │ • Cero desgaste de memoria Flash.                       │
     │                                                         │
     │     [ Cancelar y Salir ]       [ Conmutar a Modo SDR ]   │
     └─────────────────────────────────────────────────────────┘
     ```
  4. Si el usuario confirma, el P4 envía el comando SDIO `CMD_SET_RADIO_MODE(MODE_SDR)`:
     * El C6 detiene limpiamente el stack Wi-Fi (`esp_wifi_stop()`).
      * Activa el bypass de radio (`adctrig`) y empieza a transmitir ráfagas I/Q por `IF_TYPE_SDR`.
     * **Cero reinicios físicos y cero escrituras en Flash.**
  5. Al cerrar la app, el comando `CMD_SET_RADIO_MODE(MODE_WIFI)` reactiva la red normal.

---

### Opción B: Cyberdeck SIGINT Dedicado + Suite Multi-Radio Semtech

Si se destina una unidad del **ESP32-P4 como terminal de radiofrecuencia y guerra electrónica dedicada**:
* **El ESP32-C6 no hace Wi-Fi:** Su único propósito en la vida es actuar como un receptor SDR sintonizable de 2.4 GHz por ráfagas (captura a 80 MSa/s en SRAM, vaciado por SDIO a Hexal-PSRAM del P4).
* **Transmisión y Telemetría Táctica (TX/RX):**
  Como el C6 está ocupado en RX SDR, las comunicaciones bidireccionales de CBDos se delegan a transceptores de hardware dedicados conectados al bus **SPI y pines de control en la cabecera JP1**:

| Transceptor | Banda de Frecuencia | Modulaciones Soportadas | Rol en el Cyberdeck Dedicado |
| :--- | :---: | :--- | :--- |
| **Semtech SX1262** | **Sub-GHz** (150 – 960 MHz: 433 / 868 / 915 MHz) | LoRa (+22 dBm), (G)FSK | **Red Táctica de Largo Alcance:** Enlace de datos de kilómetros para chat mallado, telemetría y balizas fuera de cobertura celular. |
| **Semtech SX1280** | **2.4 GHz ISM** | LoRa 2.4 GHz, FLRC (hasta 1.3 Mbps), (G)FSK | **Canal Táctico Rápido y Ranging:** Transmisión de alta velocidad (FLRC) y medición de distancia por tiempo de vuelo (radar/localización de nodos). |
| **Semtech LR1121** (LoRa Edge) | **Multi-Banda Ultra-Versátil**:<br>• Sub-GHz (150–960 MHz)<br>• 2.4 GHz ISM<br>• Satelital S-Band (1.9–2.2 GHz) | LoRa terrestre y satelital, FLRC, FSK, escaneo pasivo Wi-Fi/GNSS | **El Santo Grial Multi-Espectro:** Permite enlazar con satélites LEO (LoRaWAN directo a satélite), escanear geolocalización GNSS/Wi-Fi sin GPS activo y transmitir en todas las bandas autorizadas. |

#### Topología de la Estación SIGINT Táctica:
```
                               ┌────────────────────────────────────────────────────────┐
                               │             ESP32-P4 (Cerebro SIGINT / CBDos)          │
                               │   - Dual-Core RISC-V @ 400 MHz                         │
                               │   - 32 MB Hexal-PSRAM @ 200 MHz (800 MB/s bus)        │
                               │   - Pantalla ST7701S MIPI-DPI 480x800 @ 60 FPS         │
                               └───────────▲───────────────────────────────▲────────────┘
                                           │                               │
                [SDIO 4-bit / 20 MB/s]     │                               │ [Bus SPI / JP1]
                                           ▼                               ▼
       ┌──────────────────────────────────────┐          ┌───────────────────────────────────┐
       │         ESP32-C6 (Oídos SDR)         │          │     SUITE SEMTECH (Voz / TX/RX)   │
       │                                      │          │                                   │
        │ • Bypass ADC RF (ESP-SDR)            │          │ • SX1262: LoRa Sub-GHz (+22dBm)   │
        │ • Captura I/Q por ráfagas a 80 MSa/s │          │ • SX1280: FLRC 2.4G & Ranging     │
        │ • Escáner de 2.4 GHz por barrido     │          │ • LR1121: Multi-Banda + Satélite  │
        │ • Sniffer por duty-cycle nRF24/Zigbee│          │ • Malla CBDos / Enlace Táctico    │
       └──────────────────────────────────────┘          └───────────────────────────────────┘
```

Esta configuración convierte al Cyberdeck en una **estación de radio táctica completa**:
1. **El C6 escucha todo en 2.4 GHz** (espectro visual, interferencias, balizas enemigas o desconocidas).
2. **Los módulos Semtech transmiten y reciben paquetes mallados cifrados** en Sub-GHz o 2.4 GHz sin interferir con la recepción del SDR.
3. **El P4 procesa ambas cosas simultáneamente** gracias a sus 400 MHz dual-core y su memoria Hexal masiva.

---

## 5. Capacidades Funcionales en CBDos y Realismo de Espectro

### A. Analizador de Espectro: Instantáneo vs Barrido
* **Banda Instantánea (hasta ~12-54 MHz durante la ráfaga, según C6):** Permite monitorear 1-2 canales Wi-Fi o varios Zigbee/nRF24 solo durante la ráfaga (~1 ms). Entre ráfagas hay tiempo ciego de vaciado.
* **Banda Completa (83.5 MHz de la banda ISM 2.4 GHz):** Para cubrir de 2.400 a 2.4835 GHz, el C6 realiza un **barrido ágil del sintetizador PLL (Frequency Hopping Sweep)** en saltos sucesivos.
  * *Realidad de ingeniería:* Durante el barrido global hay tiempos ciegos inevitables entre canales, pero permite un refresco de cascada visual de 15 a 30 FPS muy fluido en el panel ST7701S.

### B. Sniffer de Protocolos (Canal Fijo Dedicado, por duty-cycle)
* **nRF24L01+:** Sintonizado en canal fijo o salto selectivo en canales típicos (ej. canales de periféricos Logitech o drones), decodificando paquetes GFSK en Core 1 del P4 a partir de ráfagas.
* **Zigbee / Thread (IEEE 802.15.4):** Sintonizado en canal fijo de domótica (ej. Canales 11, 15, 20 o 25) para captura por ráfagas con duty-cycle (sin garantía de trama 100%).

---

## 6. Plan de Implementación por Fases (Estricto / Fase 1 Condicionante)

> ⚠️ **REGLA DE ORO:** Las Fases 2 a 5 están **estrictamente bloqueadas** hasta que la Fase 1 valide empíricamente en el hardware real que el C6 como SDIO-Slave sostiene el caudal requerido.

| Fase | Tarea Principal | Criterio de Aceptación (Go / No-Go) |
| :---: | :--- | :--- |
| **Fase 1 (Bloqueante)** | **PoC de Transporte SDIO C6 $\to$ P4 por ráfagas** | • Medir vaciado de ráfagas por SDIO a 50 MHz con datos sintéticos DMA en Hexal-PSRAM.<br>• **Meta:** pico $\ge 16 \text{ MB/s}$ durante el vaciado, duty 5-10%.<br>• Criterio: FFT/espectro estable por barrido, modo ráfagas únicamente. |
| **Fase 2** | **Port de ESP-SDR con Salida SDIO por ráfagas** | Modificar ESP-SDR en C6 integrando el driver `sdio_slave` en modo trigger-dump-send-repeat a 80 MSa/s nominal. Sin decimación (no fiable en C6). Validar dual-OTA con GPIO 6. |
| **Fase 3** | **Pipeline DSP en Core 1 del P4** | Implementar `SdrDspEngine` (FFT y demoduladores GFSK/DSSS) aislado en Core 1 sin afectar los 60 FPS de LVGL en Core 0. |
| **Fase 4** | **Vista LVGL 9.5 (`SpectrumView`)** | Canvas de cascada cromática acelerado por PPA y gráfica de barras dBm. |
| **Fase 5** | **Logging & Captura PCAP** | Almacenamiento de paquetes sniffados en tarjeta MicroSD (`/sdcard/captures/`). |

---

## 7. Evaluación de Riesgos y Mitigaciones

1. **Latencia del SDIO Slave en el C6:**
   * *Riesgo:* Que el C6 sufra desbordamiento de buffer (*underrun/overflow*) si la ráfaga no se vacía antes del siguiente trigger.
   * *Mitigación:* Modelo trigger-dump-send-repeat con duty 5-10% y handshake por GPIO 6; sin tasa continua ni decimación 80→10.
2. **Temperatura del C6:**
   * *Riesgo:* Mantener el frontend RF muestreando a máxima velocidad eleva la temperatura del silicio.
   * *Mitigación:* Duty-cycle obligatorio por diseño (ráfagas + descanso) y monitoreo térmico interno del C6.
3. **Licenciamiento GPL-3.0:**
   * *Riesgo:* Contaminación de la base de código del Core de CBDos.
   * *Mitigación:* El firmware del C6 es un ejecutable 100% independiente (código abierto GPL-3.0). El ESP32-P4 solo interactúa con él a través de una especificación abierta de tramas sobre SDIO, preservando el Core de CBDos limpio y desacoplado.
