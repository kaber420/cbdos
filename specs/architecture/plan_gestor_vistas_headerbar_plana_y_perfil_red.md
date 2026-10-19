# 📋 Plan de Arquitectura: Gestor de Modos de Vista (`ViewMode`), HeaderBar Plana y Optimización de CPU en Perfil RED

**Documento:** `specs/architecture/plan_gestor_vistas_headerbar_plana_y_perfil_red.md`  
**Estado:** ✅ Completado / Implementado  
**Fecha:** 18 de Octubre de 2026  
**Autores:** Kaber420 (Lead Architect & Project Director) & Antigravity (Senior Embedded Architecture)  
**Dependencias:** [`specs/architecture/especificacion_modos_vista_adaptativos_y_redisenio_headerbar.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/especificacion_modos_vista_adaptativos_y_redisenio_headerbar.md), [`specs/architecture/plan_creacion_perfil_cyberdeck_red.md`](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/plan_creacion_perfil_cyberdeck_red.md)

---

## 🏛️ 1. Justificación y Objetivos de Diseño

El sistema operativo **CBDos** cuenta con una especificación previa de modos de vista adaptativos (`especificacion_modos_vista_adaptativos_y_redisenio_headerbar.md`). El presente plan extiende dicha arquitectura para resolver tres necesidades clave planteadas por la Dirección de Arquitectura:

1. **Gestor de Modos de Vista Declarativo (`ViewMode`):**  
   Permitir que cada vista (`BaseView`) defina de forma desacoplada cómo debe renderizarse en pantalla: con barra o sin ella, y con fondo translúcido o sólido continuo.
2. **Optimización Extrema de CPU / Batería:**  
   Detener el motor de partículas o lluvia Matrix (`AnimatedWallpaper::stop()`) en cuanto el usuario ingrese a una aplicación sólida o inmersiva, liberando el 100% de los ciclos de renderizado de fondo (60 FPS / timer de 16 ms) para la lógica de la aplicación activa. Reanudar la animación únicamente al volver al Dashboard.
3. **HeaderBar Plana Táctica en Perfil RED (`CONFIG_CBDOS_PROFILE_RED`):**  
   Sustituir el diseño de "isla flotante" translúcida por una barra de estado sólida, plana, de ancho completo (100% ancho, `Y = 0`, radio 0 px, borde inferior oscuro/rojo), perfectamente integrada con el chasis táctico de la Cyberdeck.
4. **Curaduría y Purga de Apps en Perfil RED:**  
   Eliminar del Dashboard del perfil RED las aplicaciones casuales/multimedia (Galería, Radio Web, Grabadora, Música, Lottie) e integrar las 4 aplicaciones tácticas maestras de RF (Wi-Fi Shield, BLE Shield, Wi-Fi Strike, BLE Strike).

---

## ⚙️ 2. Especificación del Gestor de Modos de Vista

### 2.1 Enumerador `ViewMode` en `BaseView.hpp`
Se ubica en `core/src/ui/views/BaseView.hpp`:

```cpp
namespace cbdos {
namespace ui {

enum class ViewMode {
    Translucent, // Isla flotante sobre wallpaper animado, contenedor en Y=58 (Dashboard, Widgets)
    Solid,       // Barra acoplada a Y=0 (100% ancho, radio 0, fondo opaco), contenedor en Y=38/40. Pausa el wallpaper animado.
    Immersive    // Pantalla completa pura: Contenedor en Y=0 (100% alto), HeaderBar oculta. Pausa el wallpaper animado.
};

class BaseView {
public:
    explicit BaseView(const std::string& name, ViewMode mode = ViewMode::Translucent)
        : m_name(name), m_container(nullptr), m_visible(false), m_viewMode(mode) {}
    
    virtual ~BaseView() = default;

    ViewMode getViewMode() const { return m_viewMode; }
    void setViewMode(ViewMode mode) { m_viewMode = mode; }

    // ... ciclo de vida existente (onCreate, onDestroy, onShow, onHide, onUpdate)
protected:
    std::string m_name;
    lv_obj_t* m_container;
    bool m_visible;
    ViewMode m_viewMode;
};

} // namespace ui
} // namespace cbdos
```

---

## 🔋 3. Ciclo de Vida y Optimización de CPU en `UIManager`

Actualmente, `AnimatedWallpaper` ejecuta un timer LVGL cada 16 ms recalculando física y dibujando partículas por canvas. Cuando una aplicación tiene fondo oscuro opaco, renderizar este fondo oculto es un desperdicio de ciclos y energía térmica.

### 3.1 Orquestación en `UIManager::pushView()` y `popView()`
En `core/src/ui/UIManager.cpp`:

```cpp
void UIManager::pushView(std::shared_ptr<BaseView> view) {
    if (!view) return;

    closeKeyboard();
    m_headerBar.clearRightAction();
    m_headerBar.showWifi(true);

    if (!m_viewStack.empty()) {
        m_viewStack.back()->onHide();
    }

    m_viewStack.push_back(view);
    
    // 1. Adaptar HeaderBar y geometría según ViewMode
    applyViewMode(view->getViewMode());

    // 2. Control de Ciclos de CPU: Pausar/Reanudar Wallpaper
    if (view->getViewMode() == ViewMode::Solid || view->getViewMode() == ViewMode::Immersive) {
        AnimatedWallpaper::getInstance().stop(); // Pausa el timer de 16 ms
    }

    view->onCreate(m_contentContainer);
    view->onShow();

    // Actualizar títulos y botón volver
    if (m_viewStack.size() > 1) {
        m_headerBar.setTitle(view->getViewName().c_str());
        m_headerBar.showBackButton(true, [this]() {
            this->popView();
        });
    } else {
        m_headerBar.setTitle("CBDos");
        m_headerBar.showBackButton(false);
    }
}

void UIManager::popView() {
    if (m_viewStack.size() <= 1) return;

    closeKeyboard();
    m_headerBar.clearRightAction();
    m_headerBar.showWifi(true);

    auto topView = m_viewStack.back();
    topView->onHide();
    topView->onDestroy();
    m_viewStack.pop_back();

    if (!m_viewStack.empty()) {
        auto current = m_viewStack.back();
        
        // 1. Restaurar modo de vista del padre
        applyViewMode(current->getViewMode());

        // 2. Si regresamos al Dashboard (Translucent), reanudar wallpaper animado
        if (current->getViewMode() == ViewMode::Translucent) {
            AnimatedWallpaper::getInstance().start();
        }

        current->onShow();
        // ... títulos y navegación
    }
}
```

### 3.2 Método Dinámico `applyViewMode(ViewMode mode)`
Calcula la altura y posición de `m_contentContainer` dinámicamente:
* **`ViewMode::Translucent`:**
  - HeaderBar en estilo isla (o normal en RED), `Y = 8`.
  - Contenedor en `Y = 58`, alto = `screenHeight - 58`.
  - Fondo del contenedor: `LV_OPA_TRANSP`.
* **`ViewMode::Solid`:**
  - HeaderBar acoplada en `Y = 0`, alto = 38 px (S3) / 44 px (P4), ancho = 100%, radio = 0.
  - Contenedor en `Y = 38` (S3) o `Y = 44` (P4), alto = `screenHeight - 38`.
  - Fondo del contenedor: Opaco (`#0A0E17` o `#000000` en RED).
* **`ViewMode::Immersive`:**
  - HeaderBar oculta (`lv_obj_add_flag(m_container, LV_OBJ_FLAG_HIDDEN)`).
  - Contenedor en `Y = 0`, alto = `screenHeight`.

---

## 🎨 4. Rediseño de la HeaderBar (Barra Plana vs Isla Flotante)

### 4.1 Comportamiento según Perfil de Compilación
* **En Perfil Cyberdeck RED (`CONFIG_CBDOS_PROFILE_RED`):**
  - La HeaderBar es **100% plana siempre**, incluso en el Dashboard.
  - Ancho: `LV_PCT(100)`, Alto: 38 px.
  - Alineación: `LV_ALIGN_TOP_MID, 0, 0` (sin margen superior).
  - Radio de esquinas: `0 px`.
  - Fondo: Opaco táctico `#0A0A0A` con línea divisoria inferior de 1 px en rojo sutil `#330005`.
  - Reloj, batería, icono Wi-Fi alineados verticalmente al centro.
  - Botón Volver compacto: Squircle de 32x28 px con `LV_SYMBOL_LEFT` en color rojo de acento (`#E63946`).
* **En Perfil Cyberdeck FULL / LITE:**
  - Dashboard utiliza Isla Flotante translúcida (`Y = 8`, 94% ancho, radio 14 px, Glassmorphism).
  - Vistas marcadas con `ViewMode::Solid` la transforman suavemente a barra plana acoplada a `Y = 0`.

---

## 🗂️ 5. Curaduría y Clasificación de Aplicaciones para Perfil RED

### 5.1 Nueva Bandera en `cbdos_build_profile.h`
Se introduce `CBDOS_FEATURE_MULTIMEDIA`:
* **`Cyberdeck FULL`:** `1` (Música, Radio, Galería, Grabadora, Lottie activos).
* **`Cyberdeck LITE`:** `0`.
* **`Cyberdeck RED`:** `0` (Excluidos de compilación y de `AppRegistry`).

### 5.2 Matriz de Aplicaciones en Perfil RED
| Identificador App | Título en UI | `ViewMode` | Origen / Función |
| :--- | :--- | :--- | :--- |
| **`wifi_shield`** | 🛡️ Wi-Fi Shield | `ViewMode::Solid` | Suite defensiva RX (Recon, IDS Sentinel, Sniffer PCAP). |
| **`ble_shield`** | 🛡️ BLE Shield | `ViewMode::Solid` | Air Recon BLE, Geiger Tracker, Spam IDS. |
| **`wifi_strike`** | ⚔️ Wi-Fi Strike | `ViewMode::Solid` | Suite ofensiva TX (Deauth, Beacon Spam, Evil Portal). |
| **`ble_strike`** | ⚔️ BLE Strike | `ViewMode::Solid` | Suite ofensiva TX (AppleJuice, FastPair, Spoofer). |
| **`terminal`** | Terminal | `ViewMode::Solid` | Consola interactiva serie / SSH / UART mochilas. |
| **`hid`** | USB HID / BadUSB | `ViewMode::Solid` | Inyección de payloads de teclado/mouse por USB. |
| **`recon`** | LAN Recon | `ViewMode::Solid` | Escaneo activo ARP / ICMP de hosts de red. |
| **`restprober`** | RestProber | `ViewMode::Solid` | Cliente HTTP / REST API auditoría de endpoints. |
| **`sdr`** | SDR Spectrum | `ViewMode::Solid` | Analizador de espectro RF con ESPARGOS. |
| **`flasher`** | Flasheador USB | `ViewMode::Solid` | Programador de chips serie / JTAG. |
| **`files`** | Archivos | `ViewMode::Solid` | Explorador y visor de archivos en MicroSD / LittleFS. |
| **`lua`** | Lua Runner | `ViewMode::Solid` | Entorno de ejecución de scripts tácticos `.luapp`. |
| **`config`** | Configuración | `ViewMode::Solid` | Ajustes de hardware, pantalla, red y energía. |

*Apps excluidas en RED (Purga táctica):* `gallery`, `music`, `radio`, `recorder`, `lottie`, `cartridge`.

---

## 📋 6. Fases de Implementación Propuestas (Para Ejecutar Tras Aprobación)

- [x] **Fase 1: Capa Base (`BaseView` y `UIManager`)**
  - Añadir `enum class ViewMode` en `BaseView.hpp`.
  - Implementar en `UIManager` el pausado de `AnimatedWallpaper::stop()` al pasar a modo `Solid`/`Immersive` y reanudación con `start()` al volver a `Translucent`.
  - Implementar ajuste dinámico de posición y altura de `m_contentContainer`.
- [x] **Fase 2: Rediseño de HeaderBar Plana**
  - Añadir soporte en `HeaderBar.cpp` para alternar entre `FloatingIsland` y `FlatFull`.
  - Configurar que en `CONFIG_CBDOS_PROFILE_RED` arranque y opere fija en modo `FlatFull` (100% ancho, `Y = 0`, radio 0).
- [x] **Fase 3: Curaduría de `AppRegistry` y Defaults de Wallpaper**
  - Condicionar apps multimedia con `#if CBDOS_FEATURE_MULTIMEDIA` en `AppRegistry.cpp`.
  - En `WallpaperManager::init()`, asegurar que `CONFIG_CBDOS_PROFILE_RED` asigne `"animated_matrix"` como fondo por defecto.
  - Registrar las 4 aplicaciones tácticas maestras con sus vistas base.
- [x] **Fase 4: Verificación de Compilación y Comportamiento**
  - Compilación limpia en ESP32-S3 (`pio run -d bsp/esp32_s3_jc3248 -e esp32s3_red`).
  - Verificación en hardware: confirmar que la HeaderBar es plana, el dashboard solo muestra herramientas tácticas y al abrir cualquier app el fondo Matrix se pausa ahorrando CPU.
