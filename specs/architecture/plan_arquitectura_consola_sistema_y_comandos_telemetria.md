# 🧩 Especificación Técnica y Plan: Arquitectura de Consola de Sistema Desacoplada y Telemetría On-Demand

**Fecha:** 2026-10-09  
**Estado:** Implementado y Validado (Dual-Target ESP32-P4 & ESP32-S3)  
**Versión:** `v0.2.4-dev`  
**Ubicación:** `specs/architecture/plan_arquitectura_consola_sistema_y_comandos_telemetria.md`  

---

## 🎯 1. Diagnóstico y Objetivos de Diseño

### 1.1 Estado Actual y Deuda Técnica
Actualmente en el BSP de ESP32-P4 (`bsp/esp32_p4_jc4880/hal/hal_hid_p4.cpp`):
1. **Acoplamiento Espurio:** La tarea de recepción serie interactiva (`serial_interactive_cli_task`) se encuentra alojada dentro del archivo del driver **USB HID (Teclado/Ratón BadUSB)** bajo la macro `#define ENABLE_CBDOS_SERIAL_DEBUG_CLI 1`.
2. **Confusión de Puertos:** Se mezcló conceptualmente el **Puerto USB 1 (USB-Serial/JTAG)** nativo del silicio con el **Puerto USB 2 (USB OTG High-Speed)** administrado por TinyUSB / USB Host.
3. **Ausencia de Comandos Estructurados de Sistema:** Para consultar la temperatura interna de la CPU, memoria libre o uptime por puerto serie, se dependía de llamar a comandos en bruto de Lua (`return cbdos.system.cpu_temp()`), sin existir comandos de consola formales del sistema operativo.

### 1.2 Principios Rectores (Invariables de CBDos)
* **Zero-Polling & Reactivo (Regla 4):** La consola no sondea periódicamente el hardware ni emite spam al monitor serie. La lectura de temperatura analógica del chip y las métricas de memoria se ejecutan estrictamente **bajo demanda (on-demand / Request-Response)**.
* **Separación de Responsabilidades:** El driver USB HID (`hal_hid_p4.cpp`) solo gestiona periféricos HID (teclado, ratón). La consola del sistema se traslada a su propio módulo desacoplado.
* **Motor de Comandos Agnóstico (Sin duplicación de código):** El despachador de comandos procesa cadenas de texto de forma abstracta (`input -> output`), permitiendo que el mismo comando (`status`, `info`, `temp`) funcione idénticamente a través de:
  - El puerto serie de depuración de la PC (`/dev/ttyACM0` - USB Serial CDC).
  - La aplicación táctil de pantalla (`TerminalView`).
  - Un segundo puerto serie (UART externa JP1 o USB CDC Host).
* **Compatibilidad Dual-Target (P4 y S3):** La lógica de comandos reside en `core/` y las lecturas de hardware se apoyan en la API estándar `cbdos::system::*`.

---

## 📐 2. Catálogo de Comandos del Sistema

> [!NOTE]
> La especificación formal de la gramática estilo AT/SCPI y el direccionamiento multidispotivo para Hubs USB se encuentra consagrada en [`especificacion_protocolo_consola_at_scpi_y_direccionamiento_hub.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/especificacion_protocolo_consola_at_scpi_y_direccionamiento_hub.md).

Los comandos se diseñan con una sintaxis jerárquica con prefijo `sys:` (estándar embebido) y con alias directos para conveniencia del operador:

| Comando | Alias Corto | Propósito | Formato de Salida |
| :--- | :--- | :--- | :--- |
| `sys: status` | `status` | Métricas dinámicas en tiempo real (Temp, RAM, PSRAM, Uptime) | Bloque legible con valores actuales |
| `sys: info` | `info` | Identidad estática del sistema (Versión, Board, SoC, Pantalla) | Bloque estático del hardware y SO |
| `sys: temp` | - | Temperatura aislada del SoC (ideal para scripts y transitorios) | `[SYS] CPU Temp: XX.X °C` |
| `sys: mem` | - | Resumen puntual de Heap y PSRAM libre | `[SYS] RAM: XX KB | PSRAM: XX.X MB` |
| `sys: uptime`| - | Tiempo transcurrido desde el arranque | `[SYS] Uptime: Xm Ys (Z s)` |

### 2.1 Modificadores y Flags Universales (`-c`, `-i`, `-t`)

Para diagnóstico avanzado, mediciones de estrés térmico y pruebas de enlace de radio (estilo router/Unix):

| Flag | Argumento | Descripción | Valores y Ejemplos | Default |
| :--- | :--- | :--- | :--- | :--- |
| `-c` | `<N>` | Cantidad de iteraciones/muestras consecutivas | `-c 5`, `-c 10`, `-c 20` | `1` (ejecuta y finaliza) |
| `-i` | `<intervalo>` | Intervalo entre muestras (admite `ms` y `s`) | `-i 50ms`, `-i 100ms`, `-i 1s`, `-i 2s` | `1s` (piso mín: `10ms`) |
| `-t` | `<timeout>` | Timeout máximo de espera por respuesta (en `ms` o `s`) | `-t 50ms`, `-t 200ms`, `-t 1s` | Depende del subsistema |

#### Reglas de Control y Seguridad Embebida:
* **Piso mínimo de seguridad de intervalo (`10ms`):** Evita asfixiar el planificador de FreeRTOS o desbordar colas de periféricos si el operador ingresa accidentalmente `-i 0`.
* **Cancelación Interactiva Inmediata:** Si el operador envía cualquier carácter (como `Ctrl+C`, `q` o `Enter`) durante una ráfaga de `-c N`, el bucle se aborta al instante y el control regresa de inmediato al prompt de la consola.
* **Cero Reserva Excesiva de Memoria (Streaming):** Cada iteración se emite directamente al canal de salida en tiempo real en lugar de acumular un bloque gigante de texto en RAM.

### 2.2 Formatos de Respuesta:

#### `sys: status` / `status` (Ejecución simple):
```text
[SYS_STATUS]
  CPU Temp:    43.1 °C
  RAM Libre:   245 KB / 512 KB
  PSRAM Libre: 29.4 MB / 32.0 MB
  Uptime:      18m 42s
```

#### `temp -c 5 -i 100ms` (Captura de transitorio térmico de 500 ms):
```text
[SYS] CPU Temp: 43.1 °C (iter 1/5)
[SYS] CPU Temp: 43.3 °C (iter 2/5)
[SYS] CPU Temp: 43.8 °C (iter 3/5)
[SYS] CPU Temp: 44.0 °C (iter 4/5)
[SYS] CPU Temp: 43.9 °C (iter 5/5)
--- Ráfaga completada (5 muestras, intervalo 100ms, min 43.1°C, max 44.0°C) ---
```

#### `c3: ping -c 4 -i 50ms -t 100ms` (Diagnóstico de enlace RF ESP-NOW):
```text
[RADIO_PING] seq=1 rtt=11.2ms rssi=-68dBm
[RADIO_PING] seq=2 rtt=10.9ms rssi=-67dBm
[RADIO_PING] seq=3 timeout (>100ms)
[RADIO_PING] seq=4 rtt=12.1ms rssi=-69dBm
--- Resumen RF: 4 enviados, 3 recibidos (25% pérdida), RTT prom: 11.4ms ---
```

#### `sys: info` / `info`:
```text
[SYS_INFO]
  Sistema:  CyBerDeck OS (CBDos) v0.2.4-dev
  Placa:    Guition JC4880P443C (jc4880p443)
  SoC:      ESP32-P4 RISC-V Dual-Core @ 400 MHz
  Pantalla: 480x800 MIPI-DPI @ 60 FPS
```

#### Comandos de Subsistemas Preexistentes (Preservados al 100%):
* `usb: status` / `usb: info`: Inspección del estado de periféricos en el puerto USB OTG.
* `c3: status` / `c3: probe`: Sondeo del módem de radio coprocesador C3.
* `c3: ping`: Emisión de trama de radio ESP-NOW al aire.
* `tts: <texto>`: Síntesis de voz offline por altavoz con PicoTTS.
* `CBDOS:BOOTLOADER`: Reinicio forzado a modo ROM bootloader.
* `CBDOS:VERSION?`: Query de identidad de placa para el flasheador web.
* `ducky: <script>`: Intérprete DuckyScript v2.
* *Fallback*: Cualquier otra línea es evaluada por el motor interactivo **Lua++**.

---

## 🏗️ 3. Arquitectura Modular Propuesta (Enmiendas Técnicas)

### 3.1 Componente Central Agnóstico: `CommandRegistry` (`core/`)
Ubicación: `core/include/cbdos/system_cli.hpp` y `core/src/system/system_cli.cpp`.

Para respetar la **pureza de `core/` (Regla 2)** y no crear dependencias inversas hacia drivers del BSP:
* `SystemCli` implementa un registro extensible de comandos con soporte de salida directa (streaming):
```cpp
namespace cbdos {
namespace cli {

// Emisor de salida por streaming (permite imprimir iteración a iteración sin alojar bloques gigantes)
using OutputWriter = std::function<void(const std::string& chunk)>;
using AbortCheck = std::function<bool()>; // Retorna true si el usuario presionó una tecla para cancelar

struct CommandContext {
    OutputWriter write;
    AbortCheck isAborted;
};

struct ExecutionOptions {
    uint32_t count = 1;        // Cantidad de repeticiones (-c)
    uint32_t intervalMs = 1000;// Intervalo entre repeticiones (-i) en ms (mínimo 10 ms)
    uint32_t timeoutMs = 500;  // Timeout de respuesta (-t) en ms
};

using CommandHandler = std::function<void(const std::string& args, const CommandContext& ctx)>;

void registerCommand(const std::string& prefix, CommandHandler handler);
void dispatch(const std::string& line, const CommandContext& ctx);
ExecutionOptions parseOptions(const std::string& args);

} // namespace cli
} // namespace cbdos
```

* **Comandos Base registrados en Core:**
  - `sys: status`, `status`, `sys: info`, `info`
  - `sys: temp`, `sys: mem`, `sys: uptime` (todos soportan flags `-c`, `-i`)
  - `CBDOS:VERSION?` (identidad directa parseable por WebFlasher)
  - `CBDOS:BOOTLOADER` (reinicio inmediato a bootloader)
  - *Fallback:* Cualquier línea no registrada se pasa a `LuaEngine::getInstance().executeString()`.

* **Comandos Específicos registrados por los BSPs:**
  - `bsp/esp32_p4_jc4880` registra: `usb: status`, `c3: status`, `c3: ping` (soporta `-c`, `-i`, `-t`), `tts:`, `ducky:`.

### 3.2 Precedencia Estricta de Despacho
1. **Comandos Críticos WebFlasher:** `CBDOS:VERSION?` y `CBDOS:BOOTLOADER` (evaluados primero sin latencia ni adornos).
2. **Comandos Jerárquicos con Prefijo:** `sys:`, `usb:`, `c3:`, `tts:`, `ducky:`.
3. **Alias Directos de Sistema:** `status`, `info`, `temp`, `mem`, `uptime`.
4. **Evaluador Lua++:** Cualquier otra expresión se envía a Lua.

### 3.3 Transporte Dual-Target Reactivo (Zero-Polling Estricto)
* **ESP32-P4 (`hal_console_p4.cpp`):**
  - Tarea `serial_console_task` en `/dev/ttyACM0` (USB-Serial-JTAG).
  - Bloqueo puro por interrupciones: `usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY)`. Cero uso de CPU (0%) cuando no hay pulsaciones del usuario.
  - AbortCheck integrado: verifica en tiempo real si el periférico USB-Serial tiene bytes pendientes para cancelar bucles `-c N` inmediatamente.
  - Control de producción con la macro `ENABLE_CBDOS_SYSTEM_CONSOLE` (sustituye a la antigua macro de HID).
* **ESP32-S3 (`hal_console_s3.cpp` / `main.cpp`):**
  - Integración en el canal serial del S3 alimentando el mismo `cbdos::cli::dispatch(line, ctx)`.
  - Asegura paridad 100% de comandos de telemetría y diagnósticos entre ambas plataformas.

---

## 📋 4. Plan de Implementación Paso a Paso

### Fase 1: Creación del `SystemCli` Agnóstico en Core
1. Crear `core/include/cbdos/system_cli.hpp` con la interfaz de registro, streaming (`OutputWriter`), cancelación (`AbortCheck`) y parser de flags `-c`, `-i`, `-t`.
2. Crear `core/src/system/system_cli.cpp` implementando los comandos del sistema (`status`, `info`, `temp`, `mem`, `uptime`, `VERSION?`, `BOOTLOADER`) con ráfagas controladas y fallback de Lua.
3. Actualizar `core/CMakeLists.txt` para compilar `system_cli.cpp`.

### Fase 2: Extracción y Limpieza de `hal_hid_p4.cpp`
1. Remover `serial_interactive_cli_task` de `bsp/esp32_p4_jc4880/hal/hal_hid_p4.cpp`.
2. Restaurar `hal_hid_p4.cpp` como driver puro y exclusivo de TinyUSB Device HID (teclado/ratón).

### Fase 3: Implementación del Transporte Serie en P4 (`hal_console_p4.cpp`)
1. Crear `bsp/esp32_p4_jc4880/hal/hal_console_p4.cpp` con lectura no-sondeada (`portMAX_DELAY`) y soporte de buffer seguro con backspace (`\b` / `DEL`).
2. Registrar en `initSystemConsoleP4()` los comandos de hardware específicos de P4 (`usb:`, `c3:`, `tts:`, `ducky:`), incluyendo ráfagas con timeout en `c3: ping`.
3. Invocar `cbdos::bsp::initSystemConsoleP4()` desde `bsp/esp32_p4_jc4880/main/main.cpp`.
4. Configurar la macro `ENABLE_CBDOS_SYSTEM_CONSOLE 1` (default en desarrollo).

### Fase 4: Integración en S3 (Dual-Target Parity)
1. Integrar `cbdos::cli::dispatch()` en la recepción serie de `bsp/esp32_s3_jc3248`.
2. Verificar compilación limpia dual-target en ESP-IDF (P4) y PlatformIO (S3).

### Fase 5: Validación Técnica
1. Compilar P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`.
2. Compilar S3: `pio run -d bsp/esp32_s3_jc3248`.
3. Probar en caliente en `/dev/ttyACM0`:
   - `status` -> verifica métricas completas individuales.
   - `temp -c 5 -i 100ms` -> verifica ráfaga de telemetría a 100 ms sin bloqueo.
   - `c3: ping -c 4 -i 50ms -t 100ms` -> verifica ráfaga de paquetes RF con timeout.
   - Cancelación en caliente: enviar `status -c 50` y pulsar una tecla para verificar que aborta de inmediato.
   - `return 10 * 5` -> verifica que Lua responde `50`.
   - `CBDOS:VERSION?` -> verifica respuesta estándar del WebFlasher.

---

## 🛡️ 5. Criterios de Éxito y Control de Riesgos
1. **Zero-Polling Real:** Cero sondeo con `portMAX_DELAY`.
2. **Pureza Arquitectónica:** `core/` no tiene dependencias hacia drivers de P4.
3. **Preservación Invariante:** BadUSB/HID intacto, WebFlasher intacto, y compatibilidad dual-target P4/S3.
