# 📋 Plan de Implementación: Entorno Lua++ y Formato `.luapp`

Este documento detalla el plan arquitectónico y las fases de desarrollo para hacer realidad el ecosistema **Lua++** y el formato de aplicaciones portables **`.luapp`** en **CBDos**.

---

## 🎯 1. Objetivos del Proyecto

1. **Cero Compilación:** Permitir a cualquier desarrollador crear aplicaciones completas e interactivas para CBDos usando exclusivamente Lua y guardándolas en `/sdcard/apps/`.
2. **Auto-Detección en Dashboard:** El sistema escanea la MicroSD al arrancar y genera dinámicamente las tarjetas de las apps `.luapp` en la pantalla de inicio con su nombre, icono y color de acento.
3. **Integración con LVGL 9.5 (`cbdos.ui`):** Proveer funciones en Lua para instanciar componentes visuales modernos con los estilos de `DefaultTheme` sin lidiar con punteros C++.
4. **Acceso al Hardware (`cbdos.*`):** Brindar acceso a audio MP3, puertos serie UART, almacenamiento MicroSD/SPIFFS y telemetría del sistema.

---

## 🧱 2. Componentes Arquitectónicos

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           DASHBOARD VIEW                                │
│       [Browser]  [Files]  [Terminal]  ...  [Winamp Retro (.luapp)]      │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ (Al pulsar tarjeta dinámica)
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        LUAPP VIEW (BaseView)                            │
│  • Crea contenedor aislado LVGL 9.5                                     │
│  • Gestiona ciclo de vida (on_create, on_show, on_hide, on_destroy)     │
│  • Enruta eventos de widgets (clicks, sliders) hacia el Lua State       │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       LUABRIDGE & LUAPPMANAGER                          │
│  • LuappScanner: Parsea metadatos de /sdcard/apps/*.luapp              │
│  • cbdos.ui       -> lv_card, lv_button, lv_label, lv_slider, lv_toast  │
│  • cbdos.audio    -> Playback MP3/AAC en segundo plano                  │
│  • cbdos.uart     -> RX/TX Serial con routers y sensores                │
│  • cbdos.storage  -> listDir, readFile, writeFile                      │
│  • cbdos.system   -> métricas, CPU temp, RAM libre, logs                │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📅 3. Plan de Desarrollo por Fases

### 🔹 Fase 1: Parser de Metadatos y `LuappManager`
* **Módulo:** `core/src/lua/LuappManager.hpp` / `.cpp`
* **Funcionalidad:**
  * Función `scanApps(const char* dirPath)`: Escanea `/sdcard/apps/` buscando archivos `.luapp`.
  * Parser rápido: Lee solo los primeros 512 bytes del archivo para extraer metadatos sin cargar todo el script:
    * `@name`: Nombre visible en la tarjeta.
    * `@icon`: Símbolo LVGL (`LV_SYMBOL_AUDIO`, `LV_SYMBOL_FILE`, etc.) o nombre de icono.
    * `@accent`: Color hexadecimal del icono.
    * `@author`, `@version`, `@description`.
  * Integración con `DashboardView`: El Dashboard consulta a `LuappManager::getDiscoveredApps()` y renderiza las tarjetas de forma dinámica.

---

### 🔹 Fase 2: Vista Contenedora Genérica (`LuappView`)
* **Módulo:** `core/src/ui/views/LuappView.hpp` / `.cpp`
* **Funcionalidad:**
  * Hereda de `BaseView` para integrarse con `UIManager` y `HeaderBar`.
  * En `onCreate(parent)`:
    1. Crea un `lua_State` independiente para la aplicación.
    2. Registra las APIs de `LuaBridge`.
    3. Carga y ejecuta el archivo `.luapp`.
    4. Invoca la función global `on_create(root_container)` de Lua.
  * En `onShow()` / `onHide()` / `onDestroy()`:
    * Invoca los callbacks homónimos de Lua para pausar o liberar recursos.
    * Limpia los manejadores y la memoria de la máquina virtual al salir.

---

### 🔹 Fase 3: Módulo `cbdos.ui` en `LuaBridge`
* **Módulo:** `core/src/lua/LuaBridge_UI.cpp`
* **Funcionalidad:**
  * Crear funciones nativas de envoltura para LVGL 9.5:
    * `cbdos.ui.create_card(parent, w, h)` ➔ Crea `lv_obj_t*` con `DefaultTheme::applyRaisedCard`.
    * `cbdos.ui.create_label(parent, text)` ➔ Crea `lv_label_t*` y devuelve tabla con métodos `:set_text()`, `:set_color()`.
    * `cbdos.ui.create_button(parent, text, cb)` ➔ Crea botón con estilo estándar y registra el callback de Lua en el registro de Lua (`LUA_REGISTRYINDEX`).
    * `cbdos.ui.create_slider(parent, min, max, val, cb)` ➔ Slider interactivo.
    * `cbdos.ui.create_switch(parent, state, cb)` ➔ Switch con estilo.
    * `cbdos.ui.show_toast(msg)` ➔ `UIManager::showToast(msg)`.

---

### 🔹 Fase 4: Módulos `cbdos.uart`, `cbdos.audio` y `cbdos.storage`
* **Módulos:** `LuaBridge_Audio.cpp`, `LuaBridge_UART.cpp`, `LuaBridge_Storage.cpp`
* **Funcionalidad:**
  * Bindings completos para que los scripts en Lua puedan:
    * Reproducir archivos de audio (`cbdos.audio.play_file(path)`).
    * Leer y escribir en puertos serie (`cbdos.uart.write(str)`, `cbdos.uart.read()`).
    * Listar carpetas en la SD (`cbdos.storage.list_dir("/sdcard/music")`).

---

### 🔹 Fase 5: Apps de Prueba y Demostración
Creación de 3 aplicaciones oficiales de ejemplo en `/sdcard/apps/`:
1. **`winamp_mini.luapp`:** Reproductor de música retro con lista de canciones, display de tiempo y botones clásicos.
2. **`sensor_monitor.luapp`:** Monitor de hardware con gráficos de temperatura, memoria RAM libre y tiempo de actividad.
3. **`serial_beacon.luapp`:** Herramienta para enviar ráfagas de comandos AT o payloads por UART.

---

## 🧪 4. Criterios de Aceptación y Validación

- [ ] Un archivo `ejemplo.luapp` copiado a `/sdcard/apps/` aparece de inmediato en el Dashboard al iniciar el sistema.
- [ ] Al pulsar la tarjeta, la aplicación se abre en pantalla completa dentro de CBDos con el `HeaderBar` configurado con su título.
- [ ] Los botones y controles de la interfaz responden al tacto e invocan el código Lua correspondiente sin fugas de memoria (*memory leaks*).
- [ ] Al presionar el botón Atrás del `HeaderBar`, la app se destruye limpiamente (`onDestroy`) y retorna al Dashboard.
- [ ] Compilación multi-target 100% limpia tanto en **ESP32-P4** (`idf.py build`) como en **ESP32-S3** (`pio run`).
