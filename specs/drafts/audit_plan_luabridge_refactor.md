# Auditoría Forense: Refactorización de LuaBridge (Mapa Exacto de Cambios)

Este documento detalla **exactamente** qué código se movió, hacia dónde se movió, y qué estructuras internas fueron alteradas durante la modularización de `LuaBridge.cpp`. No hay suposiciones; esto es un mapa forense para auditar el código.

## 1. El Orquestador (`core/src/lua/LuaBridge.cpp`)
**Qué se le hizo:** Se vació casi por completo. 
**Estado Actual:** 
- Ya no contiene lógica de negocio ni implementaciones de funciones `lua_*`.
- Solo conserva la clase `LuaBridge`, la inicialización del estado Lua (`luaL_newstate()`), y la función `registerAll(lua_State* L)`.
- **Punto de Auditoría:** Confirmar que `registerAll` llama exactamente a las 11 funciones de registro (`registerSystemAPI`, `registerFSAPI`, `registerGfxAPI`, etc.) y que no falta ninguna.

## 2. El Puente Interno (`core/src/lua/bindings/LuaBridge_Internal.hpp`)
**Qué se le hizo:** Se creó desde cero para compartir variables y dependencias que antes vivían en el mismo archivo monolítico y no necesitaban ser compartidas.
**Código Exacto Extraído/Modificado aquí:**
- `colorToRGB565()`: Era una función estática interna. Ahora es `inline uint16_t colorToRGB565(...)` expuesta globalmente. **(Auditar: que el `inline` no cause múltiples definiciones en el linker).**
- `get_target_parent()`: Era estática. Ahora declarada como `struct _lv_obj_t* get_target_parent(lua_State* L, int argIdx);`. **(Auditar: correcta resolución de forward declaration de `lv_obj_t` sin corromper LVGL).**
- `font5x7`: Era un arreglo `static const uint8_t`. Ahora es `extern const uint8_t font5x7[96][5];` declarado aquí y definido en `LuaBindings_Canvas.cpp`. **(Auditar: riesgo de segmentación si Canvas no se vincula correctamente).**
- Dependencias base de Lua (`lua.h`, `lauxlib.h`, `lualib.h`) centralizadas aquí.

## 3. Desglose Exacto de Módulos (Bindings)

### A. Dominio Gráfico y Canvas (`LuaBindings_Gfx.cpp` y `LuaBindings_Canvas.cpp`)
- **Extraído:** Todas las funciones `lua_gfx_*` y `lua_canvas_*`.
- **Estructura Crítica Alterada:** La función interna `drawChar()` que dependía de `font5x7` fue movida a `LuaBindings_Gfx.cpp`. El arreglo `font5x7` fue definido físicamente en `LuaBindings_Canvas.cpp`. 
- **Auditoría:** Revisar `LuaBindings_Gfx.cpp` línea 142 para confirmar que `drawChar` consume correctamente el `extern font5x7`.

### B. Dominio de UI LVGL (`LuaBindings_UI.cpp`)
- **Extraído:** Más de 50 funciones `lua_ui_*` (botones, labels, callbacks, estilos).
- **Estructura Crítica Alterada:** El helper `lua_ui_create_canvas` tuvo que ser expuesto (dejó de ser `static`) porque la UI necesita poder crear Canvas anidados.
- **Auditoría:** Revisar `registerUIAPI()` para confirmar que `lua_pushcfunction(L, lua_ui_create_canvas)` no está arrojando warnings de "implicit declaration" por falta de cabeceras.

### C. Dominio de Sistema y FS (`LuaBindings_System.cpp` y `LuaBindings_FS.cpp`)
- **Extraído:** `lua_sys_*` y `lua_fs_*`.
- **Estructura Crítica Alterada:** Ninguna a nivel de punteros cruzados. Se movieron en bloque aislando los `#include <esp_system.h>` y `#include <esp_spiffs.h>`.
- **Auditoría:** Revisar que los `yield()` o `vTaskDelay()` dentro de `lua_sys_delay` se hayan llevado las cabeceras de FreeRTOS correctas.

### D. Dominio de Hardware (`LuaBindings_Hardware.cpp`, `LuaBindings_Network.cpp`, `LuaBindings_Audio.cpp`)
- **Extraído:** Funciones de `gpio`, `uart`, `net`, y `audio`.
- **Estructura Crítica Alterada:** Durante la extracción, el script original cortó los bloques `registerNetworkAPI` y `registerAudioAPI`. Esto fue **reparado a mano** añadiendo las funciones de registro faltantes al final de cada archivo respectivo.
- **Auditoría:** Abrir `LuaBindings_Network.cpp` y bajar al final del archivo para verificar visualmente que `void registerNetworkAPI(lua_State* L)` está presente y correctamente cerrado con sus `lua_pushcfunction`.

### E. Dominio de Payload (`LuaBindings_HID.cpp` y `LuaBindings_Ducky.cpp`)
- **Extraído:** `lua_hid_*` y `lua_ducky_*`.
- **Auditoría:** Confirmar que `LuaBindings_Ducky.cpp` importa correctamente el motor subyacente de `DuckyInterpreter.h` sin colisiones de nombres.

---
## Resumen de Tareas para el Usuario (Tú)
Para no hacerte adivinar, esto es lo que necesito que abras y revises físicamente (o me ordenes que lo muestre por pantalla):
1. **Abre `LuaBridge_Internal.hpp`**: Confirma que el casteo de `struct _lv_obj_t*` y el `extern font5x7` te parecen seguros.
2. **Abre `LuaBindings_Canvas.cpp` (Línea 18):** Confirma que el arreglo de la fuente está ahí físicamente y no se cortó.
3. **Abre `LuaBridge.cpp` (Línea 14):** Confirma que la lista de `register...API` está completa y llama a todos los módulos listados arriba.
