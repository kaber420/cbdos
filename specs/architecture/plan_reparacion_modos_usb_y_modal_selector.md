# 🔌 Plan Técnico Definitivo: Sistema Unificado de Selección de Modos USB (`UsbModal`), i18n y Desacoplamiento HAL en ESP32-S3 y ESP32-P4

**Fecha:** Octubre 2026  
**Documento:** `specs/architecture/plan_reparacion_modos_usb_y_modal_selector.md`  
**Estado:** 📋 Especificación Técnica Auditada (Aprobada para Implementación)  
**Autor:** Desarrollador Senior de Sistemas Embebidos / Equipo de Arquitectura CBDos  
**Objetivo:** Eliminar conmutaciones ciegas de 1 toque, replicar la arquitectura exacta de selección modal de idioma (`LanguageModal`) para los modos USB, y desacoplar los perfiles de hardware en el código activo para evitar colisiones y reinicios.

---

## 🏛️ 1. Diagnóstico Físico y Lecciones Aprendidas

### 1.1. Por qué falló el descriptor compuesto "híbrido" (Teclado + Ratón + FIDO2)
En el intento previo en `bsp/esp32_s3_jc3248/hal/hal_hid_s3.cpp`, se unificó en un solo descriptor USB:
- Teclado (`REPORT_ID_KEYBOARD = 1`)
- Ratón (`REPORT_ID_MOUSE = 2`)
- FIDO2 / CTAPHID (`REPORT_ID_FIDO = 3`, tamaño 64 bytes)

**Causa física del pánico:**
1. En USB 2.0 Full-Speed (hardware nativo de ESP32-S3), el tamaño máximo de paquete para un endpoint de interrupción HID es estrictamente de **64 bytes** (`CFG_TUD_ENDPOINT_SIZE = 64`).
2. Al incorporar un `Report ID` a un reporte de 64 bytes (FIDO2), la especificación USB obliga a anteponer el byte del ID en cada transacción sobre el bus.
3. El paquete resultante en el bus es de **65 bytes** (1 byte de Report ID + 64 bytes de payload CTAP).
4. Al recibir o enviar 65 bytes contra un búfer de endpoint configurado a 64 bytes, la pila TinyUSB sufre un desbordamiento de memoria / fallo de aserción crítico, provocando el reinicio del microcontrolador en bucle continuo al arrancar.

### 1.2. Por qué el selector de 1 toque (`cur == Hid ? Host : Hid`) era deficiente
En [`core/src/ui/views/ConfigView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/ConfigView.cpp), la fila de USB alternaba ciegamente entre dos modos con un solo tap. Esto adolecía de tres fallas graves de diseño:
1. **Falta de opciones:** Dejaba fuera a **FIDO2 / Kerberos**, tratándolo como un fantasma en lugar de un modo legítimo del Cyberdeck.
2. **Acción involuntaria:** Un toque accidental forzaba un toast y un reinicio no deseado del sistema.
3. **Inconsistencia de interfaz:** Mientras que el selector de idioma disponía de un modal elegante con visualización clara del estado activo y botón de cancelación, el selector USB era un toggle binario improvisado.

---

## 🔍 2. Auditoría del Patrón Modal (`LanguageModal`)

Para garantizar que el nuevo selector USB sea arquitectónicamente idéntico y mantenga una coherencia del 100% en todo el sistema, se replica la implementación de `LanguageModal`:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ARQUITECTURA DE PATRÓN MODAL                             │
├─────────────────────────────────────────────────────────────────────────────┤
│ 1. Capa Superior (lv_layer_top()):                                          │
│    - Máscara oscurecida a pantalla completa con LV_OPA_80 (0x000000).       │
│                                                                             │
│ 2. Tarjeta Elevada (DefaultTheme::applyRaisedCard):                         │
│    - Ancho responsive (380px para pantallas grandes, 280px para compactas). │
│    - Disposición en columna con espaciado vertical homogéneo (pad_row = 10).│
│                                                                             │
│ 3. Cabecera y Título:                                                       │
│    - lv_label con tipografía &lv_font_montserrat_16 y color de tema.        │
│                                                                             │
│ 4. Filas de Selección Dinámica:                                             │
│    - Botones estilizados con DefaultTheme::applyButton(opt, 12).             │
│    - Layout en fila (LV_FLEX_FLOW_ROW) con LV_FLEX_ALIGN_SPACE_BETWEEN.     │
│    - Izquierda: Nombre de la opción (lv_label).                             │
│    - Derecha: Icono de confirmación LV_SYMBOL_OK con color de acento         │
│      primario (DefaultTheme::getPrimaryAccent()) en la opción activa.       │
│                                                                             │
│ 5. Botón de Cierre:                                                         │
│    - Botón inferior ("Cerrar" / "Close") para descartar sin cambios.        │
│                                                                             │
│ 6. Callback de Selección (option_cb):                                       │
│    - Debounce / guarda: si ya hay un reinicio en curso, ignorar clicks.     │
│    - Si target == actual: cierra el modal sin reiniciar.                    │
│    - Si target != actual: persiste en NVS, muestra UIManager::showToast     │
│      con mensaje de reinicio, y programa un lv_timer_create(..., 1500)      │
│      para que el usuario lea el toast antes del reinicio físico.            │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 🛠️ 3. Especificación de `UsbModal` (`core/src/ui/modals/UsbModal.*`)

### 3.1. Opciones Disponibles en el Modal

| Índice UI | Modo (`UsbMode`) | Título en Pantalla | Propósito / Perfil de Hardware |
|---|---|---|---|
| **0** | `UsbMode::Hid` | `STR_CFG_USB_OPT_HID` | Teclado / Ratón compuesto (Report IDs 1 y 2, 9 y 6 bytes). |
| **1** | `UsbMode::Fido` | `STR_CFG_USB_OPT_FIDO` | Llave FIDO2 dedicada (CTAPHID puro de 64 bytes sin Report ID). |
| **2** | `UsbMode::Host` | `STR_CFG_USB_OPT_HOST` | USB Host: stack Device apagado, PHY entregado al backend de Host. |

### 3.2. Mapeo Seguro y Debounce
El enum `UsbMode` se indexa `{Hid = 0, Host = 1, Fido = 2}`. Para desacoplar la posición visual del índice del enum, se utiliza una tabla explícita:
```cpp
static const UsbMode kModes[3] = {
    UsbMode::Hid,
    UsbMode::Fido,
    UsbMode::Host
};
```
Se añade una bandera `s_rebootPending` estática para evitar que un doble tap genere timers concurrentes.

---

## 🧠 4. Actualización del Gestor Central (`UsbManager`)

En [`core/include/cbdos/usb_manager.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/usb_manager.hpp):

```cpp
namespace cbdos {
namespace usb {

enum class UsbMode : uint8_t {
    Hid  = 0,  // TinyUSB Device: Teclado y raton hacia el PC
    Host = 1,  // USB Host: modem, flasher, radios CDC, perifericos
    Fido = 2   // TinyUSB Device: Llave de seguridad FIDO2 / Kerberos pura
};
```

En [`core/src/system/usb_manager.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/system/usb_manager.cpp):
```cpp
UsbMode sanitize(uint8_t v) {
    if (v == static_cast<uint8_t>(UsbMode::Host)) return UsbMode::Host;
    if (v == static_cast<uint8_t>(UsbMode::Fido)) return UsbMode::Fido;
    return UsbMode::Hid;
}
```
*Garantía:* Cualquier valor previo en NVS (`0` o `1`) mantiene su compatibilidad absoluta. Valores no reconocidos colapsan de forma segura a `UsbMode::Hid`.

---

## ⚡ 5. Desacoplamiento de la Capa de Hardware (S3 y P4)

### 5.1. ESP32-S3 (`hal_hid_s3.cpp` y `main.cpp`)
1. **Modo `UsbMode::Hid`:**
   - Descriptor compuesto estándar: Teclado (ID 1) y Ratón (ID 2).
   - Paquetes de 9 bytes y 6 bytes (< 64 bytes).
   - Lectura de LEDs del host en `_onOutput()` para el reporte de teclado.
2. **Modo `UsbMode::Fido`:**
   - Descriptor oficial CTAPHID **sin Report ID**.
   - Paquetes de exactamente 64 bytes IN / 64 bytes OUT (`SendReport(buf, 64)`).
   - Conexión con `KerberosManager::instance().handleIncomingUsbReport()`.
   - `USB.productName("KERBEROS FIDO2 Security Key")` configurado antes de `USB.begin()`.
3. **Modo `UsbMode::Host`:**
   - TinyUSB Device apagado; se inicializa `cbdos::bsp::initUsbHostBackendS3()`.
4. **Bucle principal (`bsp/esp32_s3_jc3248/src/main.cpp`):**
   - El sondeo de cola en `loop()` debe contemplar ambos modos Device:
     ```cpp
     auto mode = cbdos::usb::UsbManager::getInstance().getBootMode();
     if (mode == cbdos::usb::UsbMode::Hid || mode == cbdos::usb::UsbMode::Fido) {
         cbdos_hid_s3_poll();
     }
     ```

### 5.2. ESP32-P4 (`hal_hid_p4.cpp` y `main.cpp`)
1. **Orden de inicialización:** Unificar para que `UsbManager::getInstance().init()` se ejecute **antes** de cualquier decisión sobre los stacks USB en `main.cpp`.
2. **Compatibilidad:** P4 inicia con `UsbMode::Hid` y `UsbMode::Host`. Si el modo es `Fido`, se prepara para que Kerberos/CTAP no interfiera con el descriptor compuesto de HID ni genere colisiones.

### 5.3. Gating en Vistas (`HidView.cpp` y `KerberosView.cpp`)
- En [HidView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/HidView.cpp): Si el modo actual no es `Hid`, mostrar el toast indicando el modo activo real (`FIDO2` o `HOST`) y bloquear el envío de HID.
- En [KerberosView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/apps/kerberos/KerberosView.cpp): Si el modo actual no es `Fido`, advertir al usuario que debe cambiar a modo FIDO2 en Ajustes para utilizar la autenticación hardware.

---

## 🌐 6. Diccionario de Cadenas i18n (`language.hpp` y `language.cpp`)

Para cumplir con la regla estricta de `language.hpp` (*NUNCA renumerar, solo añadir al final*) y respetar los `static_assert` de conteo de cadenas:

### 6.1. Reutilización de Claves Existentes (0x0025 - 0x0029)
- `STR_CFG_USB_MODE = 0x0025`: Se mantiene como título (`"Modo USB"` / `"USB Mode"`).
- `STR_CFG_USB_HID_SUB = 0x0026`: Se actualiza el texto eliminando el texto obsoleto de 1 toque:
  - ES: `"Actual: Teclado/Raton (HID)"`
  - EN: `"Current: Keyboard/Mouse (HID)"`
- `STR_CFG_USB_HOST_SUB = 0x0027`: Se actualiza el texto:
  - ES: `"Actual: USB Host (Modem/CDC)"`
  - EN: `"Current: USB Host (Modem/CDC)"`
- `STR_CFG_USB_TO_HID = 0x0028`: `"Modo HID seleccionado. Reiniciando..."` / `"HID Mode selected. Rebooting..."`
- `STR_CFG_USB_TO_HOST = 0x0029`: `"Modo USB Host seleccionado. Reiniciando..."` / `"USB Host Mode selected. Rebooting..."`

### 6.2. Nuevas Claves Añadidas al Final (desde 0x00C3)
```cpp
    // Modos USB (Fase UsbModal)
    STR_CFG_USB_OPT_HID  = 0x00C3,
    STR_CFG_USB_OPT_FIDO = 0x00C4,
    STR_CFG_USB_OPT_HOST = 0x00C5,
    STR_CFG_USB_SUB_FIDO = 0x00C6,
    STR_CFG_USB_TO_FIDO  = 0x00C7,

    // Total Count
    STR_COUNT = 0x00C8,
```

Textos correspondientes en `language.cpp`:
| `StrId` | Español (ES) | Inglés (EN) |
|---|---|---|
| `STR_CFG_USB_OPT_HID` | `"Teclado / Raton (HID)"` | `"Keyboard / Mouse (HID)"` |
| `STR_CFG_USB_OPT_FIDO` | `"Llave FIDO2 (Kerberos)"` | `"FIDO2 Key (Kerberos)"` |
| `STR_CFG_USB_OPT_HOST` | `"USB Host (Modem/CDC)"` | `"USB Host (Modem/CDC)"` |
| `STR_CFG_USB_SUB_FIDO` | `"Actual: Llave FIDO2"` | `"Current: FIDO2 Key"` |
| `STR_CFG_USB_TO_FIDO` | `"Modo FIDO2 seleccionado. Reiniciando..."` | `"FIDO2 Mode selected. Rebooting..."` |

---

## 📋 7. Plan de Ejecución en 5 Fases

- [x] **Fase 1: Vocabulario i18n y Contrato Central**
  - Añadidas las 5 nuevas claves al final de `language.hpp` (`0x00C3` a `0x00C7`), ajustado `STR_COUNT = 0x00C8`.
  - Actualizado `lang_es` y `lang_en` en `language.cpp` (textos limpios para `0x0026-0x0029` y nuevos textos `0x00C3-0x00C7`).
  - Expandido `UsbMode` (`Fido = 2`) y `sanitize()` en `usb_manager.hpp` y `usb_manager.cpp`.
- [x] **Fase 2: Implementación de `UsbModal`**
  - Creados `core/src/ui/modals/UsbModal.hpp` y `core/src/ui/modals/UsbModal.cpp` con diseño simétrico a `LanguageModal`, tabla de mapeo `kModes` y protección anti-rebote.
  - Registrado en `core/CMakeLists.txt`.
- [x] **Fase 3: Integración en Pantalla de Ajustes y Gating de Vistas**
  - En `ConfigView.cpp`: reemplazado el toggle de la fila 9 por `UsbModal::show()`.
  - Actualizado el subtítulo dinámico de la fila 9 para soportar los 3 modos (`HID`, `FIDO2`, `HOST`).
  - Actualizado gating en `HidView.cpp` y `KerberosView.cpp`.
- [x] **Fase 4: Desacoplamiento de Hardware en ESP32-S3 y ESP32-P4**
  - En `hal_hid_s3.cpp`: separados descriptores limpios (Compuesto Teclado+Ratón vs CTAPHID puro sin Report ID).
  - En `bsp/esp32_s3_jc3248/src/main.cpp`: polling en `loop()` para `Hid || Fido`.
  - En `bsp/esp32_p4_jc4880/main/main.cpp`: unificado orden de inicialización de `UsbManager`.
- [x] **Fase 5: Validación y Compilación Multi-Target**
  - Compilación PlatformIO ESP32-S3: `pio run -d bsp/esp32_s3_jc3248` -> `SUCCESS`.
  - Compilación ESP-IDF ESP32-P4: `idf.py -C bsp/esp32_p4_jc4880 build` -> `SUCCESS`.
