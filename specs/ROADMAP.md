# 🗺️ CBDos v0.2.4-dev - Roadmap & Bitácora de Desarrollo

Este documento centraliza la arquitectura, el registro de componentes y el estado de avance de **CBDos** (CyBerDeck OS), diseñado para operar offline, con soporte multi-target desacoplado y perfiles modulares de producto.

---

## 🏗️ 1. Arquitectura del Sistema

El sistema utiliza una arquitectura **Dual-Target desacoplada**:
* **`core/`**: C++ Agnóstico y UI basada en **LVGL v9.5**. No contiene dependencias directas a SDKs de hardware (`#include <driver/...>` o `#include <Arduino.h>`). Utiliza interfaces HAL (`AudioHAL`, `StorageHAL`, `NetworkHAL`, `SystemHAL`, `UsbHAL`).
* **`bsp/`**: Board Support Package específico para cada microcontrolador y placa.
  * **Target ESP32-P4:** `bsp/esp32_p4_jc4880` (JC4880P443C, 480×800 IPS MIPI-DSI @ 60 FPS, ESP-IDF 5.5).
  * **Target ESP32-S3:** `bsp/esp32_s3_jc3248` (JC3248W535, 320×480 IPS QSPI @ 30 FPS, PlatformIO + Arduino Core).

---

## 📦 2. Registro de Módulos y Estado de Implementación

| Módulo / Componente | Descripción | Estado | Target S3 | Target P4 |
| :--- | :--- | :---: | :---: | :---: |
| **Core UI Engine** | Gestor de ciclo de vida de vistas (`BaseView`, `UIManager`) en LVGL 9.5 | ✅ 100% | ✅ | ✅ |
| **Theme Engine** | Paletas dinámicas, Cyberpunk, Dark, Light y acentos | ✅ 100% | ✅ | ✅ |
| **Wallpaper Engine** | Gestor de fondos de pantalla dinámicos en PSRAM | ✅ 100% | ✅ | ✅ |
| **AppRegistry Central** | Registro singleton de apps nativas y dinámicas desacoplado de Dashboard | ✅ 100% | ✅ | ✅ |
| **Dashboard View** | Grid de aplicaciones reactivo con soporte dinámico de `AppRegistry` | ✅ 100% | ✅ | ✅ |
| **Build Profiles** | Perfiles modulares (`Cyberdeck`, `TableHub KDS`, `TableHub Tabletop`) | 🔄 En Progreso | 🟡 | 🟡 |
| **Lock & Security Core** | Servicio de bloqueo y PIN (`LockService`, `LockPinModal`) NVS dual-target | ✅ 100% | ✅ | ✅ |
| **TableHub Module** | Vistas KDS (Cocina) y Tabletop (Carta/Servicio de Mesa) | 🔄 En Progreso | ⏳ | ⏳ |
| **i18n Arquitectura** | Idioma desacoplado: Core global + diccionarios por app/módulo | 🔄 En Progreso | 🟡 | 🟡 |
| **Audio Core (Helix)** | Decodificador MP3 Helix en PSRAM + Buffer I2S | ✅ 100% | ✅ | ✅ |
| **Music Player & Radio**| Reproductor local de audio y streaming Icecast/Shoutcast | ✅ 100% | ✅ | ✅ |
| **File Manager & Editor**| Explorador universal y editor de notas/código en Flash/MicroSD | ✅ 100% | ✅ | ✅ |
| **Terminal Serial UART**| Consola interactiva para routers, sensores y debug con logging | ✅ 0% | ✅ | ✅ |
| **Flasheador Universal**| Grabador de firmware para microcontroladores ESP externos y C6 | ✅ 100% | ✅ | ✅ |
| **LAN Recon Suite** | Escáner CIDR y reconocimiento de hosts de red local | ✅ 100% | ✅ | ✅ |
| **USB Host & Device** | Host CDC ACM/CH34x/CP210x, BadUSB, Ducky, HID y selector modal | ✅ 100% | ✅ | ✅ |
| **Lua Script Engine** | Intérprete Lua embebido, `LuappManager` y hot-reloading desde SD | ✅ 60% | ✅ | ✅ |
| **MeshCore Full** | Companion LoRa paridad Android: Contactos, DM, Canales y Settings | ✅ 50% | ✅ | ✅ |

*Leyenda: ✅ Operativo / 🟡 En integración / 🔄 En desarrollo / ⏳ Pendiente*

---

## 📅 3. Roadmap por Fases

### 🟢 Fase 1: Arquitectura Base, Shell y UI Core (Completada)
- [x] Desacoplamiento total de `core/` y `bsp/`.
- [x] Migración total de vistas y componentes a **LVGL v9.5**.
- [x] Implementación de `UIManager`, `BaseView`, `ThemeEngine` y `WallpaperManager`.
- [x] Pantallas base: `SplashScreenView`, `DashboardView`, `ConfigView`, `NetworkManagerView`, `StorageConfigView`.

### 🟢 Fase 2: Subsistema Multimedia, Herramientas y USB (Completada)
- [x] Motor agnóstico `AudioPlayer` con decodificación Helix MP3/AAC.
- [x] `MusicPlayerView` y `RadioView` con streaming en vivo.
- [x] `FileManagerView` y `TextEditorView` para operaciones en Flash y MicroSD.
- [x] `SerialTerminalView` (Consola UART interactiva y data logging).
- [x] `FlasherView` (Flasheador universal de microcontroladores ESP).
- [x] Suite de diagnóstico LAN Recon (`LanReconView`).
- [x] Pila USB Host y Device en ESP32-S3 (CDC ACM, CH34x, CP210x, BadUSB, selector modal).

### 🚀 Fase 3: Ecosistema de Apps, Internacionalización Modular y Perfiles (Vigente)
- [x] **Dynamic AppRegistry en C++:** Sistema de registro singleton de aplicaciones desacoplado de `DashboardView.cpp`.
- [x] **Hot-Reloading de Apps en Lua (.luapp):** Detección y ejecución de apps desde `/sdcard/apps/`.
- [x] **Subsistema Core de Bloqueo y Seguridad (`LockService` + `LockPinModal`):**
  - Implementado `cbdos::security::LockService` con políticas `Disabled`, `Lockscreen` y `SettingsOnly`, con persistencia en NVS (`cbdos_sec/pin`, `cbdos_sec/policy`).
  - Implementado modal universal `LockPinModal` (LVGL v9.5) con modo flotante y lockscreen, deprecando el mockup `StaffPinModal`.
  - Cadenas del sistema `STR_SEC_*` añadidas a `language.hpp` y `language.cpp` (ES/EN) con validación dual-target (S3/P4).
- [ ] **Internacionalización (i18n) Desacoplada por Aplicación:**
  - `core/src/system/language.cpp`: Restringido exclusivamente a textos genéricos globales del SO (OK, Cancelar, WiFi, Ajustes, etc.).
  - Diccionarios autónomos por aplicación C++ / módulo de producto (`TableHub`, etc.) bajo sus propios `#if`.
  - Exposición de la consulta de idioma del sistema a scripts Lua (`cbdos.system.getLanguage()`) para que cada `.luapp` maneje su propio diccionario local.
- [ ] **Build Profiles & TableHub:**
  - Cierre y validación de ciclo de vida por perfiles (`CBDOS_PROFILE_CYBERDECK`, `CBDOS_PROFILE_TABLEHUB_KDS`, `CBDOS_PROFILE_TABLEHUB_TABLETOP`).
  - Lógica funcional de comandas por orden de llegada en KDS y servicio de mesa en Tabletop.
- [ ] **Iconografía Dinámica desde Almacenamiento:** Soporte de carga de iconos gráficos desde MicroSD para apps externas en `SystemIcons` y `AppRegistry`.

### 🟣 Fase 4: Optimización Avanzada y Hardware
- [ ] Aceleración 2D PPA (Pixel Processing Accelerator) en ESP32-P4 para renderizado LVGL a 60 FPS.
- [ ] Soporte de teclado físico dedicado (I2C CardKB / USB HID host passthrough).
- [ ] Gestión avanzada de energía y modos de suspensión profunda (Deep Sleep).

---

## 📚 4. Portal de Documentación

* 🧭 **[Portal de Documentación Principal](README.md)**
* 📍 **[Estado Actual del Proyecto (Current Status)](project_management/CURRENT_STATUS.md)**
* 📓 **[Changelog (Historial de Versiones)](project_management/CHANGELOG.md)**
* 🛠️ **[Estado de Refactorización y Modularización](project_management/REFACTORING_STATUS.md)**
