#pragma once
#include <lvgl.h>

namespace cbdos {
namespace ui {

// Modal de seleccion de modo USB de sistema (Hid, Fido, Host).
// Muestra las opciones con marca en el modo activo; al elegir otro
// persiste en NVS y programa el reinicio del sistema para aplicar el stack limpio.
class UsbModal {
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
