# 🛡️ Informe y Plan Técnico: Purga por Licencia Privativa y Migración Limpia a Terminal Direct-Draw (DECK//OS MIT)

**Fecha de Actualización:** 30 de Octubre de 2026  
**Ubicación:** `specs/architecture/plan_purga_licencia_privativa_y_terminal_nativa.md`  
**Clasificación:** Higiene Legal, Arquitectura Core y Rendimiento UI  
**Estado:** Ejecutado y Validado Exitosamente en Hardware (P4 y S3)  

---

## 1. Resumen Ejecutivo

Durante la auditoría de licencias del directorio local de referencias, se detectó que el repositorio `references/NucleoOS-P4` (307 MB) opera bajo la licencia **PolyForm Noncommercial License 1.0.0** (`COMMERCIAL.md`). Esta licencia prohíbe explícitamente el uso comercial, la distribución integrada en productos de hardware y el uso en producción sin el pago de un acuerdo comercial privado con su autor.

**CBDos** es un sistema operativo embebido autónomo y abierto. Mantener dependencias, fragmentos de código o citas a un proyecto con restricciones no comerciales compromete la seguridad legal y la viabilidad del proyecto.

Tras una auditoría exhaustiva del subsistema de terminal y del ecosistema de sistemas de referencia disponibles, se determinó:
1. **Clarificación sobre `libvterm`:** `libvterm` **NO** es desarrollo del sistema privativo. Es una librería C99 estándar creada por Paul Evans (Leonerd) distribuida bajo la **Licencia MIT**. El sistema descartado únicamente la copió de upstream.
2. **Descarte de "Terminal desde Cero":** Crear un emulador VT100/xterm desde cero (*Clean-Room*) es un desvío de semanas con altísimo riesgo de incompatibilidad con herramientas interactivas reales (`htop`, `nano`, `vi`, `mc`, sesiones SSH complejas).
3. **Clean Provenance desde DECK//OS (MIT):** En lugar de conservar una copia con comentarios residuales del repositorio privativo (lo que podría generar dudas técnicas en auditorías automáticas), la librería `libvterm 0.3.3` se sustituye físicamente desde **DECK//OS (`references/tab5-cyberdeck`)**, donde reside su versión limpia con archivo `LICENSE` MIT oficial.
4. **Modernización de Renderizado LVGL 9.5:** Se adopta la arquitectura Direct-Draw de DECK//OS (`deck_term.c`) en [`TerminalDisplay.cpp`](../../core/src/ui/components/terminal/TerminalDisplay.cpp), sustituyendo el actual cuello de botella de concatenación de strings con tags `#RRGGBB` por renderizado por GPU/LVGL mediante `LV_EVENT_DRAW_MAIN`, invalidación selectiva de celdas dañadas y scrollback de 2,000 líneas en PSRAM.

---

## 2. Auditoría Comparativa: Cómo Manejan la Terminal Otros Sistemas Embebidos

| Sistema de Referencia | Motor / Parser Subyacente | Renderizado en Pantalla | Pros | Contras / Limitaciones en CBDos |
| :--- | :--- | :--- | :--- | :--- |
| **DECK//OS**<br>(`references/tab5-cyberdeck`)<br>*Licencia MIT* | **`libvterm 0.3.3` (Paul Evans, MIT)** puro en C99. | **Direct-Draw en LVGL 9.5 (`LV_EVENT_DRAW_MAIN`)**.<br>Rectángulos con `lv_draw_rect` y texto con `lv_draw_label`. | • Renderizado a 60 FPS por daño selectivo (`cb_damage`).<br>• Scrollback de 2,000 líneas en PSRAM (`sb_cell_t`).<br>• Soporte de gestos táctiles (drag-scroll).<br>• 100% compatible con `htop`, `nano`, SSH. | Ninguna. Es exactamente la arquitectura y versión de LVGL (v9.5) de CBDos. |
| **TentacleOS**<br>(`references/TentacleOS`)<br>*Licencia GPL-3.0* | Ninguno (usa `lv_textarea`). | Widget estándar `lv_textarea`. | Cero líneas de código de parseo. | **No es un emulador de terminal**. Sin colores por carácter, sin secuencias ANSI ni posicionamiento de cursor. Inviable para SSH real. |
| **GhostESP**<br>(`references/ghostesp`)<br>*Licencia GPL-3.0* | Ninguno (Append-only text). | `lv_label` dentro de contenedor scroller (`terminal_scroller`). | Ligero para logs serie. | Solo sirve como visor pasivo de texto hacia abajo; las secuencias de escape ANSI se imprimen como basura. |
| **ESP32-Bit-Pirate**<br>(`references/ESP32-Bit-Pirate`)<br>*Licencia GPL-3.0* | Arquitectura C++ (`ITerminalView` + `TerminalCommandTransformer`). | Línea por línea (M5Cardputer display). | Modularidad limpia entre streams serie/web. | Diseñado para consolas de comandos 1D (REPL), no mantiene una matriz 2D de pantalla completa. |
| **Momentum / Flipper**<br>(`references/momentum-firmware`)<br>*Licencia GPL-3.0* | Parser ANSI en C (`cli_ansi.c`). | Serial / UART CLI. | Parser compacto (~300 LOC) para comandos y prompt. | Diseñado para emitir secuencias por puerto serie, no para mantener estado de pantalla 2D. |
| **FabGL**<br>*(Ecosistema ESP32 clásico)* | Emulador VT100/VT220 completo en C++. | Bitmaps directos a controlador de video VGA. | Emulación muy completa y probada. | Fuertemente acoplado a VGA/DMA en ESP32 clásico; no es portable a LVGL ni a los paneles DSI/MIPI de P4/S3. |

---

## 3. Inventario de Elementos a Intervenir

| Ubicación | Elemento Actual | Estado / Riesgo | Acción Correctiva (Clean Provenance) |
| :--- | :--- | :--- | :--- |
| **`references/`** | `NucleoOS-P4/` (307 MB) | Repositorio local con licencia `PolyForm` | Borrado físico completo del espacio de trabajo. |
| **`core/src/terminal/libvterm/`** | Copia con notas residuales | Riesgo de tecnicismo por origen | **Borrado total y sustitución física** desde `references/tab5-cyberdeck/components/libvterm/` (MIT oficial). |
| **`core/src/ui/components/terminal/`** | `TerminalDisplay.cpp` (render por `lv_label` `#RRGGBB`) | Ineficiente, quema heap, sin fondos ni scrollback | Reemplazo por patrón Direct-Draw LVGL 9.5 (`LV_EVENT_DRAW_MAIN` + scrollback PSRAM). |
| **`specs/`** | Registro maestro y borradores | Citan el sistema privativo descartado | Actualizar registro maestro acreditando formalmente a DECK//OS (MIT) y registrar expulsión del privativo. |

---

## 4. Plan de Ejecución en 5 Fases

### Fase 1: Respaldo Físico Preventivo (Zero Trust)
Antes de modificar cualquier archivo, se ejecuta un respaldo físico completo de las rutas afectadas:
```bash
cp -r specs/ specs_backup_purge_$(date +%Y%m%d_%H%M%S)
cp -r core/src/terminal/ core_src_terminal_backup_$(date +%Y%m%d_%H%M%S)
cp -r core/src/ui/components/terminal/ core_ui_terminal_backup_$(date +%Y%m%d_%H%M%S)
```

### Fase 2: Purga de Almacenamiento Local y Saneamiento Documental
1. **Eliminación del repositorio local privativo:**
   ```bash
   rm -rf references/NucleoOS-P4/
   ```
   *Liberación inmediata de 307 MB en disco y desvinculación física total.*
2. **Actualización de `specs/architecture/reference_systems_contributions_and_porting_registry.md`:**
   - Crear la sección formal: *"Bitácora de Sistemas Expulsados por Licencia Incompatible"*.
   - Registrar la expulsión formal del sistema por incompatibilidad de licencia `PolyForm Noncommercial 1.0.0`.
   - Incorporar a **DECK//OS (MIT)** como la fuente formal de referencia para el motor y widget de terminal.

### Fase 3: Sustitución Limpia de `libvterm` desde DECK//OS (MIT)
Para garantizar una cadena de custodia inmaculada (*Clean Provenance*):
1. Eliminar la carpeta residual actual:
   ```bash
   rm -rf core/src/terminal/libvterm
   ```
2. Copiar la distribución limpia y oficial desde DECK//OS:
   ```bash
   cp -r references/tab5-cyberdeck/components/libvterm core/src/terminal/libvterm
   ```
   *Esta versión incluye su archivo `LICENSE` (MIT, Copyright 2008 Paul Evans) y código C99 puro sin parches ajenos.*

### Fase 4: Optimización del Renderizador Direct-Draw en `TerminalDisplay.cpp`
Modernizar [`TerminalDisplay.cpp`](../../core/src/ui/components/terminal/TerminalDisplay.cpp) manteniendo la API C++ de CBDos pero reemplazando el renderizador interno:
1. **Direct-Draw con LVGL 9.5:**
   - Crear un objeto base `lv_obj_create(m_container)`.
   - Asignar el callback de pintado: `lv_obj_add_event_cb(m_obj, draw_event_cb, LV_EVENT_DRAW_MAIN, this)`.
   - Pintar fondos de celdas con `lv_draw_rect()` y texto contiguo con `lv_draw_label()`.
2. **Daño Selectivo (Zero CPU en reposo):**
   - El callback `damage` de `libvterm` calcula las coordenadas en píxeles de las celdas alteradas e invoca `lv_obj_invalidate_area()`. Solo se redibuja lo que cambia.
3. **Scrollback de 2,000 líneas en PSRAM:**
   - Implementar buffer circular de celdas compactas (`sb_cell_t` con carácter UTF-32, colores RGB y atributos).
   - Manejo táctil con `LV_EVENT_PRESSING` / `lv_indev_get_vect()` para desplazamiento vertical fluido por el historial.
4. **Cursor Nativo Alfa-Blended:**
   - Cursor dibujado con `lv_draw_rect` con opacidad semitransparente (60%) sobre el carácter actual.

### Fase 5: Validación Cruzada de Compilación
Verificar que ambos objetivos de hardware compilen sin advertencias ni dependencias huérfanas:
- **ESP32-P4 (ESP-IDF v5.5):**
  ```bash
  . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
  ```
- **ESP32-S3 (PlatformIO):**
  ```bash
  pio run -d bsp/esp32_s3_jc3248
  ```

---

## 5. Criterios de Aceptación para Dar por Concluido el Saneamiento
- [x] 0 referencias al sistema privativo en el registro maestro de aportes.
- [x] Carpeta `references/NucleoOS-P4/` inexistente en disco (purgados 307 MB).
- [x] Directorio `core/src/terminal/libvterm/` con archivo `LICENSE` MIT oficial de Paul Evans proveniente de DECK//OS.
- [x] Renderizador `TerminalDisplay.cpp` operando por `LV_EVENT_DRAW_MAIN` sin concatenaciones de cadenas `#RRGGBB`.
- [x] Scrollback en PSRAM (2,000 líneas) operativo y desplazable por tacto.
- [x] Compilación limpia garantizada en ESP-IDF (P4) y PlatformIO (S3).
