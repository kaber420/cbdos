# 📋 Plan de Implementación: Soporte de Teclado Físico USB Host (HID) y Feedback en HeaderBar (ESP32-P4)

> **Documento:** `specs/hardware/plan_implementacion_teclado_usb_host_p4.md`  
> **Target:** Guition JC4880P443C (ESP32-P4 RISC-V @ 400 MHz)  
> **Framework:** ESP-IDF 5.5 + LVGL v9.5 Estricto  
> **Referencias Auditadas:**  
> - `references/NucleoOS-P4/components/nv_hal/nv_hid_host.cpp`  
> - `references/ghostesp/main/managers/usb_keyboard_manager.c`  
> - `espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_usbhid.c`  
> - `specs/hardware/usb_host_hid_keyboard_p4_spec.md`  

---

## 1. Análisis Comparativo de Sistemas de Referencia

| Criterio | `ghostesp` | `NucleoOS-P4` | `esp_lvgl_port` Oficial | Decisión CBDos |
| :--- | :--- | :--- | :--- | :--- |
| **SoC / Plataforma** | ESP32-S3 (Arduino/C) | **ESP32-P4 (IDF/C++)** | **ESP32-P4 / S3 (IDF 5.x)** | **P4 Nativo (IDF 5.5)** |
| **Framework UI** | UI básica texto | LVGL custom + IME | **LVGL v9.5 estricto** | **LVGL v9.5 estricto** |
| **Driver USB Host** | Re-instala USB Host | Asume Host existente | **Asume Host existente** | **Reutiliza `hal_usb_host_p4`** |
| **Mapeo de Teclas** | Solo 57 ASCII básicos | ASCII + Flechas + Shortcuts | **ASCII + `LV_KEY_*` completos** | **`LV_KEY_*` + ASCII + `Ctrl+C`** |
| **Auto-repetición (Repeat)** | ❌ No tiene | ✅ Timer FreeRTOS/Ticks | ⚠️ Parcial | ✅ **Adoptar Timer de NucleoOS** |
| **Manejo Desconexión** | Básico | ✅ Limpia teclas pegadas | ✅ `hid_host_device_close` | ✅ **Limpia estado y avisa UI** |
| **Feedback UI (HeaderBar)**| ❌ No tiene | Event Bus interno | ❌ No tiene | ✅ **Icono `LV_SYMBOL_KEYBOARD`** |

### Conclusión Técnica
1. **`ghostesp` se descarta:** Está pensado para S3, re-instala `usb_host_install` (lo que causaría colisión fatal en el P4 con nuestro CDC Flasher) y su mapeo de teclas es plano sin navegación LVGL.
2. **`NucleoOS-P4` aporta la robustez de UX:** Su lógica para evitar teclas pegadas al desconectar (`kbd_release_all`), la traducción de `Ctrl+C` para terminales y el manejo de modificadores son ideales.
3. **`espressif/usb_host_hid` + `esp_lvgl_port` aporta la arquitectura oficial:** Se integra de forma limpia con el stack USB Host existente sin conflictos de handles ni hacking de endpoints.

### Refinamientos Críticos de la Auditoría Técnica:
1. **Exclusión Mutua de Rol USB (Host vs Device):** El SoC ESP32-P4 tiene un único controlador USB-OTG físico. En `UsbMode::Host`, el stack Device (`hal_hid_p4.cpp` BadUSB) permanece inactivo. El teclado físico opera exclusivamente cuando el sistema está en `UsbMode::Host` (gestionado por `UsbManager`).
2. **Reconciliación de `usb_host_install`:** La librería central USB Host de ESP-IDF ya es instalada por `P4UsbHostBackend::init()` en `hal_usb_host_p4.cpp`. El driver HID Host (`hid_host_install`) se monta como cliente del stack ya activo sin reinstalar la librería central.
3. **Thread-Safety Estricto en LVGL:** Los callbacks de eventos USB Host corren en una tarea FreeRTOS independiente. Para actualizar la `HeaderBar` o el foco de widgets, **está prohibido invocar APIs de LVGL sin protección**. Se utilizará un flag atómico / interrupción que la `HeaderBar` evalúe en su actualización o bajo `lvgl_port_lock(100)`.
4. **Cero Doble Autorepetición:** LVGL v9.5 dispone de gestión nativa de repetición (`long_press_repeat_time`) para indevs de teclado. El driver mantendrá `data->state = LV_INDEV_STATE_PRESSED` mientras la tecla esté en el reporte HID, y enviará `LV_INDEV_STATE_RELEASED` solo cuando se libere, delegando la cadencia de repetición al motor de LVGL sin duplicar temporizadores.

---

## 2. Arquitectura de Integración en CBDos

```
                   ┌──────────────────────────────────────────────┐
                   │        Dispositivo USB Host Físico           │
                   └──────────────────────┬───────────────────────┘
                                          │ Interrupción D+/D-
                                          ▼
                   ┌──────────────────────────────────────────────┐
                   │  P4UsbHostBackend (`hal_usb_host_p4.cpp`)     │
                   │      usb_host_install() YA inicializado      │
                   └──────────────────────┬───────────────────────┘
                                          │
                                          ▼
                   ┌──────────────────────────────────────────────┐
                   │    Driver HID Host: `espressif/usb_host_hid` │
                   │       hid_host_install(&hid_drv_config)      │
                   └──────────────────────┬───────────────────────┘
                                          │
             ┌────────────────────────────┴────────────────────────────┐
             ▼ (Evento Hot-Plug)                                       ▼ (Stream de Pulsaciones)
┌──────────────────────────────────────┐             ┌───────────────────────────────────────────┐
│ Callback Conexión / Desconexión      │             │ Parser HID Boot Keyboard Report (8 bytes) │
│ - Actualiza `s_hasPhysicalKeyboard`  │             │ - Decodifica Modifiers (Shift/Ctrl/Alt)   │
│ - Notifica a `HeaderBar`             │             │ - Maneja Auto-repeat (500ms delay / 33ms) │
└──────────────────┬───────────────────┘             └─────────────────────┬─────────────────────┘
                   │                                                       │
                   ▼                                                       ▼
┌──────────────────────────────────────┐             ┌───────────────────────────────────────────┐
│ `HeaderBar::updateKeyboardStatus()`  │             │ Cola FreeRTOS `s_kb_queue`                │
│ - Muestra/oculta `LV_SYMBOL_KEYBOARD`│             └─────────────────────┬─────────────────────┘
│ - Desactiva teclado virtual táctil   │                                   │
└──────────────────────────────────────┘                                   ▼
                                                     ┌───────────────────────────────────────────┐
                                                     │ `LVGL_Port.cpp`                           │
                                                     │ Dispositivo `LV_INDEV_TYPE_KEYPAD`        │
                                                     │ Asignado al `lv_group_t` de navegación    │
                                                     └─────────────────────┬─────────────────────┘
                                                                           │
                                                                           ▼
                                                     ┌───────────────────────────────────────────┐
                                                     │ Widget Activo con Foco:                   │
                                                     │ - `lv_textarea`: Escritura directa        │
                                                     │ - `TerminalView`: Shell interactiva / SSH │
                                                     │ - `lv_button` / `lv_list`: Flechas + Enter│
                                                     └───────────────────────────────────────────┘
```

---

## 3. Fases de Implementación Quirúrgica

### Fase 1: Inclusión de Dependencia IDF
* **Archivo:** `bsp/esp32_p4_jc4880/main/idf_component.yml`
* **Acción:** Declarar formalmente `espressif/usb_host_hid: "^1.2.0"`.
* **Resultado:** Habilita el header `usb/hid_host.h` y compila la extensión HID nativa de `esp_lvgl_port`.

### Fase 2: Backend y Driver HID Host P4
* **Archivo:** `bsp/esp32_p4_jc4880/hal/hal_usb_hid_host_p4.cpp` (nuevo módulo desacoplado)
* **Funciones:**
  1. `initUsbHidHostP4()`: Invoca `hid_host_install` enganchándose al bus USB ya levantado.
  2. `usb_hid_device_callback`: Detecta cuando el periférico conectado es un teclado boot protocol (`params.proto == HID_PROTOCOL_KEYBOARD`).
  3. `usb_hid_interface_callback`: Procesa los reportes de 8 bytes, gestiona autorepetición y coloca eventos en cola FreeRTOS.
  4. Manejo de desconexión segura: Limpia teclas activas (`kbd_release_all`) y notifica a la UI.

### Fase 3: Integración con LVGL v9.5 (`LVGL_Port.cpp`)
* **Archivo:** `bsp/esp32_p4_jc4880/hal/LVGL_Port.cpp` y `LVGL_Port.h`
* **Acción:**
  1. Registrar `s_lv_kb_indev` con tipo `LV_INDEV_TYPE_KEYPAD`.
  2. Implementar `keyboardReadCallback(lv_indev_t* indev, lv_indev_data_t* data)` extrayendo de la cola FreeRTOS.
  3. Crear un grupo global por defecto `s_kb_group = lv_group_create();` y asignarlo al indev.

### Fase 4: Core & Reactividad en `HeaderBar`
* **Archivos:**
  - `core/include/cbdos/input.hpp` (añadir `bool hasPhysicalKeyboard();`)
  - `core/src/ui/components/HeaderBar.hpp` y `HeaderBar.cpp`
* **Acción en UI:**
  1. Crear `m_labelKeyboard = lv_label_create(rightBox);` con `LV_SYMBOL_KEYBOARD` (Unicode `0xF11C`).
  2. En `HeaderBar::update()` (o reactivo vía evento de hardware), mostrar el icono en verde/cian si `hasPhysicalKeyboard() == true`, u ocultarlo con `LV_OBJ_FLAG_HIDDEN` si no hay teclado.
  3. Ocultamiento de Teclado Virtual: En `UIManager::attachKeyboard()`, si hay teclado físico, no desplegar el teclado en pantalla para maximizar el área visible.

---

## 4. Criterios de Aceptación y Validación

1. **Compilación Limpia:** `idf.py -C bsp/esp32_p4_jc4880 build` compila 100% sin advertencias ni errores.
2. **Detección Hot-Plug:** Al enchufar un teclado USB estándar al puerto USB del ESP32-P4, el log serie imprime la detección HID y en la barra superior aparece de inmediato el icono `⌨`.
3. **Escritura y Navegación:**
   - En cualquier `lv_textarea` (ej. modales de Omniwisp, editor de texto), las teclas pulsadas aparecen en pantalla.
   - Las teclas `Enter`, `Backspace`, `Tab` y flechas funcionan correctamente.
   - Dejar una tecla presionada produce auto-repetición fluida.
4. **Desconexión Limpia:** Al desenchufar el teclado, el icono `⌨` desaparece inmediatamente y ninguna tecla queda en bucle de pulsación.
