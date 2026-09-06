# 🗺️ CBDos v0.2.0 - Roadmap & Bitácora de Desarrollo

Este documento centraliza el estado actual de avance, la arquitectura, el registro de cambios (Changelog) y las metas futuras de **CBDos** (CyBerDeck OS), diseñado para operar offline y con soporte multi-target desacoplado.

---

## 🏗️ 1. Arquitectura del Sistema

El sistema utiliza una arquitectura **Dual-Target desacoplada**:
* **`core/`**: C++ Agnóstico y UI basada en **LVGL v9.5**. No contiene dependencias directas a SDKs de hardware (`#include <driver/...>` o `#include <Arduino.h>`). Utiliza interfaces HAL (`AudioHAL`, `StorageHAL`, `NetworkHAL`, `SystemHAL`).
* **`bsp/`**: Board Support Package específico para cada microcontrolador y placa.
  * **Target ESP32-P4:** `bsp/esp32_p4_jc4880` (JC4880P443C, 480×800 IPS MIPI-DSI @ 60 FPS, ESP-IDF 5.5).
  * **Target ESP32-S3:** `bsp/esp32_s3_jc3248` (JC3248W535, 320×480 IPS QSPI @ 30 FPS, PlatformIO + Arduino Core).

---

## 📦 2. Registro de Módulos y Estado de Implementación

| Módulo / Componente | Descripción | Estado | Target S3 | Target P4 |
| :--- | :--- | :---: | :---: | :---: |
| **Core UI Engine** | Gestor de ciclo de vida de vistas (`BaseView`, `UIManager`) en LVGL 9.5 | ✅ 100% | ✅ | ✅ |
| **Theme Engine** | Paletas de colores dinámicas, Cyberpunk, Dark, Light y acentos | ✅ 100% | ✅ | ✅ |
| **Wallpaper Engine** | Gestor de fondos de pantalla dinámicos en PSRAM | ✅ 100% | ✅ | ✅ |
| **Splash Screen View** | Animación de booteo y diagnóstico inicial de hardware | ✅ 100% | ✅ | ✅ |
| **Dashboard View** | Vista principal tipo Cyberdeck con accesos directos y widgets | ✅ 100% | ✅ | ✅ |
| **Config View** | Menú maestro de ajustes del sistema | ✅ 100% | ✅ | ✅ |
| **WiFi Config View** | Escaneo, conexión y gestión de credenciales WiFi | ✅ 100% | ✅ | ✅ |
| **Storage Config View**| Diagnóstico de particiones, MicroSD y LittleFS/SPIFFS | ✅ 100% | ✅ | ✅ |
| **Audio Core (Helix)** | Decodificador MP3 Helix en PSRAM + Buffer I2S | ✅ 100% | ✅ | ✅ |
| **Music Player View** | UI de reproductor de audio, lista de pistas y controles | ✅ 100% | ✅ | ✅ |
| **Radio Online View** | Reproductor de streaming Icecast/Shoutcast en directo | ✅ 100% | ✅ | ✅ |
| **File Manager View** | Explorador de archivos universal para MicroSD/Flash | ✅ 100% | ✅ | ✅ |
| **Text Editor View** | Editor de código y notas con guardado en Flash/SD | ✅ 100% | ✅ | ✅ |
| **Terminal Serial UART**| Consola interactiva para routers, sensores y debug con guardado a SD | ✅ 100% | ✅ | ✅ |
| **Flasheador Universal**| Grabador de firmware para microcontroladores ESP externos y C6 | ✅ 100% | ✅ | ✅ |
| **Mesh Routing Engine** | Direccionamiento IPv4 Mesh (`10.x.y.z`), Short IDs (DAD), Pseudo-ARP SQLite | ✅ 100% | ✅ | ✅ |
| **Navegador TLV (TLVGL)**| Cliente genérico super denso sobre ESP-NOW/WiFi con widgets LVGL 9.5 | ✅ 100% | ✅ | ✅ |
| **Gateway-Router & Proxy**| Servidor dual (TCP/Serial), transcodificador Proxy Web y persistencia SQLite | ✅ 100% | ✅ | ✅ |
| **Lua Script Engine** | Intérprete Lua embebido para scripts y micro-apps | ✅ 80% | ✅ | ✅ |
| **Cartridge Engine** | Ejecutor de juegos/cartuchos retro | ✅ 80% | ✅ | ✅ |
| **USB HID & StreamDeck**| Emulación teclado/mouse, BadUSB dual y panel MacroPad OBS | 🔄 En Plan | ⏳ | ⏳ |
| **Multi-Touch Engine** | Negociación dinámica hasta 5 puntos táctiles para Gamepad/Doom | 🔄 En Plan | ⏳ | ⏳ |
| **Synth Sound Engine** | Motor de síntesis y generador de ondas sonoras | 🔄 En Plan | ⏳ | ⏳ |

*Leyenda: ✅ Operativo / 🟡 En integración de driver hardware / 🔄 En diseño / ⏳ Pendiente*

---

## 📅 3. Roadmap por Fases

### 🟢 Fase 1: Arquitectura Base, Shell y UI Core (Completada)
- [x] Desacoplamiento total de `core/` y `bsp/`.
- [x] Migración total de vistas y componentes a **LVGL v9.5**.
- [x] Implementación de `UIManager`, `BaseView`, `ThemeEngine` y `WallpaperManager`.
- [x] Pantallas base: `SplashScreenView`, `DashboardView`, `ConfigView`, `WiFiConfigView`, `StorageConfigView`.

### 🟢 Fase 2: Subsistema Multimedia, Archivos y Herramientas (Completada)
- [x] Motor agnóstico `AudioPlayer` con decodificación Helix MP3/AAC.
- [x] `MusicPlayerView` y `RadioView` con streaming en vivo.
- [x] `FileManagerView` y `TextEditorView` para operaciones en Flash y MicroSD.
- [x] `SerialTerminalView` (Consola UART interactiva y data logging).
- [x] `FlasherView` (Flasheador universal de microcontroladores ESP).

### 🚀 Fase 3: Ecosistema para Desarrolladores (SDK & Extensibilidad)
- [ ] **Dynamic AppRegistry en C++:** Sistema de registro automático de aplicaciones mediante macros (`REGISTER_APP`) para que cualquier app nueva se agregue al Dashboard sin tener que editar `DashboardView.cpp`.
- [ ] **Apps Dinámicas en Lua (Hot-Reloading):**
  - Detección automática de aplicaciones/cartuchos almacenados en `/sdcard/apps/<app_name>/main.lua`.
  - Generación dinámica de iconos en el Dashboard desde los metadatos del script Lua.
  - Puente `LuaBridge` completo para acceder a las APIs nativas de CBDos (`cbdos.storage`, `cbdos.uart`, `cbdos.audio`, `cbdos.display`, widgets LVGL).
- [ ] **App Store / Package Format:** Formato de paquete empaquetado `.cbd` para compartir aplicaciones y juegos entre usuarios mediante la MicroSD.

### 🟣 Fase 4: Optimización Avanzada y Hardware
- [ ] Aceleración 2D PPA (Pixel Processing Accelerator) en ESP32-P4 para renderizado LVGL a 60 FPS.
- [ ] Soporte de teclado físico (I2C CardKB / USB HID).
- [ ] Gestión de energía y modo suspensión / Deep Sleep.

---

## 📚 4. Portal de Documentación

* 🧭 **[Portal de Documentación Principal](README.md)**
* 📚 **[Guía para Crear una App en CBDos](api/how_to_create_an_app.md)**
* 📖 **[Manual de Referencia de APIs del SDK](api/core_apis_reference.md)**
* 🏛️ **[Arquitectura Agnóstica y HAL](architecture/hal_and_core_architecture.md)**
* 🔌 **[Mapa de Pines y Puertos Hardware](hardware/pinouts_and_ports.md)**
