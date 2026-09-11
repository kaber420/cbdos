#pragma once
#include <lvgl.h>

namespace cbdos {
namespace ui {

// Modal de seleccion de idioma ES/EN (Fase 1 i18n).
// Muestra las opciones con marca en el idioma actual; al elegir otro
// persiste en NVS y reinicia para redibujar la UI.
class LanguageModal {
public:
    static void show(lv_obj_t* parent = nullptr);
    static void hide();

private:
    static void close_btn_cb(lv_event_t* e);
    static void option_cb(lv_event_t* e);

    static lv_obj_t* s_modalMask;
};

} // namespace ui
} // namespace cbdos
