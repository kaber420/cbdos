# 💻 Desarrollo de Apps en Lua++ (`.luapp`)

Una de las características más potentes de **CBDos** es la capacidad de ejecutar micro-aplicaciones dinámicas escritas en Lua++ sin tener que recompilar el sistema operativo ni instalar herramientas de desarrollo complejas.

---

## 🚀 ¿Cómo funciona?

1. Escribes un archivo de texto con la extensión `.luapp`.
2. Lo copias en la carpeta `/apps/` de tu tarjeta MicroSD.
3. Enciendes el CyBerDeck, abres el menú de aplicaciones y ¡tu app aparecerá lista para ejecutarse!

Cada aplicación corre dentro de un entorno seguro (*sandboxed*) en la memoria PSRAM, con acceso controlado a la interfaz gráfica, el audio, los sensores y el almacenamiento.

---

## 📄 Estructura de un archivo `.luapp`

Un archivo `.luapp` consta de una cabecera con metadatos y las funciones de ciclo de vida del sistema:

```lua
-- METADATA
-- name: Contador Digital
-- author: TuNombre
-- version: 1.0.0
-- icon: /wallpapers/app_icon.png

local count = 0
local label_count = nil

-- Se ejecuta al iniciar la aplicacion
function init(view)
    -- Crear un contenedor centrado con LVGL
    local container = ui.create_container(view)
    
    -- Crear etiqueta de texto
    label_count = ui.create_label(container, "Pulsaciones: 0")
    
    -- Crear boton interactivo
    local btn = ui.create_button(container, "¡Pulsar!")
    ui.on_click(btn, function()
        count = count + 1
        ui.set_text(label_count, "Pulsaciones: " .. tostring(count))
        sys.vibrate(50) -- Feedback háptico
    end)
end

-- Se ejecuta periódicamente en cada frame o ciclo de UI
function update(delta_ms)
    -- Lógica continua (opcional)
end

-- Se ejecuta al cerrar la app
function cleanup()
    -- Liberar recursos o guardar estado
end
```

---

## 📚 Módulos y APIs Disponibles

| Módulo | Descripción | Funciones Clave |
| :--- | :--- | :--- |
| **`ui.*`** | Motor gráfico LVGL 9.5 | `create_label`, `create_button`, `create_slider`, `set_text`, `on_click` |
| **`sys.*`** | Control del sistema operativo | `get_time`, `get_free_ram`, `reboot`, `vibrate`, `set_brightness` |
| **`storage.*`**| Persistencia y lectura/escritura | `read_file`, `write_file`, `list_dir`, `file_exists` |
| **`audio.*`** | Reproducción y efectos sonoros | `play_wav`, `play_tone`, `stop` |
| **`radio.*`** | Malla y comunicación inalámbrica| `broadcast_packet`, `on_packet_received` |

> 🔬 **Especificación Completa:**  
> Para consultar la especificación exhaustiva del formato `.luapp`, tipos de datos y sandbox de seguridad, revisa [`specs/api/luapp_specification.md`](https://github.com/kaber420/cbdos/blob/main/specs/api/luapp_specification.md).
