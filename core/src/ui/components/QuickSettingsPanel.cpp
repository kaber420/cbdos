#include "QuickSettingsPanel.hpp"
#include "../themes/DefaultTheme.h"
#include "../UIManager.hpp"
#include "../views/RadioConfigView.hpp"
#include "cbdos/display.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/system.hpp"
#include "cbdos/radio.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/config_manager.hpp"
#include "cbdos/security.hpp"
#include "cbdos/language.hpp"
#include "../modals/LockPinModal.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* QuickSettingsPanel::panelObj = nullptr;

void QuickSettingsPanel::hide() {
    if (panelObj && lv_obj_is_valid(panelObj)) {
        lv_obj_delete_async(panelObj);
        panelObj = nullptr;
    }
}

void QuickSettingsPanel::mask_click_cb(lv_event_t* e) {
    hide();
}

void QuickSettingsPanel::volume_slider_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    cbdos::audio::setVolume((uint8_t)val);
    if (code == LV_EVENT_RELEASED) {
        ConfigManager::getInstance().setVolume((uint8_t)val);
    }
}

void QuickSettingsPanel::brightness_slider_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    cbdos::display::setBrightness((uint8_t)val);
    if (code == LV_EVENT_RELEASED) {
        ConfigManager::getInstance().setBrightness((uint8_t)val);
    }
}

void QuickSettingsPanel::wifi_switch_cb(lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    if (sw) {
        bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        ConfigManager::getInstance().setWifiAutoConnect(checked);
        if (checked) {
            WiFiConfig wifiCfg;
            if (ConfigManager::getInstance().loadWiFi(wifiCfg) && wifiCfg.ssid.length() > 0) {
                if (wifiCfg.useStaticIp) {
                    cbdos::network::connectWifiStatic(
                        wifiCfg.ssid.c_str(),
                        wifiCfg.password.c_str(),
                        wifiCfg.staticIp.c_str(),
                        wifiCfg.gateway.c_str(),
                        wifiCfg.subnet.length() > 0 ? wifiCfg.subnet.c_str() : "255.255.255.0",
                        wifiCfg.dns1.c_str()
                    );
                } else {
                    cbdos::network::connectWifi(wifiCfg.ssid.c_str(), wifiCfg.password.c_str());
                }
            } else {
                cbdos::network::init();
            }
        } else {
            cbdos::network::disconnectWifi();
        }
    }
}

void QuickSettingsPanel::radio_mode_dropdown_cb(lv_event_t* e) {
    lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
    if (!dd) return;

    uint32_t sel = lv_dropdown_get_selected(dd);
    cbdos::radio::RadioMode mode = cbdos::radio::RadioMode::EspNow;
    switch (sel) {
        case 0: mode = cbdos::radio::RadioMode::WifiSta; break;
        case 1: mode = cbdos::radio::RadioMode::EspNow; break;
        case 2: mode = cbdos::radio::RadioMode::EspNowLR; break;
        case 3: mode = cbdos::radio::RadioMode::Hybrid; break;
        default: break;
    }
    cbdos::radio::setMode(mode);

    char toast[64];
    snprintf(toast, sizeof(toast), "Radio: %s", cbdos::radio::getModeName(mode));
    UIManager::showToast(toast);
}

static lv_obj_t* createSliderRow(lv_obj_t* parent, const char* labelText,
                                  int32_t minVal, int32_t maxVal, int32_t curVal,
                                  lv_event_cb_t cb) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(row);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, labelText);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    lv_obj_t* slider = lv_slider_create(row);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 12);
    lv_slider_set_range(slider, minVal, maxVal);
    lv_slider_set_value(slider, curVal, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(slider, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(slider, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, DefaultTheme::getPrimaryAccent(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);

    lv_obj_add_event_cb(slider, cb, LV_EVENT_ALL, NULL);
    return slider;
}

void QuickSettingsPanel::toggle() {
    static uint32_t lastToggleMs = 0;
    uint32_t now = lv_tick_get();
    if (now - lastToggleMs < 300) return;
    lastToggleMs = now;

    if (panelObj && lv_obj_is_valid(panelObj)) {
        hide();
        return;
    }

    panelObj = lv_obj_create(lv_layer_top());
    lv_obj_set_width(panelObj, 300);
    lv_obj_set_height(panelObj, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(panelObj, 18);
    lv_obj_align(panelObj, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_flex_flow(panelObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(panelObj, 14, 0);
    lv_obj_set_style_pad_row(panelObj, 10, 0);

    // Fila 0: Header con título y botón X
    lv_obj_t* headerRow = lv_obj_create(panelObj);
    lv_obj_set_width(headerRow, lv_pct(100));
    lv_obj_set_height(headerRow, 30);
    lv_obj_set_style_bg_opa(headerRow, 0, 0);
    lv_obj_set_style_border_width(headerRow, 0, 0);
    lv_obj_set_style_pad_all(headerRow, 0, 0);
    DefaultTheme::disableScroll(headerRow);

    lv_obj_t* lblTitle = lv_label_create(headerRow);
    lv_label_set_text(lblTitle, LV_SYMBOL_SETTINGS " Ajustes Rápidos");
    lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(lblTitle, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* btnClose = lv_button_create(headerRow);
    lv_obj_set_size(btnClose, 30, 30);
    lv_obj_align(btnClose, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btnClose, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_bg_opa(btnClose, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btnClose, 8, 0);
    lv_obj_set_style_shadow_width(btnClose, 0, 0);
    lv_obj_set_style_border_width(btnClose, 0, 0);
    lv_obj_add_event_cb(btnClose, mask_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lblX, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lblX, &lv_font_montserrat_14, 0);
    lv_obj_center(lblX);

    // Fila 1: WiFi Switch
    lv_obj_t* rowWifi = lv_obj_create(panelObj);
    lv_obj_set_width(rowWifi, lv_pct(100));
    lv_obj_set_height(rowWifi, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowWifi, 0, 0);
    lv_obj_set_style_border_width(rowWifi, 0, 0);
    lv_obj_set_style_pad_all(rowWifi, 0, 0);
    lv_obj_set_flex_flow(rowWifi, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowWifi, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(rowWifi);

    lv_obj_t* lblWifi = lv_label_create(rowWifi);
    lv_label_set_text(lblWifi, LV_SYMBOL_WIFI " Conexión WiFi");
    lv_obj_set_style_text_color(lblWifi, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblWifi, &lv_font_montserrat_12, 0);

    lv_obj_t* swWifi = lv_switch_create(rowWifi);
    if (cbdos::network::isConnected() || ConfigManager::getInstance().isWifiAutoConnect()) {
        lv_obj_add_state(swWifi, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(swWifi, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Fila 2: Slider Volumen
    createSliderRow(panelObj, LV_SYMBOL_AUDIO " Volumen",
                    0, 100, cbdos::audio::getVolume(),
                    volume_slider_cb);

    // Fila 3: Slider Brillo
    createSliderRow(panelObj, LV_SYMBOL_EYE_OPEN " Brillo",
                    10, 100, cbdos::display::getBrightness(),
                    brightness_slider_cb);

    // Fila 4: Selector Modo de Radio
    lv_obj_t* rowRadio = lv_obj_create(panelObj);
    lv_obj_set_width(rowRadio, lv_pct(100));
    lv_obj_set_height(rowRadio, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowRadio, 0, 0);
    lv_obj_set_style_border_width(rowRadio, 0, 0);
    lv_obj_set_style_pad_all(rowRadio, 0, 0);
    lv_obj_set_style_pad_row(rowRadio, 4, 0);
    lv_obj_set_flex_flow(rowRadio, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(rowRadio);

    lv_obj_t* lblRadio = lv_label_create(rowRadio);
    lv_label_set_text(lblRadio, LV_SYMBOL_SETTINGS " Modo Radio Integrada");
    lv_obj_set_style_text_color(lblRadio, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblRadio, &lv_font_montserrat_12, 0);

    lv_obj_t* ddRadio = lv_dropdown_create(rowRadio);
    lv_obj_set_width(ddRadio, lv_pct(100));
    lv_obj_set_height(ddRadio, 34);
    lv_dropdown_set_options(ddRadio,
        "📶 Wi-Fi (Estación TCP/IP)\n"
        "📻 ESP-NOW Normal (1-2 Mbps)\n"
        "🚀 ESP-NOW Long Range\n"
        "⚡ Híbrido (Wi-Fi + ESP-NOW)"
    );
    uint16_t currentModeIdx = 1;
    auto curMode = cbdos::radio::getMode();
    if (curMode == cbdos::radio::RadioMode::WifiSta) currentModeIdx = 0;
    else if (curMode == cbdos::radio::RadioMode::EspNow) currentModeIdx = 1;
    else if (curMode == cbdos::radio::RadioMode::EspNowLR) currentModeIdx = 2;
    else if (curMode == cbdos::radio::RadioMode::Hybrid) currentModeIdx = 3;

    lv_dropdown_set_selected(ddRadio, currentModeIdx);
    lv_obj_set_style_radius(ddRadio, 8, 0);
    lv_obj_set_style_bg_color(ddRadio, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(ddRadio, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ddRadio, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(ddRadio, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(ddRadio, radio_mode_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Botón directo a Configuración de Radio
    lv_obj_t* btnOpenRadio = lv_button_create(panelObj);
    lv_obj_set_width(btnOpenRadio, lv_pct(100));
    lv_obj_set_height(btnOpenRadio, 32);
    DefaultTheme::applyButton(btnOpenRadio, 8);
    lv_obj_set_style_bg_color(btnOpenRadio, lv_color_hex(0x0284C7), 0);
    lv_obj_add_event_cb(btnOpenRadio, [](lv_event_t* e) {
        QuickSettingsPanel::hide();
        using cbdos::security::LockService;
        using cbdos::security::LockPolicy;
        if (LockService::getInstance().getPolicy() == LockPolicy::SettingsOnly) {
            LockPinModal::show(
                cbdos::lang::tr(cbdos::lang::StrId::STR_SEC_ENTER_PIN),
                []() {
                    UIManager::getInstance().pushView(std::make_shared<RadioConfigView>());
                }
            );
        } else {
            UIManager::getInstance().pushView(std::make_shared<RadioConfigView>());
        }
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblBtnOpen = lv_label_create(btnOpenRadio);
    lv_label_set_text(lblBtnOpen, "📻 Configuración de Radio Integrada");
    lv_obj_set_style_text_font(lblBtnOpen, &lv_font_montserrat_12, 0);
    lv_obj_center(lblBtnOpen);
}

} // namespace ui
} // namespace cbdos
