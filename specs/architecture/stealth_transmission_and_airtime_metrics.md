# 📡 Especificación Técnica: Transmisión Furtiva de Ráfaga Ultra-Corta (LPD/LPI), Métricas de Tiempo Aire y Anti-Rastreo en CBDos

## 1. Fundamentos Físicos de la Transmisión por Ráfaga (Burst Transmission)

La doctrina de comunicaciones tácticas seguras define **LPD (Low Probability of Detection)** y **LPI (Low Probability of Intercept)** como la capacidad de transmitir información minimizando la energía total emitida en el espectro electromagnético y reduciendo el tiempo de permanencia en el aire (*Dwell Time*).

CBDos logra niveles de discreción de grado militar combinando **compresión semántica por Bytecode/Diccionario**, **tramas compactas de Capa 2 (ESP-NOW / Raw 802.15.4 / FLRC)** y **cifrado autenticado por hardware (AES-256 + HMAC-SHA256)**.

---

## 2. Análisis Comparativo de Tiempo Aire (Air-Time)

### 2.1. Métricas por Mensaje Típico (15 a 20 Palabras)
- **Texto Plano (ASCII/UTF-8 sin optimizar):** ~90 a 120 bytes.
- **Payload CBDos (Bytecode Denso + Cabecera 3B + HMAC 4B):** **~24 a 32 bytes totales**.

| Tecnología de Radio | Frecuencia / Modulación | Tasa de Bits | Duración en el Aire (32B) | Clasificación de Detección |
| :--- | :--- | :--- | :--- | :--- |
| **Walkie-Talkie FM / Voz** | VHF / UHF (Analógico) | N/A (Voz continua) | **3,000 a 10,000 ms** | 🔴 **Vulnerable / Triangulación Inmediata** |
| **Meshtastic / LoRa Típico** | 915 MHz (LoRa SF10/SF11) | ~1.5 - 5 kbps | **500 a 2,000 ms** | 🔴 **Fácilmente Rastreable** |
| **Opus Audio CBDos (HD)** | 2.4 GHz (Opus @ 12 kbps) | 12 kbps (7.5 KB/5s) | **~50 a 65 ms** | 🟡 **Detectable en ráfagas continuas** |
| **Codec2 Audio CBDos** | 2.4 GHz (Codec2 @ 2.4 kbps)| 2.4 kbps (1.5 KB/5s) | **~15 a 18 ms** | 🟢 **Al límite de integración de receptores** |
| **CBDos Bytecode (802.15.4)**| 2.4 GHz (O-QPSK C6) | 250 kbps | **~1.18 ms** | 🛡️ **Furtivo / Indetectable** |
| **CBDos Bytecode (ESP-NOW)** | 2.4 GHz (DSSS / OFDM) | 1 Mbps | **~0.45 ms** | 🛡️ **Fantasma (Confundido con ruido)** |
| **CBDos Bytecode (FLRC)** | 2.4 GHz (SX1280 GFSK) | 1.3 Mbps | **~0.30 ms** | 🛡️ **Micro-Ráfaga Invisible** |

---

## 3. Impacto Operativo en un Ciclo Diario Completo (24 Horas)

### 3.1. Escenario de Uso Intensivo: 100 Mensajes Diarios
- **Volumen:** 100 mensajes completos de ida y vuelta intercambiados durante el día.
- **Duración unitaria:** 0.0004 segundos (0.4 ms) en ESP-NOW / FLRC.

$$\text{Tiempo Total de Emisión de RF en 24h} = 100 \times 0.0004 \text{ s} = \mathbf{0.04 \text{ segundos}}$$

### 3.2. Métricas de Ocultamiento y Silencio Electromagnético:
- **Segundos totales en el día:** $24 \times 3600 = \mathbf{86,400\text{ segundos}}$.
- **Porcentaje de Silencio Absoluto:**
  $$\text{Silencio} = \frac{86,400 - 0.04}{86,400} \times 100 = \mathbf{99.99995\%}$$
- **Emisión activa:** El transmisor solo existe en el espectro durante el **0.00005% del día**.

---

## 4. Análisis de Vulnerabilidad ante Sistemas Militares de Guerra Electrónica (SIGINT / COMINT)

Los sistemas de goniometría y radiolocalización militar (TDoA - *Time Difference of Arrival* y AoA - *Angle of Arrival*) operan bajo los siguientes umbrales:

```text
 [ Ráfaga de Señal RF ]
          │
          ├─► 0.1 ms - 0.5 ms : Umbral mínimo para *Detección de Energía* (Ruido vs Señal)
          ├─► 1.0 ms - 5.0 ms : Umbral mínimo para *Línea de Rumbo de 1 Estación* (Bearing)
          └─► 10 ms - 50 ms   : Umbral mínimo para *Triangulación de 3+ Estaciones* (Geolocalización Fix)
```

### 4.1. Por qué el Bytecode de CBDos (0.3 ms - 0.4 ms) Anula la Triangulación:
1. **Insuficiencia de Tiempo de Integración:**
   - La duración de 0.3 ms es inferior al tiempo que tardan los lazos de control de ganancia automática (AGC) y los correladores de fase en sincronizarse entre múltiples receptores remotos.
2. **Inmunidad contra Triangulación Manual y Escaneo Secuencial:**
   - Los analizadores de espectro realizan barridos en ciclos de 30 a 100 ms. La probabilidad de interceptar un pulso de 0.3 ms es estadísticamente insignificante.
3. **Firma Gaussiana (Anti-Fingerprinting):**
   - El payload cifrado con **AES-256 + HMAC** tiene una distribución de entropía máxima (ruido blanco). No existen secuencias repetitivas en claro que permitan perfilar la marca o modelo del transmisor.

---

## 5. Rendimiento Energético (Impacto en Batería)

- Consumo en transmisión de radio RF ($I_{TX}$): ~150 mA @ 3.3V.
- Consumo en reposo/escucha ($I_{RX}$): ~15-30 mA.
- Energía consumida en transmisión activa durante todo el día (100 mensajes):
  $$Q = I_{TX} \times t = 150\text{ mA} \times \left(\frac{0.04\text{ s}}{3600\text{ s/h}}\right) = \mathbf{0.00166\text{ mAh}}$$
- **Conclusión de Autonomía:** El consumo de batería atribuible a la transmisión de mensajes durante un día completo es **prácticamente cero**, permitiendo semanas de operación continua con celdas LiPo estándar (18650 / 21700).

---

## 6. Recomendaciones de Implementación Táctica en CBDos

1. **Jittering de Transmisión:** Al transmitir paquetes fragmentados (ej. audio Codec2 o páginas TLVGL), aplicar retardos aleatorios (*Jitter*) de 20 a 50 ms entre micro-chunks para evitar patrones periódicos detectables.
2. **Control Adaptativo de Potencia (ATPC):** Ajustar la potencia de salida de transmisión (+0 dBm a +20 dBm) según el RSSI del último paquete recibido, reduciendo la huella de radio a lo estrictamente necesario.
