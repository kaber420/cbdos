# 📊 Estado Actual del Proyecto (Project Status)

**Versión Activa:** `v0.2.4-dev`  
**Propósito:** Tablero de control técnico con el estado real del código, tareas incompletas o a medio hacer, y deuda técnica para no perder el hilo entre sesiones.

---

## 🚧 Tareas a Medias y Trabajo en Progreso (WIP)

### 1. 🔐 Subsistema de Seguridad y Bloqueo Core (`LockService` + `LockPinModal`)
- **Estado Actual:** ✅ **Fases 1 y 2 Completadas, Compiladas y Flasheadas en Hardware Dual-Target (ESP32-S3 y ESP32-P4).**
  - Implementado `cbdos::security::LockService` (`core/include/cbdos/security.hpp`, `core/src/security/LockService.cpp`) con políticas `Disabled`, `Lockscreen` y `SettingsOnly`, persistencia en NVS (`cbdos_sec`) y lazy-init defensivo.
  - Implementado `LockPinModal` (`core/src/ui/modals/LockPinModal.hpp`, `LockPinModal.cpp`) en LVGL v9.5 estricto (modal flotante y lockscreen completa).
  - Implementada vista completa de ajustes de seguridad `SecurityConfigView` (`core/src/ui/views/SecurityConfigView.hpp`, `.cpp`) enlazada en `ConfigView`.
  - Protegido el acceso a configuración bajo la política `SettingsOnly` (`AppRegistry.cpp`, `QuickSettingsPanel.cpp`).
  - Bloqueo en arranque integrado en ambos BSPs y en reactivación por suspensión en `PowerManager`.
  - Flasheado exitosamente en ambos dispositivos físicos.

### 2. 🌐 Arquitectura de Internacionalización (i18n Desacoplada por App)
- **Estado Actual:** ✅ **Fase 1, 2 y 3 Completadas y Verificadas Dual-Target (P4 y S3).**
  - **Core/Sistema (`core/src/system/language.cpp`):** Cadenas universales del SO completas y aisladas, incluyendo claves de seguridad `STR_SEC_*`. IDs antiguos de aplicaciones (`STR_MUSIC_*`, `STR_TXT_*`, `STR_FILE_*`) marcados formalmente como `DEPRECATED_RESERVED` con tombstones vacíos `""` para preservar compatibilidad binaria TLV e invariante `static_assert`.
  - **Modularización de Apps (Opción B):** Migradas a sus propias subcarpetas `core/src/ui/views/music/`, `editor/` y `files/`.
  - **Diccionarios Autónomos Locales:** Creados `MusicPlayerLanguage.hpp`, `TextEditorLanguage.hpp`, `FileManagerLanguage.hpp` y `TableHubLanguage.hpp`, eliminando toda dependencia léxica cruzada.
  - **Vistas Refactorizadas:** `MusicPlayerView.cpp`, `TextEditorView.cpp`, `FileManagerView.cpp`, `TableHubKdsView.cpp` y `TableHubTabletopView.cpp` adaptadas a sus propios `tr()` locales, con cero textos quemados.
  - **Lua Runtime:** Expuesta función `cbdos.system.getLanguage()` y `cbdos.system.get_language()` en `LuaBindings_System.cpp` para micro-apps `.luapp`.

### 3. 🎛️ Perfiles de Compilación (`BuildProfiles`)
- **Estado:** Definidos en `core/include/cbdos_build_profile.h` y probados. Dual-target compila limpio en S3 (PlatformIO) y P4 (ESP-IDF 5.5).
- **Pendientes:**
  - Validar flujo de arranque y alternancia de perfiles en hardware real (`CYBERDECK`, `TABLEHUB_KDS`, `TABLEHUB_TABLETOP`).

### 4. 🍽️ Módulo TableHub
- **Estado:** Vistas iniciales creadas con i18n desacoplada y PIN supervisor unificado con `LockPinModal`.
- **Pendientes:**
  - `TableHubKdsView`: Construir la lógica de gestión de comandas por orden de llegada con cambio de estado de platillos.
  - `TableHubTabletopView`: Implementar la lógica de solicitud de servicios y comunicación de red.

### 5. 🖼️ Iconografía de Aplicaciones Dinámicas (.luapp en SD)
- **Estado:** `AppRegistry.cpp` escanea `/sdcard/apps/`, pero solo asigna iconos de fuente (`LV_SYMBOL_*`).
- **Pendientes:**
  - Implementar en `SystemIcons` y `AppRegistry` el soporte para cargar archivos de icono desde almacenamiento externo (MicroSD).

---

## 📋 Lista de Tareas Prioritarias

1. **Construir Funcionalidad TableHub:** Desarrollar la lógica operativa de comandas en KDS y comunicación en Tabletop.
2. **Iconos Externos en SD:** Añadir soporte de carga de imágenes para apps externas en `AppRegistry`.
3. **Validación de Perfiles de Compilación en Hardware:** Probar arranque en hardware físico bajo perfiles `CYBERDECK` y `TABLEHUB`.
