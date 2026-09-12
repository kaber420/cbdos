# 🛠️ Especificación Técnica: Mejoras de Ingeniería, Concurrencia y Arquitectura de Firmware (CBDos v0.2.3-dev)

> **Documento:** `specs/drafts/BORRADOR_ESPECIFICACION_INGENIERIA_Y_MEJORAS_FIRMWARE.md`  
> **Estado:** Borrador de Ingeniería para Evaluación Integral  
> **Ámbito:** Audio, UI/LVGL 9.5, DuckyScript/HID, Mesh/RF, Memoria/PSRAM, FreeRTOS y RISC-V  
> **Fecha:** Septiembre 2026  
> **Autor:** Equipo de Arquitectura CBDos  

---

## 1. Justificación y Análisis Crítico Global

El documento preliminar `SUGERENCIAS_FIRMWARE.md` recopiló una lista inicial de bugs y recomendaciones. No obstante, la gran mayoría de sus propuestas se limitaron a soluciones superficiales de libro de texto (ej. "usar `free`", "no usar `std::vector`", "meter un mutex genérico", "usar `string_view`").

Para un sistema operativo embebido como **CBDos**, que opera sobre un procesador de alto rendimiento **RISC-V Dual-Core @ 400 MHz (ESP32-P4)** con **32 MB de Hexal-PSRAM** junto con un target secundario **ESP32-S3**, se requieren soluciones de **ingeniería de sistemas embebidos en tiempo real**.

Este documento aterriza cada componente con soluciones arquitectónicas profundas, garantizando el cumplimiento de la **Regla de Oro #8 (Pureza de `core/`)** y la **Regla de Oro #12 (Cero Polling innecesario / Arquitectura Reactiva)**.

---

## 2. Subsistema de Audio: Arquitectura de Streaming, DMA y Decodificación

### 2.1. El Problema Real (Más allá de los memory leaks puntuales)
* En `SUGERENCIAS_FIRMWARE.md` se detectaron fugas en `probeMP3SampleRate` y desalineación de structs WAV.
* Sin embargo, el **problema estructural de fondo** es que `AudioPlayer` mezcla en el mismo hilo la lectura de archivos/sockets, la decodificación Helix MP3 y la escritura I2S.
* En reproducción local o streaming WiFi, cualquier fluctuación de lectura o jitter de red provoca **I2S Buffer Underrun (chasquidos y micro-cortes)** o bloquea la tasa de refresco de la interfaz gráfica.

### 2.2. Solución de Ingeniería: Pipeline de Audio Desacoplado de 3 Etapas

```text
 ┌────────────────────────┐
 │   1. Fuente (Source)   │ ── Lectura no bloqueante (SD / Socket HTTP)
 └───────────┬────────────┘
             │ Ring Buffer de Entrada (FIFO 64 KB en PSRAM)
             ▼
 ┌────────────────────────┐
 │ 2. Decodificador Helix │ ── Tarea FreeRTOS dedicada (Prioridad Media)
 └───────────┬────────────┘
             │ Ring Buffer PCM de Salida (Alineado a 64 bytes para DMA)
             ▼
 ┌────────────────────────┐
 │   3. Consumidor I2S    │ ── Double Buffer DMA hacia ES8311 (Prioridad Alta)
 └────────────────────────┘
```

#### A. Alineación Estricta DMA para Caché RISC-V (ESP32-P4)
* **Física del hardware:** En el ESP32-P4, el controlador DMA hacia el bus I2S requiere que los buffers residan en direcciones alineadas a **64 bytes** (tamaño de la línea de caché L1/L2) para evitar corrupción por coherencia de caché (*cache incoherency*).
* **Implementación:**
  ```cpp
  // Buffer PCM estático alineado en PSRAM
  alignas(64) static int16_t s_pcmDmaBuffer[2][AUDIO_DMA_CHUNK_SAMPLES];
  ```

#### B. Streaming con Backpressure y Watermark de Pre-buffering
* Eliminar el sondeo en bucle de `runStreamPlayback`.
* Implementar un estado de **Pre-buffering**: el decodificador no arranca la salida I2S hasta que el buffer de red haya acumulado al menos **32 KB** de datos de audio.
* **Control de Flujo:** Si el buffer de red supera el 85% de capacidad, se pausa la lectura del socket; si baja del 25%, se reanuda (mitigación total de saturación de PSRAM y underruns).

#### C. Extracción Precisa de Metadatos ID3v2 y VBR
* Sustituir definitivamente la división arbitraria `(m_fileSize - id3Offset) / 16000` por un parser de frames ID3v2 en streaming:
  1. Lectura de tags `TLEN` (longitud en ms) y `TDUR`.
  2. Detección de headers Xing / VBRI en el primer frame MPEG para calcular duración exacta en pistas VBR (Variable Bit Rate).
  3. Si el archivo carece de metadatos, la UI no muestra una barra de duración engañosa, sino que conmuta automáticamente a modo "Tiempo Transcurrido" (`01:45 / --:--`).

---

## 3. Subsistema Gráfico y UI (LVGL 9.5 Estricto): Concurrencia y Eventos

### 3.1. Diagnóstico del Error de Concurrencia en `UIManager`
* La sugerencia preliminar proponía simplemente *"poner un `std::lock_guard` en `m_viewStack`"*.
* **Por qué es un parche incompleto:** En LVGL (versión 9.5), **las llamadas a la API de LVGL no son thread-safe**. Proteger únicamente el vector `m_viewStack` con un mutex no previene que una tarea de fondo (ej. llegada de mensaje LoRa o evento de batería) modifique objetos gráficos mientras `lv_timer_handler()` está renderizando la pantalla. Esto genera corrupción de memoria gráfica y caídas aleatorias (*Guru Meditation*).

### 3.2. Solución de Ingeniería: Cola de Despacho de UI (`lv_async_call` / UI Event Queue)

Toda tarea secundaria que necesite interactuar con la pantalla debe publicar un mensaje en una cola de despacho seguro:

```text
 [ Tarea Mesh / LoRa ] ──┐
 [ Tarea Monitoreo Batería] ──┼──► [ Cola de Eventos UI (Lock-Free FIFO) ]
 [ Tarea FIDO2 / Kerberos ] ──┘                  │
                                                 ▼
                                     [ Bucle Principal LVGL ]
                                     (lv_timer_handler @ 60 FPS)
                                                 │
                                     Ejecuta callback en contexto seguro
```

#### A. Implementación del Despachador Agnóstico:
```cpp
// core/include/cbdos/ui_dispatch.hpp
#pragma once
#include <functional>

namespace cbdos {
namespace ui {

// Encola una acción para ser ejecutada dentro del hilo seguro de LVGL
void postToUi(std::function<void()> action);

// Se procesa al inicio de cada ciclo de ui::update()
void processPendingUiActions();

}} // namespace cbdos::ui
```

#### B. Gestión Eficiente del Teclado Virtual (Zero Memory Leaks)
* En lugar de instanciar teclados dinámicamente con `lv_keyboard_create()` en cada evento de foco:
  1. Se crea **una única instancia global del teclado** durante el arranque en la capa superior (`lv_layer_sys()` o `lv_layer_top()`).
  2. El teclado permanece oculto (`LV_OBJ_FLAG_HIDDEN`) hasta que cualquier `textarea` del sistema recibe el foco.
  3. Al recibir foco, se llama a `lv_keyboard_set_textarea(s_globalKeyboard, ta)` y se limpia la bandera de ocultación con una animación suave de elevación (*slide-up*).
  4. Al pulsar Enter o cerrar, se reasigna la bandera oculta sin destruir el objeto en memoria.

---

## 4. DuckyScript e Intérprete BadUSB: Compilación a Bytecode en Tiempo Real

### 4.1. Diagnóstico del Rendimiento Actual
* El código actual utiliza `std::stringstream` para parsear texto línea a línea mientras se inyectan pulsaciones por USB.
* Si un script contiene `REPEAT 500` con un `STRING` largo, el microcontrolador aloca y desasigna cadenas cientos de veces por segundo, introduciendo fluctuaciones de latencia (jitter en microsegundos) que pueden provocar que la máquina víctima pierda pulsaciones.

### 4.2. Solución de Ingeniería: Tokenizador a Bytecode de Paso Único

En lugar de interpretar texto en crudo durante la ejecución:
1. **Fase de Carga (Load & Compile):** Al seleccionar el archivo `.dd`, un parser lineal convierte el script en una estructura de instrucciones compacta (8 bytes por instrucción):

```cpp
enum class DuckyOpcode : uint8_t {
    Delay,
    KeyDown,
    KeyUp,
    StringPrint,
    Combination,
    Repeat
};

struct alignas(4) DuckyInstruction {
    DuckyOpcode opcode;
    uint8_t modifier;      // CTRL, ALT, GUI, SHIFT
    uint16_t keycode;      // HID Usage ID
    uint32_t parameter;    // Retardo en ms o offset al pool de cadenas
};
```

2. **Fase de Disparo (Zero Allocation Execution):**
   * La ejecución se convierte en un bucle secuencial sobre un array contiguo de `DuckyInstruction`.
   * **Ventajas:**
     - Consumo de memoria predecible: un script de 1,000 líneas ocupa apenas 8 KB de RAM.
     - Cero fragmentación del heap.
     - Ejecución en tiempo real con precisión de milisegundos reales vía temporizadores FreeRTOS, sin jitter de asignación de memoria.
     - Validación sintáctica previa al disparo: si el script tiene un error de sintaxis en la línea 400, el usuario se entera antes de empezar a inyectar, no a mitad del ataque.

---

## 5. Redes de Malla (MeshEngine): Prevención de Colisiones y Buffer Bloat

### 5.1. Identificadores de Mensaje y Tipado Atómico
* Se valida la recomendación preliminar de sustituir `m_currentMsgId++` por `std::atomic<uint8_t>` o un contador atómico de 16 bits (`std::atomic<uint16_t>`), ya que el ESP32-P4 dispone de instrucciones atómicas nativas en su arquitectura RISC-V (**Standard Extension "A" for Atomic Instructions**).
* Al usar 16 bits para `msg_id`, el espacio de nombres de paquetes activos aumenta de 256 a 65,536, eliminando por completo colisiones en redes saturadas.

### 5.2. Gestor de Reensamblado Fijo (Anti-OOM)
* Sustituir el vector dinámico `std::vector<ChunkReassembly> m_reassemblies` por un pool estático en PSRAM:
  ```cpp
  static constexpr size_t MAX_CONCURRENT_REASSEMBLIES = 8;
  std::array<ChunkReassemblySlot, MAX_CONCURRENT_REASSEMBLIES> m_reassemblyPool;
  ```
* Cada ranura tiene un temporizador de caducidad estricto (2,500 ms). Si un fragmento se pierde y vence el temporizador, la ranura se libera automáticamente sin fragmentar la memoria dinámica.

### 5.3. Priorización de Tráfico (QoS en Cola RF)
Implementación de una cola de salida con dos niveles de prioridad:
* **Alta Prioridad:** Beacons de enrutamiento, paquetes de control ACK y paquetes tácticos FIDO2.
* **Prioridad Normal:** Mensajes de texto plano y fragmentos de archivos.
* El transmisor siempre vacía la cola de alta prioridad antes de enviar tráfico ordinario, reduciendo la latencia de control en más de un 70%.

---

## 6. Arquitectura FreeRTOS y Concurrencia Dual-Core (ESP32-P4)

### 6.1. Asignación Óptima de Tareas a Núcleos (Core Pinning)
El ESP32-P4 cuenta con dos núcleos RISC-V operando a 400 MHz. Para evitar contención en la caché de instrucciones y garantizar 60 FPS estables en la pantalla táctil de 4.3":

```text
┌───────────────────────────────────────┬───────────────────────────────────────┐
│          CORE 0: SISTEMA E I/O        │          CORE 1: INTERFAZ Y RENDER    │
├───────────────────────────────────────┼───────────────────────────────────────┤
│ • Decodificador de Audio (Helix MP3)  │ • Bucle Gráfico LVGL 9.5              │
│ • Pila de Redes (WiFi / SDIO ESP-C6)  │ • Controlador Táctil GT911 (I2C)      │
│ • Radio Malla LoRa / Transmisión RF   │ • Máquina Virtual Lua (Lógica de Apps)│
│ • Pila USB Device (HID / CDC ACM)     │ • Efectos Gráficos y Renderizado      │
│ • Watchdog de Sistema                 │ • Despacho de Eventos de Usuario      │
└───────────────────────────────────────┴───────────────────────────────────────┘
```

### 6.2. Stack Sizes y Detección de Fugas en Tiempo de Ejecución
* En lugar de tamaños arbitrarios (8 KB, 12 KB), definir perfiles de pila calibrados y monitoreados mediante `uxTaskGetStackHighWaterMark()`:

| Tarea | Núcleo | Stack Asignado | Justificación Técnica |
| :--- | :---: | :---: | :--- |
| **`task_ui`** | 1 | 16 KB (PSRAM) | Maneja estructuras complejas de widgets LVGL 9.5 y renderizado. |
| **`task_audio_dec`** | 0 | 12 KB (PSRAM) | Estructuras internas del decodificador Helix y buffers I2S. |
| **`task_mesh_radio`** | 0 | 8 KB (Internal) | Memoria interna rápida (SRAM) para minimizar latencia en interrupciones RF. |
| **`task_usb_dev`** | 0 | 6 KB (Internal) | Procesamiento de endpoints USB TinyUSB / ESP-IDF. |

* **Protección Activa:** Si el `HighWaterMark` de cualquier tarea cae por debajo de 512 bytes, el sistema emite un aviso crítico por consola serial (`CBD_LOG_W`) antes de que ocurra un desbordamiento catastrófico.

---

## 7. Manejo Robusto de Errores y Calidad de Código en `core/`

### 7.1. Adopción de `cbdos::Result<T, ErrorCode>` sin Overhead de RTTI
Para eliminar la inconsistencia de retornar `bool`, punteros nulos o códigos numéricos ambiguos sin activar excepciones C++:

```cpp
// core/include/cbdos/result.hpp
#pragma once
#include <utility>

namespace cbdos {

enum class ErrorCode {
    Ok = 0,
    OutOfMemory,
    InvalidArgument,
    Timeout,
    IoError,
    HardwareFault,
    NotSupported
};

template <typename T>
class Result {
public:
    Result(const T& val) : m_hasValue(true), m_value(val), m_error(ErrorCode::Ok) {}
    Result(T&& val) : m_hasValue(true), m_value(std::move(val)), m_error(ErrorCode::Ok) {}
    Result(ErrorCode err) : m_hasValue(false), m_error(err) {}

    bool isOk() const { return m_hasValue; }
    const T& value() const { return m_value; }
    ErrorCode error() const { return m_error; }

private:
    bool m_hasValue;
    T m_value;
    ErrorCode m_error;
};

} // namespace cbdos
```

### 7.2. Contenedor RAII para Archivos (`ScopedFile`)
Para evitar memory leaks y descriptores zombis en operaciones con tarjetas MicroSD:

```cpp
// core/include/cbdos/scoped_file.hpp
#pragma once
#include <cstdio>

namespace cbdos {

class ScopedFile {
public:
    explicit ScopedFile(const char* path, const char* mode) : m_fp(std::fopen(path, mode)) {}
    ~ScopedFile() { close(); }

    void close() {
        if (m_fp) {
            std::fclose(m_fp);
            m_fp = nullptr;
        }
    }

    FILE* get() const { return m_fp; }
    operator FILE*() const { return m_fp; }
    bool isOpen() const { return m_fp != nullptr; }

    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;

    ScopedFile(ScopedFile&& other) noexcept : m_fp(other.m_fp) { other.m_fp = nullptr; }
    ScopedFile& operator=(ScopedFile&& other) noexcept {
        if (this != &other) {
            close();
            m_fp = other.m_fp;
            other.m_fp = nullptr;
        }
        return *this;
    }

private:
    FILE* m_fp;
};

} // namespace cbdos
```

---

## 8. Matriz Comparativa: Sugerencias Preliminares vs. Esta Especificación

| Área | Propuesta en `SUGERENCIAS_FIRMWARE.md` | Solución de Ingeniería en este Documento |
| :--- | :--- | :--- |
| **Audio** | Validar `ftell` y poner `free(tmpBuf)`. | **Pipeline desacoplado de 3 etapas**: Ring buffers con watermark, memoria DMA alineada a 64 bytes para caché RISC-V, parser preciso ID3v2 TLEN/Xing. |
| **UI Concurrencia** | Mutex plano en `m_viewStack`. | **UI Event Queue**: Las tareas secundarias jamás tocan LVGL; publican acciones en una cola procesada de forma segura a 60 FPS por el hilo gráfico. |
| **Teclado LVGL** | Validar si `s_activeKeyboard` existe. | **Instancia única en `sys_layer`**: Cero destrucciones dinámicas; animación slide-up/down y reasignación de puntero de input. |
| **DuckyScript** | Usar `std::string_view` en lugar de `stringstream`. | **Compilación previa a Bytecode**: Las instrucciones se ejecutan desde un array contiguo de 8 bytes; latencia determinista y cero asignaciones en tiempo real. |
| **Mesh / LoRa** | Poner `std::atomic<uint8_t>` en el ID de mensaje. | **IDs de 16 bits con instrucciones atómicas RISC-V**, pool fijo de reensamblados anti-OOM y cola RF con priorización (QoS). |
| **Dual Core** | Ninguna (código corre sin afinidad explícita). | **Core Pinning Formal**: Core 0 dedicado a I/O, audio y radio; Core 1 dedicado exclusivamente a renderizado LVGL 9.5 y máquina virtual Lua. |
| **Manejo de Errores** | Mención vaga a templates `Result<T>`. | **`cbdos::Result<T>` y `ScopedFile` RAII livianos**: Cero overhead de RTTI, sin excepciones C++ y garantía de liberación de descriptores de archivos. |

---

*Borrador de ingeniería integral formulado para evaluación y aprobación.*
