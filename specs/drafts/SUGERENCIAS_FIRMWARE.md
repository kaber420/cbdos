# 📋 Documento de Sugerencias, Errores y Dedos Técnicos — CBDos v0.2.3-dev

> **Proyecto:** CyBerDeck OS (CBDos) — Firmware embebido para ESP32-P4 / ESP32-S3  
> **Fecha:** Septiembre 2026  
> **Alcance:** Análisis de calidad de código, bugs potenciales, optimizaciones y mejores prácticas

---

## 1. 🐛 BUGS DETECTADOS

### 1.1. AudioPlayer: `getID3v2Size` — Desbordamiento de tipo firmado
**Archivo:** `core/src/audio/AudioPlayer.cpp:109-126`

```cpp
static uint32_t getID3v2Size(FILE* f) {
    uint8_t header[10];
    fseek(f, 0, SEEK_SET);
    if (fread(header, 1, 10, f) == 10) {
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
            uint32_t id3Size = ((header[6] & 0x7F) << 21) |
                               ((header[7] & 0x7F) << 14) |
                               ((header[8] & 0x7F) << 7)  |
                                (header[9] & 0x7F);
```
**Problema:** `fread` retorna `size_t` (unsigned), pero la comparación `== 10` es correcta. Sin embargo, **no se valida `fseek`** ni se verifica si `ftell` funciona antes de usarse en `m_fileSize = ftell(f)`. Si el archivo falla, `ftell` retorna `-1L`, que al asignarse a `uint32_t` se convierte en `4294967295`, causando desbordamiento.

**Sugerencia:** Validar siempre el retorno de `fseek` y `ftell`:
```cpp
if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
m_fileSize = ftell(f);
if (m_fileSize < 0) { fclose(f); return false; }
fseek(f, 0, SEEK_SET);
```

### 1.2. AudioPlayer: `probeMP3SampleRate` — Memory leak en `malloc`
**Archivo:** `core/src/audio/AudioPlayer.cpp:129-181`

**Problema:** En `probeMP3SampleRate`, se asigna `probeBuf` con `malloc` y `tmpBuf` también con `malloc`. Si `MP3FindSyncWord` falla (retorna `< 0`), el código hace `free(probeBuf)` y `MP3FreeDecoder(dec)` pero **NO hace `free(tmpBuf)`**. Esto causa un memory leak en un entorno con PSRAM limitada.

**Sugerencia:** Usar RAII o goto limpio:
```cpp
int16_t* tmpBuf = (int16_t*)malloc(...);
if (!tmpBuf) { free(probeBuf); MP3FreeDecoder(dec); return 44100; }
// ... código ...
free(tmpBuf);
free(probeBuf);
MP3FreeDecoder(dec);
```

### 1.3. AudioPlayer: `runWavPlayback` — Buffer stack de 16-bit sin alineación
**Archivo:** `core/src/audio/AudioPlayer.cpp:651-722`

**Problema:** En `runWavPlayback`, se lee `fmtData` con `fread(fmtData, 1, 16, m_file)` y luego se castea a `uint16_t*` y `uint32_t*` para leer campos. En ESP32 (RISC-V), el acceso a memoria no alineada puede causar excepciones o datos corruptos.

**Sugerencia:** Usar `memcpy` para extraer campos de bytes:
```cpp
uint16_t numCh;
memcpy(&numCh, fmtData + 2, 2);
uint32_t sampleRate;
memcpy(&sampleRate, fmtData + 4, 4);
```

### 1.4. AudioPlayer: `m_totalTimeSec` — Estimación basura y falta de uso de metadatos MP3
**Archivo:** `core/src/audio/AudioPlayer.cpp:551, 617`

```cpp
m_totalTimeSec = (m_fileSize > id3Offset) ? ((m_fileSize - id3Offset) / 16000) : 180;
```
**Problema 1:** El cálculo asume un bitrate fijo de 16000 bps (~2 KB/s). Esto es una aproximación muy pobre para MP3 (bitrate real: 64-320 kbps). El valor se corrige después en `runMp3Playback()` con `info.bitrate`, pero la estimación inicial incorrecta puede causar UI parpadeante o seek incorrecto.

**Problema 2:** **Los MP3 ya tienen duración en metadatos.** No hay razón para estimar. Los metadatos ID3v2 almacenan la duración real:
- **`TLEN`** (Text Information frame): Longitud en milisegundos como texto
- **`TDUR`**: Duración en milisegundos
- **Header Xing/Info**: En el primer frame MPEG contiene `nSamples` → `duración = nSamples / sampleRate`

Leer `TLEN`/`TDUR` tiene coste **casi cero** porque `play()` ya lee el header ID3v2 (vía `getID3v2Size()`) para saltar metadatos. Solo hay que parsear 2-3 frames más buscando esas tags.

**Sugerencia:** Implementar `getID3v2Duration()`:
1. Después de `getID3v2Size()`, iterar los frames del ID3v2 buscando "TLEN" o "TDUR"
2. Si existe, parsear el valor como `uint32_t` (ms) y asignar a `m_totalTimeSec`
3. Si no existen metadatos, dejar `m_totalTimeSec = 0` — **la UI no muestra duración ni seek bar**
4. **NO mostrar "calculando..." ni placeholder** — si no hay dato, no se muestra
5. En `runMp3Playback()`, usar `info.bitrate` como respaldo si los metadatos no existieron y se logró decodificar el primer frame
6. **Eliminar completamente** `/ 16000` y el `180` como fallback


### 1.5. AudioPlayer: `runStreamPlayback` — Buffer de audio en streaming sin sincronización
**Archivo:** `core/src/audio/AudioPlayer.cpp:298-500`

**Problema:** En `runStreamPlayback`, se usa `cbdos::audio::writeAudio(pcmBuf, outSamples * sizeof(int16_t))` pero no hay mecanismo de backpressure. Si la tasa de recepción de red supera la tasa de reproducción I2S, los buffers de PSRAM se llenarán y se perderán muestras. Además, `m_totalTimeSec = 0` para streaming significa que cualquier operación de seek o UI de progreso se comportará mal.

**Sugerencia:** Implementar un anillo de buffers (ring buffer) con límite máximo y mecanismo de backpressure. Agregar un `m_bufferLevel` que reporte cuántos KB hay en cola.

### 1.6. ConfigManager: Contraseñas en texto plano en NVS
**Archivo:** `core/src/system/ConfigManager.cpp:193-239`

**Problema:** Las contraseñas WiFi se guardan en NVS sin cifrado:
```cpp
cfg.password = backend->getString("pass", "");
backend->setString("pass", cfg.password);
```
En un dispositivo con acceso físico (cyberdeck), cualquier persona con un lector UART o acceso al flash puede extraer credenciales WiFi.

**Sugerencia:** Implementar cifrado XOR con una key derivada del Device ID + una per-device salt, o usar el hardware AES del ESP32 para cifrar las credenciales antes de persistirlas.

### 1.7. ConfigManager: `loadSystem` — Validación de language hardcodeada
**Archivo:** `core/src/system/ConfigManager.cpp:73`

```cpp
if (cfg.language != "en") cfg.language = "es";
```
**Problema:** Solo soporta "en" y "es". Si alguien guarda "pt" o "fr", se sobrescribe silenciosamente a "es" sin advertencia.

**Sugerencia:** Mantener el valor original y validar en la UI, o usar un set permitido:
```cpp
static const std::set<std::string> VALID_LANGUAGES = {"en", "es", "pt", "fr"};
if (VALID_LANGUAGES.find(cfg.language) == VALID_LANGUAGES.end()) {
    cfg.language = "es"; // fallback silencioso
}
```

### 1.8. UIManager: `ta_event_cb` — Memory leak de teclado
**Archivo:** `core/src/ui/UIManager.cpp:170-205`

**Problema:** Cada vez que un `textarea` recibe `LV_EVENT_FOCUSED`, se crea un nuevo teclado con `lv_keyboard_create(topLayer)`. El teclado anterior no se verifica si está oculto o se reutiliza. Si el usuario toca 10 textareas seguidas, se crean 10 keyboards.

**Sugerencia:** Verificar si `s_activeKeyboard` ya existe y está oculto antes de crear uno nuevo, o reutilizar el existente:
```cpp
if (!s_activeKeyboard || !lv_obj_is_valid(s_activeKeyboard)) {
    s_activeKeyboard = lv_keyboard_create(topLayer);
    // ... configurar ...
}
lv_obj_clear_flag(s_activeKeyboard, LV_OBJ_FLAG_HIDDEN);
```

### 1.9. UIManager: `pushView` — Race condition con `m_viewStack`
**Archivo:** `core/src/ui/UIManager.cpp:212-236`

**Problema:** `m_viewStack` es un `std::vector<std::shared_ptr<BaseView>>` que se modifica desde múltiples contextos (task de UI + posible interrupción). No hay mutex que proteja el acceso. Si `popView` se llama mientras `pushView` está en ejecución, el vector puede corruptarse.

**Sugerencia:** Agregar un mutex de protección para `m_viewStack`:
```cpp
std::lock_guard lock(m_viewStackMutex);
m_viewStack.push_back(view);
```

### 1.10. DuckyInterpreter: `REPEAT` — Stack overflow potencial
**Archivo:** `core/src/hid/DuckyInterpreter.cpp:157-165`

```cpp
if (upperCmd == "REPEAT") {
    uint32_t count = 0;
    if (ss >> count && !m_lastCommand.empty()) {
        for (uint32_t i = 0; i < count; ++i) {
            parseAndExecuteLine(m_lastCommand);
        }
    }
    return;
}
```
**Problema:** Si `count` es muy grande (ej. `999999`) y `m_lastCommand` contiene un `DELAY` de 1000ms, esto bloquearía el sistema durante ~11 días. Además, si `m_lastCommand` contiene otro `REPEAT`, hay recursion anidada.

**Sugerencia:** Agregar un límite máximo de repeticiones y validar:
```cpp
const uint32_t MAX_REPEAT = 1000;
if (count > MAX_REPEAT) count = MAX_REPEAT;
```

### 1.11. MeshEngine: `m_currentMsgId` — Race condition sin atomics
**Archivo:** `core/src/mesh/MeshEngine.cpp:151-152`

```cpp
uint8_t msg_id = m_currentMsgId++;
if (m_currentMsgId == 0) m_currentMsgId = 1;
```
**Problema:** Si `sendPacket` se llama desde diferentes cores o tasks sin protección, `m_currentMsgId++` no es atómico. En ESP32-P4 (dual-core RISC-V), esto puede causar colisiones de msg_id.

**Sugerencia:** Usar `std::atomic<uint8_t>` o un spinlock:
```cpp
static std::atomic<uint8_t> s_msgIdCounter{1};
uint8_t msg_id = s_msgIdCounter++;
```

### 1.12. KerberosManager: `userPresentThunk` — Bloqueo activo consumiendo CPU
**Archivo:** `core/src/security/kerberos/KerberosManager.cpp:169-222`

**Problema:** El bucle `while (self->m_pendingPresence)` hace `delay(10)` y llama a `lv_timer_handler()` y `cbdos::ui::update()` en cada iteración. Esto crea un busy-wait que consume un core entero durante hasta 30 segundos.

**Sugerencia:** Implementar como un FreeRTOS timer o usar un event group con timeout:
```cpp
// En vez del while loop, registrar un callback y retornar
// El callback se dispara cuando el usuario toca o aprueba
```

---

## 2. ⚠️ PROBLEMAS DE ARQUITECTURA Y CÓDIGO

### 2.1. Singleton pattern — Dependencias ocultas y orden de inicialización
**Archivos afectados:** `AudioPlayer`, `UIManager`, `DuckyInterpreter`, `MeshEngine`, `ConfigManager`, `KerberosManager`, etc.

**Problema:** El patrón Singleton se usa extensivamente pero hay **dependencias circulares**:
- `UIManager` incluye `PowerManager.hpp`, `LuaBridge.hpp`
- `AudioPlayer` incluye `cbdos/network.hpp`, `cbdos/socket.hpp`
- `KerberosManager` incluye `cbdos/ui.hpp`, `cbdos/input.hpp`

Esto puede causar el **Static Initialization Order Fiasco (SIFO)**, donde un singleton se usa antes de que otro esté inicializado.

**Sugerencia:** 
- Usar `std::shared_ptr` o un `init()` explícito que retorne `bool`
- Implementar un `ServiceLocator` o `AppContext` central que se pase a los componentes
- Eliminar dependencias circulares con forward declarations

### 2.2. Falta de `#pragma once` consistente
**Archivos:** Mezcla de `#pragma once` y `#ifndef` guards en diferentes archivos.

**Sugerencia:** Estandarizar a `#pragma once` en todos los headers (es soportado por todos los compiladores modernos en ESP-IDF).

### 2.3. Inconsistente uso de `FILE*` vs `std::fstream`
**Problema:** El código usa `FILE*` (C-style) con `fopen`, `fread`, `fseek`, `fclose` en algunas partes y debería usar `std::fstream` o wrappers RAII. En un sistema embebido con PSRAM limitada, `FILE*` puede ser más eficiente, pero la falta de RAII causa memory leaks si hay excepciones o retornos tempranos.

**Sugerencia:** Crear una clase `ScopedFILE` RAII:
```cpp
class ScopedFILE {
    FILE* m_fp;
public:
    explicit ScopedFILE(const char* path, const char* mode) : m_fp(fopen(path, mode)) {}
    ~ScopedFILE() { if (m_fp) fclose(m_fp); }
    operator FILE*() { return m_fp; }
    ScopedFILE(const ScopedFILE&) = delete;
    ScopedFILE& operator=(const ScopedFILE&) = delete;
};
```

### 2.4. Falta de logging estructurado
**Problema:** El sistema usa `CBD_LOG_I(TAG, "...")` macros pero no hay un sistema de logging jerárquico con niveles configurables por módulo. El `log::write` en `cbdos_core.cpp` es un `printf` simple.

**Sugerencia:** Implementar:
- Timestamps en cada log message
- Filtrado por módulo (audio, mesh, ui, security)
- Buffer circular en PSRAM para logs de crash (circular log buffer)
- Nivel de log configurable por módulo

### 2.5. Sin sistema de error robusto
**Problema:** Hay una mezcla de códigos de retorno: `bool` para éxito/fallo, `int` para errores numéricos, `nullptr`, excepciones no usadas. No hay un `Error` o `Result<T>` type que unifique el manejo de errores.

**Sugerencia:** Implementar un `Result<T>` tipo:
```cpp
template<typename T>
struct Result {
    bool ok;
    T value;
    int errorCode;
    const char* message;
};
```

### 2.6. Magic numbers everywhere
**Problema:** Valores como `800`, `58`, `44100`, `16000`, `2500`, `300000` aparecen repetidamente sin definiciones con nombre.

**Sugerencia:** Crear un archivo `constants.hpp`:
```cpp
namespace cbdos { namespace config {
    constexpr int SCREEN_HEIGHT = 800;
    constexpr int HEADER_BAR_HEIGHT = 58;
    constexpr int AUDIO_SAMPLE_RATE = 44100;
    constexpr int REASSEMBLY_TIMEOUT_MS = 2500;
    constexpr int ARP_EXPIRY_MS = 300000;
    constexpr int MAX_CHUNK_PAYLOAD = 240;
}}
```

### 2.7. `extern "C"` para wrappers C/C++
**Problema:** Archivos como `cbdos_core.cpp` usan `extern "C"` implícito o no para llamar a funciones de C. El `cbdos/rtos.hpp` y otros headers mezclan C y C++ sin protección consistente.

**Sugerencia:** Asegurar que todos los headers C tengan:
```cpp
#ifdef __cplusplus
extern "C" {
#endif
// ... contenido ...
#ifdef __cplusplus
}
#endif
```

---

## 3. 🔧 DEDOS TÉCNICOS / OPTIMIZACIONES

### 3.1. PSRAM Management — Asignación dinámica sin tracking
**Archivos:** `AudioPlayer.cpp`, `MeshEngine.cpp`, `runStreamPlayback`

**Problema:** Se asigna PSRAM con `cbdos::mem::alloc_psram()` pero nunca se verifica cuánta PSRAM queda libre. Con 32MB de PSRAM, es fácil agotarla con:
- Buffer de audio: ~64KB
- Decoder Helix: ~64KB
- Ring buffer de streaming: ~64KB
- Mesh chunks: ~2KB
- LVGL cache: ~256KB

**Sugerencia:** Implementar un monitor de PSRAM:
```cpp
size_t psram_free = esp_get_free_heap_size(); // PSRAM
if (psram_free < PSRAM_MINIMUM) {
    // Reducir buffers o rechazar operación
}
```

### 3.2. Task Stack Sizes — Sin verificación de uso real
**Problema:** Los stack sizes se especifican hardcodeados:
- `playbackTask`: 8192 bytes
- `streamPlaybackTask`: 12288 bytes

En ESP32-P4 con RISC-V, el tamaño real necesario puede variar. Si un stack se desborda, corrompe la memoria y causa comportamiento indefinido.

**Sugerencia:** Usar `uxTaskGetStackHighWaterMark()` para medir el uso real y ajustar. Agregar una utilidad que verifique stack usage en tiempo de ejecución:
```cpp
UBaseType_t highWater = uxTaskGetStackHighWaterMark(NULL);
if (highWater < 512) {
    // Peligro de stack overflow
}
```

### 3.3. `volatile` misuse — Variables compartidas sin memory barriers
**Archivo:** `AudioPlayer.hpp:71-77`

```cpp
volatile uint32_t m_currentSec = 0;
volatile bool m_isPlaying = false;
volatile bool m_isPaused = false;
```
**Problema:** `volatile` en C++ **no garantiza atomicidad** ni memory ordering. En ESP32-P4 con arquitectura RISC-V, una escritura de 32-bit puede no ser atómica, y el compilador puede reordenar accesos. `volatile` solo impide optimizaciones del compilador, no barreras de CPU.

**Sugerencia:** Usar `std::atomic<>` para variables compartidas entre cores/tasks:
```cpp
std::atomic<uint32_t> m_currentSec{0};
std::atomic<bool> m_isPlaying{false};
```

### 3.4. `std::vector` en entornos embebidos
**Problema:** `std::vector` se usa extensivamente (`MeshEngine::m_reassemblies`, `ConfigManager::s_cachedGateways`). `std::vector` usa `malloc/realloc/free` internamente, que en ESP32 puede causar fragmentation de heap.

**Sugerencia:** Para contenedores en embebidos, usar:
- `std::array` con tamaño fijo si se conoce el máximo
- `etl::vector` (Embedded Template Library) con pool de memoria estático
- O un `std::vector` custom con allocator de PSRAM fijo

```cpp
// Ejemplo con pool estático
using ReassemblyList = std::array<ChunkReassembly, 4>; // Max 4 reensamblos
```

### 3.5. Funciones `__attribute__((weak))` — Sin sustituto fuerte verificado
**Archivo:** `core/src/cbdos_core.cpp:18-59`

**Problema:** Hay muchas funciones `weak` que son sobrescritas por el BSP, pero **no hay verificación de que exista una implementación fuerte**. Si el BSP no provee la implementación, se llama al `weak` que hace `malloc` o retorna `nullptr`.

**Sugerencia:** Agregar asserts o logs al inicio de cada `weak`:
```cpp
__attribute__((weak)) void* alloc_psram(size_t size) { 
    CBD_LOG_W("mem", "Usando fallback malloc para PSRAM");
    return ::malloc(size); 
}
```

### 3.6. Falta de unit tests
**Problema:** No hay framework de testing para el `core/` agnóstico. El BSP tiene tests de `smoke_test` pero no para la lógica de negocio del core.

**Sugerencia:** Implementar tests con Unity o Check:
- `tests/test_audio.cpp` — Test de detección de formato, cálculo de duración
- `tests/test_config.cpp` — Test de persistencia de configuración
- `tests/test_ducky.cpp` — Test de parsing de comandos DuckyScript
- `tests/test_mesh.cpp` — Test de fragmentación/reensamblado

### 3.7. `#include` dependency graph — Inclusión innecesaria
**Problema:** Headers incluyen más de lo necesario:
- `UIManager.cpp` incluye `lua/LuaBridge.hpp` solo para `checkAndClearNeedsRefresh()`
- `AudioPlayer.cpp` incluye `cbdos/network.hpp` y `cbdos/socket.hpp` solo para streaming

**Sugerencia:** Usar forward declarations donde sea posible y Pimpl para desacoplar:
```cpp
// En AudioPlayer.hpp
class ISocketStream; // Forward declaration
class AudioPlayer {
    std::unique_ptr<ISocketStream> m_stream; // Pimpl
};
```

### 3.8. `snprintf` sin validación de tamaño
**Problema:** En `AudioPlayer.cpp:346-353`:
```cpp
int reqLen = snprintf(req, sizeof(req), "...", path.c_str(), host.c_str());
```
Si `path` o `host` son muy largos, `snprintf` trunca sin advertencia. El HTTP request podría ser inválido.

**Sugerencia:** Verificar el retorno:
```cpp
int reqLen = snprintf(req, sizeof(req), "...", ...);
if (reqLen < 0 || reqLen >= sizeof(req)) {
    CBD_LOG_E(TAG, "HTTP request too long");
    return false;
}
```

### 3.9. `memcpy` sin verificación de tamaño
**Problema:** Múltiples usos de `memcpy` sin validar que el buffer destino tenga suficiente espacio:
```cpp
memcpy(dt->name, packet.payload.data() + 6, copy_n);
```
Si `copy_n > sizeof(dt->name)`, hay buffer overflow.

**Sugerencia:** Siempre usar `memcpy_s` o verificar:
```cpp
if (copy_n >= sizeof(dt->name)) copy_n = sizeof(dt->name) - 1;
memcpy(dt->name, packet.payload.data() + 6, copy_n);
dt->name[copy_n] = '\0';
```

### 3.10. `std::stringstream` en tiempo real — Asignación dinámica
**Problema:** `DuckyInterpreter::loadFromString` y `parseAndExecuteLine` usan `std::stringstream`, que hace `malloc/free` en cada línea. En tiempo real esto puede causar latencia inaceptable.

**Sugerencia:** Usar `std::string_view` o parsing manual:
```cpp
std::string_view line(m_lines[m_currentLine]);
size_t pos = line.find(' ');
std::string_view command = line.substr(0, pos);
std::string_view arg = (pos != std::string::npos) ? line.substr(pos + 1) : "";
```

---

## 4. 🛡️ SEGURIDAD

### 4.1. WiFi Passwords — Almacenamiento sin cifrado
**Archivo:** `ConfigManager.cpp:197, 228`

**Problema:** Contraseñas WiFi en texto plano en NVS.

**Sugerencia:** Cifrar con AES-128-ECB usando el hardware crypto del ESP32:
```cpp
esp_aes_context aes;
esp_aes_setkey(&aes, device_key, 128);
esp_aes_crypt_ecb(&aes, ESP_AES_ENCRYPT, (uint8_t*)password, encrypted_pwd);
```

### 4.2. Hardcoded PIN en `importGateway`
**Archivo:** `ConfigManager.cpp:353`

```cpp
if (pin == "1234") {
```
**Problema:** PIN hardcodeado. Cualquiera que sepa el código fuente puede importar gateways falsos.

**Sugerencia:** Usar un sistema de pairing con challenge-response o NVS protegido.

### 4.3. Falta de validación de inputs en red
**Problema:** `runStreamPlayback` parsea URLs sin validar. Una URL maliciosa podría:
- Hacer `host` muy largo y desbordar `req[512]`
- Usar protocolo `file://` para acceso arbitrario
- No verificar certificados TLS para HTTPS

**Sugerencia:** 
- Validar URL con regex antes de parsear
- Implementar cert pinning para HTTPS
- Limitar `host` a 63 caracteres (RFC DNS)

### 4.4. FIDO2/CTAP2 — `ctaphid_dispatch` sin timeout de seguridad
**Problema:** Si un atacante envía paquetes CTAPHID maliciosos, no hay límite de intentos ni rate limiting.

**Sugerencia:** Implementar:
- Lockout después de N intentos fallidos
- Rate limiting de paquetes por segundo
- Timeout de sesión CTAPHID después de 30 segundos

### 4.5. USB HID — BadUSB sin autenticación de payloads
**Problema:** Los scripts `.dd` se ejecutan sin verificación de firma. Cualquier `.dd` en la SD se ejecuta automáticamente.

**Sugerencia:** Implementar un sistema de firma de scripts con ECDSA o Ed25519.

---

## 5. 📐 MEJORES PRÁCTICAS DE CÓDIGO

### 5.1. Formato de nombres inconsistente
**Problema:** Mezcla de `snake_case`, `camelCase`, `PascalCase`, `SCREAMING_SNAKE_CASE`:
- `m_currentFile`, `m_isPlaying` — camelCase
- `m_frame_buffer` — snake_case (en otros archivos)
- `LV_OPA_COVER` — SCREAMING (pero es macro LVGL)
- `CTRL_DST_ONLY` — SCREAMING
- `DEFAULT_DELAY`, `DEFAULTDELAY` — SCREAMING

**Sugerencia:** Estandarizar:
- Miembros privados: `mCamelCase` (o `m_camel_case` según la convención del equipo)
- Constantes: `kCamelCase` o `DEFAULT_DELAY`
- Macros: `LV_...` (dejar como LVGL las externas)
- Funciones: `camelCase`
- Archivos: `snake_case`

### 5.2. Falta de `const` correctness
**Problema:** Funciones que no modifican el objeto no son `const`:
```cpp
std::string AudioPlayer::getCurrentTrackTitle() const { // Bien
std::string AudioPlayer::getFormatString() const {       // Bien
AudioFormat AudioPlayer::getFormat() const { return m_format; } // Bien
bool AudioPlayer::isPlaying() const { return m_isPlaying; }    // Bien
```
Pero otras funciones como `getStats()` deberían retornar por valor `const` o al menos no modificar estado. Verificar que todos los getters sean `const`.

### 5.3. `m_bytesProcessed` no se usa consistentemente
**Problema:** En `runWavPlayback`, `m_bytesProcessed` se actualiza, pero en `runMp3Playback` se usa `totalSamplesDecoded` local. En `AudioPlayer::getStats()`, `stats.bufferPercent = 100` es un placeholder.

**Sugerencia:** Usar un solo mecanismo de progreso y calcular `bufferPercent` real:
```cpp
if (m_fileSize > 0) {
    stats.bufferPercent = (uint8_t)((m_bytesProcessed * 100) / m_fileSize);
}
```

### 5.4. `m_totalTimeSec` para MP3 — Estimación basura y metadatos disponibles
**Archivo:** `AudioPlayer.cpp:551, 617`

```cpp
m_totalTimeSec = (m_fileSize > id3Offset) ? ((m_fileSize - id3Offset) / 16000) : 180;
```
**Problema:** `16000` no es un bitrate MP3 estándar. Es una estimación arbitraria. **Los MP3 ya tienen duración en metadatos ID3v2** (`TLEN`, `TDUR`) o en el header Xing/Info del primer frame. No debería necesitarse estimación alguna.

**Sugerencia:** Implementar `getID3v2Duration()` que parsee `TLEN`/`TDUR` del header ID3v2. Si no hay metadatos, `m_totalTimeSec = 0` y la UI no muestra duración. Eliminar `/ 16000` y el `180` como fallback.

### 5.5. Error handling en `MeshEngine::tick`
**Problema:** `tick` limpia reensamblados expirados pero no llama a `m_towersCb` si se expira un timeout, lo que puede dejar referencias zombie.

**Sugerencia:** Agregar callback para notificar expiración:
```cpp
if (r.start_time_ms > 0 && (current_time_ms - r.start_time_ms) > REASSEMBLY_TIMEOUT_MS) {
    if (m_reassemblyTimeoutCb) m_reassemblyTimeoutCb(r.msg_id);
    return true;
}
```

### 5.6. `sendTowerProbe` — Nombre hardcodeado
**Problema:**
```cpp
const char* client_name = "CBDos S3";
```
**Sugerencia:** Obtener el nombre del dispositivo de la configuración o board identity:
```cpp
const char* client_name = cbdos::board::getIdentity().name.c_str();
```

---

## 6. 📊 TABLA DE PRIORIDADES

| Prioridad | Problema | Impacto | Esfuerzo |
|:---|:---|:---:|:---:|
| 🔴 Alta | 1.1 - Validación de `ftell` | Crash / Corrupción | Bajo |
| 🔴 Alta | 1.2 - Memory leak en probeMP3 | Agotamiento PSRAM | Bajo |
| 🔴 Alta | 1.3 - Alineación WAV fmt | Crash RISC-V | Bajo |
| 🔴 Alta | 1.11 - Race condition msg_id | Colisiones mesh | Bajo |
| 🔴 Alta | 2.1 - Dependencias circulares | Initialization order | Alto |
| 🟡 Media | 1.6 - Contraseñas en texto plano | Security | Medio |
| 🟡 Media | 1.8 - Memory leak de teclado | Agotamiento LVGL objects | Bajo |
| 🟡 Media | 2.3 - Falta de RAII para FILE | Memory leaks | Medio |
| 🟡 Media | 3.3 - `volatile` misuse | Race conditions | Medio |
| 🟡 Media | 3.4 - `std::vector` en embebido | Heap fragmentation | Medio |
| 🟢 Baja | 5.1 - Formato de nombres | Mantenibilidad | Bajo |
| 🟢 Baja | 5.3 - bufferPercent placeholder | UX engañosa | Bajo |
| 🟢 Baja | 6.0 - Unit tests | Calidad | Alto |

---

## 7. 🚀 SUGERENCIAS DE MEJORA GENERAL

### 7.1. Implementar un Device Identity System
Agregar un `BoardIdentity` que se lee del hardware e incluye:
- Chip ID, revision, MAC addresses
- Serial number de fabrica
- Firmware version, build timestamp
- PSRAM size detectada

### 7.2. Agregar OTA Updates
El ESP32-P4 soporta OTA con ESP-IDF. Implementar:
- `OtaManager` en `core/src/system/`
- Verificación de firma del firmware
- A/B partitions para rollback seguro
- Notificación progreso en UI

### 7.3. Implementar Watchdog de seguridad
- Hardware watchdog configurado para resetear si el sistema se cuelga
- Software watchdog que monitorea tareas críticas
- Log de crash con backtrace en PSRAM

### 7.4. System Info Monitor (pendiente en roadmap)
Ya está planificado en el README como `⏳ Pendiente`. Implementar:
- Monitor de RAM libre, heap fragmentation
- FPS de LVGL en tiempo real
- Temperatura del chip (sensor internal ESP32)
- Uso de CPU por core

### 7.5. Refactorización del logging
- Agregar timestamps con `esp_timer_get_time()`
- Filtrado por módulo con nivel configurable
- Buffer circular para crash dumps
- Log de errors a un archivo en MicroSD para diagnóstico remoto

### 7.6. Documentación de APIs
- Generar Doxygen del `core/`
- Documentar todas las structs y sus campos
- Documentar el protocolo mesh (header format, packet types)
- Crear una API reference en `docs/`

### 7.7. CI/CD Pipeline
- Compilación automática para ambos targets (ESP-IDF + PlatformIO)
- Análisis estático de código con `cppcheck` o `clang-tidy`
- Linting con `clang-format`
- Tests automáticos para el `core/` agnóstico

---

## 8. 📝 NOTAS FINALES

### Sobre el código analizado
El firmware CBDos tiene una **base sólida** con una arquitectura modular bien pensada (core agnóstico + BSP). Sin embargo, al ser un proyecto en rápido desarrollo con muchos features, se acumularon:

1. **Problemas de concurrencia** por falta de sincronización adecuada entre cores/tasks
2. **Gestión de memoria inconsistente** por mezcla de C y C++ allocation
3. **Falta de validaciones** en entradas de usuario y red
4. **Magic numbers** que dificultan el mantenimiento

### Recomendación de priorización
1. **Primero:** Arreglar los bugs de corrupción de memoria (1.1, 1.2, 1.3, 1.11)
2. **Segundo:** Mejorar la seguridad (2.6 contraseñas, 4.1-4.5)
3. **Tercero:** Refactorizar arquitectura para reducir acoplamiento
4. **Cuarto:** Implementar tests y CI/CD
5. **Quinto:** Optimizaciones de rendimiento y documentación

---

*Documento generado para el equipo de desarrollo de CBDos.*  
*Para discusiones o dudas, abrir un issue en el repositorio con la etiqueta `tech-debt`.*
