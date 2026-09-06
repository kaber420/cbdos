# 🎨 Especificación Técnica Completa: CBD Paint (`cbd_paint.luapp`)

**Fecha:** 24 de Agosto, 2026  
**Versión:** 1.0.0 (Master Blueprint)  
**Estado:** Especificación de Diseño y Arquitectura Aprobada  
**Entorno de Ejecución:** CBDos Lua++ Runtime (`LuappView` + `cbdos.ui` + `cbdos.canvas`)  
**Target de Archivos:** MicroSD (`/sdcard/apps/cbd_paint.luapp` y `/sdcard/drawings/`)  

---

## 📌 1. Visión General y Filosofía de Diseño

**CBD Paint** es la suite de dibujo y edición gráfica matricial para el sistema operativo **CBDos**. Inspirada en la simplicidad e inmediatez de aplicaciones clásicas como **MS Paint (Windows 95/98)** y **Deluxe Paint (Commodore Amiga)**, combinada con herramientas modernas de **Pixel Art** (como Aseprite y PICO-8).

### Objetivos Clave
1. **Experiencia Táctil de Alta Precisión:** Totalmente operable mediante pantalla táctil capacitiva (GT911), con soporte de zoom macro (hasta 16x) y rejilla para edición píxel a píxel con dedos o stylus.
2. **Herramientas Clásicas Completas:** Lápiz, Pincel, Borrador, Bote de Pintura (Relleno por Inundación), Cuentagotas, Líneas, Formas Geométricas y Texto.
3. **Deshacer/Rehacer (Undo/Redo):** Pila de estados en memoria PSRAM para revertir trazos accidentales.
4. **Compatibilidad Universal BMP:** Guardado y carga directa en formato estándar Windows BMP (24-bit / 16-bit RGB565) directamente en la tarjeta MicroSD.
5. **Zero Platform Pollution:** Lógica de pintado desacoplada, ejecutada sobre el motor Lua++ de CBDos con aceleración de primitivas críticas en C++ vía `cbdos.canvas`.

---

## 🖥️ 2. Diseño de la Interfaz de Usuario (UI Layout)

La interfaz se distribuye verticalmente aprovechando tanto pantallas verticales (480x800) como horizontales (320x480):

```
┌───────────────────────────────────────────────────────────────────────────┐
│ [←] CBD PAINT           [↶] [↷]    [🔍 4x]  [# GRID]   [💾]  [⚙]          │ <- HeaderBar
├───────────────────────────────────────────────────────────────────────────┤
│ [Herramientas]                                                            │
│ ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐             │
│ │  ✎  │  🖌 │  ▧  │  🪣  │  💉 │  ✋  │  ╱  │  ▢  │  ◯  │  T  │             │ <- Toolbar
│ └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘             │
│ [Grosor: ● 1px | ● 2px | ● 4px | ● 8px]   [Modo Forma: ⬚ Borde | ⬛ Lleno] │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│   LIENZO VIRTUAL / VIEWPORT INTERACTIVO                                   │
│   ┌─────────────────────────────────────────────────────────────────┐     │
│   │                                                                 │     │
│   │                 ┌───┬───┬───┬───┬───┬───┐                       │     │
│   │                 │   │   │ ■ │ ■ │   │   │  <- Píxeles escalados │     │
│   │                 ├───┼───┼───┼───┼───┼───┤     con Rejilla       │     │
│   │                 │   │ ■ │ ■ │ ■ │ ■ │   │     activa            │     │
│   │                 └───┴───┴───┴───┴───┴───┘                       │     │
│   │                                                                 │     │
│   └─────────────────────────────────────────────────────────────────┘     │
│                                                                           │
├───────────────────────────────────────────────────────────────────────────┤
│ [PALETA: Modo Clásico / PICO-8 / Cyberpunk]  [Color 1: █] [Color 2: █]     │
│ ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐       │
│ │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │ A │ B │ C │ D │ E │ F │       │ <- Paleta de Color
│ └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘       │
├───────────────────────────────────────────────────────────────────────────┤
│ Pos: X: 34, Y: 18 | Lienzo: 128x128 | Zoom: 4x | PSRAM: 28.4 MB Libre     │ <- Status Bar
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 🧰 3. Caja de Herramientas (Toolbox)

| Icono | Herramienta | Descripción y Comportamiento |
|---|---|---|
| ✎ | **Lápiz (Pencil)** | Trazo continuo puro de 1 píxel sin suavizado. Ideal para pixel art de alta precisión. |
| 🖌 | **Pincel (Brush)** | Trazo continuo con grosor configurable (1px, 2px, 4px, 8px) con cabezal cuadrado o redondo. |
| ▧ | **Borrador (Eraser)** | Pinta con el color de fondo actual (Color 2) con tamaño ajustable (2px, 4px, 8px, 16px). |
| 🪣 | **Bote de Pintura (Bucket)** | Relleno por inundación (*Flood Fill*) de áreas cerradas de un mismo color. |
| 💉 | **Cuentagotas (Picker)** | Muestrea el color del píxel pulsado y lo asigna como Color 1 (Primario). |
| ✋ | **Mano / Pan (Hand)** | Arrastra el lienzo en pantalla cuando el nivel de zoom supera los límites del viewport. |
| ╱ | **Línea Recta (Line)** | Dibuja una línea recta entre el punto inicial de contacto y el punto de liberación. |
| ▢ | **Rectángulo (Rect)** | Traza un rectángulo o cuadrado. Selector de modo: solo contorno o relleno sólido. |
| ◯ | **Círculo (Circle)** | Traza un círculo o elipse. Selector de modo: solo contorno o relleno sólido. |
| 🔤 | **Texto (Text Stamp)** | Estampa texto tipográfico bitmap clásico de 8x8 en la posición tocada. |

---

## 🔍 4. Motor de Viewport, Zoom y Pixel Grid

Para garantizar que los dibujos no pierdan fidelidad ni consuman memoria innecesaria, se utiliza una arquitectura de **Lienzo Desacoplado**:

```
[ Buffer de Imagen Original ]   ──(Escalado Nearest-Neighbor)──>   [ Canvas de LVGL en Pantalla ]
    (Ej: 64x64 píxeles)                                                  (Ej: 400x400 píxeles)
```

### A. Factores de Zoom Soportados
- **1x (Escala Real):** Renderizado directo 1:1.
- **2x (Zoom Básico):** Cada píxel virtual ocupa 2x2 píxeles de pantalla.
- **4x (Zoom Detalle):** Cada píxel virtual ocupa 4x4 píxeles de pantalla.
- **8x (Pixel Art Standard):** Cada píxel virtual ocupa 8x8 píxeles de pantalla.
- **16x (Macro Edición):** Cada píxel virtual ocupa 16x16 píxeles de pantalla, ideal para retocar iconos de 16x16 o 32x32 con el dedo sin fallar el toque.

### B. Rejilla Dinámica (Pixel Grid)
- En niveles de zoom $\ge 4x$, el usuario puede activar la **Rejilla de Píxeles**.
- Dibuja líneas tenues de contraste (`#2A3547`) entre las fronteras de los píxeles magnificados para delimitar con exactitud cada celda de color.

### C. Mapeo Táctil Bidireccional
Transformación de la coordenada táctil de pantalla $(T_x, T_y)$ a la coordenada virtual del lienzo $(L_x, L_y)$:
$$L_x = \left\lfloor \frac{T_x - \text{PanX}}{\text{ZoomFactor}} \right\rfloor, \quad L_y = \left\lfloor \frac{T_y - \text{PanY}}{\text{ZoomFactor}} \right\rfloor$$

---

## 🎨 5. Sistema de Paletas de Color

CBD Paint incluye un conmutador de paletas rápidas de 16 colores seleccionados:

### 1. Paleta Clásica (MS Paint / Windows Standard)
| Índice | Nombre | Hex | Índice | Nombre | Hex |
|---|---|---|---|---|---|
| 0 | Negro | `#000000` | 8 | Gris Claro | `#C0C0C0` |
| 1 | Marrón Oscuro | `#800000` | 9 | Rojo Brillante | `#FF0000` |
| 2 | Verde Oscuro | `#008000` | 10 | Verde Lima | `#00FF00` |
| 3 | Ocre / Oliva | `#808000` | 11 | Amarillo | `#FFFF00` |
| 4 | Azul Marino | `#000080` | 12 | Azul Eléctrico | `#0000FF` |
| 5 | Púrpura | `#800080` | 13 | Magenta | `#FF00FF` |
| 6 | Verde Azulado | `#008080` | 14 | Cian Brillante | `#00FFFF` |
| 7 | Gris Medio | `#808080` | 15 | Blanco Puro | `#FFFFFF` |

### 2. Paleta PICO-8 (Retro Fantasy Console)
`#000000`, `#1D2B53`, `#7E2553`, `#008751`, `#AB5236`, `#5F574F`, `#C2C3C7`, `#FFF1E8`,  
`#FF004D`, `#FFA300`, `#FFEC27`, `#00E436`, `#29ADFF`, `#83769C`, `#FF77A8`, `#FFCCAA`.

### 3. Paleta Cyberpunk / Neon CBD
`#0A0E17`, `#1A2238`, `#4E148C`, `#8500C0`, `#D600FF`, `#FF0055`, `#FF5722`, `#FFB300`,  
`#00E5FF`, `#00F5D4`, `#00B4D8`, `#0077B6`, `#7209B7`, `#F72585`, `#E0E6ED`, `#FFFFFF`.

### 4. Paleta GameBoy (Classic 4-Shade Green)
`#0F380F` (Verde Muy Oscuro), `#306230` (Verde Oscuro), `#8BAC0F` (Verde Medio), `#9BBC0F` (Verde Claro).

---

## 🔄 6. Sistema de Deshacer / Rehacer (Undo / Redo Buffer)

- **Capacidad:** 8 a 16 niveles de historial en PSRAM.
- **Mecanismo:** Al iniciar un nuevo trazo (`PRESSED`), se realiza una instantánea compacta (*snapshot*) del buffer del lienzo en una pila circular.
- **Rendimiento:** Para un lienzo de 128x128 píxeles en RGB565 (32 KB por frame), almacenar 10 estados de Undo requiere únicamente **320 KB de PSRAM**, lo cual es despreciable frente a los 32 MB disponibles en ESP32-P4 o los 8 MB en ESP32-S3.

---

## 💾 7. Sistema de Archivos, Guardado y Carga (BMP Engine)

### A. Estructura de Directorios en MicroSD
- `/sdcard/drawings/`: Directorio principal donde se almacenan las creaciones del usuario.
- Nomenclatura automática: `/sdcard/drawings/draw_YYYYMMDD_HHMMSS.bmp` o selección de nombre mediante diálogo.

### B. Formato de Exportación: Windows BMP 24-bit / 16-bit
- Encabezado estándar `BITMAPFILEHEADER` (14 bytes) + `BITMAPINFOHEADER` (40 bytes).
- Sin compresión (`BI_RGB`).
- Los archivos exportados son 100% compatibles con Windows, macOS, Linux, navegadores y visores de imagen nativos.

---

## ⚙️ 8. Extensiones Requeridas en el Core (`LuaBridge` / `cbdos.canvas`)

Para dotar a Lua de la máxima fluidez y capacidades interactivas, se extienden las APIs de `cbdos.canvas`:

1. **`cbdos.canvas.on_touch(canvas, function(event, x, y) ... end)`**:
   - `event`: `"pressed"`, `"drag"`, `"released"`.
   - Permite trazar de forma fluida mientras el usuario desliza el dedo por la pantalla.
2. **`cbdos.canvas.get_px(canvas, x, y)` -> `hexColor`**:
   - Muestreo de color de un píxel específico para la herramienta cuentagotas y algoritmos de relleno.
3. **`cbdos.canvas.flood_fill(canvas, x, y, hexColor)`**:
   - Relleno por inundación implementado en C++ con cola BFS optimizada para velocidad instantánea.
4. **`cbdos.canvas.save_bmp(canvas, filepath)` -> `boolean`**:
   - Exportación directa del buffer del canvas a archivo `.bmp` en la MicroSD.
5. **`cbdos.canvas.load_bmp(canvas, filepath)` -> `boolean`**:
   - Carga y renderizado de un archivo `.bmp` en el canvas.

---

## 📅 9. Fases de Implementación

- [ ] **Fase 1: Extensión HAL & LuaBridge:**
  - Implementación de `on_touch`, `get_px`, `flood_fill`, `save_bmp` y `load_bmp` en [LuaBridge.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/LuaBridge.cpp).
  - Verificación de compilación en ESP32-P4 (`idf.py build`) y ESP32-S3 (`pio run`).
- [ ] **Fase 2: Motor de Lienzo y Viewport en Lua (`cbdrawer.luapp`):**
  - Implementación de buffers virtuales, escalado con zoom (1x-16x) y rejilla de píxeles.
- [ ] **Fase 3: Barra de Herramientas y Paletas Duales:**
  - Integración de Lápiz, Pincel, Borrador, Cuentagotas, Línea, Rectángulo, Círculo y Texto.
  - Conmutador de 4 paletas y selector de grosores.
- [ ] **Fase 4: Historial de Deshacer/Rehacer y Diálogos de Archivo:**
  - Pila de Undo/Redo en memoria.
  - Diálogo de guardar archivo en `/sdcard/drawings/` con teclado táctil virtual.
