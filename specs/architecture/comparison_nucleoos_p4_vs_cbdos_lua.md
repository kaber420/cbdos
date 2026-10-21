# Análisis Comparativo y Propuestas de Arquitectura: CBDos (`Lua++`) vs. NucleoOS-P4 (`luaapp`)

**Fecha:** 20 de Octubre de 2026  
**Estado:** Documento de Arquitectura y Referencia Técnica  
**Repositorio de Referencia Local:** `references/NucleoOS-P4/`  
**Módulos Auditados en CBDos:** `core/src/lua/`, `core/src/ui/views/LuappView.cpp`, `specs/api/luapp_specification.md`  
**Módulos Auditados en NucleoOS-P4:** `ports/luaapp/`, `ports/lua/`, `docs/LUA_APPS.md`

---

## 1. Resumen Ejecutivo y Filosofía Arquitectónica

Este documento contrasta la implementación real de aplicaciones y scripts Lua en **CBDos** frente al sistema de referencia **NucleoOS-P4**.

### 1.1. La Ventaja Competitiva Innegociable de CBDos: LVGL 9.5 Nativo + Hardware Real
El análisis del código fuente demuestra que **CBDos posee una arquitectura superior para aplicaciones de sistema y herramientas de campo** gracias a dos pilares fundamentales que **se mantienen como núcleo innegociable**:

1. **UI en Modo Retenido con LVGL 9.5 Nativo (`cbdos.ui.*`):**
   - En CBDos (`core/src/lua/bindings/LuaBindings_UI.cpp`), los scripts `.luapp` crean e interactúan con widgets reales de **LVGL 9.5** (`lv_obj_t*`, tarjetas Flexbox, botones, sliders, switches, textareas) integrados con `HeaderBar` y `DefaultTheme`.
   - **Cero Polling y 0% CPU en reposo:** Una vez creada la vista en `on_create(parent)`, LVGL gestiona en C++ nativo el scroll inercial, las animaciones, el foco y el repintado parcial. La máquina virtual de Lua solo se despierta cuando ocurre un evento real (`LV_EVENT_CLICKED`, `LV_EVENT_VALUE_CHANGED`), cumpliendo estrictamente la regla de sistema reactivo y permitiendo que funcione con total fluidez tanto en **ESP32-S3** como en **ESP32-P4**.
2. **Acceso Directo a Periféricos, Red y Ciberseguridad (`LuaBridge`):**
   - Tanto las apps `.luapp` como los scripts ejecutados por consola/runner tienen acceso inmediato en C++ a todo el arsenal de CBDos: **GPIO, UART, USB HID / BadUSB / DuckyScript, Cliente SSH, LAN Recon (ICMP Ping, TCP Port Probe, ARP Scanner), Audio y Almacenamiento**.

### 1.2. ¿Por qué NucleoOS-P4 no usa LVGL en sus Lua Apps?
En NucleoOS-P4, el motor Lua (`ports/luaapp/luaapp.c`) está compilado a **WebAssembly (WAMR)** y corre encerrado dentro de un sandbox (`nv_wasm`).
- Debido a que la memoria lineal de WASM está aislada del firmware C++, **sus apps Lua no pueden instanciar objetos `lv_obj_t*` de LVGL ni acceder a los drivers físicos del sistema**.
- Para sortear esa barrera, NucleoOS tuvo que programar un renderizador 2D por software (`ports/luaapp/gfx.c`) y dibujar sus propios controles en modo inmediato (`ports/luaapp/lib/ui.lua`), reservando hasta **8 MB de RAM por aplicación** (`ram_budget`) y redibujando píxeles por software en cada cuadro.
- Además, su binario **Lua de consola (`ports/lua/nv_lua_glue.c`) es un intérprete CLI virgen sin ningún binding**: solo lee `stdin` y escribe en `stdout` dentro de su emulador de terminal, sin acceso a hardware, red ni UI.

---

## 2. Tabla Comparativa Contrastada (Código Fuente Real)

| Dimensión Técnica | CBDos (`core/src/lua/`) | NucleoOS-P4 (`ports/luaapp` y `ports/lua`) | Veredicto Técnico |
| :--- | :--- | :--- | :--- |
| **Motor de UI Principal** | **LVGL 9.5 Nativo (`cbdos.ui.*`)**: Crea objetos reales `lv_obj_t*` con `DefaultTheme`, layout Flexbox automático, scroll inercial nativo y eventos por interrupción/callback (`LUA_REGISTRYINDEX`). | **Modo Inmediato (IMGUI) en Lua (`ui.lua`)**: Sin LVGL. En cada `nv.draw()` dibuja rectángulos/texto por software sobre un buffer RGB565 y calcula colisiones táctiles (`ui.hit`). | **Gana CBDos.** Integración visual 100% nativa con el SO, 0% uso de CPU en reposo y mínimo consumo de RAM (compatible con ESP32-S3 y ESP32-P4). |
| **Lua CLI / Consola** | **Integrado con `LuaBridge` (`LuaEngine` + `LuaRunner`)**: Cualquier script o comando inline tiene acceso completo a todos los bindings (`cbdos.gpio`, `uart`, `hid`, `ssh`, `net`, `ui`, `gfx`). | **Lua 5.4 WASI Estándar (`ports/lua/`)**: Cero bindings de hardware o sistema. Solo `stdin`/`stdout` y archivos en `/sdcard/home`. Sin `os.execute`. | **Gana CBDos por goleada.** Permite automatización real de hardware, pentesting y administración de campo desde scripts o puerto serie. |
| **Ejecución y Memoria** | **C++ Nativo con PSRAM (`LuaEngine::psramAlloc`)**: Llamadas C directas sin intermediarios. `LuaRunner` usa tarea FreeRTOS dedicada con `lua_sethook` cada 1,000 opcodes para no bloquear el watchdog. | **Doble VM (Lua 5.4 dentro de WAMR AOT/WASM)**: Aislamiento fuerte de memoria, pero requiere toolchain LLVM WASI + `wamrc` y presupuestos de ~8 MB de RAM por app. | **CBDos gana en rendimiento, huella de RAM y simplicidad de toolchain.** NucleoOS gana en aislamiento ante scripts maliciosos de terceros. |
| **Bindings de Hardware y Auditoría** | **Completos (`core/src/lua/bindings/`)**: `gpio` (`pin_mode`, `digital_read/write`), `uart` (`init/read/write`), `hid` (teclado/ratón USB, LEDs CapsLock/NumLock), `ducky`, `ssh` (`connect/exec`), `net` (LAN scan, ping, TCP probe). | **Inexistentes en periféricos físicos**: Solo expone `nv.tone`, `nv.sound`, `nv.speak`, `nv.backlight` y reloj del sistema. Sin GPIO, UART, HID, SSH ni escáner de red. | **Gana CBDos.** NucleoOS está enfocado en consumo doméstico/multimedia; CBDos es una estación técnica y cyberdeck real. |
| **Renderizador 2D (`canvas` / `gfx`)** | **`LuaBindings_Canvas.cpp` y `LuaBindings_Gfx.cpp`**: Primitivas básicas en coordenadas enteras, sin canal Alpha, sin transformaciones matriciales y fuente fija `font5x7` escalada por bloques. | **`ports/luaapp/gfx.c` (600 líneas C)**: Coordenadas `float`, antialiasing en bordes, matriz `push/pop/translate/rotate/scale/shear`, 8 modos de blend, superficies con canal Alpha (`surface_t`), fuentes Montserrat (12..72px) y tracking de filas sucias (`g_dirty_y0..y1`). | **Gana NucleoOS en el motor de dibujo 2D (`gfx.c`).** Sus técnicas de renderizado 2D en C son ideales para enriquecer nuestro widget `cbdos.canvas` / `cbdos.gfx`. |
| **Red e IoT Asíncrono** | **HTTP Síncrono + SSH + LAN Recon Asíncrono**: `http.get/post` espera la respuesta en la tarea del script; el escáner LAN corre en background y se consulta con `net.scanning()` / `net.results()`. | **HTTP Asíncrono con Callbacks + WebSockets + MQTT + mDNS + Home Assistant (`nvrt.lua`)**: Basado en handles no bloqueantes sondeados por el runtime. | **Complementarios:** CBDos domina en SysAdmin/Recon (SSH, ICMP, TCP Probe); NucleoOS aporta un excelente diseño de APIs asíncronas con callbacks para HTTP, WS y MQTT. |
| **Persistencia de Datos por App** | **Archivos crudos (`cbdos.storage.read_file` / `write_file`)**: El desarrollador debe elegir rutas y serializar/parsear manualmente sus datos. | **Key-Value Automático (`nv.save(k, v)` / `nv.load(k, def)`)**: Serializa tablas, números, booleanos y strings en un JSON atómico por app (`store.json.tmp` -> `rename`). | **Gana NucleoOS en ergonomía** para guardar configuraciones y estados de una app en una sola línea. |
| **Empaquetado y Módulos (`require`)** | **Script único `.luapp` con cabecera `@meta` (`LuappManager`)**: Muy práctico para editar un solo archivo en la SD, pero aún no resuelve `require()` relativos a una carpeta de aplicación ni bundles de assets. | **Dual: Carpeta en SD (`/sdcard/home/lua/<app>/main.lua`) + Bundle `.lpk`**: Configura `package.searchers` para permitir `require("modulo")` dentro de la carpeta o paquete comprimido con `miniz`. | **Ambos enfoques deben combinarse en CBDos**: conservar el archivo `.luapp` simple y añadir soporte de carpetas `.luapp` con `require()` local. |
| **Compatibilidad LÖVE 2D** | No implementada actualmente. | **`ports/luaapp/lib/love.lua` (55 KB en Lua puro)**: Traduce la API de LÖVE 2D (`love.load/update/draw`, `love.graphics`, `love.audio`) sobre sus primitivas 2D. | **Excelente inspiración para el modo Canvas/Juegos de CBDos.** |

---

## 3. Propuestas de Evolución para CBDos (Inspiradas en NucleoOS-P4)

Manteniendo intacta nuestra arquitectura base (**LVGL 9.5 nativo en `cbdos.ui.*` + ejecución directa C++ en `LuaBridge`**), se proponen las siguientes **6 mejoras concretas** inspiradas en los aciertos de NucleoOS-P4:

### Propuesta 1: Timers Reactivos Integrados con LVGL (`cbdos.timer.*`)
- **Problema actual en CBDos:** En `LuappView`, cuando `on_create(parent)` termina de construir los widgets LVGL, si la aplicación necesita refrescar periódicamente un sensor UART, consultar el estado de un escaneo LAN (`net.scanning()`) o animar un indicador sin bloquear la UI, no dispone de una API de temporizadores en Lua vinculada al ciclo de vida de la vista.
- **Inspiración (`nvrt.lua` L61-79):** Funciones `after(ms, fn)`, `every(ms, fn) -> id` y `cancel(id)`.
- **Implementación en CBDos:**
  - Crear `cbdos.timer.every(ms, callback)`, `cbdos.timer.after(ms, callback)` y `cbdos.timer.cancel(id)` respaldados por `lv_timer_create` de LVGL 9.5.
  - Asociar todos los timers creados por una `.luapp` al contexto de `LuappView`, de modo que en `LuappView::onDestroy()` se eliminen automáticamente todos los `lv_timer_t` activos (cero fugas de memoria o callbacks huérfanos).

### Propuesta 2: Persistencia Clave-Valor Atómica por Aplicación (`cbdos.storage.save` / `load`)
- **Problema actual en CBDos:** Guardar preferencias de usuario (ej. IP de un servidor SSH, último volumen, contador o tabla de favoritos) en una `.luapp` requiere construir rutas manualmente y parsear texto en Lua.
- **Inspiración (`nvrt.lua` L81-113):** `nv.save(key, value)` y `nv.load(key, default)`.
- **Implementación en CBDos:**
  - Añadir `cbdos.storage.save(key, value)` y `cbdos.storage.load(key, default)` que serialicen automáticamente tipos nativos de Lua (`number`, `string`, `boolean`, `table`) en `/sdcard/apps/.data/<app_id>.json` usando escritura atómica (`.tmp` + `rename`).

### Propuesta 3: Trazas de Error Completas (`luaL_traceback`) y Registro `.last_error`
- **Problema actual en CBDos:** En `LuappView.cpp` (L84, L94, L119) y `LuaBindings_UI.cpp` (L73), las llamadas `lua_pcall(L, nargs, 0, 0)` pasan `0` como índice de función de manejo de errores (`msgh`). Si un callback falla dentro de una función anidada, solo devuelve el mensaje corto sin el *stack traceback* completo.
- **Inspiración (`luaapp.c` L708-725 y L809-812):** Uso sistemático de un wrapper `pcall_tb` con `luaL_traceback` y guardado automático del último error en disco para depuración por consola.
- **Implementación en CBDos:**
  - Incorporar un helper `LuaBridge::pcallWithTraceback(lua_State* L, int nargs, int nresults)` que inyecte `luaL_traceback`.
  - Mostrar el stacktrace completo en la tarjeta roja de LVGL (`LuappView::renderError`) y volcarlo al puerto serie y a `/sdcard/apps/.last_error` para depuración inmediata en caliente.

### Propuesta 4: Soporte de Paquetes en Carpeta y `require()` Local en `LuappManager`
- **Problema actual en CBDos:** `LuappManager::scanApps` descubre archivos `.luapp` individuales en `/sdcard/apps/`. A medida que las aplicaciones crezcan (ej. clientes complejos, suites de red o juegos con sprites), meter todo en un solo archivo limita la modularidad.
- **Inspiración (`luaapp.c` L640-665):** Un custom searcher en `package.searchers` que resuelve `require("ui.panel")` buscando `ui/panel.lua` relativo al directorio de la app.
- **Implementación en CBDos:**
  - Mantener el soporte de archivos únicos `/sdcard/apps/mi_app.luapp` (compatibilidad 100% con lo actual).
  - Ampliar `LuappManager` para que también reconozca directorios `/sdcard/apps/mi_app/main.luapp` y configure `package.path` al abrir la app para permitir `require()` de módulos `.lua` y carga de assets (iconos/sonidos) relativos a su propia carpeta.

### Propuesta 5: Evolución del Renderizador 2D en `cbdos.canvas` y `cbdos.gfx`
- **Problema actual en CBDos:** Nuestro widget `cbdos.ui.create_canvas` (`LuaBindings_Canvas.cpp`) y el modo framebuffer (`LuaBindings_Gfx.cpp`) tienen primitivas básicas sin antialiasing, sin grosor de línea variable, sin transformaciones (`translate/rotate/scale`), sin canal Alpha y usan únicamente la fuente bitmap `font5x7`.
- **Inspiración (`ports/luaapp/gfx.c`):** Renderizador 2D ligero en C (~600 líneas) con soporte de floats, antialiasing de bordes, pila de matrices 2D (`push/pop/translate/rotate/scale`), superficies off-screen con canal Alpha, modos de blend en RGB565 y seguimiento de filas sucias (`dirty_y0..y1`).
- **Implementación en CBDos:**
  - Portar las rutinas matemáticas de dibujo de `gfx.c` (arcos con grosor, rectángulos redondeados con antialiasing, polígonos rellenos, rotación de sprites y renderizado de fuentes proporcionales LVGL/Montserrat sobre buffer RGB565) directamente dentro de nuestro `LuaBindings_Canvas.cpp` y `LuaBindings_Gfx.cpp`.
  - Esto permitirá que una app `.luapp` use **LVGL 9.5** para su estructura general (cabecera, pestañas, botones, listas) y use un `cbdos.canvas` enriquecido para pintar radares Wi-Fi/BLE, analizadores lógicos, gráficas vectoriales o juegos 2D a altos FPS.

### Propuesta 6: Capa Opcional de Compatibilidad LÖVE 2D sobre `cbdos.canvas` / `.cbd`
- **Inspiración (`ports/luaapp/lib/love.lua`):** Una vez mejoradas las primitivas 2D de la Propuesta 5, podemos adaptar un módulo ligero en Lua (`love.lua`) que mapee la API estándar de **LÖVE 2D** (`love.load`, `love.update(dt)`, `love.draw()`, `love.graphics.*`) directamente a nuestro motor C++ nativo, abriendo las puertas a ejecutar juegos y demos del ecosistema LÖVE en CBDos con mucha menor huella de RAM que en NucleoOS.

---

## 4. Hoja de Ruta Sugerida (Priorización)

| Fase | Mejora Propuesta | Impacto | Complejidad | Archivos de CBDos Involucrados |
| :---: | :--- | :---: | :---: | :--- |
| **Fase 1** | **Propuesta 3:** `luaL_traceback` en `LuappView` y `LuaBindings_UI` + log `.last_error` | Alto (DX / Debugging) | Baja | `LuappView.cpp`, `LuaBindings_UI.cpp`, `LuaBridge.hpp` |
| **Fase 2** | **Propuesta 1:** API `cbdos.timer.every` / `after` / `cancel` sobre `lv_timer_t` con auto-limpieza | Muy Alto (Apps reactivas) | Baja-Media | `LuappView.hpp/cpp`, `LuaBindings_System.cpp` (o `LuaBindings_Timer.cpp`) |
| **Fase 3** | **Propuesta 2:** Persistencia `cbdos.storage.save(k, v)` / `load(k, def)` por aplicación | Alto (Ergonomía Lua) | Media | `LuaBindings_FS.cpp` |
| **Fase 4** | **Propuesta 4:** Soporte de carpetas `/sdcard/apps/<id>/main.luapp` y `require()` relativo | Medio-Alto (Modularidad) | Media | `LuappManager.cpp`, `LuappView.cpp` |
| **Fase 5** | **Propuesta 5 y 6:** Motor 2D avanzado en `cbdos.canvas` / `cbdos.gfx` (basado en `gfx.c`) + capa `love` | Alto (Gráficos 2D y Juegos) | Media-Alta | `LuaBindings_Canvas.cpp`, `LuaBindings_Gfx.cpp` |
