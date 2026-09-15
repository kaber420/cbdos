# Propuesta: Implementación de API de Entrada de Texto para Lua (Fase 4 - Aplazado)

Este documento detalla la propuesta técnica para añadir soporte de entrada de texto a los scripts de Lua en CBDos, garantizando la **Unificación del Teclado del Sistema**.

## Contexto
Actualmente, el motor de scripting en Lua (`LuaBridge.cpp`) expone varios componentes de interfaz gráfica (botones, etiquetas, contenedores, canvas), pero carece de un mecanismo para que el usuario pueda introducir texto. 

Para solucionar esto sin introducir deuda técnica (como instanciar múltiples teclados locales ocultos o duplicar la lógica), se debe integrar directamente con el nuevo gestor global `cbdos::ui::UIManager`.

## APIs a Implementar en Lua

Se proponen las siguientes funciones dentro del espacio de nombres `ui`:

### 1. `ui.create_textarea(parent, placeholder)`
Crea un campo de texto de LVGL (`lv_textarea`).
- **Comportamiento interno:** Al crearse, el componente de C++ deberá invocar automáticamente `cbdos::ui::UIManager::attachKeyboard(textarea)`. Esto garantiza que, desde la perspectiva del creador del script en Lua, el teclado funcione de manera transparente y utilice la misma UI global del SO.
- **Retorno:** Un objeto (userdata) que representa el campo de texto.

### 2. `ui.get_text(textarea)`
Permite al script de Lua extraer el texto actual escrito por el usuario en el campo.
- **Implementación:** Wrapper directo de `lv_textarea_get_text()`.

### 3. `ui.set_text(textarea, texto)`
Permite al script sobrescribir o limpiar el contenido del campo de texto. (Opcional, pero muy útil para flujos interactivos).

### 4. Eventos de Confirmación (Callback)
Al usar `UIManager::attachKeyboard(obj, config)`, la configuración en C++ permite pasar un lambda `onSubmit`. 
Es fundamental que el *binding* de Lua pueda recibir un callback para notificar al script cuando el usuario presione el botón de aceptar (Enter/Submit) en el Teclado Global.

```lua
-- Ejemplo de uso proyectado en un script Lua:
local ta = ui.create_textarea(pantalla, "Escribe tu nombre...")

-- Opcional: escuchar el evento de cuando el usuario termina de escribir
ui.on_submit(ta, function(texto)
    ui.show_toast("Hola " .. texto)
end)
```

## Cambios Requeridos en `LuaBridge.cpp`

1. Añadir `static int lua_ui_create_textarea(lua_State* L)`.
2. Añadir `static int lua_ui_get_text(lua_State* L)`.
3. Mapear un evento o función para enlazar el callback de `onSubmit` del `UIManager` con la pila (stack) de ejecución de Lua, teniendo precaución de liberar la referencia al callback en el evento `LV_EVENT_DELETE` para evitar *memory leaks*.
4. Registrar estas funciones en el bloque de inicialización de la librería `ui`.

## Conclusión
Implementando estas funciones bajo esta arquitectura aseguraremos que los scripts en Lua sean potentes, interactivos y que respeten las políticas de memoria y diseño gráfico impuestas por el sistema operativo CBDos.
