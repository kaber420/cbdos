# 🖥️ Especificación Arquitectónica: Emulador de Terminal VT100/ANSI (`libvterm`) y Renderizado Reactivo LVGL 9.5

**Fecha:** 21 de Octubre de 2026  
**Estado:** 📐 Aprobado para Implementación  
**Categoría:** Subsistema Core / Terminal & UI (`core/src/terminal/`, `core/src/ui/components/terminal/`, `core/src/ui/fonts/`)  
**Código de Referencia Auditado:** `references/NucleoOS-P4/components/libvterm/` y `references/NucleoOS-P4/components/nv_apps/terminal_app.cpp`

---

## 1. Justificación y Diagnóstico del Problema Actual

En la implementación actual de **CBDos**, el módulo [`TerminalDisplay.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/components/terminal/TerminalDisplay.cpp) utiliza un algoritmo rudimentario en `sanitizeAndStripAnsi()`:
- **Destrucción de Códigos de Control:** Filtra y elimina a ciegas las secuencias de escape ANSI (`0x1B [...]`), impidiendo el uso de colores, negritas, subrayados o posicionamiento del cursor.
- **Incompatibilidad con TUI / Curses:** Al conectarse por **UART** (consola Cisco, MikroTik, U-Boot, ESP32) o **SSH** (`cbdos::ssh`), aplicaciones interactivas de pantalla completa como `htop`, `nano`, `top`, `vim`, `raspi-config` o menús de router muestran una salida corrupta o ilegible.
- **Cuello de Botella de Rendimiento:** Usar un `lv_textarea` estándar para un stream serie continuo genera realocaciones masivas de cadenas de texto y parpadeo continuo en el redraw de LVGL.

---

## 2. Arquitectura del Nuevo Emulador de Terminal

La solución adopta la arquitectura probada en `NucleoOS-P4`, desacoplándola e integrándola en la jerarquía de CBDos:

```
┌────────────────────────────────────────────────────────┐
│             TerminalView (Vista LVGL 9.5)              │
├──────────────────────────┬─────────────────────────────┤
│   SerialControlBar       │      SshControlBar          │
├──────────────────────────┴─────────────────────────────┤
│                                                        │
│               TerminalDisplay (VTermWidget)            │
│  ┌──────────────────────────────────────────────────┐  │
│  │ Matriz de Celdas (Cols x Rows)                   │  │
│  │ - Fuente Monoespaciada DejaVu Sans Mono 17        │  │
│  │   (con caracteres de dibujo de cajas 0x2500-259F)│  │
│  │ - Paleta ANSI Tango 16 Colores + TrueColor       │  │
│  │ - Cursor Intermitente (Blink) y Modos Raw/Cooked │  │
│  │ - Scrollback de 1,000 líneas en PSRAM            │  │
│  └──────────────────────────────────────────────────┘  │
│                                                        │
├────────────────────────────────────────────────────────┤
│          TerminalCommandBar / Fila Táctica             │
│   [ESC] [TAB] [CTRL] [ALT] [ ↑ ] [ ↓ ] [ ← ] [ → ] [^C]│
└──────────────────────────┬─────────────────────────────┘
                           │
             ┌─────────────┴─────────────┐
             ▼                           ▼
    SerialStreamAdapter          SshStreamAdapter
    (UART / USB CDC ACM)         (Sesión libssh SSH-2)
```

### 2.1. Motor `libvterm 0.3.3` (`core/src/terminal/libvterm/`)
- Biblioteca C99 pura desarrollada por Paul Evans (Neovim), sin dependencias del sistema operativo ni de POSIX.
- Mantiene la máquina de estados completa de emulación VT100 / VT220 / xterm:
  - Rejilla de celdas bidimensional (`VTermScreen`, `VTermScreenCell`).
  - Atributos por celda: `bold`, `underline`, `reverse`, `strike`, color de primer plano (`fg`) y fondo (`bg`).
  - Respuestas automáticas a consultas del host (DSR/CPR, DA).
  - Alocación de memoria redirigida a PSRAM externa mediante `VTermAllocatorFunctions` (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`).

### 2.2. Tipografía Monoespaciada (`cbdos_font_mono_17.c`)
- DejaVu Sans Mono 17 px generada para LVGL con soporte para:
  - ASCII y Latin-1 completo.
  - Caracteres semigráficos y de dibujo de cajas (`0x2500` a `0x259F`), bloques sólidos y flechas (`0x2190` a `0x2195`), indispensables para bordes y tablas en TUI (`htop`, `dialog`, `nano`).

### 2.3. Renderizado Reactivo por Filas Dañadas (`LV_EVENT_DRAW_MAIN`)
- En lugar de redibujar todo el contenedor en cada byte entrante, `libvterm` emite el callback `cb_damage(VTermRect r)`:
  - Se calcula el área de píxeles exacta de las filas afectadas (`r.start_row .. r.end_row`).
  - Se invoca `lv_obj_invalidate_area(obj, &d)` únicamente sobre esas filas.
- Durante el evento `LV_EVENT_DRAW_MAIN`:
  - Se itera sobre las filas visibles dentro del área de recorte (`layer->_clip_area`).
  - Se agrupan caracteres contiguos con los mismos atributos para dibujarlos en un solo paso con `lv_draw_label()`, maximizando el rendimiento del renderizador de LVGL v9.5.

### 2.4. Scrollback Táctil en PSRAM
- Se mantiene un buffer circular de 1,000 líneas (`SbCell`) en memoria PSRAM externa.
- Soporta arrastre vertical táctil (`LV_EVENT_PRESSING` / gestos) para revisar la historia previa, con barra indicadora de posición en el margen derecho.
- Tocar la pantalla regresa automáticamente a la vista en vivo de la terminal.

---

## 3. Interfaces de Comunicación y Flujo de Datos

### 3.1. Entrada de Datos (Recepción: Host -> Terminal)
```cpp
void TerminalDisplay::appendText(const char* text, size_t len) {
    if (!m_vt || !text || len == 0) return;
    // Procesamiento ONLCR si no estamos en alternate screen (curses)
    vterm_write_onlcr(text, len);
}
```

### 3.2. Salida de Datos (Transmisión: Terminal -> Host)
- Teclas especiales de la barra rápida (`ESC`, `TAB`, `^C`, flechas direccionales) envían secuencias de escape estándar hacia el stream activo (`m_activeStream->write(...)`):
  - `↑`: `\x1B[A`
  - `↓`: `\x1B[B`
  - `→`: `\x1B[C`
  - `←`: `\x1B[D`
  - `TAB`: `\t`
  - `ESC`: `\x1B`
  - `Ctrl+C`: `\x03`
  - `Ctrl+D`: `\x04`

---

## 4. Plan de Implementación por Fases

| Fase | Módulo | Archivos Involucrados | Objetivo |
| :---: | :--- | :--- | :--- |
| **Fase 1** | **Integración de `libvterm`** | `core/src/terminal/libvterm/` | Copiar fuentes C de `libvterm`, configurar alocador en PSRAM y registrar en `core/CMakeLists.txt` y `bsp/esp32_s3_jc3248/platformio.ini`. |
| **Fase 2** | **Fuente Monoespaciada** | `core/src/ui/fonts/cbdos_font_mono_17.c` | Incorporar la fuente DejaVu Sans Mono 17 con glifos de dibujo de cajas y tabla ASCII. |
| **Fase 3** | **Refactorización de `TerminalDisplay`** | `core/src/ui/components/terminal/TerminalDisplay.hpp/.cpp` | Implementar el widget de celdas LVGL v9.5, callbacks de daño, paleta Tango ANSI y buffer de scrollback. |
| **Fase 4** | **Enriquecimiento de Controles e Interacción** | `core/src/ui/components/terminal/TerminalCommandBar.*`, `TerminalView.*` | Integrar flechas de navegación y combinaciones de control con el teclado en pantalla y teclado físico USB Host. |
| **Fase 5** | **Compilación Cruzada y Validación** | ESP-IDF (ESP32-P4) y PlatformIO (ESP32-S3) | Validar compilación limpia sin errores de enlazado ni dependencias rotas en ambas arquitecturas. |

---

## 5. Bitácora de Validación en Hardware y Roadmap de Renderizado

### 5.1. Pánico por Puntero a Pila en `libvterm` (Resuelto ✅)
- **Síntoma:** `Guru Meditation Error: Core 1 panic'ed (Instruction access fault). MEPC: 0x0000000c RA: 0x400f5596 (damagerect at screen.c:158)`.
- **Causa Raíz:** La estructura `VTermScreenCallbacks` fue declarada como variable local en la pila de `TerminalDisplay::create()`. `vterm_screen_set_callbacks` guarda el puntero directo sin copiar por valor. Al salir de la función, la pila fue reciclada y el puntero al callback `damage` quedó apuntando a memoria basura (`0x0000000c`).
- **Solución:** Declarar `s_screenCallbacks` como `static const VTermScreenCallbacks` persistente e inmutable en Flash/ROM.

### 5.2. Conflicto de Etiquetas de Color `#RRGGBB` y Cursor en LVGL v9 (Pendiente de Migración ⚠️)
- **Síntoma:** Al recibir comandos interactivos o al posicionarse el cursor, aparecen caracteres literales en pantalla como `#121417 _`, `#34e2e2 `, y el texto se desalinea o quiebra en múltiples renglones.
- **Causa Raíz:** 
  1. En LVGL v8 existía la función `lv_label_set_recolor()` que interpretaba `#RRGGBB texto#` como colores dinámicos en tiempo de ejecución.
  2. En **LVGL v9**, este mecanismo fue removido del core de `lv_label`.
  3. Al formatear los colores en `TerminalDisplay::refreshDisplay()`, las cadenas `#RRGGBB ` se inyectan en el string del label y se imprimen **literalmente como caracteres visibles**, sumando entre 8 y 9 letras por cada cambio de color.
  4. Este exceso de caracteres artificiales excede el ancho físico de columnas (`m_cols`), forzando saltos de línea prematuros en cascada y quebrando la alineación del cursor y de los prompts remotos.
- **Hoja de Ruta (Roadmap):** 
  - La arquitectura de `TerminalDisplay` está encapsulada modularmente respecto a `TerminalView`.
  - La solución definitiva consiste en migrar el dibujo del widget al estándar nativo de LVGL v9: interceptar `LV_EVENT_DRAW_MAIN` y utilizar `lv_draw_rect` (para fondos de celda y cursor) y `lv_draw_label` (para glifos) directamente sobre el canvas/layer, eliminando por completo la concatenación de cadenas y las etiquetas de texto.

### 5.3. Retorno de Carro `CR` vs `CRLF` y Prompts Duplicados
- **Síntoma:** Al enviar comandos a routers (MikroTik RouterOS / Cisco / Linux), el prompt `[user@device] > ` se dibuja repetido 2 o 3 veces y las letras del autocompletado caen escalonadas.
- **Causa:** 
  1. En consolas de terminal interactivas, la tecla Enter envía únicamente un byte `\r` (CR, `0x0D`). Enviar `CRLF` (`\r\n`) despacha dos comandos consecutivos al PTY del servidor: el comando solicitado + un Enter en blanco, provocando la emisión de un prompt redundante.
  2. Los mensajes de estado locales (`[SSH] Conectando...`, `Sesión establecida`) inyectados en el buffer antes del inicio empujan hacia abajo el primer prompt del servidor.
- **Ajustes:** Establecer `CR` como fin de línea por defecto en la barra de comandos y limpiar la pantalla (`m_display.clear()`) en el momento en que se confirma la sesión interactiva para que el shell inicie en la celda (0,0).
