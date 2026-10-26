# Plan Técnico: Corrección del Reinicio a Modo Bootloader Nativo USB (ESP32-S3)

**Documento:** `specs/architecture/plan_reparacion_reinicio_bootloader_usb_s3.md`  
**Categoría:** Arquitectura / Hardware & BSP  
**Estado:** 📋 Especificación y Plan Técnico  
**Fecha:** 25 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Dispositivo:** Guition JC3248W535 (ESP32-S3 USB Nativo GPIO19/20)

---

## 1. Diagnóstico del Problema y Causa Raíz

En la placa **JC3248W535**, el conector USB-C se conecta **directamente** a los pines físicos de datos `GPIO19` (D-) y `GPIO20` (D+) del chip ESP32-S3 (periférico USB-Serial-JTAG / USB OTG interno). No existe ningún chip puente externo (como CH340 o CP2102).

### Por qué fallaba la implementación actual:
Actualmente, `hal_system_s3.cpp` implementa:
```cpp
void restartToBootloader() {
    REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    ESP.restart();
}
```
1. `ESP.restart()` (`esp_restart()`) realiza un reinicio por software del núcleo CPU.
2. Sin embargo, **no restablece eléctricamente el transceptor (PHY) USB nativo** ni notifica al controlador del host (Linux).
3. Linux nunca ve un evento de desconexión física del bus (`disconnect event`).
4. Cuando la ROM de arranque toma el control tras el reinicio, el endpoint USB queda en un estado no sincronizado o desfasado con el driver `cdc_acm` del kernel de Linux.
5. Resultado: `esptool` envía paquetes a `/dev/ttyACM0` y la ROM no responde, arrojando el error `No serial data received`.

---

## 2. Solución de Silicio: Persistencia y Desconexión de Bus en ROM

Espressif diseñó mecanismos específicos en la ROM del ESP32-S3 para gestionar el reinicio hacia el bootloader por USB nativo sin necesidad de tocar botones físicos (`BOOT` / `EN`):

### 2.1. Desconexión Eléctrica del Bus USB según el Modo Activo (Consola CDC vs Teclado HID)
Antes de reiniciar, el firmware debe cerrar el stack USB activo y desconectar intencionalmente las resistencias pull-up del puerto USB nativo (`GPIO19/20`). Esto genera un pulso de desconexión física (`disconnect event`) en el host Linux, obligando al kernel a destruir `/dev/ttyACM0` de la sesión anterior:
- **Si el sistema está en Modo Consola (TinyUSB CDC ACM):** Invocar `USBSerial.flush()`, `USB.enable(false)` o apagar el transceptor DWC2/OTG para cortar la sesión CDC limpiamente.
- **Si el sistema está en Modo Teclado (HID / FIDO):** Ejecutar `tud_disconnect()` para forzar la caída del bus.
- **Universal:** Desvincular el PHY USB de los descriptores de la aplicación para que el host no intente reutilizar una interfaz colgada.

### 2.2. Bandera de Persistencia de ROM (`USBDC_PERSIST_FLAG_BOOTLOADER`)
El archivo de cabecera de la ROM de Espressif (`esp32s3/rom/usb/usb_persist.h` y `chip_usb_dwc2.h`) provee la API de bajo nivel para garantizar que la ROM entre en modo bootloader real:
```cpp
#include "esp32s3/rom/usb/usb_persist.h"
#include "esp32s3/rom/usb/chip_usb_dwc2.h"
#include <soc/rtc_cntl_reg.h>

void restartToBootloader() {
    // 1. Desconectar el stack USB activo (sea Consola CDC o HID)
    auto mode = cbdos::usb::UsbManager::getInstance().getBootMode();
    if (mode == cbdos::usb::UsbMode::Cdc) {
        USBSerial.flush();
        USB.enable(false);
    }
    
    // 2. Indicar a la ROM que el siguiente arranque USB es exclusivo para descarga
    chip_usb_set_persist_flags(USBDC_PERSIST_FLAG_BOOTLOADER);

    // 3. Forzar bit de arranque en modo descarga en el dominio RTC
    REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);

    // 4. Pequeña espera para asentar la caída del bus en Linux
    delay(100);

    // 5. Reinicio del sistema
    esp_restart();
}
```

### 2.3. Configuración de `esptool`
Verificado directamente en el CLI oficial de `esptool` instalado en el sistema (`python3 -m esptool --help`):
- Las opciones válidas para `--before` son exclusivamente con guion medio: `[default-reset | usb-reset | no-reset | no-reset-no-sync]`.
- Las opciones válidas para `--after` son: `[hard-reset | soft-reset | no-reset | no-reset-stub | watchdog-reset]`.

**Comportamiento según el estado de la placa:**
- Si la placa **ya está reiniciada en modo bootloader real** (ROM Download Mode mediante `restartToBootloader()`), no necesita reset previo porque el chip ya está escuchando a `esptool`. Por tanto, `--before=no-reset` conecta de forma instantánea sin reiniciar el USB.
- Si se flashea contra el firmware en ejecución con reset automático de PlatformIO, el flag oficial es `--before=usb-reset`.
- **Regla:** No se modificará `platformio.ini` arbitrariamente; el objetivo central es que el comando de reinicio en el microcontrolador deje la ROM lista para que el flasheador no tenga fricción.

---

## 3. Plan de Ejecución en 3 Fases

### Fase 1: Corrección en la Capa HAL (`hal_system_s3.cpp`)
- [ ] Incluir cabeceras de ROM de Espressif (`usb_persist.h`, `chip_usb_dwc2.h`).
- [ ] Implementar la secuencia de desconexión de bus y configuración de bandera de persistencia en `restartToBootloader()`.
- [ ] Verificar que compile limpio en PlatformIO.

### Fase 2: Validación por Consola Serie y UI
- [ ] Validar que al tocar el botón en pantalla o enviar `CBDOS:BOOTLOADER` por serie:
  - Linux emite un evento `dmesg`: desconexión de `/dev/ttyACM0` y reconexión inmediata como dispositivo de arranque USB de Espressif (`ID 303a:1001 Espressif USB JTAG/serial debug unit`).
  - No requiere pulsar ningún botón físico en la placa.

### Fase 3: Validación del Flasheo Manos Libres
- [ ] Ejecutar `pio run -d bsp/esp32_s3_jc3248 -t upload` mientras la placa está en el modo bootloader genuino.
- [ ] Confirmar que `esptool` sincroniza en el intento 1 (`Connecting...` instantáneo) y completa la escritura al 100%.
