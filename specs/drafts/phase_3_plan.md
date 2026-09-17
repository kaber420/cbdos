# Borrador de Plan: Fase 3 - Ecosistema para Desarrolladores (SDK & Extensibilidad)

Este documento detalla el plan de implementación propuesto para la **Fase 3** del roadmap de CBDos, centrada en facilitar la creación, distribución y ejecución de aplicaciones tanto nativas (C++) como dinámicas (Lua).

---

## 1. Dynamic AppRegistry en C++ (Desacoplamiento del Dashboard)

Actualmente, cada nueva aplicación requiere modificar el código de `DashboardView.cpp` para agregar su botón y constructor correspondiente. El objetivo es descentralizar esto.

### Implementación Propuesta:
- **`AppRegistry` Singleton:** Un gestor global que almacene metadatos de cada aplicación (Nombre, Ícono, Función factoría para instanciar la vista).
- **Macro `REGISTER_APP`:** Una macro que aproveche la inicialización estática de C++ para registrar la aplicación en el `AppRegistry` antes de que se ejecute `app_main()`.
  ```cpp
  #define REGISTER_APP(className, appName, appIcon) \
      static cbdos::AppRegistration _reg_##className(appName, appIcon, []() -> cbdos::ui::BaseView* { return new className(); })
  ```
- **Refactorización de `DashboardView`:** Modificar el método `onCreate` para que itere de forma automática sobre `AppRegistry::getApps()` y genere dinámicamente la grilla de iconos basándose en las apps registradas, respetando los colores y el estilo del ThemeEngine.

---

## 2. Apps Dinámicas en Lua (Hot-Reloading y Sandboxing)

Se busca permitir a los usuarios desarrollar aplicaciones directamente en la MicroSD (editables desde el *Text Editor*) sin necesidad de recompilar todo el firmware del sistema operativo.

### Implementación Propuesta:
- **Estructura de Directorios (MicroSD):**
  - Las aplicaciones residirán de manera independiente en `/sdcard/apps/<nombre_app>/`.
  - Archivos clave: `main.lua` (punto de entrada) y `app.json` (metadatos: nombre, ícono de fuente LVGL, versión, permisos).
- **Escaneo Dinámico de Montaje:** 
  - Al iniciar el sistema o al insertar la MicroSD, un escáner leerá `/sdcard/apps/` y registrará temporalmente estas apps en el `AppRegistry` (con un distintivo visual) para que aparezcan automáticamente en el Dashboard.
- **LuaBridge (Enlace C++ -> Lua):**
  - Implementar un *wrapper* utilizando la API C de Lua para exponer servicios de CBDos al intérprete.
  - **APIs planeadas:** 
    - `cbdos.ui.*` (Constructores de vistas, layouts, botones, etiquetas y notificaciones).
    - `cbdos.storage.*` (Lectura y escritura segura de archivos dentro de su *sandbox*).
    - `cbdos.audio.*` (Ejecución de SFX y reproducción).
- **Hot-Reloading (Desarrollo Rápido):** Habilitar una rutina que vigile cambios en el `main.lua` o proveer una combinación de botones para reiniciar la máquina virtual de Lua, recargando el script instantáneamente sin reiniciar el ESP32.

---

## 3. App Store y Formato de Paquete (`.cbd`)

Para fomentar el intercambio de software y juegos en la comunidad, se definirá un formato de paquete estándar y fácil de instalar.

### Implementación Propuesta:
- **Estructura del formato `.cbd` (CyBerDeck Package):**
  - El formato será un archivo comprimido estándar (por ejemplo, ZIP sin compresión nativo, o TAR ligero) renombrado con la extensión `.cbd`.
  - Estará autocontenido, con el `app.json`, scripts `.lua` y recursos multimedia integrados.
- **Instalador de Paquetes UI:**
  - Se añadirá una integración con `FileManagerView`. Al seleccionar o hacer clic en un archivo `.cbd`, el sistema lanzará un diálogo: *"¿Desea instalar esta aplicación [Nombre]?"*.
  - Si el usuario acepta, el sistema desempaquetará el contenido de forma segura en `/sdcard/apps/<paquete>/` y forzará la actualización del Dashboard.
- **Gestor de Aplicaciones (App Manager):**
  - Se creará una nueva pestaña o submenú en el `ConfigView` para ver las aplicaciones instaladas en la MicroSD, consultar el espacio que ocupan y permitir desinstalarlas limpiamente.

---

## ❓ Preguntas Abiertas e Investigaciones Requeridas (Para el Usuario)

Para que podamos ejecutar con precisión esta Fase 3, necesito que definamos los siguientes puntos de diseño:

1. **Gestor Lua:** El roadmap marca Lua en un 80%. ¿Qué intérprete estamos usando actualmente en esa base (ej. Lua 5.4 nativo, eLua, LuaJIT)? Esto definirá cómo hacemos los enlaces.
2. **Widgets LVGL en Lua:** Hacer un *binding* 1 a 1 de toda la API de LVGL 9.5 hacia Lua es una labor inmensa. ¿Preferirías que expongamos una biblioteca abstracta más simple de alto nivel (ej. `cbdos.ui.crearBoton()`), o buscar una forma de mapear los punteros nativos de LVGL directamente?
3. **Formato `.cbd`:** ¿Es preferible que usemos un formato **TAR (sin compresión)** para el desempaquetado de `.cbd` y así ahorrar RAM y ciclos de CPU valiosos en el ESP32, o prefieres integrar una librería ligera para manejar archivos **ZIP**?
