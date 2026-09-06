# 🎨 Especificación Técnica - Fase 1: Sistema de UI Universal de CyBerDeck OS

## 📍 1. Ubicación del Proyecto y Rutas Absolutas

Todo el desarrollo del nuevo sistema operativo agnóstico reside en:
* **Directorio Raíz del Proyecto:** `/home/kaber420/Documentos/proyectos/cbdos/`

### 📂 Mapa de Rutas Físicas de la UI:

| Componente | Ruta Absoluta en el Sistema de Archivos | Propósito |
| :--- | :--- | :--- |
| **API Pública de UI** | `/home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/ui.hpp` | Interfaz de alto nivel para invocar pantallas |
| **Tokens y Temas** | `/home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/theme.hpp` | Definición de colores neón, fuentes y estilos |
| **Directorio de UI Core** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/` | Implementación 100% universal C++ |
| **Motor de Temas** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/ThemeEngine.cpp` | Gestor de paletas en tiempo real |
| **Gestor de Pantallas** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/UIManager.cpp` | Navegación de vistas (`push`, `pop`, `switch`) |
| **Barra de Estado** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Components/HeaderBar.cpp` | Reloj, estado WiFi, monitor de RAM |
| **Panel de Control** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Components/QuickSettingsPanel.cpp` | Brillo, volumen, WiFi, reset |
| **Lanzador de Apps** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Views/DashboardView.cpp` | Grid táctil de aplicaciones |
| **Clase Base de Vistas** | `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Views/BaseView.hpp` | Contrato común de ciclo de vida |

---

## 🎯 2. Objetivo General
Diseñar e implementar el **Sistema de Interfaz Gráfica Universal** en el componente `core/` de **CyBerDeck OS (CBDos v0.2.0)**, permitiendo que la interfaz gráfica sea rica, fluida, reactiva y **100% responsiva** adaptándose de forma automática a las dos pantallas objetivo:
* **ESP32-P4 (`/home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880`):** 480x800 @ 60 FPS (MIPI DPI).
* **ESP32-S3 (`/home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248`):** 320x480 @ 30 FPS (QSPI).

---

## 🏗️ 3. Arquitectura de Componentes de UI

```
/home/kaber420/Documentos/proyectos/cbdos/
│
├── core/
│   ├── include/cbdos/
│   │   ├── ui.hpp                           <- API pública de alto nivel para UI
│   │   └── theme.hpp                        <- Tokens de color, estilos y temas
│   └── src/ui/
│       ├── UIManager.cpp / .hpp             <- Orquestador del ciclo de vida de pantallas
│       ├── ThemeEngine.cpp / .hpp           <- Motor de temas dinámicos
│       ├── Components/
│       │   ├── HeaderBar.cpp / .hpp         <- Barra superior de estado
│       │   ├── QuickSettingsPanel.cpp / .hpp<- Panel deslizante de brillo/volumen
│       │   └── NavigationBar.cpp / .hpp     <- Barra inferior de navegación
│       └── Views/
│           ├── BaseView.hpp                 <- Clase base abstracta
│           ├── DashboardView.cpp / .hpp     <- Lanzador principal de Apps
│           └── SettingsView.cpp / .hpp      <- Menú de configuración
│
└── bsp/                                     <- Hardware (solo arranca y cede el control al Core)
    ├── esp32_p4_jc4880/main/main.cpp
    └── esp32_s3_jc3248/src/main.cpp
```

---

## 🎨 4. Especificación de los Componentes Clave

### 4.1. `ThemeEngine` (`core/src/ui/ThemeEngine.cpp`)
Permite cambiar paletas en tiempo de ejecución sin reiniciar el microcontrolador:
* **Cyberpunk Neon (Default):**
  * Fondo: `#0a0e17` (Azul medianoche profundo).
  * Acento Primario: `#00E5FF` (Cian cibernético).
  * Acento Secundario: `#FF2A6D` (Rosa neón vibrante).
  * Éxito / Estado: `#00FF88` (Verde esmeralda).
  * Tarjetas / Paneles: `#141C2B` con bordes sutiles en `#233247`.
* **Matrix Terminal:**
  * Fondo: `#000000`, Texto: `#00FF41`, Acentos: `#008F11`.
* **Solar Amber (Retro Industrial):**
  * Fondo: `#100C08`, Texto: `#FFB000`, Acentos: `#D48800`.

### 4.2. `HeaderBar` (`core/src/ui/Components/HeaderBar.cpp`)
* **Altura:** 32 px fija.
* **Elementos:**
  * Icono de CBDos y título del sistema.
  * Reloj en tiempo real (formato 24h `HH:MM:SS`).
  * Indicador de estado WiFi / IP (`cbdos::network::isConnected()`).
  * Monitor de RAM dinámica en vivo (`cbdos::system::getFreeHeap()`).
* **Interacción:** Tocar la barra despliega el `QuickSettingsPanel`.

### 4.3. `QuickSettingsPanel` (`core/src/ui/Components/QuickSettingsPanel.cpp`)
* **Comportamiento:** Panel flotante con animación deslizante desde arriba (*slide-down*).
* **Controles:**
  * Slider de brillo en tiempo real (`cbdos::display::setBrightness()`).
  * Slider de volumen de audio (`cbdos::audio::setVolume()`).
  * Interruptor WiFi (Activar/Desactivar).
  * Selector rápido de temas visuales.
  * Botón de reinicio seguro del sistema (`cbdos::system::restart()`).

### 4.4. `DashboardView` (`core/src/ui/Views/DashboardView.cpp`)
* **Diseño Responsivo:**
  * **En P4 (480x800):** Grid de 3 columnas x 4 filas con tarjetas HD (130x110 px).
  * **En S3 (320x480):** Grid compacto de 2 columnas x 3 filas (140x95 px).
* **Aplicaciones Iniciales en el Grid:**
  1. 📻 **Radio Online:** Streaming MP3/AAC por internet.
  2. 📜 **Lua Scripts:** Ejecutor de scripts interactivos.
  3. 🎮 **Cartuchos / Juegos:** Doom, emuladores y mini-juegos.
  4. 📁 **Archivos / SD:** Explorador de almacenamiento VFS.
  5. 🖼️ **Galería / Wallpapers:** Gestor de fondos de pantalla.
  6. ⚙️ **Configuración:** Ajustes del sistema y diagnóstico.

---

## 📐 5. Patrón de Responsividad para Pantallas

Cada vista consultará las capacidades del display para organizar sus elementos:

```cpp
void DashboardView::setupLayout() {
    auto caps = cbdos::display::getCapabilities();
    
    if (caps.width >= 480) {
        // Modo Alta Resolución (ESP32-P4)
        setupGridColumns(3);
        setCardDimensions(130, 110);
    } else {
        // Modo Compacto (ESP32-S3)
        setupGridColumns(2);
        setCardDimensions(140, 95);
    }
}
```

---

## 🚀 6. Plan de Ejecución Paso a Paso

1. **Paso 1: Tokens de Tema y `ThemeEngine`**
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/theme.hpp`
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/ThemeEngine.cpp`
2. **Paso 2: Clase Base `BaseView` y `UIManager`**
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Views/BaseView.hpp`
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/UIManager.cpp`
3. **Paso 3: Componentes Universales (`HeaderBar` + `QuickSettingsPanel`)**
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Components/HeaderBar.cpp`
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Components/QuickSettingsPanel.cpp`
4. **Paso 4: `DashboardView` Responsivo**
   * Crear `/home/kaber420/Documentos/proyectos/cbdos/core/src/ui/Views/DashboardView.cpp`
5. **Paso 5: Compilación y Validación Cruzada en Hardware**
   * Compilar y probar en ESP32-P4 (`bsp/esp32_p4_jc4880/`).
   * Compilar y probar en ESP32-S3 (`bsp/esp32_s3_jc3248/`).
