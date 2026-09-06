# Propuesta de Arquitectura: Lua Sandbox y Dinamismo Web para el Navegador Vectorial (v0.1.2)

**Estado:** 💡 Propuesta de Arquitectura & Especificación de Seguridad  
**Target:** CBDos (ESP32-P4 / ESP32-S3)  
**Subsistemas:** Engine V-DOM, Runtime Lua 5.4, LVGL 9.5  
**Ubicación Oficial:** `docs/proposals/proposal_vector_browser_lua_sandbox.md`  

---

## 1. Visión General y Objetivos

El **Navegador Vectorial Alternet (v0.1.2)** permite visualizar sitios web ultraligeros en nodos Mesh mediante un Modelo de Documento Vectorial (V-DOM). Para dotar a estas páginas de reactividad, dinamismo e interacción (el equivalente a **JavaScript en la Web tradicional**), se especifica la integración de un **Entorno de Aislamiento Seguro (Lua Sandbox)**.

### Objetivos Principales:
1. **Dinamismo Interactivo:** Permitir que los sitios web de la red Mesh modifiquen elementos del DOM, respondan a eventos táctiles y procesen lógica local en el cliente.
2. **Editor Visual Drag & Drop y Plantillas:** Soporte bidireccional en el Editor Web (SaaS) para diseñar arrastrando widgets o importando/exportando plantillas en código JSON.
3. **Optimización de Red y Memoria (Binario TLV / MsgPack):** El compilador traduce el diseño visual o código a **búferes binarios TLV (`.vdom` / `.mesh`)** de ~140 bytes por página para envío ultrarrápido sobre ESP-NOW/LoRa.
4. **Seguridad Absoluta (Zero-Trust):** Garantizar que ningún script remoto pueda acceder a archivos locales (MicroSD), registros NVS, APIs de sistema, subsistemas de radio cruda ni comandos de hardware.
5. **Protección Anti-Congelamiento:** Evitar que scripts maliciosos o mal optimizados agoten la memoria RAM o bloqueen el bucle de renderizado de LVGL 9.5.
6. **Control de Usuario (Policy Enforcement):** Ofrecer un interruptor global de seguridad en la configuración de CBDos para habilitar, preguntar o deshabilitar la ejecución de scripts web.

---

## 2. Modelo de Aislamiento y Sandboxing (Zero-Trust VM)

La máquina virtual de Lua utilizada por el navegador opera en una instancia `lua_State` **completamente aislada** de la máquina virtual nativa del sistema operativo:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   MÁQUINA VIRTUAL LUA DEL NAVEGADOR                    │
│                      (Sandboxed Lua State - RAM: 64KB)                  │
├────────────────────────────────────────────────────────────────────────┤
│ 🚫 LIBRERÍAS DESTRUIDAS / BLOQUEADAS:                                  │
│   • io.* (Sin acceso a MicroSD ni archivos)                            │
│   • os.* (Sin acceso al sistema ni comandos de shell)                  │
│   • package.* / require (Sin carga de librerías externas o C++)        │
│   • debug.* (Sin inspección de memoria ni hooks internos)              │
│   • cbdos.* (Sin acceso a NVS, Audio, HID, Flasher, etc.)              │
├────────────────────────────────────────────────────────────────────────┤
│ ✅ LIBRERÍAS MATEMÁTICAS Y STRING (INOCUAS):                           │
│   • math.*, string.*, table.*, utf8.*                                  │
├────────────────────────────────────────────────────────────────────────┤
│ ⚡ API EXCLUSIVA Y SEGURA DEL V-DOM (vdom.*):                           │
│   • vdom.getElementById("temp_label"):setText("24.5 °C")               │
│   • vdom.setStyle("card_01", "bg_color", "0x00FF00")                   │
│   • vdom.setTimeout(ms, callback)                                      │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Mecanismos de Protección y Control de Recursos

### 3.1 Eliminación Total de Apis Peligrosas
Al inicializar la `lua_State` para la página web, **no** se ejecuta `luaL_openlibs()`. En su lugar, el cargador C++ registra explícitamente solo las librerías matemáticas y de cadenas, omitiendo por completo los módulos `io`, `os`, `package`, `debug` y `cbdos.*`.

### 3.2 Límite de Instrucciones CPU (Instruction Hook Anti-Bucle Infinito)
Para prevenir cuelgues causados por bucles infinitos (`while true do end`), la VM de Lua registra un hook mediante `lua_sethook()`:
* **Límite máximo por evento:** 50,000 instrucciones virtuales.
* Si un script supera este umbral, el hook interrumpe la ejecución con el error: `"Error: Script excedió el tiempo máximo de CPU permitido"`.

### 3.3 Asignación Restringida de Memoria RAM (Custom Allocator)
El estado de Lua se crea usando `lua_newstate()` con una función de asignación de memoria personalizada (`SandboxedAllocator`):
* **Cuota Máxima:** 64 KB de PSRAM por pestaña o página V-DOM activa.
* Si un script intenta asignar más memoria de la permitida, la asignación falla de forma controlada (`NULL`), impidiendo el agotamiento de memoria del sistema.

### 3.4 Política de Ejecución Configurable en CBDos UI
En los ajustes del sistema se integra una directiva de seguridad:
* **Scripting Web Lua:** `[Desactivado | Preguntar | Activado]`
  * **Desactivado (Default Seguro):** La página renderiza exclusivamente el V-DOM estático.
  * **Preguntar:** Muestra una notificación emergente en LVGL 9.5 antes de ejecutar scripts de un nuevo dominio u origen Mesh.

---

## 4. API Expuesta al Sandbox (`vdom.*`)

El script remoto **solo tiene acceso a la manipulación visual del V-DOM de su propia página**:

```cpp
namespace cbdos::web {

// Funciones expuestas a la tabla global 'vdom' en Lua Sandbox
int l_vdom_get_element(lua_State* L);     // vdom.getElementById(id)
int l_vdom_set_text(lua_State* L);        // node:setText(str)
int l_vdom_set_style(lua_State* L);       // node:setStyle(attr, value)
int l_vdom_set_timeout(lua_State* L);     // vdom.setTimeout(ms, func)
int l_vdom_clear_timeout(lua_State* L);   // vdom.clearTimeout(timer_id)
int l_vdom_navigate(lua_State* L);        // vdom.navigate("nav://target")

} // namespace cbdos::web
```

---

## 5. Formato de Scripts en Páginas V-DOM

### 5.1 Modo Inline (Script Embebido en JSON)
```json
{
  "protocol_version": "0.1.2",
  "meta": {
    "title": "Monitor de Telemetría Node #04"
  },
  "script": "function on_button_press() local lbl = vdom.get('status_lbl') lbl:setText('Actualizando...') vdom.setTimeout(1000, function() lbl:setText('OK - 24.5 °C') end) end",
  "body": [
    {
      "type": "text",
      "id": "status_lbl",
      "content": "Estado: Standby"
    },
    {
      "type": "button",
      "label": "Refrescar Telemetría",
      "on_click": "on_button_press()"
    }
  ]
}
```

### 5.2 Modo Externo (`script_src`)
```json
{
  "meta": {
    "title": "Portal Mesh Interactivo",
    "script_src": "alt://node4.alt/assets/dashboard.lua"
  },
  "body": [ ... ]
}
```

---

## 6. Plan de Implementación por Fases

| Fase | Tarea | Componentes |
| :--- | :--- | :--- |
| **Fase 1: Sandbox VM Core** | Implementar `SandboxedLuaState` con alloc custom y eliminación de libs del SO. | `core/src/web/SandboxedLuaState.cpp` |
| **Fase 2: V-DOM DOM Bindings** | Crear la API `vdom.*` para manipular nodos LVGL 9.5 en RAM. | `core/src/web/VDOMBindings.cpp` |
| **Fase 3: Safety Hooks & Timeout** | Integrar `lua_sethook` para limitar instrucciones a 50,000 ops. | `core/src/web/SandboxedLuaState.cpp` |
| **Fase 4: Configuración UI** | Añadir preferencia `Web Lua Scripting` en `ConfigView`. | `core/src/ui/views/ConfigView.cpp` |
