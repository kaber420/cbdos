# 🏛️ Especificación y Plan de Implementación: Registro Unificado de Aplicaciones (AppRegistry) e Iconografía Dinámica Desacoplada

**Documento:** `specs/architecture/especificacion_app_registry_e_iconografia_dinamica.md`  
**Versión:** 1.0.0 (RFC-CBDOS-APPREG-01)  
**Estado:** 📋 Propuesta Lista para Ejecución  
**Target:** Multi-Target (ESP32-P4 ESP-IDF / ESP32-S3 PlatformIO)  
**Subsistemas:** `core/ui` (`AppRegistry`, `DashboardView`, `SystemIcons`), `core/lua` (`LuappManager`), `bsp/esp32_s3_jc3248/platformio.ini`  
**Fecha:** Octubre 2026  

---

## 1. Problema Raíz y Objetivos Técnicos

### 1.1. Diagnóstico Actual
1. **Acoplamiento Fuerte y Antipatrón en `DashboardView`:**
   - La lista de apps está hardcodeada en un vector estático dentro de `DashboardView::onCreate()` con bifurcaciones de `#if CBDOS_FEATURE_*`.
   - El evento de click contiene una cadena monolítica de 20 `if-else` (`app.id == "editor"`, etc.) para instanciar vistas.
2. **Fragilidad de Enlace y Dependencia Local en PlatformIO:**
   - Para embeber los 13 iconos binarios de 48×48 px (`.bin`), PlatformIO genera símbolos basados en la ruta absoluta de la máquina local (`_binary__home_kaber420_Documentos_...`).
   - Para compilar en local se parcheó `platformio.ini` con 13 banderas `-Wl,--defsym=...`, lo cual causa fallo inmediato en cualquier otra máquina, contenedor o CI/CD.
3. **Disparidad de Iconos entre C++ y Micro-Apps Lua:**
   - Las apps del sistema C++ usan bitmaps ARGB8888 de 48×48 px embebidos.
   - Las apps de usuario (`.luapp` en MicroSD) están restringidas a tipografía monocromática de fuentes LVGL (`LV_SYMBOL_*`). No pueden usar iconos propios.

### 1.2. Objetivos de la Arquitectura
- **Eliminar por completo las rutas de máquina y `-Wl,--defsym` de `platformio.ini`.**
- **Crear `AppRegistry`:** Un singleton en `core` que actúa como registro centralizado y desacoplado de aplicaciones.
- **Soporte de Iconografía Unificada y Dinámica:** Soporte para:
  - `IconType::SYSTEM_BUILTIN` (Iconos del sistema).
  - `IconType::FS_IMAGE` (Imágenes binarias ARGB8888/RGB565 o PNG/BMP desde LittleFS / MicroSD `/sdcard/apps/mi_app.bin`).
  - `IconType::LVGL_SYMBOL` (Fallback elegante con glifos vectoriales tipográficos).
- **Zero-Touch Dashboard:** `DashboardView` solo renderiza `AppRegistry::getInstance().getVisibleApps()` y delega el lanzamiento a `app.launchAction()`.

---

## 2. Diseño de Arquitectura

```
                       ┌────────────────────────────┐
                       │       DashboardView        │
                       │ (Dibuja Grid Reactivo LVGL)│
                       └─────────────▲──────────────┘
                                     │ Consulta apps visibles
                       ┌─────────────┴──────────────┐
                       │        AppRegistry         │
                       │   (Singleton Centralizado) │
                       └──────▲──────────────▲──────┘
                              │              │
               Auto-registro  │              │ Escaneo en caliente
               según perfil   │              │ en /sdcard/apps/
               ┌──────────────┴─────┐  ┌─────┴──────────────┐
               │  System Apps (C++) │  │ Luapps (.luapp SD) │
               │  - TerminalView    │  │  - WifiScan.luapp  │
               │  - AudioRecorder   │  │  - Cart.luapp      │
               │  - ConfigView      │  │                    │
               └────────────────────┘  └────────────────────┘
```

### 2.1. Estructura de Datos (`AppDescriptor`)

```cpp
enum class AppOrigin {
    SYSTEM_NATIVE,   // Compilada en firmware C++
    LUA_SCRIPT,      // Dinámica desde MicroSD (.luapp)
    CARTRIDGE        // Ejecutable o cartucho standalone
};

enum class IconType {
    SYSTEM_BUILTIN,  // Icono del sistema gestionado por SystemIcons
    FS_IMAGE,        // Ruta a archivo en filesystem (/sdcard/... o /spiffs/...)
    LVGL_SYMBOL      // Símbolo vectorial de fuente LVGL
};

struct AppIconDescriptor {
    IconType type = IconType::LVGL_SYMBOL;
    std::string source;         // ID de icono (ej: "recorder") o ruta ("S:/apps/app.bin")
    std::string fallbackSymbol; // Ej: LV_SYMBOL_AUDIO
};

struct AppDescriptor {
    std::string id;
    std::string title;
    AppOrigin origin = AppOrigin::SYSTEM_NATIVE;
    AppIconDescriptor icon;
    uint32_t accentColor = 0x3B82F6;
    bool visibleInDashboard = true;
    std::function<void()> launchAction;
};
```

---

## 3. Plan de Acción por Fases

### Fase 1: Creación de `AppRegistry` (`core/src/ui/AppRegistry.hpp / .cpp`)
- Implementar clase singleton `AppRegistry`.
- Métodos:
  - `void registerApp(const AppDescriptor& desc);`
  - `void unregisterApp(const std::string& id);`
  - `const std::vector<AppDescriptor>& getVisibleApps() const;`
  - `void rescanExternalApps();`
- Inicialización declarativa de apps del sistema respetando los flags de `cbdos_build_profile.h` (`CBDOS_FEATURE_FLASHER`, `CBDOS_FEATURE_TERMINAL`, `CBDOS_FEATURE_CARTRIDGE`, etc.).

### Fase 2: Refactorización de `DashboardView`
- Eliminar el vector rígido `m_apps` y la lista de 20 `if-else` en `cardClickedEventCb`.
- Al hacer clic en una tarjeta, invocar directamente `app.launchAction()`.
- Iterar sobre `AppRegistry::getInstance().getVisibleApps()` para renderizar las tarjetas.

### Fase 3: Desacoplamiento y Portabilidad de `SystemIcons` + Iconos de Archivo
- Refactorizar `SystemIcons::createIcon(lv_obj_t* parent, const AppIconDescriptor& iconDesc, int32_t size)`:
  - Si `FS_IMAGE`: cargar archivo binario directo a PSRAM o usar driver `lv_fs` de LVGL.
  - Si `SYSTEM_BUILTIN`: obtener descriptor de imagen.
  - Si no existe: fallback automático a `LV_SYMBOL_*`.
- **Limpieza de Build en PlatformIO (`platformio.ini`):**
  - Eliminar los 13 `-Wl,--defsym=...home_kaber420...`.
  - Para PlatformIO, compilar los iconos de forma portable sin depender de rutas absolutas del usuario.

### Fase 4: Integración en `LuappManager`
- Al descubrir un archivo `.luapp`, verificar si existe archivo compañero `.bin` o `.png` (ej: `/sdcard/apps/scanner.bin`) o leer `@icon_file`.
- Si existe, registrar la app con `IconType::FS_IMAGE`.
- Registrar la app directamente en `AppRegistry`.

---

## 4. Criterios de Aceptación y Validación
1. **Multi-Target Build Limpio:**
   - Compilación limpia de ESP32-P4 con ESP-IDF (`idf.py -C bsp/esp32_p4_jc4880 build`).
   - Compilación limpia de ESP32-S3 con PlatformIO (`pio run -d bsp/esp32_s3_jc3248`).
2. **Cero Rutas Absolutas:** Búsqueda en todo el repositorio de cadenas `/home/kaber420` en archivos `.ini`, `.cmake` y código fuente debe retornar 0 coincidencias.
3. **Cero Regresiones UI:** El Dashboard debe desplegar exactamente los mismos iconos y tarjetas, abriendo cada aplicación con su acción nativa.
