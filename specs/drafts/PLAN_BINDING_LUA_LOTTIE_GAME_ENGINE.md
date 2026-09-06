# Plan de Implementación: Bindings de Lua y Lottie (ThorVG) para Videojuegos y UI en CBDos

Este documento define el diseño técnico, la API para scripts de Lua (`cbdos.lottie.*`) y la integración del ciclo de renderizado con **LVGL 9.5 (ThorVG)** tanto en **ESP32-P4** como en **ESP32-S3**.

---

## 🎯 Objetivo
Permitir a los desarrolladores y usuarios crear videojuegos 2D interactivos, mascotas y componentes visuales animados escribiendo exclusivamente código **Lua (`.lua`)** y consumiendo archivos vectoriales **Lottie (`.json`)** desde la MicroSD, con renderizado fluido a 60 FPS sin necesidad de recompilar el firmware en C++.

---

## 🧩 1. Especificación de la API de Lua (`cbdos.lottie.*`)

Se expondrá un nuevo módulo en el estado global de Lua con soporte orientado a objetos o basado en handles:

### Métodos de Gestión y Creación:
```lua
-- Crea un objeto Lottie vectorial desde un archivo JSON en la MicroSD o memoria
-- Parámetros: path, ancho, alto, [x, y]
-- Retorna: ID o tabla objeto del Lottie
local hero = cbdos.lottie.create("/sdcard/games/runner/hero.json", 128, 128, 50, 200)

-- Métodos del objeto:
hero:set_pos(x, y)               -- Mueve el objeto en la pantalla
hero:set_size(w, h)              -- Escala el tamaño vectorial
hero:set_src(path)               -- Cambia la animación en caliente (ej. idle -> jump)
hero:set_frame(frame_num)        -- Salta a un fotograma exacto (control de timeline)
hero:get_frame()                 -- Obtiene el fotograma actual
hero:get_total_frames()          -- Retorna el total de fotogramas de la animación
hero:set_loop(true/false)        -- Activa o desactiva la repetición en bucle
hero:play()                      -- Inicia o reanuda la reproducción
hero:pause()                     -- Pausa la reproducción
hero:set_hidden(true/false)      -- Oculta o muestra el objeto
hero:destroy()                   -- Libera el buffer en PSRAM y destruye el widget
```

### Funciones Globales del Módulo:
```lua
cbdos.lottie.clear_all()         -- Destruye todos los objetos Lottie creados por el script
cbdos.lottie.is_supported()      -- Retorna true (validado en P4 y S3)
```

---

## 🛠️ 2. Arquitectura Interna C++ (`LuaBridge_Lottie`)

### Estructura de Control de Instancias:
En `core/src/lua/`:
* Se mantendrá una estructura de control de instancias activas de `lv_obj_t*` creadas con `lv_lottie_create()`.
* La asignación de memoria de rasterizado de ThorVG (`width * height * 4` bytes) se realizará dinámicamente en **PSRAM** (`MALLOC_CAP_SPIRAM`).
* En la limpieza del script Lua (o al salir del juego), `LuaBridge` destruirá automáticamente todos los objetos Lottie asociados para evitar fugas de memoria (*zero memory leaks*).

---

## 📂 3. Cambios Propuestos en el Código

### Componente `core/src/lua/`

#### [MODIFY] `core/src/lua/LuaBridge.hpp`
* Declarar el método privado `static void registerLottieAPI(lua_State* L);`.

#### [MODIFY] `core/src/lua/LuaBridge.cpp`
* Implementar las funciones puente C-Lua:
  * `lua_lottie_create`, `lua_lottie_set_pos`, `lua_lottie_set_size`, `lua_lottie_set_src`, `lua_lottie_set_frame`, `lua_lottie_get_frame`, `lua_lottie_get_total_frames`, `lua_lottie_set_loop`, `lua_lottie_play`, `lua_lottie_pause`, `lua_lottie_destroy`, `lua_lottie_clear_all`.
* Vincular la tabla `cbdos.lottie` y la metatabla del objeto `LottieObject`.
* Añadir `registerLottieAPI(L)` en `LuaBridge::registerAll()`.

---

## 🧪 4. Plan de Verificación

### Compilación Multi-Target Obligatoria:
1. Compilar para **ESP32-S3**:
   ```bash
   pio run -d bsp/esp32_s3_jc3248
   ```
2. Compilar para **ESP32-P4**:
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh
   cd bsp/esp32_p4_jc4880 && idf.py build
   ```

### Pruebas Funcionales en Hardware:
1. Ejecutar un script de prueba Lua (`/sdcard/test_lottie.lua`) que cree un personaje Lottie, lo mueva por pantalla según toques táctiles y cambie entre animaciones.
2. Validar que la tasa de cuadros se mantenga estable a 60 FPS.
3. Verificar que al cerrar el script Lua la memoria PSRAM se libere al 100% sin fragmentación.
