# Especificación Técnica y Plan de Corrección: Protocolo ESP-SDR en ESP32-S3 y Controles en CBDos

**Documento:** `specs/architecture/plan_correcciones_sdr_s3_protocolo_y_controles.md`  
**Fecha:** Octubre 2026  
**Versión:** v2 (v1 preservada en `specs/history/plan_correcciones_sdr_s3_protocolo_y_controles_v1.md`)  
**Target:** ESP32-P4 (Host) + ESP32-S3 Super Mini (Dispositivo SDR ESPARGOS)  
**Módulos Afectados:** `core/include/cbdos/sdr/esp_sdr_client.hpp`, `core/src/sdr/esp_sdr_client.cpp`, `core/src/ui/views/SdrView.hpp`, `core/src/ui/views/SdrView.cpp`

---

## 1. Resumen y Justificación

Durante las pruebas operativas en hardware real y el análisis exhaustivo de la documentación oficial de **ESP-SDR (ESPARGOS)**, se identificaron los requerimientos exactos para alinear **CBDos** con las capacidades reales del silicio del **ESP32-S3**:

1. **Cuelgue en el Cambio de Canal / Frecuencia:** La falta de sincronización con el delimitador `SPECEND\n` y la falta de purga en la FIFO USB CDC colisiona tramas binarias `SPC1` con comandos de texto `FREQ`, provocando la muerte del stream y obligando a reconectar físicamente el cable USB.
2. **Tasas de Muestreo Físicas (Span):** El ESP32-S3 solo soporta **80 MS/s, 40 MS/s y 16 MS/s** en modo continuo con FFT en chip. Las tasas de 4, 8, 10 y 20 MS/s pertenecen a otros chips (C5, C61, S31).
3. **Filtro Analógico (Filter Bandwidth):** El S3 soporta un filtro paso-bajos analógico en el rango de **13 MHz a 69 MHz**, además del valor especial **`0`** para el ajuste más ancho (filtro abierto / bypass).
4. **Sintonía Extendida y Modo Experimental 5/6 LO:** El firmware permite sintonizar entre 100 y 6000 MHz. En el ESP32-S3, se activa automáticamente el modo divisor 5/6 del oscilador local (LO) entre **1842 y 2209 MHz**, extendiendo la recepción verificada hasta los **~1.84 GHz**. Fuera de la banda ISM (2400–2483.5 MHz), se muestra una advertencia informativa sin bloquear la sintonía.
5. **Control de Ganancia:** Soporte para Hardware AGC por defecto (`GAIN HARDWARE`) y ganancia manual por índice (`GAIN MANUAL <index>`).

---

## 2. Diagnóstico Técnico y Fundamentos de Hardware

### 2.1. Cuelgue de Comunicación al Cambiar de Canal

* **Mecanismo del Bug:**
  1. En streaming continuo On-Chip (`SPEC 0 ...`), el S3 emite tramas binarias `SPC1` sin interrupción.
  2. Para detener el stream, el protocolo exige enviar `\n`. El firmware finaliza la trama activa y responde con `SPECEND\n`.
  3. Previamente, `stopStreaming()` emitía `\n` y retornaba de inmediato sin drenar hasta `SPECEND\n`.
  4. Al llamar inmediatamente a `client.tune(freqMHz)` (`FREQ <f>\n`) y `client.startStreaming()`, el texto del comando y los ACKs colisionaban con bytes binarios remanentes en la FIFO USB CDC.
  5. `startStreaming()` fallaba al verificar el encabezado `SPEC`, cancelaba la tarea (`m_streaming = false`) y el canal serie quedaba desincronizado.

### 2.2. Matriz Oficial de Capacidades (ESPARGOS)

| Chip | Continuous RF capture with on-chip FFT | Snapshot FFT | Rango Ancho de Banda Analógico |
| :--- | :--- | :--- | :--- |
| **ESP32-S3** | **256–2048 bins; 16 / 40 / 80 MS/s; native USB** | **—** | **13–69 MHz (y 0 = más ancho)** |
| ESP32-C2 | — | 256–2048 bins; 80 / 40 / 16 MS/s | 12–20 MHz |
| ESP32-C3 | — | 256–2048 bins; 80 MS/s | 14–62 MHz |
| ESP32-C5 | — | 256–2048 bins; 4/8/10/20/40/80 MS/s | 11–48 MHz (5 GHz > 3 GHz) |
| ESP32-C61 / S31 | 256 bins; 4/8/10/20/40/80 MS/s | 512–2048 bins | 13–54 MHz |
| ESP32-S2 | — | 256–2048 bins; 16 / 40 / 80 MS/s | 15–60 MHz |

* **Conclusión para el S3:** Únicamente deben exponerse las tasas **80 MS/s, 40 MS/s y 16 MS/s**.

### 2.3. Sintonía y Modo Experimental 5/6 LO

* **Banda ISM Estándar:** 2400.0 – 2483.5 MHz (cobertura Wi-Fi / Bluetooth / Zigbee).
* **Modo Experimental 5/6 LO (1842 – 2209 MHz):** El ESP32-S3 conmuta internamente el divisor de su oscilador local para enganchar el PLL por debajo de los 2.4 GHz, permitiendo capturar señales en el rango de ~1.84 GHz a ~2.21 GHz.
* **Comportamiento en UI (decisión del usuario, v2):** Permitir sintonizar libremente en pasos de 1 MHz y 10 MHz en el **rango completo del firmware: 100 – 6000 MHz** (clamp en `tuneToFrequency()` y en el gesto de arrastre), mostrando un indicador de estado visual cuando se opera en banda experimental o fuera de la banda ISM.

### 2.4. Diagnóstico de Concurrencia (Auditoría del código real, v2)

La auditoría de `esp_sdr_client.cpp` y `SdrView.cpp` reveló que la causa raíz del cuelgue no es sólo la falta de drenaje de `SPECEND`, sino **acceso concurrente al CDC desde dos tareas**:

| # | Hallazgo | Ubicación | Impacto |
| :--- | :--- | :--- | :--- |
| C1 | `stopStreaming()` espera máx. 200 ms (20×10 ms) y anula el handle aunque la tarea siga viva. La tarea, al salir, emite otro `\n` + `purge()` (L604-607) que puede caer sobre el `FREQ`/ACK de la UI. | `stopStreaming`, `streamTaskLoop` | Desincronización / ACK perdido |
| C2 | Drenar `SPECEND` desde `stopStreaming()` (hilo LVGL) mientras la tarea de stream sigue leyendo crearía **dos lectores simultáneos** del CDC. | Plan v1 §3.1.1 | Tramas partidas |
| C3 | `eTaskGetState()` sobre una tarea auto-eliminada (`vTaskDelete(nullptr)`) no es fiable: el TCB puede haberse liberado/reutilizado. | `stopStreaming` | UB |
| C4 | `m_streaming` es `bool` plano compartido entre Core 0 y el hilo LVGL. | `EspSdrClient` | Data race |
| C5 | `onGainModeClicked` llama `setGain()` con el stream activo → misma colisión texto/binario. `setFilterBandwidth()` "en caliente" la tendría también. | `SdrView` | Cuelgue |
| C6 | El callback (Core 0) reasigna `std::vector m_sharedSpectrum` mientras LVGL lo lee → posible realloc durante la lectura. | `SdrView` | Crash potencial |
| C7 | `tune()`/`setGain()` aceptan cualquier respuesta no vacía (`\|\| !resp.empty()`). | `EspSdrClient` | Falsos positivos |
| C8 | `tune()`/`setGain()` duplicados: en `tuneToFrequency()` y de nuevo al inicio de `streamTaskLoop`. | ambos | Redundancia |
| C9 | `findSpecProfile()` cae a 80M/2048 si la tasa no existe, pero el frame se etiqueta con la tasa pedida. | `EspSdrClient` | Ejes incorrectos |

---

## 3. Especificación Técnica de la Solución

### 3.1. Capa de Protocolo y Transporte (`EspSdrClient`)

1. **Parada Atómica y Sincronizada con Drenaje de `SPECEND` (`stopStreaming`) — Modelo de Propietario Único (v2):**
   * **Regla:** mientras exista la tarea de stream, **sólo ella lee el CDC**. Nadie más escribe comandos de texto.
   * `stopStreaming()` (llamante, p. ej. hilo LVGL):
     1. Registra `xTaskGetCurrentTaskHandle()` como destinatario de notificación.
     2. `m_streaming.store(false)` (`std::atomic<bool>`).
     3. Bloquea en `ulTaskNotifyTake()` con timeout de seguridad (~600 ms). No usa `eTaskGetState()`.
   * `streamTaskLoop()` al detectar `m_streaming == false`:
     1. Transmite `\n` al CDC (único punto que lo hace).
     2. Lee y descarta hasta capturar `SPECEND\n` o agotar 300 ms.
     3. Ejecuta `cdc->purge()`.
     4. Notifica al llamante (`xTaskNotifyGive`) y se auto-elimina.
   * Si el timeout de `stopStreaming()` vence, se registra el fallo y se fuerza `purge()` + re-`SYNC` antes del siguiente comando.

1b. **Secuencia Común Parar → Comando → Reanudar (v2):**
   * Toda orden de control (`FREQ`, `GAIN`, `BANDWIDTH`, cambio de Span/FFT) con stream activo pasa por un helper único en `SdrView` (p. ej. `applyWithStreamPause(std::function<void()>)`) que ejecuta `stopStreaming()` → comando → `startStreaming()`.
   * `streamTaskLoop()` deja de reenviar `FREQ`/`GAIN` al arrancar (C8); el estado de radio se aplica antes de `startStreaming()`.

1c. **Validación de Respuestas (v2):**
   * `tune()`, `setGain()` y `setFilterBandwidth()` dejan de aceptar cualquier línea no vacía (C7). El criterio exacto (`OK` explícito vs. silencio + ausencia de `ERR`) queda **pendiente de verificación en hardware** (ver §3.3).

2. **Control de Filter Bandwidth (`setFilterBandwidth`):**
   * Añadir método público en [EspSdrClient](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/sdr/esp_sdr_client.hpp):
     ```cpp
     bool setFilterBandwidth(uint32_t bwMHz);
     ```
   * Si `bwMHz == 0`, envía `BANDWIDTH 0\n` (filtro abierto / más ancho).
   * Si `bwMHz > 0`, valida y acota en el rango físico $[13, 69]$ MHz y envía `BANDWIDTH <bwMHz>\n`.
   * Verificar respuesta `OK` del firmware.
   * Integrar campo `uint32_t filterBwMHz = 0;` en `SdrConfig`.

3. **Control de Ganancia (`setGain`):**
   * Modo AGC: Transmitir `GAIN HARDWARE\n`.
   * Modo Manual: Transmitir `GAIN MANUAL <index>\n` (índice **0 a 82**, verificado con `LIMITS?` — §3.3).

4. **Perfiles de Tasa S3:**
   * Limitar la búsqueda de perfiles `SPEC` a las tasas nativas:
     * `80000000` (80 MS/s, divider 0)
     * `40000000` (40 MS/s, divider 1)
     * `16000000` (16 MS/s, divider 6)
   * Tamaños FFT: 256, 512, 1024, 2048 bins.
   * Fallback (C9): si la tasa solicitada no existe, se usa 40 MS/s **y se refleja la tasa efectiva** en `SdrFrame::sampleRate`.

5. **Datos Compartidos Stream → UI (C6, v2):**
   * Reemplazar la reasignación de `std::vector` desde el callback por un doble buffer preasignado (2048 floats) protegido por un `portMUX`/mutex de sección crítica corta; el hilo LVGL copia bajo el lock y renderiza fuera de él.

### 3.2. Capa de Interfaz Gráfica (`SdrView` en LVGL v9.5)

1. **Menú de Span (Sample Rate):**
   * Opciones en `m_ddSpan`:
     ```text
     80 MHz (80 MS/s)
     40 MHz (40 MS/s)
     16 MHz (16 MS/s)
     ```
   * Mapeo de selección directo a `80000000`, `40000000`, `16000000`.

2. **Control de Filter Bandwidth en Barra de Herramientas:**
   * Selector interactivo en la barra superior (`m_ddFilterBw`):
     * `BW: Auto (0)` -> Filtro abierto/máximo
     * `BW: 13 MHz` (mínimo físico)
     * `BW: 20 MHz`
     * `BW: 30 MHz`
     * `BW: 40 MHz` (valor típico/default)
     * `BW: 50 MHz`
     * `BW: 60 MHz`
     * `BW: 69 MHz` (máximo físico)
   * Callback `onFilterBwSelected` que invoque `setFilterBandwidth()` en caliente.

3. **Sintonía Frecuencial y Rango Extendido (Modo 5/6 LO):**
   * Presets actualizados en `m_ddPresets`:
     * `1.85 GHz (5/6 LO Exp)`
     * `2.00 GHz (5/6 LO Exp)`
     * `2.20 GHz (5/6 LO Exp)`
     * `Ch 1 (2412 MHz)`
     * `Ch 6 (2437 MHz)`
     * `Ch 11 (2462 MHz)`
     * `Ch 14 (2484 MHz)`
     * `2.60 GHz`
   * Indicador visual en `m_lblMeasBar`:
     * Si la frecuencia está entre 2400 y 2483.5 MHz: texto normal ("ISM 2.4G").
     * Si está entre 1842 y 2209 MHz: badge cian `"5/6 LO Exp"`.
     * Fuera de rango garantizado: badge amarillo `"Extended LO"`.

4. **Secuencia Atómica de Cambio de Canal (aplica también a Gain, Filter BW, Span y FFT — §3.1.1b):**
   * Clamp de frecuencia: 100 – 6000 MHz.
   * `tuneToFrequency()` implementa:
     ```cpp
     if (m_isRunning) {
         client.stopStreaming(); // Drena SPECEND y purga cola serie
         client.tune(freqMHz);   // Canal limpio
         client.startStreaming(m_config, callback); // Reinicio fluido
     } else {
         client.tune(freqMHz);
     }
     ```

### 3.3. Verificación de Protocolo en Hardware (✅ verificado 2026-10-13)

Verificado con el S3 conectado directamente al PC (`/dev/ttyACM0`, script `probe_s3_protocol.py`). Firmware: `VERSION {"revision":"e74f2a470972ec163c247f1fe88d32e08579242a","build_date":"2026-10-08","profile":"esp32s3"}`.

| Comando | Respuesta real |
| :--- | :--- |
| `FREQ 2437/1850/2000/2700/6000` | `OK\n` |
| `FREQ 99` | `ERR command\n` |
| `GAIN HARDWARE`, `GAIN MANUAL 40` | `OK\n` |
| `GAIN MANUAL 99` | `ERR command\n` |
| `BANDWIDTH 0/13/40/69` | `OK\n` |
| `BANDWIDTH 5`, `BANDWIDTH 100` | `ERR command\n` |
| `RANGE?` | `RANGE 100 6000 1` |
| `LIMITS?` | `{"gain":[0,82,1],"bandwidth":[13,69,1,0],"rates":[80000000,40000000,16000000],"bits":[8,10]}` |
| `INFO` / `CAPS` | `S3SDR 6 burst 16380` / `... TUNEEXT RX40 RX16 LPFANA GAIN HWAGC IQ8 RING SPEC SPECN ...` |
| `SPEC 0 <p3> <upf> <hold> <div> <n> 1` | ACK `SPEC <n> <rate> 12288 <freqMHz>` y luego tramas binarias `SPC1` (+ `SPS1` ocasionales) |
| `\n` durante stream | `SPECEND <stats...>\n` en **~50 ms** (80M/2048 y 40M/512); **0 bytes residuales** tras `SPECEND` |

**Consecuencias para la implementación:**
* Criterio de validación estricto: éxito sólo si se recibe la línea `OK`; `ERR` = fallo.
* **Ganancia manual: rango 0–82** (no 0–60).
* Drenaje de `SPECEND` con timeout de 300 ms (holgura ×6 sobre los 50 ms medidos).
* Tabla de perfiles `SPEC` del S3 según `SPECINFO?` (difiere de la v1 en 80 MS/s):

| Rate | Div | 256 | 512 | 1024 | 2048 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 16 MS/s | 6 | 2,1 | 2,2 | 2,4 | 3,7 |
| 40 MS/s | 1 | 5,3 | 5,5 | 5,9 | 7,17 |
| 80 MS/s | 0 | 10,6 | **12,10** | **14,16** | **14,30** |

(valores `param3,upf`)

---

## 4. Matriz de Validación y Pruebas

| ID | Prueba | Criterio de Éxito |
| :--- | :--- | :--- |
| **TEST-SDR-01** | Cambio de Canal en Vivo (Ch 1 -> Ch 6 -> Ch 11) | El espectro sintoniza la nueva frecuencia de inmediato sin congelarse ni desconectar USB. |
| **TEST-SDR-02** | Selección de Span 16 MS/s, 40 MS/s y 80 MS/s | El ancho visual del espectro y la FFT se ajustan limpiamente sin cuelgues del S3. |
| **TEST-SDR-03** | Ajuste de Filtro Analógico (13M, 40M, 69M y Auto 0) | Los comandos `BANDWIDTH` se confirman con `OK`; a 13 MHz se observa atenuación visible en los bordes. |
| **TEST-SDR-04** | Sintonía en Modo Experimental 5/6 LO (1.85 GHz) | El S3 engancha el LO fraccionario y grafica la traza RF alrededor de 1850 MHz sin errores. |
| **TEST-SDR-05** | Ciclos Múltiples Start / Stop / Channel Switch | 20 cambios continuos no producen desincronización de paquetes ni pérdidas de tramas. |
| **TEST-SDR-06** | Toggle AGC/Manual y Filter BW con stream activo | El stream se pausa/reanuda limpiamente; sin cuelgue ni reconexión USB. |
| **TEST-SDR-07** | Arrastre rápido en el gráfico / pulsaciones repetidas de ±1 MHz | Sin crash por acceso concurrente al espectro compartido; `stopStreaming()` nunca vence su timeout. |

---

## 5. Estado del Documento

* **Estado:** ✅ v1 aprobada. v2 añade el diagnóstico de concurrencia (§2.4), el modelo de propietario único (§3.1.1), la secuencia común (§3.1.1b), el rango 100–6000 MHz y la verificación de protocolo (§3.3).
* **Siguiente Paso:** Verificación §3.3 en hardware → implementación en `core/src/sdr/` y `core/src/ui/views/` → build P4 + S3.
