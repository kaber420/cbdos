# 🧩 Especificación Técnica y Plan: Desacoplamiento de Lua y Comandos Explícitos en la Consola del Sistema

**Fecha:** 2026-10-09  
**Estado:** Aprobado  
**Versión:** `v1.1.0`  
**Ubicación:** `specs/architecture/plan_desacoplamiento_lua_y_comandos_explicitos_consola.md`  

---

## 🎯 1. Diagnóstico y Justificación Técnica

### 1.1 Estado Actual y Deuda Técnica
Actualmente, el despachador de la consola del sistema (`cbdos::cli::dispatch` en `core/src/system/system_cli.cpp`) posee un mecanismo de **fallback ciego**:
* Si una línea no coincide con ningún comando registrado del sistema (`sys:`, `c3:`, `usb:`, `status`, etc.), el texto se entrega automáticamente a `LuaEngine::getInstance().executeString()`.

### 1.2 Problemas Identificados:
1. **Errores Tipográficos Enmascarados:** Si el operador escribe mal un comando del sistema (ej: `stauts`, `tempp`, `helpp`), en lugar de recibir un mensaje claro del sistema operativo (`Comando no reconocido: 'stauts'`), el runtime de Lua se despierta y emite errores crípticos del compilador de Lua (`[SERIAL_CLI_ERR] syntax error near 'stauts'`).
2. **Consumo Innecesario de PSRAM y CPU:** Cualquier ruido eléctrico o ráfaga de bytes espurios recibidos por la UART o USB-Serial activa el compilador de Lua, realizando asignaciones dinámicas en memoria sin ningún propósito.
3. **Pérdida de Jerarquía y Control:** En un sistema operativo profesional, ningún intérprete de scripts debe ejecutarse de fondo de forma implícita. La ejecución de código debe ser siempre **explícita y deliberada** por parte del operador.

---

## 📐 2. Objetivos y Diseño Propuesto

### 2.1 Principios Rectores:
* **Comando Desconocido Determinista:** Ante cualquier entrada no reconocida, el sistema responde de forma limpia y estándar:
  ```text
  [SYS] Comando desconocido: "asdf". Escribe 'help' o '?' para ver la lista de comandos.
  ```
* **Lua como Subsistema Formal de Primera Clase (`lua:` / `lua`):**
  Para ejecutar una expresión o script de Lua, el operador debe precederlo explícitamente con su prefijo:
  ```text
  cbdos> lua: return 21 * 2
  [LUA_OUT] 42

  cbdos> lua: hid.type("ls -la\n")
  [LUA_OUT] OK
  ```
* **Sintaxis y Ayuda Rápida:**
  Si el usuario escribe `lua` o `lua:` sin argumentos:
  ```text
  [LUA] Uso: lua: <código> (ej: lua: return 10 * 5)
  ```
* **Modo Interactivo (REPL con prompt `lua>` - Fase 2):**
  La máquina de estados para sesión interactiva con cambio dinámico de prompt (`lua> ` vs `cbdos> `) queda desacoplada para la capa de consola/terminal interactiva (`hal_console_p4.cpp` / `TerminalView`).

---

## 🏗️ 3. Modificaciones en el Código (`core/src/system/system_cli.cpp`)

1. **Eliminación del Fallback Ciego:**
   - En `dispatch()`: Remover la llamada incondicional a `LuaEngine` al final de la función.
   - En su lugar, emitir con buffer protegido (truncado preventivo a 64 bytes):
     ```cpp
     char errBuf[160];
     snprintf(errBuf, sizeof(errBuf), "[SYS] Comando desconocido: \"%.64s\". Escribe 'help' o '?' para ver la lista de comandos.\n", trimmed.c_str());
     if (ctx.write) ctx.write(errBuf);
     ```

2. **Registro de `cmdLua`:**
   - Registrar `"lua"` en `cbdos::cli::init()` mediante `registerCommand("lua", cmdLua)`.
   - Dado que el despachador reconoce automáticamente coincidencias de prefijo con espacio (`"lua "`) y con dos puntos (`"lua:"`), registrar `"lua"` cubre: `lua: <código>`, `lua:<code>`, `lua <código>` y `lua`.
   - Handler:
     ```cpp
     static void cmdLua(const std::string& args, const CommandContext& ctx) {
         if (args.empty()) {
             if (ctx.write) ctx.write("[LUA] Uso: lua: <código> (ej: lua: return 10 * 5)\n");
             return;
         }
         std::string outRes;
         bool ok = ::LuaEngine::getInstance().executeString(args, &outRes);
         if (ok) {
             if (!outRes.empty() && ctx.write) {
                 ctx.write("[LUA_OUT] " + outRes + "\n");
             }
         } else {
             if (ctx.write) {
                 ctx.write("[LUA_ERR] " + ::LuaEngine::getInstance().getLastError() + "\n");
             }
         }
     }
     ```

3. **Actualización del Manual (`help`):**
   - Incorporar `lua: <script>` en el catálogo formal de comandos de `help`.

---

## 📋 4. Plan de Ejecución Paso a Paso

1. **Paso 1: Modificar `core/src/system/system_cli.cpp`:**
   - Implementar `cmdLua`.
   - Registrar `"lua:"` y `"lua"`.
   - Reemplazar el fallback implícito por el mensaje de comando desconocido.
   - Actualizar el texto de `help`.

2. **Paso 2: Validación de Compilación Dual-Target:**
   - Compilar ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`.
   - Compilar ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`.

3. **Paso 3: Validación en Vivo en Hardware (`/dev/ttyACM0`):**
   - Flashear a ESP32-P4.
   - Probar comando inválido: `stauts` → Debe responder `[SYS] Comando desconocido: "stauts"`.
   - Probar Lua explícito: `lua: return 50 * 2` → Debe responder `[LUA_OUT] 100`.
   - Probar `help` → Debe listar `lua:` formalmente.

---

## 🛡️ 5. Criterios de Éxito
* Ningún error tipográfico despierta al motor Lua.
* Lua solo se ejecuta bajo demanda explícita con `lua: <código>`.
* Cero regresiones en los comandos de hardware (`status`, `temp`, `c3: ping`, etc.) y compatibilidad dual-target verificada.
