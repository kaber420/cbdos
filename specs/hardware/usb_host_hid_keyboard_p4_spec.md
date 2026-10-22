# ⌨️ Especificación Técnica: Teclado Físico USB Host (HID) y Teclado I2C en ESP32-P4

> **Estado:** Aprobado / Especificación de Hardware e Interfaces  
> **Fecha:** 21 de Octubre de 2026  
> **Target Primario:** Guition JC4880P443C (ESP32-P4 RISC-V @ 400 MHz)  
> **Framework Gráfico:** LVGL v9.5 estricto (`LV_INDEV_TYPE_KEYPAD` + `lv_group_t`)  
> **Sistema de Referencia Auditado:** `references/Slave_I` (`platforms/tab5/main/hal/components/hal_usb.cpp`)

---

## 1. Visión General y Objetivos

Actualmente, **CBDos** opera en todas sus plataformas únicamente mediante pantalla táctil (`LV_INDEV_TYPE_POINTER`). Para convertir la placa **Guition JC4880P443C (ESP32-P4)** en una estación de campo o Cyberdeck táctico de alto rendimiento, es indispensable soportar entrada por teclado físico:

1. **Teclado USB Convencional (USB HID Host):** Conexión *plug-and-play* de cualquier teclado USB estándar (mecánico, de membrana o inalámbrico con dongle USB 2.4 GHz) a los puertos USB de la placa.
2. **Teclado I2C Compacto:** Soporte para teclados integrados en el chasis (M5Stack CardKB, teclados tipo BlackBerry Q10 o matrices con controlador I2C TCA8418).
3. **Navegación Unificada en LVGL v9.5:** Control de interfaz completa (botones, listas, cards, consolas y campos de texto) usando flechas, Enter, Escape y Tabulador.
4. **Ocultamiento Dinámico del Teclado Virtual:** Detección en tiempo real de teclado físico conectado para no desplegar el teclado en pantalla y maximizar el área visible.

---

## 2. Topología de Hardware en ESP32-P4 (JC4880P443C)

El microcontrolador **ESP32-P4** incorpora un periférico controlador **USB 2.0 On-The-Go (OTG)** con transceptor PHY integrado Full-Speed / High-Speed:

```
┌────────────────────────────────────────────────────────┐
│               ESP32-P4 (JC4880P443C)                   │
│                                                        │
│   USB_DP (D+) ────────────────────► Conector USB-A     │
│   USB_DM (D-) ────────────────────► (Host Port)        │
│                                                        │
│   GPIO VBUS_EN ───────────────────► Interruptor 5V     │
│   (Habilitación de alimentación VBUS al periférico)    │
│                                                        │
│   I2C_SDA (GPIO 7)  ──────────────► Puerto I2C         │
│   I2C_SCL (GPIO 8)  ──────────────► Teclado Chasis     │
└────────────────────────────────────────────────────────┘
```

---

## 3. Arquitectura del Software: Doble Entrada y Cola Unificada

Para evitar duplicación de código en la UI, el sistema implementa una cola compartida FreeRTOS (`kb_queue`) alimentada concurrentemente por la tarea USB Host y la tarea de sondeo I2C:

```
 ┌──────────────────────────────────┐      ┌──────────────────────────────────┐
 │    Dispositivo USB HID Host      │      │     Teclado I2C (CardKB/Q10)     │
 │  (usb/hid_host + boot protocol)  │      │   (Driver Master I2C @ 100kHz)   │
 └─────────────────┬────────────────┘      └─────────────────┬────────────────┘
                   │ Reporte 8 bytes                         │ Registros ASCII
                   ▼                                         ▼
 ┌────────────────────────────────────────────────────────────────────────────┐
 │                  Parser y Traductor a Keycodes LVGL                        │
 │           - Modificadores (Shift, Ctrl, Alt)                               │
 │           - Teclas Especiales (Enter, Backspace, Esc, Flechas, Tab)        │
 │           - Caracteres ASCII (a..z, A..Z, 0..9, símbolos)                  │
 └─────────────────────────────────────┬──────────────────────────────────────┘
                                       │ xQueueSend(kb_queue, &lvkey, 0)
                                       ▼
       ┌──────────────────────────────────────────────────────────────┐
       │     FreeRTOS Queue: `s_kb_queue` (32 items, sizeof(uint32_t))│
       └───────────────────────────────┬──────────────────────────────┘
                                       │ lvgl_kb_read_cb()
                                       ▼
       ┌──────────────────────────────────────────────────────────────┐
       │              LVGL v9.5 Keypad Input Device                   │
       │              `lv_indev_t* s_lv_kb_indev`                     │
       │              Tipo: `LV_INDEV_TYPE_KEYPAD`                    │
       │              Asignado a: `s_kb_nav_group` (`lv_group_t`)     │
       └───────────────────────────────┬──────────────────────────────┘
                                       ▼
     ┌──────────────────────────────────────────────────────────────────┐
     │ Widget con Foco Actual en UI:                                    │
     │  - `lv_textarea`: Escritura directa de caracteres (Shell / Ed.)  │
     │  - `lv_button` / `lv_list`: Foco y activación con Enter / Flechas│
     └──────────────────────────────────────────────────────────────────┘
```

---

## 4. Parser de Reportes HID Boot Protocol (USB)

El estándar USB HID define el *Boot Keyboard Input Report* con una estructura de 8 bytes fija:

```c
typedef struct {
    uint8_t modifiers;      // Bit 0: LCtrl, 1: LShift, 2: LAlt, 3: LGUI, 4: RCtrl, 5: RShift, etc.
    uint8_t reserved;       // Siempre 0x00
    uint8_t keycodes[6];    // Hasta 6 teclas presionadas simultáneamente (Key Rollover)
} __attribute__((packed)) hid_boot_kb_report_t;
```

### Tabla de Traducción de Keycodes HID a LVGL:

```cpp
static uint32_t usb_hid_to_lvkey(uint8_t code, bool shift)
{
    // Letras minúsculas / mayúsculas
    if (code >= 0x04 && code <= 0x1d) {
        char c = static_cast<char>('a' + (code - 0x04));
        return shift ? static_cast<uint32_t>(c - 32) : static_cast<uint32_t>(c);
    }
    // Fila numérica y símbolos con Shift
    if (code >= 0x1e && code <= 0x26) {
        static const char* sh = ")!@#$%^&*(";
        return shift ? static_cast<uint32_t>(sh[code - 0x1d])
                     : static_cast<uint32_t>('1' + (code - 0x1e));
    }
    // Teclas de control y navegación LVGL
    switch (code) {
        case 0x27: return shift ? ')' : '0';
        case 0x28: return LV_KEY_ENTER;
        case 0x29: return LV_KEY_ESC;
        case 0x2a: return LV_KEY_BACKSPACE;
        case 0x2b: return LV_KEY_NEXT;        // Tabulador -> Siguiente foco
        case 0x2c: return ' ';
        case 0x2d: return shift ? '_' : '-';
        case 0x2e: return shift ? '+' : '=';
        case 0x2f: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
        case 0x4a: return LV_KEY_HOME;
        case 0x4d: return LV_KEY_END;
        case 0x4f: return LV_KEY_RIGHT;       // Flecha Derecha
        case 0x50: return LV_KEY_LEFT;        // Flecha Izquierda
        case 0x51: return LV_KEY_DOWN;        // Flecha Abajo
        case 0x52: return LV_KEY_UP;          // Flecha Arriba
        default:   return 0;
    }
}
```

---

## 5. Integración con LVGL v9.5

### 5.1. Callback de Lectura de Indev (`lvgl_kb_read_cb`)
```cpp
static void lvgl_kb_read_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    static uint32_t last_key = 0;
    uint32_t current_key = 0;

    if (s_kb_queue && xQueueReceive(s_kb_queue, &current_key, 0) == pdTRUE) {
        last_key = current_key;
        data->key = current_key;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
```

### 5.2. Inicialización en el BSP (`LVGL_Port.cpp` / HAL P4)
```cpp
void init_keyboard_subsystem(lv_display_t* disp)
{
    // 1. Crear cola de eventos FreeRTOS
    s_kb_queue = xQueueCreate(32, sizeof(uint32_t));

    // 2. Crear grupo de navegación global para teclado
    s_kb_nav_group = lv_group_create();
    lv_group_set_wrap(s_kb_nav_group, true);

    // 3. Registrar dispositivo de entrada KEYPAD en LVGL
    s_lv_kb_indev = lv_indev_create();
    lv_indev_set_type(s_lv_kb_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(s_lv_kb_indev, lvgl_kb_read_cb);
    lv_indev_set_display(s_lv_kb_indev, disp);
    lv_indev_set_group(s_lv_kb_indev, s_kb_nav_group);
}
```

---

## 6. Detección Dinámica y Supresión del Teclado Virtual

Cuando un teclado físico está conectado, el teclado virtual en pantalla consume espacio valioso en la pantalla táctil de 480×800.

Se define el hook del sistema:
```cpp
bool cbdos_has_physical_keyboard();
```
* **Comportamiento en UI:** Si `cbdos_has_physical_keyboard() == true`, las vistas de CBDos (`FilesView`, `TerminalView`, `SettingsView`, modales) omiten la creación de `lv_keyboard_create()` y asignan directamente el foco del `s_kb_nav_group` al `lv_textarea`. Si se desconecta, el teclado virtual vuelve a emerger automáticamente.

---

## 7. Plan de Implementación en Código

1. **Fase 1 (BSP P4):** Crear componente `bsp/esp32_p4_jc4880/hal/hal_usb_host_keyboard.cpp` con el stack `usb/hid_host` de ESP-IDF 5.5.
2. **Fase 2 (Indev LVGL):** Registrar `s_lv_kb_indev` en `LVGL_Port.cpp` dentro del BSP P4.
3. **Fase 3 (Enfoque en Vistas):** Vincular los editores de texto y la consola UART/SSH al grupo `s_kb_nav_group` para escritura fluida en caliente.
