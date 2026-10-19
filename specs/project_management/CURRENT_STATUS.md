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

### 3. 🎛️ Perfiles de Compilación y Perfil Cyberdeck RED (`CONFIG_CBDOS_PROFILE_RED`)
- **Estado Actual:** ✅ **Completado, Compilado y Flasheado en Hardware Real (JC3248W535 / ESP32-S3).**
  - **Gestor de Modos de Vista (`ViewMode`):** Implementado en `BaseView.hpp` (`Translucent`, `Solid`, `Immersive`). Orquestación dinámica en `UIManager` con pausado de `AnimatedWallpaper` en vistas sólidas para ahorro de ciclos de CPU y batería.
  - **HeaderBar Plana Táctica:** Implementada en `HeaderBar.cpp` (ancho 100%, Y=0, radio 0 px, botón volver compacto 36x28 px).
  - **Purga Multimedia:** Gating con `CBDOS_FEATURE_MULTIMEDIA` (0 en RED, 1 en FULL). Galería, Radio, Grabadora, Música y Lottie purgados en RED, ahorrando más de 80 KB de Flash.
  - **Suites Tácticas RF (LVGL 9.5 & ViewMode::Solid):** Creadas e integradas en `AppRegistry`: `WifiShieldView` (Recon, IDS Sentinel, Sniffer PCAP), `BleShieldView` (Air Recon, Geiger, Spam IDS), `WifiStrikeView` (Deauth, Beacon Spam, Evil Portal, PMKID) y `BleStrikeView` (AppleJuice, FastPair, SwiftPair, Spoofer).
  - **Validación Física:** Flasheado y funcionando en la placa JC3248W535.

### 4. 🍽️ Módulo TableHub
- **Estado:** Vistas iniciales creadas con i18n desacoplada y PIN supervisor unificado con `LockPinModal`.
- **Pendientes:**
  - `TableHubKdsView`: Construir la lógica de gestión de comandas por orden de llegada con cambio de estado de platillos.
  - `TableHubTabletopView`: Implementar la lógica de solicitud de servicios y comunicación de red.

### 5. 🖼️ Iconografía de Aplicaciones Dinámicas (.luapp en SD)
- **Estado:** `AppRegistry.cpp` escanea `/sdcard/apps/`, pero solo asigna iconos de fuente (`LV_SYMBOL_*`).
- **Pendientes:**
  - Implementar en `SystemIcons` y `AppRegistry` el soporte para cargar archivos de icono desde almacenamiento externo (MicroSD).

### 6. 🔌 Deuda Técnica Hardware: Desacoplamiento de Bus I2C en BSP (Prioritario)
- **Estado:** Identificado y documentado formalmente.
- **Problema:** En el ESP32-P4, `TouchHAL` asume la creación del bus I2C maestro y `AudioHAL` depende de él para configurar el códec ES8311.
- **Acción Pendiente:** Extirpar la inicialización de I2C de `TouchHAL` y delegarla a un `I2cMasterBus` central en el BSP ([Ver Plan Arquitectónico](../architecture/plan_desacoplamiento_bus_i2c_centralizado.md)).

---

## 📋 Lista de Tareas Prioritarias

1. **Construir Funcionalidad TableHub:** Desarrollar la lógica operativa de comandas en KDS y comunicación en Tabletop.
2. **Iconos Externos en SD:** Añadir soporte de carga de imágenes para apps externas en `AppRegistry`.
3. **Validación de Perfiles de Compilación en Hardware:** Probar arranque en hardware físico bajo perfiles `CYBERDECK` y `TABLEHUB`.
4. **Revisión de Propuesta PowerManager (Warm Sleep 80 MHz + LVGL Inactivity):** [Ver Propuesta Técnica](../proposals/proposal_power_manager_active_idle_and_global_inactivity.md).
5. ⚠️ **Desacoplamiento de Bus I2C en BSP (Refactor Prioritario):** Extirpar bus de `TouchHAL` y centralizar en `I2cMasterBus` ([Ver Plan](../architecture/plan_desacoplamiento_bus_i2c_centralizado.md)).
6. **Suite IEEE 802.15.4 (Zigbee / Matter / Thread / P2P):** Desbloquear radio cruda en coprocesador C6 vía SDIO ([Ver Especificación](../architecture/especificacion_tecnica_suite_802154_zigbee_thread_matter.md)).
7. **Enlace Táctico BLE Coded PHY (1 MHz @ 125 kbps):** Pruebas de largo alcance en canales estrechos ([Ver Propuesta](../proposals/proposal_ble_coded_phy_long_range_1mhz.md)).
