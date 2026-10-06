# 🏛️ Especificación de Arquitectura: Perfiles de Compilación y Familia de Productos (CBDos & TableHub)

**Documento:** `specs/architecture/plan_perfiles_compilacion_productos_cbdos_y_tablehub.md`  
**Versión:** 1.0.0 (RFC-CBDOS-BUILD-01)  
**Estado:** ✅ Completado y Validado (Fases 1 a 6 Exitosas Multi-Target)  
**Target:** Multi-Target (ESP32-P4 ESP-IDF / ESP32-S3 PlatformIO)  
**Subsistemas:** Sistema de Build (CMake / PlatformIO), Core Engine, UIManager, Gestor de Vistas  
**Fecha:** Octubre 2026  

---

## 1. Visión General y Justificación Técnica

Actualmente, **CBDos (CyBerDeck OS)** compila de forma monolítica todas sus vistas, aplicaciones y subsistemas en un único binario. Esto incluye herramientas de diagnóstico de red (`LanReconView`), flasheo autónomo por USB (`FlasherView`), motor de cartuchos (`CartridgeView`), sintetizador de voz (`PicoTTS`), terminal serie/SSH (`TerminalView`) y decodificadores multimedia (`Helix`).

Al expandir el ecosistema hacia dispositivos comerciales y de hostelería (**TableHub**):
1. **Riesgo de Fork (Anti-Patrón):** Bifurcar el repositorio para crear un firmware de restaurante duplicaría el mantenimiento del HAL, drivers MIPI/QSPI, LVGL 9.5 y la capa de red.
2. **Superficie de Exposición y Seguridad:** En un entorno público (cocina o mesa de comensal), las herramientas de pentesting, terminales y flasheo USB no deben existir físicamente dentro del binario.
3. **Optimización de Recursos (Flash y RAM):** Retirar librerías no utilizadas (`PicoTTS` ~1.2 MB de Flash, emuladores y herramientas de red) reduce el binario en más del 50%, permitiendo doble partición OTA segura y liberando memoria PSRAM/SRAM para colas de pedidos y gráficos ricos.

### Objetivo:
Crear una **arquitectura de perfiles de compilación (`Build Profiles`)** jerárquica gobernada por banderas de preprocesador y filtros en los sistemas de construcción (CMake en ESP-IDF y PlatformIO en S3), permitiendo generar desde un único repositorio tanto la suite de ciberdeck como terminales dedicadas del ecosistema **TableHub** (**KDS** de cocina y terminal **Tabletop** de comensal).

---

## 2. Taxonomía de Familias y Productos

El sistema se estructura en dos niveles: **Familia de Producto** y **Modo/Perfil Operativo**.

```
                            ┌────────────────────────────────────────┐
                            │           CBDos Shared Base            │
                            │   (HAL, LVGL 9.5, System, NVS, Audio)  │
                            └───────────────────┬────────────────────┘
                                                │
                    ┌───────────────────────────┴───────────────────────────┐
                    ▼                                                       ▼
        ┌───────────────────────┐                               ┌───────────────────────┐
        │  PRODUCTO: CYBERDECK  │                               │   PRODUCTO: TABLEHUB  │
        │  (Ciberdeck / Campo)  │                               │ (Hostelería / Kiosco) │
        └───────────┬───────────┘                               └───────────┬───────────┘
                    │                                                       │
          ┌─────────┴─────────┐                                   ┌─────────┴─────────┐
          ▼                   ▼                                   ▼                   ▼
  ┌──────────────┐    ┌──────────────┐                    ┌──────────────┐    ┌──────────────┐
  │ PROFILE_FULL │    │ PROFILE_LITE │                    │ PROFILE_KDS  │    │PROFILE_TABLETOP
  │ (Suite campo,│    │ (Flash redu- │                    │ (Cocina P4,  │    │ (Comensal,   │
  │ flasher, tts)│    │ cida < 4 MB) │                    │  comandas)   │    │ carta/pedir) │
  └──────────────┘    └──────────────┘                    └──────────────┘    └──────────────┘
```

### 2.1. Definición de Productos y Modos

1. **Familia `CYBERDECK`:**
   * **`PROFILE_CYBERDECK_FULL` (Default):** Firmware completo para estaciones tácticas de ciberdeck. Incluye Dashboard libre, terminales, flasheador, recon de red, PicoTTS, codecs Helix, Lua Runner y motor de cartuchos.
   * **`PROFILE_CYBERDECK_LITE`:** Firmware para microcontroladores o placas con Flash limitada (< 4 MB). Mantiene el Dashboard y terminal básica, pero excluye PicoTTS, codecs pesados y cartuchos.

2. **Familia `TABLEHUB`:**
   * **`PROFILE_TABLEHUB_KDS` (Kitchen Display System):** Terminal de visualización de comandas para cocina (principalmente ESP32-P4 en pantallas de 7" o 10.1" MIPI-DSI).
     * Arranque directo a la vista táctica de cocina (`TableHubKdsView`).
     * Sin Dashboard ni acceso a configuración de sistema.
     * Alerta sonora acústica (WAV/buzzer) al ingresar nuevas órdenes.
     * Integración reactiva con MQTT local para actualización de estados (`EN_PREPARACION` -> `LISTO`).
   * **`PROFILE_TABLEHUB_TABLETOP`:** Terminal interactiva para mesa de comensal (ESP32-S3 de 3.5"-5" o ESP32-P4).
     * Arranque directo a la carta digital interactiva (`TableHubTabletopView`).
     * Funcionalidad para solicitar platos, llamar camarero y pedir la cuenta.
     * Bloqueo físico de ajustes (`KIOSK_LOCK`) con PIN de staff en esquina oculta.

---

## 3. Matriz de Capacidades y Feature Flags

Para evitar acoplar el código a nombres rígidos de productos, la configuración se traduce en *Feature Flags* booleanos atómicos:

| Feature Flag | `CYBERDECK_FULL` | `CYBERDECK_LITE` | `TABLEHUB_KDS` | `TABLEHUB_TABLETOP` |
| :--- | :---: | :---: | :---: | :---: |
| **`CBDOS_FEATURE_DASHBOARD`** | 1 | 1 | 0 | 0 |
| **`CBDOS_FEATURE_KIOSK_LOCK`** | 0 | 0 | 1 | 1 |
| **`CBDOS_FEATURE_STAFF_PIN`** | 0 | 0 | 1 | 1 |
| **`CBDOS_FEATURE_TERMINAL`** | 1 | 1 | 0 | 0 |
| **`CBDOS_FEATURE_FLASHER`** | 1 | 0 | 0 | 0 |
| **`CBDOS_FEATURE_LAN_RECON`** | 1 | 0 | 0 | 0 |
| **`CBDOS_FEATURE_CARTRIDGE`** | 1 | 0 | 0 | 0 |
| **`CBDOS_FEATURE_PICOTTS`** | 1 | 0 | 0 | 0 |
| **`CBDOS_FEATURE_HELIX_CODECS`**| 1 | 0 | 0 | 0 |
| **`CBDOS_FEATURE_LUA`** | 1 | 0 | 1 | 1 |
| **`CBDOS_FEATURE_TABLEHUB_MQTT`**| 0 | 0 | 1 | 1 |
| **`CBDOS_FEATURE_KDS_ALERT`** | 0 | 0 | 1 | 0 |
| **`CBDOS_DEFAULT_VIEW`** | `VIEW_DASHBOARD` | `VIEW_DASHBOARD` | `VIEW_TABLEHUB_KDS`| `VIEW_TABLEHUB_TABLETOP` |

---

## 4. Estructura de la Cabecera Maestra: `core/include/cbdos_build_profile.h`

```c
#pragma once

// ============================================================================
// CBDos Build Profiles & Modular Feature Flags
// Especificación: specs/architecture/plan_perfiles_compilacion_productos_cbdos_y_tablehub.md
// ============================================================================

// Identificadores de Vista Inicial
#define CBDOS_VIEW_DASHBOARD            1
#define CBDOS_VIEW_TABLEHUB_KDS         2
#define CBDOS_VIEW_TABLEHUB_TABLETOP    3

#if defined(CONFIG_CBDOS_PRODUCT_TABLEHUB)
    // ------------------------------------------------------------------------
    // FAMILIA: TABLEHUB (Hostelería y Restauración)
    // ------------------------------------------------------------------------
    #define CBDOS_PRODUCT_NAME          "TableHub"
    #define CBDOS_FEATURE_DASHBOARD     0
    #define CBDOS_FEATURE_KIOSK_LOCK    1
    #define CBDOS_FEATURE_STAFF_PIN     1
    #define CBDOS_FEATURE_TERMINAL      0
    #define CBDOS_FEATURE_FLASHER       0
    #define CBDOS_FEATURE_LAN_RECON     0
    #define CBDOS_FEATURE_CARTRIDGE     0
    #define CBDOS_FEATURE_PICOTTS       0
    #define CBDOS_FEATURE_HELIX_CODECS  0
    #define CBDOS_FEATURE_LUA           1
    #define CBDOS_FEATURE_TABLEHUB_MQTT 1

    #if defined(CONFIG_TABLEHUB_MODE_KDS)
        #define CBDOS_PROFILE_NAME      "TableHub KDS"
        #define CBDOS_FEATURE_KDS_ALERT 1
        #define CBDOS_DEFAULT_VIEW      CBDOS_VIEW_TABLEHUB_KDS
    #elif defined(CONFIG_TABLEHUB_MODE_TABLETOP)
        #define CBDOS_PROFILE_NAME      "TableHub Tabletop"
        #define CBDOS_FEATURE_KDS_ALERT 0
        #define CBDOS_DEFAULT_VIEW      CBDOS_VIEW_TABLEHUB_TABLETOP
    #else
        #error "Debe especificar un modo de TableHub: CONFIG_TABLEHUB_MODE_KDS o CONFIG_TABLEHUB_MODE_TABLETOP"
    #endif

#else
    // ------------------------------------------------------------------------
    // FAMILIA: CYBERDECK (Default)
    // ------------------------------------------------------------------------
    #define CBDOS_PRODUCT_NAME          "CBDos Cyberdeck"
    #define CBDOS_FEATURE_DASHBOARD     1
    #define CBDOS_FEATURE_KIOSK_LOCK    0
    #define CBDOS_FEATURE_STAFF_PIN     0
    #define CBDOS_FEATURE_TABLEHUB_MQTT 0
    #define CBDOS_FEATURE_KDS_ALERT     0
    #define CBDOS_DEFAULT_VIEW          CBDOS_VIEW_DASHBOARD

    #if defined(CONFIG_CBDOS_PROFILE_LITE)
        #define CBDOS_PROFILE_NAME      "Cyberdeck Lite"
        #define CBDOS_FEATURE_TERMINAL  1
        #define CBDOS_FEATURE_FLASHER   0
        #define CBDOS_FEATURE_LAN_RECON 0
        #define CBDOS_FEATURE_CARTRIDGE 0
        #define CBDOS_FEATURE_PICOTTS   0
        #define CBDOS_FEATURE_HELIX_CODECS 0
        #define CBDOS_FEATURE_LUA       0
    #else
        #ifndef CONFIG_CBDOS_PROFILE_FULL
            #define CONFIG_CBDOS_PROFILE_FULL 1
        #endif
        #define CBDOS_PROFILE_NAME      "Cyberdeck Full"
        #define CBDOS_FEATURE_TERMINAL  1
        #define CBDOS_FEATURE_FLASHER   1
        #define CBDOS_FEATURE_LAN_RECON 1
        #define CBDOS_FEATURE_CARTRIDGE 1
        #define CBDOS_FEATURE_PICOTTS   1
        #define CBDOS_FEATURE_HELIX_CODECS 1
        #define CBDOS_FEATURE_LUA       1
    #endif

#endif
```

---

## 5. Integración en los Sistemas de Construcción

### 5.1. ESP32-P4 (ESP-IDF / CMake)

En `bsp/esp32_p4_jc4880/main/Kconfig.projbuild`:
```kconfig
menu "CBDos Product & Build Profile"

choice CBDOS_PRODUCT_FAMILY
    prompt "Product Family"
    default CBDOS_PRODUCT_CYBERDECK

config CBDOS_PRODUCT_CYBERDECK
    bool "CBDos Cyberdeck"

config CBDOS_PRODUCT_TABLEHUB
    bool "TableHub Hospitality Appliance"
endchoice

if CBDOS_PRODUCT_CYBERDECK
    choice CBDOS_CYBERDECK_PROFILE
        prompt "Cyberdeck Variant"
        default CBDOS_PROFILE_FULL

    config CBDOS_PROFILE_FULL
        bool "Full Cyberdeck Suite (All Apps, Tools, Flasher, TTS)"

    config CBDOS_PROFILE_LITE
        bool "Lite Profile (Slim Footprint)"
    endchoice
endif

if CBDOS_PRODUCT_TABLEHUB
    choice TABLEHUB_DEVICE_MODE
        prompt "TableHub Operating Mode"
        default TABLEHUB_MODE_KDS

    config TABLEHUB_MODE_KDS
        bool "KDS - Kitchen Display System (Full Kitchen Dashboard)"

    config TABLEHUB_MODE_TABLETOP
        bool "Tabletop - Guest Dining Terminal (Order & Service Screen)"
    endchoice
endif

endmenu
```

En `core/CMakeLists.txt`:
```cmake
# Excluir PicoTTS si está apagado
if(CONFIG_CBDOS_PROFILE_FULL)
    list(APPEND CORE_SRCS ${PICOTTS_SRCS})
endif()

# Excluir herramientas de ciberdeck si estamos en TableHub
if(NOT CONFIG_CBDOS_PRODUCT_TABLEHUB)
    list(APPEND CORE_SRCS
        "src/ui/views/FlasherView.cpp"
        "src/ui/views/LanReconView.cpp"
        "src/ui/views/TerminalView.cpp"
        "src/ui/views/CartridgeView.cpp"
    )
endif()
```

### 5.2. ESP32-S3 (PlatformIO)

En `bsp/esp32_s3_jc3248/platformio.ini`:
```ini
; 1. Entorno Cyberdeck Completo (Default)
[env:esp32s3_cyberdeck]
extends = env
build_flags =
    ${env.build_flags}
    -DCONFIG_CBDOS_PROFILE_FULL=1

; 2. Entorno TableHub Tabletop (Comensal)
[env:esp32s3_tabletop]
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
    -DCONFIG_CBDOS_PRODUCT_TABLEHUB=1
    -DCONFIG_TABLEHUB_MODE_TABLETOP=1

; 3. Entorno TableHub KDS (Cocina)
[env:esp32s3_kds]
extends = env
build_src_filter =
    ${env:esp32s3_tabletop.build_src_filter}
build_flags =
    ${env.build_flags}
    -DCONFIG_CBDOS_PRODUCT_TABLEHUB=1
    -DCONFIG_TABLEHUB_MODE_KDS=1
```

---

## 6. Comportamiento en Runtime (UIManager)

En `core/src/ui/UIManager.cpp`:

1. **Arranque según Perfil:**
   ```cpp
   #if CBDOS_FEATURE_KIOSK_LOCK
       // En modo TableHub / Kiosk arranca directo a la vista del dispositivo
       #if defined(CONFIG_TABLEHUB_MODE_KDS)
           openView(std::make_shared<TableHubKdsView>());
       #elif defined(CONFIG_TABLEHUB_MODE_TABLETOP)
           openView(std::make_shared<TableHubTabletopView>());
       #endif
   #else
       // Modo Cyberdeck estándar
       openDashboard();
   #endif
   ```

2. **Bloqueo de HeaderBar:**
   - Si `CBDOS_FEATURE_KIOSK_LOCK` está activo, tocar la barra de estado **no** abre `QuickSettings`.
   - Si `CBDOS_FEATURE_STAFF_PIN` está activo, una pulsación prolongada (3s) en la esquina superior derecha solicita el PIN numérico de supervisor para acceder a ajustes de brillo, volumen o Wi-Fi.

3. **Audio y Voz:**
   - En `main.cpp`, el servicio `PicoTTS` solo se registra si `CBDOS_FEATURE_PICOTTS = 1`.

---

## 7. Plan de Implementación por Fases

- [x] **Fase 1: Header de Control:** Crear `core/include/cbdos_build_profile.h` con la matriz completa de macros.
  * **Entregables:**
    * `core/include/cbdos_build_profile.h`: Matriz atómica completa de `CBDOS_FEATURE_*`, identificadores de vistas iniciales y familias `CYBERDECK` y `TABLEHUB`.
    * `core/include/cbdos/build_profile.hpp`: Alias de inclusión C++ agnóstico.
- [x] **Fase 2: Integración ESP-IDF (P4):** Crear `bsp/esp32_p4_jc4880/main/Kconfig.projbuild` y segmentar fuentes en `core/CMakeLists.txt`.
  * **Entregables:**
    * `bsp/esp32_p4_jc4880/main/Kconfig.projbuild`: Menú interactivo de selección de familia y perfil para `idf.py menuconfig`.
    * `core/CMakeLists.txt`: Segmentación condicional de fuentes (`PicoTTS`, herramientas tácticas de ciberdeck y vistas TableHub).
    * **Validación:** Compilación completa y limpia en ESP32-P4 (`idf.py build` -> exit code 0).
- [x] **Fase 3: Integración PlatformIO (S3):** Configurar los entornos `[env:esp32s3_cyberdeck]`, `[env:esp32s3_tabletop]` y `[env:esp32s3_kds]` en `platformio.ini`.
  * **Entregables:**
    * `bsp/esp32_s3_jc3248/platformio.ini`: Definición de perfiles `esp32s3_cyberdeck`, `esp32s3_tabletop` y `esp32s3_kds` con flags de build dedicados.
    * Protecciones de preprocesador atómicas (`#if CBDOS_FEATURE_*`) aplicadas en:
      * `bsp/esp32_s3_jc3248/src/main.cpp` y `bsp/esp32_p4_jc4880/main/main.cpp` (`PicoTTSService`).
      * `core/src/ui/views/DashboardView.cpp` (inclusión y lanzamiento condicional de apps).
      * `core/src/ui/views/FlasherView.cpp`, `LanReconView.cpp`, `TerminalView.cpp`, `CartridgeView.cpp`.
      * `core/src/cartridge/CartridgeManager.cpp` y `core/src/tts/PicoTTSService.cpp`.
      * `bsp/esp32_s3_jc3248/src/hal_flasher_s3.cpp` y `bsp/esp32_p4_jc4880/hal/hal_flasher_p4.cpp`.
    * **Validación:** Compilación completa y limpia en ESP32-S3 (`pio run -d bsp/esp32_s3_jc3248` -> exit code 0).
- [x] **Fase 4: Adaptación de UIManager:** Implementar el arranque condicional según `CBDOS_DEFAULT_VIEW` y el bloqueo de interacción de la `HeaderBar` (`CBDOS_FEATURE_KIOSK_LOCK` y `CBDOS_FEATURE_STAFF_PIN`).
  * **Entregables:**
    * `core/src/ui/modals/StaffPinModal.hpp` y `StaffPinModal.cpp`: Modal de seguridad glassmorphic con teclado numérico reactivo en LVGL 9.5 y PIN de supervisor ("1234").
    * `core/src/ui/components/HeaderBar.hpp` y `HeaderBar.cpp`: Bloqueo condicional de apertura de `QuickSettingsPanel` (`CBDOS_FEATURE_KIOSK_LOCK`) y temporizador de pulsación prolongada de 3 segundos en zona superior derecha para disparar `StaffPinModal` (`CBDOS_FEATURE_STAFF_PIN`).
    * `core/src/ui/UIManager.hpp` y `UIManager.cpp`: Métodos `openTableHubKds()` y `openTableHubTabletop()`, inicialización condicional según `CBDOS_DEFAULT_VIEW` (`CBDOS_VIEW_TABLEHUB_KDS`, `CBDOS_VIEW_TABLEHUB_TABLETOP`, `CBDOS_VIEW_DASHBOARD`).
    * `core/include/cbdos/ui.hpp`: Exportación de interfaces `openTableHubKds()` y `openTableHubTabletop()`.
- [x] **Fase 5: Vistas de Producto TableHub:** Crear los esqueletos de `TableHubKdsView` y `TableHubTabletopView` en `core/src/ui/views/tablehub/`.
  * **Entregables:**
    * `core/src/ui/views/tablehub/TableHubKdsView.hpp` y `TableHubKdsView.cpp`: Vista táctica de cocina con barra de estado y columnas Kanban ("Pendientes", "En Preparación", "Listos para Servir").
    * `core/src/ui/views/tablehub/TableHubTabletopView.hpp` y `TableHubTabletopView.cpp`: Vista interactiva de mesa con barra de servicios ("Llamar Camarero", "Pedir Cuenta") y catálogo de categorías de carta digital.
    * Actualización en `core/CMakeLists.txt` y `bsp/esp32_s3_jc3248/platformio.ini` para incluir e indexar el directorio `tablehub`.
- [x] **Fase 6: Verificación de Compilación Cruzada:** Compilar limpio en ESP-IDF para P4 y PlatformIO para S3 para todos los perfiles sin warnings ni errores de enlace.
  * **Validación Multi-Target:**
    * `pio run -d bsp/esp32_s3_jc3248` (Default Cyberdeck Suite): **SUCCESS** (Flash: 3.61 MB / 53.0%).
    * `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_tabletop`: **SUCCESS** (Flash: 2.31 MB / 34.0%, ahorro de ~1.3 MB).
    * `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_kds`: **SUCCESS** (Flash: 2.31 MB / 34.0%).
    * `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build` (ESP32-P4): **SUCCESS** (`Built target app`, exit code 0).

