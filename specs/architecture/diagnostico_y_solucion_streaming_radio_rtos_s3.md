# Diagnóstico y Corrección de Bloqueo en Radio Online (ESP32-S3)

## 1. Contexto del Problema
Al reproducir emisoras en Radio Online sobre la plataforma ESP32-S3 (JC3248W535):
- En ocasiones el nombre de la estación no se actualizaba en la interfaz.
- La radio comenzaba a reproducir el audio de forma continua.
- El panel táctil y la interfaz de usuario completa (LVGL 9.5) dejaban de responder de inmediato, forzando un reinicio por hardware para cambiar de estación.

## 2. Diagnóstico Técnico
1. **Descarte de Causa por Almacenamiento:**
   - La radio online opera vía streaming TCP directo a RAM/PSRAM e I2S DMA. No depende ni lee de la tarjeta MicroSD para streaming; los favoritos se almacenan en la Flash interna (SPIFFS / LittleFS).

2. **Causa Raíz Arquitectónica:**
   - El archivo `bsp/esp32_s3_jc3248/hal/hal_system_s3.cpp` omitía la implementación del espacio de nombres `cbdos::rtos` (`createTask`, `deleteTask`, `sleepMs`, `createMutex`, etc.).
   - Al no existir implementación en el BSP de S3, el linker recurría a la función stub por defecto con atributo débil (`__attribute__((weak))`) en `core/src/cbdos_core.cpp`.
   - Dicho stub débil ejecutaba la función pasada como parámetro de manera **sincrónica en el hilo llamante**:
     ```cpp
     __attribute__((weak)) TaskHandle createTask(TaskFunction fn, ...) {
         if (fn) fn(param); // Ejecución sincrónica en el mismo hilo
         return nullptr;
     }
     ```
   - Al pulsar "Play" en la vista de Radio (`RadioView`), el callback de evento de LVGL corre en el **Core 1** dentro de `loop()`.
   - `AudioPlayer::playStream(...)` invocaba `createTask(streamPlaybackTask, "helix_stream_task", 12288, this, 6, 0)`.
   - Como resultado, `streamPlaybackTask` y su bucle `runStreamPlayback()` secuestraban el **Core 1** y la tarea principal de Arduino/LVGL de forma infinita, impidiendo que `lv_timer_handler()` y el driver del táctil volvieran a procesar eventos.

## 3. Acciones de Corrección Implementadas
1. **Implementación de `cbdos::rtos` en `bsp/esp32_s3_jc3248/hal/hal_system_s3.cpp`:**
   - Implementado `createTask` con `xTaskCreatePinnedToCore` / `xTaskCreate` nativos de FreeRTOS.
   - Implementado `deleteTask` con `vTaskDelete`.
   - Implementado `sleepMs` con `vTaskDelay(pdMS_TO_TICKS(ms))`.
   - Implementados los mutexes con semáforos de FreeRTOS (`xSemaphoreCreateMutex`, `xSemaphoreTake`, `xSemaphoreGive`, `vSemaphoreDelete`).
   - Ahora `helix_stream_task` se ejecuta en una tarea independiente asignada a **Core 0**, dejando el **Core 1** libre para la UI y el táctil a 60 FPS.

2. **Optimización de Respuesta y Detención en `core/src/audio/AudioPlayer.cpp`:**
   - Reducido el timeout del socket `client->recv` a 1000 ms con reintento si sigue conectado y no se solicitó parar (`m_stopRequested`), permitiendo que las peticiones de parada o cambio de estación respondan de inmediato.
   - Ajustado el ciclo de espera en `AudioPlayer::stop()` a 1500 ms (150 iteraciones de 10 ms).

## 4. Validación Multi-Target
- **ESP32-S3:** Compilado limpio con PlatformIO (`pio run -d bsp/esp32_s3_jc3248`) -> **SUCCESS**.
- **ESP32-P4:** Compilado limpio con ESP-IDF (`idf.py -C bsp/esp32_p4_jc4880 build`) -> **SUCCESS**.
