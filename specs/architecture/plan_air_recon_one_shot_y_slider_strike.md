# Plan Técnico: Escaneo Air Recon Bajo Demanda (One-Shot) con Slider en Wi-Fi Strike

**Fecha:** 2026-10-19  
**Autor:** Kaber420 & Antigravity (Senior Embedded Systems Developer)  
**Revisión y Auditoría:** Aprobado con enmiendas por `glm-5.3-flash-free` vía OpenCode  
**Categoría:** Arquitectura y UI Embebida (`specs/architecture/`)  
**Objetivo:** Eliminar el conflicto de concurrencia en la radio física 2.4 GHz del ESP32-S3 entre el escáner continuo de fondo (`WifiShieldService`) y la inyección cruda de tramas (`WifiStrikeService`). Proporcionar un control bajo demanda en la interfaz de usuario de Wi-Fi Strike compuesto por un slider para configurar la duración del escaneo (1 a 10 segundos) y un botón de disparo directo, manteniendo la modularidad, exclusión mutua atómica y thread-safety estricto en LVGL v9.5.

---

## 1. Diagnóstico y Causa Raíz

En el ESP32-S3 existe un único transceptor Wi-Fi físico (Single-PHY). Hasta ahora:
1. `WifiShieldService` mantenía un hilo FreeRTOS periódico ejecutando `WiFi.scanNetworks(false, true, false, 120)` cada 3 segundos.
2. Al iniciar la emisión en `WifiStrikeService`, el servicio llamaba a `WifiShieldService::stopScan()`, pero esta llamada solo modificaba un flag booleano sin esperar a que el barrido hardware de canales finalizara.
3. Durante ese intervalo (~1.5 a 2.5 s), `esp_wifi_80211_tx()` rechazaba cualquier trama con `ESP_ERR_WIFI_STATE`, lo que provocaba 8 fallos consecutivos casi instantáneos y el error en pantalla:
   > *«La radio rechazo la emision de forma consecutiva»*
4. Al abortar la sesión, el código intentaba reactivar automáticamente el escaneo de fondo, perpetuando el ciclo de interferencia.

---

## 2. Enmiendas Técnicas Incorporadas (Auditoría GLM-5.3)

1. **Exclusión Mutua Atómica (Hardware Claim):**  
   No delegar la exclusión mutua únicamente a botones deshabilitados en la UI. Se implementa verificación atómica a nivel servicio: si la radio está en emisión (`WifiStrike`), `startTimedScan()` es rechazado inmediatamente; y si el escaneo está físicamente en curso en el HAL, `startSession()` de Strike se rechaza con error de radio ocupada (`RadioResult::Busy`).
2. **Thread-Safety Estricto en LVGL v9.5 (Cero llamadas directas desde FreeRTOS):**  
   La tarea `wifi_scan_task` corre en Core 0. **Queda terminantemente prohibido** llamar funciones de LVGL (`lv_dropdown_*`, `lv_label_*`, etc.) desde el callback de FreeRTOS. En su lugar, el worker actualiza variables atómicas en `WifiShieldService` (`m_scanning.store(false)` y timestamp de actualización), y la vista `WifiStrikeView` detecta la transición en su `lv_timer` periódico (`refreshDeauthStatus`), ejecutando la actualización gráfica de forma 100% segura en el hilo de LVGL.
3. **Mapeo de Duración del Slider:**  
   En `hal_wifi_s3.cpp`, el tiempo fijado por el slider ($T$ segundos) se gestiona mediante un bucle de pasadas: el worker ejecuta pasadas de escaneo hasta que `(millis() - startMs) >= (durationSeconds * 1000)`. Al expirar el tiempo, llama a `WiFi.scanDelete()`, libera el hardware y pasa a `Idle`.
4. **Aislamiento Multi-Target (S3 vs P4):**  
   En ESP32-P4, el subsistema de inyección cruda 802.11 (`WifiStrikeRadioP4`) devuelve `RadioResult::Unsupported`. El escaneo bajo demanda compila limpio en ambas plataformas, pero la UI en P4 mantendrá deshabilitada la emisión cruda para evitar rechazos en un target sin transceiver de inyección.

---

## 3. Especificación de Cambios por Componente

### 3.1. Core Service: `WifiShieldService` (`core/src/security/`)
- Soporte para duración en segundos: `bool startTimedScan(uint32_t durationSeconds = 3);`.
- `std::atomic<bool> m_scanning` y `std::atomic<uint32_t> m_lastScanFinishMs` para consulta segura entre hilos.
- Integración con el gestor de reclamo de radio: rechazar escaneo si Strike tiene la radio tomada (`StrikeRadioState::Emitting`).

### 3.2. HAL S3: `hal_wifi_s3.cpp` (`bsp/esp32_s3_jc3248/hal/`)
- Migrar `s_scanRunning` a `std::atomic<bool>`.
- Guardar `s_scanDurationMs`. En `wifiScanWorker`:
  ```cpp
  uint32_t startMs = millis();
  while (s_scanRunning && (millis() - startMs < s_scanDurationMs)) {
      int n = WiFi.scanNetworks(false, true, false, 100);
      // Procesar APs...
      WiFi.scanDelete();
      if (!s_scanRunning || (millis() - startMs >= s_scanDurationMs)) break;
      vTaskDelay(pdMS_TO_TICKS(200));
  }
  s_scanRunning = false;
  security::WifiShieldService::getInstance().notifyScanStopped();
  ```
- Al expirar, la tarea finaliza y deja el driver libre.

### 3.3. Core Service: `WifiStrikeService` (`core/src/security/`)
- Eliminar `m_restoreScan` y la reactivación automática al salir o fallar.
- En `startSession()`: comprobar `WifiShieldService::getInstance().isScanning()`. Si está escaneando, retornar `false` con mensaje: `"Radio ocupada: escaneo en curso"`.

### 3.4. UI: `WifiStrikeView` (`core/src/ui/views/`)
- En la sección **OBJETIVO (AIR RECON)** de la pestaña Deauth:
  - Tarjeta o fila compacta con:
    - Etiqueta `DURACIÓN ESCANEO: 3 s`.
    - `lv_slider` configurado de 1 a 10 (valor por omisión: 3).
    - Evento del slider que actualiza el texto de segundos en vivo.
    - Botón `m_btnScanNow` con texto `🔍 Escanear Redes`.
  - Al pulsar el botón:
    - Inicia `startTimedScan(segundos)`.
    - Cambia estado visual del botón a ámbar `⏳ Escaneando...`.
  - En `refreshDeauthStatus()` (temporizador de LVGL):
    - Si el escaneo pasó de activo a inactivo:
      - Restaura el botón a `🔍 Escanear Redes`.
      - Llama a `syncDeauthTargets()` para actualizar el dropdown con las APs detectadas ordenadas por RSSI.
      - Muestra Toast: `"Escaneo completado: N APs detectadas"`.
    - Si `WifiStrikeService` está emitiendo: deshabilitar el botón de escaneo y el slider (`lv_obj_add_state(..., LV_STATE_DISABLED)`).

---

## 4. Plan de Verificación

1. **Compilación Limpia:**
   - PlatformIO ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
   - ESP-IDF ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
2. **Prueba Funcional en Hardware:**
   - Flasheo en ESP32-S3 con manos libres (`pio run -d bsp/esp32_s3_jc3248 -t upload`).
   - Mover el slider a 4 segundos y pulsar `Escanear Redes`.
   - Verificar que escanea durante 4 s exactos, finaliza, puebla el menú desplegable y deja la radio libre.
   - Seleccionar un objetivo y pulsar `Iniciar Emisión`.
   - Confirmar que la emisión corre fluidamente sin ningún rechazo de radio consecutivo.
