#include "HidView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/hid.hpp"
#include "cbdos/usb_manager.hpp"
#include "cbdos/system.hpp"
#include <cstdlib>
#include <cstring>

namespace cbdos {
namespace ui {

// La vista solo pide modo al UsbManager (core). Jamás toca BSP/IDF/Arduino.
// Si el arranque fue en modo Host, el PHY lo tiene el Host y el HID no puede
// instalarse: se ofrece reboot a modo HID en vez de pelear el hardware.
static bool hidModeReady() {
    if (cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Hid) {
        UIManager::showToast("Modo HOST activo: cambia a HID en Config");
        return false;
    }
    return true;
}

// ── StreamDeck: labels + keycodes directos (F13-F24 para OBS, sin soft en PC) ──
struct DeckItem {
    const char* label;
    uint8_t key;
    uint8_t mod;
    uint32_t color;
};
static const DeckItem kDeck[] = {
    {"CAM 1", cbdos::hid::keycode::KEY_F13, cbdos::hid::MOD_NONE, 0x2196F3},
    {"PANT", cbdos::hid::keycode::KEY_F14, cbdos::hid::MOD_NONE, 0x3F51B5},
    {"MUTE", cbdos::hid::keycode::KEY_F15, cbdos::hid::MOD_NONE, 0xF44336},
    {"CLIP", cbdos::hid::keycode::KEY_F16, cbdos::hid::MOD_NONE, 0x4CAF50},
    {"BRB", cbdos::hid::keycode::KEY_F17, cbdos::hid::MOD_NONE, 0xFF9800},
    {"VOL", cbdos::hid::keycode::KEY_F18, cbdos::hid::MOD_NONE, 0x9C27B0},
    {"ESC", cbdos::hid::keycode::KEY_ESCAPE, cbdos::hid::MOD_NONE, 0x607D8B},
    {"GUI+R", cbdos::hid::keycode::KEY_R, cbdos::hid::MOD_LGUI, 0x00BCD4},
    {"TERM", cbdos::hid::keycode::KEY_T, (uint8_t)(cbdos::hid::MOD_LCTRL | cbdos::hid::MOD_LALT), 0x8BC34A},
};
static constexpr size_t kDeckCount = sizeof(kDeck) / sizeof(kDeck[0]);

// Quick keys del tab Teclado: 0=ENTER 1=ESC 2=TAB 3=GUI+R 4=CTRL+ALT+T 5=ALT+F4
static void fireQuickKey(int idx) {
    using namespace cbdos::hid;
    enable();
    switch (idx) {
        case 0: sendCombo({keycode::KEY_ENTER}); break;
        case 1: sendCombo({keycode::KEY_ESCAPE}); break;
        case 2: sendCombo({keycode::KEY_TAB}); break;
        case 3: sendCombo({keycode::KEY_R}, MOD_LGUI); break;
        case 4: sendCombo({keycode::KEY_T}, (uint8_t)(MOD_LCTRL | MOD_LALT)); break;
        case 5: sendCombo({keycode::KEY_F4}, MOD_LALT); break;
        default: break;
    }
}

HidView::HidView() : BaseView("HID Control") {}

bool HidView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;
    UIManager::getInstance().getHeaderBar().setTitle("HID Control");
    UIManager::getInstance().getHeaderBar().showWifi(false);

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 6, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_container, 6, 0);
    DefaultTheme::disableScroll(m_container);

    buildStatusBar(m_container);
    buildTabs(m_container);
    refreshStatus();

    m_timer = lv_timer_create(statusTimerCb, 500, this);
    return true;
}

void HidView::onDestroy() {
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
    m_tabview = nullptr;
    m_statusLabel = nullptr;
    m_enableSwitch = nullptr;
    m_textarea = nullptr;
    m_keyboard = nullptr;
    m_pad = nullptr;
    BaseView::onDestroy();
}

void HidView::onShow() { BaseView::onShow(); }
void HidView::onHide() { BaseView::onHide(); }

void HidView::onThemeChanged(cbdos::theme::ThemeType, const cbdos::theme::ThemePalette&) {}

// ── Barra de estado: switch enable + label conectado/LEDs ──
void HidView::buildStatusBar(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(row, 10);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(row);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, "HID USB");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    m_statusLabel = lv_label_create(row);
    lv_label_set_text(m_statusLabel, "...");
    lv_obj_set_style_text_font(m_statusLabel, &lv_font_montserrat_12, 0);

    m_enableSwitch = lv_switch_create(row);
    if (cbdos::hid::isEnabled()) lv_obj_add_state(m_enableSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(m_enableSwitch, enableSwitchCb, LV_EVENT_VALUE_CHANGED, this);
}

void HidView::buildTabs(lv_obj_t* parent) {
    m_tabview = lv_tabview_create(parent);
    lv_tabview_set_tab_bar_position(m_tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(m_tabview, 38);
    lv_obj_set_size(m_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_tabview, 1);
    lv_obj_set_style_bg_opa(m_tabview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_tabview, 0, 0);

    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);

    lv_obj_t* tKb = lv_tabview_add_tab(m_tabview, "Teclado");
    lv_obj_t* tPad = lv_tabview_add_tab(m_tabview, "Touchpad");
    lv_obj_t* tDeck = lv_tabview_add_tab(m_tabview, "Deck");

    lv_obj_t* content = lv_tabview_get_content(m_tabview);
    DefaultTheme::disableScroll(content);

    buildKeyboardTab(tKb);
    buildTouchpadTab(tPad);
    buildStreamDeckTab(tDeck);
}

// ── Tab 1: teclado LIVE tecla-por-tecla (sin botón Enviar) ──
void HidView::buildKeyboardTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_row(tab, 6, 0);

    // Eco local de lo ya enviado por HID (solo lectura visual)
    m_textarea = lv_textarea_create(tab);
    lv_obj_set_size(m_textarea, LV_PCT(100), 72);
    lv_textarea_set_placeholder_text(m_textarea, "Teclado LIVE: pulsa y sale en el PC...");
    lv_textarea_set_one_line(m_textarea, false);

    lv_obj_t* btnRow = lv_obj_create(tab);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(btnRow, 6, 0);
    lv_obj_set_style_pad_row(btnRow, 6, 0);
    DefaultTheme::disableScroll(btnRow);

    const char* quickLabels[] = {"Limpiar", "ENTER", "ESC", "TAB", "GUI+R", "CTRL+ALT+T", "ALT+F4"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t* b = lv_button_create(btnRow);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, quickLabels[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_center(l);
        lv_obj_set_user_data(b, (void*)(uintptr_t)i);
        if (i == 0)
            lv_obj_add_event_cb(b, clearTextCb, LV_EVENT_CLICKED, this);
        else
            lv_obj_add_event_cb(b, quickKeyCb, LV_EVENT_CLICKED, this);
    }

    m_keyboard = lv_keyboard_create(tab);
    lv_obj_set_size(m_keyboard, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_keyboard, 1);
    lv_keyboard_set_textarea(m_keyboard, m_textarea);
    // Interceptar cada pulsación y reenviarla por HID en vivo
    lv_obj_add_event_cb(m_keyboard, kbHidCb, LV_EVENT_VALUE_CHANGED, this);
}

// ── Tab 2: pad táctil relativo + botones + wheel ──
void HidView::buildTouchpadTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_row(tab, 6, 0);

    m_pad = lv_obj_create(tab);
    lv_obj_set_size(m_pad, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_pad, 1);
    DefaultTheme::applySunkenCard(m_pad, 12);
    lv_obj_add_flag(m_pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_pad, padEventCb, LV_EVENT_ALL, this);

    lv_obj_t* hint = lv_label_create(m_pad);
    lv_label_set_text(hint, "Arrastra para mover  -  tap = click izq.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_center(hint);
    lv_obj_remove_flag(hint, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* btnRow = lv_obj_create(tab);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(btnRow);

    const char* btns[] = {"Left", "Right", "Middle", "Wheel+", "Wheel-"};
    for (int i = 0; i < 5; i++) {
        lv_obj_t* b = lv_button_create(btnRow);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, btns[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_center(l);
        lv_obj_set_user_data(b, (void*)(uintptr_t)i);
        if (i <= 2)
            lv_obj_add_event_cb(b, mouseBtnCb, LV_EVENT_CLICKED, this);
        else
            lv_obj_add_event_cb(b, wheelCb, LV_EVENT_CLICKED, this);
    }
}

// ── Tab 3: grid 3x3 StreamDeck ──
void HidView::buildStreamDeckTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(tab, 10, 0);
    lv_obj_set_style_pad_column(tab, 10, 0);

    for (size_t i = 0; i < kDeckCount; i++) {
        lv_obj_t* b = lv_button_create(tab);
        lv_obj_set_size(b, 100, 72);
        lv_obj_set_style_bg_color(b, lv_color_hex(kDeck[i].color), 0);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, kDeck[i].label);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
        lv_obj_set_user_data(b, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(b, deckBtnCb, LV_EVENT_CLICKED, this);
    }
}

void HidView::refreshStatus() {
    if (!m_statusLabel || !lv_obj_is_valid(m_statusLabel)) return;
    if (cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Hid) {
        lv_label_set_text(m_statusLabel, "Modo HOST: cambia en Config");
        if (m_enableSwitch && lv_obj_is_valid(m_enableSwitch))
            lv_obj_remove_state(m_enableSwitch, LV_STATE_CHECKED);
        return;
    }
    bool en = cbdos::hid::isEnabled();
    bool conn = cbdos::hid::isConnected();
    bool ready = cbdos::hid::isReady();
    char buf[128];
    if (!en)
        snprintf(buf, sizeof(buf), "OFF - activa HID");
    else if (!conn)
        snprintf(buf, sizeof(buf), "ON - conecta USB-OTG al PC");
    else if (!ready)
        snprintf(buf, sizeof(buf), "PC! enumerando...");
    else
        snprintf(buf, sizeof(buf), "PC! listo L:%02X", cbdos::hid::getLedState());
    lv_label_set_text(m_statusLabel, buf);
    if (m_enableSwitch && lv_obj_is_valid(m_enableSwitch)) {
        if (en) lv_obj_add_state(m_enableSwitch, LV_STATE_CHECKED);
        else lv_obj_remove_state(m_enableSwitch, LV_STATE_CHECKED);
    }
}

// ── Callbacks ──
static void hidRebootTimerCb(lv_timer_t* t) {
    lv_timer_delete(t);
    cbdos::usb::UsbManager::getInstance().reboot();
}

void HidView::enableSwitchCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (on &&
        cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Hid) {
        // Pedir modo HID al manager y reiniciar para aplicarlo limpio al arranque.
        // Diferido con timer para que el toast alcance a pintarse antes del reboot.
        cbdos::usb::UsbManager::getInstance().requestMode(cbdos::usb::UsbMode::Hid);
        UIManager::showToast("Modo HOST->HID: reiniciando...");
        lv_timer_create(hidRebootTimerCb, 1500, nullptr);
        if (self) self->refreshStatus();
        return;
    }
    bool ok = on ? cbdos::hid::enable() : cbdos::hid::disable();
    if (on && !ok) {
        UIManager::showToast("HID no activo");
    } else if (on && !cbdos::hid::isConnected()) {
        UIManager::showToast("HID ON: usa puerto USB-OTG al PC");
    }
    if (self) self->refreshStatus();
}

// Tecla LIVE: LVGL ya escribió en el textarea, reenviamos esa misma tecla por HID
void HidView::kbHidCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    lv_obj_t* kb = (lv_obj_t*)lv_event_get_target(e);
    if (!kb) return;
    uint32_t btnId = lv_keyboard_get_selected_button(kb);
    const char* txt = lv_keyboard_get_button_text(kb, btnId);
    if (!txt || !txt[0]) return;

    // Filtrar controles internos del keyboard LVGL (no son teclas HID)
    if (strcmp(txt, LV_SYMBOL_KEYBOARD) == 0 || strcmp(txt, LV_SYMBOL_SETTINGS) == 0) return;
    // ABC / abc / 123 / #+= cambian el mapa, no se envían
    if (strcmp(txt, "ABC") == 0 || strcmp(txt, "abc") == 0 ||
        strcmp(txt, "123") == 0 || strcmp(txt, "#+=") == 0) return;

    if (!hidModeReady()) return;
    if (!cbdos::hid::enable()) {
        UIManager::showToast("HID no activo");
        return;
    }

    using namespace cbdos::hid;
    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        sendCombo({keycode::KEY_BACKSPACE});
    } else if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0 || strcmp(txt, LV_SYMBOL_OK) == 0) {
        sendCombo({keycode::KEY_ENTER});
    } else if (strcmp(txt, LV_SYMBOL_LEFT) == 0) {
        sendCombo({keycode::KEY_LEFT});
    } else if (strcmp(txt, LV_SYMBOL_RIGHT) == 0) {
        sendCombo({keycode::KEY_RIGHT});
    } else if (strcmp(txt, LV_SYMBOL_UP) == 0) {
        sendCombo({keycode::KEY_UP});
    } else if (strcmp(txt, LV_SYMBOL_DOWN) == 0) {
        sendCombo({keycode::KEY_DOWN});
    } else if (strcmp(txt, " ") == 0 || strcmp(txt, "Space") == 0) {
        sendString(" ", 5);
    } else if (strlen(txt) == 1 || (txt[0] != '\0' && txt[1] == '\0')) {
        // Caracter normal: LVGL ya lo puso en el textarea, lo mandamos tal cual
        char s[2] = {txt[0], '\0'};
        sendString(std::string(s), 5);
    } else if (strcmp(txt, "Tab") == 0) {
        sendCombo({keycode::KEY_TAB});
    } else {
        // Tecla larga ("Enter", "Esc"...): intentar por nombre
        uint8_t kc = nameToKeycode(txt);
        if (kc != keycode::KEY_NONE) sendCombo({kc});
    }
    if (self) self->refreshStatus();
}

void HidView::clearTextCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    if (!self || !self->m_textarea) return;
    lv_textarea_set_text(self->m_textarea, "");
}

void HidView::quickKeyCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    lv_obj_t* b = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(uintptr_t)lv_obj_get_user_data(b);
    // user_data: 0=Limpiar (no llega aquí), 1..6 mapean a quick 0..5
    if (!hidModeReady()) return;
    if (!cbdos::hid::enable()) {
        UIManager::showToast("HID no activo");
        return;
    }
    if (!cbdos::hid::isConnected()) UIManager::showToast("Sin PC: revisa cable USB-OTG");
    fireQuickKey(idx - 1);
    if (self) self->refreshStatus();
}

void HidView::mouseBtnCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    lv_obj_t* b = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(uintptr_t)lv_obj_get_user_data(b);
    if (!hidModeReady()) return;
    if (!cbdos::hid::enable()) {
        UIManager::showToast("HID no activo");
        return;
    }
    if (!cbdos::hid::isConnected()) UIManager::showToast("Sin PC: revisa cable USB-OTG");
    if (idx == 0) cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_LEFT);
    else if (idx == 1) cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_RIGHT);
    else cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_MIDDLE);
    if (self) self->refreshStatus();
}

void HidView::wheelCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    lv_obj_t* b = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(uintptr_t)lv_obj_get_user_data(b);
    if (!hidModeReady()) return;
    if (!cbdos::hid::enable()) {
        UIManager::showToast("HID no activo");
        return;
    }
    cbdos::hid::mouseMove(0, 0, (idx == 3) ? 1 : -1);
    if (self) self->refreshStatus();
}

void HidView::padEventCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    if (!self) return;
    // En modo Host el PHY no es HID: ignorar sin spam (la barra ya avisa).
    if (cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Hid) return;
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        self->m_lastPadX = p.x;
        self->m_lastPadY = p.y;
        self->m_accumX = 0;
        self->m_accumY = 0;
        cbdos::hid::enable();
    } else if (code == LV_EVENT_PRESSING) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        if (self->m_lastPadX < 0) {
            self->m_lastPadX = p.x;
            self->m_lastPadY = p.y;
            return;
        }
        int dx = (int)p.x - (int)self->m_lastPadX;
        int dy = (int)p.y - (int)self->m_lastPadY;
        self->m_lastPadX = p.x;
        self->m_lastPadY = p.y;
        self->m_accumX += dx * 2;  // sensibilidad x2
        self->m_accumY += dy * 2;
        // Enviar en pasos int8
        while (self->m_accumX >= 10 || self->m_accumX <= -10 ||
               self->m_accumY >= 10 || self->m_accumY <= -10) {
            int8_t sx = (int8_t)(self->m_accumX > 127 ? 127 : (self->m_accumX < -127 ? -127 : self->m_accumX));
            int8_t sy = (int8_t)(self->m_accumY > 127 ? 127 : (self->m_accumY < -127 ? -127 : self->m_accumY));
            // limitar paso para no saturar
            if (sx > 20) sx = 20;
            if (sx < -20) sx = -20;
            if (sy > 20) sy = 20;
            if (sy < -20) sy = -20;
            cbdos::hid::mouseMove(sx, sy, 0);
            self->m_accumX -= sx;
            self->m_accumY -= sy;
            if (self->m_accumX > -10 && self->m_accumX < 10 &&
                self->m_accumY > -10 && self->m_accumY < 10) break;
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        self->m_lastPadX = -1;
        self->m_lastPadY = -1;
        self->m_accumX = 0;
        self->m_accumY = 0;
    } else if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) {
        if (!cbdos::hid::enable()) return;
        cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_LEFT);
    }
}

void HidView::deckBtnCb(lv_event_t* e) {
    auto* self = static_cast<HidView*>(lv_event_get_user_data(e));
    lv_obj_t* b = (lv_obj_t*)lv_event_get_target(e);
    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(b);
    if (idx >= kDeckCount) return;
    if (!hidModeReady()) return;
    if (!cbdos::hid::enable()) {
        UIManager::showToast("HID no activo");
        return;
    }
    if (!cbdos::hid::isConnected()) UIManager::showToast("Sin PC: revisa cable USB-OTG");
    cbdos::hid::sendCombo({kDeck[idx].key}, kDeck[idx].mod);
    UIManager::showToast(kDeck[idx].label);
    if (self) self->refreshStatus();
}

void HidView::statusTimerCb(lv_timer_t* t) {
    auto* self = static_cast<HidView*>(lv_timer_get_user_data(t));
    if (self) self->refreshStatus();
}

}  // namespace ui
}  // namespace cbdos
