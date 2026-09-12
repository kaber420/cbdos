#include "NetworkManagerView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/network.hpp"
#include "cbdos/time.hpp"
#include "cbdos/language.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

namespace cbdos {
namespace ui {

using cbdos::lang::tr;
using cbdos::lang::StrId;

NetworkManagerView* NetworkManagerView::s_instance = nullptr;
cbdos::radio::RadioConfig NetworkManagerView::s_radioCfg;
WiFiConfig NetworkManagerView::s_wifiCfg;
static std::vector<cbdos::radio::WifiApInfo> s_scannedAps;
static std::vector<cbdos::radio::DiscoveredNode> s_sweepNodes;

NetworkManagerView::NetworkManagerView()
    : BaseView(tr(StrId::STR_CFG_NET)) {
    s_instance = this;
    ConfigManager::getInstance().loadRadio(s_radioCfg);
    ConfigManager::getInstance().loadWiFi(s_wifiCfg);
}

NetworkManagerView::~NetworkManagerView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void NetworkManagerView::onDestroy() {
    UIManager::closeKeyboard();
    BaseView::onDestroy();
}

bool NetworkManagerView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor principal con scroll vertical
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 24, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    buildSlot0Radio(m_container);
    buildSlot1Backpack(m_container);
    buildSlot2Usb(m_container);

    updateModeVisibility();

    return true;
}

void NetworkManagerView::buildSlot0Radio(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(card, 14);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 10, 0);

    // Cabecera Slot 0 con switch maestro
    lv_obj_t* headerRow = lv_obj_create(card);
    lv_obj_set_size(headerRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(headerRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(headerRow, 0, 0);
    lv_obj_set_style_pad_all(headerRow, 0, 0);
    lv_obj_set_flex_flow(headerRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(headerRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblTitle = lv_label_create(headerRow);
    lv_label_set_text(lblTitle, (std::string(LV_SYMBOL_WIFI " ") + tr(StrId::STR_NET_SLOT0)).c_str());
    lv_obj_set_style_text_color(lblTitle, lv_color_white(), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);

    m_swPower = lv_switch_create(headerRow);
    if (s_radioCfg.enabled && s_radioCfg.mode != cbdos::radio::RadioMode::Off) {
        lv_obj_add_state(m_swPower, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(m_swPower, radioPowerSwCb, LV_EVENT_VALUE_CHANGED, this);

    // Selector de Modo
    lv_obj_t* rowMode = lv_obj_create(card);
    lv_obj_set_size(rowMode, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowMode, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowMode, 0, 0);
    lv_obj_set_style_pad_all(rowMode, 0, 0);
    lv_obj_set_flex_flow(rowMode, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowMode, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblMode = lv_label_create(rowMode);
    lv_label_set_text(lblMode, tr(StrId::STR_NET_MODE));
    lv_obj_set_style_text_color(lblMode, lv_color_hex(0x94A3B8), 0);

    m_ddMode = lv_dropdown_create(rowMode);
    lv_obj_set_size(m_ddMode, 160, 36);
    lv_dropdown_set_options(m_ddMode, tr(StrId::STR_NET_MODES));
    
    if (s_radioCfg.mode == cbdos::radio::RadioMode::EspNow) lv_dropdown_set_selected(m_ddMode, 0);
    else if (s_radioCfg.mode == cbdos::radio::RadioMode::EspNowLR) lv_dropdown_set_selected(m_ddMode, 1);
    else if (s_radioCfg.mode == cbdos::radio::RadioMode::WifiSta) lv_dropdown_set_selected(m_ddMode, 2);
    else if (s_radioCfg.mode == cbdos::radio::RadioMode::Off) lv_dropdown_set_selected(m_ddMode, 4);
    else lv_dropdown_set_selected(m_ddMode, 0);

    lv_obj_add_event_cb(m_ddMode, modeSelectCb, LV_EVENT_VALUE_CHANGED, this);

    // ────────────────────────────────────────────────────────────
    // Sub-Panel ESP-NOW / LR (Canal + Potencia + Barrido)
    // ────────────────────────────────────────────────────────────
    m_boxMeshControls = lv_obj_create(card);
    lv_obj_set_size(m_boxMeshControls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(m_boxMeshControls, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_color(m_boxMeshControls, lv_color_hex(0x222B3D), 0);
    lv_obj_set_style_border_width(m_boxMeshControls, 1, 0);
    lv_obj_set_style_radius(m_boxMeshControls, 8, 0);
    lv_obj_set_style_pad_all(m_boxMeshControls, 10, 0);
    lv_obj_set_flex_flow(m_boxMeshControls, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_boxMeshControls, 8, 0);

    m_lblChannelVal = lv_label_create(m_boxMeshControls);
    char bufCh[32];
    snprintf(bufCh, sizeof(bufCh), tr(StrId::STR_NET_CH), s_radioCfg.channel);
    lv_label_set_text(m_lblChannelVal, bufCh);
    lv_obj_set_style_text_color(m_lblChannelVal, lv_color_hex(0x00E5FF), 0);

    m_sliderChannel = lv_slider_create(m_boxMeshControls);
    lv_obj_set_width(m_sliderChannel, lv_pct(100));
    lv_slider_set_range(m_sliderChannel, 1, 13);
    lv_slider_set_value(m_sliderChannel, s_radioCfg.channel, LV_ANIM_OFF);
    lv_obj_add_event_cb(m_sliderChannel, channelSliderCb, LV_EVENT_VALUE_CHANGED, this);

    m_lblTxVal = lv_label_create(m_boxMeshControls);
    char bufTx[32];
    snprintf(bufTx, sizeof(bufTx), tr(StrId::STR_NET_TX), s_radioCfg.txPower);
    lv_label_set_text(m_lblTxVal, bufTx);
    lv_obj_set_style_text_color(m_lblTxVal, lv_color_hex(0x00E5FF), 0);

    m_sliderTx = lv_slider_create(m_boxMeshControls);
    lv_obj_set_width(m_sliderTx, lv_pct(100));
    lv_slider_set_range(m_sliderTx, 2, 20);
    lv_slider_set_value(m_sliderTx, s_radioCfg.txPower, LV_ANIM_OFF);
    lv_obj_add_event_cb(m_sliderTx, txPowerSliderCb, LV_EVENT_VALUE_CHANGED, this);

    // Botón Barrido ESP-NOW
    m_btnSweep = lv_button_create(m_boxMeshControls);
    lv_obj_set_size(m_btnSweep, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(m_btnSweep, lv_color_hex(0x1F293D), 0);
    lv_obj_set_style_border_color(m_btnSweep, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(m_btnSweep, 1, 0);
    lv_obj_add_event_cb(m_btnSweep, sweepBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblSwp = lv_label_create(m_btnSweep);
    lv_label_set_text(lblSwp, (std::string(LV_SYMBOL_REFRESH " ") + tr(StrId::STR_NET_SWEEP)).c_str());
    lv_obj_set_style_text_color(lblSwp, lv_color_hex(0x00E5FF), 0);
    lv_obj_center(lblSwp);

    m_lblSweepStatus = lv_label_create(m_boxMeshControls);
    lv_label_set_text(m_lblSweepStatus, tr(StrId::STR_NET_SWEEP_IDLE));
    lv_obj_set_style_text_color(m_lblSweepStatus, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(m_lblSweepStatus, &lv_font_montserrat_12, 0);

    m_sweepListContainer = lv_obj_create(m_boxMeshControls);
    lv_obj_set_size(m_sweepListContainer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_sweepListContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_sweepListContainer, 0, 0);
    lv_obj_set_style_pad_all(m_sweepListContainer, 0, 0);
    lv_obj_set_flex_flow(m_sweepListContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_sweepListContainer, 4, 0);

    // ────────────────────────────────────────────────────────────
    // Sub-Panel Wi-Fi STA (Escaneo + Conexión + IP)
    // ────────────────────────────────────────────────────────────
    m_boxWifiControls = lv_obj_create(card);
    lv_obj_set_size(m_boxWifiControls, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(m_boxWifiControls, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_color(m_boxWifiControls, lv_color_hex(0x222B3D), 0);
    lv_obj_set_style_border_width(m_boxWifiControls, 1, 0);
    lv_obj_set_style_radius(m_boxMeshControls, 8, 0);
    lv_obj_set_style_pad_all(m_boxWifiControls, 10, 0);
    lv_obj_set_flex_flow(m_boxWifiControls, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_boxWifiControls, 8, 0);

    m_btnScanWifi = lv_button_create(m_boxWifiControls);
    lv_obj_set_size(m_btnScanWifi, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(m_btnScanWifi, lv_color_hex(0x005577), 0);
    lv_obj_set_style_border_color(m_btnScanWifi, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(m_btnScanWifi, 1, 0);
    lv_obj_add_event_cb(m_btnScanWifi, wifiScanBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblScn = lv_label_create(m_btnScanWifi);
    lv_label_set_text(lblScn, (std::string(LV_SYMBOL_WIFI " ") + tr(StrId::STR_NET_SCAN)).c_str());
    lv_obj_set_style_text_color(lblScn, lv_color_white(), 0);
    lv_obj_center(lblScn);

    m_lblWifiScanStatus = lv_label_create(m_boxWifiControls);
    lv_label_set_text(m_lblWifiScanStatus, tr(StrId::STR_NET_SCAN_IDLE));
    lv_obj_set_style_text_color(m_lblWifiScanStatus, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(m_lblWifiScanStatus, &lv_font_montserrat_12, 0);

    m_wifiListContainer = lv_obj_create(m_boxWifiControls);
    lv_obj_set_size(m_wifiListContainer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_wifiListContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_wifiListContainer, 0, 0);
    lv_obj_set_style_pad_all(m_wifiListContainer, 0, 0);
    lv_obj_set_flex_flow(m_wifiListContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_wifiListContainer, 4, 0);
}

void NetworkManagerView::buildSlot1Backpack(lv_obj_t* parent) {
    m_cardBackpack = lv_obj_create(parent);
    lv_obj_set_width(m_cardBackpack, lv_pct(100));
    lv_obj_set_height(m_cardBackpack, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(m_cardBackpack, 14);
    lv_obj_set_flex_flow(m_cardBackpack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_cardBackpack, 12, 0);
    lv_obj_set_style_pad_row(m_cardBackpack, 8, 0);

    lv_obj_t* rowHeader = lv_obj_create(m_cardBackpack);
    lv_obj_set_size(rowHeader, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowHeader, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowHeader, 0, 0);
    lv_obj_set_style_pad_all(rowHeader, 0, 0);
    lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblTitle = lv_label_create(rowHeader);
    lv_label_set_text(lblTitle, (std::string(LV_SYMBOL_DRIVE " ") + tr(StrId::STR_NET_SLOT1)).c_str());
    lv_obj_set_style_text_color(lblTitle, lv_color_white(), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);

    auto* iface1 = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(1);
    bool isConnected = iface1 && iface1->isReady();

    m_lblBackpackStatus = lv_label_create(m_cardBackpack);
    if (!isConnected) {
        lv_label_set_text(m_lblBackpackStatus, tr(StrId::STR_NET_BP_OFF));
        lv_obj_set_style_text_color(m_lblBackpackStatus, lv_color_hex(0x64748B), 0);
    } else {
        lv_label_set_text(m_lblBackpackStatus, tr(StrId::STR_NET_BP_ON));
        lv_obj_set_style_text_color(m_lblBackpackStatus, lv_color_hex(0x10B981), 0);
    }
    lv_obj_set_style_text_font(m_lblBackpackStatus, &lv_font_montserrat_12, 0);
}

void NetworkManagerView::buildSlot2Usb(lv_obj_t* parent) {
    m_cardUsb = lv_obj_create(parent);
    lv_obj_set_width(m_cardUsb, lv_pct(100));
    lv_obj_set_height(m_cardUsb, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(m_cardUsb, 14);
    lv_obj_set_flex_flow(m_cardUsb, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_cardUsb, 12, 0);
    lv_obj_set_style_pad_row(m_cardUsb, 8, 0);

    lv_obj_t* rowHeader = lv_obj_create(m_cardUsb);
    lv_obj_set_size(rowHeader, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowHeader, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowHeader, 0, 0);
    lv_obj_set_style_pad_all(rowHeader, 0, 0);
    lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblTitle = lv_label_create(rowHeader);
    lv_label_set_text(lblTitle, (std::string(LV_SYMBOL_USB " ") + tr(StrId::STR_NET_SLOT2)).c_str());
    lv_obj_set_style_text_color(lblTitle, lv_color_white(), 0);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);

    auto* iface2 = cbdos::network::NetworkInterfaceManager::getInstance().getInterface(2);
    bool isConnected = iface2 && iface2->isReady();

    lv_obj_t* lblStatus = lv_label_create(m_cardUsb);
    if (isConnected) {
        uint8_t mac[6] = {0};
        char mac_str[32] = "N/A";
        if (iface2->getMacAddress(mac)) {
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
        const char* alias = iface2->getAlias();
        const char* modeName = (iface2->getMode() == cbdos::network::InterfaceMode::EspNowLR) ? "ESP-NOW LR" : "ESP-NOW Normal";

        char info[160];
        snprintf(info, sizeof(info), tr(StrId::STR_NET_USB_INFO),
                 (alias && alias[0]) ? alias : tr(StrId::STR_NET_USB_MODEM), modeName, iface2->getChannel(), mac_str);
        lv_label_set_text(lblStatus, info);
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x00E5FF), 0);
    } else {
        lv_label_set_text(lblStatus, tr(StrId::STR_NET_USB_OFF));
        lv_obj_set_style_text_color(lblStatus, lv_color_hex(0x64748B), 0);
    }
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);
}

void NetworkManagerView::updateModeVisibility() {
    bool isPowered = s_radioCfg.enabled && (s_radioCfg.mode != cbdos::radio::RadioMode::Off);

    if (m_boxWifiControls && lv_obj_is_valid(m_boxWifiControls)) {
        if (isPowered && s_radioCfg.mode == cbdos::radio::RadioMode::WifiSta) {
            lv_obj_remove_flag(m_boxWifiControls, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_boxWifiControls, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (m_boxMeshControls && lv_obj_is_valid(m_boxMeshControls)) {
        if (isPowered && (s_radioCfg.mode == cbdos::radio::RadioMode::EspNow || s_radioCfg.mode == cbdos::radio::RadioMode::EspNowLR)) {
            lv_obj_remove_flag(m_boxMeshControls, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_boxMeshControls, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void NetworkManagerView::radioPowerSwCb(lv_event_t* e) {
    auto* sw = (lv_obj_t*)lv_event_get_target(e);
    auto* view = static_cast<NetworkManagerView*>(lv_event_get_user_data(e));
    if (!sw || !view) return;

    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    s_radioCfg.enabled = enabled;
    if (!enabled) {
        s_radioCfg.mode = cbdos::radio::RadioMode::Off;
        cbdos::radio::setRadioPower(false);
        // Radio apagada a proposito: no reconectar WiFi en el arranque.
        ConfigManager::getInstance().setWifiAutoConnect(false);
    } else {
        s_radioCfg.mode = cbdos::radio::RadioMode::EspNow;
        cbdos::radio::setRadioPower(true);
        cbdos::radio::setMode(s_radioCfg.mode);
    }

    ConfigManager::getInstance().saveRadio(s_radioCfg);
    view->updateModeVisibility();
    UIManager::showToast(enabled ? tr(StrId::STR_NET_RADIO_ON) : tr(StrId::STR_NET_RADIO_OFF));
}

void NetworkManagerView::modeSelectCb(lv_event_t* e) {
    auto* dd = (lv_obj_t*)lv_event_get_target(e);
    auto* view = static_cast<NetworkManagerView*>(lv_event_get_user_data(e));
    if (!dd || !view) return;

    uint16_t sel = lv_dropdown_get_selected(dd);
    if (sel == 0) {
        s_radioCfg.mode = cbdos::radio::RadioMode::EspNow;
        s_radioCfg.enabled = true;
    } else if (sel == 1) {
        s_radioCfg.mode = cbdos::radio::RadioMode::EspNowLR;
        s_radioCfg.enabled = true;
    } else if (sel == 2) {
        s_radioCfg.mode = cbdos::radio::RadioMode::WifiSta;
        s_radioCfg.enabled = true;
    } else if (sel == 4) {
        s_radioCfg.mode = cbdos::radio::RadioMode::Off;
        s_radioCfg.enabled = false;
    }

    cbdos::radio::setMode(s_radioCfg.mode);
    ConfigManager::getInstance().saveRadio(s_radioCfg);
    view->updateModeVisibility();
    UIManager::showToast(tr(StrId::STR_NET_MODE_SAVED));
}


void NetworkManagerView::channelSliderCb(lv_event_t* e) {
    auto* sl = (lv_obj_t*)lv_event_get_target(e);
    auto* view = static_cast<NetworkManagerView*>(lv_event_get_user_data(e));
    if (!sl || !view) return;

    s_radioCfg.channel = static_cast<uint8_t>(lv_slider_get_value(sl));
    cbdos::radio::setChannel(s_radioCfg.channel);
    ConfigManager::getInstance().saveRadio(s_radioCfg);

    if (view->m_lblChannelVal && lv_obj_is_valid(view->m_lblChannelVal)) {
        char buf[32];
        snprintf(buf, sizeof(buf), tr(StrId::STR_NET_CH), s_radioCfg.channel);
        lv_label_set_text(view->m_lblChannelVal, buf);
    }
}

void NetworkManagerView::txPowerSliderCb(lv_event_t* e) {
    auto* sl = (lv_obj_t*)lv_event_get_target(e);
    auto* view = static_cast<NetworkManagerView*>(lv_event_get_user_data(e));
    if (!sl || !view) return;

    s_radioCfg.txPower = static_cast<int8_t>(lv_slider_get_value(sl));
    cbdos::radio::setTxPower(s_radioCfg.txPower);
    ConfigManager::getInstance().saveRadio(s_radioCfg);

    if (view->m_lblTxVal && lv_obj_is_valid(view->m_lblTxVal)) {
        char buf[32];
        snprintf(buf, sizeof(buf), tr(StrId::STR_NET_TX), s_radioCfg.txPower);
        lv_label_set_text(view->m_lblTxVal, buf);
    }
}

void NetworkManagerView::wifiScanBtnCb(lv_event_t* e) {
    auto* view = static_cast<NetworkManagerView*>(lv_event_get_user_data(e));
    if (!view) return;

    if (view->m_lblWifiScanStatus && lv_obj_is_valid(view->m_lblWifiScanStatus)) {
        lv_label_set_text(view->m_lblWifiScanStatus, tr(StrId::STR_NET_SCANNING));
        lv_obj_set_style_text_color(view->m_lblWifiScanStatus, lv_palette_main(LV_PALETTE_CYAN), 0);
    }

    cbdos::radio::startWifiScan([](const std::vector<cbdos::radio::WifiApInfo>& aps, bool success) {
        s_scannedAps = aps;
        lv_async_call([](void* user_data) {
            auto* v = static_cast<NetworkManagerView*>(user_data);
            if (v) v->refreshWifiListUi();
        }, s_instance);
    });
}

void NetworkManagerView::refreshWifiListUi() {
    if (!m_wifiListContainer || !lv_obj_is_valid(m_wifiListContainer)) return;
    lv_obj_clean(m_wifiListContainer);

    if (s_scannedAps.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_wifiListContainer);
        lv_label_set_text(emptyLbl, tr(StrId::STR_NET_NO_APS));
        lv_obj_set_style_text_color(emptyLbl, lv_color_hex(0x64748B), 0);
        return;
    }

    for (size_t i = 0; i < s_scannedAps.size(); i++) {
        const auto& ap = s_scannedAps[i];

        lv_obj_t* card = lv_button_create(m_wifiListContainer);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 44);
        DefaultTheme::applyButton(card, 8);
        lv_obj_set_user_data(card, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(card, wifiApClickCb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(card, 10, 0);

        lv_obj_t* lblSsid = lv_label_create(card);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %s", ap.isEncrypted ? LV_SYMBOL_SETTINGS : LV_SYMBOL_WIFI, ap.ssid.c_str());
        lv_label_set_text(lblSsid, buf);
        lv_obj_set_style_text_color(lblSsid, lv_color_white(), 0);

        lv_obj_t* lblSignal = lv_label_create(card);
        char sig[24];
        snprintf(sig, sizeof(sig), "%d dBm", ap.rssi);
        lv_label_set_text(lblSignal, sig);
        lv_obj_set_style_text_color(lblSignal, lv_color_hex(0x00E5FF), 0);
    }
}

void NetworkManagerView::wifiApClickCb(lv_event_t* e) {
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= s_scannedAps.size()) return;

    const auto& ap = s_scannedAps[idx];

    // Abrir diálogo de conexión
    lv_obj_t* modal = lv_obj_create(lv_screen_active());
    lv_obj_set_size(modal, 280, 220);
    lv_obj_center(modal);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_color(modal, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(modal, 2, 0);
    lv_obj_set_style_radius(modal, 12, 0);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    lv_obj_t* lblTitle = lv_label_create(modal);
    char t[64];
    snprintf(t, sizeof(t), tr(StrId::STR_NET_CONNECT_TO), ap.ssid.c_str());
    lv_label_set_text(lblTitle, t);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* taPass = lv_textarea_create(modal);
    lv_obj_set_size(taPass, LV_PCT(100), 38);
    lv_textarea_set_placeholder_text(taPass, tr(StrId::STR_NET_PASS_PH));
    lv_textarea_set_password_mode(taPass, true);
    lv_textarea_set_one_line(taPass, true);
    lv_obj_add_event_cb(taPass, [](lv_event_t* ev) {
        UIManager::attachKeyboard((lv_obj_t*)lv_event_get_target(ev));
    }, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_size(btnCancel, LV_PCT(45), 36);
    lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0x334155), 0);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, tr(StrId::STR_ST_CANCEL));
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, [](lv_event_t* ev) {
        lv_obj_t* m = (lv_obj_t*)lv_event_get_user_data(ev);
        UIManager::closeKeyboard();
        lv_obj_delete(m);
    }, LV_EVENT_CLICKED, modal);

    struct ConnCtx {
        std::string ssid;
        lv_obj_t* modal;
        lv_obj_t* taPass;
    };
    auto* ctx = new ConnCtx{ap.ssid, modal, taPass};

    lv_obj_t* btnConn = lv_button_create(btnRow);
    lv_obj_set_size(btnConn, LV_PCT(48), 36);
    lv_obj_set_style_bg_color(btnConn, lv_color_hex(0x00E5FF), 0);
    lv_obj_t* lblOk = lv_label_create(btnConn);
    lv_label_set_text(lblOk, tr(StrId::STR_NET_CONNECT));
    lv_obj_set_style_text_color(lblOk, lv_color_black(), 0);
    lv_obj_center(lblOk);

    lv_obj_add_event_cb(btnConn, [](lv_event_t* ev) {
        auto* c = static_cast<ConnCtx*>(lv_event_get_user_data(ev));
        if (c) {
            const char* pass = lv_textarea_get_text(c->taPass);
            WiFiConfig cfg;
            cfg.ssid = c->ssid;
            cfg.password = pass ? pass : "";
            ConfigManager::getInstance().saveWiFi(cfg);
            // Marcar autoconexion: sin esto el arranque nunca reconecta
            // (antes lo hacia WiFiConfigView al guardar).
            ConfigManager::getInstance().setWifiAutoConnect(true);
            cbdos::network::connectWifi(cfg.ssid.c_str(), cfg.password.c_str());
            UIManager::showToast(tr(StrId::STR_NET_CONNECTING));
            UIManager::closeKeyboard();
            lv_obj_delete(c->modal);
            delete c;
        }
    }, LV_EVENT_CLICKED, ctx);
}

void NetworkManagerView::sweepBtnCb(lv_event_t* e) {
    auto* view = static_cast<NetworkManagerView*>(lv_event_get_user_data(e));
    if (!view) return;

    if (view->m_lblSweepStatus && lv_obj_is_valid(view->m_lblSweepStatus)) {
        lv_label_set_text(view->m_lblSweepStatus, tr(StrId::STR_NET_SWEEPING));
        lv_obj_set_style_text_color(view->m_lblSweepStatus, lv_palette_main(LV_PALETTE_CYAN), 0);
    }

    cbdos::radio::startChannelSweep([](uint8_t currentChannel, uint8_t totalChannels, const std::vector<cbdos::radio::DiscoveredNode>& nodes, bool finished) {
        if (finished) {
            s_sweepNodes = nodes;
            lv_async_call([](void* user_data) {
                auto* v = static_cast<NetworkManagerView*>(user_data);
                if (v) v->refreshSweepListUi();
            }, s_instance);
        }
    });
}

void NetworkManagerView::refreshSweepListUi() {
    if (!m_sweepListContainer || !lv_obj_is_valid(m_sweepListContainer)) return;
    lv_obj_clean(m_sweepListContainer);

    if (m_lblSweepStatus && lv_obj_is_valid(m_lblSweepStatus)) {
        char buf[64];
        snprintf(buf, sizeof(buf), tr(StrId::STR_NET_SWEEP_DONE), s_sweepNodes.size());
        lv_label_set_text(m_lblSweepStatus, buf);
        lv_obj_set_style_text_color(m_lblSweepStatus, lv_color_hex(0x10B981), 0);
    }

    for (const auto& node : s_sweepNodes) {
        lv_obj_t* card = lv_obj_create(m_sweepListContainer);
        lv_obj_set_size(card, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x161C28), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x28334A), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(card);
        char info[64];
        snprintf(info, sizeof(info), "Nodo 0x%04X (Ch %u)", node.short_id, node.channel);
        lv_label_set_text(lbl, info);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);

        lv_obj_t* lblRssi = lv_label_create(card);
        char rssi[24];
        snprintf(rssi, sizeof(rssi), "%d dBm", node.rssi);
        lv_label_set_text(lblRssi, rssi);
        lv_obj_set_style_text_color(lblRssi, lv_color_hex(0x00E5FF), 0);
    }
}

} // namespace ui
} // namespace cbdos
