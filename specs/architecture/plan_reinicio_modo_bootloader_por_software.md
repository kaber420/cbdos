# 🚀 Plan de Arquitectura: Reinicio en Modo Bootloader por Software (Zero Botones Físicos)

**Fecha:** Octubre 2026  
**Documento:** `specs/architecture/plan_reinicio_modo_bootloader_por_software.md`  
**Estado:** 📋 Especificación Técnica y Plan de Integración  
**Autor:** Desarrollador Senior de Sistemas Embebidos / Arquitectura CBDos  
**Objetivo:** Permitir reiniciar el sistema directamente al bootloader de descarga (ROM Download Mode) sin tocar los botones físicos `BOOT` ni `EN`, disponible desde la UI (`PowerConfigView`), mediante comando en la consola serie (`CBDOS:BOOTLOADER`), y garantizando que el toolchain (`platformio.ini`) limpie el registro de arranque tras el flasheo.

---

## 🏛️ 1. Justificación y Fundamentos Técnicos

### 1.1. El Problema de las Carcasas y el Acceso Físico
En hardware como la CyberDeck o terminales portátiles cerrados (JC3248W535 o JC4880P443C), los botones físicos `BOOT` y `RST` quedan dentro del chasis o en lugares inaccesibles. Si el usuario o desarrollador requiere actualizar el firmware, debe existir una vía 100% nativa por software para entrar al bootloader de fábrica.

### 1.2. Mecanismo de Hardware en el Silicio de Espressif
Los SoCs ESP32-S3 y ESP32-P4 implementan un registro de control en el dominio de bajo consumo (RTC / LP AON) que sobreescribe la lógica de muestreo de pines de strapping (GPIO0).
Cuando el bit `FORCE_DOWNLOAD_BOOT` está en nivel alto (`1`), la ROM de arranque ignora el estado de los pines físicos y entra inmediatamente al modo de descarga por USB/UART tras un reinicio de la CPU.

---

## ⚙️ 2. Especificación Técnica por Plataforma

### 2.1. ESP32-S3 (JC3248W535)
- **Registro:** `RTC_CNTL_OPTION1_REG`
- **Máscara:** `RTC_CNTL_FORCE_DOWNLOAD_BOOT` (bit 0)
- **Implementación en BSP (`hal_system_s3.cpp`):**
  ```cpp
  #include <soc/rtc_cntl_reg.h>
  #include <esp_system.h>

  namespace cbdos {
  namespace system {

  void restartToBootloader() {
      // Fuerza al bootloader de la ROM a entrar en modo descarga
      REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
      esp_restart();
  }

  } // namespace system
  } // namespace cbdos
  ```

### 2.2. ESP32-P4 (JC4880P443C)
- **Registro:** `LP_SYSTEM_REG_SYS_CTRL_REG` (LP System Domain)
- **Máscara:** `LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT`
- **Implementación en BSP (`hal_system_p4.cpp`):**
  ```cpp
  #include <soc/lp_system_reg.h>
  #include <esp_system.h>

  namespace cbdos {
  namespace system {

  void restartToBootloader() {
      // Fuerza al bootloader de la ROM del P4 a entrar en modo descarga
      REG_SET_BIT(LP_SYSTEM_REG_SYS_CTRL_REG, LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT);
      esp_restart();
  }

  } // namespace system
  } // namespace cbdos
  ```

---

## 🌐 3. Contrato de la API Core e Internacionalización

### 3.1. Extensión de `core/include/cbdos/system.hpp`
```cpp
namespace cbdos {
namespace system {

// Reinicio estándar de la aplicación
void restart();

// Reinicio forzado a la ROM de descarga (Bootloader Mode)
void restartToBootloader();

} // namespace system
} // namespace cbdos
```

### 3.2. Cadenas i18n (`language.hpp` y `language.cpp`)
Se reservan IDs al final del diccionario preservando la regla de no renumerar:
```cpp
    // Modo Bootloader
    STR_PWR_BTN_BOOTLOADER   = 0x00CB,
    STR_PWR_TOAST_BOOTLOADER = 0x00CC,

    // Total Count
    STR_COUNT = 0x00CD,
```

**Textos Asociados:**
| `StrId` | Español (ES) | Inglés (EN) |
|---|---|---|
| `STR_PWR_BTN_BOOTLOADER` | `"Modo Bootloader (Flasheo)"` | `"Bootloader Mode (Flasher)"` |
| `STR_PWR_TOAST_BOOTLOADER` | `"Reiniciando en modo Bootloader..."` | `"Rebooting into Bootloader mode..."` |

---

## 🎨 4. Integración en UI (`PowerConfigView`)

En `core/src/ui/views/PowerConfigView.cpp`:
- Añadir un nuevo botón de acción debajo de *"Reiniciar Sistema"*:
  - **Icono:** `LV_SYMBOL_DOWNLOAD`
  - **Color de acento:** Celeste (`#38BDF8` / `lv_color_hex(0x38bdf8)`)
  - **Texto:** `tr(StrId::STR_PWR_BTN_BOOTLOADER)`
  - **Callback (`bootloader_btn_cb`):**
    1. Muestra Toast: `tr(StrId::STR_PWR_TOAST_BOOTLOADER)`
    2. Programa reinicio diferido breve (500 ms) para asegurar que el toast y la pantalla alcancen a refrescarse antes de entregar la CPU a la ROM.
    3. Invoca `cbdos::system::restartToBootloader()`.

---

## 💻 5. Integración en Consola Serie / Flasheador Web

En el bucle de procesamiento de comandos serie (`main.cpp` en S3 y P4):
- Añadir el comando `CBDOS:BOOTLOADER`:
  ```cpp
  if (line == "CBDOS:BOOTLOADER") {
      console.println("OK: REBOOTING TO BOOTLOADER");
      delay(100);
      cbdos::system::restartToBootloader();
  }
  ```
- Esto permite que el flasheador web o herramientas en la PC manden la placa a modo descarga enviando una sola línea por serie.

---

## 🔧 6. Configuración de PlatformIO (`platformio.ini`)

En `bsp/esp32_s3_jc3248/platformio.ini`:
```ini
upload_flags =
    --before=usb_reset
    --after=hard_reset
```
*Justificación:* El flag `--after=hard_reset` en `esptool` para ESP32-S3 ejecuta la limpieza del registro `RTC_CNTL_FORCE_DOWNLOAD_BOOT`, asegurando que una vez escrita la flash, el chip regrese al modo normal de ejecución de la aplicación.

---

## 📋 7. Plan de Ejecución en 4 Fases

- [x] **Fase 1: Capa de Abstracción de Sistema e i18n**
  - Declarar `restartToBootloader()` en `cbdos/system.hpp`.
  - Implementar la función en `hal_system_s3.cpp` y `hal_system_p4.cpp`.
  - Añadir cadenas `STR_PWR_BTN_BOOTLOADER` y `STR_PWR_TOAST_BOOTLOADER` a `language.hpp` y `language.cpp`.
- [x] **Fase 2: Integración en la Interfaz Gráfica (`PowerConfigView`)**
  - Añadir botón de acción en `PowerConfigView.cpp` con estilo visual unificado y llamada a `restartToBootloader()`.
- [x] **Fase 3: Integración en Consola Serie y PlatformIO**
  - Añadir comando `CBDOS:BOOTLOADER` en el bucle serie de S3 (`main.cpp`) y en el CLI de P4 (`hal_hid_p4.cpp`).
  - Configurar `upload_flags` con `--after=hard_reset` en `platformio.ini`.
- [x] **Fase 4: Verificación Multi-Target y Validación**
  - Compilación limpia en ESP32-S3 (`pio run -d bsp/esp32_s3_jc3248`) y ESP32-P4 (`idf.py -C bsp/esp32_p4_jc4880 build`).
  - Comprobación en hardware: validar entrada limpia al bootloader sin presionar botones físicos.
