# 📋 Propuesta de Arquitectura: Registro Unificado de Aplicaciones e Iconografía Dinámica (AppRegistry & Luapp Assets)

**Estado:** 💡 Borrador / Propuesta Técnica  
**Target:** Multi-Target (ESP32-P4 / ESP32-S3)  
**Subsistemas:** `UIManager`, `DashboardView`, `LuappManager`, `SystemIcons`, `BuildProfiles`  
**Ubicación:** `specs/drafts/BORRADOR_REGISTRO_UNIFICADO_APPS_E_ICONOGRAFIA.md`  

---

## 1. Motivación y Diagnóstico del Estado Actual

Actualmente en **CBDos**, el lanzamiento y presentación de aplicaciones presenta una asimetría técnica entre las apps nativas en C++ y las micro-apps dinámicas en Lua (`.luapp`):

1. **Acoplamiento Fuerte en el Dashboard (Hardcoded):**
   - En `DashboardView.cpp`, las apps del sistema están definidas en un array estático en código (`m_apps = { {"editor", ...}, {"radio", ...}, ... }`).
   - Agregar una nueva app o excluirla según el perfil de compilación (`BuildProfile`) obliga a modificar directamente `DashboardView.cpp` mediante `#ifdef` invasivos o bifurcaciones.
2. **Disparidad de Iconos entre Sistema y Lua:**
   - **Apps del Sistema:** Utilizan assets binarios ARGB8888 de alta resolución (48×48 px) embebidos en Flash (`SystemIcons.cpp`).
   - **Apps Lua (.luapp):** Solo soportan caracteres tipográficos monocromáticos de fuentes LVGL (`LV_SYMBOL_*`), o caen al icono genérico de archivo (`LV_SYMBOL_FILE`). No disponen de soporte para iconos personalizados gráficos procedentes de la tarjeta MicroSD.
3. **Falta de Abstracción Común:**
   - Para el usuario y para el shell gráfico, una app es un elemento que tiene título, icono, color de acento y una acción de lanzamiento (`launch()`). El Dashboard no debería requerir saber si la app se ejecuta desde Flash interna en C++ o desde la MicroSD en Lua.

---

## 2. Solución Propuesta: `AppRegistry` Desacoplado

Se plantea la creación de un servicio singleton central en `core/src/ui/AppRegistry.hpp` que actúe como la **única fuente de verdad** de aplicaciones disponibles en el sistema.

```
                           ┌────────────────────────────┐
                           │      DashboardView         │
                           │   (Pinta tarjetas/grid)    │
                           └─────────────▲──────────────┘
                                         │ Consulta apps activas
                           ┌─────────────┴──────────────┐
                           │      AppRegistry           │
                           │  (Singleton Centralizado)  │
                           └──────▲──────────────▲──────┘
                                  │              │
                   Registra al arranque          │ Escaneo dinámico
                                  │              │ en /sdcard/apps/
                   ┌──────────────┴─────┐  ┌─────┴──────────────┐
                   │ System Apps (C++)  │  │ Luapps (.luapp SD) │
                   │  - TerminalView    │  │  - MiApp.luapp     │
                   │  - MusicPlayerView │  │  - Tool.luapp      │
                   │  - FlasherView     │  │                    │
                   └────────────────────┘  └────────────────────┘
```

### 2.1. Descriptor Común de Aplicación (`AppDescriptor`)

```cpp
enum class AppOrigin {
    SYSTEM_NATIVE,   // Compilada en el firmware (C++ / LVGL 9.5)
    LUA_SCRIPT,      // Dinámica desde Flash o MicroSD (.luapp)
    CARTRIDGE        // Ejecutable standalone / partición dedicada
};

enum class IconType {
    SYSTEM_BIN,      // Referencia a SystemIcons (ej: "app_terminal")
    FS_IMAGE,        // Ruta absoluta a archivo en MicroSD (ej: "/sdcard/apps/demo.bin" o ".png")
    LVGL_SYMBOL      // Símbolo vectorial font (ej: LV_SYMBOL_KEYBOARD)
};

struct AppIconDescriptor {
    IconType type;
    std::string source;      // ID de icono o ruta en FS
    uint32_t fallbackColor;  // Color de acento
};

struct AppDescriptor {
    std::string id;          // Identificador único (ej: "sys.editor", "lua.calc")
    std::string title;       // Nombre visible en UI (soporta i18n)
    AppOrigin origin;        // Tipo de app
    AppIconDescriptor icon;  // Especificación del icono
    uint32_t accentColor;    // Color de tarjeta y glow neón
    
    // Acción de lanzamiento desacoplada
    std::function<void()> launchAction;
};
```

---

## 3. Soporte de Iconos Gráficos para `.luapp`

Para que las aplicaciones cargadas desde la MicroSD luzcan con la misma calidad visual que las del sistema:

### 3.1. Esquemas de Iconos para Luapps

1. **Convención de Archivo Hermano (Recomendado):**
   - Si existe una app `/sdcard/apps/wifitool.luapp`, el sistema buscará automáticamente en el mismo directorio:
     - `/sdcard/apps/wifitool.bin` (Raw ARGB8888 o RGB565 de 48×48 px)
     - `/sdcard/apps/wifitool.png` (Imagen estándar decodificada por LVGL)
2. **Metadatos en Cabecera de `.luapp`:**
   Se extienden los metadatos parseados por `LuappManager`:
   ```lua
   -- @name: Analizador WiFi
   -- @icon_file: /sdcard/apps/icons/wifi_scanner.bin
   -- @icon: wifi
   -- @accent: #00E5FF
   -- @version: 1.2
   ```
   *Si `@icon_file` no existe en la MicroSD, el parser recurre a `@icon` con el símbolo LVGL estándar (`LV_SYMBOL_WIFI`).*

### 3.2. Carga en Memoria Eficiente (Agnóstica al Hardware)
- En ESP32-S3 y ESP32-P4 con PSRAM, el icono de 48×48 px (aprox. 4.6 KB en RGB565 o 9.2 KB en ARGB8888) se reserva en PSRAM para no consumir memoria interna (SRAM).
- Se genera un descriptor `lv_image_dsc_t` dinámico que se entrega a `lv_image_set_src()`.

---

## 4. Registro Declarativo para Apps del Sistema (`REGISTER_APP`)

Para eliminar el array manual en `DashboardView.cpp` y soportar **Perfiles de Compilación** sin tocar la UI:

```cpp
// En EditorView.cpp
REGISTER_SYSTEM_APP({
    .id = "sys.editor",
    .titleKey = StrId::STR_APP_EDITOR,
    .icon = { IconType::SYSTEM_BIN, "editor" },
    .accentColor = 0x3B82F6,
    .featureFlag = CBDOS_FEATURE_EDITOR,
    .factory = []() {
        UIManager::getInstance().pushView(std::make_shared<EditorView>());
    }
});
```

### Ventajas de este Enfoque:
1. **Zero Acoplamiento:** `DashboardView` únicamente consulta `AppRegistry::getInstance().getVisibleApps()` e itera para dibujar las tarjetas.
2. **Compatibilidad total con Build Profiles:** Si `CBDOS_FEATURE_FLASHER = 0` en un perfil Kiosk o Lite, la macro no registra la app y el Dashboard jamás se entera de su existencia, sin generar errores de enlace ni huecos en la UI.
3. **Hot-Reloading Limpio:** Al insertar la MicroSD o refrescar el Dashboard, `AppRegistry::rescanLuaApps()` actualiza la cuadrícula de iconos en caliente.

---

## 5. Próximos Pasos para Implementación
1. Validar el diseño propuesto con el usuario.
2. Crear la clase `AppRegistry` en `core/src/ui/`.
3. Migrar el loop de tarjetas de `DashboardView` para leer desde el registro.
4. Adaptar `LuappManager` para registrar las apps detectadas en `AppRegistry` y resolver sus iconos (.bin/.png).
