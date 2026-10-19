# Plan de Arquitectura: Bindings Modulares de Lua para el Periférico BLE (`cbdos.ble` / `ble.*`)

**Fecha:** 2026-10-19  
**Estado:** Implementado y Verificado (Multi-Target S3 + P4 OK)  
**Ubicación Objetivo:** `core/src/lua/bindings/LuaBindings_Ble.cpp`, `core/src/lua/bindings/LuaBridge_Internal.hpp`, `core/src/lua/LuaBridge.cpp`  
**Dependencias:** `core/include/cbdos/ble.hpp`, `core/include/cbdos/ble_types.hpp`, `core/src/security/BleShieldService.hpp`

---

## 1. Intención y Alcance Arquitectónico

El objetivo de este documento es especificar cómo exponer las capacidades legítimas del periférico Bluetooth Low Energy (BLE) y del servicio de telemetría pasiva (`BleShieldService`) hacia la máquina virtual Lua 5.4 de CBDos mediante la tabla `cbdos.ble` (y su alias global `ble`).

### Principios Rectorales (Reglas de CBDos)
1. **Desacoplamiento Total de Plataforma (Regla 2):**
   - El archivo de bindings reside en `core/src/lua/bindings/` y **jamás** debe incluir cabeceras de ESP-IDF, Arduino o NimBLE (`NimBLEDevice.h`, `host/ble_gap.h`, etc.).
   - Toda interacción con el hardware ocurre exclusivamente a través del contrato abstracto `cbdos::ble::IBleAdapter` ([`core/include/cbdos/ble.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/ble.hpp)) y del servicio singleton `cbdos::security::BleShieldService` ([`core/src/security/BleShieldService.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/security/BleShieldService.hpp)).
2. **Arranque "Offline-First" (Regla 4):**
   - Registrar los bindings en `LuaBridge::registerAll()` **no debe inicializar** el stack de Bluetooth ni encender la radio.
   - El stack solo se inicializa bajo demanda cuando el script Lua invoca explícitamente `ble.init()`, `ble.scan_start()` o `ble.adv_start()`.
3. **Cero Polling Innecesario y Seguridad de Hilos (Regla 4):**
   - Los procesos de escaneo y telemetría corren en segundo plano en las tareas del stack nativo y actualizan `BleShieldService` de forma reactiva (protegido internamente por `std::mutex m_mutex`).
   - Las funciones de Lua consultan instantáneas seguras por valor (`getDevices()`, `getSpamAlert()`, `getState()`) sin bloquear la UI ni crear timers adicionales.
4. **Honestidad de Hardware y Multi-Target (Regla 3 y Regla 6):**
   - En plataformas sin radio BLE nativa (como ESP32-P4 sin coprocesador inicializado), `cbdos::ble::isAvailable()` devuelve `false`. Los bindings deben reportar `false` o tablas vacías de forma honesta, sin datos simulados ni mocks.

---

## 2. Estructura de Archivos a Crear / Modificar

Siguiendo la arquitectura definida en [`specs/architecture/modular_lua_bridge_architecture.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/modular_lua_bridge_architecture.md):

```
core/
 └── src/
      └── lua/
           ├── LuaBridge.cpp                    # [MODIFICAR] Invocar registerBleAPI(L) y crear alias global "ble"
           └── bindings/
                ├── LuaBridge_Internal.hpp      # [MODIFICAR] Declarar void registerBleAPI(lua_State* L);
                └── LuaBindings_Ble.cpp         # [NUEVO] Implementación aislada del dominio cbdos.ble
```

> **Nota sobre sistemas de compilación:**
> - **ESP32-S3 (PlatformIO):** Compila automáticamente todos los `.cpp` dentro de `core/src/lua/bindings/`.
> - **ESP32-P4 (ESP-IDF):** Verificar `bsp/esp32_p4_jc4880/main/CMakeLists.txt` (o el `CMakeLists.txt` de `core`). Si lista los archivos de `bindings/` manualmente en lugar de usar `GLOB`, añadir `LuaBindings_Ble.cpp` a `SRCS`.

---

## 3. Especificación Detallada de la API Lua (`cbdos.ble` / `ble`)

### 3.1. Estado y Control del Adaptador (`cbdos::ble`)

| Función Lua | Firma / Retorno | Comportamiento Interno en C++ |
| :--- | :--- | :--- |
| `ble.available()` | `() -> boolean` | Retorna `cbdos::ble::isAvailable()`. |
| `ble.init([name])` | `(string?) -> boolean, string?` | Obtiene `auto* a = cbdos::ble::getAdapter()`. Si `!a`, retorna `false, "No adapter"`. Llama a `a->init(name ? name : "CBDos")`. Si falla, retorna `false, a->getLastError()`. |
| `ble.shutdown()` | `() -> nil` | Si `getAdapter()` existe, detiene actividad y llama a `a->shutdown()`. |
| `ble.state()` | `() -> string` | Lee `cbdos::ble::getState()` y retorna `"unavailable"`, `"idle"`, `"scanning"` o `"advertising"`. |
| `ble.set_tx_power(dbm)` | `(integer) -> boolean` | Valida rango (`-12` a `+9` dBm) y llama a `getAdapter()->setTxPower((int8_t)dbm)`. |
| `ble.last_error()` | `() -> string` | Retorna `getAdapter() ? getAdapter()->getLastError() : "No adapter"`. |

---

### 3.2. Emisión de Anuncios GAP Estándar y Crudos (`IBleAdapter::startAdvertising`)

Permite que un script Lua actúe como baliza BLE (iBeacon, Eddystone, sensor IoT personalizado) o emita tramas AD (`Advertising Data` / `Scan Response`) construidas desde Lua.

#### `ble.adv_start(config_or_hex, [scan_rsp_hex], [interval_ms]) -> boolean, string?`
Acepta dos formas de invocación:

1. **Modo Tabla (Recomendado para scripts legibles):**
   ```lua
   local ok, err = ble.adv_start({
       data = "02010606094342446F73", -- Hex string (o string binario crudo si raw = true), máx 31 bytes
       scan_rsp = "",                 -- Opcional: Scan Response en hex (máx 31 bytes)
       name = "CBDos-Sensor",         -- Opcional: Si no se provee 'data', construye AD estándar [Flags 0x06 + 0x09 Complete Local Name]
       interval = 100,                -- Intervalo en ms (20..10240, por defecto 100)
       tx_power = 9,                  -- Potencia en dBm (-12..9, por defecto 127 = default del chip)
       scannable = true,              -- Responder a SCAN_REQ (true/false)
       random_mac = false,            -- Rotar MAC aleatoria estática en esta emisión
       mac = "C2:11:22:33:44:55"      -- Opcional: MAC estática específica de 6 bytes
   })
   ```
2. **Modo Rápido Posicional:**
   ```lua
   local ok, err = ble.adv_start("02010606094342446F73", nil, 100)
   ```

**Reglas de validación en C++ para `lua_ble_adv_start`:**
- Parsear cadenas hexadecimales ignorando espacios, `:` o `-` (helper `parseHexBytes`).
- Validar estrictamente que `len <= 31` y `scanRspLen <= 31` (límite del estándar BLE Legacy Advertising). Si excede 31 bytes, retornar `luaL_error` o `false, "Payload exceeds 31 bytes"`.
- Si el escáner de `BleShieldService` estaba activo, llamar primero a `BleShieldService::getInstance().stopScan()` para respetar el arbitraje de radio única documentado en `ble.hpp`.
- Construir `cbdos::ble::BleAdvParams` en el stack local de la función C; dado que `IBleAdapter::startAdvertising(params)` copia los bytes al controlador HCI antes de retornar, los buffers locales `uint8_t advBuf[31]` y `uint8_t rspBuf[31]` son completamente seguros.

#### `ble.adv_stop() -> boolean`
- Llama a `getAdapter()->stopAdvertising()`.

---

### 3.3. Escaneo Pasivo/Activo y Telemetría (`BleShieldService`)

Conecta Lua con el servicio en segundo plano `cbdos::security::BleShieldService::getInstance()` (el mismo patrón que usa `cbdos.net` con `LanScannerService` en [`LuaBindings_Network.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/bindings/LuaBindings_Network.cpp#L175-L234)):

| Función Lua | Firma / Retorno | Comportamiento Interno en C++ |
| :--- | :--- | :--- |
| `ble.scan_start()` | `() -> boolean` | Inicializa `BleShieldService::getInstance().init()`, llama a `startScan()` y retorna `isScanning()`. |
| `ble.scan_stop()` | `() -> nil` | Llama a `BleShieldService::getInstance().stopScan()`. |
| `ble.scanning()` | `() -> boolean` | Retorna `BleShieldService::getInstance().isScanning()`. |
| `ble.scan_clear()` | `() -> nil` | Llama a `BleShieldService::getInstance().clearDevices()`. |
| `ble.scan_count()` | `() -> integer` | Retorna `BleShieldService::getInstance().getDeviceCount()`. |
| `ble.results()` | `() -> table` | Itera sobre `BleShieldService::getInstance().getDevices()` y construye un array Lua (1-indexed). |

#### Esquema de cada elemento retornado por `ble.results()`:
```lua
{
    mac = "AA:BB:CC:DD:EE:FF",       -- dev.macStr
    name = "AirPods Pro",            -- dev.name
    vendor = "Apple, Inc.",          -- dev.vendor
    details = "...",                 -- dev.details
    category = "Apple AirPods",      -- cbdos::radio::getCategoryName(dev.category)
    tag = "[PODS]",                  -- cbdos::radio::getCategoryTag(dev.category)
    rssi = -54,                      -- dev.rssi (integer)
    tx_power = 7,                    -- dev.txPower (integer)
    is_tracker = false,              -- dev.isTracker (boolean)
    is_camera = false,               -- dev.isCameraDevice (boolean)
    is_random_mac = true,            -- dev.isRandomMac (boolean)
    hits = 12,                       -- dev.hitCount (integer)
    first_seen = 10420,              -- dev.firstSeenMs (integer)
    last_seen = 15890,               -- dev.lastSeenMs (integer)
    raw_hex = "1E FF 4C 00 ...",     -- dev.rawPayload convertido a string hex sin espacios (ej. "1EFF4C00...")
    raw_len = 31,                    -- dev.rawPayloadLen (integer)
    -- Solo si dev.hasBattery == true:
    battery_l = 80,                  -- dev.batteryL (nil si 0xFF)
    battery_r = 85,                  -- dev.batteryR (nil si 0xFF)
    battery_case = 60                -- dev.batteryCase (nil si 0xFF)
}
```

---

### 3.4. Localizador Geiger y Monitor IDS (`BleShieldService`)

Permite crear scripts o mini-apps (`Luapps`) de búsqueda de dispositivos perdidos o monitoreo defensivo del espectro BLE:

| Función Lua | Firma / Retorno | Comportamiento Interno en C++ |
| :--- | :--- | :--- |
| `ble.geiger_start(mac_str, [audio])` | `(string, boolean?) -> boolean` | Parsea `mac_str` (`"AA:BB:CC:DD:EE:FF"`) a `uint8_t mac[6]`, aplica `setGeigerAudioEnabled(audio)` y llama a `setGeigerTarget(mac)`. |
| `ble.geiger_stop()` | `() -> nil` | Llama a `clearGeigerTarget()`. |
| `ble.geiger_status()` | `() -> table` | Retorna `{ active, rssi, proximity, distance_m, tick_ms, audio }`. |
| `ble.ids_status()` | `() -> table` | Consulta `getSpamAlert()` y `getTotalSpamPackets()`. Retorna `{ enabled, alert_active, type, pps, total_packets, description }`. |
| `ble.ids_enable(enabled)` | `(boolean) -> nil` | Llama a `setIdsEnabled(enabled)`. |

---

## 4. Helpers de Implementación Recomendados (`LuaBindings_Ble.cpp`)

Para mantener el código limpio, seguro en memoria y sin dependencias externas, incluir en el namespace anónimo de `LuaBindings_Ble.cpp`:

1. **`parseHexString(const char* hex, uint8_t* out, size_t maxLen, size_t* outLen)`:**
   - Recorre el string ignorando separadores (`' '`, `':'`, `'-'`).
   - Convierte pares de nibbles hexadecimales a `uint8_t`.
   - Devuelve `false` si hay un carácter inválido, un número impar de nibbles o si supera `maxLen`.
2. **`parseMacAddress(const char* macStr, uint8_t outMac[6])`:**
   - Reutiliza `parseHexString` exigiendo exactamente `outLen == 6`.
3. **`bytesToHexString(const uint8_t* data, size_t len)`:**
   - Convierte `rawPayload` en un `std::string` hexadecimal en mayúsculas (`"02011A..."`) para que los scripts Lua puedan inspeccionar, filtrar o retransmitir tramas fácilmente.

---

## 5. Registro en el Orquestador (`LuaBridge.cpp` y `LuaBridge_Internal.hpp`)

### 5.1. En [`core/src/lua/bindings/LuaBridge_Internal.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/bindings/LuaBridge_Internal.hpp)
Añadir junto al resto de declaraciones:
```cpp
void registerBleAPI(lua_State* L);
```

### 5.2. En [`core/src/lua/LuaBridge.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/LuaBridge.cpp)
1. Dentro de `LuaBridge::registerAll(lua_State* L)` (antes de `lua_setglobal(L, "cbdos")`):
   ```cpp
   registerBleAPI(L);
   ```
2. En la sección de aliases globales (debajo de `http`):
   ```cpp
   lua_getglobal(L, "cbdos");
   lua_getfield(L, -1, "ble");
   lua_setglobal(L, "ble");
   ```
3. **Buena práctica de pila Lua:** Limpiar la pila con `lua_pop(L, ...)` o fijar el tope al finalizar `registerAll` para no dejar referencias colgantes de la tabla `cbdos` en el stack inicial de `lua_State`.

---

## 6. Ejemplo de Uso desde un Script Lua

Una vez implementado este plan, cualquier script en `/sd/scripts/` o `/sd/luapps/` podrá operar el periférico BLE tanto para escaneo como para emisión de tramas:

```lua
-- 1. Verificar disponibilidad e inicializar radio BLE bajo demanda
if not ble.available() then
    print("Este dispositivo no tiene radio BLE disponible.")
    return
end

ble.init("CBDos-Lab")

-- 2. Escanear durante 3 segundos usando el servicio en segundo plano
ble.scan_clear()
ble.scan_start()
sys.delay(3000)
ble.scan_stop()

local devices = ble.results()
print("Dispositivos BLE detectados: " .. #devices)
for i, d in ipairs(devices) do
    print(string.format("[%d] %s (%s) RSSI:%d dBm Raw:%s",
        i, d.mac, d.name ~= "" and d.name or d.category, d.rssi, d.raw_hex))
end

-- 3. Emitir un anuncio BLE estándar por 2 segundos
local ok, err = ble.adv_start({
    name = "CBDos-Beacon",
    interval = 100,
    scannable = true,
    random_mac = true
})

if ok then
    sys.delay(2000)
    ble.adv_stop()
else
    print("Error en anuncio BLE: " .. tostring(err))
end
```

---

## 7. Checklist de Verificación (Definition of Done)

- [x] `LuaBindings_Ble.cpp` no incluye ninguna cabecera de plataforma (`NimBLE`, `esp_bt`, etc.), solo cabeceras de `core/`.
- [x] Ninguna función de `LuaBindings_Ble.cpp` usa generadores de datos falsos ni mocks (Regla 6).
- [x] Compilación limpia en **ESP32-S3**: `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_red`.
- [x] Compilación limpia en **ESP32-P4**: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`.
- [x] Actualizar [`specs/api/core_apis_reference.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/api/core_apis_reference.md) con la sección `cbdos.ble` / `ble.*`.
