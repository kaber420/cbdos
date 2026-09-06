# 🦆 Especificación: Motor BadUSB Dual (Ducky Clásico & Smart Interactivo en Lua)

## 📌 1. Visión General
Este subsistema dota a **CBDos** de capacidades avanzadas de emulación **USB HID (Human Interface Device)** y **USB CDC (Serial)** para actuar como herramienta de automatización, diagnóstico e inyección de comandos, compatible tanto con scripts estándar tipo *DuckyScript* como con flujos interactivos avanzados programados en **Lua**.

---

## 🏗️ 2. Modos de Operación

El motor opera bajo dos modalidades seleccionables:

```text
┌────────────────────────────────────────────────────────────────────────┐
│                     SUBSISTEMA BADUSB DE CBDOS                         │
├───────────────────────────────────┬────────────────────────────────────┤
│ 1. MODO CLÁSICO ("Ducky Tonto")   │ 2. MODO SMART (Lua Interactivo)    │
├───────────────────────────────────┼────────────────────────────────────┤
│ • Interpreta sintaxis .dd clásica │ • Flujo dinámico con lógica (if)   │
│ • Unidireccional a ciegas         │ • Feedback por LEDs de teclado     │
│ • Cero requerimientos de software │ • Feedback bidireccional por CDC   │
│ • Ideal para plantillas genéricas │ • Monitoreo en tiempo real en UI   │
└───────────────────────────────────┴────────────────────────────────────┘
```

---

## 🔄 3. Mecanismos de Confirmación y Feedback

### A. Canal de Retorno por LEDs (Caps/Num/Scroll Lock)
* **Principio:** No requiere abrir puertos serie ni permisos elevados en el sistema anfitrión.
* **Mecanismo:** El script inyectado en el host conmuta el estado de las teclas de bloqueo (`CapsLock` / `ScrollLock` / `NumLock`). El stack USB de CBDos detecta el paquete `SET_REPORT` del host y notifica al motor Lua.
* **Codificación:** Patrones binarios breves de conmutación para validar etapas (ej. *Host Listo*, *Operación Exitosa*, *Fallo*).

### B. Canal Bidireccional por CDC (Puerto Serie Virtual)
* **Principio:** Enumeración compuesta (`USB HID Keyboard + USB CDC ACM`).
* **Mecanismo:** El teclado inyecta comandos que redirigen la salida hacia el puerto serie COM/TTY creado por el propio ESP32.
* **Visualización:** La pantalla de CBDos muestra en tiempo real la salida de comandos, logs y estado de ejecución.

---

## 📜 4. APIs y Bindings de Lua (`hid.*` y `ducky.*`)

### Interfaz del Motor Ducky Clásico
```lua
-- Cargar y ejecutar una plantilla estándar DuckyScript desde la tarjeta SD
ducky.load_file("/sd/payloads/windows_recon.dd")
ducky.set_default_delay(50)
ducky.run()
```

### Interfaz Interactiva Avanzada (Lua Smart)
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

## 🎨 5. Interfaz de Usuario en CBDos (LVGL 9.5)

* **Selector de Plantillas:** Navegador de archivos `.dd` y `.lua` almacenados en la MicroSD.
* **Monitor Serie Integrado:** Ventana de terminal en vivo para observar las respuestas recibidas vía CDC.
* **Indicador de Estado:** Visualización gráfica del estado de la conexión USB y los LEDs del teclado virtual.
