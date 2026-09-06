# Plan de Implementación: Subsistema de Grabación de Audio y App Grabadora (Voice Memo)
**Ecosistema:** CBDos (ESP32-P4 / ES8311 / LVGL 9.5 / MicroSD)  
**Documento:** Plan de Arquitectura y Desarrollo Limpio (Desacoplamiento Estricto)  
**Ruta:** `docs/architecture/audio_recording_and_recorder_app_plan.md`  
**Estado:** Propuesta de Plan Técnico

---

## 1. Principio de Pureza Arquitectónica (Zero Layer Crossing)

Para respetar rigurosamente la **Ley de Pureza Arquitectónica de CBDos**:
1. **`core/include/cbdos/audio.hpp` (Agnóstico):** Extender la API abstracta de audio para soportar captura/grabación PCM (ej. `startRecording`, `stopRecording`, `readAudio`, `isRecording`, `getMicLevel`).
2. **`core/src/audio/WavRecorder.cpp` (Agnóstico):** Manejador que toma bloques PCM puros, genera el encabezado RIFF/WAVE estándar y escribe en el backend de almacenamiento (`cbdos::storage::writeFile` o streams de archivos) sin saber nada de ESP-IDF ni hardware.
3. **`bsp/esp32_p4_jc4880/hal/AudioHAL.cpp` (Específico de Plataforma):**
   * Configurar el canal I2S0 en modo dúplex o canal RX en **GPIO 48 (DIN)** con buffers DMA.
   * Inicializar el códec **ES8311 en modo ADC (`ESP_CODEC_DEV_WORK_MODE_BOTH` o `ESP_CODEC_DEV_TYPE_IN`)** y configurar la ganancia analógica del micrófono (PGA Gain / Mic Bias).
   * Implementar la función de lectura I2S RX (`Board_Audio_Read()`).
4. **`bsp/esp32_s3_jc3248/` (Agnóstico / Safe Stub):** Si la placa S3 no tiene micrófono físico, retorna `false` de forma segura sin romper la compilación multi-target.
5. **`apps/Recorder/` o `core/src/ui/views/AudioRecorderView.cpp` (LVGL 9.5 Puro):** Vista visual con medidor de nivel / vúmetro en tiempo real, cronómetro y lista de notas de voz.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CAPA DE APLICACIÓN / UI (LVGL 9.5)                   │
│          [AudioRecorderView] ──► Visualizador VU / Onda + Botones       │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    CAPA NÚCLEO (core/ - C++ Agnóstico)                  │
│                                                                         │
│   ┌───────────────────────────┐       ┌─────────────────────────────┐   │
│   │   cbdos::audio::record    │ ◄───► │      WavRecorder (.wav)     │   │
│   │ (Buffer ring en PSRAM)    │       │   (RIFF Header + Streaming) │   │
│   └─────────────┬─────────────┘       └──────────────┬──────────────┘   │
└─────────────────┼────────────────────────────────────┼──────────────────┘
                  │                                    │
                  ▼                                    ▼
┌──────────────────────────────────────┐  ┌───────────────────────────────┐
│  CAPA DE HARDWARE (bsp/esp32_p4)     │  │  cbdos::storage (MicroSD)     │
│  - ES8311 ADC Mode & PGA Gain        │  │  - /sdcard/recordings/*.wav   │
│  - I2S0 RX DMA (GPIO 48 DIN)         │  └───────────────────────────────┘
└──────────────────────────────────────┘
```

---

## 2. Fases del Plan de Implementación

### Fase 1: Extensión del Contrato HAL y Core (`core/include/cbdos/audio.hpp`)
- Declarar las nuevas interfaces públicas de captura:
  ```cpp
  namespace cbdos {
  namespace audio {

  struct RecordConfig {
      uint32_t sampleRate = 16000; // 16 kHz (ideal para voz) o 44100 Hz
      uint8_t channels = 1;        // Mono (1) o Estéreo (2)
      uint8_t bitsPerSample = 16;  // 16-bit PCM
      uint8_t micGainDb = 24;      // Ganancia de entrada del micrófono (0 a 30 dB)
  };

  bool recordStart(const char* targetFilePath, const RecordConfig& cfg = RecordConfig());
  void recordPause();
  void recordResume();
  void recordStop();
  bool isRecording();
  uint32_t getRecordDurationMs();
  float getMicPeakLevel(); // Retorna 0.0 a 1.0 para animar vúmetros / osciloscopios

  // Funciones de lectura cruda para streaming / Walkie-Talkie
  size_t readAudio(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100);

  } // namespace audio
  } // namespace cbdos
  ```

---

### Fase 2: Implementación de Bajo Nivel en BSP (`bsp/esp32_p4_jc4880`)
1. **Configuración del códec ES8311:**
   - Habilitar el módulo ADC interno del ES8311.
   - Configurar la ganancia del micrófono (Mic PGA Gain) para asegurar una captura de voz clara sin distorsión.
2. **Configuración del bus I2S0 RX:**
   - Crear canal `I2S_CHANNEL_DEFAULT_CONFIG` para RX sobre **GPIO 48 (DIN)**.
   - Crear el descriptor `esp_codec_dev_new` de tipo `ESP_CODEC_DEV_TYPE_IN` o Dúplex.
3. **Mapeo en `AudioHAL.cpp`:**
   - Implementar `Board_Audio_Record_Start()`, `Board_Audio_Record_Stop()` y `Board_Audio_Read()`.

---

### Fase 3: Motor de Archivos WAV en Core (`WavRecorder.cpp`)
- Implementar la escritura eficiente en streaming hacia la MicroSD:
  1. Escribir encabezado RIFF WAVE estándar de 44 bytes con tamaño provisional (`0`).
  2. Búfer en PSRAM de 16 KB para minimizar la cantidad de escrituras en la MicroSD.
  3. Al detener la grabación (`recordStop`), actualizar los campos `ChunkSize` y `Subchunk2Size` con el tamaño final exacto y cerrar el archivo.

---

### Fase 4: Aplicación y Vista Gráfica en LVGL 9.5 (`AudioRecorderView`)
- **Diseño Visual:**
  - **Cabecera:** Título "Grabadora de Voz", estado (Listo / Grabando / Pausado).
  - **Visualizador Reactivo:** 16 barras verticales o arco animado en LVGL que reacciona a `cbdos::audio::getMicPeakLevel()`.
  - **Cronómetro:** Formato digital `00:02:45`.
  - **Controles Táctiles:** Botón circular grande de Grabación (Rojo / Pausa / Stop) y botón de Guardar.
  - **Historial de Grabaciones:** Lista desplazable con los archivos guardados en `/sdcard/recordings/`, con botón de reproducción directa por el altavoz y borrado.

---

### Fase 5: Exposición a Lua Engine (`cbdos.audio.*`)
- Extender `LuaAudioBridge` para que scripts Lua puedan crear grabadoras automáticas o detectores de sonido:
  ```lua
  audio.record_start("/sdcard/recordings/nota1.wav", 16000)
  sys.sleep(5000)
  audio.record_stop()
  print("Grabación finalizada")
  ```

---

## 3. Plan de Verificación y Pruebas

1. **Prueba de Captura Cruda:** Grabar 5 segundos de audio a 16 kHz 16-bit PCM en `/sdcard/test.wav`.
2. **Prueba de Reproducción Inmediata:** Reproducir el archivo recién grabado usando el reproductor existente `cbdos::audio::playFile("/sdcard/test.wav")` por el altavoz de la JC4880.
3. **Prueba de Integridad de Archivo:** Extraer la MicroSD o verificar con reproductor estándar en PC que el archivo `.wav` se reproduce con volumen claro y sin ruidos.
4. **Verificación Multi-Target:** Compilar limpiamente en P4 (`idf.py build`) y en S3 (`pio run`).
