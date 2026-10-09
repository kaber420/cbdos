# 🧩 Especificación Técnica: Modularización de Apps (Opción B) e i18n Desacoplada

**Fecha:** 2026-10-08  
**Estado:** Aprobado con Enmiendas Técnicas (Evaluado por Opencode Muse Spark 1.3)  
**Versión:** `v0.2.4-dev`  
**Ubicación:** `specs/architecture/plan_modularizacion_apps_e_i18n_desacoplada.md`

---

## 🎯 1. Propósito y Objetivos de Diseño

El presente documento define el plan arquitectónico para la **modularización de aplicaciones por subdirectorios (Opción B)** y la **descentralización definitiva de sus diccionarios de idioma (i18n)**.

### Objetivos:
1. **Aislamiento por Directorios (Opción B):** Mover cada aplicación a su propia subcarpeta dentro de `core/src/ui/views/` (siguiendo el exitoso patrón de `tablehub/`, `meshcore/`, `radio/` y `utilities/`).
2. **Encapsulamiento Lingüístico Autónomo:** Cada módulo de aplicación es dueño exclusivo de su diccionario local `[App]Language.hpp`, consultando únicamente el idioma configurado en el sistema operativo mediante `cbdos::lang::getLanguage()`.
3. **Cero Dependencias Léxicas Cruzadas:** Prohibido que una aplicación use cadenas de otra aplicación (ej. `FileManager` no importa diccionarios de `editor` ni cadenas prestadas; duplica localmente términos como Flash o SD).
4. **Preservación Invariante del Core (Tombstones TLV):** Para respetar la regla de reserva TLV de `language.hpp` (*"NUNCA renumerar, solo añadir al final"*) y el acceso directo indexado por array en `language.cpp` con `static_assert`, las cadenas retiradas de apps no se borran del enum sino que se marcan como `DEPRECATED_RESERVED` con placeholders vacíos `""`.
5. **Soporte de Compilación Dual-Target Impecable:** Actualizar `core/CMakeLists.txt` (ESP32-P4 / ESP-IDF) y `bsp/esp32_s3_jc3248/platformio.ini` (ESP32-S3 / PlatformIO) asegurando compilación limpia en cada fase sin regresiones.

---

## 🏗️ 2. Estructura de Directorios Propuesta (Opción B)

```
core/src/ui/views/
├── BaseView.hpp                    <── Clase base común
├── DashboardView.cpp/.hpp          <── Core SO
├── ConfigView.cpp/.hpp             <── Core SO
│
├── music/                          <── NUEVO MÓDULO
│   ├── MusicPlayerView.hpp
│   ├── MusicPlayerView.cpp
│   └── MusicPlayerLanguage.hpp     <── Diccionario local (8 cadenas)
│
├── editor/                         <── NUEVO MÓDULO
│   ├── TextEditorView.hpp
│   ├── TextEditorView.cpp
│   └── TextEditorLanguage.hpp      <── Diccionario local (7 cadenas)
│
├── files/                          <── NUEVO MÓDULO
│   ├── FileManagerView.hpp
│   ├── FileManagerView.cpp
│   └── FileManagerLanguage.hpp     <── Diccionario local autónomo (13 cadenas)
│
├── tablehub/                       <── Módulo TableHub (Ya operativo)
│   ├── TableHubKdsView.cpp/.hpp
│   ├── TableHubTabletopView.cpp/.hpp
│   └── TableHubLanguage.hpp
│
├── meshcore/                       <── Módulo MeshCore
├── radio/                          <── Módulo Radio
└── utilities/                      <── Módulo Utilidades
```

---

## 🌐 3. Patrón de Diccionario Local Autónomo

Cada módulo de aplicación implementa su diccionario bajo su propio espacio de nombres. No se exportan símbolos globales ni se añaden IDs al Core `cbdos/language.hpp`.

### 3.1. `MusicPlayerLanguage.hpp` (8 cadenas)
* Espacio de nombres: `cbdos::music::lang`
* Cadenas: `TITLE`, `SELECT_TRACK`, `GREET_BITBOT`, `EMPTY_SD`, `VIEW_PLAYER`, `VIEW_LIST`, `PLAYING`, `PAUSED`.

### 3.2. `TextEditorLanguage.hpp` (7 cadenas)
* Espacio de nombres: `cbdos::editor::lang`
* Cadenas: `TITLE`, `SAVE_AS`, `FLASH`, `SD`, `CANCEL`, `SAVE`, `OPEN`, `EMPTY`.

### 3.3. `FileManagerLanguage.hpp` (13 cadenas autónomas)
* Espacio de nombres: `cbdos::files::lang`
* Cadenas: `TITLE`, `TAB_SD`, `TAB_FLASH`, `CALC`, `EMPTY_DEL`, `EMPTY`, `DIR`, `CANCEL`, `DEL_BTN`, `DEL_TITLE`, `FOR_TITLE`, `BACKUP_FLASH`, `RESTORE_SD`, `CLOSE`.
* **Regla estricta:** No consume `STR_TXT_SD` ni `STR_TXT_FLASH`. Es completamente autónomo.

---

## 🏛️ 4. Preservación del Core e Invariante TLV (Tombstones)

Para proteger la integridad del despachador `tr(StrId)` y el `static_assert(sizeof(lang_es)/... == StrId::STR_COUNT)`:
1. En `core/include/cbdos/language.hpp`, los IDs `0x0038..0x003F` (Music) y `0x009B..0x00AC` (Files/Editor) se marcan explícitamente:
   ```cpp
   STR_MUSIC_TITLE = 0x0038, // DEPRECATED: migrado a music::lang::Str. Reservado.
   ```
2. En `core/src/system/language.cpp`, los slots en `lang_es[]` y `lang_en[]` se reemplazan por un placeholder tombstone:
   ```cpp
   "", // 0x0038 DEPRECATED (migrado a music/)
   ```
Esto preserva el tamaño exacto del array, el cumplimiento de `static_assert` y la compatibilidad binaria futura de paquetes TLV en SD.

---

## 🛠️ 5. Impacto en los Sistemas de Compilación

### 5.1. ESP-IDF (ESP32-P4): `core/CMakeLists.txt`
1. **Actualización de rutas en `CORE_SRCS`:**
   - `"src/ui/views/music/MusicPlayerView.cpp"`
   - `"src/ui/views/editor/TextEditorView.cpp"`
   - `"src/ui/views/files/FileManagerView.cpp"`
2. **Actualización de `INCLUDE_DIRS`:**
   - Añadir `"src/ui/views/music"`, `"src/ui/views/editor"`, `"src/ui/views/files"`.

### 5.2. PlatformIO (ESP32-S3): `bsp/esp32_s3_jc3248/platformio.ini`
1. PlatformIO compila automáticamente archivos `.cpp` dentro del subárbol de `core/src/`.
2. Añadir en `build_flags` los directorios include:
   - `-I ../../core/src/ui/views/music`
   - `-I ../../core/src/ui/views/editor`
   - `-I ../../core/src/ui/views/files`

### 5.3. Inclusiones en `AppRegistry.cpp` y Vistas Cruzadas
- `#include "views/music/MusicPlayerView.hpp"`.
- `#include "views/editor/TextEditorView.hpp"`.
- `#include "views/files/FileManagerView.hpp"`.

---

## 📋 6. Plan de Ejecución en 2 Micro-Pasos Verificables

Siguiendo la recomendación de Opencode para evitar problemas de bisección o regresiones silenciosas:

### 🔹 Micro-Paso 1: Reorganización, Diccionarios Locales y Builds
1. Crear subcarpetas `music/`, `editor/`, `files/` en `core/src/ui/views/`.
2. Mover los `.hpp` y `.cpp` respectivos.
3. Crear `MusicPlayerLanguage.hpp`, `TextEditorLanguage.hpp` y `FileManagerLanguage.hpp`.
4. Refactorizar las vistas para usar sus propios diccionarios locales (`cbdos::music::lang::tr()`, etc.).
5. Actualizar `core/CMakeLists.txt` y `platformio.ini`.
6. Actualizar includes en `AppRegistry.cpp`, `FileManagerView.cpp`, `LuaRunnerView.cpp`.
7. **Verificación de Compilación 1:** Validar build limpio en ESP32-P4 (ESP-IDF) y ESP32-S3 (PlatformIO).

### 🔹 Micro-Paso 2: Deprecación en Core (Tombstones) y Cierre
1. Marcar IDs migrados en `core/include/cbdos/language.hpp` como `DEPRECATED_RESERVED`.
2. Reemplazar cadenas en `lang_es[]` y `lang_en[]` de `core/src/system/language.cpp` por `""` (tombstones).
3. **Verificación de Compilación 2:** Validar build limpio dual-target (P4 y S3).
4. Actualizar `specs/project_management/CURRENT_STATUS.md`.

---

## 🛡️ 7. Criterios de Aceptación

1. Cada una de las 3 apps cuenta con su propia subcarpeta e incluye su propio archivo `*Language.hpp`.
2. Cero cadenas de texto quemadas en las vistas migradas.
3. Cero dependencias léxicas ni imports cruzados entre `music/`, `editor/` y `files/`.
4. Ningún `StrId` renumerado o alterado; los IDs migrados quedan reservados con tombstone `""` para respetar `static_assert` y compatibilidad TLV.
5. El Core no almacena textos internos activos de las aplicaciones de usuario.
6. `grep` de `STR_MUSIC_`, `STR_TXT_`, `STR_FILE_` en `views/music/`, `views/editor/` y `views/files/` devuelve 0 resultados.
7. Compilación dual-target limpia en ESP32-P4 (ESP-IDF) y ESP32-S3 (PlatformIO) al finalizar cada micro-paso.
