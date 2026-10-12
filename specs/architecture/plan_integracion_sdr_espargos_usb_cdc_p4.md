# Plan Técnico y de Pruebas: Integración ESP-SDR (ESPARGOS) vía USB Host CDC en ESP32-P4

**Documento:** `specs/architecture/plan_integracion_sdr_espargos_usb_cdc_p4.md`  
**Rama:** `feature/sdr-usb-espargos`  
**Estado:** 🔍 Plan de Desarrollo y Pruebas  
**Fecha:** Octubre 2026  
**Target:** ESP32-P4 (Guition JC4880P443C) + Mochila ESP32-S3 Super Mini  
**Módulos Afectados:** `bsp/esp32_p4_jc4880/hal/`, `core/include/cbdos/sdr/`, `core/src/sdr/`, `core/src/ui/views/`

---

## 1. Resumen y Objetivo

Integrar la funcionalidad de análisis de radiofrecuencia (SDR) en la banda de 2.4 GHz en **CBDos** sobre la plataforma **ESP32-P4**, conectando un **ESP32-S3 Super Mini** con el firmware original de código abierto **ESP-SDR** (del ecosistema ESPARGOS) a través del puerto **USB Host CDC-ACM**.

La implementación replica de manera autónoma en el Cyberdeck la experiencia de la aplicación web oficial (**[espargos.net/espsdr/app/](https://espargos.net/espsdr/app/)**), trasladando la captura, el motor de FFT vectorial y la visualización gráfica (espectro y cascada/waterfall) directamente a C++ y **LVGL v9.5**.

```
┌─────────────────────────────────────────┐               ┌────────────────────────────────────────────────────────┐
│     ESP32-S3 Super Mini (Mochila)       │               │                ESP32-P4 (CBDos Master)                 │
│                                         │               │                                                        │
│  • Firmware: ESP-SDR Oficial            │  USB CDC-ACM  │  • USB Host: P4UsbHostBackend / IUsbCdcChannel         │
│  • RF Front-end: 2.4 GHz Wi-Fi Bypass   │  (VID 0x303A) │  • Protocolo: EspSdrClient (SYNC, FREQ, GAIN, ráfagas) │
│  • Captura: Ráfagas I/Q @ 80/40 MSa/s   │ ------------->│  • DSP: esp-dsp (RISC-V Core 1 @ 400 MHz, FFT 512/1024)│
│  • Puerto: USB-Serial-JTAG nativo       │  (PID 0x1001) │  • UI: SdrView (LVGL 9.5: Espectro + Waterfall Canvas) │
└─────────────────────────────────────────┘               └────────────────────────────────────────────────────────┘
```

---

## 2. Topología de Hardware y Transporte USB

1. **Hardware Host (P4):** Placa Guition JC4880P443C con puerto USB OTG 2.0 High-Speed (480 Mbps).
2. **Hardware Dispositivo (S3):** ESP32-S3 Super Mini con puerto USB-C nativo (conectado directamente o mediante HUB USB).
3. **Identificación USB:**
   * **VID:** `0x303A` (Espressif Systems).
   * **PID:** `0x1001` (USB-Serial-JTAG CDC).
   * **Clase:** `DeviceClass::CdcAcm` / `VendorSpecific`.
4. **Transporte de Datos:** Emulación de línea serie sobre USB CDC a través de la abstracción existente `cbdos::usb::IUsbCdcChannel`.

---

## 3. Especificación del Protocolo ESP-SDR (Mapeado de `radio.js`)

El firmware ESP-SDR en el S3 se controla mediante comandos de texto terminados en `\n` y responde con ráfagas estructuradas:

1. **Handshake y Sincronización:**
   * Comando enviado por P4: `\nSYNC <timestamp>\n`
   * Respuesta esperada: `SYNC <timestamp>\n`
   * *Propósito:* Drenar logs de arranque del ESP32-S3 y confirmar que el firmware está listo.
2. **Identificación de Capacidades:**
   * Comando: `SPECINFO?\n`
   * Lectura de versión, rango de frecuencias (2400-2484 MHz), tasas de muestreo y límites de ganancia.
3. **Configuración de Radio:**
   * Sintonía de Frecuencia: `FREQ <f_MHz>\n` (ej. `FREQ 2412`).
   * Ganancia: Modo automático `GAIN AUTO\n` o manual por índice `GAIN <0-60>\n`.
   * Tasa de muestreo / Ancho de banda: `RATE <sample_rate>\n` (típicamente 20 MSa/s o 40 MSa/s).
4. **Captura y Flujo I/Q:**
   * Recepción de ráfagas binarias: Cabecera con metadatos + muestras I/Q entrelazadas ($I_0, Q_0, I_1, Q_1, \dots$) + CRC32 de 32 bits para validación de integridad.

---

## 4. Fases de Desarrollo y Plan de Pruebas

### Fase 1: Enlace USB CDC y Handshake en Consola CLI [✅ COMPLETADA]
* **Objetivo:** Establecer comunicación bidireccional estable entre el P4 y el S3 Super Mini sin involucrar la UI gráfica todavía.
* **Estado:** ✅ Validado en hardware real (Guition JC4880P443C + ESP32-S3 Super Mini con ESP-SDR oficial).
* **Telemetría Verificada en Caliente:**
  - VID/PID: `0x303A:0x1001` (ESP32 USB-Serial-JTAG).
  - Handshake: `SYNC <timestamp>` -> OK.
  - Perfiles reportados por `SPECINFO`: Soporte continuo a 16 MSa/s, 40 MSa/s y 80 MSa/s con FFT de 256 a 2048 bins.
  - Arbitraje CDC: Adquisición y liberación de canal limpias y sin cuelgues.

### Fase 2: Motor de Protocolo y Adquisición de Ráfagas (`EspSdrClient`) [✅ COMPLETADA]
* **Objetivo:** Adquirir ráfagas de muestras I/Q y tramas de espectro pre-calculadas (`SPC1`) verificando la integridad de datos por hardware/software.
* **Estado:** ✅ Validado en hardware real en caliente vía comando `sdr capture` / `sdr test`.
* **Telemetría Verificada en Caliente:**
  - Adquisición: 5/5 ráfagas consecutivas exitosas (1024 muestras I/Q c/u @ 2412 MHz / 40 MSa/s).
  - Tiempos de transferencia: 4 a 8 ms por ráfaga.
  - Errores de CRC32: 0 errores (0%).
  - Pérdida de tramas: 0 pérdidas.
  - Throughput promedio entregado: 170.7 kS/s.
  - Niveles de potencia RMS medidos dinámicamente: -24.9 dBFS a -16.7 dBFS (actividad RF Wi-Fi real detectada).
  - Multi-Target: Compilación limpia tanto en ESP32-P4 (`idf.py build`) como en ESP32-S3 (`pio run`).

### Fase 3: Pipeline DSP en Core 1 del ESP32-P4 (`SdrDspEngine`) [✅ COMPLETADA]
* **Objetivo:** Transformar las ráfagas temporales I/Q en un vector de potencia espectral en dBFS, calculando la traza en vivo, traza suavizada (EMA) y traza de retención de picos (*Max-Hold*).
* **Estado:** ✅ Validado y compilado limpio en multi-target. Benchmarks CLI y stream loop en Core 1 implementados.

### Fase 4: Vista Gráfica en LVGL v9.5 (`SdrView`) [✅ COMPLETADA]
* **Objetivo:** Replicar visualmente la interfaz de [espargos.net/espsdr/app/](https://espargos.net/espsdr/app/) con la estética premium de CBDos.
* **Estado:** ✅ Implementado `SdrView` derivado de `BaseView` e integrado en `AppRegistry`.
* **Componentes UI Implementados:**
  1. Integración con `HeaderBar` ("SDR Analyzer") y navegación nativa de vuelta al Dashboard.
  2. Panel de control superior: Botón Play/Pause, selector de canales Wi-Fi 1-14, botón de ganancia (AGC/Manual), selector de FFT (256, 512, 1024) y botón de reset para *Max-Hold*.
  3. Panel de espectro superior con `lv_chart`: doble serie (traza en vivo en verde `#37C964` y traza *Max-Hold* en amarillo `#E6B969`), escala dBFS (-120 a 0) y etiquetas de frecuencia portadora y leyenda.
  4. Panel de cascada (waterfall) inferior con `lv_canvas`: buffer RGB565 de 440x180 px con LUT térmica ESPARGOS de 256 colores y desplazamiento línea a línea (`memmove`).
  5. Barra de telemetría inferior: estado de conexión USB, pico dBFS, frecuencia del pico, microsegundos DSP y FPS en vivo.
  6. Desacoplamiento seguro: Core 1 adquiere y procesa ráfagas en segundo plano; el timer LVGL @ 30 FPS refresca la pantalla sin contención.

### Fase 5: Validación de Campo y Criterios Go / No-Go [✅ EN CURSO / DESPLEGADO]
* **Criterios Go / No-Go:**
  * **Protocolo de Alto Rendimiento:** Streaming continuo On-Chip Spectrum (`SPEC`) con paquetes `SPC1` a 200+ espectros/segundo. Búfer CDC de 16 KB en Core 0. Perfiles soportados para 80M, 40M, 20M, 10M, 8M y 4 MSa/s con 2048, 1024, 512 y 256 bins FFT.
  * **Frecuencia Continua y Pan/Drag Táctil:** Cobertura de sintonización continua de 100 MHz a 2700 MHz (2.7 GHz). Permite arrastrar (pan & drag) con el dedo horizontalmente sobre el espectro o la cascada para cambiar la frecuencia central en vivo, además de botones de paso fino (`-10M`, `-1M`, `+1M`, `+10M`) y presets rápidos.
  * **Ejes Graduados Oficiales (ESP-WebSDR):**
    * **Eje Y (dBFS):** Orilla izquierda con escala numérica `-20`, `-40`, `-60`, `-80`, `-100 dBFS`.
    * **Eje X (MHz):** 5 marcas numéricas calculadas en vivo (`Start`, `F1`, `Center ▲`, `F3`, `Stop`) según la frecuencia sintonizada y el span.
    * **Barra de Medidas (Meas Bar):** `Start · Center · Span · Stop · Bins · Pico dBFS @ MHz`.
  * **Hot-Plug:** Desconectar y volver a conectar la mochila por USB sin provocar kernel panic ni fugas de memoria en CBDos.
  * **Multi-Target:** Verificado y compilado limpio para ambas plataformas:
    * ESP32-P4 (`idf.py build`): ✅ Exitoso (código 0). Flasheado y arrancado en `/dev/ttyACM0`.
    * ESP32-S3 (`pio run`): ✅ Exitoso (código 0).

---

## 5. Estado Actual y Pruebas en Vivo

1. **Firmware P4:** Flasheado en hardware real en `/dev/ttyACM0`. SO operando a 60 FPS con pantalla ST7701S.
2. **App "SDR Spectrum":** Interfaz completa replicada de ESP-WebSDR operativa con arrastre táctil y rango hasta 2.7 GHz.
3. **Dongle ESP-SDR:** Listo en el puerto USB Host del P4 para exploración de espectro en tiempo real.


