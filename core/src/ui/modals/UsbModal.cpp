#include "UsbModal.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/language.hpp"
#include "cbdos/usb_manager.hpp"

namespace cbdos {
namespace ui {

lv_obj_t* UsbModal::s_modalMask = nullptr;
static bool s_rebootPending = false;

static const cbdos::usb::UsbMode kModes[4] = {
    cbdos::usb::UsbMode::Hid,
    cbdos::usb::UsbMode::Fido,
    cbdos::usb::UsbMode::Cdc,
    cbdos::usb::UsbMode::Host
};

void UsbModal::close_btn_cb(lv_event_t* e) {
    (void)e;
    hide();
}

void UsbModal::hide() {
    if (s_modalMask && lv_obj_is_valid(s_modalMask)) {
        lv_obj_delete_async(s_modalMask);
        s_modalMask = nullptr;
    }
}

void UsbModal::option_cb(lv_event_t* e) {
    if (s_rebootPending) return;

    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    if (!btn) return;
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < 0 || idx >= 4) return;

    namespace usb = cbdos::usb;
    namespace lang = cbdos::lang;
    using lang::tr;
    using lang::StrId;

    usb::UsbMode current = usb::UsbManager::getInstance().getBootMode();
    usb::UsbMode next = kModes[idx];

    hide();
    if (next == current) return;

    s_rebootPending = true;
    usb::UsbManager::getInstance().requestMode(next);

    StrId toastMsg = StrId::STR_CFG_USB_TO_HID;
    if (next == usb::UsbMode::Fido) {
        toastMsg = StrId::STR_CFG_USB_TO_FIDO;
    } else if (next == usb::UsbMode::Cdc) {
        toastMsg = StrId::STR_CFG_USB_TO_CDC;
    } else if (next == usb::UsbMode::Host) {
        toastMsg = StrId::STR_CFG_USB_TO_HOST;
    }

    UIManager::showToast(tr(toastMsg));

    // Reboot diferido: el toast debe pintarse antes del restart.
    lv_timer_create(
        [](lv_timer_t* t) {
            lv_timer_delete(t);
            cbdos::usb::UsbManager::getInstance().reboot();
        },
        1500, nullptr);
}

void UsbModal::show(lv_obj_t* parent) {
    (void)parent;
    hide();

    namespace lang = cbdos::lang;
    namespace usb = cbdos::usb;
    using lang::tr;
    using lang::StrId;

    auto caps = cbdos::display::getCapabilities();

    s_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_modalMask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_modalMask, 0, 0);
    lv_obj_set_style_bg_color(s_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_modalMask, 0, 0);
    lv_obj_set_style_pad_all(s_modalMask, 10, 0);

    // Tarjeta Modal con Glassmorphism
    lv_obj_t* card = lv_obj_create(s_modalMask);
    lv_obj_set_width(card, (caps.width >= 480) ? 380 : 280);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_center(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Titulo
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, tr(StrId::STR_CFG_USB_MODE));
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    // Opciones: HID, FIDO2, HOST con marca en el modo activo actual
    const usb::UsbMode currentMode = usb::UsbManager::getInstance().getBootMode();
    const StrId labels[4] = {
        StrId::STR_CFG_USB_OPT_HID,
        StrId::STR_CFG_USB_OPT_FIDO,
        StrId::STR_CFG_USB_OPT_CDC,
        StrId::STR_CFG_USB_OPT_HOST
    };

    for (int i = 0; i < 4; i++) {
        const bool selected = (currentMode == kModes[i]);

        lv_obj_t* opt = lv_button_create(card);
        lv_obj_set_width(opt, lv_pct(100));
        lv_obj_set_height(opt, 46);
        DefaultTheme::applyButton(opt, 12);
        lv_obj_set_user_data(opt, (void*)(intptr_t)i);
        lv_obj_add_event_cb(opt, option_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_set_flex_flow(opt, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(opt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(opt, 16, 0);
        lv_obj_set_style_pad_right(opt, 16, 0);

        lv_obj_t* nameLbl = lv_label_create(opt);
        lv_label_set_text(nameLbl, tr(labels[i]));
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_16, 0);

        lv_obj_t* checkLbl = lv_label_create(opt);
        lv_label_set_text(checkLbl, selected ? LV_SYMBOL_OK : "");
        if (selected) {
            lv_obj_set_style_text_color(checkLbl, DefaultTheme::getPrimaryAccent(), 0);
            lv_obj_set_style_text_font(checkLbl, &lv_font_montserrat_16, 0);
        }
    }

    // Boton Cerrar
    lv_obj_t* btnClose = lv_button_create(card);
    lv_obj_set_size(btnClose, lv_pct(100), 38);
    DefaultTheme::applyButton(btnClose, 10);

    lv_obj_t* lblC = lv_label_create(btnClose);
    lv_label_set_text(lblC, tr(StrId::STR_ABOUT_CLOSE));
    lv_obj_center(lblC);

    lv_obj_add_event_cb(btnClose, close_btn_cb, LV_EVENT_CLICKED, nullptr);
}

} // namespace ui
} // namespace cbdos
