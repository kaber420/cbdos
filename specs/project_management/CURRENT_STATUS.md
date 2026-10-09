# 📊 Estado Actual del Proyecto (Project Status)

**Versión Activa:** `v0.2.4-dev`  
**Propósito:** Tablero de control técnico con el estado real del código, tareas incompletas o a medio hacer, y deuda técnica para no perder el hilo entre sesiones.

---

## 🚧 Tareas a Medias y Trabajo en Progreso (WIP)

### 1. 🔐 Subsistema de Seguridad y Bloqueo Core (`LockService` + `LockPinModal`)
- **Estado Actual:** ✅ **Fase 1 y 2 Completadas y Validadas Dual-Target (P4/S3).**
  - Implementado `cbdos::security::LockService` (`core/include/cbdos/security.hpp`, `core/src/security/LockService.cpp`) con políticas `Disabled`, `Lockscreen` y `SettingsOnly`, persistencia en NVS (`cbdos_sec`) y lazy-init defensivo.
  - Implementado `LockPinModal` (`core/src/ui/modals/LockPinModal.hpp`, `LockPinModal.cpp`) en LVGL v9.5 estricto (modal flotante y lockscreen completa).
  - Implementada vista completa de ajustes de seguridad `SecurityConfigView` (`core/src/ui/views/SecurityConfigView.hpp`, `.cpp`) enlazada en `ConfigView`.
  - Protegido el acceso a configuración bajo la política `SettingsOnly` (`AppRegistry.cpp`, `QuickSettingsPanel.cpp`).
  - Bloqueo en arranque integrado en ambos BSPs y en reactivación por suspensión en `PowerManager`.
- **Pendientes:**
  - Validación física en hardware ESP32-S3.

### 2. 🌐 Arquitectura de Internacionalización (i18n Desacoplada por App)
- **Estado Actual:** En progreso (Fase 1 Core completada, Fase 2 Producto pendiente).
  - **Core/Sistema (`core/src/system/language.cpp`):** Cadenas universales del SO completas, incluyendo claves de seguridad `STR_SEC_*`.
  - **Pendientes (Fase 2):**
    - Crear el diccionario autónomo `TableHubLanguage.hpp` en `core/src/ui/views/tablehub/` para KDS y Tabletop.
    - Reemplazar los textos quemados en `TableHubKdsView.cpp` y `TableHubTabletopView.cpp`.
    - Purgar cadenas específicas de apps del diccionario central del Core.
    - Exponer `cbdos.system.getLanguage()` en `LuaBindings_System.cpp` para que las micro-apps `.luapp` consulten el idioma del SO.

### 3. 🎛️ Perfiles de Compilación (`BuildProfiles`)
- **Estado:** Definidos en `core/include/cbdos_build_profile.h` y probados. Dual-target compila limpio en S3 (PlatformIO) y P4 (ESP-IDF 5.5).
- **Pendientes:**
  - Validar flujo de arranque y alternancia de perfiles en hardware real (`CYBERDECK`, `TABLEHUB_KDS`, `TABLEHUB_TABLETOP`).

### 4. 🍽️ Módulo TableHub
- **Estado:** Vistas iniciales creadas; el modal de PIN supervisor ya está respaldado por el nuevo `LockPinModal` del Core.
- **Pendientes:**
  - `TableHubKdsView`: Construir la lógica de gestión de comandas por orden de llegada con cambio de estado de platillos.
  - `TableHubTabletopView`: Implementar la lógica de solicitud de servicios y comunicación de red.

### 5. 🖼️ Iconografía de Aplicaciones Dinámicas (.luapp en SD)
- **Estado:** `AppRegistry.cpp` escanea `/sdcard/apps/`, pero solo asigna iconos de fuente (`LV_SYMBOL_*`).
- **Pendientes:**
  - Implementar en `SystemIcons` y `AppRegistry` el soporte para cargar archivos de icono desde almacenamiento externo (MicroSD).

---

## 📋 Lista de Tareas Prioritarias

1. **Fase 2 i18n Desacoplada (TableHub):**
   - Crear `TableHubLanguage.hpp` y eliminar textos quemados en `TableHubKdsView` y `TableHubTabletopView`.
   - Exponer consulta de idioma a scripts Lua (`cbdos.system.getLanguage()`).
2. **Implementar Ventana Completa de Ajustes (`SecurityConfigView`):**
   - Especificación formal creada en [`specs/architecture/plan_implementacion_security_config_view.md`](../architecture/plan_implementacion_security_config_view.md).
   - Implementar vista derivada de `BaseView` con selector de `LockPolicy`, cambio de PIN guiado y bloqueo manual.
   - Enlazar navegación en `ConfigView`.
3. **Construir Funcionalidad TableHub:** Desarrollar la lógica operativa de comandas en KDS y comunicación en Tabletop.
4. **Iconos Externos en SD:** Añadir soporte de carga de imágenes para apps externas en `AppRegistry`.
