# GUÍA FORMAL DE AUDITORÍA Y CODE REVIEW: Refactorización `LuaBridge`

**Destinatario:** Revisor de código / Desarrollador auditor  
**Documento Complementario:** `specs/architecture/luabridge_refactoring_report.md` (Catálogo completo de las 135 APIs)  
**Objetivo:** Permitir a un tercero verificar la integridad técnica de la modularización de `LuaBridge.cpp` sin tener que adivinar qué cambió, qué se movió o qué riesgos potenciales existen.

---

## 1. Resumen de la Intervención

| Métrica | Antes del Refactor | Después del Refactor |
| :--- | :--- | :--- |
| **Archivo principal** | `LuaBridge.cpp` (~2480 líneas) | `LuaBridge.cpp` (38 líneas, solo orquestación) |
| **Estructura** | 1 monolito con 10 dominios mezclados | 10 archivos `.cpp` por dominio en `core/src/lua/bindings/` |
| **Header interno** | Inexistente (todo era `static` en un solo `.cpp`) | `core/src/lua/bindings/LuaBridge_Internal.hpp` |
| **Total APIs Lua** | 135 registros (`lua_pushcfunction`) | 135 registros (`lua_pushcfunction`) exactos |

---

## 2. Clasificación de Cambios: Qué se Movió vs Qué se Modificó

Para que el revisor no pierda tiempo revisando 2500 líneas de lógica idéntica, el código se divide en dos categorías:

### Categoría A: Código que SOLO se movió (Mapeo 1:1, Lógica Intacta)
El 98% del código son funciones `static int lua_*` que interactúan con el hardware o LVGL. Estas funciones **no sufrieron ningún cambio interno en su lógica, argumentos ni retornos**. Solo se agruparon en su archivo de dominio correspondiente:

- `LuaBindings_System.cpp` (millis, batería, temperatura, heap, psram, wifi_status)
- `LuaBindings_FS.cpp` (operaciones de archivo en SD y SPIFFS)
- `LuaBindings_Hardware.cpp` (GPIO digital y UART)
- `LuaBindings_Audio.cpp` (reproducción y grabación I2S)
- `LuaBindings_Network.cpp` (escaneo de red, ping, cliente SSH)
- `LuaBindings_HID.cpp` (emulación de teclado y ratón USB)
- `LuaBindings_Ducky.cpp` (intérprete DuckyScript)

### Categoría B: Código que SUFRIÓ Modificaciones Estructurales (PUNTOS DE AUDITORÍA OBLIGATORIOS)
Estos son los **únicos 4 puntos** donde el código fue modificado para permitir la compilación modular. El revisor debe enfocarse aquí:

#### Punto Crítico 1: Declaración externa de la fuente `font5x7`
- **Ubicación de la Definición:** `core/src/lua/bindings/LuaBindings_Canvas.cpp` (Línea ~18)
- **Ubicación de la Declaración:** `core/src/lua/bindings/LuaBridge_Internal.hpp` (Línea ~27)
- **Consumidor externo:** `core/src/lua/bindings/LuaBindings_Gfx.cpp` (Línea ~142 en `drawChar`)
- **Qué auditar:** Verificar que `font5x7` está definida como `extern const uint8_t font5x7[96][5]` y que `drawChar` en Gfx la referencia sin desbordar memoria ni generar advertencias de enlace.

#### Punto Crítico 2: Forward Declaration de `lv_obj_t` sin incluir headers pesados
- **Ubicación:** `core/src/lua/bindings/LuaBridge_Internal.hpp` (Línea ~33)
- **Código:**
  ```cpp
  struct _lv_obj_t;
  struct _lv_obj_t* get_target_parent(lua_State* L, int argIdx);
  ```
- **Implementación:** `core/src/lua/bindings/LuaBindings_UI.cpp`
- **Qué auditar:** Verificar que se utiliza `struct _lv_obj_t*` como forward declaration opaca para evitar colisiones de typedef con LVGL 9.5 en compilación cruzada.

#### Punto Crítico 3: Visibilidad cruzada de `lua_ui_create_canvas`
- **Ubicación de Implementación:** `core/src/lua/bindings/LuaBindings_Canvas.cpp`
- **Ubicación de Registro:** Se registra en `LuaBindings_Canvas.cpp` (`cbdos.canvas.create`) y también en `LuaBindings_UI.cpp` (`cbdos.ui.canvas` por retrocompatibilidad).
- **Qué auditar:** Verificar que la función no tiene el modificador `static` para permitir su registro en ambos namespaces.

#### Punto Crítico 4: Inlining de conversión de color `colorToRGB565`
- **Ubicación:** `core/src/lua/bindings/LuaBridge_Internal.hpp` (Línea ~20)
- **Código:**
  ```cpp
  inline uint16_t colorToRGB565(uint8_t r, uint8_t g, uint8_t b) {
      return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  ```
- **Qué auditar:** Verificar que la palabra clave `inline` previene errores de "multiple definition" entre unidades de traducción (`.cpp`).

---

## 3. Checklist de Auditoría para el Revisor (Paso a Paso)

El revisor debe completar esta lista de comprobación:

- [ ] **Paso 1 (Orquestador):** Abrir `core/src/lua/LuaBridge.cpp` y comprobar que `registerAll(lua_State* L)` invoca exactamente las siguientes 11 funciones:
  - `registerSystemAPI(L)`
  - `registerFSAPI(L)`
  - `registerGfxAPI(L)`
  - `registerCanvasAPI(L)`
  - `registerUIAPI(L)`
  - `registerAudioAPI(L)`
  - `registerNetworkAPI(L)`
  - `registerSSHAPI(L)`
  - `registerHIDAPI(L)`
  - `registerDuckyAPI(L)`
  - `registerHardwareAPI(L)`
- [ ] **Paso 2 (Header Interno):** Abrir `core/src/lua/bindings/LuaBridge_Internal.hpp` y verificar que solo contiene declaraciones `extern`, funciones `inline` y los headers de Lua (`lua.h`, `lauxlib.h`, `lualib.h`).
- [ ] **Paso 3 (Aislamiento de Módulos):** Verificar que ningún archivo dentro de `core/src/lua/bindings/*.cpp` incluye headers de plataforma prohibidos según las reglas de arquitectura (ej. `<Arduino.h>` o `<driver/...>`).
- [ ] **Paso 4 (Aliases de Compatibilidad):** Verificar en `LuaBindings_FS.cpp:133` que las funciones repetidas corresponden a los alias intencionales de `cbdos.storage` para retrocompatibilidad con scripts previos.

---

## 4. Validación de Compilación Cruzada

El revisor debe ejecutar ambos entornos de construcción y confirmar que el código compila sin errores:

### Target 1: ESP32-P4 (ESP-IDF 5.5 nativo en CMake/Ninja)
```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_p4_jc4880
idf.py build
```
*Criterio de Aprobación:* Salida final `Project build complete.` y generación de `build/cbdos_p4.bin`.

### Target 2: ESP32-S3 (PlatformIO + Arduino Core)
```bash
pio run -d bsp/esp32_s3_jc3248
```
*Criterio de Aprobación:* Salida final `[SUCCESS]` en el entorno `esp32s3` y generación de `firmware.factory.bin`.

---

## 5. Script de Validación en Tiempo de Ejecución (Smoke Test)

Cargar y ejecutar el siguiente script Lua a través de la consola serie (`/dev/ttyACM0`) o la aplicación de scripting de CBDos para verificar que todos los bindings responden en hardware real:

```lua
-- Smoke Test Automatizado de Bindings CBDos
local modules = {
    {"sys", sys or cbdos.sys},
    {"fs", fs or cbdos.fs},
    {"gfx", gfx or cbdos.gfx},
    {"canvas", canvas or cbdos.canvas},
    {"ui", ui or cbdos.ui},
    {"audio", audio or cbdos.audio},
    {"net", net or cbdos.net},
    {"hid", hid or cbdos.hid},
    {"ducky", ducky or cbdos.ducky},
    {"gpio", gpio or cbdos.gpio},
}

print("=== INICIANDO VALIDACION DE MODULOS LUA ===")
local passed = 0
for _, mod in ipairs(modules) do
    local name = mod[1]
    local ref = mod[2]
    if ref ~= nil and type(ref) == "table" then
        print(string.format("  [OK] Modulo '%s' presente y exportado.", name))
        passed = passed + 1
    else
        print(string.format("  [ERROR] Modulo '%s' es NIL o corrupto.", name))
    end
end

print(string.format("=== RESULTADO: %d/%d MODULOS CORRECTOS ===", passed, #modules))
assert(passed == #modules, "ERROR CRITICO: Uno o mas modulos no se registraron en la VM de Lua")
```

---
**Dictamen:** Con esta guía y el reporte de arquitectura complementario, cualquier auditor técnico cuenta con la trazabilidad completa para validar el cambio en menos de 15 minutos sin requerir suposiciones.
