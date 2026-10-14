# Especificación Técnica y Plan de Corrección: Protocolo ESP-SDR en ESP32-S3 y Controles en CBDos

**Documento:** `specs/architecture/plan_correcciones_sdr_s3_protocolo_y_controles.md`  
**Fecha:** Octubre 2026  
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
* **Comportamiento en UI:** Permitir sintonizar libremente en pasos de 1 MHz y 10 MHz entre 1800 y 2700 MHz (o rango extendido), mostrando un indicador de estado visual cuando se opera en banda experimental o fuera de la banda ISM.

---

## 3. Especificación Técnica de la Solución

### 3.1. Capa de Protocolo y Transporte (`EspSdrClient`)

1. **Parada Atómica y Sincronizada con Drenaje de `SPECEND` (`stopStreaming`):**
   * Transmitir salto de línea `\n` al canal CDC.
   * Ejecutar un ciclo de lectura y drenaje hasta capturar la cadena `SPECEND` o agotar un timeout de seguridad (300 ms).
   * Ejecutar `cdc->purge()` para vaciar completamente los buffers de recepción del Host.
   * Esperar a que la tarea FreeRTOS `m_streamTaskHandle` finalice ordenadamente antes de retornar.

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
   * Modo Manual: Transmitir `GAIN MANUAL <index>\n` (índice 0 a 60).

4. **Perfiles de Tasa S3:**
   * Limitar la búsqueda de perfiles `SPEC` a las tasas nativas:
     * `80000000` (80 MS/s, divider 0)
     * `40000000` (40 MS/s, divider 1)
     * `16000000` (16 MS/s, divider 6)
   * Tamaños FFT: 256, 512, 1024, 2048 bins.

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

4. **Secuencia Atómica de Cambio de Canal:**
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

---

## 4. Matriz de Validación y Pruebas

| ID | Prueba | Criterio de Éxito |
| :--- | :--- | :--- |
| **TEST-SDR-01** | Cambio de Canal en Vivo (Ch 1 -> Ch 6 -> Ch 11) | El espectro sintoniza la nueva frecuencia de inmediato sin congelarse ni desconectar USB. |
| **TEST-SDR-02** | Selección de Span 16 MS/s, 40 MS/s y 80 MS/s | El ancho visual del espectro y la FFT se ajustan limpiamente sin cuelgues del S3. |
| **TEST-SDR-03** | Ajuste de Filtro Analógico (13M, 40M, 69M y Auto 0) | Los comandos `BANDWIDTH` se confirman con `OK`; a 13 MHz se observa atenuación visible en los bordes. |
| **TEST-SDR-04** | Sintonía en Modo Experimental 5/6 LO (1.85 GHz) | El S3 engancha el LO fraccionario y grafica la traza RF alrededor de 1850 MHz sin errores. |
| **TEST-SDR-05** | Ciclos Múltiples Start / Stop / Channel Switch | 20 cambios continuos no producen desincronización de paquetes ni pérdidas de tramas. |

---

## 5. Estado del Documento

* **Estado:** ✅ Aprobado y sincronizado con las especificaciones oficiales de hardware.
* **Siguiente Paso:** Implementación del código en `core/src/sdr/` y `core/src/ui/views/`.
