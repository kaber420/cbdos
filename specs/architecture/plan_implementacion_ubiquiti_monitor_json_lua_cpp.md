# Plan de Implementación: cbdos.json y Ubiquiti Monitor (Lua Cartridge + Native C++ View)

## 1. Contexto y Objetivos
Para completar la aplicación de monitoreo de estaciones y sectores Ubiquiti AirOS (probada ya a nivel HTTP/HTTPS, certificados/cookies y Vault de credenciales), el sistema requiere:
1. **Binding `cbdos.json` en LuaBridge**: Exponer funciones `cbdos.json.decode(json_string)` y `cbdos.json.encode(table)` en el runtime Lua utilizando `cJSON` (nativo y de alta eficiencia en ESP-IDF / Arduino-ESP32) para convertir los payloads de AirOS (`/status.cgi`, ~28 KB) en tablas Lua nativas sin saturar el heap.
2. **Cartucho Dinámico `ubiquiti.lua` (`.cbdapp`)**: Aplicación en Lua que:
   - Consulta el `cbdos.vault` para obtener host/usuario/contraseña sin quemarlos en código.
   - Autentica vía HTTPS POST `/api/auth` (o `/login.cgi`) extrayendo cookies de sesión y tokens anti-CSRF.
   - Descarga `/status.cgi` y decodifica el JSON de telemetría de AP y las estaciones CPE.
   - Muestra en pantalla reactiva (LVGL / canvas / UI) la telemetría del AP (frecuencia, SSID, noise floor, CCQ) y lista de estaciones asociadas (MAC, IP, RSSI, Tx/Rx rate).
3. **Vista Nativa C++ `UbiquitiView`**:
   - Implementada en `core/src/ui/views/UbiquitiView.cpp` heredando de `BaseView`.
   - Integrada con `HeaderBar`, `VaultService` y el cliente HTTP/HTTPS nativo del HAL.
   - Enlazada en el menú principal o suite de herramientas de red.

---

## 2. Fase 1: Bindings `cbdos.json` en LuaBridge

### 2.1. Archivo `core/src/lua/bindings/LuaBindings_Json.cpp`
- Funciones C expuestas a Lua:
  - `cbdos.json.decode(str)`: Parsea `str` con `cJSON_Parse()`. Si falla, retorna `nil, error_msg`. Si es exitoso, recorre recursivamente el árbol de `cJSON` y lo convierte a tipos nativos Lua:
    - `cJSON_Object` $\rightarrow$ Lua table con llaves string.
    - `cJSON_Array` $\rightarrow$ Lua table indexada 1..N.
    - `cJSON_String` $\rightarrow$ Lua string.
    - `cJSON_Number` $\rightarrow$ Lua number (o integer si aplica).
    - `cJSON_True`/`cJSON_False` $\rightarrow$ Lua boolean.
    - `cJSON_NULL` $\rightarrow$ `nil`.
    - Llama a `cJSON_Delete()` inmediatamente para liberar la memoria C.
  - `cbdos.json.encode(val)`: Recorre recursivamente una tabla Lua o valor y genera un string formateado / minificado con `cJSON_PrintUnformatted()`.
- Declaración en `core/src/lua/bindings/LuaBridge_Internal.hpp`:
  `void registerJsonAPI(lua_State* L);`
- Registro en `core/src/lua/LuaBridge.cpp`:
  `registerJsonAPI(L);` y alias global `json`.
- Inclusión en `core/CMakeLists.txt` y verificación de `cJSON.h` en ESP-IDF y PlatformIO.

---

## 3. Fase 2: Cartucho Dinámico `ubiquiti.lua`

### 3.1. Arquitectura del Script Lua
Ubicación: `data/apps/ubiquiti.lua` (o ejecutable desde `/sdcard/apps/ubiquiti.lua`).
- **Paso 1: Vault Check**:
  Verifica si hay credenciales en `cbdos.vault.list("ubnt")` o busca por host. Si no hay, ofrece pantalla de configuración rápida para ingresar IP/Host, usuario y contraseña y guardarlas cifradas.
- **Paso 2: Autenticación HTTPS**:
  Ejecuta `cbdos.http.request({ method = "POST", url = "https://" .. host .. "/api/auth", ... })`.
  Extrae la cabecera `Set-Cookie` y el token CSRF `X-CSRF-ID`.
- **Paso 3: Consulta `/status.cgi` y Decodificación**:
  ```lua
  local res = cbdos.http.request({
      method = "GET",
      url = "https://" .. host .. "/status.cgi",
      headers = {
          ["Cookie"] = session_cookie,
          ["X-CSRF-ID"] = csrf_token
      }
  })
  if res.status == 200 then
      local data, err = cbdos.json.decode(res.body)
      -- procesar AP y data.wireless.sta
  end
  ```
- **Paso 4: UI Reactiva**:
  Usa los bindings de UI/Gfx de Lua para dibujar la barra de estado y tarjetas de clientes CPE con indicadores visuales de señal (dBm) y tasas de transferencia.

---

## 4. Fase 3: Vista Nativa C++ `UbiquitiView`

### 4.1. Estructura de Clases
- `core/src/ui/views/UbiquitiView.hpp` y `core/src/ui/views/UbiquitiView.cpp`.
- Hereda de `BaseView`.
- Interfaz reactiva:
  - Header integrado con `HeaderBar`.
  - Panel superior de telemetría de Radio/AP (SSID, canal, frecuencia, ruido, CCQ).
  - Lista scrollable de CPEs con barras de nivel de señal.
  - Botón de refresco manual o intervalo reactivo seguro.
- Comunicación desacoplada: Se comunica mediante `IHttpClient` y `VaultService` sin bloquear el loop de renderizado de LVGL.

---

## 5. Criterios de Aceptación y Pruebas
1. Compilación 100% limpia en PlatformIO (`bsp/esp32_s3_jc3248`) y ESP-IDF (`bsp/esp32_p4_jc4880`).
2. Validación en script Lua del parseo de un JSON de 28 KB de AirOS verificando integridad y liberación de memoria.
3. Cero fugas de memoria (`cJSON_Delete` garantizado en todos los caminos de retorno).
4. Cero datos simulados o engañosos (Regla 6).
