# Especificación Técnica: Mecanismo de Rotación y Vista Panorámica Declarativa de Aplicaciones

- **Estado:** Propuesta Técnica / En Revisión
- **Fecha:** 2026-11-02
- **Autor:** CBDos Architecture & Embedded Systems Team
- **Componentes Afectados:** `core/include/cbdos/display.hpp`, `core/src/ui/views/BaseView.hpp`, `core/src/ui/UIManager.*`, `bsp/esp32_p4_jc4880/hal/`, `bsp/esp32_s3_jc3248/hal/`

---

## 1. Motivación y Objetivos

Actualmente, el sistema operativo CyBerDeck OS (CBDos) opera por defecto en modo **Portrait** nativo:
- **ESP32-P4 (JC4880):** 480 × 800 píxeles.
- **ESP32-S3 (JC3248):** 320 × 480 píxeles.

Sin embargo, ciertas aplicaciones y servicios sufren severamente por la falta de ancho horizontal:
1. **Terminal Serial / SSH (`TerminalView`):** Requiere un ancho estándar de al menos 80 columnas de texto monoespaciado para comandos Unix, tablas (`htop`, `ps`, `netstat`), depuración GDB y CLI de red.
2. **Cliente KVM / VNC Remoto (`KvmClientView`):** Los escritorios y servidores remotos operan en relaciones de aspecto panorámicas (16:9, 16:10, 4:3). En vertical se generan franjas negras masivas y el escalado reduce el texto a proporciones ilegibles.
3. **Herramientas de Análisis de Red y RF (`RestProber`, `LanRecon`, `SdrView`):** Se benefician enormemente de un diseño de doble columna (split-view: lista/petición a la izquierda, inspector/respuesta a la derecha) y espectrogramas horizontales de cascada (waterfall).
4. **TableHub KDS / Tabletop:** Las estaciones de cocina y pedidos operan universalmente en pantallas apaisadas para visualizar comandas simultáneas.

### Objetivos:
1. Crear un **mecanismo declarativo en el ciclo de vida de las vistas** (`BaseView`), de modo que una aplicación simplemente declare su preferencia de orientación (`Portrait`, `Landscape`, `Auto`, `Inherit`).
2. Implementar la **orquestación automática en `UIManager`**: Conmutar la rotación del display al hacer `pushView` y restaurar limpiamente la orientación previa de la vista padre al hacer `popView`.
3. Garantizar **soporte multiplataforma determinista y transparente**:
   - **ESP32-S3:** Rotación en bus QSPI AXS15231B + matriz táctil.
   - **ESP32-P4:** Rotación acelerada para bus MIPI-DSI DPI continuo (PPA por hardware o `lv_draw_rotate` optimizado).
4. Proveer un control manual en la `HeaderBar` o `QuickSettingsPanel` para rotación libre cuando el usuario lo desee.

---

## 2. Arquitectura de Software (Core Agnóstico)

### 2.1. Tipos de Orientación en `cbdos::display` (`core/include/cbdos/display.hpp`)

```cpp
namespace cbdos {
namespace display {

enum class Orientation : uint8_t {
    Portrait          = 0, // 0°: Vertical estándar (480x800 en P4, 320x480 en S3)
    Landscape         = 1, // 90°: Panorámico apaisado (800x480 en P4, 480x320 en S3)
    PortraitInverted  = 2, // 180°: Vertical invertido
    LandscapeInverted = 3  // 270°: Panorámico invertido
};

bool setOrientation(Orientation orientation);
Orientation getOrientation();
bool isLandscape();

} // namespace display
} // namespace cbdos
```

### 2.2. Política de Orientación en `BaseView` (`core/src/ui/views/BaseView.hpp`)

Cada vista declara su comportamiento respecto a la pantalla:

```cpp
enum class ViewOrientation {
    Inherit,    // Mantiene la orientación actual activa (Dashboard, modales, etc.)
    Portrait,   // Fuerza orientación vertical (ej. widgets de teléfono/asistente)
    Landscape,  // Fuerza orientación horizontal panorámica (Terminal, KVM, KDS, etc.)
    Auto        // Permite conmutar libremente según sensor o botón de usuario
};

class BaseView {
public:
    explicit BaseView(const std::string& name, 
                      ViewMode mode = ViewMode::Translucent,
                      ViewOrientation orientation = ViewOrientation::Inherit)
        : m_name(name), m_container(nullptr), m_visible(false), 
          m_viewMode(mode), m_orientation(orientation) {}

    ViewOrientation getViewOrientation() const { return m_orientation; }
    void setViewOrientation(ViewOrientation ori) { m_orientation = ori; }

    virtual void onOrientationChanged(Orientation newOri) {}
    ...
protected:
    ViewOrientation m_orientation;
};
```

### 2.3. Orquestación del Ciclo de Vida en `UIManager` (`core/src/ui/UIManager.cpp`)

1. **Entrada a la Vista (`pushView(view)`):**
   ```cpp
   ViewOrientation targetOri = view->getViewOrientation();
   if (targetOri == ViewOrientation::Landscape && !cbdos::display::isLandscape()) {
       cbdos::display::setOrientation(cbdos::display::Orientation::Landscape);
   } else if (targetOri == ViewOrientation::Portrait && cbdos::display::isLandscape()) {
       cbdos::display::setOrientation(cbdos::display::Orientation::Portrait);
   }
   applyViewMode(view->getViewMode()); // Recalcula ancho y alto según nueva resolución
   ```

2. **Salida de la Vista (`popView()`):**
   ```cpp
   m_viewStack.pop_back();
   if (!m_viewStack.empty()) {
       auto parentView = m_viewStack.back();
       ViewOrientation parentOri = parentView->getViewOrientation();
       if (parentOri == ViewOrientation::Portrait && cbdos::display::isLandscape()) {
           cbdos::display::setOrientation(cbdos::display::Orientation::Portrait);
       } else if (parentOri == ViewOrientation::Landscape && !cbdos::display::isLandscape()) {
           cbdos::display::setOrientation(cbdos::display::Orientation::Landscape);
       }
       applyViewMode(parentView->getViewMode());
       parentView->onShow();
   }
   ```

3. **Reajuste Dinámico de Componentes de Sistema:**
   - `HeaderBar`: Al recibir `LV_EVENT_RESOLUTION_CHANGED`, se reubican el título, reloj, iconos de batería/Wi-Fi/Bluetooth y botón Atrás al nuevo ancho (800 px o 480 px).
   - `m_contentContainer`: Se redimensiona automáticamente al 100% de la nueva anchura y `(screenHeight - headerHeight)`.

---

## 3. Implementación de Hardware y Nivel de BSP

### 3.1. ESP32-S3 (JC3248W535: AXS15231B + Touch AXS15231B)
- **Principio de Dueño Único de Rotación (Single Owner):** Para evitar la doble transposición (LVGL + Hardware/Canvas), LVGL 9 asume la autoridad lógica de la rotación mediante `lv_display_set_rotation(s_lv_display, rot)`.
- **Canvas y Flush:** En `lvgl_display_flush_cb`, las áreas y mapas de píxeles se entregan ya orientados lógicamente por LVGL. El canvas de hardware se mantiene alineado y el driver táctil `JC3248W535_Touch::setRotation` se sincroniza únicamente si no se delega el mapeo al pipeline interno de `lv_indev`.
- **Impacto en PSRAM (S3):** El búfer de pantalla completa existente (300 KB) se reutiliza; no se requiere memoria adicional en PSRAM externa.

### 3.2. ESP32-P4 (JC4880: ST7701S MIPI-DSI DPI + Touch GT911)
- **Panel DPI y Canal Directo:** El controlador de hardware MIPI-DSI DPI en ESP32-P4 escanea un búfer físico de 480 × 800 píxeles continuos a 34 MHz.
- **Pipeline de Transposición Panorámica (800 × 480 -> 480 × 800):**
  - LVGL 9 dibuja lógicamente en modo horizontal (800 × 480).
  - Al operar en modo panorámico, el búfer de dibujo lógico (800×480) se transpone hacia los búferes de escaneo físico del controlador DPI (480×800) en el `flushCallback`.
  - **Aceleración por Hardware:** Se emplea el periférico **2D PPA (Pixel Processing Accelerator)** del ESP32-P4 mediante operación DMA síncrona/asíncrona, con fallback a `lv_draw_rotate` por áreas sucias.
  - **Presupuesto de Memoria (PSRAM P4):**
    - Búferes DPI físicos existentes: 2 × (480 × 800 × 2) = 1.5 MB (fijos).
    - Búfer lógico de dibujo LVGL en PSRAM: 1 × (800 × 480 × 2) = 768 KB (asignado dinámicamente o reservado al conmutar).
    - Con 16 MB de PSRAM en el módulo P4, el incremento de 768 KB representa únicamente el 4.8% de la memoria, siendo completamente seguro y viable.
- **Entrada Táctil (GT911):** `TouchHAL` entrega coordenadas físicas crudas (0..480, 0..800). Al vincular `s_lv_indev` con `s_lv_display`, LVGL 9 aplica la transformación de coordenadas según la rotación activa sin alterar los drivers de bajo nivel.

---

## 4. Adaptación de Aplicaciones Clave

### 4.1. Terminal SSH y Serial (`TerminalView`)
- Constructor: `TerminalView::TerminalView() : BaseView("Terminal", ViewMode::Solid, ViewOrientation::Landscape)`.
- **Capacidad de Columnas según Target:**
  - **ESP32-P4 (800 px ancho):** Con `cbdos_font_mono_17` (ancho de glifo ~10 px), acomoda **78 a 80 columnas completas**, cumpliendo el estándar VT100 / POSIX sin saltos de línea forzados.
  - **ESP32-S3 (480 px ancho):** Con `cbdos_font_mono_17`, acomoda **46 a 48 columnas** (frente a las 30 columnas en vertical), representando un incremento de área útil del 60%.
- Barra de estado / `HeaderBar` adopta un alto compacto (32–38 px) en modo horizontal para maximizar las líneas de terminal visibles.

### 4.2. Futuro Cliente KVM / VNC (`KvmClientView`)
- Relación de aspecto 16:9 y 4:3 se adaptan de forma natural al modo 800×480 sin compresión vertical.

### 4.3. TableHub KDS / Tabletop
- Disposición de tarjetas de comandas en rejilla horizontal multipedido (3 o 4 columnas de tickets simultáneos).

---

## 5. Plan de Ejecución y Verificación

1. **Fase 1 (Core):**
   - Incorporar `Orientation` y prototipos en `cbdos/display.hpp`.
   - Incorporar `ViewOrientation` y constructor extendido en `BaseView.hpp`.
   - Actualizar `UIManager.cpp` con la lógica de conmutación y restauración en `pushView` / `popView`.
2. **Fase 2 (BSP S3 & P4):**
   - Implementar `cbdos::display::setOrientation` en `hal_display_s3.cpp` y `hal_display_p4.cpp` / `LVGL_Port.cpp`.
   - Verificar la coherencia de buffers y táctil en rotación 0° y 90°.
3. **Fase 3 (Aplicación Terminal):**
   - Activar `ViewOrientation::Landscape` en `TerminalView`.
   - Verificar que al pulsar "Atrás" se regresa al Dashboard en Portrait de forma limpia e instantánea.
4. **Fase 4 (Compilación y Verificación Cruzada):**
   - Compilación limpia multi-target:
     - `idf.py -C bsp/esp32_p4_jc4880 build`
     - `pio run -d bsp/esp32_s3_jc3248`
