# Especificación de Diseño: Cliente SSH y Herramienta de Campo en CBDos

**Estado:** Borrador de Arquitectura  
**Target:** ESP32-P4 (Guition JC4880P443C) y ESP32-S3 (JC3248W535)  
**Dependencias Core:** LVGL 9.5, C++ agnóstico, mbedTLS / libssh2 o WolfSSH  
**Ubicación sugerida:** `docs/drafts/draft_ssh_client_and_field_admin.md`

---

## 1. Visión General y Casos de Uso

El cliente SSH de CBDos está diseñado como una herramienta portátil de administración y diagnóstico en campo para técnicos, sysadmins y operadores de red WISP/ISP.

### Casos de Uso Principales:
1. **Administración Rápida de CPEs / Routers / APs:**
   - Reinicio remoto de antenas y routers (Ubiquiti airMAX/UniFi, MikroTik RouterOS, OpenWrt, TP-Link Pharos, Cisco IOS, Linux).
   - Aplicación de comandos de contingencia o reseteo de interfaces de red sin requerir una laptop.
2. **Terminal Interactiva Portátil (ANSI / VT100):**
   - Acceso interactivo completo a shells (`bash`, `sh`, `ash`, `RouterOS CLI`) con renderizado en pantalla táctil con teclado virtual y soporte para teclados Bluetooth/USB.
3. **Automatización One-Tap (Acciones Rápidas):**
   - Ejecución por lotes o con un solo toque de perfiles preconfigurados almacenados en MicroSD en formato JSON o scripts Lua.
4. **Integración con Scripting Lua:**
   - Exposición de APIs de SSH a través del runtime Lua de CBDos (`cbd.ssh.connect()`, `cbd.ssh.exec()`).

---

## 2. Arquitectura del Sistema (Cumplimiento de la Pureza Arquitectónica)

De acuerdo con las **Reglas 7 (Offline-First)** y **Regla 8 (Pureza de `core/`)**, la funcionalidad SSH se estructura en capas estrictamente desacopladas:

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

## 3. Interfaces de Abstracción en `core/hal/`

### 3.1 `ISshClientBackend.hpp`

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

## 4. Estructura de Perfiles de Host (`/sdcard/system/ssh_hosts.json`)

Para facilitar el trabajo en campo, los dispositivos frecuentes se guardan en la MicroSD:

```json
{
  "version": 1,
  "hosts": [
    {
      "id": "cpe-torre-norte",
      "name": "Ubiquiti NanoStation AC",
      "ip": "192.168.1.20",
      "port": 22,
      "user": "ubnt",
      "auth_type": "password",
      "password_vault_id": "cpe_default",
      "quick_actions": [
        { "name": "Reboot Antenna", "cmd": "reboot" },
        { "name": "Check Signal / AirView", "cmd": "ubntbox mca-status" }
      ]
    },
    {
      "id": "mikrotik-core",
      "name": "MikroTik RB4011",
      "ip": "192.168.88.1",
      "port": 22,
      "user": "admin",
      "auth_type": "pubkey",
      "key_path": "/sdcard/system/keys/id_ed25519",
      "quick_actions": [
        { "name": "Reboot RouterOS", "cmd": "/system reboot" },
        { "name": "Check Interfaces", "cmd": "/interface print brief" }
      ]
    }
  ]
}
```

---

## 5. Diseño de Interfaz de Usuario (LVGL 9.5)

### 5.1 Pantalla Principal de la App SSH:
1. **HeaderBar de CBDos:**
   - Indicador de estado Wi-Fi (con conexión bajo demanda).
   - Botón de desconexión rápida y estado de la sesión actual.
2. **Selector de Hosts / Pestaña de Conexión Rápida:**
   - Lista táctil de dispositivos guardados con iconos de fabricante (Ubiquiti, MikroTik, Linux, Genérico).
   - Entrada manual: Formulario para IP, Puerto, Usuario y Clave.
3. **Panel de Quick-Actions:**
   - Botones grandes para ejecutar comandos de un solo toque sin abrir la terminal.
   - Diálogo modal con barra de progreso y log del resultado del comando.
4. **Pantalla de Terminal Interactiva:**
   - Vista de texto monoespaciado con scroll táctil suave.
   - Soporte de secuencias de escape ANSI (colores y cursor básico).
   - Teclado en pantalla contextual con teclas especiales (`Ctrl`, `Esc`, `Tab`, `Alt`, flechas de dirección).

---

## 6. Integración con el Motor de Scripting Lua

El subsistema de Lua de CBDos puede exponer la API de SSH para crear scripts personalizados en la MicroSD:

```lua
-- Ejemplo: reboot_all_cpes.lua
local cpe_list = {"192.168.1.20", "192.168.1.21", "192.168.1.22"}

for _, ip in ipairs(cpe_list) do
    print("Conectando a " .. ip .. "...")
    local session, err = cbd.ssh.connect({
        host = ip,
        port = 22,
        user = "ubnt",
        password = "secretpassword",
        timeout = 3000
    })

    if session then
        print("Enviando orden de reinicio...")
        local output, code = session:exec("reboot")
        print("Resultado: " .. tostring(code))
        session:close()
    else
        print("Fallo de conexión en " .. ip .. ": " .. err)
    end
end
```

---

## 7. Plan de Implementación por Fases

| Fase | Tarea | Componentes |
| :--- | :--- | :--- |
| **Fase 1: HAL & Backend** | Integrar `libssh2` o `wolfSSH` con mbedTLS en BSP, implementar `ISshClientBackend`. | `core/hal/ISshClientBackend.hpp`, `bsp/` |
| **Fase 2: Modo No Interactivo (Exec)** | Crear `SshSessionManager` con capacidad para ejecutar comandos únicos y procesar retorno. | `core/services/ssh/` |
| **Fase 3: UI Quick-Actions** | Crear vista en LVGL 9.5 para lista de hosts y ejecución de perfiles desde MicroSD. | `core/ui/views/SshQuickActionView.hpp` |
| **Fase 4: Terminal Interactiva ANSI** | Crear widget de terminal VT100 en LVGL 9.5 con teclado virtual extendido. | `core/ui/widgets/LvglTerminal.hpp` |
| **Fase 5: Binding Lua** | Exponer `cbd.ssh` en la máquina virtual Lua de CBDos. | `core/scripting/lua_ssh_bindings.cpp` |

---

## 8. Consideraciones de Seguridad y Almacenamiento

- Las contraseñas de perfiles pueden almacenarse cifradas con una clave maestra del sistema o solicitarse en el momento de la conexión.
- Soporte para claves privadas OpenSSH (`Ed25519`, `RSA 2048/4096`) almacenadas en `/sdcard/system/keys/` con permisos y passphrase opcional.
- Limpieza garantizada de buffers de memoria (zeroize) tras cerrar cada sesión criptográfica.
