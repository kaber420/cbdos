# Propuesta de Aplicación: Cliente SSH y Herramienta de Campo en CBDos

**Estado:** 💡 Propuesta de Arquitectura & Aplicación  
**Target:** ESP32-P4 (Guition JC4880P443C) y ESP32-S3 (JC3248W535)  
**Dependencias Core:** LVGL 9.5, C++ agnóstico, mbedTLS / libssh2, Lua 5.4  
**Ubicación Oficial:** `docs/proposals/proposal_ssh_client_and_lua_field_admin.md`  

---

## 1. Visión General y Objetivos

La **App Cliente SSH y Administración de Campo** está concebida como un instrumento táctico portátil para técnicos de red, ingenieros WISP/ISP y administradores de sistemas. Permite interactuar directamente con equipos de infraestructura de red (routers MikroTik, antenas Ubiquiti airMAX/UniFi, switches Cisco, servidores Linux, etc.) desde la pantalla táctil de CBDos, sin requerir llevar una computadora portátil al campo.

### Objetivos Clave:
1. **Administración Rápida de Campo (Field Admin):**
   - Configuración, inspección de estado y reinicio remoto de CPEs y Routers.
   - Perfiles preconfigurados guardados en MicroSD (`/sdcard/system/ssh_hosts.json`).
2. **Automatización Mediante Scripts en Lua (`cbdos.ssh`):**
   - Ejecución por lotes o con un solo toque de rutinas de aprovisionamiento en Lua.
   - Compatibilidad directa con comandos CLI de MikroTik RouterOS y Ubiquiti airOS.
3. **Terminal Interactiva ANSI / VT100:**
   - Renderizado interactivo full-screen en LVGL 9.5 con teclado táctil contextual en pantalla (teclas `Esc`, `Tab`, `Ctrl`, `Alt` y flechas).
4. **Respeto Estricto de la Arquitectura Offline-First:**
   - Red encendida **exclusivamente bajo demanda** al abrir la aplicación o ejecutar un script.

---

## 2. Arquitectura de Software y Desacoplamiento (Pureza de `core/`)

De acuerdo con las **Reglas 7 (Offline-First)** y **Regla 8 (Pureza de `core/`)**, la aplicación se divide en 4 capas estrictas:

```
┌────────────────────────────────────────────────────────┐
│                   CAPA DE APLICACIÓN                   │
│   AppSshClient (LVGL 9.5)  /  Scripting Lua (cbd.ssh)  │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                    CAPA DE SERVICIO                    │
│      core/services/ssh/SshSessionManager.hpp/.cpp      │
│  - Perfiles de Hosts (JSON) en MicroSD                 │
│  - Buffer de Terminal VT100 / ANSI Parser              │
│  - Control de Sesiones y Canales                       │
└───────────────────────────┬────────────────────────────┘
                            │ (Interfaces Abstractas C++)
┌───────────────────────────▼────────────────────────────┐
│                   CAPA HAL (core/hal/)                 │
│  ISshClientBackend.hpp  │  INetworkAdapter.hpp         │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                  CAPA BSP (bsp/...)                    │
│  bsp/.../Libssh2MbedTlsBackend.cpp (ESP32-P4 / S3)     │
│  - Sockets BSD / LwIP sobre Wi-Fi On-Demand            │
│  - Criptografía acelerada por Hardware (AES, SHA, RSA) │
│  - Lectura de claves privadas desde MicroSD (HAL)      │
└────────────────────────────────────────────────────────┘
```

---

## 3. Contratos de Interfaces Abstractas (`core/hal/ISshClientBackend.hpp`)

```cpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>

namespace cbd::hal {

enum class SshAuthType {
    PASSWORD,
    PUBLIC_KEY
};

enum class SshSessionState {
    DISCONNECTED,
    RESOLVING,
    CONNECTING_TCP,
    KEY_EXCHANGE,
    AUTHENTICATING,
    CONNECTED,
    ERROR_AUTH,
    ERROR_TIMEOUT,
    ERROR_SOCKET
};

struct SshAuthCredentials {
    SshAuthType type;
    std::string username;
    std::string password;            // Usado si type == PASSWORD
    std::string privateKeyPath;      // Ruta en MicroSD (ej. "/sdcard/.ssh/id_ed25519")
    std::string passphrase;          // Opcional para clave protegida
};

struct SshConnectParams {
    std::string host;
    uint16_t port = 22;
    SshAuthCredentials auth;
    uint32_t timeoutMs = 10000;
};

using SshDataCallback = std::function<void(const uint8_t* data, size_t len)>;
using SshStateCallback = std::function<void(SshSessionState state, const std::string& message)>;

class ISshClientBackend {
public:
    virtual ~ISshClientBackend() = default;

    virtual bool initialize() = 0;
    virtual void deinitialize() = 0;

    // Control de Conexión
    virtual bool connectSession(const SshConnectParams& params, 
                                SshStateCallback onStateChanged) = 0;
    virtual void disconnectSession() = 0;
    virtual bool isConnected() const = 0;

    // Modo Comando Único (Exec no interactivo)
    virtual bool executeCommand(const std::string& command, 
                                std::string& outResponse, 
                                int& outExitCode, 
                                uint32_t timeoutMs = 5000) = 0;

    // Modo Shell Interactivo
    virtual bool openInteractiveShell(SshDataCallback onDataReceived, 
                                      uint16_t cols = 80, 
                                      uint16_t rows = 24) = 0;
    virtual bool sendInput(const uint8_t* data, size_t len) = 0;
    virtual void resizePty(uint16_t cols, uint16_t rows) = 0;
    virtual void closeShell() = 0;
};

} // namespace cbd::hal
```

---

## 4. API Expuesta al Runtime Lua (`LuaBridge`)

Para permitir la automatización por scripts, el módulo expondrá la tabla `cbdos.ssh` en el runtime de Lua:

### 4.1 Métodos Disponibles en Lua:
* `cbdos.ssh.new()`: Instancia un nuevo objeto cliente SSH.
* `client:connect(host, port, user, password_or_key_path)`: Establece conexión. Retorna `true/false` y mensaje de error.
* `client:exec(command)`: Ejecuta un comando no interactivo. Retorna `output_string, exit_code`.
* `client:disconnect()`: Cierra la sesión y libera sockets.

### 4.2 Ejemplo de Script Lua para MikroTik:
```lua
-- /sdcard/scripts/mikrotik_setup.lua
print("Iniciando aprovisionamiento MikroTik...")

local client = cbdos.ssh.new()
local ok, err = client:connect("192.168.88.1", 22, "admin", "password_segura")

if ok then
    print("Conectado con éxito!")
    
    -- Configurar SSID y Modo Wireless
    local out1, code1 = client:exec("/interface wireless set 0 mode=ap-bridge ssid='Node_CBDos'")
    print("Resultado Wireless:", out1)

    -- Asignar IP de gestión
    local out2, code2 = client:exec("/ip address add address=192.168.88.254/24 interface=bridge")
    print("Resultado IP:", out2)

    client:disconnect()
    cbdos.audio.beep(1000, 200) -- Beep de éxito
else
    print("Error de conexión:", err)
    cbdos.audio.beep(300, 500) -- Beep de error
end
```

### 4.3 Ejemplo de Script Lua para Ubiquiti (airOS):
```lua
-- /sdcard/scripts/ubiquiti_reboot.lua
local client = cbdos.ssh.new()

if client:connect("192.168.1.20", 22, "ubnt", "ubnt") then
    print("Reiniciando antena Ubiquiti airMAX...")
    client:exec("reboot")
    client:disconnect()
end
```

---

## 5. Diseño de Interfaz de Usuario (LVGL 9.5)

La aplicación `SshAppView` ofrecerá dos modos principales:

1. **Modo Conexión y Quick-Actions:**
   - Selector táctil de dispositivos frecuentes (`/sdcard/system/ssh_hosts.json`).
   - Botones rápidos de 1-Tap (*"Reboot MikroTik"*, *"Check Status AirMAX"*, *"Reset Interface"*).
   - Formulario manual para IP, Puerto, Usuario y Clave.

2. **Modo Terminal Interactivo ANSI (VT100):**
   - Canvas/Widget monoespaciado en LVGL 9.5 con procesamiento de colores ANSI.
   - Teclado virtual extendido en la parte inferior de la pantalla (teclas `Esc`, `Tab`, `Ctrl`, `Alt`, `|`, `/`, `-`, `^C`, Flechas).
   - Scroll táctil fluido para navegar por el historial del buffer de terminal.

---

## 6. Plan de Implementación por Fases

| Fase | Descripción | Componentes |
| :--- | :--- | :--- |
| **Fase 1: HAL & Backend** | Adaptación de `libssh2` sobre `mbedTLS` en BSP. Implementación de `ISshClientBackend`. | `core/hal/ISshClientBackend.hpp`, `bsp/` |
| **Fase 2: Modo Exec (Comandos no interactivos)** | Implementar `SshSessionManager` para ejecución de comandos únicos y manejo de timeouts. | `core/services/ssh/` |
| **Fase 3: Binding Lua** | Exponer `cbdos.ssh` en el runtime Lua (`LuaBridge.cpp`). | `core/src/lua/LuaBridge.cpp` |
| **Fase 4: UI Quick-Actions** | Vista LVGL 9.5 para ejecutar acciones guardadas en JSON en MicroSD. | `core/src/ui/views/SshQuickActionView.cpp` |
| **Fase 5: Terminal VT100 Interactiva** | Terminal táctil full ANSI con teclado virtual extendido. | `core/src/ui/widgets/LvglTerminal.cpp` |
