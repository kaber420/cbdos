# Plan de Soporte de Teclado USB Host para Editor de Scripts y Terminal

## 1. Objetivo y Alcance

Permitir la conexión directa y transparente de un **Teclado USB físico** (USB HID Boot Keyboard, alámbrico o receptor inalámbrico 2.4 GHz) al puerto USB-C mediante un adaptador OTG en el **ESP32-P4** (con arquitectura base extensible al ESP32-S3).

### Casos de Uso Principales:
1. **Editor de Scripts y Lua ([`TextEditorView`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TextEditorView.cpp)):**
   - Escribir código, scripts `.lua`, texto y configuración sin que el teclado virtual táctil tape la mitad de la pantalla.
   - Soporte completo para caracteres alfanuméricos, símbolos, saltos de línea (`Enter`), tabulación (`Tab`), borrado (`Backspace`/`Delete`) y navegación con flechas de cursor.
2. **Terminal Serie y SSH ([`TerminalView`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TerminalView.cpp)):**
   - Escribir comandos directamente y enviarlos con la tecla `Enter`.
3. **Modales y Formularios del Sistema:**
   - Introducción ágil de contraseñas Wi-Fi, parámetros SSH y URLs.

---

## 2. Arquitectura de Integración (Plug & Play)

```text
 ┌────────────────────────────────────────────────────────┐
 │                   Teclado USB Físico                   │
 └──────────────────────────┬─────────────────────────────┘
                            │ USB D+/D- (OTG)
 ┌──────────────────────────▼─────────────────────────────┐
 │           Stack USB Host ESP32-P4 (IDF)                │
 │         (usb_host_install ya operativo)                │
 └──────────────────────────┬─────────────────────────────┘
                            │ USB Class 0x03 (HID)
 ┌──────────────────────────▼─────────────────────────────┐
 │       Driver espressif/usb_host_hid                    │
 │    - Enumeración Boot Keyboard                         │
 │    - Scancodes HID -> ASCII & Modifiers (Shift/Ctrl)   │
 └──────────────────────────┬─────────────────────────────┘
                            │ Interrupción / Queue
 ┌──────────────────────────▼─────────────────────────────┐
 │       esp_lvgl_port_usbhid (LVGL v9)                   │
 │    - Driver LV_INDEV_TYPE_KEYPAD                       │
 │    - Asociación a lv_group_t por defecto               │
 └──────────────────────────┬─────────────────────────────┘
                            │ Focus automático
 ┌──────────────────────────▼─────────────────────────────┐
 │  m_textArea en TextEditorView / TerminalView           │
 │  (Escribe directamente en el buffer de código)         │
 └────────────────────────────────────────────────────────┘
```

---

## 3. Plan de Implementación Paso a Paso

### Paso 1: Agregar Dependencia `espressif/usb_host_hid`
En [`bsp/esp32_p4_jc4880/main/idf_component.yml`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/main/idf_component.yml):
```yaml
dependencies:
  espressif/usb_host_hid: "^1.0.0"
```
Esto descarga el cliente oficial de ESP-IDF y expone el encabezado `#include "usb/hid_host.h"`, activando automáticamente el bloque de compilación `#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT` que ya existe dentro de nuestro componente `esp_lvgl_port`.

### Paso 2: Configuración del Grupo y Registro en `LVGL_Port.cpp`
En [`bsp/esp32_p4_jc4880/hal/LVGL_Port.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/LVGL_Port.cpp):
1. Crear un grupo de navegación por defecto de LVGL si no existe:
   ```cpp
   lv_group_t* def_group = lv_group_get_default();
   if (!def_group) {
       def_group = lv_group_create();
       lv_group_set_default(def_group);
   }
   ```
2. Inicializar la entrada de teclado USB HID:
   ```cpp
   lvgl_port_hid_keyboard_cfg_t kb_cfg = {
       .disp = m_lvDisplay
   };
   lv_indev_t* kb_indev = lvgl_port_add_usb_hid_keyboard_input(&kb_cfg);
   if (kb_indev && def_group) {
       lv_indev_set_group(kb_indev, def_group);
   }
   ```

### Paso 3: Enfoque Automático y Ocultación del Teclado Táctil
En [`core/src/ui/views/TextEditorView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TextEditorView.cpp):
1. Asegurar que al crearse la vista, `m_textArea` se agregue al grupo por defecto:
   ```cpp
   lv_group_t* def_group = lv_group_get_default();
   if (def_group) {
       lv_group_add_obj(def_group, m_textArea);
       lv_group_focus_obj(m_textArea);
   }
   ```
2. Si se detecta entrada desde un teclado físico (`LV_EVENT_KEY`), llamar a `UIManager::closeKeyboard()` para que el teclado táctil virtual no robe espacio en la pantalla.

---

## 4. Criterios de Aceptación y Validación

1. **Compilación Limpia:**
   - ESP32-P4 (ESP-IDF): `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
   - ESP32-S3 (PlatformIO): `pio run -d bsp/esp32_s3_jc3248`
2. **Validación en Hardware Real (ESP32-P4):**
   - Conectar un teclado USB físico al puerto USB-C (mediante OTG).
   - Los logs del sistema registran la conexión del teclado (`HID_HOST_DRIVER_EVENT_CONNECTED`, protocolo Boot Keyboard).
   - Abrir el **Editor de Texto / Lua**: pulsar teclas físicas escribe texto en tiempo real en `m_textArea`.
   - Probar teclas especiales: `Enter` crea una nueva línea, `Backspace` borra caracteres anteriores, las flechas mueven el cursor.
   - Abrir la **Terminal Serie**: escribir texto y presionar `Enter` envía el comando por el puerto serie activo.
   - Desconectar el teclado USB: el sistema no se bloquea y los controles táctiles siguen respondiendo con normalidad.
