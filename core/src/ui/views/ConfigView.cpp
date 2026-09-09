#include "ConfigView.hpp"
#include "PowerConfigView.hpp"
#include "NetworkManagerView.hpp"
#include "WallpaperConfigView.hpp"
#include "StorageConfigView.hpp"
#include "TimeConfigView.hpp"
#include "../modals/DiagnosticsModal.hpp"
#include "../modals/AboutModal.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/config_manager.hpp"
#include "cbdos/usb_manager.hpp"
#include "cbdos/system.hpp"
#include <cstdio>
#include <string>

namespace cbdos {
namespace ui {

lv_timer_t* ConfigView::s_nvsTimer = nullptr;
uint32_t ConfigView::s_nvsStartTime = 0;
lv_obj_t* ConfigView::s_nvsBar = nullptr;
lv_obj_t* ConfigView::s_nvsSubLabel = nullptr;

ConfigView::ConfigView()
    : BaseView("Configuracion") {
}

void ConfigView::onDestroy() {
    cancel_nvs_reset();
    BaseView::onDestroy();
}

void ConfigView::cancel_nvs_reset() {
    if (s_nvsTimer) {
        lv_timer_delete(s_nvsTimer);
        s_nvsTimer = nullptr;
    }
    s_nvsStartTime = 0;
    if (s_nvsBar && lv_obj_is_valid(s_nvsBar)) {
        lv_bar_set_value(s_nvsBar, 0, LV_ANIM_OFF);
    }
    if (s_nvsSubLabel && lv_obj_is_valid(s_nvsSubLabel)) {
        lv_label_set_text(s_nvsSubLabel, "Manten presionado 3s para borrar");
    }
}

void ConfigView::nvs_timer_cb(lv_timer_t* timer) {
    (void)timer;
    if (!s_nvsStartTime) return;
    
    uint32_t elapsed = lv_tick_elaps(s_nvsStartTime);
    if (elapsed < 3000) {
        int32_t progress = (elapsed * 100) / 3000;
        if (s_nvsBar && lv_obj_is_valid(s_nvsBar)) {
            lv_bar_set_value(s_nvsBar, progress, LV_ANIM_OFF);
        }
        if (s_nvsSubLabel && lv_obj_is_valid(s_nvsSubLabel)) {
            float rem = (3000.0f - (float)elapsed) / 1000.0f;
            if (rem < 0.0f) rem = 0.0f;
            char buf[48];
            snprintf(buf, sizeof(buf), "Soltar para cancelar (%.1fs)", rem);
            lv_label_set_text(s_nvsSubLabel, buf);
        }
    } else {
        cancel_nvs_reset();
        ConfigManager::getInstance().clearAllNvs();
        UIManager::showToast("NVS borrado completamente");
    }
}

void ConfigView::nvs_btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        s_nvsStartTime = lv_tick_get();
        if (s_nvsTimer) {
            lv_timer_delete(s_nvsTimer);
        }
        s_nvsTimer = lv_timer_create(nvs_timer_cb, 50, NULL);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        cancel_nvs_reset();
    } else if (code == LV_EVENT_DELETE) {
        cancel_nvs_reset();
        s_nvsBar = nullptr;
        s_nvsSubLabel = nullptr;
    }
}

void ConfigView::btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        int id = (int)(intptr_t)lv_obj_get_user_data(btn);
        if (id == 1) {
            UIManager::getInstance().pushView(std::make_shared<NetworkManagerView>());
        } else if (id == 2) {
            UIManager::getInstance().pushView(std::make_shared<StorageConfigView>());
        } else if (id == 3) {
            UIManager::getInstance().pushView(std::make_shared<TimeConfigView>());
        } else if (id == 4) {
            UIManager::getInstance().pushView(std::make_shared<PowerConfigView>());
        } else if (id == 5) {
            UIManager::getInstance().pushView(std::make_shared<WallpaperConfigView>());
        } else if (id == 6) {
            DiagnosticsModal::show();
        } else if (id == 8) {
            AboutModal::show();
        } else if (id == 9) {
            // Selector de modo USB de sistema: persiste y reinicia para aplicar
            // limpio al arranque (las apps jamás tocan el BSP directo).
            // Reboot diferido con timer: el toast debe pintarse antes del restart.
            auto cur = cbdos::usb::UsbManager::getInstance().getBootMode();
            auto next = (cur == cbdos::usb::UsbMode::Hid)
                            ? cbdos::usb::UsbMode::Host
                            : cbdos::usb::UsbMode::Hid;
            cbdos::usb::UsbManager::getInstance().requestMode(next);
            UIManager::showToast(next == cbdos::usb::UsbMode::Hid
                                     ? "Modo HOST->HID: reiniciando..."
                                     : "Modo HID->HOST: reiniciando...");
            lv_timer_create(
                [](lv_timer_t* t) {
                    lv_timer_delete(t);
                    cbdos::usb::UsbManager::getInstance().reboot();
                },
                1500, nullptr);
        }
    }
}

bool ConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor principal scrollable
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 24, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    struct OptionItem {
        const char* title;
        const char* subtitle;
        int id;
    };

    OptionItem options[] = {
        {"Sistema y Energia", "Apagado, reinicio, standby y auto-suspension", 4},
        {"Redes e Interfaces", "Wi-Fi, ESP-NOW, LoRa y Ranuras de Hardware", 1},
        {"Fecha y Hora", "Zona horaria, horario de verano y NTP", 3},
        {"Almacenamiento", "Gestion de MicroSD, Flash y USB", 2},
        {"Fondo de Pantalla", "Elegir wallpaper de SD o Flash", 5},
        {"Sistema", "Diagnostico de hardware y memoria", 6},
        {"Acerca de CBDos", "v0.2.3-dev, Licencia GPLv3 y Repo", 8},
        {"Resetear NVS", "Manten presionado 3s para borrar", 7}
    };

    // Fila de modo USB de sistema (se dibuja aparte por subtítulo dinámico)
    const bool usbHid =
        cbdos::usb::UsbManager::getInstance().getBootMode() == cbdos::usb::UsbMode::Hid;
    const std::string usbSub = usbHid ? "Actual: HID teclado/raton (tocar=cambiar a HOST)"
                                      : "Actual: HOST modem/flasher (tocar=cambiar a HID)";


    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
        lv_obj_t* card = lv_button_create(m_container);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 60);
        DefaultTheme::applyButton(card, 14);
        lv_obj_set_user_data(card, (void*)(intptr_t)options[i].id);


        if (options[i].id == 7) {
            lv_obj_add_event_cb(card, nvs_btn_event_cb, LV_EVENT_ALL, NULL);
        } else {
            lv_obj_add_event_cb(card, btn_event_cb, LV_EVENT_CLICKED, NULL);
        }

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(card, 16, 0);
        lv_obj_set_style_pad_right(card, 16, 0);
        lv_obj_set_style_pad_top(card, 8, 0);
        lv_obj_set_style_pad_bottom(card, 8, 0);

        // Texto (Título + Subtítulo)
        lv_obj_t* textCont = lv_obj_create(card);
        lv_obj_set_flex_grow(textCont, 1);
        lv_obj_set_style_bg_opa(textCont, 0, 0);
        lv_obj_set_style_border_width(textCont, 0, 0);
        lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(textCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(textCont, 0, 0);
        lv_obj_set_style_pad_row(textCont, 2, 0);
        lv_obj_remove_flag(textCont, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* titleLbl = lv_label_create(textCont);
        lv_label_set_text(titleLbl, options[i].title);
        lv_obj_set_style_text_color(titleLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);

        lv_obj_t* subLbl = lv_label_create(textCont);
        lv_label_set_text(subLbl, options[i].subtitle);
        lv_obj_set_style_text_color(subLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);

        // Icono a la derecha
        lv_obj_t* iconRight = lv_label_create(card);
        if (options[i].id == 7) {
            lv_label_set_text(iconRight, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_color(iconRight, lv_color_hex(0xEF4444), 0);
        } else {
            lv_label_set_text(iconRight, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(iconRight, DefaultTheme::getMutedTextColor(), 0);
        }
        lv_obj_set_style_text_font(iconRight, &lv_font_montserrat_14, 0);

        // Barra para Reset NVS
        if (options[i].id == 7) {
            s_nvsSubLabel = subLbl;

            lv_obj_t* bar = lv_bar_create(card);
            s_nvsBar = bar;
            lv_obj_add_flag(bar, LV_OBJ_FLAG_FLOATING);
            lv_bar_set_range(bar, 0, 100);
            lv_bar_set_value(bar, 0, LV_ANIM_OFF);
            lv_obj_set_size(bar, lv_pct(100), 4);
            lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 6);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0x2A2E3D), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0xEF4444), LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
            lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
            lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // Fila de modo USB de sistema (id 9): mismo estilo de tarjeta
    {
        lv_obj_t* card = lv_button_create(m_container);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 60);
        DefaultTheme::applyButton(card, 14);
        lv_obj_set_user_data(card, (void*)(intptr_t)9);
        lv_obj_add_event_cb(card, btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(card, 16, 0);
        lv_obj_set_style_pad_right(card, 16, 0);
        lv_obj_set_style_pad_top(card, 8, 0);
        lv_obj_set_style_pad_bottom(card, 8, 0);

        lv_obj_t* textCont = lv_obj_create(card);
        lv_obj_set_flex_grow(textCont, 1);
        lv_obj_set_style_bg_opa(textCont, 0, 0);
        lv_obj_set_style_border_width(textCont, 0, 0);
        lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(textCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(textCont, 0, 0);
        lv_obj_set_style_pad_row(textCont, 2, 0);
        lv_obj_remove_flag(textCont, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* titleLbl = lv_label_create(textCont);
        lv_label_set_text(titleLbl, "Modo USB");
        lv_obj_set_style_text_color(titleLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);

        lv_obj_t* subLbl = lv_label_create(textCont);
        lv_label_set_text(subLbl, usbSub.c_str());
        lv_obj_set_style_text_color(subLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);

        lv_obj_t* iconRight = lv_label_create(card);
        lv_label_set_text(iconRight, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(iconRight, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(iconRight, &lv_font_montserrat_14, 0);
    }

    return true;
}

} // namespace ui
} // namespace cbdos
