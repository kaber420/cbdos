# 🛠️ Plan de Reparación: Restauración de Versión Full y Aislamiento de Perfiles (`build` vs `build_red`) en ESP32-P4

**Fecha:** 2 de Noviembre de 2026  
**Estado:** Pendiente de Autorización para Ejecución  
**Ubicación:** `specs/architecture/plan_reparacion_aislamiento_perfiles_full_y_red_p4.md`  
**Archivos Afectados:** `bsp/esp32_p4_jc4880/sdkconfig`, `bsp/esp32_p4_jc4880/CMakeLists.txt`, `bsp/esp32_p4_jc4880/sdkconfig.red`  

---

## 1. Diagnóstico de la Falla (Por qué se perdió la versión Full en la JC4880)

En la plataforma **ESP32-S3 (`platformio.ini`)**, los entornos están limpiamente separados (`-e esp32s3` compila en `.pio/build/esp32s3` con perfil Full, y `-e esp32s3_red` compila en `.pio/build/esp32s3_red` con perfil Red) sin interferir entre sí.

Sin embargo, en la plataforma **ESP32-P4 (`bsp/esp32_p4_jc4880/`)** se cometieron dos errores graves en una intervención pasada:
1. **Sobrescritura destructiva del `sdkconfig` maestro:**
   En `bsp/esp32_p4_jc4880/sdkconfig` (líneas 792-793), se desactivó el perfil Full y se dejó fijado el perfil Red:
   ```ini
   # CONFIG_CBDOS_PROFILE_FULL is not set
   CONFIG_CBDOS_PROFILE_RED=y
   ```
   Como consecuencia, cualquier compilación normal (`idf.py -C bsp/esp32_p4_jc4880 build`) compilaba la versión recortada **Red** (que apaga `CBDOS_FEATURE_MULTIMEDIA`, eliminando Radio Online, Música, Grabadora, Galería y PicoTTS).
2. **Falta de aislamiento de `SDKCONFIG` al usar `-B build_red`:**
   En ESP-IDF, pasar únicamente `-B build_red` cambia el directorio de salida de los binarios, pero **sigue utilizando y sobrescribiendo el archivo `sdkconfig` de la raíz del proyecto** a menos que el `CMakeLists.txt` redirija explícitamente la variable `SDKCONFIG` hacia el interior de `build_red/`.

---

## 2. Arquitectura de Solución (Aislamiento Total de los 5 Perfiles en CMake)

En el código fuente (`core/CMakeLists.txt`, `core/include/cbdos_build_profile.h` y `bsp/esp32_p4_jc4880/main/Kconfig.projbuild`) **los 5 perfiles existen tanto para la JC3248 (S3) como para la JC4880 (P4)** y todos viven dentro del único directorio `build/`:
1. **`FULL` (Por defecto en `build/`):** Suite Cyberdeck Completa (Multimedia + Radio HTTPS + Red Team + SDR + Terminal + Lua).
2. **`RED` (`-B build/red`):** Edición Táctica Red Team (Seguridad, RF, Terminal, SDR, tema rojo, sin multimedia).
3. **`LITE` (`-B build/lite`):** Edición Ligera (Dashboard + Terminal básica, sin TTS ni codecs pesados).
4. **`TABLEHUB KDS` (`-B build/kds`):** Pantalla de Cocina (Kitchen Display System Kanban).
5. **`TABLEHUB TABLETOP` (`-B build/tabletop`):** Terminal de Mesa / Carta Digital para Comensal con bloqueo Kiosco.

```
bsp/esp32_p4_jc4880/
├── CMakeLists.txt          <-- Detecta automáticamente el perfil según '-B build/<perfil>'
├── sdkconfig               <-- MAESTRO INMUTABLE: CONFIG_CBDOS_PROFILE_FULL=y
├── sdkconfig.red           <-- Overlay para perfil Red
├── sdkconfig.lite          <-- Overlay para perfil Lite
├── sdkconfig.kds           <-- Overlay para TableHub KDS (Cocina)
├── sdkconfig.tabletop      <-- Overlay para TableHub Tabletop (Mesa)
└── build/                  <-- UN SOLO DIRECTORIO PARA TODAS LAS COMPILACIONES
    ├── cbdos_p4.bin        <-- Compilación Versión FULL (Por defecto)
    ├── red/                <-- Compilación Versión RED (-B build/red)
    ├── lite/               <-- Compilación Versión LITE (-B build/lite)
    ├── kds/                <-- Compilación Versión KDS (-B build/kds)
    └── tabletop/           <-- Compilación Versión TABLETOP (-B build/tabletop)
```

---

## 3. Cambios Exactos a Aplicar

### Paso 1: Restaurar el `sdkconfig` Maestro a `FULL` (`bsp/esp32_p4_jc4880/sdkconfig`)
En las líneas 792–794 de `bsp/esp32_p4_jc4880/sdkconfig`, restituir el perfil completo como configuración principal:
```ini
CONFIG_CBDOS_PROFILE_FULL=y
# CONFIG_CBDOS_PROFILE_RED is not set
# CONFIG_CBDOS_PROFILE_LITE is not set
```

### Paso 2: Crear Overlays Ligeros para `lite`, `kds` y `tabletop` en `bsp/esp32_p4_jc4880/`
* **`sdkconfig.red`** (ya existente):
  ```ini
  CONFIG_CBDOS_PRODUCT_CYBERDECK=y
  CONFIG_CBDOS_PROFILE_FULL=n
  CONFIG_CBDOS_PROFILE_RED=y
  ```
* **`sdkconfig.lite`**:
  ```ini
  CONFIG_CBDOS_PRODUCT_CYBERDECK=y
  CONFIG_CBDOS_PROFILE_FULL=n
  CONFIG_CBDOS_PROFILE_LITE=y
  ```
* **`sdkconfig.kds`**:
  ```ini
  CONFIG_CBDOS_PRODUCT_CYBERDECK=n
  CONFIG_CBDOS_PRODUCT_TABLEHUB=y
  CONFIG_TABLEHUB_MODE_KDS=y
  ```
* **`sdkconfig.tabletop`**:
  ```ini
  CONFIG_CBDOS_PRODUCT_CYBERDECK=n
  CONFIG_CBDOS_PRODUCT_TABLEHUB=y
  CONFIG_TABLEHUB_MODE_TABLETOP=y
  ```

### Paso 3: Aislar Todos los Perfiles en `bsp/esp32_p4_jc4880/CMakeLists.txt`
Modificar `bsp/esp32_p4_jc4880/CMakeLists.txt` antes de `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` para que cada `-B build_<perfil>` tenga su propio `sdkconfig` privado dentro de su carpeta sin tocar jamás el `sdkconfig` maestro de la raíz:

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../../core"
)

# Aislamiento estricto de perfiles por carpeta de compilación (-B)
if(CMAKE_BINARY_DIR MATCHES "build_red$")
    message(STATUS "[CBDos-P4] Perfil: CYBERDECK RED (Aislado en build_red/sdkconfig)")
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.red")
elseif(CMAKE_BINARY_DIR MATCHES "build_lite$")
    message(STATUS "[CBDos-P4] Perfil: CYBERDECK LITE (Aislado en build_lite/sdkconfig)")
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.lite")
elseif(CMAKE_BINARY_DIR MATCHES "build_kds$")
    message(STATUS "[CBDos-P4] Perfil: TABLEHUB KDS COCINA (Aislado en build_kds/sdkconfig)")
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.kds")
elseif(CMAKE_BINARY_DIR MATCHES "build_tabletop$")
    message(STATUS "[CBDos-P4] Perfil: TABLEHUB TABLETOP MESA (Aislado en build_tabletop/sdkconfig)")
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.tabletop")
else()
    message(STATUS "[CBDos-P4] Perfil: CYBERDECK FULL (Maestro en sdkconfig)")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cbdos_p4)
```

---

## 4. Tabla de Equivalencia de Comandos (ESP32-P4 vs ESP32-S3)

| Perfil / Producto | Comando en JC4880 (ESP32-P4) | Comando en JC3248 (ESP32-S3) |
| :--- | :--- | :--- |
| **1. Cyberdeck FULL (Default)** | `idf.py -C bsp/esp32_p4_jc4880 -p /dev/ttyACM0 flash` | `pio run -d bsp/esp32_s3_jc3248 -t upload` |
| **2. Cyberdeck RED (Táctico)** | `idf.py -C bsp/esp32_p4_jc4880 -B build_red -p /dev/ttyACM0 flash` | `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_red -t upload` |
| **3. Cyberdeck LITE** | `idf.py -C bsp/esp32_p4_jc4880 -B build_lite -p /dev/ttyACM0 flash` | `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_lite -t upload` |
| **4. TableHub KDS (Cocina)** | `idf.py -C bsp/esp32_p4_jc4880 -B build_kds -p /dev/ttyACM0 flash` | `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_kds -t upload` |
| **5. TableHub Tabletop (Mesa)** | `idf.py -C bsp/esp32_p4_jc4880 -B build_tabletop -p /dev/ttyACM0 flash` | `pio run -d bsp/esp32_s3_jc3248 -e esp32s3_tabletop -t upload` |

