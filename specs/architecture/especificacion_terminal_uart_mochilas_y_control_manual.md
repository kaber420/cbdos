# 📟 Especificación Técnica: Subsistema Serial de CBDos, Control de Mochilas y Gestión Dinámica de Puertos

**Versión:** 2.1.0 (RFC-CBDOS-SERIAL-SYSTEM)  
**Estado:** Aprobado / Especificación de Arquitectura  
**Target:** ESP32-P4 (JC4880P443C) / ESP32-S3 (JC3248W535)  
**Módulos Afectados:** `core/src/ui/views/SerialTerminalView.*`, `core/include/cbdos/serial.hpp`, `bsp/esp32_p4_jc4880/hal/*`, `bsp/esp32_s3_jc3248/hal/*`

---

## 📌 1. Motivación y Diagnóstico de Fallas Previas

La implementación anterior de la aplicación `SerialTerminalView` y el backend UART presentaba limitaciones estructurales que no correspondían a un sistema operativo moderno:

1. **Auto-inicialización a Ciegas (`Blind Auto-Init`):**
   - Al entrar a la aplicación, se ejecutaba de forma automática la inicialización del hardware en pines no verificados y se iniciaba un temporizador de sondeo periódico (`lv_timer`) a 25 ms sin autorización explícita del usuario, desperdiciando CPU y energía.
2. **Presets Artificiales y Opciones Fantasma:**
   - Se mostraban opciones arbitrarias como `"JP1 Alt 1 (TX:50 RX:49)"` o puertos USB fijos en pantalla incluso cuando no había ningún periférico conectado.
3. **Líneas Flotantes y Ruido Parásito:**
   - La falta de pull-up interno en RX provocaba que, al abrir la UART sin carga conectada, el pin captara ruido ambiental, saturando el buffer con ráfagas infinitas de bytes corruptos y *framing errors*.
4. **Colapso del Renderizador de LVGL 9:**
   - Se volcaba el buffer completo mediante `lv_textarea_set_text()`, forzando a LVGL a recalcular layout de kilobytes de texto decenas de veces por segundo. Además, bytes binarios corruptos bloqueaban el decodificador de fuentes tipográficas.
5. **Falta de Abstracción Multi-Puerto:**
   - No se trataba al puerto USB y a los puertos UART físicos bajo una misma abstracción unificada de flujo serie bidireccional (*Stream*), impidiendo la alternancia limpia entre ellos.
6. **Reinicio Indiscriminado a Bootloader (Falta de Control Dual):**
   - Los pulsos de reinicio enviados sin diferenciar estados de líneas de control dejaban permanentemente a microcontroladores remotos atrapados en la ROM de descarga (`waiting for download`) en lugar de arrancar su aplicación de usuario.
7. **Incompatibilidad de Fin de Línea y Eco Local:**
   - Forzar `\r\n` y eco de transmisión fijo en todo momento producía errores de sintaxis en shells Linux/BusyBox y duplicaba caracteres en consolas interactivas.

---

## 🎯 2. Principios de Diseño del Subsistema Serial

```
                             ┌─────────────────────────────────────────────────────────┐
                             │           SerialTerminalView (UI LVGL 9.5)              │
                             │  - Selector de Puerto Dinámico (Solo puertos reales)    │
                             │  - Selector Baudrate: [9600 ... 115200 ... 921600]      │
                             │  - Botón Conectar / Desconectar (Control manual)        │
                             │  - Control Dual: [ ⚡ RST ] (Run) / [ 📥 DFU ] (ROM)     │
                             │  - Control de Flujo: [ ⏸️ Hold ] (Pausa Scroll)         │
                             │  - Selector Fin de Línea: [CRLF | LF | CR | None]       │
                             │  - Toggle Eco Local: [ Echo: ON / OFF ]                 |
                             └────────────────────────────┬────────────────────────────┘
                                                          │ Control Explícito
                                                          ▼
                             ┌─────────────────────────────────────────────────────────┐
                             │                 ISerialPort (Core HAL)                  │
                             │  - open(config) / close()                               │
                             │  - read(buf, len) / write(buf, len)                     │
                             │  - pulseControlPin(durationMs, enterBootloader)         │
                             │  - setControlPin(level)                                 │
                             └────────────────────────────┬────────────────────────────┘
                                                          │
                                     ┌────────────────────┴────────────────────┐
                                     ▼                                         ▼
                 ┌───────────────────────────────────────┐ ┌───────────────────────────────────────┐
                 │          UartHardwarePort             │ │            UsbCdcPort                 │
                 │   - ESP32-P4: TX:32, RX:28 (JP1)      │ │   - Detección Hotplug por hardware    │
                 │   - Reset: GPIO 54 / Boot: GPIO 34    │ │   - RST: RTS / BOOT: DTR              │
                 │   - ESP32-S3: TX:15, RX:16 (Ext)      │ │   - Aparece SOLO si está enchufado    │
                 │   - Pull-up interno en RX             │ │   - Se retira al desconectar          │
                 │   - Modo Manual (pines libres)        │ │   - RingBuffer DMA en background      │
                 └───────────────────────────────────────┘ └───────────────────────────────────────┘
```

### 2.1. Cero Auto-Arranque y Control Manual Obligatorio
- **Reposo Inicial:** Al abrir la vista `SerialTerminalView`, ningún periférico se reserva, polariza ni inicializa.
- **Acción Explícita:** La conexión se establece **únicamente** cuando el usuario selecciona los parámetros y presiona **"Conectar"**.
- **Desconexión Limpia:** Al presionar **"Desconectar"** (o al salir de la aplicación), se ejecuta inmediatamente `close()`, liberando los GPIOs del chip y deteniendo cualquier recepción de fondo para evitar consumo parásito.

### 2.2. Detección Dinámica por Hardware (Hotplug USB CDC Real)
- **Cero puertos USB fantasma:** El puerto USB CDC **NO está hardcodeado** en la lista si no hay nada conectado.
- **Conexión en Caliente:** Cuando el usuario enchufa un módem o microcontrolador al conector USB Host (USB 2), el stack hardware emite un evento nativo (`CDC_CONNECTED`). En ese instante, `USB CDC` aparece disponible en el selector de puertos.
- **Desconexión en Caliente:** Si el dispositivo USB se desconecta, el stack genera el evento de desconexión, la terminal cierra el canal automáticamente y retira la opción de la interfaz.

### 2.3. Puertos Físicos y Pines Reales Soportados (Alcance Focalizado)

El sistema se enfoca estrictamente en los puertos probados y estables:

#### Target ESP32-P4 (JC4880P443C)
| Puerto en UI | TX Pin | RX Pin | Pin Reset (RST) | Pin Boot (DFU) | Comportamiento |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **UART JP1 (Mochila / C6)** | **GPIO 32** | **GPIO 28** | **GPIO 54** | **GPIO 34** | Siempre disponible. Hardware probado. Pull-up en RX. |
| **USB CDC** | USB D+ / D- | USB D+ / D- | Línea RTS | Línea DTR | **Dinámico:** Solo visible si hay dispositivo CDC conectado. |
| **Manual / Custom** | Configurable | Configurable | Opcional | Opcional | Permite ingresar pines TX/RX libres reales. |

#### Target ESP32-S3 (JC3248W535)
| Puerto en UI | TX Pin | RX Pin | Pin Control Aux | Comportamiento |
| :--- | :--- | :--- | :--- | :--- |
| **UART Externa** | **GPIO 15** | **GPIO 16** | N/A | Siempre disponible. Hardware probado. |
| **USB CDC** | USB D+ / D- | USB D+ / D- | N/A | **Dinámico:** Solo visible si hay dispositivo CDC conectado. |
| **Manual / Custom** | Configurable | Configurable | N/A | Permite seleccionar pines libres del S3. |

### 2.4. Control Dual de Hardware y Mochilas (Reinicio Normal vs Modo DFU/Bootloader)
Para interactuar con microcontroladores externos y mochilas inteligentes (ESP32, RP2040, STM32), se proporcionan dos acciones de reinicio diferenciadas:

1. **Reinicio Normal (`[ ⚡ RST ]` - Run Mode):**
   - **En USB CDC-ACM:** Fija `DTR=true` (GPIO0 en HIGH) y genera pulso en `RTS=true` (Reset LOW por 100 ms) seguido de `RTS=false`. El microcontrolador arranca de inmediato su aplicación o firmware de usuario.
   - **En UART JP1 (P4):** Mantiene GPIO 34 (BOOT) en nivel HIGH y conmuta GPIO 54 (RST) a LOW durante 100 ms.
2. **Reinicio a Bootloader (`[ 📥 DFU ]` - Download/ROM Mode):**
   - **En USB CDC-ACM:** Fija `DTR=false` (GPIO0 en LOW), activa `RTS=true` (Reset) por 100 ms, libera `RTS=false` y restaura `DTR=true`. El microcontrolador entra forzosamente en modo descarga de ROM (`waiting for download`), listo para flashear.
   - **En UART JP1 (P4):** Fija GPIO 34 (BOOT) en nivel LOW, conmuta GPIO 54 (RST) a LOW por 100 ms y libera las líneas.

### 2.5. Blindaje de Hardware (Pull-Up en RX)
- Al abrir cualquier puerto UART sobre un pin RX físico, el driver activa la resistencia interna de pull-up (`gpio_pullup_en(rxPin)`).
- Esto mantiene la línea en nivel lógico alto (`IDLE`) en caso de que la mochila conectada esté apagada o en reposo, erradicando *framing errors* y tormentas de bytes parásitos.

### 2.6. Principio Reactivo (Cero Polling Innecesario - Regla 12)
- El backend serie opera mediante eventos de recepción (interrupción/cola FreeRTOS o callback de recepción).
- La UI se actualiza únicamente cuando existen nuevos datos por renderizar (`m_hasNewData = true`), manteniendo el procesador en reposo durante periodos de silencio.

### 2.7. Renderizado Incremental, Filtro Limpio de Códigos ANSI y Modo HEX
- **Inserción Rápida:** Se utiliza exclusivamente `lv_textarea_add_text()`, insertando fragmentos al final del buffer visual sin recalcular todo el layout.
- **Stripper de Secuencias de Escape ANSI / VT100:**
  - Se procesan y remueven las secuencias de escape completas de color o control de cursor (`\x1b[...m`, `\x1b[?25h`, etc.) para evitar que los logs formateados de ESP-IDF o Linux muestren basura como `[0;32m` o `[0m` en pantalla.
- **Sanitización de Caracteres:**
  - Caracteres ASCII válidos (`0x20` a `0x7E`), tabuladores (`\t`) y saltos de línea (`\r`, `\n`) se renderizan directamente.
  - Caracteres binarios o secuencias UTF-8 truncadas se formatean de forma segura (ej. `\xNN` o vista HEX) para evitar corrupciones en el motor tipográfico de LVGL.

### 2.8. Protocolo de Transmisión: Fin de Línea y Eco Local
- **Selector de Fin de Línea (Line Ending):**
  - Permite seleccionar el terminador anexado al presionar Enviar:
    - **`CRLF (\r\n)`:** Estándar para comandos AT, módems celulares y microcontroladores ESP.
    - **`LF (\n)`:** Requerido para consolas Linux, shells BusyBox y Raspberry Pi (evita el error `syntax error: unexpected "\r"`).
    - **`CR (\r)`:** Consolas VT100 tradicionales y REPLs antiguos.
    - **`None`:** Envío del texto puro sin caracteres adicionales (ideal para comandos interactivos y macros).
- **Toggle de Eco Local (Local Echo ON / OFF):**
  - **OFF (por defecto para consolas interactivas):** La terminal muestra únicamente lo que el dispositivo remoto responde, evitando duplicar caracteres en shells que ya tienen eco propio.
  - **ON (para comandos AT):** Muestra `[TX] > comando` en la ventana local, permitiendo verificar lo escrito ante dispositivos silenciosos.

### 2.9. Mecanismo de Pausa y Auto-Scroll (Hold)
- **Botón `[ ⏸️ Hold ]`:**
  - Congela temporalmente el auto-scroll hacia el final (`LV_TEXTAREA_CURSOR_LAST`) y suspende la actualización gráfica de la pantalla.
  - El backend y el RingBuffer de recepción continúan capturando datos en segundo plano sin pérdida.
  - Permite al usuario seleccionar, examinar o copiar trazas de error en medio de ráfagas continuas de logs sin que el texto salte de posición.
  - Al desactivar Hold, la pantalla se actualiza con el texto acumulado y retoma el seguimiento en tiempo real.

---

## 📋 3. Especificación de la Interfaz de Usuario (UI)

### 3.1. Barra de Herramientas Superior (Top Toolbar)
```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ [Puerto ▼] [115200 ▼] [ ▶ Abrir ] [ ⚡ RST ] [ 📥 DFU ] [ ⏸️ Hold ] [ 🗑️ ] [ 💾 SD ]     │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```
1. **Selector de Puerto (Dropdown Dinámico):** Muestra exclusivamente puertos reales disponibles (`UART JP1`, `USB CDC` si está conectado, o `Manual`).
2. **Selector de Baudrate:** `9600`, `19200`, `38400`, `57600`, `115200`, `230400`, `460800`, `921600`.
3. **Botón Conectar / Desconectar:** Verde (`▶ Abrir`) en reposo / Rojo (`⏹ Cerrar`) al estar conectado.
4. **Botón `[ ⚡ RST ]`:** Genera reinicio en modo normal de ejecución (Run Mode).
5. **Botón `[ 📥 DFU ]`:** Fuerza al dispositivo a entrar en modo bootloader de ROM.
6. **Botón `[ ⏸️ Hold ]`:** Pausa/reanuda la actualización visual del terminal.
7. **Botón `[ 🗑️ Limpiar ]`:** Vacía el contenido de la pantalla y reinicia el búfer local.
8. **Botón `[ 💾 Guardar en SD ]`:** Guarda el búfer capturado en `/sdcard/serial_log_TIMESTAMP.txt`.

### 3.2. Barra de Comandos y Protocolo (Command & Protocol Bar)
```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ [ CRLF ▼ ] [ Echo: OFF ] | [ ENTER ] [ ^C ] [ ^Z ] [ SPACE ] [ TAB ]                     │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ [ Área de Entrada: Comando...                          ] [ ▶ Enviar ] [ ⌨️ Teclado ]     │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```
1. **Selector Fin de Línea:** Dropdown con opciones `CRLF`, `LF`, `CR`, `None`.
2. **Toggle Eco Local:** Botón alternador `Echo: OFF` (gris) / `Echo: ON` (azul resaltado).
3. **Botones Rápidos de Control:** Envío inmediato de teclas especiales de terminal (`ENTER`, `^C`, `^Z`, `SPACE`, `TAB`).
4. **Campo de Entrada de Texto:** Textarea interactivo con soporte de foco y teclado virtual LVGL.

---

## 💻 4. Contratos HAL (Core C++ Puro)

### 4.1. Descriptores y Contratos (`core/include/cbdos/serial.hpp`)
```cpp
namespace cbdos {
namespace serial {

enum class PortType {
    HardwareUart,
    UsbCdcAcm,
    ManualUart
};

enum class LineEnding {
    CRLF,   // \r\n
    LF,     // \n
    CR,     // \r
    NONE    // sin terminador adicional
};

struct SerialPortDescriptor {
    std::string id;            // "jp1", "usb_otg", "ext_s3", "manual"
    std::string displayName;   // "UART JP1 (TX:32 RX:28)", "USB CDC-ACM"
    PortType type;
    int defaultTx;             // -1 para USB
    int defaultRx;             // -1 para USB
    int controlPin;            // 34/54 en JP1 P4, -1 si no aplica
    bool isAvailable;          // true para UART, dinámico para USB
};

struct SerialConfig {
    std::string portId;
    uint32_t baudrate = 115200;
    int txPin = -1;
    int rxPin = -1;
    int controlPin = -1;
};

class ISerialPort {
public:
    virtual ~ISerialPort() = default;
    virtual bool open(const SerialConfig& config) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual size_t available() = 0;
    virtual size_t read(uint8_t* buffer, size_t maxLen) = 0;
    virtual std::string readString(size_t maxLen = 1024) = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual size_t writeString(const std::string& str) = 0;
    virtual void flush() = 0;
    virtual bool setBaudrate(uint32_t baudrate) = 0;
    
    // Control de líneas auxiliares y reinicio dual
    virtual bool setControlPin(bool level) = 0;
    virtual bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false) = 0;
};

class ISerialBackend {
public:
    virtual ~ISerialBackend() = default;
    virtual std::vector<SerialPortDescriptor> getAvailablePorts() = 0;
    virtual ISerialPort* getPort(const std::string& portId) = 0;
    virtual void setHotplugCallback(std::function<void(bool connected, const std::string& portId)> cb) = 0;
};

// APIs de conveniencia en Core
std::vector<SerialPortDescriptor> getAvailablePorts();
bool open(const SerialConfig& config);
void close();
bool isOpen();
size_t available();
size_t read(uint8_t* buffer, size_t maxLen);
std::string readString(size_t maxLen = 1024);
size_t write(const uint8_t* data, size_t len);
size_t writeString(const std::string& str);
bool setBaudrate(uint32_t baudrate);
bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false);

inline bool resetTarget() { return pulseControlPin(100, false); }
inline bool enterBootloader() { return pulseControlPin(100, true); }

} // namespace serial
} // namespace cbdos
```

---

## 🛠️ 5. Plan de Implementación por Fases

1. **Fase 1: Extensión de Contratos en Core (`core/include/cbdos/serial.hpp`)**
   - Incorporar el parámetro `enterBootloader` en `pulseControlPin()`.
   - Definir helpers `resetTarget()` y `enterBootloader()`.

2. **Fase 2: Backend Hardware ESP32-P4 (`bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`)**
   - En `P4UsbOtgPort`: Implementar control dual DTR/RTS en `pulseControlPin()` para alternar limpiamente entre Run Mode (`DTR=1`) y Bootloader Mode (`DTR=0`).
   - En `P4SerialPort`: Conectar señales de reinicio normal y bootloader para JP1.

3. **Fase 3: Refinamiento de la Interfaz (`SerialTerminalView`)**
   - Añadir botones dedicados `[ ⚡ RST ]` y `[ 📥 DFU ]` en la barra superior.
   - Implementar botón `[ ⏸️ Hold ]` para congelar el auto-scroll de la pantalla.
   - Integrar selector de fin de línea (`CRLF`, `LF`, `CR`, `None`) y toggle de `Echo: ON/OFF`.
   - Incorporar el stripper de secuencias de escape ANSI para eliminar basura tipográfica (`\x1b[...m`).

4. **Fase 4: Verificación y Validación Multi-Target**
   - Verificar compilación limpia en ESP32-P4 (`idf.py build`) y ESP32-S3 (`pio run`).
   - Validar conmutación entre Run Mode y DFU en hardware real.
   - Probar comandos con shells Linux y módems AT verificando la ausencia de duplicados y terminaciones limpias.
