# Plan Técnico: Control Dual de Reinicio (Normal RST vs Modo DFU/Bootloader) en Terminal Serial

**Fecha:** 4 de Septiembre de 2026  
**Autor:** Antigravity AI & kaber420  
**Target:** ESP32-P4 (JC4880P443C) y ESP32-S3 (JC3248W535)  
**Ubicación:** `docs/drafts/PLAN_CONTROL_REBOOT_Y_DFU_TERMINAL_SERIAL.md`  
**Estado:** Propuesta Técnica Aprobada para Futura Implementación  

---

## 1. Contexto y Objetivos

Durante la sesión de depuración en caliente, se logró la recepción en vivo de datos en el **Terminal Serial** (`SerialTerminalView`) desde un **ESP32-S3** conectado al puerto USB OTG del **ESP32-P4**. 

Al activar la señal de reset actual mediante `pulseControlPin()`, el microcontrolador conectado respondió con su banner de ROM:
```text
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x0 (DOWNLOAD(USB/UART0))
waiting for download
```

### Hallazgo Físico sobre DTR y RTS:
En el protocolo estándar USB CDC-ACM (y circuitos de auto-programación ESP32 / esptool / USB-Serial-JTAG):
1. **RTS** está enlazado a la línea de reset físico (**EN / CHIP_PU**).
2. **DTR** está enlazado a la línea de selección de arranque (**GPIO0 / BOOT**).

* **Comportamiento Actual:** El método `pulseControlPin` enviaba `DTR=false, RTS=true`. Al tener `DTR=false` (LOW) durante el pulso de reset, el microcontrolador entra forzosamente en **Modo Bootloader / Descarga de ROM** (`waiting for download`).
* **Objetivo de la Mejora:** Proporcionar al usuario en la barra de herramientas del Terminal Serial dos acciones dedicadas y diferenciadas:
  - **`[ ⚡ RST ]` (Reinicio Normal / Run Mode):** Reinicia el chip conectado y permite que ejecute de inmediato su aplicación / firmware de usuario.
  - **`[ 📥 DFU ]` (Reinicio a Bootloader / Download Mode):** Fuerza al chip a entrar en modo descarga de ROM (`waiting for download`), preparándolo para flasheo de firmware o inspección de memoria.

---

## 2. Diseño de Arquitectura y Contratos HAL (`core/`)

Para mantener la **Ley de Pureza Arquitectónica de `core/`** y la compatibilidad con todos los targets (P4 y S3):

### A. Extensión del Contrato `ISerialPort` ([`core/include/cbdos/serial.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/serial.hpp))

Se extiende el método de control de reset para aceptar el parámetro opcional `enterBootloader`:

```cpp
class ISerialPort {
public:
    // ... métodos existentes ...

    /**
     * @brief Reinicia el hardware externo conectado.
     * @param durationMs Duración del pulso de reset en milisegundos.
     * @param enterBootloader Si es true, fuerza al chip a entrar en modo ROM Bootloader/DFU.
     *                        Si es false (por defecto), realiza un reinicio normal a la aplicación.
     */
    virtual bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false) = 0;
};
```

### B. Funciones de Conveniencia en la API de Sistema (`cbdos::serial`)

```cpp
namespace cbdos::serial {
    bool pulseControlPin(uint32_t durationMs = 100, bool enterBootloader = false);
    
    // Wrappers semánticos explícitos:
    inline bool resetTarget() { return pulseControlPin(100, false); }
    inline bool enterBootloader() { return pulseControlPin(100, true); }
}
```

---

## 3. Implementación en los Backends (`bsp/`)

### A. Driver USB OTG en ESP32-P4 ([`bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp))

En la clase `P4UsbOtgPort`:

```cpp
bool pulseControlPin(uint32_t durationMs, bool enterBootloader) override {
    if (!m_isOpen || !m_cdcDev) return false;

    if (enterBootloader) {
        // Modo DFU: DTR=false (GPIO0=0), RTS=true (Reset activo)
        cdc_acm_host_set_control_line_state(m_cdcDev, false, true);
        vTaskDelay(pdMS_TO_TICKS(durationMs));
        // Reset liberado manteniendo GPIO0 en LOW momentáneamente
        cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
        vTaskDelay(pdMS_TO_TICKS(20));
        // Restaurar DTR=true para abrir canal de comunicación con el bootloader ROM
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
        ESP_LOGI(TAG_SERIAL, "Dispositivo puesto en Modo Bootloader / DFU");
    } else {
        // Modo Normal: DTR=true (GPIO0=1), RTS=true (Reset activo)
        cdc_acm_host_set_control_line_state(m_cdcDev, true, true);
        vTaskDelay(pdMS_TO_TICKS(durationMs));
        // Reset liberado con GPIO0 en HIGH -> Ejecuta firmware de usuario
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
        ESP_LOGI(TAG_SERIAL, "Reinicio normal enviado por USB OTG (Run Mode)");
    }
    return true;
}
```

### B. Driver UART GPIO / JP1 ([`hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp) y [`hal_uart_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_uart_s3.cpp))

Para puertos UART físicos (como el conector JP1 con GPIO 34 asignado a Reset):
- `pulseControlPin` conmuta el pin de reset a nivel LOW por `durationMs` y lo devuelve a HIGH.

---

## 4. Diseño de la Interfaz Gráfica (`SerialTerminalView`)

### Top Toolbar (Fila Principal de Herramientas):
En [`core/src/ui/views/SerialTerminalView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/SerialTerminalView.cpp):

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ [Puerto ▼] [115200 ▼] [ ▶ Abrir ] [ ⚡ RST ] [ 📥 DFU ] [ 🗑️ ] [ 💾 SD ]   │
└─────────────────────────────────────────────────────────────────────────────┘
```

1. **Botón `[ ⚡ RST ]`:**
   - Color: Naranja (`lv_palette_main(LV_PALETTE_ORANGE)`).
   - Callback: Invoca `cbdos::serial::pulseControlPin(100, false)`.
   - Mensaje de log: `\n[⚡ RST] Reinicio normal enviado (Run Mode).\n`
2. **Botón `[ 📥 DFU ]`:**
   - Color: Morado / Púrpura (`lv_palette_main(LV_PALETTE_DEEP_PURPLE)`).
   - Callback: Invoca `cbdos::serial::pulseControlPin(100, true)`.
   - Mensaje de log: `\n[📥 DFU] Chip forzado a Modo Bootloader (waiting for download).\n`

---

## 5. Plan de Verificación Futuro

1. **Compilación Multi-Target:**
   - ESP32-P4: `idf.py build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
2. **Prueba de Reinicio Normal:**
   - Conectar ESP32-S3 por USB OTG al P4.
   - Presionar `[ ⚡ RST ]`.
   - Verificar en el Terminal que el S3 imprime los logs de su aplicación activa (ej. inicialización de CBDos S3 o MeshCore) en lugar de quedarse en el bootloader.
3. **Prueba de Modo DFU:**
   - Presionar `[ 📥 DFU ]`.
   - Verificar en el Terminal que el chip responde con `waiting for download`.
