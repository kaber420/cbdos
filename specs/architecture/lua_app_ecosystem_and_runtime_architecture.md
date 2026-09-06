# 🚀 Ecosistema de Lua Apps y Runtime de Ejecución en CBDos

**Fecha:** 2026-09-02  
**Versión:** 1.0.0  
**Estado:** Especificación Técnica & Modelo de Extensibilidad  
**Target:** ESP32-P4 (400 MHz, 32 MB PSRAM) · ESP32-S3 (240 MHz, 8 MB PSRAM)  

---

## 🏛️ 1. Filosofía Arquitectónica: Core Limpio vs. Ecosistema de Apps

Para garantizar la **integridad legal, seguridad y máxima tracción comunitaria en GitHub**, CBDos adopta el modelo desacoplado de plataformas exitosas como **Flipper Zero, Android y Pwnagotchi**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       COMUNIDAD / USUARIOS FINALES                          │
│  Descarga y comparte scripts en MicroSD (/sdcard/apps/*.lua)               │
│  • badusb_recon.lua    • ble_spammer.lua    • usb_cdc_terminal.lua         │
│  • lora_messenger.lua  • wifi_survey.lua    • spectrum_waterfall.lua       │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (Carga dinámica sin compilar)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                    CBDos CORE RUNTIME (100% C++ Limpio)                      │
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │  Lua App Launcher & Sandboxed Execution Engine (FreeRTOS Task)        │  │
│  └───────────────────────────────────┬───────────────────────────────────┘  │
│                                      │                                      │
│  ┌───────────────────────────────────┴───────────────────────────────────┐  │
│  │                 CBDos Unified Lua API Bindings Layer                  │  │
│  │   [ ui.* ]      [ hid.* ]     [ usb.* ]     [ radio.* ]    [ sys.* ]  │  │
│  │   Controles     BadUSB &      Host CDC      ESP-NOW, LoRa  Archivos   │  │
│  │   Táctiles      LED Feedback  Plug & Play   WiFi / BLE     MicroSD    │  │
│  └───────────────────────────────────┬───────────────────────────────────┘  │
│                                      │                                      │
├──────────────────────────────────────┴──────────────────────────────────────┤
│                   HAL ABSTRACTO & DRIVERS DE SILICIO                         │
│   ESP32-P4 High-Speed USB Host · MIPI-DSI LVGL 9.5 · SDMMC · ES8311 Audio   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Principios Fundamentales:
1. **Core 100% Limpio y de Propósito General:** El repositorio oficial de CBDos distribuye un sistema operativo para cyberdecks con un entorno de ejecución (*runtime*) robusto, APIs de hardware universales y herramientas de diagnóstico. No incluye herramientas ofensivas empaquetadas en la Flash.
2. **Extensibilidad "Zero-Compilation":** Los usuarios y desarrolladores pueden crear herramientas avanzadas (auditoría, radiofrecuencia, telecomunicaciones, automatizaciones) simplemente escribiendo archivos `.lua` y colocándolos en la MicroSD (`/sdcard/apps/`).
3. **Desarrollo Rápido y Portabilidad:** Un script se escribe en minutos, se prueba en caliente mediante la consola interactiva `/dev/ttyACM0` y corre idéntico en cualquier dispositivo con CBDos.

---

## 📂 2. Estructura de Carpetas en Almacenamiento (MicroSD)

El sistema de archivos escanea `/sdcard/apps/` para listar las aplicaciones disponibles en la interfaz gráfica:

```
/sdcard/
├── apps/
│   ├── BadUSB_Suite/
│   │   ├── app.json            # Metadatos (nombre, autor, versión, icono)
│   │   ├── main.lua            # Código fuente principal de la app
│   │   └── payloads/           # Scripts Ducky .dd o submódulos
│   │       ├── win_recon.dd
│   │       └── lin_enum.dd
│   ├── BLE_Beacon_Spam.lua     # App autocontenida de un solo archivo
│   ├── USB_Serial_Monitor.lua  # Terminal interactiva para dongles USB-C
│   └── LoRa_Mesh_Chat.lua      # Mensajería descentralizada
├── cartridges/                 # Binarios para el Standalone Field Programmer
│   ├── marauder_v1.0.bin
│   └── meshtastic_c3.bin
└── scripts/                    # Scripts rápidos de consola y payloads
```

### 📄 Formato del Archivo de Metadatos (`app.json` opcional):
```json
{
  "name": "BLE Chaos Spammer",
  "version": "1.2.0",
  "author": "kaber420",
  "category": "Wireless",
  "icon": "radio_tower",
  "description": "Herramienta de auditoría y pruebas de proximidad BLE para iOS, Android y Windows.",
  "permissions": ["ble", "ui"]
}
```
*Si la app es un archivo individual `.lua`, los metadatos se extraen automáticamente de los comentarios de cabecera:*
```lua
-- @name: USB CDC Terminal
-- @version: 1.0.0
-- @author: Anonymous
-- @category: Hardware Tools
-- @desc: Monitor serie para módems y dongles USB-C
```

---

## 🎛️ 3. Especificación de APIs de CBDos para Lua

El runtime de CBDos expone un conjunto de módulos estándar optimizados para bajo consumo de memoria y ejecución a 60 FPS:

### 3.1. Módulo `ui` (Interfaz Gráfica Táctil Inmediata)
Permite a cualquier script crear vistas completas sin necesidad de lidiar con la complejidad directa de LVGL en C++:

| Función Lua | Descripción |
| :--- | :--- |
| `ui.clear()` | Limpia el contenedor de la aplicación actual. |
| `ui.add_button(text, callback, [x, y, w, h])` | Crea un botón táctil con estilo cyberpunk y callback de pulsación. |
| `ui.add_label(text, [x, y])` | Añade una etiqueta de texto dinámica con soporte de tipografía neón. |
| `ui.add_log_area([max_lines])` | Crea una consola de texto auto-scrolleable para logs de terminal. |
| `ui.log(text)` | Imprime una línea en el área de log gráfica y en el puerto serie. |
| `ui.add_switch(label, initial_state, on_toggle)` | Añade un switch/toggle interactivo. |
| `ui.add_slider(min, max, current, on_change)` | Control deslizable (para potencia de RF, volumen, etc.). |
| `ui.toast(message, duration_ms)` | Muestra una notificación flotante no intrusiva en pantalla. |

### 3.2. Módulo `hid` (Automatización, BadUSB & Feedback de LEDs)
Control de emulación USB HID compuesto con sincronización reactiva:

| Función Lua | Descripción |
| :--- | :--- |
| `hid.get_leds()` | Retorna tabla `{numlock = bool, capslock = bool, scrolllock = bool}` del Host. |
| `hid.press_key(key_name)` | Envía pulsación de tecla (`"ENTER"`, `"GUI"`, `"CTRL"`, `"ALT"`). |
| `hid.type_string(text, [delay_ms])` | Escribe una cadena de texto respetando la distribución del teclado. |
| `hid.run_ducky(file_path_or_str)` | Parsea y ejecuta un archivo DuckyScript (`.dd`). |
| `hid.mouse_move(dx, dy, [wheel])` | Movimiento analógico relativo del ratón. |
| `hid.mouse_click(button)` | Clic con botón (`"left"`, `"right"`, `"middle"`). |

### 3.3. Módulo `usb` (USB Host CDC Plug & Play por USB-C)
Control bidireccional de dongles, módems y transceptores conectados al puerto USB-C:

| Función Lua | Descripción |
| :--- | :--- |
| `usb.is_device_connected()` | Retorna `true` si hay un periférico USB CDC enumerado en el conector USB-C. |
| `usb.get_device_info()` | Retorna tabla `{vid = 0x303A, pid = 0x1001, product = "ESP32-C3 USB-Serial"}`. |
| `usb.set_baudrate(baud)` | Configura la tasa de baudios de la interfaz CDC-ACM (ej. `115200`). |
| `usb.send(data_bytes_or_string)` | Transmite datos por el endpoint USB Host CDC. |
| `usb.on_receive(callback_function)` | Registra un callback reactivo ejecutado cada vez que llegan datos del periférico. |
| `usb.set_dtr_rts(dtr, rts)` | Conmuta las señales de control virtuales DTR y RTS. |

### 3.4. Módulo `radio` / `wifi` / `ble` (Comunicaciones Inalámbricas)
Acceso a radios internas (coprocesador C6) y externas:

| Función Lua | Descripción |
| :--- | :--- |
| `ble.start_beacon_spam(type_id)` | Inicia ráfaga de paquetes BLE (Apple, Android, Windows). |
| `ble.stop_beacon_spam()` | Detiene la emisión de paquetes publicitarios BLE. |
| `wifi.scan_networks()` | Retorna lista de APs detectados `{ssid, bssid, rssi, channel, auth}`. |
| `espnow.send(mac_str, payload)` | Envía trama directa por protocolo ESP-NOW. |
| `espnow.on_receive(callback)` | Callback al recibir tramas de la red en malla o nodos cercanos. |

### 3.5. Módulo `sys` & `fs` (Sistema Operativo y Archivos)
Utilidades de bajo nivel:

| Función Lua | Descripción |
| :--- | :--- |
| `sys.delay_ms(ms)` | Pausa no bloqueante cediendo tiempo al planificador de FreeRTOS. |
| `sys.get_free_heap()` | Retorna la memoria RAM interna y PSRAM libre en bytes. |
| `fs.read_file(path)` | Lee el contenido completo de un archivo en MicroSD o Flash. |
| `fs.write_file(path, data)` | Escribe o anexa datos a un archivo en MicroSD. |
| `fs.list_dir(dir_path)` | Retorna array con los nombres de archivos en la carpeta. |

---

## 📝 4. Ejemplo Práctico: Plantilla de una Lua App

A continuación se muestra un ejemplo completo de cómo luce una aplicación en Lua (`/sdcard/apps/SmartBadUSB.lua`) que aprovecha la UI y el feedback de LEDs:

```lua
-- @name: Smart Recon BadUSB
-- @version: 1.0.0
-- @author: CommunityDev
-- @desc: Payloads con verificación de terminal abierta mediante CapsLock

local app = {}

function app.init()
    ui.clear()
    ui.add_label("🥷 Smart BadUSB Suite", 10, 10)
    
    ui.add_button("🚀 Lanzar Recon Windows", function()
        ui.log("[*] Inyectando apertura de PowerShell...")
        hid.press_key("GUI r")
        sys.delay_ms(300)
        hid.type_string("powershell -WindowStyle Hidden\n")
        
        ui.log("[*] Esperando sincronización de terminal vía LED...")
        local timeout = 50 -- 5 segundos máx
        local ready = false
        
        while timeout > 0 do
            local leds = hid.get_leds()
            if leds.capslock then
                ready = true
                break
            end
            sys.delay_ms(100)
            timeout = timeout - 1
        end
        
        if ready then
            ui.log("[+] Host sincronizado! Enviando comando final...")
            hid.type_string("whoami /all > $env:TEMP\\recon.txt\n")
            ui.toast("Payload ejecutado con éxito!", 2000)
        else
            ui.log("[-] Timeout: Host no respondió.")
        end
    end)
    
    ui.add_log_area(12)
    ui.log("Sistema listo. Conecta el cable USB-C al objetivo.")
end

-- Punto de entrada ejecutado por el runtime de CBDos
app.init()
```

---

## 🛡️ 5. Sandboxing, Aislamiento y Estabilidad

Para evitar que un error de programación en un script de la comunidad cause un pánico del kernel en el ESP32:

1. **Protección `pcall` en el Runtime:** Toda llamada desde el despachador de eventos a callbacks de Lua se ejecuta bajo un envoltorio seguro (`lua_pcall`). Si un script arroja un error (ej. variable `nil`), el error se muestra en pantalla y la app se detiene de forma limpia, retornando al Dashboard.
2. **Límite de Asignación en PSRAM:** El intérprete Lua asigna tablas y objetos en la PSRAM de 32 MB del ESP32-P4, protegiendo la memoria SRAM interna del sistema operativo.
3. **Watchdog y Yielding:** Los bucles de script cuentan con chequeo de tiempo para evitar bloquear el hilo de refresco de LVGL (garantizando 60 FPS estables).

---

## 🚀 6. Beneficios para la Comunidad y Roadmap de Difusión

- **Publicación en GitHub:** El repositorio de CBDos se mantiene limpio, modular y con foco en arquitectura de sistemas operativos embebidos.
- **Repositorio Complementario (`cbdos-community-apps`):** Se creará un repositorio separado donde la comunidad podrá enviar Pull Requests con sus propias herramientas `.lua`, emuladores, reproductores y payloads.
- **Cero Barreras de Entrada:** Los usuarios no necesitan instalar ESP-IDF, toolchains de RISC-V ni PlatformIO: solo descargan la release de CBDos, la flashean una sola vez, y añaden tantas Lua Apps a su MicroSD como deseen.
