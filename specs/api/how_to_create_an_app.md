# 🛠️ Guía del Desarrollador: Cómo Crear una App para CBDos

Esta guía explica paso a paso cómo crear una nueva aplicación o vista interactiva para el sistema operativo **CBDos** utilizando C++ y **LVGL 9.5**.

---

## 🏗️ 1. Concepto y Arquitectura de una Vista (`BaseView`)

Todas las aplicaciones principales en CBDos heredan de la clase base `cbdos::ui::BaseView` ([BaseView.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/BaseView.hpp)). 

### Ciclo de vida de una App:
1. **`onCreate(lv_obj_t* parent)`:** Se llama una sola vez cuando la vista se instancia y se crea la interfaz gráfica de LVGL dentro de `m_container`.
2. **`onShow()`:** Se ejecuta cada vez que la vista pasa al primer plano (configurar título en el `HeaderBar`, iniciar timers).
3. **`onHide()`:** Se ejecuta cuando otra vista se sobrepone o el usuario minimiza la app (pausar timers o render).
4. **`onDestroy()`:** Limpieza de memoria, detención de timers y liberación de recursos de hardware.
5. **`onThemeChanged()`:** Notificación cuando el usuario cambia el tema del sistema operativo.

---

## 📝 2. Plantilla de Código para una Nueva App

### Archivo Cabecera: `core/src/ui/views/MiAppView.hpp`
```cpp
#pragma once
#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class MiAppView : public BaseView {
public:
    MiAppView();
    ~MiAppView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    lv_obj_t* m_labelCounter = nullptr;
    lv_obj_t* m_btnIncrement = nullptr;
    int m_counter = 0;

    static void btnClickedCb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
```

---

### Archivo Fuente: `core/src/ui/views/MiAppView.cpp`
```cpp
#include "MiAppView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/system.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

MiAppView::MiAppView()
    : BaseView("MiApp"), m_counter(0) {
}

MiAppView::~MiAppView() {
    onDestroy();
}

bool MiAppView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // 1. Crear el contenedor raíz de la vista (m_container)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 16, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 2. Crear una tarjeta interactiva con estilo DefaultTheme
    lv_obj_t* card = lv_obj_create(m_container);
    lv_obj_set_size(card, 260, 180);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 3. Etiqueta de texto
    m_labelCounter = lv_label_create(card);
    lv_label_set_text(m_labelCounter, "Contador: 0");
    lv_obj_set_style_text_color(m_labelCounter, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_labelCounter, &lv_font_montserrat_20, 0);

    // 4. Botón con estilo estándar
    m_btnIncrement = lv_button_create(card);
    DefaultTheme::applyButton(m_btnIncrement, 10);
    lv_obj_set_size(m_btnIncrement, 140, 44);
    lv_obj_set_style_margin_top(m_btnIncrement, 16, 0);

    lv_obj_t* lblBtn = lv_label_create(m_btnIncrement);
    lv_label_set_text(lblBtn, "Incrementar");
    lv_obj_center(lblBtn);

    // Registrar evento de click
    lv_obj_add_event_cb(m_btnIncrement, btnClickedCb, LV_EVENT_CLICKED, this);

    return true;
}

void MiAppView::onDestroy() {
    BaseView::onDestroy();
}

void MiAppView::onShow() {
    BaseView::onShow();
    // Configurar la barra superior (HeaderBar)
    HeaderBar& hb = UIManager::getInstance().getHeaderBar();
    hb.setTitle("Mi Nueva App");
    hb.showWifi(false); // Ocultar icono WiFi si no se utiliza
}

void MiAppView::onHide() {
    BaseView::onHide();
}

void MiAppView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    }
}

void MiAppView::btnClickedCb(lv_event_t* e) {
    MiAppView* self = static_cast<MiAppView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_counter++;
    char buf[32];
    snprintf(buf, sizeof(buf), "Contador: %d", self->m_counter);
    lv_label_set_text(self->m_labelCounter, buf);

    cbdos::system::log(cbdos::system::LogLevel::Info, "MiApp", "Contador incrementado a %d", self->m_counter);
}

} // namespace ui
} // namespace cbdos
```

---

## 🚀 3. Registro en el Build System y en el Dashboard

Para que tu aplicación aparezca en el sistema y se compile:

### Paso 1: Agregar a `core/CMakeLists.txt`
```cmake
    SRCS
        ...
        "src/ui/views/MiAppView.cpp"
```

### Paso 2: Registrar en `DashboardView.cpp`
En `core/src/ui/views/DashboardView.cpp`:
1. Incluir la cabecera: `#include "MiAppView.hpp"`
2. Agregar la tarjeta en el constructor:
   ```cpp
   {"miapp", "Mi App", LV_SYMBOL_STAR, 0x3B82F6},
   ```
3. Agregar el callback de navegación en `cardClickedEventCb`:
   ```cpp
   } else if (strcmp(appId, "miapp") == 0) {
       UIManager::getInstance().pushView(std::make_shared<MiAppView>());
   }
   ```

---

## ⚡ 4. Creación de Apps Portables en Lua++ (`.luapp` - Cero Compilación)

A partir de CBDos v0.2.1+, puedes crear aplicaciones interactivas completas **sin necesidad de compilar el firmware**, guardándolas simplemente como archivos `.luapp` en la carpeta `/sdcard/apps/` de tu tarjeta MicroSD.

### 4.1. Estructura de un archivo `.luapp`

Un archivo `.luapp` es un script en Lua con metadatos en su encabezado y funciones de ciclo de vida:

```lua
-- @name: Mi App Portable
-- @icon: LV_SYMBOL_FILE
-- @accent: #00F5D4
-- @author: Tu Nombre
-- @version: 1.0
-- @description: Aplicación de ejemplo sin compilación

local lblCount = nil
local counter = 0

function on_create(parent)
    local card = cbdos.ui.create_card(parent, -1, -1)
    
    local title = cbdos.ui.create_label(card, "HOLA DESDE LUA++")
    cbdos.ui.set_color(title, 0x00F5D4)
    cbdos.ui.set_font_size(title, 16)

    lblCount = cbdos.ui.create_label(card, "Contador: 0")
    cbdos.ui.set_font_size(lblCount, 14)

    local row = cbdos.ui.create_row(card)
    cbdos.ui.create_button(row, "+1 Sumar", function()
        counter = counter + 1
        cbdos.ui.set_text(lblCount, "Contador: " .. counter)
        cbdos.ui.show_toast("Incrementado a " .. counter)
    end)
end

function on_show()
    print("[Mi App] Visible")
end

function on_destroy()
    print("[Mi App] Cerrada y memoria liberada")
end
```

### 4.2. APIs Disponibles en Lua++ (`cbdos.*`)

- **UI LVGL 9.5 (`cbdos.ui`):**
  - `cbdos.ui.create_card(parent, w, h)`: Crea tarjeta elevada con `DefaultTheme`.
  - `cbdos.ui.create_sunken_card(parent, w, h)`: Crea caja hundida para displays LCD o terminales.
  - `cbdos.ui.create_label(parent, text)`: Crea etiqueta de texto.
  - `cbdos.ui.set_text(obj, text)` / `cbdos.ui.set_color(obj, 0xRRGGBB)` / `cbdos.ui.set_font_size(obj, 12|14|16|24)`.
  - `cbdos.ui.create_button(parent, text, onClickCallback)`: Botón interactivo.
  - `cbdos.ui.create_slider(parent, min, max, val, onChangeCallback)`: Slider táctil.
  - `cbdos.ui.create_switch(parent, state, onToggleCallback)`: Interruptor booleano.
  - `cbdos.ui.create_row(parent)` / `cbdos.ui.create_column(parent)`: Contenedores Flex layout.
  - `cbdos.ui.show_toast(mensaje)`: Muestra una notificación flotante en la interfaz.

- **Audio (`cbdos.audio`):**
  - `cbdos.audio.play_file(path)` / `cbdos.audio.stop()` / `cbdos.audio.set_volume(0-100)` / `cbdos.audio.beep(freq, ms)`.

- **UART y Comunicaciones (`cbdos.uart`):**
  - `cbdos.uart.init(baud, txPin, rxPin)` / `cbdos.uart.write(str)` / `cbdos.uart.read(maxLen)` / `cbdos.uart.available()`.

- **Sistema y Telemetría (`cbdos.system`):**
  - `cbdos.system.free_heap()` / `cbdos.system.free_psram()` / `cbdos.system.cpu_temp()` / `cbdos.system.get_time()` / `cbdos.system.sleep(ms)`.

---

## ✅ 5. Verificación Multi-Target
Compila en ambos entornos para garantizar compatibilidad total:
```bash
# Target ESP32-P4 (ESP-IDF)
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_p4_jc4880 && idf.py build

# Target ESP32-S3 (PlatformIO)
pio run -d bsp/esp32_s3_jc3248
```
