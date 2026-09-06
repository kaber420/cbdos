# 🎛️ Especificación Técnica: Ecosistema StreamDeck, MacroPad & BadUSB Dual (CBDos v0.2.1)

## 📌 1. Visión General
Este subsistema dota a **CBDos** de una plataforma unificada para el control de estaciones de trabajo, streaming en vivo y automatizaciones mediante la emulación **USB HID (Teclado/Mouse/MacroPad)** y **USB CDC (Serial)**. 

La arquitectura centraliza todas las capacidades en una sola API agnóstica (`cbdos::hid`), permitiendo que múltiples herramientas convivan sin duplicación de código:

1. **StreamDeck / MacroPad Táctil:** Controlador visual en vivo para streaming (OBS Studio, Twitch, YouTube, Kick) y atajos de software de edición.
2. **Teclado & Touchpad Virtual:** Periférico de control de emergencia en pantalla IPS de 4.3" a 60 FPS.
3. **BadUSB Clásico ("Ducky Tonto"):** Intérprete estándar de plantillas DuckyScript (`.dd`).
4. **BadUSB Smart Interactivo:** Flujos de automatización con lógica condicional en **Lua** con confirmación por LEDs o canal serie bidireccional.

---

## 🏗️ 2. Arquitectura Unificada y Ecosistema de Aplicaciones

```text
                                  ┌────────────────────────┐
                                  │   API CENTRAL CBDOS    │
                                  │     cbdos/hid.hpp      │
                                  └───────────▲────────────┘
                                              │
       ┌────────────────────────┬─────────────┴──────────┬────────────────────────┐
       │                        │                        │                        │
┌──────┴──────┐          ┌──────┴──────┐          ┌──────┴──────┐          ┌──────┴──────┐
│  STREAMDECK │          │  TOUCHPAD / │          │  BADUSB     │          │  LUA SMART  │
│  MACROPAD   │          │  KEYBOARD   │          │  DUCKY (.dd)│          │  RUNNER     │
└─────────────┘          └─────────────┘          └─────────────┘          └─────────────┘
  Rejilla táctil           Control mouse/           Ejecuta scripts          Automatiza con
  para OBS, Mute,          teclas de PC             estándar de              lógica condicional
  Clips y Audio            en pantalla              inyección rápida         y feedback en vivo
```

---

## 🎛️ 3. Aplicación StreamDeck / MacroPad Táctil (OBS, Twitch, YouTube, Kick)

Aprovechando la resolución de **480×800** y LVGL 9.5:

### 3.1. Capacidades Principales:
* **Rejillas Configurables:** Cuadrículas dinámicas (3×3, 3×4 o 4×5 botones) con iconos PNG/BMP, etiquetas de texto y colores de estado.
* **Control de Escenas & Fuentes en OBS:** Conmutación de escenas, cambio de cámara y disparo de alertas usando atajos globales (`F13` - `F24`, combinaciones `Ctrl+Alt+Shift`).
* **Control de Audio con Feedback Visual:** Botones conmutables (Toggle) para Mute/Unmute de micrófono (Rojo = Mute, Verde = Live) y ajuste de volumen de Discord/Spotify.
* **Soundboard Integrado:** Disparo de efectos de sonido y memes en directo.
* **Doble Modo de Conexión:**
  * **Modo USB Directo:** Envío de teclas/macros por cable sin requerir software instalado en la PC.
  * **Modo Wi-Fi / OBS WebSocket:** Control bidireccional por red para sincronizar el estado real de OBS en la pantalla de CBDos.

### 3.2. Ejemplo de Configuración en Lua (`/sd/config/streamdeck.lua`):
```lua
return {
    pages = {
        {
            name = "Streaming OBS",
            grid = { cols = 3, rows = 4 },
            buttons = {
                { label = "CAMARA 1", icon = "/sd/icons/cam.png",  key = "F13", color = 0x2196F3 },
                { label = "PANTALLA", icon = "/sd/icons/desk.png", key = "F14", color = 0x3F51B5 },
                { label = "MUTE MIC", icon = "/sd/icons/mic.png",  key = "F15", toggle = true, color = 0xF44336 },
                { label = "CLIP NOW", icon = "/sd/icons/clip.png", key = "F16", color = 0x4CAF50 },
                { label = "BRB WAIT", icon = "/sd/icons/brb.png",  key = "F17", color = 0xFF9800 },
                { label = "VOL MUSIC", action = "media_mute",      icon = "/sd/icons/vol.png", color = 0x9C27B0 }
            }
        }
    }
}
```

---

## 🦆 4. Subsistema BadUSB: Modo Clásico & Modo Smart

### 4.1. Modo Clásico ("Ducky Tonto")
* Interpreta directamente archivos con sintaxis DuckyScript (`.dd`) almacenados en la tarjeta MicroSD.
* Operación rápida y universal sin requerir software adicional en la máquina anfitriona.

### 4.2. Modo Smart (Lua Interactivo)
Permite construir scripts con lógica condicional, bucles y validación de estado antes de proceder al siguiente paso:

```lua
-- Ejemplo: Automatización interactiva con confirmación por LEDs
hid.press_gui("r")
hid.delay(300)
hid.type("powershell -WindowStyle Hidden\n")
hid.delay(500)

-- Inyección de comando con señalización por CapsLock
local cmd = "$found = Test-Path 'C:\\backup.zip'; " ..
            "$w = New-Object -ComObject WScript.Shell; " ..
            "if ($found) { $w.SendKeys('{CAPSLOCK}') } else { $w.SendKeys('{SCROLLLOCK}') }\n"

hid.type(cmd)

-- Espera confirmación del host
local event = hid.wait_led_event(3000) -- timeout en ms

if event == hid.LED_CAPSLOCK then
    ui.show_toast("Elemento detectado. Continuando flujo...")
    hid.type("Write-Output 'OK' > \\\\.\\COM4\n")
else
    ui.show_toast("Elemento no encontrado. Abortando.")
end
```

---

## 🔄 5. Mecanismos de Confirmación y Feedback (BadUSB)

### A. Canal de Retorno por LEDs (Caps/Num/Scroll Lock)
* **Principio:** Cero configuración en el host. No requiere abrir puertos serie ni permisos elevados.
* **Mecanismo:** El script inyectado conmuta el estado de los LEDs de bloqueo. El stack USB de CBDos detecta el paquete `SET_REPORT` del host y notifica al motor de Lua.
* **Codificación:** Patrones binarios breves de conmutación para validar etapas (ej. *Host Listo*, *Operación Exitosa*, *Fallo*).

### B. Canal Bidireccional por CDC (Puerto Serie Virtual)
* **Principio:** Enumeración compuesta (`USB HID Keyboard + USB CDC ACM`).
* **Mecanismo:** El teclado inyecta comandos que redirigen la salida hacia el puerto serie COM/TTY creado por el propio ESP32.
* **Visualización:** La pantalla de CBDos muestra en tiempo real la salida de comandos, logs y estado de ejecución.

---

## 📜 6. APIs y Bindings de Lua (`hid.*` y `ducky.*`)

```lua
-- Motor Ducky Clásico
ducky.load_file("/sd/payloads/windows_recon.dd")
ducky.set_default_delay(50)
ducky.run()

-- Motor HID Directo
hid.press_key("RETURN")
hid.press_combo({"CTRL", "ALT", "t"})
hid.type("uname -a\n")
hid.mouse_move(50, -20)
hid.mouse_click("LEFT")
local led_state = hid.get_leds()
```

---

## 🎨 7. Vistas de Usuario en CBDos (LVGL 9.5)

* **StreamDeck View:** Rejilla táctil de botones de macros con iconos y estados interactivos.
* **Touchpad & Teclado View:** Área táctil para control de cursor, barra de scroll y teclado virtual QWERTY.
* **Selector de Plantillas BadUSB:** Explorador de archivos `.dd` y `.lua` almacenados en la MicroSD.
* **Monitor Serie Integrado:** Terminal en vivo para observar las respuestas recibidas vía CDC.
