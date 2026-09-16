# Propuesta de Modularización: LuaBridge

## Problema Actual
`LuaBridge.cpp` es actualmente un archivo monolítico de más de 2,300 líneas. Este archivo actúa como el punto central de registro para **todas** las funciones de C++ expuestas a la máquina virtual de Lua (bindings). Debido a su tamaño, modificar cualquier aspecto del scripting o añadir nuevas funciones provoca grandes cargas cognitivas, largos tiempos de compilación y riesgo de conflictos en el control de versiones.

## Solución Propuesta: Domain-Driven Separation
Aplicar el mismo principio arquitectónico utilizado en la refactorización de `MeshCoreView.cpp`, pero enfocado en la capa de scripting. El objetivo es aislar grupos lógicos de funciones en sus propios archivos de *bindings*.

### Estructura de Archivos Recomendada
Se propone crear un directorio dedicado para los conectores lógicos: `core/src/lua/bindings/`.

1. **`LuaBindings_System.cpp`**: Utilidades base del sistema operativo (tiempos, `millis`, `delay`, memoria libre, lectura de temperatura de CPU, estado de batería e IPs).
2. **`LuaBindings_Hardware.cpp`**: Control e interacción directa con pines y periféricos físicos (GPIO, `pinMode`, `digitalWrite`, `digitalRead`, inicialización UART).
3. **`LuaBindings_FS.cpp`**: Control del sistema de archivos, montaje de tarjetas SD, lectura/escritura de archivos estáticos y gestión de directorios.
4. **`LuaBindings_Audio.cpp`**: Controladores multimedia, reproducción de música (Helix MP3/AAC), grabación de micrófono, niveles y volumen maestro.
5. **`LuaBindings_UI.cpp`**: Controles (wrappers) nativos de LVGL para instanciar componentes visuales en pantalla mediante scripts (botones, etiquetas, sliders, menús desplegables).
6. **`LuaBindings_Gfx.cpp`**: Primitivas gráficas para el componente Canvas (dibujo de píxeles manuales, rectángulos, círculos, líneas).
7. **`LuaBindings_Mesh.cpp`**: Puentes con el `meshcore_client` para la transmisión y recepción de eventos y paquetes en la malla local.

### Rol del Orquestador (`LuaBridge.cpp`)
El archivo original `LuaBridge.cpp` reduciría su tamaño a unas pocas decenas de líneas. Pasaría a tener un solo propósito:
1. Instanciar la máquina virtual de Lua (`luaL_newstate`).
2. Abrir las librerías estándar nativas (`luaL_openlibs`).
3. Invocar secuencialmente las funciones orquestadoras de registro de los submódulos (ej. `register_lua_system(L)`, `register_lua_hardware(L)`, etc.) que estarán declaradas de forma estructurada en un archivo header interno (`LuaBridge_Internal.hpp`).

## Ventajas
- **Aislamiento de Dependencias**: `LuaBindings_Audio.cpp` será el único archivo que necesite importar los headers de Helix o I2S. `LuaBindings_UI.cpp` será el único que requiera LVGL de forma profunda. Esto previene la contaminación de dependencias cruzadas.
- **Mantenibilidad y Escalabilidad**: Agregar soporte de script a un nuevo periférico (ej. un acelerómetro I2C) solo requerirá crear su propio módulo `LuaBindings_I2C.cpp` sin alterar en absoluto a las rutinas de interfaz o sistema de archivos.
- **Compilaciones Ágiles**: Al trabajar en una API específica, los sistemas de compilación no tendrán que reprocesar los módulos no afectados.
