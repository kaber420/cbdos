# 🌙 Especificación de Lua++ y Formato `.luapp` (CBDos Lua Apps)

**Lua++ (Lua Plus Plus)** es el entorno de desarrollo y ejecución de aplicaciones dinámicas de alto nivel para **CBDos**. Permite a cualquier persona crear herramientas, reproductores, utilidades y aplicaciones interactivas directamente en la tarjeta MicroSD sin necesidad de configurar un entorno de compilación C++, compilar herramientas ni flashear el microcontrolador.

---

## 🧭 Diferencias en el Ecosistema de Software

| Formato | Tipo de Software | Entorno de Ejecución | Caso de Uso |
| :---: | :--- | :--- | :--- |
| **`.luapp`** | **Apps de Sistema en Lua++** | Coexiste dentro de **CBDos + LVGL 9.5** | Reproductores (Winamp), utilidades, clientes de red, terminales, IoT |
| **`.cbd`** | **Cartuchos Autónomos** | Loop exclusivo (60 FPS directo a pantalla) | Juegos retro, emuladores (GB/Doom), demos gráficas |
| **`.cpp`** | **Apps Nativas Core** | Integrado en el firmware compilado | Sistema base, configuraciones maestras, flasheador ROM |

---

## 📄 1. Estructura de un Archivo `.luapp`

Un archivo `.luapp` es un script de Lua estándar enriquecido con un encabezado de metadatos estructurado al inicio del archivo:

```lua
--[[
@name: Winamp Retro
@author: CyberDeckDev
@version: 1.0.0
@icon: music
@accent: #00F5D4
@description: Reproductor de musica clasico con ecualizador visual
]]--

-- Variables locales
local count = 0
local lbl_display = nil

-- 1. Ciclo de Vida: Inicialización de la Interfaz
function on_create(parent)
    -- Crear una tarjeta contenedora
    local card = cbdos.ui.create_card(parent, 280, 200)
    
    -- Crear etiqueta de texto
    lbl_display = cbdos.ui.create_label(card, "Pistas en MicroSD: 0")
    
    -- Crear botón interactivo
    local btn_scan = cbdos.ui.create_button(card, "Escanear Música", function()
        local files = cbdos.storage.list_dir("/sdcard/music")
        lbl_display:set_text("Canciones encontradas: " .. #files)
        cbdos.ui.show_toast("Escaneo completado!")
    end)
    
    -- Iniciar audio de bienvenida
    cbdos.audio.set_volume(80)
end

-- 2. Ciclo de Vida: Vista entra a primer plano
function on_show()
    cbdos.system.log("Winamp", "App mostrada en pantalla")
end

-- 3. Ciclo de Vida: Vista pasa a segundo plano
function on_hide()
    cbdos.system.log("Winamp", "App minimizada")
end

-- 4. Ciclo de Vida: Cierre de la App
function on_destroy()
    cbdos.system.log("Winamp", "Recursos liberados")
end
```

---

## 🧰 2. APIs Expuestas a Lua++ (`cbdos.*`)

El puente nativo `LuaBridge` expone los servicios esenciales del sistema operativo:

### 2.1. `cbdos.system`
* `cbdos.system.get_time_ms()`: Milisegundos desde el arranque.
* `cbdos.system.get_free_heap()`: Memoria RAM libre en bytes.
* `cbdos.system.get_free_psram()`: Memoria PSRAM libre.
* `cbdos.system.get_cpu_temp()`: Temperatura del procesador en °C.
* `cbdos.system.log(tag, mensaje)`: Imprime en la consola del sistema.
* `cbdos.system.sleep_ms(ms)`: Pausa no bloqueante.

### 2.2. `cbdos.storage`
* `cbdos.storage.list_dir(path)`: Devuelve una tabla con los nombres de archivos en la ruta.
* `cbdos.storage.read_file(path)`: Lee el contenido completo de un archivo como string.
* `cbdos.storage.write_file(path, contenido)`: Escribe o crea un archivo en Flash o MicroSD.
* `cbdos.storage.file_exists(path)`: Retorna `true` si el archivo existe.
* `cbdos.storage.delete_file(path)`: Elimina un archivo.
* `cbdos.storage.is_sd_mounted()`: Retorna `true` si hay MicroSD insertada.

### 2.3. `cbdos.audio`
* `cbdos.audio.play_file(path)`: Reproduce un archivo MP3/AAC en segundo plano.
* `cbdos.audio.pause()` / `cbdos.audio.resume()`: Control de reproducción.
* `cbdos.audio.stop()`: Detiene la pista actual.
* `cbdos.audio.set_volume(0-100)`: Ajusta el volumen del DAC.
* `cbdos.audio.get_position_ms()`: Tiempo transcurrido de la canción en ms.
* `cbdos.audio.get_duration_ms()`: Duración total de la canción en ms.

### 2.4. `cbdos.uart`
* `cbdos.uart.init(tx, rx, baudrate)`: Abre un puerto serie con hardware externo.
* `cbdos.uart.available()`: Retorna la cantidad de bytes listos para leer.
* `cbdos.uart.read()`: Lee los caracteres entrantes como string.
* `cbdos.uart.write(data)`: Envía datos o comandos por los pines serie.
* `cbdos.uart.deinit()`: Cierra el puerto serie.

### 2.5. `cbdos.ui` (Constructores Rápidos LVGL)
* `cbdos.ui.create_card(parent, width, height)`: Contenedor con bordes redondeados y estilo visual del tema.
* `cbdos.ui.create_label(parent, texto)`: Etiqueta de texto.
* `cbdos.ui.create_button(parent, label, on_click_callback)`: Botón interactivo.
* `cbdos.ui.create_slider(parent, min, max, val, on_change_callback)`: Slider deslizable.
* `cbdos.ui.create_switch(parent, initial_state, on_toggle_callback)`: Interruptor On/Off.
* `cbdos.ui.create_textarea(parent, placeholder)`: Campo de texto o terminal interactiva.
* `cbdos.ui.show_toast(mensaje)`: Muestra una notificación flotante momentánea.

---

## 📂 3. Distribución e Instalación de Apps `.luapp`

1. **Ubicación en la MicroSD:**
   ```
   /sdcard/
   └── apps/
       ├── winamp.luapp
       ├── terminal_router.luapp
       └── sensor_clima.luapp
   ```
2. **Auto-Detección:**
   * Al encender el dispositivo o insertar la tarjeta MicroSD, CBDos lee los encabezados `@name` e `@icon` de cada archivo `.luapp`.
   * Agrega automáticamente las tarjetas correspondientes en el **Dashboard** principal.
   * Al pulsar sobre la tarjeta, el motor `LuaRunner` lanza la aplicación de forma instantánea.

3. **Compartir Software:**
   * Basta con enviarle el archivo `.luapp` a otro usuario de CBDos (o subirlo a un repositorio) para que funcione de inmediato en cualquier dispositivo compatible.
