# Plan Técnico: Estabilización y Caché en PSRAM para Radio Web (Anti-Jitter RingBuffer)

**Documento:** `specs/architecture/plan_reparacion_y_estabilizacion_cache_radio_web_psram.md`  
**Módulo:** `core/src/audio/AudioPlayer.cpp` y `RadioManager`  
**Estado:** Propuesta Técnica de Reparación para Aprobación  
**Fecha:** 14 de Octubre de 2026  
**Autor:** Antigravity Senior Embedded Systems Developer  
**Hardware Target:** ESP32-P4 (32 MB PSRAM) y ESP32-S3 (8 MB / 16 MB PSRAM)  

---

## 1. Diagnóstico Técnico: ¿Por qué se corta la Radio Web?

La causa de los constantes cortes y chasquidos en la reproducción de estaciones de radio web no es un fallo del chip Wi-Fi ni del códec Helix MP3, sino un **cuello de botella de arquitectura en el manejo de memoria y flujo de datos** en [`core/src/audio/AudioPlayer.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.cpp).

### Defectos Detectados en el Código Actual:

1. **Buffer Minúsculo y Ridículo (32 KB):**
   En [`AudioPlayer.cpp:407`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.cpp#L407):
   ```cpp
   const size_t IN_BUF_SIZE = 32768; // 32 KB de buffer en memoria
   uint8_t* inBuf = (uint8_t*)cbdos::mem::alloc_psram(IN_BUF_SIZE);
   ```
   * Una estación MP3 estándar a **128 kbps** transmite a razón de **16 KB/segundo**.
   * Una estación a **192 kbps** transmite a razón de **24 KB/segundo**.
   * Con 32 KB, el sistema solo es capaz de almacenar **2.0 segundos de audio** (a 128 kbps) o **1.3 segundos** (a 192 kbps).
   * Cualquier reintento de trama TCP de Wi-Fi, negociación de baliza (beacon DTIM) o micro-retardo en el router de 1.5 segundos vacía el buffer por completo (`buffer underrun`), cortando la música.

2. **Acoplamiento Bloqueante entre Red e I2S (Lockstep Stalling):**
   En [`AudioPlayer.cpp:440`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.cpp#L440):
   ```cpp
   int nRead = client->recv(inBuf + bytesLeft, toRead, 1000);
   ```
   La misma tarea de FreeRTOS (`helix_stream_task`) se encarga tanto de recibir datos del socket como de decodificar y empujar muestras a la DMA del I2S. Cuando los datos bajan de 16 KB, la tarea se detiene a esperar por la red con un timeout de **hasta 1000 ms**.
   * Durante ese segundo de espera, **la decodificación Helix se detiene totalmente**.
   * Como la DMA del I2S solo tiene buffer para ~80 ms, se produce un silencio inmediato.

3. **Inexistencia de Pre-Buffering al Arrancar:**
   Apenas el socket recibe el primer bloque de unos cientos de bytes, el bucle comienza a decodificar y enviar audio al I2S. Comienza a sonar con el buffer prácticamente vacío (al 2% o 5%), haciéndolo hipersensible a cualquier fluctuación desde el primer segundo.

4. **Ineficiencia de `memmove` en Buffer Lineal:**
   El código actual usa un buffer plano lineal y ejecuta `memmove(inBuf, readPtr, bytesLeft)` en cada vuelta para compactar los datos no consumidos. Si simplemente aumentáramos el tamaño de este buffer plano a 1 MB, ejecutar `memmove` de cientos de kilobytes continuamente saturaría el bus SPI/Octal de la PSRAM.

---

## 2. Nueva Arquitectura: RingBuffer Circular en PSRAM (Estándar de Industria)

En sistemas embebidos y reproductores de streaming (VLC, TuneIn, ESP32-audioI2S), un buffer de 1 minuto introduce un retraso excesivo con respecto a la emisión real. 

El estándar de oro equilibrado para microcontroladores es un buffer de **entre 8 y 12 segundos** (128 KB a 192 KB en PSRAM), con un **pre-buffer de arranque de solo 2 segundos** (32 KB):
* **Arranque instantáneo (Zapping ágil):** Al tocar una emisora, solo se precargan 32 KB (que por Wi-Fi se descargan en **200 a 300 ms**). La radio comienza a sonar de inmediato.
* **Margen de seguridad:** Una vez arrancada, la tarea sigue llenando el buffer en segundo plano hasta los **192 KB** (~12 segundos de audio), absorbiendo cualquier renegociación de Wi-Fi o congestión en el router.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        FLUJO DE DATOS ASÍNCRONO                        │
│                                                                        │
│   [ Socket HTTP Streaming ]                                            │
│              │ (Descarga ráfagas de red en Core 0)                     │
│              ▼                                                         │
│   ┌──────────────────────────────────────────────────────────────┐     │
│   │           RINGBUFFER CIRCULAR EN PSRAM (192 KB)              │     │
│   │   [Pre-buffer de arranque: 32 KB ≈ 2 s (descarga en 250ms)]   │     │
│   │                                                              │     │
│   │  Head (Escritura Red) --------------> Tail (Lectura Decoder) │     │
│   │  Capacidad total: 192 KB ≈ 12 segundos de colchón anti-lag   │     │
│   └──────────────────────────────┬───────────────────────────────┘     │
│                                  │ (Lectura fluida y continua)         │
│                                  ▼                                     │
│                     [ Decoder Helix MP3 ]                              │
│                                  │                                     │
│                                  ▼                                     │
│                     [ I2S DMA Buffer (Speaker) ]                       │
└────────────────────────────────────────────────────────────────────────┘
```

### Comparativa de Métricas:

| Parámetro | Implementación Defectuosa Actual | **Nueva Arquitectura Propuesta** |
| :--- | :---: | :---: |
| **Capacidad Total** | 32 KB lineales (con `memmove`) | **192 KB RingBuffer circular en PSRAM** |
| **Tiempo de Caché (128 kbps)** | 2.0 segundos | **12.0 segundos** |
| **Tiempo de Caché (192 kbps)** | 1.3 segundos | **8.0 segundos** |
| **Pre-buffering para arrancar** | 0 KB (arranque en frío) | **32 KB (apenas 200-300 ms de descarga)** |
| **Zapping de emisoras** | Se corta al instante | **Instantáneo (< 500 ms) y estable** |
| **Uso de PSRAM** | 32 KB | **192 KB** (< 0.6% de los 32 MB de PSRAM) |

---

## 3. Plan de Implementación Paso a Paso

### Paso 1: Estructura RingBuffer en PSRAM (Zero `memmove`)
Implementar una clase eficiente en C++ que gestione el buffer circular con punteros `m_head` y `m_tail`:
* Memoria reservada con `cbdos::mem::alloc_psram(192 * 1024)`.
* Métodos: `write(data, len)`, `read(dest, len)`, `peek(dest, len)`, `skip(len)`, `available()`, `freeSpace()`.
* Cero copias masivas de memoria: las lecturas y escrituras se envuelven modularmente (`index % capacity`).

### Paso 2: Fase de Pre-Buffering Ultra Ágil
Al seleccionar una estación:
1. Conectar con el servidor HTTP y recibir cabeceras `ICY 200 OK`.
2. **Pre-carga rápida (Estado `Buffering`):**
   * Descargar hasta acumular **32 KB** en el RingBuffer.
   * Con una conexión Wi-Fi estándar, esta descarga toma entre **200 ms y 400 ms**.
3. Iniciar la decodificación Helix y la salida I2S de inmediato.
4. El hilo de red continúa rellenando el buffer hasta los **192 KB** mientras el usuario ya está escuchando la música.

### Paso 3: Socket No Bloqueante en Core 0
En lugar de congelar la tarea durante 1000 ms con `recv`:
* Usar lecturas con timeout ultra corto (**20 ms** a **30 ms**) o no bloqueantes.
* Si el socket no entrega datos en ese lapso, el bucle **continúa decodificando de inmediato** a partir de los datos ya disponibles en el RingBuffer de PSRAM.
* El I2S nunca sufre hambre de datos (`underrun`) ante ráfagas de congestión Wi-Fi de hasta 8-10 segundos.

### Paso 4: Auto-Recuperación Inteligente (Re-buffering)
Si por pérdida total de señal Wi-Fi prolongada el RingBuffer se agota por completo:
* Pausar suavemente la alimentación I2S (sin chasquidos de DC offset).
* Re-acumular 32 KB antes de reanudar el audio limpiamente.

---

## 4. Archivos a Modificar

1. **[`core/src/audio/AudioPlayer.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.cpp):**
   * Reemplazar el buffer lineal de 32 KB y los `memmove` por `PsramRingBuffer` de 192 KB.
   * Implementar la máquina de estados de pre-buffering (32 KB de arranque).
   * Reducir el timeout de recepción de socket a 25 ms.
2. **[`core/src/audio/AudioPlayer.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.hpp):**
   * Exponer métricas de buffer en tiempo real: `getBufferPercent()`.
3. **[`core/src/ui/views/RadioView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RadioView.cpp):**
   * Opcional: Mostrar estado de conexión/buffering en la barra de reproducción.

---

## 5. Validación y Criterios de Aceptación

1. **Prueba de Zapping (Cambio de Estación):**
   * Al pulsar una nueva emisora, el audio debe comenzar a sonar en menos de **500 ms**.
2. **Prueba de Estabilidad Wi-Fi:**
   * La radio debe soportar fluctuaciones y caídas breves de red de hasta **8 a 10 segundos** sin un solo salto ni chasquido.
3. **Compilación Multi-Target:**
   * Build limpio garantizado en ESP32-P4 (ESP-IDF) y ESP32-S3 (PlatformIO).
