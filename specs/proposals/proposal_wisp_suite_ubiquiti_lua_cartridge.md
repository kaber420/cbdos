# Propuesta Técnica: Suite de Gestión WISP (Ubiquiti AirOS) en Cartuchos Dinámicos Lua++

**Documento:** `specs/proposals/proposal_wisp_suite_ubiquiti_lua_cartridge.md`  
**Categoría:** Proposal / Network & Lua Applications  
**Estado:** 📋 Propuesta Lista para Revisión  
**Fecha:** 24 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Precedencia:** Extensión sobre [`proposal_ssh_client_and_lua_field_admin.md`](proposal_ssh_client_and_lua_field_admin.md), [`plan_implementacion_vault_service.md`](../architecture/plan_implementacion_vault_service.md) y [`especificacion_cliente_api_ubiquiti_airos.md`](../network/especificacion_cliente_api_ubiquiti_airos.md).

---

## 1. Justificación y Visión

Implementar el soporte de antenas y estaciones de telecomunicaciones (**Ubiquiti AirMAX / AirOS**) directamente como una aplicación compilada en C++ dentro del firmware base presenta varias desventajas:
1. **Inflación del Firmware del OS:** Obliga a que el firmware base de CBDos crezca con lógica propietaria de un fabricante específico, restando espacio en la memoria Flash para otras herramientas.
2. **Ciclo Lento de Desarrollo:** Cualquier cambio en la interfaz gráfica, un nuevo campo en el JSON o una actualización de AirOS v8 a v9 requeriría recompilar el kernel de CBDos por completo (`pio run` / `idf.py build`) y flashear por cable.
3. **Falta de Modularidad:** Si un usuario utiliza CBDos para radioafición, hacking de hardware o audio, no necesita tener el código de Ubiquiti ocupando RAM ni espacio en pantalla.

### La Solución Arquitectónica
Desacoplar la aplicación en un **Cartucho Dinámico de Aplicación en Lua (`/sdcard/apps/ubiquiti.lua` o `.cbdapp`)**.
- **CBDos provee únicamente los Servicios Fundacionales (Core Contracts):**
  - Criptografía y credenciales seguras (`cbdos.vault.*`).
  - Cliente HTTPS con cabeceras y cookies (`cbdos.http.*`).
  - Terminal y ejecución interactiva SSH (`cbdos.ssh.*`).
  - Parser rápido JSON (`cbdos.json.*`).
  - Creación de interfaz gráfica LVGL reactiva (`cbdos.ui.*`).
- **La App de Ubiquiti es 100% código Lua:**
  - Se puede editar directamente en la tarjeta MicroSD o con el editor de texto en pantalla de CBDos.
  - Corre tanto en **ESP32-S3** como en **ESP32-P4** sin tocar una sola línea del código C++.
  - Se distribuye, actualiza o desinstala como un archivo independiente.

---

## 2. Los Bindings Necesarios en el Core de CBDos

Para que la aplicación Lua tenga acceso a toda la potencia del hardware y los servicios sin comprometer la seguridad:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   APLICACIÓN LUA: ubiquiti.lua                         │
│   • Gestión de APs     • Monitor de Señal CPEs   • Configuración SSH   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ (Consume Bindings Lua)
┌───────────────────────────────────▼────────────────────────────────────┐
│                  BINDINGS EN EL CORE (core/src/lua/)                   │
│   • cbdos.vault.*   • cbdos.http.*   • cbdos.ssh.*   • cbdos.json.*    │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ (Delega a Servicios Canónicos)
┌───────────────────────────────────▼────────────────────────────────────┐
│                    SERVICIOS CORE Y SUBSISTEMAS                        │
│   • VaultService    • IHttpClient    • ISshClient    • cJSON / LVGL    │
└────────────────────────────────────────────────────────────────────────┘
```

### 2.1. Binding de Bóveda (`cbdos.vault.*`)
Permite guardar y recuperar contraseñas de radios de forma cifrada en hardware sin que el script Lua maneje llaves ni texto plano en disco:
```lua
-- Guardar credencial cifrada
cbdos.vault.set("ubnt", "192.168.1.20", "MiPasswordSeguro")

-- Leer contraseña descifrada en memoria RAM volátil
local pass = cbdos.vault.get("ubnt", "192.168.1.20")

-- Listar dispositivos guardados en el espacio Ubiquiti
local ips = cbdos.vault.list("ubnt")
```

### 2.2. Binding HTTP Extendido (`cbdos.http.*`)
Ampliación de los bindings actuales para admitir cabeceras personalizadas (`X-CSRF-ID`), cookies de sesión y códigos de estado:
```lua
local resp = cbdos.http.request({
    method = "POST",
    url = "https://192.168.1.20/api/auth",
    headers = {
        ["Content-Type"] = "application/x-www-form-urlencoded"
    },
    body = "username=ubnt&password=" .. pass,
    timeout_ms = 5000
})

local csrf_token = resp.headers["X-CSRF-ID"]
local session_cookie = resp.headers["Set-Cookie"]
```

### 2.3. Binding de Parseo JSON (`cbdos.json.*`)
Conversión directa de payloads JSON a tablas nativas de Lua usando la biblioteca `cJSON` ya integrada en el sistema:
```lua
local payload = cbdos.http.get("https://192.168.1.20/status.cgi", {
    headers = { ["X-CSRF-ID"] = csrf_token, ["Cookie"] = session_cookie }
})

local data = cbdos.json.decode(payload.body)
print("Hostname:", data.host.hostname)
print("Frecuencia:", data.wireless.frequency, "MHz")
```

### 2.4. Binding SSH (`cbdos.ssh.*`)
Ya existente en CBDos ([`LuaBindings_Network.cpp`](../../core/src/lua/bindings/LuaBindings_Network.cpp)), listo para interactuar con los CPEs:
```lua
local session = cbdos.ssh.connect({
    host = "192.168.10.45",
    port = 22,
    username = "ubnt",
    password = pass
})

-- Leer parámetro de configuración
local txpower = session:exec("cat /tmp/system.cfg | grep radio.1.txpower")

-- Escribir nuevo valor y aplicar
session:exec("sed -i 's/radio.1.txpower=.*/radio.1.txpower=22/' /tmp/system.cfg")
session:exec("cfgmtd -w -p /etc/ && /usr/etc/rc.d/rc.softrestart save")

session:close()
```

---

## 3. Estructura y Flujo de la App en Lua (`ubiquiti.lua`)

El cartucho de la aplicación se divide en tres pantallas o pestañas manejadas en Lua:

```
[ TAB 1: SECTORES (APs) ]
  • Lista scrollable de APs guardados.
  • Botón [+] para agregar AP (IP, usuario, contraseña) -> Guarda en Vault.
  • Al tocar un AP -> Cambia a TAB 2 y arranca polling HTTPS reactivo.

[ TAB 2: DETALLES Y CLIENTES CPE ]
  • Resumen: SSID, Canal, Frecuencia, Ruido (dBm), Airtime %.
  • Tabla dinámica de CPEs conectados:
    - Hostname del CPE y MAC.
    - Barras de señal (Verde > -65 dBm, Amarillo > -75 dBm, Rojo peor).
    - Estado del cable Ethernet: [100M Full] o [DESCONECTADO] en rojo.
    - Throughput Tx / Rx en Kbps.
  • Al tocar un CPE -> Abre TAB 3 (Modal de Configuración).

[ TAB 3: CONFIGURACIÓN ACTIVA DEL CPE (SSH) ]
  • Panel de telemetría extendida: Distancia en metros, Uptime, CCQ / Capacidad AirMAX.
  • Edición rápida: Slider de potencia TX, Selector de Canal, SSID.
  • Botón "Aplicar Cambios": Envía comandos SSH directamente a `/tmp/system.cfg`.
  • Terminal interactiva embebida: Para correr comandos libres (`ping`, `athstats`, `traceroute`).
  • Botón "Desconectar SSH": Libera la sesión y la memoria RAM del ESP32.
```

---

## 4. Plan de Ejecución por Fases

### Fase 1: Habilitación de Contratos y Bindings en el Core C++
- [ ] **1.1:** Completar `VaultService` en C++ ([`plan_implementacion_vault_service.md`](../architecture/plan_implementacion_vault_service.md)) y modernizar el hash de `LockService`.
- [ ] **1.2:** Exponer `cbdos.vault` a Lua en `core/src/lua/bindings/LuaBindings_Security.cpp`.
- [x] **1.3:** Extender `cbdos.http` en Lua para soportar headers personalizados y cookies de respuesta.
- [ ] **1.4:** Exponer `cbdos.json.decode()` y `cbdos.json.encode()` a Lua usando `cJSON`.

### Fase 2: Servidor Mock de Laboratorio en PC
- [ ] **2.1:** Crear script `tools/mock_airos_server.py` que replique HTTPS `/api/auth` y `/status.cgi` con fixtures realistas descargados localmente.
- [ ] **2.2:** Probar la autenticación y lectura de datos desde scripts Lua directamente en CBDos contra la PC de desarrollo.

### Fase 3: Construcción del Cartucho `ubiquiti.lua`
- [ ] **3.1:** Implementar la máquina de estados de red en Lua (Login $\rightarrow$ Token $\rightarrow$ Polling `/status.cgi`).
- [ ] **3.2:** Construir la interfaz gráfica LVGL reactiva en Lua para APs y lista de clientes CPE.
- [ ] **3.3:** Implementar el módulo de configuración SSH y guardado en flash (`cfgmtd`).

### Fase 4: Empaquetado y Distribución
- [ ] **4.1:** Empaquetar la app como cartucho listo para MicroSD (`/sdcard/apps/ubiquiti.lua`).
- [ ] **4.2:** Probar su ejecución sin modificar ni recompilar el firmware del sistema.
