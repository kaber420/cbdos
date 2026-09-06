# Especificación Técnica: Ecosistema de Aplicaciones Vectoriales, Winks Multimedia y Bindings Lua

**Estado:** 📐 Propuesta / Futura Implementación  
**Versión de Especificación:** v1.0.0  
**Módulos Relacionados:** `core/ui/views/`, `core/lua/`, `core/mesh/`, `cbdos::lottie`  
**Autor:** CBDos Architecture Core Team  
**Fecha:** Agosto 2026  
**Documentos Base:**  
- [`motor_animacion_parallax_y_narrativa_interactiva_spec.md`](file:///home/kaber420/Documentos/proyectos/cbdos/docs/architecture/motor_animacion_parallax_y_narrativa_interactiva_spec.md)  
- [`audio_sincronizacion_y_localizacion_motor_cuentos.md`](file:///home/kaber420/Documentos/proyectos/cbdos/docs/architecture/audio_sincronizacion_y_localizacion_motor_cuentos.md)  

---

## 🎯 1. Visión y Objetivos

Con la validación exitosa del motor vectorial **ThorVG** y el widget **Lottie** en hardware real (**ESP32-P4** a 400 MHz con 32 MB de PSRAM), CBDos trasciende las interfaces embebidas estáticas convencionales.

Esta especificación formaliza la integración de animaciones vectoriales e interactividad avanzada a lo largo de tres pilares del sistema operativo:
1. **Aplicaciones del Sistema Enriquecidas (C++):** Calendarios dinámicos, clima reactivo, temporizadores con recompensas visuales y mascotas virtuales de escritorio.
2. **Protocolo de "Winks" y Guiños Multimedia en Red / Radio Mesh:** Transmisión ultraligera por el aire ($1\text{ a }4\text{ bytes}$) que desencadena animaciones a pantalla completa y efectos de audio sincronizados en los dispositivos receptores (inspirado en la interacción social de los mensajeros de los años 2000).
3. **Bindings de Lua para Creadores (`cbdos.lottie`):** Capacidad de crear y distribuir aplicaciones animadas independientes (`.luapp`) desde la tarjeta MicroSD sin necesidad de recompilar el firmware del sistema.

---

## 📅 2. Aplicaciones Nativas Enriquecidas (Casos de Uso C++)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CASOS DE USO DE LOTTIE EN APPS                    │
├──────────────────────────┬──────────────────────────────────────────────────┤
│ Aplicación / Módulo      │ Comportamiento Visual Vectorial                  │
├──────────────────────────┼──────────────────────────────────────────────────┤
│ 📅 Calendario & Eventos  │ Confeti en cumpleaños, fuegos artificiales, hojas│
│                          │ de calendario que pasan volando en 3D.           │
├──────────────────────────┼──────────────────────────────────────────────────┤
│ ☀️ Widget del Clima      │ Sol con rayos giratorios, nubes de lluvia con    │
│                          │ relámpagos, nieve cayendo en bucle continuo.     │
├──────────────────────────┼──────────────────────────────────────────────────┤
│ 🐣 Mascota Virtual       │ Expresiones reactivas al tacto, estado de ánimo  │
│    (Tamagotchi CBDos)    │ según batería y música sonando con auriculares.  │
├──────────────────────────┼──────────────────────────────────────────────────┤
│ ⏱️ Pomodoro / Tareas     │ Campana que salta al terminar el tiempo, medallas│
│                          │ doradas y cohetes al completar objetivos.        │
└──────────────────────────┴──────────────────────────────────────────────────┘
```

---

## 💬 3. Protocolo de "Winks" Multimedia en Chat Mesh / Radio

En redes tácticas de bajo ancho de banda (LoRa, ESP-NOW o Mesh TLV), transmitir archivos de video o audio por el aire es inviable. El sistema de **Winks** resuelve esto mediante un modelo de **renderizado e impacto local**:

```
[DISPOSITIVO EMISOR]                                       [DISPOSITIVO RECEPTOR]
  ┌───────────────────────┐                                  ┌───────────────────────┐
  │ Usuario pulsa Wink 04 │                                  │ Paquete recibido:     │
  │ "Beso / Corazón"      │                                  │ TYPE: WINK, ID: 0x04  │
  └───────────┬───────────┘                                  └───────────┬───────────┘
              │                                                          │
              │ Payload de Radio: [TAG_WINK][0x04] (2 bytes)             ▼
              └───────────────────────────────────────────────► ┌───────────────────────┐
                                                                │ 1. Abre Overlay Modal │
                                                                │ 2. Carga Lottie #04   │
                                                                │ 3. Suena beso.mp3     │
                                                                │ 4. Cierre automático  │
                                                                └───────────────────────┘
```

### 3.1 Estructura del Paquete TLV de Wink
```cpp
struct WinkMessageTLV {
    uint8_t tag;        // TLV_TAG_WINK = 0x88
    uint8_t length;     // Longitud = 2 bytes
    uint8_t wink_id;    // ID del Wink (0x01 a 0xFF)
    uint8_t flags;      // Flags (0x01: Con sonido, 0x02: Vibración)
};
```

---

## 📜 4. Especificación de la API de Scripting en Lua (`cbdos.lottie`)

Para permitir que la comunidad desarrolle aplicaciones animadas en archivos `.luapp` sin tocar C++, se expone el módulo `cbdos.lottie` en el `LuaBridge`:

### 4.1 Catálogo de Funciones Lua

| Función Lua | Descripción | Parámetros |
| :--- | :--- | :--- |
| `cbdos.lottie.create(parent, path, w, h)` | Instancia un widget Lottie en pantalla | `parent`: contenedor LVGL<br>`path`: ruta al `.json` o `.tvg`<br>`w, h`: dimensiones en píxeles |
| `lottie:play()` | Inicia la reproducción en bucle | Ninguno |
| `lottie:pause()` | Pausa la animación en el frame actual | Ninguno |
| `lottie:set_progress(val)` | Salta a un punto específico de la animación | `val`: número flotante de `0.0` a `1.0` |
| `lottie:set_speed(speed)` | Modifica la velocidad de reproducción | `speed`: multiplicador (`1.0` normal, `2.0` doble) |
| `lottie:destroy()` | Libera el draw buffer de la PSRAM | Ninguno |

### 4.2 Ejemplo de Aplicación en Lua (`/sdcard/apps/reloj_pomodoro.luapp`)
```lua
-- Reloj Pomodoro con Campana Animada en Lua
local screen = cbdos.ui.get_active_screen()
local card = cbdos.ui.create_card(screen, 300, 400)

local anim_campana = cbdos.lottie.create(card, "/sdcard/assets/lottie/bell.json", 180, 180)
anim_campana:play()

local lbl_timer = cbdos.ui.create_label(card, "25:00")
cbdos.ui.set_font(lbl_timer, "montserrat_24")

cbdos.sys.on_timer(25 * 60 * 1000, function()
    cbdos.audio.play("/sdcard/audio/alarm_bell.mp3")
    anim_campana:set_speed(2.0) -- Sacudir campana más rápido
    cbdos.ui.set_text(lbl_timer, "¡Tiempo Cumplido!")
end)
```

---

## ⚡ 5. Gestión de Memoria PSRAM y Rendimiento

1. **Destrucción Segura de Draw Buffers:**
   - Cada widget Lottie reserva un buffer ARGB8888 de $W \times H \times 4$ bytes en PSRAM (ej. $200 \times 200 \times 4 = 160\text{ KB}$).
   - Al cerrarse la vista o invocarse `onDestroy()`, el draw buffer se destruye de forma estricta mediante `lv_draw_buf_destroy()` para evitar fugas de memoria (*memory leaks*).
2. **Prioridad de Tareas en FreeRTOS:**
   - La decodificación de audio (`AudioHAL` / Helix) corre en el **Core 0**.
   - El renderizado de LVGL y ThorVG corre en el **Core 1**, garantizando que el audio jamás sufra tartamudeos (*stuttering*) mientras se procesan gráficos vectoriales pesados.

---

## 🗓️ 6. Plan de Implementación por Fases

- [x] **Hito 1 (Completado):** Activación de ThorVG / Lottie en ESP32-P4 y validación en hardware con `LottieTestView`.
- [ ] **Hito 2:** Implementación del catálogo de Winks en la app de Radio / Chat Mesh.
- [ ] **Hito 3:** Integración del widget `cbdos.lottie` en el `LuaBridge` (`core/src/lua/LuaBridge.cpp`).
- [ ] **Hito 4:** Creación del primer paquete de Widgets animados (Calendario, Clima, Mascota).
