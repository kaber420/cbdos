# 🔌 Plan Técnico Definitivo: Sistema de 4 Modos USB Puros (`UsbModal`), Modo Consola CDC Aislado y Erradicación de Colisiones Hardware

**Fecha:** Octubre 2026  
**Documento:** `specs/architecture/plan_4_modos_usb_puros_con_cdc.md`  
**Estado:** 📋 Especificación Técnica y Plan de Implementación  
**Autor:** Desarrollador Senior de Sistemas Embebidos / Arquitectura CBDos  
**Objetivo:** Eliminar la inyección de consola serie en los modos HID y FIDO2, aislar la consola CDC en su propio modo dedicado, y garantizar que cada modo USB sea un descriptor puro sin interferencias de endpoints.

---

## 🏛️ 1. Diagnóstico Forense y Causa Raíz

### 1.1. Por qué fallaba el bus USB (`error -22`) y se reiniciaba la placa
En los análisis de registros del núcleo Linux (`journalctl`):
```text
usbhid 3-3:1.0: can't add hid device: -22
cdc_acm 3-3:1.1: ttyACM0: USB ACM device
usb 3-3: USB disconnect -> REINICIO EN BUCLE
```

**Causa física:**
1. En `platformio.ini`, la bandera `-DARDUINO_USB_CDC_ON_BOOT=1` forzaba a la pila de Arduino a crear obligatoriamente una interfaz `cdc_acm` en el bus USB al arrancar el chip.
2. Al intentar levantar simultáneamente un teclado, un ratón o una llave FIDO2, el sistema creaba un dispositivo compuesto híbrido ("quimera") que competía por los limitados endpoints del único controlador USB-OTG del ESP32-S3.
3. Un teclado o ratón comercial **jamás expone una consola serie CDC hacia el host**. La mezcla de clases confunde al stack host y desborda los recursos, provocando el error `-22` (`-EINVAL`) en el host y un fallo de aserción crítico con reinicio en el microcontrolador.

### 1.2. Principio Rector: Modos Puros y Excluyentes
El bus USB no debe mezclar responsabilidades. Cada función del sistema debe residir en su propio perfil de hardware limpio y aislado:
- **Teclado:** Debe ser 100% Teclado/Ratón HID. Sin CDC.
- **FIDO2:** Debe ser 100% Llave de Seguridad WebAuthn (CTAPHID). Sin CDC.
- **Consola:** Debe ser 100% Puerto Serie USB CDC ACM para depuración y flasheo. Sin HID.
- **Host:** Debe ser 100% Anfitrión USB para periféricos y radios externas.

---

## 🛠️ 2. Especificación de los 4 Modos de Hardware

| Índice UI | Modo (`UsbMode`) | Nombre en Pantalla | Perfil de Hardware | Descripción y Uso |
|---|---|---|---|---|
| **0** | `UsbMode::Hid` | **Teclado / Ratón (HID)** | HID Compuesto Puro | Teclado + Ratón para BadUSB, Touchpad y control de PC. Sin CDC. |
| **1** | `UsbMode::Fido` | **Llave FIDO2 (Kerberos)** | CTAPHID Puro (64B) | Llave hardware WebAuthn/U2F nativa. Sin Report ID, sin CDC. |
| **2** | `UsbMode::Cdc` | **Consola Serie / CDC** | USB CDC ACM Puro | Puerto serie virtual para flasheador web, terminal serie y depuración. Sin HID. |
| **3** | `UsbMode::Host` | **USB Host (Módems/CDC)** | Anfitrión USB | Stack Device apagado; PHY entregado al backend de Host para radios y módems. |

---

## ⚙️ 3. Configuración del BSP y Build System (`platformio.ini`)

En `bsp/esp32_s3_jc3248/platformio.ini`:
```ini
build_flags =
    ...
    -DARDUINO_USB_CDC_ON_BOOT=0   ; Cero inyeccion de CDC al arranque
    -DARDUINO_USB_MODE=0          ; TinyUSB en control del bus OTG
```

*Efecto:* El microcontrolador arranca en frío sin forzar interfaces serie en el bus. La activación de interfaces USB ocurre bajo demanda según el modo persistido en NVS.

---

## 🌐 4. Diccionario de Cadenas i18n (`language.hpp` y `language.cpp`)

Se añaden las cadenas para el modo CDC al final del enum `StrId`, preservando la regla estricta de no renumerar:

```cpp
    // Modos USB (Fase 4 Modos Puros)
    STR_CFG_USB_OPT_CDC  = 0x00C8,
    STR_CFG_USB_SUB_CDC  = 0x00C9,
    STR_CFG_USB_TO_CDC   = 0x00CA,

    // Total Count
    STR_COUNT = 0x00CB,
```

**Textos asociados:**
| `StrId` | Español (ES) | Inglés (EN) |
|---|---|---|
| `STR_CFG_USB_OPT_CDC` | `"Consola Serie / CDC"` | `"Serial Console / CDC"` |
| `STR_CFG_USB_SUB_CDC` | `"Actual: Consola CDC"` | `"Current: CDC Console"` |
| `STR_CFG_USB_TO_CDC` | `"Modo Consola CDC seleccionado. Reiniciando..."` | `"CDC Console Mode selected. Rebooting..."` |

---

## 🧠 5. Gestor Central (`UsbManager`)

En `core/include/cbdos/usb_manager.hpp`:
```cpp
namespace cbdos {
namespace usb {

enum class UsbMode : uint8_t {
    Hid  = 0,  // TinyUSB Device: Teclado y raton puro
    Host = 1,  // USB Host: modem, flasher, radios CDC, perifericos
    Fido = 2,  // TinyUSB Device: Llave FIDO2 / Kerberos pura
    Cdc  = 3   // TinyUSB Device: Consola Serie / Flasheador Web puro
};
```

En `core/src/system/usb_manager.cpp`:
```cpp
UsbMode sanitize(uint8_t v) {
    if (v == static_cast<uint8_t>(UsbMode::Host)) return UsbMode::Host;
    if (v == static_cast<uint8_t>(UsbMode::Fido)) return UsbMode::Fido;
    if (v == static_cast<uint8_t>(UsbMode::Cdc))  return UsbMode::Cdc;
    return UsbMode::Hid;
}
```

---

## 🎨 6. Ventana Modal (`UsbModal`)

En `core/src/ui/modals/UsbModal.cpp`:
- Se define la tabla de 4 modos:
  ```cpp
  static const cbdos::usb::UsbMode kModes[4] = {
      cbdos::usb::UsbMode::Hid,
      cbdos::usb::UsbMode::Fido,
      cbdos::usb::UsbMode::Cdc,
      cbdos::usb::UsbMode::Host
  };
  ```
- 4 botones con diseño `DefaultTheme::applyButton(opt, 12)`, mostrando el título y la marca verde `LV_SYMBOL_OK` en el modo actualmente activo.
- Al seleccionar una opción distinta a la actual: persiste en NVS, muestra el toast de reinicio y programa el temporizador diferido de 1500 ms.

---

## ⚡ 7. Desacoplamiento de Hardware en ESP32-S3 (`main.cpp` y `hal_hid_s3.cpp`)

### 7.1. Arranque y Despacho en `bsp/esp32_s3_jc3248/src/main.cpp`:
```cpp
    cbdos::usb::UsbManager::getInstance().init();
    auto bootMode = cbdos::usb::UsbManager::getInstance().getBootMode();

    if (bootMode == cbdos::usb::UsbMode::Host) {
        cbdos::bsp::initUsbHostBackendS3();
    } else if (bootMode == cbdos::usb::UsbMode::Cdc) {
        // Modo Consola Serie puro: habilita USB CDC para web flasher y terminal
        USBSerial.begin(115200);
        USB.begin();
    } else {
        // Modos Device HID o FIDO2 puros: inicializa driver sin consola
        cbdos::bsp::initHidDriverS3();
    }
```

### 7.2. Descriptores en `hal_hid_s3.cpp`:
- **Modo `Hid`:** Únicamente `USBHIDKeyboard` y `USBHIDMouse`. Cero interfaces CDC.
- **Modo `Fido`:** Únicamente `USBHIDS3Fido` (descriptor CTAPHID de 64 bytes sin Report ID). Cero interfaces CDC.
- **Modo `Cdc` y `Host`:** La capa HID permanece completamente inactiva.

---

## 📋 8. Plan de Ejecución en 5 Fases

- [x] **Fase 1: Extensión de i18n y Contrato `UsbMode::Cdc`**
  - Añadir `STR_CFG_USB_OPT_CDC`, `STR_CFG_USB_SUB_CDC`, `STR_CFG_USB_TO_CDC` a `language.hpp` y textos en `language.cpp`.
  - Actualizar `UsbMode::Cdc = 3` y `sanitize()` en `usb_manager.hpp` y `usb_manager.cpp`.
- [x] **Fase 2: Configuración del BSP (`platformio.ini`)**
  - Establecer `-DARDUINO_USB_CDC_ON_BOOT=0` para erradicar la inyección de consola.
- [x] **Fase 3: Ampliación de `UsbModal` y Ajustes**
  - Actualizar `UsbModal.cpp` para mostrar las 4 opciones (`HID`, `FIDO2`, `CDC`, `HOST`).
  - Actualizar el subtítulo dinámico de la fila 9 en `ConfigView.cpp` para contemplar `UsbMode::Cdc`.
- [x] **Fase 4: Desacoplamiento Físico de Hardware (S3 y P4)**
  - Configurar en S3 `main.cpp` la inicialización exclusiva de `USBSerial` solo cuando `bootMode == UsbMode::Cdc`.
  - En `hal_hid_s3.cpp`, asegurar que los modos HID y FIDO2 no instancien ni arranquen interfaces CDC.
- [x] **Fase 5: Validación Multi-Target y Comprobación en Placa**
  - [x] Compilación limpia en ESP32-S3 y ESP32-P4.
  - [x] Flasheo a la JC3248 (`/dev/ttyACM0`) completado con éxito.
  - [x] Verificación en `journalctl`: erradicación confirmada del error `-22`, cero colisiones y bus USB estable sin reinicios.
