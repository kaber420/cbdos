# Especificación Técnica: Modos de Vista Adaptativos (`ViewMode`) y Rediseño de HeaderBar

**Proyecto:** CBDos (Core UI)  
**Versión:** 1.0  
**Fecha:** Septiembre 2026  
**Autor:** Equipo de Desarrollo CBDos  
**Estado:** Propuesta Aprobada / Listo para Implementación  

---

## 1. Contexto y Diagnóstico del Problema

### 1.1 El Descuadre Estético Actual
CBDos ha evolucionado hacia un lenguaje de diseño moderno: iconos squircle de gran tamaño con bordes reactivos neón al pulsar (`LV_STATE_PRESSED`), fondos oscuros nórdicos/cyberpunk (`#161821`, `#1B1E29`) y widgets táctiles fluidos.

Sin embargo, el manejo del área de pantalla de las aplicaciones presenta una desconexión estética y funcional:
1. En [`UIManager.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/UIManager.cpp#L88), el contenedor de aplicaciones (`m_contentContainer`) está rígidamente fijado en:
   ```cpp
   lv_obj_set_pos(m_contentContainer, 0, 58);
   lv_obj_set_height(m_contentContainer, screenHeight - 58);
   ```
2. La [`HeaderBar`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/components/HeaderBar.cpp) está configurada como una "isla flotante" centrada de 44px de altura, separada a 8px del techo de la pantalla (`Y = 8`), ocupando el 94% del ancho.
3. **El conflicto de estilos:**
   * **Aplicaciones Translúcidas (Glassmorphism / Desktop Widgets):** Como el Dashboard o pantallas con tarjetas flotantes, tienen fondo 100% transparente. El wallpaper del sistema se ve continuo de fondo y la isla flotante luce armónica.
   * **Aplicaciones Sólidas / Workspace (Terminal, Editor, BadUSB/Kerberos, Ajustes):** Requieren fondo oscuro opaco (`#161821`) para contraste y lectura. Al arrancar en `Y = 58`, se genera una franja vacía superior donde el wallpaper de fábrica o personalizado se asoma alrededor de la HeaderBar, rematando con un corte recto artificial donde empieza la caja de la aplicación.
   * **Pérdida de Espacio Vertical:** En resoluciones compactas (480x800 en ESP32-P4 y especialmente 320x480 en ESP32-S3), perder 58 píxeles en aplicaciones de consola o edición roba entre el 12% y el 15% del área útil.

---

## 2. Arquitectura de Modos de Vista (`ViewMode`)

Para preservar la libertad de diseño sin forzar a todas las aplicaciones a un único patrón rígido, se introduce el concepto de `ViewMode` en el contrato base [`BaseView`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/BaseView.hpp).

```
                      ┌─────────────────────────────────────────┐
                      │            BaseView (Core)              │
                      │  - m_viewMode = ViewMode::Translucent   │
                      └────────────────────┬────────────────────┘
                                           │
                ┌──────────────────────────┼──────────────────────────┐
                ▼                          ▼                          ▼
     ┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────────┐
     │ ViewMode::Translucent│   │   ViewMode::Solid   │    │ ViewMode::Immersive │
     ├─────────────────────┤    ├─────────────────────┤    ├─────────────────────┤
     │ • Dashboard         │    │ • Terminal          │    │ • Juegos / Emulador │
     │ • Galería (Cards)   │    │ • Editor de Texto   │    │ • Visor Multimedia  │
     │ • Audio Player      │    │ • Configuración     │    │ • Terminal Pro      │
     │ • Widgets flotantes │    │ • Gestor Archivos   │    │ • Lottie Fullscreen │
     └─────────────────────┘    └─────────────────────┘    └─────────────────────┘
```

### 2.1 Definición del Enumerador
```cpp
namespace cbdos {
namespace ui {

enum class ViewMode {
    Translucent, // Por defecto: Isla flotante sobre el wallpaper, contenedor en Y=58. Preserva compatibilidad total.
    Solid,       // Ventana continua: HeaderBar acoplada a Y=0 (100% ancho, fondo sólido continuo). Cero wallpaper asomándose.
    Immersive    // Pantalla completa pura: Contenedor en Y=0 (100% alto). HeaderBar oculta. Despliegue temporal por gesto.
};

} // namespace ui
} // namespace cbdos
```

---

## 3. Especificación Técnica por Componente

### 3.1 [`BaseView.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/BaseView.hpp)
* **Contrato extendido:**
  ```cpp
  class BaseView {
  public:
      explicit BaseView(const std::string& name, ViewMode mode = ViewMode::Translucent)
          : m_name(name), m_container(nullptr), m_visible(false), m_viewMode(mode) {}

      ViewMode getViewMode() const { return m_viewMode; }
      void setViewMode(ViewMode mode) { m_viewMode = mode; }

  protected:
      std::string m_name;
      lv_obj_t* m_container;
      bool m_visible;
      ViewMode m_viewMode;
  };
  ```
* **Retrocompatibilidad:** El parámetro por defecto `ViewMode::Translucent` garantiza que ninguna vista existente se rompa al compilar.

---

### 3.2 [`HeaderBar`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/components/HeaderBar.hpp) Adaptativa y Rediseño

#### A. Adaptación Dinámica según `ViewMode`
La `HeaderBar` dispondrá del método `void setMode(ViewMode mode)`:

1. **Modo `Translucent`:**
   * Ancho: 94% (`LV_PCT(94)`), Alto: 44px.
   * Posición: Centrada en `Y = 8` (`LV_ALIGN_TOP_MID`).
   * Fondo: `#1B1E29` con `LV_OPA_70` (Glassmorphism), borde de 1px `#3B4252`, radio de esquinas 14px.
2. **Modo `Solid`:**
   * Ancho: 100% (`LV_PCT(100)`), Alto: 40px.
   * Posición: Pegada al techo en `Y = 0`, `X = 0`.
   * Fondo: `DefaultTheme::getBgColor()` (`#161821`) con `LV_OPA_COVER` (100% opaco).
   * Bordes: Radio 0, borde inferior sutil de 1px `#2D3748`.
   * **Efecto visual:** Se funde completamente con el fondo de la app, eliminando cualquier espacio muerto o wallpaper filtrado.
3. **Modo `Immersive`:**
   * La barra se oculta deslizándose hacia arriba fuera de la pantalla (`Y = -48` mediante `lv_anim_t`) o aplicando `LV_OBJ_FLAG_HIDDEN`.

#### B. Rediseño del Botón "Volver" (Look & Feel Moderno)
* Se descarta el botón rectangular antiguo de 84x30 con texto tosco.
* Se implementa un botón **Squircle Moderno**:
  * Dimensiones: 34x34 px o píldora compacta 54x30 px.
  * Icono: `LV_SYMBOL_LEFT` con tipografía de 16px en color Cyan Primario (`#00F5D4`).
  * Estado presionado (`LV_STATE_PRESSED`): Borde neón brillante con micro-glow cyan, en perfecta sintonía con las tarjetas del Dashboard.

---

### 3.3 [`UIManager`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/UIManager.cpp) y Gestor de Layout

Al transicionar entre vistas en `pushView()`, `popView()` y `openDashboard()`:
1. Se consulta el modo de la vista activa:
   ```cpp
   ViewMode mode = view->getViewMode();
   m_headerBar.setMode(mode);
   ```
2. Se reconfigura la geometría de `m_contentContainer`:
   * **Translucent:** `lv_obj_set_pos(m_contentContainer, 0, 58);`  
     `lv_obj_set_height(m_contentContainer, screenHeight - 58);`
   * **Solid:** `lv_obj_set_pos(m_contentContainer, 0, 40);`  
     `lv_obj_set_height(m_contentContainer, screenHeight - 40);`
   * **Immersive:** `lv_obj_set_pos(m_contentContainer, 0, 0);`  
     `lv_obj_set_height(m_contentContainer, screenHeight);`
3. **Soporte de Gesto en Modo Inmersivo:**
   * Un detector táctil en el borde superior (`Y = 0..16`) o callback `LV_EVENT_GESTURE` permite que al deslizar hacia abajo (swipe-down), la `HeaderBar` descienda animada flotando en `lv_layer_top()` como un Dynamic HUD temporal, replegándose automáticamente tras 3.5 segundos de inactividad o al pulsar Volver.

---

## 4. Clasificación Inicial de Aplicaciones Existentes

| Aplicación | Modo Recomendado | Justificación |
| :--- | :--- | :--- |
| **Dashboard** | `ViewMode::Translucent` | Vista principal con Wallpaper y tarjetas de apps de cristal. |
| **Terminal** | `ViewMode::Solid` (u Opcional `Immersive`) | Requiere fondo negro puro continuo `#161821`, pestañas serial/SSH y máxima altura vertical. |
| **Editor de Texto** | `ViewMode::Solid` | Entorno de trabajo continuo sin cortes de cabecera. |
| **Configuración** | `ViewMode::Solid` | Aspecto nativo de ventana de sistema pulida. |
| **Gestor de Archivos** | `ViewMode::Solid` | Lista limpia con cabecera de ruta y herramientas. |
| **Reproductor de Música** | `ViewMode::Translucent` | Interfaz estética con controles flotantes sobre el wallpaper. |
| **Lottie Test / Juegos** | `ViewMode::Immersive` | Pantalla completa total sin distracciones. |

---

## 5. Plan de Pruebas y Validación Multi-Target

1. **Compilación Limpia en Target ESP32-P4:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh
   cd bsp/esp32_p4_jc4880
   idf.py build
   ```
2. **Compilación Limpia en Target ESP32-S3:**
   ```bash
   pio run -d bsp/esp32_s3_jc3248
   ```
3. **Validación de Regresión:**
   * Comprobar que el Dashboard preserva al 100% su aspecto actual.
   * Comprobar que al abrir la Terminal la barra se acopla inmediatamente al techo sin franja de wallpaper.
   * Comprobar que al pulsar el nuevo botón Volver se retorna al Dashboard y la barra restaura su modo flotante de forma transparente.
