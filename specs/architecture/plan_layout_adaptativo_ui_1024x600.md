# Plan de Layout Adaptativo UI para Pantallas Panorámicas (1024×600 y Multi-Resolución)

> **Estado:** Auditoría y especificación documentada (no implementada todavía).
> **Contexto:** Tras levantar `bsp/esp32_p4_v3/` en la **Guition JC1060P470C (1024×600)** en modo horizontal nativo, se auditó `core/src/ui/` y `core/src/kvm/` para identificar por qué ciertos elementos se ven reducidos a la mitad de la pantalla o desaprovechan el ancho disponible.

---

## 1. Diagnóstico Actual en `core/src/ui/`

### Lo que YA es dinámico
- **`UIManager.cpp`:** `applyViewMode()` calcula `m_contentContainer` con `lv_display_get_horizontal_resolution` y `vertical_resolution`, ocupando el 100% real (`1024×600` menos `HeaderBar`).
- **Vistas con `LV_PCT(100)` base:** `LanReconView`, `SdrView`, `FlasherView`, `AudioRecorderView`, `PowerConfigView`, `MeshCoreView`, `UtilitiesView`, `WallpaperConfigView` y `DashboardView` ya usan `LV_PCT(100)` en `m_container`.

### Por qué parte de la UI se ve fija o a la mitad en 1024×600
1. **Dimensiones fijas en píxeles (modales, toolbars y etiquetas):**
   | Archivo | Elementos con ancho/tamaño fijo |
   |---|---|
   | `core/src/kvm/KvmClientView.cpp` | `m_toolbar` (`780×42`), `m_connectModal` (`440×270`), `m_taHost`/`m_taPort` (`380×42`), `m_taKvmInput` (`400×36`), `btnRow` (`380×44`) |
   | `core/src/ui/views/LottieTestView.cpp` | `m_animCard` (`280×280`), `m_navRow` (`420×44`), `m_statsCard` (`400`), `m_btnBack` (`160×38`) |
   | `core/src/ui/views/CartridgeView.cpp` | `dialog` (`300×340`), `m_modalBg` (`280×190`), `pBar` (`230×18`) |
   | `core/src/ui/views/NetworkManagerView.cpp` | `modal` (`280×220`), `m_ddMode` (`160×36`) |
   | `core/src/ui/views/omniwisp/OmniwispAddGroupModal.cpp` | `s_card` (`300×260`) |
   | `core/src/ui/components/QuickSettingsPanel.cpp` | `panelObj` (`300px` de ancho) |
   | `core/src/ui/views/radio/RadioView_Favorites.cpp`, `Explore.cpp`, `PlayerBar.cpp` | Etiquetas de nombre fijas a `240px` / `250px` en lugar de `lv_obj_set_flex_grow(..., 1)` |
   | `core/src/ui/views/LandscapeTestView.cpp` | `m_btnTouch` (`320×48`) |
   | `core/src/ui/views/TerminalView.cpp` | `m_ddMode` (`175px`) |
   | `core/src/ui/views/BleShieldView.cpp` | `m_arcGeiger` (`170×170`), `centerBox` (`110×110`) |

2. **Umbral binario único y centrado solo en ancho (`caps.width >= 480`):**
   - `DashboardView.cpp`: solo diferencia `320px` (S3) vs `>= 480px` (P4), asignando `itemWidth = 140`, `iconSize = 84`. En `1024×600` desaprovecha un breakpoint `Wide (>= 800)` para 5–6 columnas.
   - `core/src/ui/modals/` (`AboutModal.cpp`, `DiagnosticsModal.cpp`, `LanguageModal.cpp`, `UsbModal.cpp`, `LockPinModal.cpp`): usan `(caps.width >= 480)` sin verificar `caps.height`. En `AboutModal` y `DiagnosticsModal`, el alto fijo de `620px` **desborda verticalmente** en `1024×600`, `800×480` y `480×320`. Además, muestran cadenas de hardware fijas (`"480x800 MIPI-DPI @ 60 FPS"`).

3. **Fondos y Teclado con dimensiones fijas:**
   - `WallpaperManager.cpp`: mantiene `WALLPAPER_WIDTH = 320` y `WALLPAPER_HEIGHT = 480` fijos (`307,200` bytes), lo que deja corto o desajustado el fondo en `480×800` y `1024×600`.
   - `AnimatedWallpaper.cpp`: inicializa partículas con `rand() % 480` / `rand() % 800` y usa fallbacks `480×800`, concentrando partículas en la mitad izquierda en `1024×600`.
   - `UIManager.cpp`: el teclado virtual usa `kbHeight = (screenH >= 800) ? 280 : 190` y `lv_pct(100)` de ancho; en `480×320` ocupa ~60% del alto y en `1024×600` estira demasiado las teclas.

4. **Diseño de 1 sola columna vertical en vistas `LV_PCT(100)`:**
   - Vistas como `LanReconView`, `SdrView`, `MeshCoreView_Contacts`, `WifiShieldView`, `BleShieldView` se estiran a `1024px` de ancho pero apilan todo en una sola columna vertical, reduciendo el área visible útil en `600px` de alto.

---

## 2. Decisión Arquitectónica: ¿CDT, Archivo de Configuración o `UiMetrics`?

- **NO en el CDT (`cbdos_device_tree.h`):** el Device Tree describe hardware físico inmutable en compilación (pines, buses, `WIDTH`/`HEIGHT` nativos del panel, `FPS`). Como CBDos soporta rotación dinámica en tiempo de ejecución (`cbdos::display::setOrientation()`), un valor estático en CDT no puede expresar `480×800` ↔ `800×480` o `320×480` ↔ `480×320`. *(Única excepción futura: propiedades físicas del panel como PPI/DPI o márgenes de bisel si fueran necesarios como factor de escala).*
- **NO archivo externo (JSON/INI en LittleFS/SD):** añadiría I/O de disco innecesario al instanciar vistas/modales y fragilidad si el archivo falta.
- **SÍ helper C++ en `core/src/ui/` (`UiMetrics.hpp`):** lógica pura agnóstica a la plataforma que consulta una única fuente de verdad (`cbdos::display::getCapabilities()`, que ya intercambia `width`/`height` según `isLandscape()`) y calcula métricas en tiempo real.

---

## 3. Estrategia Recomendada (por Etapas)

### Etapa 1 — Tamaño Adaptativo (Bajo impacto, alta mejora visual inmediata)
*Sin rediseñar la estructura completa de cada app, hacer que los contenedores, modales y etiquetas escalen bien desde `320×480` / `480×320` hasta `1024×600`:*

1. **Helper centralizado de métricas en 2 ejes (`UiMetrics` en `core/src/ui/`):**
   - Leer `cbdos::display::getCapabilities()` como fuente única de resolución activa (`width`, `height`).
   - Clasificar **ancho (`WidthClass`)**:
     - `Compact` (`width < 480`): `320×480` (S3 portrait).
     - `Medium` (`480..799`): `480×800` (P4 portrait) y `480×320` (S3 landscape).
     - `Wide` (`width >= 800`): `800×480` (P4 landscape) y `1024×600` (JC1060P470C).
   - Clasificar **altura (`HeightClass` / límites verticales)**:
     - `Short` (`height < 640`, cubre `320`, `480` y `600` en horizontal) vs `Tall` (`height >= 640`).
     - Exponer `maxModalHeight = caps.height - 24` para garantizar que ningún modal se salga de pantalla en landscape.
2. **Reemplazar dimensiones fijas por porcentajes con límites (`LV_PCT` + `max_width` / `max_height`):**
   - Modales (`CartridgeView`, `NetworkManagerView`, `OmniwispAddGroupModal`, `KvmClientView::m_connectModal`, `AboutModal`, `DiagnosticsModal`, `LanguageModal`, `UsbModal`, `LockPinModal`): usar `LV_PCT(90)` en `Compact`, `LV_PCT(60)`–`LV_PCT(75)` en `Wide` con `lv_obj_set_style_max_width(..., 520, 0)`, `LV_SIZE_CONTENT` en alto y `lv_obj_set_style_max_height(..., metrics.maxModalHeight, 0)` con scroll vertical interno.
   - Actualizar textos de hardware en `AboutModal` / `DiagnosticsModal` para mostrar la resolución real desde `caps.width × caps.height` y `caps.targetFps`.
   - Toolbars y barras de texto (`KvmClientView::m_toolbar`, `LottieTestView::m_navRow`, `RadioView_*`): usar `LV_PCT(100)` en contenedores y `lv_obj_set_flex_grow(obj, 1)` en labels/textareas en lugar de `240px`/`400px`/`780px`.
   - `QuickSettingsPanel`: usar `LV_PCT(85)` en `Compact` y `max_width` (`380px`–`420px`) en `Wide`.
3. **Ajuste de `DashboardView` para `Wide (>= 800)`:**
   - Mantener 3 columnas en `Compact` (`96px`) y `Medium` (`140px`), y añadir perfil `Wide` para distribuir 4–5 columnas en `800×480` y 5–6 columnas limpias en `1024×600`.
4. **Teclado virtual (`UIManager.cpp`) y rotación en caliente:**
   - Calcular altura proporcional acotada (`clamp(screenH * 35 / 100, 140, 260)`) para no tapar el 60% de la pantalla en `480×320` ni cubrir campos en `1024×600`. En `Wide`, acotar el ancho máximo del teclado centrado (`max_width` ~ `780px`–`840px`).
   - Definir comportamiento ante `setOrientation()` en caliente (reconstruir o notificar re-layout a la vista activa si cambia el `WidthClass`/`HeightClass`).

### Etapa 2 — Rediseño UX Panorámico y Fondos Dinámicos
*Para ejecutar progresivamente vista por vista cuando se inicie la refactorización de UI:*

- **Patrón Maestro-Detalle (`WidthClass::Wide`, `>= 800px`):**
  - Contenedor raíz en `LV_FLEX_FLOW_ROW` (ej. panel izquierdo `35%–40%` + panel derecho `60%–65%` con `flex_grow = 1`).
  - Aplicable a: `LanReconView` (lista de hosts / detalle + puertos), `NetworkManagerView` (redes / configuración), `MeshCoreView` (contactos / conversación), `WifiShieldView` / `BleShieldView` (controles y radar/medidor a la izquierda, tabla de capturas a la derecha), `SdrView` (espectro amplio + controles laterales).
- **`WallpaperManager.cpp` y `AnimatedWallpaper.cpp`:**
  - En `WallpaperManager.cpp`, reemplazar `WALLPAPER_WIDTH = 320` y `WALLPAPER_HEIGHT = 480` por la resolución activa de `cbdos::display::getCapabilities()` (en `1024×600` RGB565 son ~1.23 MB en los 32 MB PSRAM del P4; en S3 se mantiene `320×480` ~300 KB).
  - En `AnimatedWallpaper.cpp`, usar el ancho/alto real del canvas (`lv_obj_get_width`/`height` o `caps`) al inicializar y actualizar partículas.

---

## 4. Matriz de Impacto por Resolución

| Resolución | Target / Modo | Efecto de los cambios |
|---|---|---|
| **`320×480`** | ESP32-S3 (`JC3248`) Portrait | Sin regresiones (`Compact`). Mantiene 3 columnas (`96px`), modales al `90%` y buffer de wallpaper en ~300 KB PSRAM. |
| **`480×320`** | ESP32-S3 (`JC3248`) Landscape | **Corrige roturas actuales:** evita que modales de `620px` desborden verticalmente y reduce el teclado de `190px` a `~140px`. |
| **`480×800`** | ESP32-P4 (`JC4880`) Portrait | Sin regresiones (`Medium`). Conserva 3 columnas amplias (`140px`), modales `~430px` y teclado `~260px`. |
| **`800×480`** | ESP32-P4 (`JC4880`) Landscape | Corrige desbordamiento vertical de modales, pasa a 4–5 columnas en Dashboard y habilita Maestro-Detalle en Etapa 2. |
| **`1024×600`** | ESP32-P4 (`JC1060P470C`) Native | Dashboard en 5–6 columnas, toolbars/labels a ancho completo (`flex_grow`), modales centrados con `max_width`/`max_height`, wallpaper/partículas a `1024×600` y vistas en 2 columnas (Etapa 2). |
