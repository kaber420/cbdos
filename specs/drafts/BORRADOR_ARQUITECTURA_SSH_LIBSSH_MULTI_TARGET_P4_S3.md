# Especificación de Arquitectura: Cliente SSH Multi-Target (`libssh`) y Motor de Automatización en Campo

**Documento:** `docs/drafts/BORRADOR_ARQUITECTURA_SSH_LIBSSH_MULTI_TARGET_P4_S3.md`  
**Estado:** Borrador de Arquitectura y Especificación de Integración  
**Targets:** 
- **ESP32-P4** (Guition JC4880P443C - ESP-IDF 5.5 nativo)
- **ESP32-S3** (JC3248W535 - PlatformIO / Arduino Core)  
**Stack de Criptografía / Red:** `libssh` (C Estándar) + `mbedTLS` acelerado por hardware  
**Librerías Seleccionadas:** 
- Target P4: `david-cermak/libssh` (ESP Component Registry oficial)
- Target S3: `ewpa/LibSSH-ESP32` (Arduino / PlatformIO)

---

## 📌 1. Visión General y Propósito en Campo

El subsistema SSH de CBDos está concebido como una **herramienta portátil de diagnóstico y aprovisionamiento en campo** para administradores de sistemas, ingenieros de redes y técnicos de telecomunicaciones (WISPs).

Permite interactuar directamente desde la pantalla táctil de CBDos con:
1. **Antenas y CPEs Ubiquiti (airMAX, EdgeOS, UniFi):** Alineación de antenas, consulta de RSSI/CCQ, cambio de canales, provisionamiento inicial y reinicios controlados.
2. **Routers MikroTik (RouterOS v6 / v7):** Ejecución de scripts de configuración rápida, lectura de interfaces, reseteo de rutas o firewall, y diagnósticos sin necesidad de laptop.
3. **SBCs y Servidores Linux (Raspberry Pi, mini PCs, OpenWrt):** Verificación de estado, reinicio de servicios `systemd`, o apertura de terminales de rescate de emergencia.

---

## 🎯 2. Estrategia Multi-Target con `libssh` Unificado

Para cumplir con la **Regla 2 (Verificación Multi-Target Obligatoria)** y la **Regla 8 (Ley de Pureza Arquitectónica de `core/`)**, la gran ventaja de esta elección es que **ambas librerías comparten exactamente la misma API de C (`libssh/libssh.h`)**:

```
                               ┌─────────────────────────────────────────────────────────┐
                               │           UI (LVGL 9.5) / Scripts en Lua                │
                               │  - SshFieldAdminView (Perfiles MikroTik / UBNT / RPi)   │
                               │  - Bindings Lua: ssh.connect(), session:exec(), etc.    │
                               └────────────────────────────┬────────────────────────────┘
                                                            │ C++ Puro (Agnóstico)
                                                            ▼
                               ┌─────────────────────────────────────────────────────────┐
                               │                 ISshClient (Core HAL)                   │
                               │  - connect(config) / disconnect()                       │
                               │  - execute(command, timeoutMs) -> SshExecResult         │
                               │  - openInteractiveShell() / sendInput()                 │
                               └────────────────────────────┬────────────────────────────┘
                                                            │
                                                            ▼
                               ┌─────────────────────────────────────────────────────────┐
                               │     LibsshClient (Adaptador C++ ÚNICO y Compartido)     │
                               │  - Código idéntico: llamadas estándar a <libssh.h>      │
                               │  - Manejo de sesiones, autenticación, canales y PTY     │
                               │  - Opera sobre sockets BSD / LwIP de ambos targets      │
                               └────────────────────────────┬────────────────────────────┘
                                                            │
                               ┌────────────────────────────┴────────────────────────────┐
                               ▼ (ESP32-P4: ESP-IDF 5.5)                                 ▼ (ESP32-S3: PlatformIO / Arduino)
  ┌──────────────────────────────────────────────────────┐  ┌──────────────────────────────────────────────────────┐
  │         Manifiesto ESP-IDF (idf_component.yml)       │  │             Manifiesto PlatformIO (platformio.ini)   │
  │  - Dependencia: david-cermak/libssh                  │  │  - Dependencia: ewpa/LibSSH-ESP32                    │
  │  - Red: ESP-Hosted (WiFi C6 vía SDIO)                │  │  - Red: WiFi nativo S3                              │
  │  - Crypto: mbedTLS HW Accelerated (P4 Crypto Engine) │  │  - Crypto: mbedTLS HW Accelerated                    │
  └──────────────────────────────────────────────────────┘  └──────────────────────────────────────────────────────┘
                               │                                                         │
                               └────────────────────────────┬────────────────────────────┘
                                                            ▼
                                        ┌───────────────────────────────────────┐
                                        │  Misma API C nativa de <libssh.h>:    │
                                        │  - ssh_new()                          │
                                        │  - ssh_connect()                      │
                                        │  - ssh_userauth_password()            │
                                        │  - ssh_userauth_publickey()           │
                                        │  - ssh_channel_new()                  │
                                        │  - ssh_channel_request_exec()         │
                                        │  - ssh_channel_read()                 │
                                        └───────────────────────────────────────┘
```

---

## 🛠️ 3. Integración en los Entornos de Compilación

### 3.1 Target ESP32-P4 (`bsp/esp32_p4_jc4880`)
En el archivo de manifiesto de componentes del BSP (`main/idf_component.yml`):

```yaml
dependencies:
  david-cermak/libssh:
    version: "^0.9.5"
```
*Al ejecutar `idf.py build`, el gestor de componentes de ESP-IDF descarga, configura y compila `libssh` enlazándolo automáticamente con lwIP y los drivers criptográficos de hardware del ESP32-P4.*

### 3.2 Target ESP32-S3 (`bsp/esp32_s3_jc3248`)
En el archivo de configuración del entorno (`platformio.ini`):

```ini
[env:esp32s3_jc3248]
platform = espressif32
framework = arduino
lib_deps =
    ewpa/LibSSH-ESP32 @ ^4.1.0
```

---

## 📐 4. Definición de la Capa de Abstracción HAL (`core/`)

Ubicación del contrato: `core/include/cbdos/hal/ISshClient.hpp`  
*(100% C++ estándar, cero dependencias de FreeRTOS, ESP-IDF o Arduino).*

```cpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <memory>

namespace cbdos::hal {

enum class SshAuthType {
    Password,
    PublicKey
};

enum class SshSessionState {
    Disconnected,
    ConnectingTcp,
    KeyExchange,
    Authenticating,
    Ready,
    ErrorAuthFailed,
    ErrorTimeout,
    ErrorSocket
};

struct SshConfig {
    std::string host;
    uint16_t port{22};
    std::string username;
    SshAuthType authType{SshAuthType::Password};
    std::string password;              // Usado si authType == Password
    std::string privateKeyPath;        // Ruta en MicroSD (ej. "/sdcard/keys/id_ed25519")
    std::string passphrase;            // Opcional para clave privada cifrada
    uint32_t timeoutMs{8000};
};

struct SshExecResult {
    bool success{false};
    int exitCode{-1};
    std::string stdOut;
    std::string stdErr;
    std::string errorMessage;
};

using SshDataCallback = std::function<void(const uint8_t* data, size_t len)>;
using SshStateCallback = std::function<void(SshSessionState state, const std::string& msg)>;

class ISshClient {
public:
    virtual ~ISshClient() = default;

    // Control de ciclo de vida de la conexión
    virtual bool connect(const SshConfig& config, SshStateCallback onStateChanged = nullptr) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Modo 1: Automatización / Scripting (Comando único no interactivo)
    virtual SshExecResult execute(const std::string& command, uint32_t timeoutMs = 10000) = 0;

    // Modo 2: Consola Interactiva (Pseudo-terminal PTY / VT100)
    virtual bool openShell(SshDataCallback onData, uint16_t cols = 80, uint16_t rows = 24) = 0;
    virtual bool sendInput(const uint8_t* buffer, size_t length) = 0;
    virtual void resizePty(uint16_t cols, uint16_t rows) = 0;
    virtual void closeShell() = 0;
};

} // namespace cbdos::hal
```

---

## 📜 5. Integración con el Motor de Scripting en Lua (`cbdos.ssh`)

El runtime de Lua en CBDos expone los bindings nativos para que cualquier script almacenado en la MicroSD (`/sdcard/scripts/net/`) pueda automatizar tareas complejas sin tocar C++.

### 5.1 Especificación de la API Lua
* `ssh.connect(table_config)` -> Retorna un objeto `Session` o `nil, err`.
* `session:exec(cmd, [timeout_ms])` -> Retorna `output_string, exit_code`.
* `session:is_connected()` -> Retorna `boolean`.
* `session:close()` -> Cierra canales y sockets.

### 5.2 Ejemplo Práctico 1: Aprovisionamiento de MikroTik (`mikrotik_setup.lua`)
```lua
print("[CBDos] Conectando a MikroTik RouterOS en 192.168.88.1...")

local session, err = cbdos.ssh.connect({
    host = "192.168.88.1",
    port = 22,
    username = "admin",
    password = "" -- Password de fábrica
})

if not session then
    print("[ERROR] No se pudo conectar: " .. tostring(err))
    return
end

print("[OK] Sesión SSH establecida. Configurando identidad e IP...")

-- Ejecutar comandos en RouterOS
session:exec("/system identity set name=\"NODO-TORRE-NORTE\"")
session:exec("/ip address add address=10.50.0.254/24 interface=ether2")
local out, code = session:exec("/ip address print")

print("[INFO] Direcciones configuradas:")
print(out)

session:close()
print("[CBDos] Aprovisionamiento completado.")
```

### 5.3 Ejemplo Práctico 2: Monitor de Señal Ubiquiti AirOS (`ubnt_align.lua`)
```lua
local session = cbdos.ssh.connect({
    host = "192.168.1.20",
    port = 22,
    username = "ubnt",
    password = "ubnt"
})

if not session then
    print("Error conectando a antena Ubiquiti")
    return
end

print("Leyendo métricas de enlace de la antena...")
local raw_status = session:exec("mca-status | grep -E 'signal|wlanChain|ccq'")
print(raw_status)

session:close()
```

---

## 🖥️ 6. Diseño de la Aplicación de Usuario (`SshFieldAdminView` en LVGL 9.5)

Para permitir uso ágil en campo con una sola mano:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 📡 WiFi: Conectado [UBNT-AP-MGR] │ IP: 192.168.1.25 │ 🔋 85%  [ ❌ Cerrar ] │
├─────────────────────────────────────────────────────────────────────────────┤
│ Perfil: [ Ubiquiti NanoStation AC  ▼ ]   Host: [ 192.168.1.20    ] Puerto: 22│
│ Usuario: [ ubnt            ] Auth: [ Password ▼ ] Clave: [ **********     ]│
│ [ ⚡ CONECTAR AL DISPOSITIVO ]                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🚀 Acciones Rápidas (One-Tap):                                              │
│ ┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐  │
│ │ 🔄 Reiniciar CPE     │ │ 📊 Medir Señal/RSSI  │ │ 📦 Backup Config     │  │
│ └──────────────────────┘ └──────────────────────┘ └──────────────────────┘  │
│ ┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐  │
│ │ 📜 Ejecutar Script...│ │ 💻 Abrir Terminal SSH│ │ ⚙️ Setup Factory Def │  │
│ └──────────────────────┘ └──────────────────────┘ └──────────────────────┘  │
├─────────────────────────────────────────────────────────────────────────────┤
│ Log / Salida del Comando:                                                   │
│ [10:14:02] SSH Conectado a 192.168.1.20 (airOS 8.7.1)                       │
│ [10:14:03] Comando enviado: 'mca-status'                                    │
│ [10:14:03] Signal: -58 dBm (Chain 0: -60, Chain 1: -59) CCQ: 98.4%          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 🔒 7. Políticas de Seguridad y Desacoplamiento (Reglas del Sistema)

1. **Principio Offline-First (Regla 7):**
   - El subsistema SSH no tiene tareas en segundo plano en el arranque.
   - La pila Wi-Fi / red se activa de manera reactiva únicamente cuando el usuario presiona "Conectar" o lanza un script SSH. Al desconectar o salir de la vista, los sockets se destruyen y la interfaz puede volver a reposo.
2. **Almacenamiento de Llaves en MicroSD:**
   - Soporte para llaves privadas OpenSSH (`id_ed25519` o `id_rsa`) leídas directamente desde la tarjeta MicroSD (`/sdcard/system/keys/`).
   - Las contraseñas en perfiles JSON pueden guardarse vacías para solicitarse interactivamente, o encriptadas en el almacenamiento del sistema.
3. **Manejo de Errores y Timeouts de Campo:**
   - Si un CPE o antena se reinicia mientras corre un comando, `ISshClient` maneja de forma asíncrona la caída de la conexión y el timeout sin congelar el hilo principal de LVGL 9.5 ni colapsar el sistema operativo.

---

## 📋 8. Resumen de Próximos Pasos

1. **Implementar interfaz `ISshClient.hpp` en `core/include/cbdos/hal/`.**
2. **Declarar las dependencias en cada BSP:**
   - En ESP32-P4: Añadir `david-cermak/libssh` a `bsp/esp32_p4_jc4880/main/idf_component.yml`.
   - En ESP32-S3: Añadir `ewpa/LibSSH-ESP32` a `bsp/esp32_s3_jc3248/platformio.ini`.
3. **Crear el adaptador compartido `LibsshClient.cpp`** que implementa `ISshClient` llamando a las APIs nativas de `<libssh/libssh.h>`.
4. **Exponer los bindings en el motor Lua** (`core/src/scripting/`).
5. **Crear la vista de interfaz gráfica** `SshFieldAdminView` en LVGL 9.5.
