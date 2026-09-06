# 🗺️ Plan Maestro de Refactorización Fase 2: Storage HAL, Audio Sink y UART Terminal

**Documento:** `docs/architecture/plan_maestro_refactorizacion_fase_dos_storage_audio_uart.md`  
**Versión:** 1.0.0  
**Estado:** Plan Propuesto para Aprobación  
**Autor:** Equipo de Arquitectura CBDos  
**Fecha:** Agosto 2026  

---

## 📌 1. Resumen Ejecutivo

Habiendo culminado exitosamente la Fase 1 (**Persistencia NVS `IPersistenceBackend`**, **Radio `IRadioBackend`**, **Red `INetworkAdapter`** y **Malla `IMeshTransport`**), la **Fase 2** tiene como objetivo eliminar los últimos `weak symbols` y funciones directas restantes en `core/src/cbdos_core.cpp`, consolidando el 100% del sistema bajo la **Ley de Pureza Arquitectónica de `core/` (Zero Platform Pollution)**.

Este plan detalla la abstracción y desacoplamiento de:
1. **Subsistema de Almacenamiento y Archivos (`cbdos::storage` / `IStorageBackend`):** MicroSD (SDMMC 4-bit en P4 / SPI en S3), Flash Interna (SPIFFS/LittleFS) y almacenamiento USB MSC Host.
2. **Subsistema de Audio y Salida I2S (`cbdos::audio` / `IAudioSink`):** Salida PCM I2S hacia códec ES8311 (P4) / ES8388 (S3) desacoplada de la decodificación Helix (MP3/AAC/WAV) en `core/`.
3. **Subsistema de Comunicación Serial (`cbdos::uart` / `IUartBackend`):** Acceso multi-puerto para terminal interactiva, consolas de depuración y puertos de expansión JP1.

---

## 🏛️ 2. Arquitectura de los Nuevos Contratos HAL

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          APLICACIONES Y UI (CORE)                           │
│   FileManagerView  •  MusicPlayerView  •  SerialTerminalView  •  LuaRunner  │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (APIs de Alto Nivel)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                    DESPACHADORES AGNÓSTICOS (CORE)                          │
│   cbdos::storage::*        │   cbdos::audio::*          │   cbdos::uart::*  │
│   (storage.cpp)            │   (AudioPipeline.cpp)      │   (uart.cpp)      │
└──────────────────┬─────────┴────────────┬───────────────┴───────────┬───────┘
                   │                      │                           │
                   ▼ (Contratos HAL C++)  ▼                           ▼
    ┌─────────────────────────┐  ┌─────────────────┐  ┌───────────────────────┐
    │     IStorageBackend     │  │   IAudioSink    │  │     IUartBackend      │
    └──────────────┬──────────┘  └────────┬────────┘  └───────────┬───────────┘
                   │                      │                       │
 ┌─────────────────┴──────────────────────┴───────────────────────┴──────────┐
 │                     INYECCIÓN EN TIEMPO DE ARRANQUE (BSP)                 │
 ├──────────────────────────────────────────┬────────────────────────────────┤
 │          ESP32-S3 (JC3248)               │          ESP32-P4 (JC4880)     │
 │  • S3StorageBackend (SD SPI/LittleFS)    │  • P4StorageBackend (SDMMC/VFS)│
 │  • S3AudioSink (I2S Arduino / ES8388)    │  • P4AudioSink (I2S / ES8311)  │
 │  • S3UartBackend (HardwareSerial)        │  • P4UartBackend (driver/uart) │
 └──────────────────────────────────────────┴────────────────────────────────┘
```

---

## 📐 3. Especificación de Interfaces (`core/include/cbdos/`)

### 3.1. Almacenamiento (`core/include/cbdos/storage.hpp`)

```cpp
namespace cbdos {
namespace storage {

enum class StorageType {
    InternalFlash,
    SdCard,
    UsbDrive
};

struct FileEntry {
    std::string name;
    size_t size;
    bool isDirectory;
};

struct StorageStats {
    bool isMounted;
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t freeBytes;
    std::string label;
};

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual bool init() = 0;
    virtual bool mountSd() = 0;
    virtual bool unmountSd() = 0;
    virtual bool isSdMounted() const = 0;
    virtual bool isFlashMounted() const = 0;

    virtual StorageStats getFlashStats() const = 0;
    virtual StorageStats getSdCardStats() const = 0;
    virtual StorageStats getUsbStats() const = 0;

    virtual std::vector<FileEntry> listDir(const char* path) = 0;
    virtual bool fileExists(const char* path) = 0;
    virtual std::string readFile(const char* path) = 0;
    virtual bool writeFile(const char* path, const std::string& content) = 0;
    virtual bool deleteFile(const char* path) = 0;
    virtual bool copyFile(const char* srcPath, const char* dstPath) = 0;
    virtual bool makeDir(const char* path) = 0;
    virtual size_t getFreeBytes(StorageType type) const = 0;
    virtual size_t getTotalBytes(StorageType type) const = 0;
};

void setStorageBackend(IStorageBackend* backend);
IStorageBackend* getStorageBackend();

// APIs públicas de conveniencia
bool init();
bool mountSd();
bool unmountSd();
bool isSdMounted();
bool isFlashMounted();
StorageStats getFlashStats();
StorageStats getSdCardStats();
StorageStats getUsbStats();
std::vector<FileEntry> listDir(const char* path);
bool fileExists(const char* path);
std::string readFile(const char* path);
bool writeFile(const char* path, const std::string& content);
bool deleteFile(const char* path);
bool copyFile(const char* srcPath, const char* dstPath);
bool makeDir(const char* path);
size_t getFreeBytes(StorageType type);
size_t getTotalBytes(StorageType type);

} // namespace storage
} // namespace cbdos
```

---

### 3.2. Audio Sink (`core/include/cbdos/audio.hpp`)

```cpp
namespace cbdos {
namespace audio {

struct AudioStats {
    bool isPlaying;
    bool isMuted;
    uint8_t volume;
    uint32_t sampleRate;
    uint8_t channels;
    uint8_t bitsPerSample;
    size_t bufferUnderruns;
};

class IAudioSink {
public:
    virtual ~IAudioSink() = default;

    virtual bool init(uint32_t sampleRate, uint8_t channels, uint8_t bitsPerSample) = 0;
    virtual size_t write(const int16_t* pcmSamples, size_t sampleCount) = 0;
    virtual void setVolume(uint8_t volumePercent) = 0;
    virtual uint8_t getVolume() const = 0;
    virtual void mute(bool enable) = 0;
    virtual bool isMuted() const = 0;
    virtual void deinit() = 0;
};

void setAudioSink(IAudioSink* sink);
IAudioSink* getAudioSink();

// Control de Pipeline Universal (Core)
bool init();
bool playStream(const char* url);
bool playFile(const char* path);
void stop();
void pause();
void resume();
void setVolume(uint8_t vol);
uint8_t getVolume();
AudioStats getStats();

} // namespace audio
} // namespace cbdos
```

---

### 3.3. Terminal UART (`core/include/cbdos/uart.hpp`)

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

void setUartBackend(IUartBackend* backend);
IUartBackend* getUartBackend();

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

## 🛠️ 4. Etapas de Ejecución

### Etapa 2.1: Almacenamiento (`IStorageBackend`) ✅ [COMPLETADA Y VALIDADA]
1. Actualizar `core/include/cbdos/storage.hpp` con la interfaz `IStorageBackend`. (Completado)
2. Crear `core/src/system/storage.cpp` implementando el despacho agnóstico. (Completado)
3. Implementar `S3StorageBackend` en `bsp/esp32_s3_jc3248/hal/hal_storage_s3.cpp`. (Completado)
4. Implementar `P4StorageBackend` en `bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp`. (Completado)
5. Inyectar `initStorageBackend()` en `main.cpp` de S3 y P4. (Completado)
6. Validación multi-target limpia: ESP-IDF 5.5 (P4) y PlatformIO (S3). (Completado)

### Etapa 2.2: Salida de Audio (`IAudioSink`)
1. Actualizar `core/include/cbdos/audio.hpp` con la interfaz `IAudioSink`.
2. Conectar el decodificador Helix y el `AudioPlayer` de `core/` para emitir muestras PCM directamente a `IAudioSink`.
3. Implementar `S3AudioSink` en `bsp/esp32_s3_jc3248/hal/hal_audio_s3.cpp`.
4. Implementar `P4AudioSink` en `bsp/esp32_p4_jc4880/hal/hal_audio_p4.cpp`.
5. Inyectar `initAudioSink()` en `main.cpp` de S3 y P4.

### Etapa 2.3: Terminal UART (`IUartBackend`)
1. Actualizar `core/include/cbdos/uart.hpp` con la interfaz `IUartBackend`.
2. Crear `core/src/system/uart.cpp` con el despacho agnóstico.
3. Implementar `S3UartBackend` en `bsp/esp32_s3_jc3248/hal/hal_uart_s3.cpp`.
4. Implementar `P4UartBackend` en `bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`.
5. Inyectar `initUartBackend()` en `main.cpp` de S3 y P4.

### Etapa 2.4: Limpieza Total de `cbdos_core.cpp`
1. Remover todos los `weak symbols` restantes en `cbdos_core.cpp`.
2. Verificar compilación limpia multi-target (`pio run` e `idf.py build`).
3. Flashear y validar en hardware físico P4.

---

## 🧪 5. Plan de Pruebas y Validación

1. **Prueba de MicroSD en P4 y S3:**
   - Exploración de carpetas, lectura y escritura de archivos desde `FileManagerView` y `LuappManager`.
2. **Prueba de Reproducción de Audio:**
   - Reproducir archivo MP3/WAV desde MicroSD y validar salida limpia por altavoz/DAC sin saturación ni cortes.
3. **Prueba de Terminal Serie:**
   - Enviar y recibir datos interactivos desde `SerialTerminalView` hacia hardware externo.
4. **Verificación Multi-Target:**
   - `pio run -d bsp/esp32_s3_jc3248` (0 warnings, 0 errores).
   - `idf.py build` en `bsp/esp32_p4_jc4880` (100% SUCCESS).
