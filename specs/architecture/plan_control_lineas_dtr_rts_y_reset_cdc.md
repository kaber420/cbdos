# Plan de Corrección de Control de Líneas DTR/RTS y Secuencias de Reset CDC

## 1. Contexto y Diagnóstico del Bug

### 1.1. Síntoma
Al abrir el puerto serie USB CDC en modo Terminal o invocar un reset normal (`resetTarget(false)`), los dispositivos basados en el periférico de silicio **ESP32 USB-Serial-JTAG** (ESP32-S3, ESP32-C3, ESP32-C6) arrancan inesperadamente en el bootloader de ROM imprimiendo:
```text
waiting for download
```
en lugar de reiniciar y ejecutar la aplicación del firmware usuario.

### 1.2. Causa Raíz
En el periférico de hardware **USB-Serial-JTAG** de Espressif:
- La línea **DTR** está cableada internamente al pin **`BOOT` (GPIO0)** (activo en nivel bajo cuando `DTR = true / 1`).
- La línea **RTS** está cableada internamente al pin **`RESET` (CHIP_PU)** (activo en nivel bajo cuando `RTS = true / 1`).

En [`bsp/esp32_p4_jc4880/hal/hal_usb_cdc_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_usb_cdc_p4.cpp):
1. Al abrir la terminal (`open()`), se ejecutaba:
   ```cpp
   cdc_acm_host_set_control_line_state(m_cdcDev, true, false); // DTR=1 -> BOOT permanentemente presionado (GPIO0=0)
   ```
2. Al ejecutar un reset normal (`resetTarget(false)`), la señal RTS generaba un pulso de reset mientras `DTR` continuaba activo o en un estado transitorio donde `GPIO0` permanecía en nivel bajo, haciendo que el chip sampleara `BOOT=0` al salir del reset y entrara en modo descarga de ROM (`waiting for download`).

---

## 2. Tabla de Verdad de Señales DTR/RTS

| Estado / Operación | DTR (`BOOT/GPIO0`) | RTS (`RESET/CHIP_PU`) | Efecto en ESP32 USB-Serial-JTAG |
|---|---|---|---|
| **Reposo / Terminal Activa** | `false` (0) | `false` (0) | Firmware en ejecución, BOOT liberado (GPIO0=1), EN=1 |
| **Reset Normal (Run)** | `false` (0) | `true` (1) $\rightarrow$ `false` (0) | Reinicia y ejecuta el firmware del target (GPIO0=1) |
| **Reset a Bootloader (DFU)** | `true` (1) | Secuencia 4 pasos con `RTS` | Entra a ROM Bootloader (`waiting for download`) |

---

## 3. Plan de Cambios Quirúrgicos

### 3.1. Corrección en `P4UsbCdcChannel::open()`
- Cambiar la inicialización de líneas de control para que `DTR=false` y `RTS=false`:
  ```cpp
  cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
  ```

### 3.2. Refactorización de `sendNativeJtagBootSequence()`
- **Para `enterBootloader == false` (Reset Normal):**
  Asegurar explícitamente `DTR=false` antes, durante y después del pulso de Reset:
  ```cpp
  cdc_acm_host_set_control_line_state(m_cdcDev, false, false);
  vTaskDelay(pdMS_TO_TICKS(50));
  cdc_acm_host_set_control_line_state(m_cdcDev, false, true); // Pulso EN=LOW
  vTaskDelay(pdMS_TO_TICKS(100));
  cdc_acm_host_set_control_line_state(m_cdcDev, false, false); // EN=HIGH, BOOT=HIGH -> Run Firmware
  vTaskDelay(pdMS_TO_TICKS(100));
  ```
- **Para `enterBootloader == true` (Flasher Bootloader):**
  Mantener la secuencia de 4 pasos de `esptool` asegurando la retención de `DTR=true` durante la liberación de reset.

### 3.3. Refactorización de `sendStandardBridgeBootSequence()`
- Para puentes CP210x, CH34x y FTDI externos:
  - Normal Reset: `DTR=false`, pulso en `RTS=true` $\rightarrow$ `RTS=false`.

---

## 4. Criterios de Aceptación y Validación

1. **Compilación Limpia:**
   - ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
2. **Validación en Hardware Real:**
   - Al pulsar "Reset" desde la aplicación Terminal, el target ESP32 se reinicia limpiamente y emite los logs normales de arranque de su aplicación (sin caer en `waiting for download`).
   - Al usar la aplicación Flasher para flashear un firmware, el target entra en bootloader correctamente y luego se reinicia a modo Run.
