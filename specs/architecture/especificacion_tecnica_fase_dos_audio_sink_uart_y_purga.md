# 🏛️ Especificación Técnica: Fase 2 - Audio Sink, Grabación de Audio, UART y Purga de Símbolos Weak (CBDos v0.2.1)

**Documento:** `docs/architecture/especificacion_tecnica_fase_dos_audio_sink_uart_y_purga.md`  
**Versión:** 1.0.0  
**Estado:** Documento de Especificación Técnica para Evaluación y Aprobación  
**Autor:** Equipo de Arquitectura de Software CBDos  
**Fecha:** Agosto 2026  

---

## 📌 1. Estado Actual y Alcance de la Fase 2

Habiendo concluido y validado exitosamente la **Etapa 2.1 (Storage HAL `IStorageBackend`)** con compilación cruzada limpia al 100% en ESP-IDF 5.5 (ESP32-P4) y PlatformIO (ESP32-S3), este documento técnico define en detalle las especificaciones de diseño, contratos HAL y arquitectura de las etapas restantes de la **Fase 2**:

1. **Etapa 2.2: Subsistema de Salida de Audio (`IAudioSink`) y Captura de Micrófono (`IAudioSource`):**
   - Desacoplamiento total del códec Everest ES8311 (P4 vía I2C/I2S) y ES8388 (S3).
   - Conexión agnóstica de los motores de decodificación Helix MP3/AAC y `WavPlayer` de `core/`.
   - Conexión de captura PCM para `WavRecorder` (grabación de notas de voz).
2. **Etapa 2.3: Subsistema de Comunicación Serial (`IUartBackend`):**
   - Desacoplamiento de puertos UART hardware (P4 `driver/uart` y S3 `HardwareSerial`).
   - Gestión de pines dinámicos y presets para el puerto JP1 (Flasheo ESP32-C6 y terminal interactiva).
3. **Etapa 2.4: Purga Final de Símbolos Débiles (`weak`):**
   - Eliminación del 100% de `__attribute__((weak))` en [`core/src/cbdos_core.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/cbdos_core.cpp).

---

## 🏗️ 2. Arquitectura de Desacoplamiento de Audio y UART

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          APLICACIONES Y UI (CORE)                           │
│   MusicPlayerView  •  AudioRecorderView  •  RadioView  •  SerialTerminalView│
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (APIs de Alto Nivel de CBDos)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                    DESPACHADORES AGNÓSTICOS (CORE)                          │
│     cbdos::audio::* (AudioPlayer/WavPlayer)   │   cbdos::uart::* (uart.cpp) │
└──────────────────┬───────────────────────────┴───────────┬──────────────────┘
                   │                                       │
                   ▼ (Contratos HAL C++ Puros)             ▼
   ┌────────────────────────────────┐         ┌───────────────────────────────┐
   │    IAudioSink / IAudioSource   │         │          IUartBackend         │
   └───────────────┬────────────────┘         └───────────────┬───────────────┘
                   │                                          │
  ┌────────────────┴──────────────────────────────────────────┴───────────────┐
  │                    INYECCIÓN EN TIEMPO DE ARRANQUE (BSP)                  │
  ├──────────────────────────────────────────┬────────────────────────────────┤
  │          ESP32-P4 (JC4880)               │          ESP32-S3 (JC3248)     │
  │  • P4AudioSink (I2S DMA + ES8311 I2C)    │  • S3AudioSink (I2S Arduino)   │
  │  • P4AudioSource (ES8311 Mic ADC)        │  • S3AudioSource (No-op/I2S In)│
  │  • P4UartBackend (driver/uart nativo)    │  • S3UartBackend (HardwareSer) │
  └──────────────────────────────────────────┴────────────────────────────────┘
```

---

## 📐 3. Especificación Detallada de Contratos HAL

### 3.1. Salida y Captura de Audio (`core/include/cbdos/audio.hpp`)

El subsistema de audio en `core/` no debe interactuar directamente con drivers de códec ni primitivas I2S. Toda emisión y captura de muestras PCM ocurre a través de interfaces virtuales puras:

```cpp
namespace cbdos {
namespace audio {

enum class CodecType {
    None,
    MP3,
    AAC,
    WAV
};

struct AudioStats {
    bool isPlaying;
    CodecType codec;
    uint32_t sampleRate;
    uint8_t channels;
    uint32_t bitRate;
    uint8_t bufferPercent;
};

struct RecordConfig {
    uint32_t sampleRate = 16000; // 16 kHz estándar de voz
    uint8_t channels = 1;        // 1 = Mono, 2 = Estéreo
    uint8_t bitsPerSample = 16;  // 16-bit PCM
    uint8_t micGainDb = 24;      // Ganancia de entrada (0 a 30 dB)
};

// ────────────────────────────────────────────────────────────────
// Contrato de Salida de Audio (Reproducción I2S / DAC)
// ────────────────────────────────────────────────────────────────
class IAudioSink {
public:
    virtual ~IAudioSink() = default;

    virtual bool init(uint32_t sampleRate = 44100, uint8_t channels = 2, uint8_t bitsPerSample = 16) = 0;
    virtual void deinit() = 0;
    virtual size_t write(const void* pcmData, size_t sizeBytes, uint32_t timeoutMs = 100) = 0;
    virtual void setVolume(uint8_t volumePercent) = 0;
    virtual uint8_t getVolume() const = 0;
    virtual bool setSampleRate(uint32_t sampleRate) = 0;
    virtual void mute(bool enable) = 0;
    virtual bool isMuted() const = 0;
    virtual void playTone(uint32_t freqHz, uint32_t durationMs) = 0;
};

// ────────────────────────────────────────────────────────────────
// Contrato de Entrada de Audio (Micrófono / Captura I2S ADC)
// ────────────────────────────────────────────────────────────────
class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    virtual bool init(const RecordConfig& cfg) = 0;
    virtual void deinit() = 0;
    virtual size_t read(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100) = 0;
    virtual void setMicGain(uint8_t gainDb) = 0;
    virtual float getPeakLevel() = 0;
};

void setAudioSink(IAudioSink* sink);
IAudioSink* getAudioSink();

void setAudioSource(IAudioSource* source);
IAudioSource* getAudioSource();

// APIs públicas del SDK de CBDos (Consumidas por Vistas y Lua)
bool init();
bool playStream(const char* url);
bool playFile(const char* filepath);
void stop();
void pause();
void resume();
void setVolume(uint8_t volumePercent);
uint8_t getVolume();
bool setSampleRate(uint32_t sampleRate);
void playTone(uint32_t freqHz = 1000, uint32_t durationMs = 100);
void playBeep();
void seek(uint32_t seconds);
uint32_t getCurrentTimeSec();
uint32_t getTotalTimeSec();
bool writeAudio(const void* src, size_t size);
AudioStats getStats();

// Grabación
bool recordStart(const char* targetFilePath, const RecordConfig& cfg = RecordConfig());
void recordPause();
void recordResume();
void recordStop();
bool isRecording();
bool isRecordPaused();
uint32_t getRecordDurationMs();
float getMicPeakLevel();
size_t readAudio(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100);

} // namespace audio
} // namespace cbdos
```

---

### 3.2. Comunicación Serial Multi-Puerto (`core/include/cbdos/uart.hpp`)

Desacopla el acceso UART para permitir que `SerialTerminalView`, el flasheador del ESP32-C6 y scripts Lua se comuniquen con periféricos sin importar si la plataforma es ESP-IDF o Arduino:

```cpp
namespace cbdos {
namespace uart {

struct UartPinPreset {
    std::string name;
    int txPin;
    int rxPin;
};

class IUartBackend {
public:
    virtual ~IUartBackend() = default;

    virtual bool init(int txPin, int rxPin, uint32_t baudrate) = 0;
    virtual void deinit() = 0;
    virtual bool isInitialized() const = 0;
    virtual size_t available() = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual std::string readString(size_t maxLen = 1024) = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual size_t writeString(const std::string& str) = 0;
    virtual void flush() = 0;
    virtual bool setBaudrate(uint32_t baudrate) = 0;
    virtual int getDefaultTxPin() const = 0;
    virtual int getDefaultRxPin() const = 0;
    virtual uint32_t getDefaultBaudrate() const = 0;
    virtual const std::vector<UartPinPreset>& getPinPresets() const = 0;
};

void setBackend(IUartBackend* backend);
IUartBackend* getBackend();

// APIs públicas
bool init(int txPin, int rxPin, uint32_t baudrate);
void deinit();
bool isInitialized();
size_t available();
size_t read(uint8_t* buffer, size_t maxLen);
std::string readString(size_t maxLen = 1024);
size_t write(const uint8_t* data, size_t len);
size_t writeString(const std::string& str);
void flush();
bool setBaudrate(uint32_t baudrate);
int getDefaultTxPin();
int getDefaultRxPin();
uint32_t getDefaultBaudrate();
const std::vector<UartPinPreset>& getPinPresets();

} // namespace uart
} // namespace cbdos
```

---

## 📋 4. Plan de Ejecución por Pasos

### 🔹 Paso 1: Salida y Captura de Audio (`IAudioSink` e `IAudioSource`)
1. **Actualizar [`audio.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/audio.hpp):** Declarar las interfaces `IAudioSink` e `IAudioSource` con sus setters/getters.
2. **Conectar el Pipeline de Core:** Asegurar que `AudioPlayer.cpp`, `WavPlayer.cpp` y `WavRecorder.cpp` emitan y reciban muestras PCM exclusivamente a través del sink/source inyectado.
3. **Implementar en ESP32-P4 ([`hal_audio_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_audio_p4.cpp)):**
   - Clase `P4AudioSink`: I2S standard TX DMA hacia ES8311 (I2C SDA=7, SCL=8, MCLK=13, BCLK=12, WS=10, DOUT=9, PA=11).
   - Clase `P4AudioSource`: I2S RX DMA para micrófono ES8311.
   - Función de inyección `cbdos::bsp::initAudioBackendP4()`.
4. **Implementar en ESP32-S3 ([`hal_audio_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_audio_s3.cpp)):**
   - Clase `S3AudioSink`: Driver de audio I2S Arduino.
   - Función de inyección `cbdos::bsp::initAudioBackendS3()`.

---

### 🔹 Paso 2: Terminal Serial y UART (`IUartBackend`)
1. **Actualizar [`uart.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/uart.hpp):** Declarar la interfaz `IUartBackend`.
2. **Crear despachador agnóstico:** `core/src/system/uart.cpp` delegando en `IUartBackend*`.
3. **Implementar en ESP32-P4 ([`hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp)):**
   - Clase `P4UartBackend` sobre `driver/uart` de ESP-IDF (UART1 con pines configurables para JP1).
   - Función de inyección `cbdos::bsp::initUartBackendP4()`.
4. **Implementar en ESP32-S3 ([`hal_uart_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_uart_s3.cpp)):**
   - Clase `S3UartBackend` sobre `HardwareSerial` de Arduino.
   - Función de inyección `cbdos::bsp::initUartBackendS3()`.

---

### 🔹 Paso 3: Purga Completa de Símbolos Débiles y Verificación
1. **Limpieza en [`cbdos_core.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/cbdos_core.cpp):**
   - Eliminar los bloques `weak` de `audio` y de cualquier otro subsistema restante.
2. **Compilación Cruzada Simultánea:**
   - ESP32-P4: `idf.py build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`

---

## 🧪 5. Matriz de Validación de Calidad

| Prueba | Target | Procedimiento de Validación | Criterio de Éxito |
| :--- | :--- | :--- | :--- |
| **Reproducción de Música** | P4 & S3 | Reproducir archivo `.mp3` / `.wav` desde MicroSD con `MusicPlayerView`. | Salida limpia I2S sin chasquidos ni caídas de búfer. |
| **Generación de Tonos** | P4 & S3 | Ejecutar `cbdos::audio::playTone(1000, 200)` al pulsar botones en UI. | Pitido claro por altavoz. |
| **Grabación de Micrófono** | P4 | Grabar 5 segundos desde `AudioRecorderView` y verificar archivo WAV en MicroSD. | Audio inteligible y espectro visible en VU meter. |
| **Terminal Serial JP1** | P4 & S3 | Enviar comandos AT o eco serie desde `SerialTerminalView`. | TX/RX fluido bidireccional. |
| **Cero Símbolos Weak** | Core | Inspección estática con `grep -rn "__attribute__((weak))" core/src/cbdos_core.cpp`. | Solo quedan `mem` y `rtos` básicos o cero símbolos. |

---

## 🔬 6. Análisis de Rendimiento, Justificación Técnica y Beneficios

### 6.1. ¿Por qué desacoplar el Códec de los Motores de Audio (Helix / WavPlayer / WavRecorder)?

En un sistema operativo embebido como CBDos, acoplar directamente el decodificador matemático de audio con los registros o librerías de hardware de un microcontrolador específico genera deuda técnica y duplicación de código innecesaria. La separación de responsabilidades aporta los siguientes beneficios fundamentales:

1. **Cero Código Duplicado (Un Solo Reproductor Universal en `core/`):**
   - La lógica del reproductor MP3/AAC/WAV (control de reproducción, cálculo de tiempos ID3, salto de pistas, buffers circulares de streaming y empaquetado de archivos WAV) es idéntica independientemente del microcontrolador.
   - En lugar de mantener una versión de `AudioPlayer` para ESP-IDF (P4) y otra para Arduino (S3), existe una **única implementación pura en C++ en `core/`**.

2. **Portabilidad Inmediata a Nuevos Códecs y Targets:**
   - En el **ESP32-P4**, el sink se conecta al driver del códec **Everest ES8311** vía I2C e I2S DMA.
   - En el **ESP32-S3**, el sink se conecta a **ES8388** o a un amplificador digital estándar como **MAX98357A**.
   - Para futuros targets (ej. simulación en PC/Linux o soporte USB Audio), no es necesario modificar ni una línea de los decodificadores ni de las interfaces gráficas: únicamente se implementa un nuevo `IAudioSink` de ~50 líneas de código.

3. **Efectos de Audio y Enrutamiento Centralizado en el Kernel:**
   - Al canalizar el flujo de muestras PCM por un punto central agnóstico, `core/` puede calcular espectros FFT en tiempo real para visualizadores de ondas en la UI, aplicar volumen maestro o generar tonos y alertas del sistema (`playTone()`, `playBeep()`) sin interferir con el hardware físico.

4. **Grabación de Audio Simétrica y Modular (`IAudioSource`):**
   - La aplicación `AudioRecorderView` y la clase `WavRecorder` solo se preocupan por estructurar el encabezado RIFF/WAV y almacenar los datos en disco a través de `cbdos::storage`.
   - La obtención de muestras crudas del micrófono se delega al `IAudioSource`, aislando el muestreo DMA de la gestión de archivos.

---

### 6.2. Análisis de Rendimiento e Impacto en CPU

```
┌────────────────────────────────────────────────────────────────────────┐
│ FLUJO DE EJECUCIÓN DEL PIPELINE DE AUDIO EN CBDOS                      │
├────────────────────────────────────────────────────────────────────────┤
│ 1. Lectura de archivo MP3/WAV desde MicroSD/Flash (cbdos::storage)     │
│    └─► Consumo de CPU: Mínimo (transferencia por bloques)              │
│                                                                        │
│ 2. Decodificación Matemática (libhelix MP3/AAC en CPU)                │
│    └─► Consumo de CPU: 99.5% del tiempo total de procesamiento         │
│        (Transformada DCT, descompresión Huffman, síntesis sub-banda)   │
│                                                                        │
│ 3. Transferencia de muestras PCM al Sink (IAudioSink->write())         │
│    └─► Consumo de CPU: < 0.001% (una llamada virtual cada ~23 ms,      │
│        toma menos de 1 microsegundo por bloque de 1024 bytes)          │
│                                                                        │
│ 4. Transmisión Física I2S hacia el Códec (Controlador DMA Hardware)    │
│    └─► Consumo de CPU: 0% (el hardware DMA alimenta el bus I2S        │
│        de forma autónoma y transparente)                               │
└────────────────────────────────────────────────────────────────────────┘
```

* **Impacto en Rendimiento:** **0% (Inapreciable).** La indirección de puntero virtual en C++ toma 2 a 4 ciclos de reloj por bloque decodificado. En una CPU a 400 MHz (ESP32-P4) o 240 MHz (ESP32-S3), esto representa nanosegundos frente a los miles de ciclos que toma la decodificación Huffman en libhelix.
* **Garantía de Fluidez:** Se preserva la tasa de 60 FPS estables en la interfaz gráfica LVGL 9 y reproducción de audio cristalina a 44.1 kHz / 16-bit estéreo sin cortes ni caídas de buffer (*buffer underruns*).

