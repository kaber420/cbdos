# Propuesta de Arquitectura: Perfiles de Compilación y Modulación de Subconjuntos (Build Profiles)

**Estado:** 💡 Propuesta Formal de Arquitectura  
**Target:** Multi-Target (ESP32-P4 ESP-IDF / ESP32-S3 PlatformIO)  
**Subsistemas:** Sistema de Build (CMake / PlatformIO), Core Engine, UIManager, Gestor de Vistas  
**Ubicación Oficial:** `specs/proposals/proposal_build_profiles_and_modular_subsets.md`  

---

## 1. Visión General y Justificación Técnica

Actualmente, **CBDos (CyBerDeck OS)** compila de forma monolítica todas sus vistas, aplicaciones y subsistemas en un único binario. Esto incluye herramientas avanzadas de diagnóstico de red (`LanReconView`), flasheo autónomo por USB (`FlasherView`), motor de cartuchos (`CartridgeView`), sintetizador de voz (`PicoTTS`), terminal ANSI serie/SSH (`TerminalView`) y emuladores.

Cuando se desea desplegar CBDos en escenarios dedicados (como terminales de restaurante **TableHub**, pantallas fijas de punto de venta, paneles domóticos o dispositivos con memoria Flash reducida):
1. **Riesgo de Fork (Anti-Patrón):** La tentación común es "capar" el sistema creando una copia bifurcada del código fuente. Esto fragmenta el mantenimiento, duplicando el esfuerzo de corregir drivers del HAL, LVGL 9.5 y el stack de red.
2. **Desperdicio de Recursos:** Compilar herramientas de hacking, terminales o flasheador en un dispositivo de comensal o quiosco consume valiosos megabytes de Flash, satura las particiones OTA y desperdicia memoria RAM.
3. **Superficie de Exposición Indeseada:** En un entorno desatendido o público, las herramientas de diagnóstico no deben existir físicamente en el binario para evitar manipulaciones o fugas de configuración.

### Objetivo:
Implementar **Perfiles de Compilación (`Build Profiles`)** gobernados por flags de preprocesador y filtros en los sistemas de construcción (CMake en ESP-IDF y PlatformIO en S3). Un único repositorio y base de código compartida genera artefactos binarios especializados: desde la suite completa de ciberdeck hasta un sistema quiosco ultraligero y seguro.

---

## 2. Taxonomía de Perfiles Propuestos

Se establecen 3 perfiles oficiales de compilación para CBDos:

```
                          ┌───────────────────────────┐
                          │   CBDos Shared Codebase   │
                          │   (HAL, LVGL 9.5, Core)   │
                          └─────────────┬─────────────┘
                                        │
           ┌────────────────────────────┼────────────────────────────┐
           ▼                            ▼                            ▼
┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐
│    PROFILE_FULL      │   │    PROFILE_KIOSK     │   │     PROFILE_LITE     │
│   (Default Cyberdeck)│   │ (Appliance / TableHub│   │ (Restricted Hardware)│
├──────────────────────┤   ├──────────────────────┤   ├──────────────────────┤
│ • Terminal / SSH     │   │ • Arranque directo a │   │ • UI básica LVGL 9.5 │
│ • USB Flasher & DTR  │   │   App dedicada       │   │ • Sin TTS / Sintesis │
│ • LAN Recon / Sniff  │   │ • HeaderBar bloqueada│   │ • Sin Cartuchos/Juegos│
│ • Emuladores/Cartridge│  │ • Sin Terminal ni    │   │ • Red y Storage base │
│ • PicoTTS Completo   │   │   herramientas campo │   │ • Flash < 4 MB       │
│ • Lua Runner libre   │   │ • PIN de Staff/Admin │   │                      │
└──────────────────────┘   └──────────────────────┘   └──────────────────────┘
```

### Detalle de Capacidades por Perfil:

| Subsistema / Vista | `PROFILE_FULL` (Default) | `PROFILE_KIOSK` (TableHub/POS) | `PROFILE_LITE` (Minimal) |
| :--- | :---: | :---: | :---: |
| **HAL Universal (Display, Touch, Power, NVS)** | ✅ Sí | ✅ Sí | ✅ Sí |
| **Red Base (Wi-Fi, FastBoot NVS)** | ✅ Sí | ✅ Sí | ✅ Sí |
| **Mesh / ESP-NOW** | ✅ Sí | ✅ Opcional (`CONFIG_CBDOS_MESH=1`) | ❌ No |
| **Runtime Lua (`.luapp` / `LuaBridge`)** | ✅ Completo | ✅ Modo Sandboxed / App única | ❌ Excluido |
| **Dashboard y App Drawer** | ✅ Libre navegación | ❌ Bypass directo a Vista Principal | ⚠️ Simplificado |
| **Flasher USB Host (`FlasherView`)** | ✅ Sí | ❌ Excluido del binario | ❌ Excluido |
| **LAN Recon & Packet Tools (`LanReconView`)** | ✅ Sí | ❌ Excluido del binario | ❌ Excluido |
| **Terminal Serie / SSH (`TerminalView`)** | ✅ Sí | ❌ Excluido del binario | ❌ Excluido |
| **Cartuchos y Emulación (`CartridgeView`)** | ✅ Sí | ❌ Excluido del binario | ❌ Excluido |
| **PicoTTS (Síntesis de voz en C)** | ✅ Sí (~1.2 MB Flash) | ❌ Opcional | ❌ Excluido |
| **Reproductor de Música / Codecs Helix** | ✅ Sí | ❌ Excluido (Solo beeps/audio WAV) | ❌ Excluido |

---

## 3. Arquitectura de Flags y Control de Preprocesador

Se define un archivo de configuración central: `core/include/cbdos_build_profile.h`.

```cpp
#pragma once

// 1. Detección del Perfil Activo (Definido por CMake o PlatformIO)
#if defined(CONFIG_CBDOS_PROFILE_KIOSK)
    #define CBDOS_PROFILE_NAME          "Kiosk / Appliance"
    #define CBDOS_FEATURE_DASHBOARD     0
    #define CBDOS_FEATURE_TERMINAL      0
    #define CBDOS_FEATURE_FLASHER       0
    #define CBDOS_FEATURE_LAN_RECON     0
    #define CBDOS_FEATURE_CARTRIDGE     0
    #define CBDOS_FEATURE_PICOTTS       0
    #define CBDOS_FEATURE_HELIX_CODECS  0
    #define CBDOS_FEATURE_KIOSK_LOCK    1

#elif defined(CONFIG_CBDOS_PROFILE_LITE)
    #define CBDOS_PROFILE_NAME          "Lite / Minimal"
    #define CBDOS_FEATURE_DASHBOARD     1
    #define CBDOS_FEATURE_TERMINAL      1
    #define CBDOS_FEATURE_FLASHER       0
    #define CBDOS_FEATURE_LAN_RECON     0
    #define CBDOS_FEATURE_CARTRIDGE     0
    #define CBDOS_FEATURE_PICOTTS       0
    #define CBDOS_FEATURE_HELIX_CODECS  0
    #define CBDOS_FEATURE_LUA           0
    #define CBDOS_FEATURE_KIOSK_LOCK    0

#else // Default: FULL (Cyberdeck)
    #define CBDOS_PROFILE_NAME          "Full Cyberdeck"
    #define CBDOS_FEATURE_DASHBOARD     1
    #define CBDOS_FEATURE_TERMINAL      1
    #define CBDOS_FEATURE_FLASHER       1
    #define CBDOS_FEATURE_LAN_RECON     1
    #define CBDOS_FEATURE_CARTRIDGE     1
    #define CBDOS_FEATURE_PICOTTS       1
    #define CBDOS_FEATURE_HELIX_CODECS  1
    #define CBDOS_FEATURE_LUA           1
    #define CBDOS_FEATURE_KIOSK_LOCK    0
#endif
```

---

## 4. Integración en los Sistemas de Compilación

### 4.1. ESP-IDF (ESP32-P4 - `bsp/esp32_p4_jc4880`)

En `bsp/esp32_p4_jc4880/Kconfig.projbuild` o mediante `sdkconfig`:
```kconfig
choice CBDOS_BUILD_PROFILE
    prompt "CBDos Target Profile"
    default CBDOS_PROFILE_FULL

config CBDOS_PROFILE_FULL
    bool "Full Cyberdeck Suite (All Apps, Tools, Flasher, TTS)"

config CBDOS_PROFILE_KIOSK
    bool "Kiosk / Dedicated Appliance Mode (Locked UI, Slim Footprint)"

config CBDOS_PROFILE_LITE
    bool "Lite Minimal Profile (Flash constrained)"
endchoice
```

En `core/CMakeLists.txt`, la lista de fuentes se filtra dinámicamente antes de registrar el componente:
```cmake
# Fuentes base universales
set(CORE_SOURCES
    "src/cbdos_core.cpp"
    "src/ui/ThemeEngine.cpp"
    "src/ui/UIManager.cpp"
    "src/ui/WallpaperManager.cpp"
    "src/ui/themes/DefaultTheme.cpp"
    "src/ui/assets/SystemIcons.cpp"
    "src/ui/components/HeaderBar.cpp"
)

# Inclusión condicional de herramientas de campo
if(CONFIG_CBDOS_PROFILE_FULL)
    list(APPEND CORE_SOURCES
        "src/ui/views/FlasherView.cpp"
        "src/ui/views/LanReconView.cpp"
        "src/ui/views/CartridgeView.cpp"
        "src/ui/views/TerminalView.cpp"
        ${PICOTTS_SRCS}
        ${HELIX_SRCS}
    )
endif()

if(CONFIG_CBDOS_PROFILE_KIOSK)
    # Solo compila la vista de quiosco/restaurante y omite librerías pesadas
    list(APPEND CORE_SOURCES
        "src/ui/views/TableHubView.cpp" # O la vista designada
    )
endif()
```

### 4.2. PlatformIO (ESP32-S3 - `bsp/esp32_s3_jc3248`)

En `platformio.ini`, se definen entornos paralelos aprovechando la herencia `[env]`:
```ini
; Entorno Base Cyberdeck Completo
[env:esp32s3_full]
extends = env
build_flags =
    ${env.build_flags}
    -DCONFIG_CBDOS_PROFILE_FULL=1

; Entorno Quiosco / Restaurante (TableHub)
[env:esp32s3_kiosk]
extends = env
build_src_filter =
    +<**/*.cpp>
    +<**/*.c>
    -<../../core/src/tts/**>
    -<../../core/src/ui/views/FlasherView.cpp>
    -<../../core/src/ui/views/LanReconView.cpp>
    -<../../core/src/ui/views/TerminalView.cpp>
    -<../../core/src/cartridge/**>
build_flags =
    ${env.build_flags}
    -DCONFIG_CBDOS_PROFILE_KIOSK=1
```

---

## 5. Comportamiento en Tiempo de Ejecución (Runtime Kiosk Mode)

Bajo el perfil `KIOSK`, el [UIManager](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/UIManager.cpp) adopta una disciplina estricta de seguridad física:

1. **Bypass del Dashboard:**
   ```cpp
   #if CBDOS_FEATURE_KIOSK_LOCK
       // En modo quiosco, arranca directamente a la aplicación dedicada
       openView(ViewId::KioskApp);
   #else
       // Modo cyberdeck estándar
       openDashboard();
   #endif
   ```
2. **HeaderBar Inmutable:**
   - Se desactivan las llamadas a `toggleQuickSettings()` al pulsar la barra de estado.
   - Solo se visualizan: Identificador de estación/mesa, nivel de batería, calidad de enlace de red (Wi-Fi/Mesh) y reloj.
3. **Mecanismo de Desbloqueo para Personal (Staff PIN):**
   - Un gesto específico (ej. triple tap prolongado en la esquina superior derecha) despliega un modal con teclado numérico para ingresar el PIN de supervisor/servicio, permitiendo acceder a diagnósticos locales si es estrictamente necesario.

---

## 6. Métricas Estimadas de Reducción de Huella

Para una compilación típica en microcontroladores con particiones de 16 MB Flash y 8 MB PSRAM:

| Parámetro | Perfil `FULL` | Perfil `KIOSK` | Ahorro / Beneficio |
| :--- | :---: | :---: | :---: |
| **Tamaño Binario Flash (.bin)** | ~3.8 MB - 4.5 MB | **~1.6 MB - 2.1 MB** | **> 50% de reducción** |
| **Espacio Libre para OTA / Slots** | Ajustado en particiones de 3 MB | **Sobra > 50% de Flash para doble slot OTA** | Permite actualizaciones remotas ultra-seguras |
| **Tiempo de Compilación Limpia** | ~45 - 60 seg | **~18 - 25 seg** | Más del doble de velocidad de compilación |
| **RAM Heap Estática Ocupada** | ~140 KB | **~65 KB** | Más RAM libre para buffers de red y UI |

---

## 7. Plan de Implementación por Fases

1. **Fase 1: Creación del Header Central:**
   - Crear `core/include/cbdos_build_profile.h` con macros de activación por subsistema.
2. **Fase 2: Condicionamiento en `core/CMakeLists.txt`:**
   - Segmentar las listas `SRCS` en componentes nucleares obligatorios y componentes opcionales gobernados por `CONFIG_CBDOS_PROFILE_*`.
3. **Fase 3: Refactorización Reactiva en `UIManager`:**
   - Envolver el arranque de `DashboardView` y las entradas del menú de navegación en comprobaciones `#if CBDOS_FEATURE_*`.
4. **Fase 4: Configuración de Entornos PlatformIO:**
   - Agregar el target `[env:esp32s3_kiosk]` en `bsp/esp32_s3_jc3248/platformio.ini`.
