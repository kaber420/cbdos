# 📐 Arquitectura Modular para los Bindings de Lua en CBDos (LuaBridge Modular)

**Fecha:** 28 de Agosto de 2026  
**Versión de CBDos:** v0.2.1  
**Estado:** Documento de Especificación Arquitectónica y Diseño Técnico  
**Ubicación de Código:** `core/src/lua/bindings/` y `core/src/lua/LuaBridge.cpp`

---

## 📌 1. Motivación y Diagnóstico del Estado Actual

Actualmente, el archivo `core/src/lua/LuaBridge.cpp` ha crecido hasta superar las **1,750 líneas de código**, agrupando en un único archivo monolítico:
* Control de Sistema, Delays, Timers y NVS.
* Audio (Beeps, sintetizador, reproducción MP3 y volumen).
* Almacenamiento, MicroSD y Flash (`fs` / `storage`).
* Control de hardware GPIO y UART serial.
* Red Wi-Fi y HTTP.
* Motor gráfico 2D con Framebuffer directo (`cbdos.gfx.*`).
* Widgets de interfaz gráfica LVGL 9.5 (`cbdos.ui.*`).
* Lienzos interactivos (`cbdos.canvas.*`).

### ⚠️ Riesgos del Enfoque Monolítico:
1. **Riesgo de Regresiones Accidentales:** Al agregar o modificar una funcionalidad en la UI o Canvas, es fácil sobrescribir o alterar por error funciones críticas de GFX o Audio (como ocurrió en commits previos con los stubs vacíos).
2. **Dificultad de Auditoría y Mantenimiento:** Navegar un archivo de 1,800 líneas dificulta la revisión de diffs de Git y las revisiones en herramientas externas como OpenCode.
3. **Escalabilidad Limitada:** Incorporar nuevos bindings para sensores, Bluetooth, teclado USB o motores 3D saturaría aún más el archivo central.

---

## 🏗️ 2. Arquitectura Modular Propuesta

Se propone desacoplar los bindings en un directorio dedicado dentro de `core/src/lua/bindings/`, donde cada dominio técnico reside en su propia unidad de compilación aislada:

```
core/
 ├── include/
 │    └── cbdos/
 │         └── lua_bridge.hpp          # API pública agnóstica de registro
 └── src/
      └── lua/
           ├── LuaEngine.cpp           # Gestión del estado de la VM Lua 5.4
           ├── LuaRunner.cpp           # Tarea FreeRTOS y ejecución de scripts
           ├── LuaBridge.cpp           # Orquestador central (apenas ~60 líneas)
           └── bindings/               # Módulos de Bindings Especializados
                ├── LuaBindings_Common.hpp     # Helpers compartidos (conversión de tipos, colores)
                ├── LuaBindings_System.cpp     # cbdos.system.*
                ├── LuaBindings_Audio.cpp      # cbdos.audio.*
                ├── LuaBindings_Storage.cpp    # cbdos.fs.* / cbdos.storage.*
                ├── LuaBindings_GpioUart.cpp   # cbdos.gpio.* / cbdos.uart.*
                ├── LuaBindings_Network.cpp    # cbdos.network.*
                ├── LuaBindings_Gfx.cpp        # cbdos.gfx.* (Direct Framebuffer 2D)
                ├── LuaBindings_UI.cpp         # cbdos.ui.* (LVGL 9.5 Widgets)
                └── LuaBindings_Canvas.cpp     # cbdos.canvas.* (Lienzos embebidos)
```

---

## 🧩 3. Responsabilidad Detallada por Módulo

### 3.1. `LuaBindings_Common.hpp`
Contiene funciones utilitarias *inline* y estructuras comunes:
* Conversión de colores (`colorToRGB565`, extracción de RGB).
* Helpers para extracción segura de parámetros Lua (`checkInt`, `optString`, etc.).
* Tabla de fuentes bitmap (ej. `font5x7`) para renderizado de texto directo.

### 3.2. `LuaBindings_Gfx.cpp` (`cbdos.gfx.*`)
* **Propósito:** Modo pantalla completa de alto rendimiento sin sobrecarga de widgets.
* **APIs Registradas:**
  - `cbdos.gfx.clear(color)` (limpieza y sincronización de doble buffer).
  - `cbdos.gfx.draw_rect(x, y, w, h, color, filled)`
  - `cbdos.gfx.draw_circle(cx, cy, r, color, filled)`
  - `cbdos.gfx.draw_line(x0, y0, x1, y1, color)`
  - `cbdos.gfx.draw_text(x, y, text, color, size)`
  - `cbdos.gfx.touch()` (lectura de coordenadas táctiles puras).
  - `cbdos.gfx.flush()` (sincronización de caché L1/L2 y alternancia DMA).
  - `cbdos.gfx.pause_ui(seconds)` / `resume_ui()`.
  - `cbdos.gfx.width()` / `height()` / `rgb(r, g, b)`.

### 3.3. `LuaBindings_UI.cpp` (`cbdos.ui.*`)
* **Propósito:** Creación y gestión declarativa de componentes gráficos LVGL 9.5.
* **APIs Registradas:**
  - `cbdos.ui.create_window(title)`
  - `cbdos.ui.create_button(parent, text, cb)`
  - `cbdos.ui.create_label(parent, text)`
  - `cbdos.ui.create_card(parent)`
  - `cbdos.ui.create_switch(parent, text, cb)`
  - `cbdos.ui.create_text_input(parent, placeholder, cb)`
  - `cbdos.ui.create_canvas(parent, w, h)`

### 3.4. `LuaBindings_Canvas.cpp` (`cbdos.canvas.*`)
* **Propósito:** Lienzos 2D interactivos incrustados dentro del flujo de UI de LVGL.
* **APIs Registradas:**
  - `cbdos.canvas.fill(canvas, color)`
  - `cbdos.canvas.draw_rect(...)`
  - `cbdos.canvas.draw_circle(...)`
  - `cbdos.canvas.draw_line(...)`
  - `cbdos.canvas.draw_text(...)`
  - `cbdos.canvas.on_touch(canvas, cb)`

### 3.5. `LuaBindings_Audio.cpp` (`cbdos.audio.*`)
* **APIs Registradas:** `beep(freq, ms)`, `play_mp3(path)`, `stop()`, `set_volume(vol)`, `get_volume()`.

### 3.6. `LuaBindings_System.cpp` (`cbdos.system.*`)
* **APIs Registradas:** `delay(ms)`, `millis()`, `free_psram()`, `free_heap()`, `get_battery()`, `get_preference()`, `set_preference()`.

### 3.7. `LuaBindings_Storage.cpp` (`cbdos.fs.*` / `cbdos.storage.*`)
* **APIs Registradas:** `read_file(path)`, `write_file(path, data)`, `file_exists(path)`, `list_dir(path)`, `delete_file(path)`.

### 3.8. `LuaBindings_GpioUart.cpp` (`cbdos.gpio.*` / `cbdos.uart.*`)
* **APIs Registradas:** `pin_mode(pin, mode)`, `digital_write(pin, val)`, `digital_read(pin)`, `uart.write(port, data)`, `uart.read(port)`.

### 3.9. `LuaBindings_Network.cpp` (`cbdos.network.*`)
* **APIs Registradas:** `wifi_status()`, `get_ip()`, `http_get(url)`, `http_post(url, data)`.

---

## 🔌 4. Orquestador Central: `LuaBridge.cpp`

Con esta arquitectura, `LuaBridge.cpp` se convierte en un orquestador limpio y elegante de menos de 70 líneas:

```cpp
#include "LuaBridge.hpp"

// Declaraciones de los módulos especializados
namespace cbdos {
namespace lua_bindings {
    void registerSystemAPI(lua_State* L);
    void registerAudioAPI(lua_State* L);
    void registerStorageAPI(lua_State* L);
    void registerGpioUartAPI(lua_State* L);
    void registerNetworkAPI(lua_State* L);
    void registerGfxAPI(lua_State* L);
    void registerUIAPI(lua_State* L);
    void registerCanvasAPI(lua_State* L);
}
}

void LuaBridge::registerAll(lua_State* L) {
    if (!L) return;

    // Crear la tabla global principal 'cbdos'
    lua_newtable(L);

    // Inyectar cada subdominio de forma modular e independiente
    cbdos::lua_bindings::registerSystemAPI(L);
    cbdos::lua_bindings::registerAudioAPI(L);
    cbdos::lua_bindings::registerStorageAPI(L);
    cbdos::lua_bindings::registerGpioUartAPI(L);
    cbdos::lua_bindings::registerNetworkAPI(L);
    cbdos::lua_bindings::registerGfxAPI(L);
    cbdos::lua_bindings::registerUIAPI(L);
    cbdos::lua_bindings::registerCanvasAPI(L);

    // Establecer como global en la máquina virtual Lua
    lua_setglobal(L, "cbdos");
}
```

---

## 🎯 5. Beneficios Inmediatos

1. **Aislamiento Total de Fallas:** Un cambio o corrección en los widgets de `LuaBindings_UI.cpp` tiene **cero impacto** y no puede tocar los binarios ni el código de `LuaBindings_Gfx.cpp` ni de `LuaBindings_Audio.cpp`.
2. **Facilidad de Compilación:** Compilación paralela (`make -j10` / `ninja`) más rápida al compilar unidades `.cpp` pequeñas en lugar de un único archivo gigante.
3. **Escalabilidad para Plugins y Drivers:** Añadir una nueva categoría (ejemplo: `cbdos.bluetooth.*` o `cbdos.sensors.*`) solo requiere añadir un archivo `LuaBindings_Sensors.cpp` y una línea en `LuaBridge::registerAll()`.
4. **Cumplimiento Estricto de Reglas de Proyecto:** Garantiza pureza arquitectónica y evita que futuras modificaciones manuales o por IA generen regresiones destructivas.
