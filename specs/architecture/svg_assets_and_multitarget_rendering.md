# Especificación de Arquitectura: Assets Vectoriales SVG y Renderizado Multi-Target (CBDos v0.2.1)

## 📌 1. Visión General
CyBerDeck OS (CBDos) implementa un sistema desacoplado para la gestión de iconos y recursos gráficos mediante **archivos vectoriales `.svg` puros e independientes**.

Este enfoque garantiza que:
1. Los iconos no están embebidos como cadenas de texto hardcodeadas dentro del código fuente C++.
2. Los usuarios y desarrolladores pueden personalizar la apariencia del sistema operativo reemplazando o editando directamente los archivos SVG en `core/assets/icons/`.
3. El motor gráfico de LVGL 9.5 (`ThorVG`) procesa estos recursos vectoriales en cualquier microcontrolador soportado (ESP32-P4, ESP32-S3, etc.).

---

## 📂 2. Estructura de Directorios de Assets

Todos los archivos vectoriales del sistema residen de forma centralizada en la capa de `core/`:

```
cbdos/
└── core/
    ├── assets/
    │   └── icons/
    │       ├── app_recorder.svg
    │       ├── app_radio.svg
    │       ├── app_browser.svg
    │       ├── app_terminal.svg
    │       ├── app_cartridge.svg
    │       ├── app_lua.svg
    │       ├── app_editor.svg
    │       ├── app_utilities.svg
    │       ├── app_gallery.svg
    │       ├── app_files.svg
    │       ├── app_music.svg
    │       ├── app_flasher.svg
    │       └── app_config.svg
    └── src/
        └── ui/
            └── assets/
                ├── SystemIcons.hpp
                └── SystemIcons.cpp
```

> **Regla de Oro:** Está estrictamente prohibido duplicar o copiar archivos `.svg` dentro de los directorios de los BSPs (`bsp/esp32_p4_jc4880`, `bsp/esp32_s3_jc3248`).

---

## ⚙️ 3. Pipeline de Compilación Multi-Target

### A. ESP32-P4 (ESP-IDF / CMake)
En `core/CMakeLists.txt`, los archivos SVG se compilan directamente en secciones `.rodata` de la Flash utilizando `EMBED_TXTFILES`:

```cmake
target_add_binary_data(${COMPONENT_LIB} "assets/icons/app_recorder.svg" TEXT)
# Genera el símbolo: _binary_app_recorder_svg_start
```

### B. ESP32-S3 (PlatformIO / SCons)
En `bsp/esp32_s3_jc3248/platformio.ini`, se utiliza `board_build.embed_txtfiles` junto con alias de símbolos en el linker (`-Wl,--defsym`) para mapear de forma transparente los objetos compilados hacia los símbolos estándar `_binary_app_<nombre>_svg_start`.

---

## 🚀 4. Estrategia de Renderizado y Caché según Capacidades de Hardware

| Característica | ESP32-P4 (Guition JC4880P443C) | ESP32-S3 (JC3248W535) |
| :--- | :--- | :--- |
| **CPU / Frecuencia** | Dual RISC-V @ 400 MHz | Dual Xtensa @ 240 MHz |
| **Acelerador 2D** | Hardware PPA (Pixel Processing Accel) | Software puro por CPU |
| **Bus de Pantalla** | MIPI-DPI (480x800 @ 60 FPS) | QSPI / SPI (320x480 @ 30 FPS) |
| **Estrategia SVG** | Decodificación dinámica en tiempo real | Rasterizado inicial único a buffer PSRAM |
| **Efectos Táctiles** | Glow neón, sombras gaussianas y escala elástica | Cambio de contraste/brillo instantáneo |

---

## 🧩 5. API de Consumo de Iconos (`SystemIcons`)

La clase `cbdos::ui::SystemIcons` proporciona una interfaz agnóstica para toda la interfaz gráfica de CBDos:

```cpp
// Inicializa los decodificadores de LVGL
cbdos::ui::SystemIcons::init();

// Crea un objeto lv_image con el SVG correspondiente escalado al tamaño deseado
lv_obj_t* icon = cbdos::ui::SystemIcons::createIcon(parent, "recorder", 48);
```

### Contrato de Funcionamiento:
1. `getSvgData(appId)`: Retorna el puntero `const char*` a los datos binarios del archivo SVG en Flash.
2. `createIcon(parent, appId, size)`: Configura el descriptor de imagen `lv_image_dsc_t` con `LV_COLOR_FORMAT_ARGB8888` y aplica el factor de escala vectorial hacia el widget `lv_image`.
