#include "RadioConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/network.hpp"
#include "cbdos/display.hpp"
#include "cbdos/config_manager.hpp"
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

RadioConfigView* RadioConfigView::s_instance = nullptr;
cbdos::radio::RadioConfig RadioConfigView::s_cfg;

lv_obj_t* RadioConfigView::s_swPower = nullptr;
lv_obj_t* RadioConfigView::s_lblPowerStatus = nullptr;
lv_obj_t* RadioConfigView::s_ddMode = nullptr;
lv_obj_t* RadioConfigView::s_sliderChannel = nullptr;
lv_obj_t* RadioConfigView::s_lblChannelVal = nullptr;
lv_obj_t* RadioConfigView::s_sliderTx = nullptr;
lv_obj_t* RadioConfigView::s_lblTxVal = nullptr;

lv_obj_t* RadioConfigView::s_boxWifi = nullptr;
lv_obj_t* RadioConfigView::s_btnScanWifi = nullptr;
lv_obj_t* RadioConfigView::s_lblWifiScanStatus = nullptr;
lv_obj_t* RadioConfigView::s_wifiListContainer = nullptr;

lv_obj_t* RadioConfigView::s_boxMesh = nullptr;
lv_obj_t* RadioConfigView::s_btnSweep = nullptr;
lv_obj_t* RadioConfigView::s_barSweepProgress = nullptr;
lv_obj_t* RadioConfigView::s_lblSweepStatus = nullptr;
lv_obj_t* RadioConfigView::s_sweepListContainer = nullptr;

std::vector<cbdos::radio::WifiApInfo> RadioConfigView::s_scannedAps;
std::vector<cbdos::radio::DiscoveredNode> RadioConfigView::s_sweptNodes;

RadioConfigView::RadioConfigView()
    : BaseView("Radio Integrada") {
    s_instance = this;
}

RadioConfigView::~RadioConfigView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void RadioConfigView::onDestroy() {
    cbdos::radio::stopScan();
    BaseView::onDestroy();
}

void RadioConfigView::radio_power_sw_cb(lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    if (!sw) return;

    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    s_cfg.enabled = on;
    cbdos::radio::setRadioPower(on);

    if (s_lblPowerStatus && lv_obj_is_valid(s_lblPowerStatus)) {
        if (on) {
            lv_label_set_text(s_lblPowerStatus, "Estado: Encendida (Módem RF Activo)");
            lv_obj_set_style_text_color(s_lblPowerStatus, lv_color_hex(0x10B981), 0);
        } else {
            lv_label_set_text(s_lblPowerStatus, "Estado: Apagada (Modo Seguro / Offline)");
            lv_obj_set_style_text_color(s_lblPowerStatus, lv_color_hex(0x9CA3AF), 0);
        }
    }

    update_mode_visibility();
}

void RadioConfigView::mode_select_cb(lv_event_t* e) {
    lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
    if (!dd) return;

    uint32_t sel = lv_dropdown_get_selected(dd);
    // Mapeo: 0 = WifiSta, 1 = EspNow, 2 = EspNowLR, 3 = Hybrid
    switch (sel) {
        case 0: s_cfg.mode = cbdos::radio::RadioMode::WifiSta; break;
        case 1: s_cfg.mode = cbdos::radio::RadioMode::EspNow; break;
        case 2: s_cfg.mode = cbdos::radio::RadioMode::EspNowLR; break;
        case 3: s_cfg.mode = cbdos::radio::RadioMode::Hybrid; break;
        default: s_cfg.mode = cbdos::radio::RadioMode::EspNow; break;
    }

    cbdos::radio::setMode(s_cfg.mode);
    update_mode_visibility();

    char toast[64];
    snprintf(toast, sizeof(toast), "Modo: %s", cbdos::radio::getModeName(s_cfg.mode));
    UIManager::showToast(toast);
}

void RadioConfigView::channel_slider_cb(lv_event_t* e) {
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    if (!sl) return;

    int32_t val = lv_slider_get_value(sl);
    s_cfg.channel = static_cast<uint8_t>(val);
    cbdos::radio::setChannel(s_cfg.channel);

    if (s_lblChannelVal && lv_obj_is_valid(s_lblChannelVal)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Canal RF: %u", s_cfg.channel);
        lv_label_set_text(s_lblChannelVal, buf);
    }
}

void RadioConfigView::txpower_slider_cb(lv_event_t* e) {
    lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
    if (!sl) return;

    int32_t val = lv_slider_get_value(sl);
    s_cfg.txPower = static_cast<int8_t>(val);
    cbdos::radio::setTxPower(s_cfg.txPower);

    if (s_lblTxVal && lv_obj_is_valid(s_lblTxVal)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Potencia TX: +%d dBm", s_cfg.txPower);
        lv_label_set_text(s_lblTxVal, buf);
    }
}

void RadioConfigView::update_mode_visibility() {
    bool isPowered = s_cfg.enabled && (s_cfg.mode != cbdos::radio::RadioMode::Off);

    if (s_boxWifi && lv_obj_is_valid(s_boxWifi)) {
        if (isPowered && (s_cfg.mode == cbdos::radio::RadioMode::WifiSta || s_cfg.mode == cbdos::radio::RadioMode::Hybrid)) {
            lv_obj_remove_flag(s_boxWifi, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_boxWifi, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_boxMesh && lv_obj_is_valid(s_boxMesh)) {
        if (isPowered && (s_cfg.mode == cbdos::radio::RadioMode::EspNow || 
                          s_cfg.mode == cbdos::radio::RadioMode::EspNowLR || 
                          s_cfg.mode == cbdos::radio::RadioMode::Hybrid)) {
            lv_obj_remove_flag(s_boxMesh, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_boxMesh, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ─── Wi-Fi Scan & UI ───
void RadioConfigView::wifi_scan_btn_cb(lv_event_t* e) {
    (void)e;
    if (s_lblWifiScanStatus && lv_obj_is_valid(s_lblWifiScanStatus)) {
        lv_label_set_text(s_lblWifiScanStatus, "Escaneando redes Wi-Fi...");
        lv_obj_set_style_text_color(s_lblWifiScanStatus, lv_palette_main(LV_PALETTE_CYAN), 0);
    }

    cbdos::radio::startWifiScan([](const std::vector<cbdos::radio::WifiApInfo>& aps, bool success) {
        s_scannedAps = aps;
        // Notificar en el hilo de UI
        lv_timer_t* t = lv_timer_create([](lv_timer_t* timer) {
            refresh_wifi_list_ui();
            if (s_lblWifiScanStatus && lv_obj_is_valid(s_lblWifiScanStatus)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Escaneo completado: %u redes encontradas", (unsigned int)s_scannedAps.size());
                lv_label_set_text(s_lblWifiScanStatus, buf);
                lv_obj_set_style_text_color(s_lblWifiScanStatus, lv_color_hex(0x10B981), 0);
            }
            lv_timer_delete(timer);
        }, 10, nullptr);
        (void)t;
    });
}

void RadioConfigView::wifi_ap_click_cb(lv_event_t* e) {
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx < s_scannedAps.size()) {
        const auto& ap = s_scannedAps[idx];
        char toast[64];
        snprintf(toast, sizeof(toast), "Red elegida: %s (Ch %u)", ap.ssid.c_str(), ap.channel);
        UIManager::showToast(toast);

        // Guardar SSID e intentar conectar si es abierta
        WiFiConfig wifiCfg;
        ConfigManager::getInstance().loadWiFi(wifiCfg);
        wifiCfg.ssid = ap.ssid;
        ConfigManager::getInstance().saveWiFi(wifiCfg);

        if (!ap.isEncrypted) {
            cbdos::network::connectWifi(ap.ssid.c_str(), "");
            UIManager::showToast("Conectando a red abierta...");
        }
    }
}

void RadioConfigView::refresh_wifi_list_ui() {
    if (!s_wifiListContainer || !lv_obj_is_valid(s_wifiListContainer)) return;
    lv_obj_clean(s_wifiListContainer);

    if (s_scannedAps.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_wifiListContainer);
        lv_label_set_text(emptyLbl, "No se detectaron redes Wi-Fi cercanas.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
        return;
    }

    for (size_t i = 0; i < s_scannedAps.size(); i++) {
        const auto& ap = s_scannedAps[i];

        lv_obj_t* card = lv_button_create(s_wifiListContainer);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 48);
        DefaultTheme::applyButton(card, 10);
        lv_obj_set_user_data(card, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(card, wifi_ap_click_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(card, 10, 0);

        // Nombre SSID y canal
        lv_obj_t* nameCol = lv_obj_create(card);
        lv_obj_set_size(nameCol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(nameCol, 0, 0);
        lv_obj_set_style_border_width(nameCol, 0, 0);
        lv_obj_set_style_pad_all(nameCol, 0, 0);
        lv_obj_set_flex_flow(nameCol, LV_FLEX_FLOW_COLUMN);
        DefaultTheme::disableScroll(nameCol);

        lv_obj_t* lblSsid = lv_label_create(nameCol);
        lv_label_set_text(lblSsid, ap.ssid.empty() ? "(Oculta)" : ap.ssid.c_str());
        lv_obj_set_style_text_color(lblSsid, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblSsid, &lv_font_montserrat_14, 0);

        lv_obj_t* lblCh = lv_label_create(nameCol);
        char chBuf[32];
        snprintf(chBuf, sizeof(chBuf), "Canal %u %s", ap.channel, ap.isEncrypted ? "[Cifrado]" : "[Abierto]");
        lv_label_set_text(lblCh, chBuf);
        lv_obj_set_style_text_color(lblCh, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblCh, &lv_font_montserrat_12, 0);

        // RSSI Badge
        lv_obj_t* lblRssi = lv_label_create(card);
        char rssiBuf[32];
        snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", ap.rssi);
        lv_label_set_text(lblRssi, rssiBuf);
        lv_obj_set_style_text_font(lblRssi, &lv_font_montserrat_12, 0);

        if (ap.rssi > -65) {
            lv_obj_set_style_text_color(lblRssi, lv_color_hex(0x10B981), 0);
        } else if (ap.rssi > -80) {
            lv_obj_set_style_text_color(lblRssi, lv_color_hex(0xF59E0B), 0);
        } else {
            lv_obj_set_style_text_color(lblRssi, lv_color_hex(0xEF4444), 0);
        }
    }
}

// ─── ESP-NOW / LR Channel Sweep & UI ───
void RadioConfigView::sweep_btn_cb(lv_event_t* e) {
    (void)e;
    if (s_lblSweepStatus && lv_obj_is_valid(s_lblSweepStatus)) {
        lv_label_set_text(s_lblSweepStatus, "Iniciando barrido multicanal (1..13)...");
    }
    if (s_barSweepProgress && lv_obj_is_valid(s_barSweepProgress)) {
        lv_bar_set_value(s_barSweepProgress, 0, LV_ANIM_ON);
    }

    cbdos::radio::startChannelSweep([](uint8_t curCh, uint8_t totalCh, const std::vector<cbdos::radio::DiscoveredNode>& nodes, bool finished) {
        s_sweptNodes = nodes;
        lv_timer_t* t = lv_timer_create([](lv_timer_t* timer) {
            uint32_t progress = (uint32_t)(uintptr_t)lv_timer_get_user_data(timer);
            if (s_barSweepProgress && lv_obj_is_valid(s_barSweepProgress)) {
                lv_bar_set_value(s_barSweepProgress, progress, LV_ANIM_OFF);
            }
            refresh_sweep_list_ui();

            if (s_lblSweepStatus && lv_obj_is_valid(s_lblSweepStatus)) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Barrido: %u nodos detectados", (unsigned int)s_sweptNodes.size());
                lv_label_set_text(s_lblSweepStatus, buf);
            }
            lv_timer_delete(timer);
        }, 10, (void*)(uintptr_t)((curCh * 100) / totalCh));
        (void)t;
    });
}

void RadioConfigView::tune_node_channel_cb(lv_event_t* e) {
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx < s_sweptNodes.size()) {
        const auto& node = s_sweptNodes[idx];
        s_cfg.channel = node.channel;
        cbdos::radio::setChannel(node.channel);

        if (s_sliderChannel && lv_obj_is_valid(s_sliderChannel)) {
            lv_slider_set_value(s_sliderChannel, node.channel, LV_ANIM_ON);
        }
        if (s_lblChannelVal && lv_obj_is_valid(s_lblChannelVal)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Canal RF: %u", node.channel);
            lv_label_set_text(s_lblChannelVal, buf);
        }

        char toast[64];
        snprintf(toast, sizeof(toast), "Sintonizado a: %s (Canal %u)", node.name, node.channel);
        UIManager::showToast(toast);
    }
}

void RadioConfigView::refresh_sweep_list_ui() {
    if (!s_sweepListContainer || !lv_obj_is_valid(s_sweepListContainer)) return;
    lv_obj_clean(s_sweepListContainer);

    if (s_sweptNodes.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_sweepListContainer);
        lv_label_set_text(emptyLbl, "No se han detectado nodos en el barrido.\nToca 'Iniciar Barrido' para escanear el espectro.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
        return;
    }

    for (size_t i = 0; i < s_sweptNodes.size(); i++) {
        const auto& n = s_sweptNodes[i];

        lv_obj_t* card = lv_obj_create(s_sweepListContainer);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(card, 10);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(card, 8, 0);
        DefaultTheme::disableScroll(card);

        lv_obj_t* infoCol = lv_obj_create(card);
        lv_obj_set_size(infoCol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(infoCol, 0, 0);
        lv_obj_set_style_border_width(infoCol, 0, 0);
        lv_obj_set_style_pad_all(infoCol, 0, 0);
        lv_obj_set_flex_flow(infoCol, LV_FLEX_FLOW_COLUMN);
        DefaultTheme::disableScroll(infoCol);

        lv_obj_t* lblName = lv_label_create(infoCol);
        char nameBuf[48];
        snprintf(nameBuf, sizeof(nameBuf), "%s", n.name[0] ? n.name : "Nodo Desconocido");
        lv_label_set_text(lblName, nameBuf);
        lv_obj_set_style_text_color(lblName, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_set_style_text_font(lblName, &lv_font_montserrat_14, 0);

        lv_obj_t* lblSub = lv_label_create(infoCol);
        char subBuf[48];
        snprintf(subBuf, sizeof(subBuf), "Canal %u | ID: 0x%04X | %d dBm", n.channel, n.short_id, n.rssi);
        lv_label_set_text(lblSub, subBuf);
        lv_obj_set_style_text_color(lblSub, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblSub, &lv_font_montserrat_12, 0);

        // Botón Sintonizar
        lv_obj_t* btnTune = lv_button_create(card);
        lv_obj_set_size(btnTune, 90, 32);
        DefaultTheme::applyButton(btnTune, 8);
        lv_obj_set_style_bg_color(btnTune, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_add_event_cb(btnTune, tune_node_channel_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* lblTune = lv_label_create(btnTune);
        lv_label_set_text(lblTune, "Sintonizar");
        lv_obj_set_style_text_font(lblTune, &lv_font_montserrat_12, 0);
        lv_obj_center(lblTune);
    }
}

void RadioConfigView::save_radio_config_cb(lv_event_t* e) {
    (void)e;
    ConfigManager::getInstance().saveRadio(s_cfg);
    cbdos::radio::setRadioPower(s_cfg.enabled);
    cbdos::radio::setMode(s_cfg.mode);
    cbdos::radio::setChannel(s_cfg.channel);
    cbdos::radio::setTxPower(s_cfg.txPower);
    UIManager::showToast("Configuración de Radio Guardada");
    UIManager::getInstance().popView();
}

bool RadioConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    ConfigManager::getInstance().loadRadio(s_cfg);

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 24, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // ─── TARJETA 1: Interruptor Maestro de Radio ───
    lv_obj_t* masterCard = lv_obj_create(m_container);
    lv_obj_set_width(masterCard, lv_pct(100));
    lv_obj_set_height(masterCard, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(masterCard, 12);
    lv_obj_set_flex_flow(masterCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(masterCard, 10, 0);
    lv_obj_set_style_pad_row(masterCard, 6, 0);

    lv_obj_t* rowMaster = lv_obj_create(masterCard);
    lv_obj_set_width(rowMaster, lv_pct(100));
    lv_obj_set_height(rowMaster, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowMaster, 0, 0);
    lv_obj_set_style_border_width(rowMaster, 0, 0);
    lv_obj_set_style_pad_all(rowMaster, 0, 0);
    lv_obj_set_flex_flow(rowMaster, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowMaster, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(rowMaster);

    lv_obj_t* lblRadioTitle = lv_label_create(rowMaster);
    lv_label_set_text(lblRadioTitle, "Alimentación de Radio");
    lv_obj_set_style_text_color(lblRadioTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblRadioTitle, &lv_font_montserrat_14, 0);

    s_swPower = lv_switch_create(rowMaster);
    if (s_cfg.enabled) {
        lv_obj_add_state(s_swPower, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(s_swPower, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(s_swPower, radio_power_sw_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    s_lblPowerStatus = lv_label_create(masterCard);
    lv_label_set_text(s_lblPowerStatus, s_cfg.enabled ? "Encendida" : "Apagada");
    lv_obj_set_style_text_color(s_lblPowerStatus, s_cfg.enabled ? lv_color_hex(0x10B981) : lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(s_lblPowerStatus, &lv_font_montserrat_12, 0);

    // ─── TARJETA 2: Modo de Operación y Canal RF ───
    lv_obj_t* modeCard = lv_obj_create(m_container);
    lv_obj_set_width(modeCard, lv_pct(100));
    lv_obj_set_height(modeCard, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modeCard, 12);
    lv_obj_set_flex_flow(modeCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modeCard, 10, 0);
    lv_obj_set_style_pad_row(modeCard, 8, 0);

    lv_obj_t* lblModeTitle = lv_label_create(modeCard);
    lv_label_set_text(lblModeTitle, "Modo de Operación");
    lv_obj_set_style_text_color(lblModeTitle, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(lblModeTitle, &lv_font_montserrat_12, 0);

    s_ddMode = lv_dropdown_create(modeCard);
    lv_dropdown_set_options(s_ddMode, 
        "Wi-Fi (Estación TCP/IP)\n"
        "ESP-NOW Normal (1-2 Mbps)\n"
        "ESP-NOW Long Range (LR @ 250 kbps)\n"
        "Híbrido (Wi-Fi + ESP-NOW)"
    );
    lv_obj_set_width(s_ddMode, lv_pct(100));
    lv_obj_set_style_radius(s_ddMode, 8, 0);
    lv_obj_set_style_bg_color(s_ddMode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(s_ddMode, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_ddMode, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(s_ddMode, DefaultTheme::getPrimaryAccent(), 0);

    uint16_t selIdx = 1;
    if (s_cfg.mode == cbdos::radio::RadioMode::WifiSta) selIdx = 0;
    else if (s_cfg.mode == cbdos::radio::RadioMode::EspNow) selIdx = 1;
    else if (s_cfg.mode == cbdos::radio::RadioMode::EspNowLR) selIdx = 2;
    else if (s_cfg.mode == cbdos::radio::RadioMode::Hybrid) selIdx = 3;
    lv_dropdown_set_selected(s_ddMode, selIdx);
    lv_obj_add_event_cb(s_ddMode, mode_select_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Canal RF Slider
    s_lblChannelVal = lv_label_create(modeCard);
    char chTxt[32];
    snprintf(chTxt, sizeof(chTxt), "Canal RF: %u", s_cfg.channel);
    lv_label_set_text(s_lblChannelVal, chTxt);
    lv_obj_set_style_text_color(s_lblChannelVal, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(s_lblChannelVal, &lv_font_montserrat_12, 0);

    s_sliderChannel = lv_slider_create(modeCard);
    lv_obj_set_width(s_sliderChannel, lv_pct(100));
    lv_obj_set_height(s_sliderChannel, 12);
    lv_slider_set_range(s_sliderChannel, 1, 13);
    lv_slider_set_value(s_sliderChannel, s_cfg.channel, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_sliderChannel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(s_sliderChannel, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_sliderChannel, DefaultTheme::getPrimaryAccent(), LV_PART_KNOB);
    lv_obj_add_event_cb(s_sliderChannel, channel_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Potencia TX Slider
    s_lblTxVal = lv_label_create(modeCard);
    char txTxt[32];
    snprintf(txTxt, sizeof(txTxt), "Potencia TX: +%d dBm", s_cfg.txPower);
    lv_label_set_text(s_lblTxVal, txTxt);
    lv_obj_set_style_text_color(s_lblTxVal, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(s_lblTxVal, &lv_font_montserrat_12, 0);

    s_sliderTx = lv_slider_create(modeCard);
    lv_obj_set_width(s_sliderTx, lv_pct(100));
    lv_obj_set_height(s_sliderTx, 12);
    lv_slider_set_range(s_sliderTx, 2, 20);
    lv_slider_set_value(s_sliderTx, s_cfg.txPower, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_sliderTx, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(s_sliderTx, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_sliderTx, DefaultTheme::getPrimaryAccent(), LV_PART_KNOB);
    lv_obj_add_event_cb(s_sliderTx, txpower_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // ─── TARJETA 3: Sección Wi-Fi (Escaneo Real) ───
    s_boxWifi = lv_obj_create(m_container);
    lv_obj_set_width(s_boxWifi, lv_pct(100));
    lv_obj_set_height(s_boxWifi, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(s_boxWifi, 12);
    lv_obj_set_flex_flow(s_boxWifi, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_boxWifi, 10, 0);
    lv_obj_set_style_pad_row(s_boxWifi, 8, 0);

    s_btnScanWifi = lv_button_create(s_boxWifi);
    lv_obj_set_width(s_btnScanWifi, lv_pct(100));
    lv_obj_set_height(s_btnScanWifi, 38);
    DefaultTheme::applyButton(s_btnScanWifi, 8);
    lv_obj_set_style_bg_color(s_btnScanWifi, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(s_btnScanWifi, wifi_scan_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lblScanWifi = lv_label_create(s_btnScanWifi);
    lv_label_set_text(lblScanWifi, "Escanear Redes Wi-Fi");
    lv_obj_set_style_text_font(lblScanWifi, &lv_font_montserrat_12, 0);
    lv_obj_center(lblScanWifi);

    s_lblWifiScanStatus = lv_label_create(s_boxWifi);
    lv_label_set_text(s_lblWifiScanStatus, "");
    lv_obj_set_style_text_color(s_lblWifiScanStatus, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(s_lblWifiScanStatus, &lv_font_montserrat_12, 0);

    s_wifiListContainer = lv_obj_create(s_boxWifi);
    lv_obj_set_width(s_wifiListContainer, lv_pct(100));
    lv_obj_set_height(s_wifiListContainer, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_wifiListContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_wifiListContainer, 0, 0);
    lv_obj_set_style_pad_row(s_wifiListContainer, 6, 0);
    lv_obj_set_style_bg_opa(s_wifiListContainer, 0, 0);
    lv_obj_set_style_border_width(s_wifiListContainer, 0, 0);

    // ─── TARJETA 4: Sección ESP-NOW / LR (Barrido Multicanal) ───
    s_boxMesh = lv_obj_create(m_container);
    lv_obj_set_width(s_boxMesh, lv_pct(100));
    lv_obj_set_height(s_boxMesh, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(s_boxMesh, 12);
    lv_obj_set_flex_flow(s_boxMesh, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_boxMesh, 10, 0);
    lv_obj_set_style_pad_row(s_boxMesh, 8, 0);

    s_btnSweep = lv_button_create(s_boxMesh);
    lv_obj_set_width(s_btnSweep, lv_pct(100));
    lv_obj_set_height(s_btnSweep, 38);
    DefaultTheme::applyButton(s_btnSweep, 8);
    lv_obj_set_style_bg_color(s_btnSweep, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_add_event_cb(s_btnSweep, sweep_btn_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lblSweep = lv_label_create(s_btnSweep);
    lv_label_set_text(lblSweep, "Barrido Multicanal (Ch 1..13)");
    lv_obj_set_style_text_color(lblSweep, lv_color_black(), 0);
    lv_obj_set_style_text_font(lblSweep, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSweep);

    s_barSweepProgress = lv_bar_create(s_boxMesh);
    lv_obj_set_width(s_barSweepProgress, lv_pct(100));
    lv_obj_set_height(s_barSweepProgress, 8);
    lv_bar_set_range(s_barSweepProgress, 0, 100);
    lv_bar_set_value(s_barSweepProgress, 0, LV_ANIM_OFF);

    s_lblSweepStatus = lv_label_create(s_boxMesh);
    lv_label_set_text(s_lblSweepStatus, "");
    lv_obj_set_style_text_color(s_lblSweepStatus, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(s_lblSweepStatus, &lv_font_montserrat_12, 0);

    s_sweepListContainer = lv_obj_create(s_boxMesh);
    lv_obj_set_width(s_sweepListContainer, lv_pct(100));
    lv_obj_set_height(s_sweepListContainer, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_sweepListContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_sweepListContainer, 0, 0);
    lv_obj_set_style_pad_row(s_sweepListContainer, 6, 0);
    lv_obj_set_style_bg_opa(s_sweepListContainer, 0, 0);
    lv_obj_set_style_border_width(s_sweepListContainer, 0, 0);

    // ─── BOTÓN GUARDAR CONFIGURACIÓN ───
    lv_obj_t* btnSave = lv_button_create(m_container);
    lv_obj_set_width(btnSave, lv_pct(100));
    lv_obj_set_height(btnSave, 44);
    DefaultTheme::applyButton(btnSave, 10);
    lv_obj_set_style_bg_color(btnSave, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(btnSave, save_radio_config_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lblSave = lv_label_create(btnSave);
    lv_label_set_text(lblSave, "Guardar Preferencias de Radio");
    lv_obj_set_style_text_font(lblSave, &lv_font_montserrat_14, 0);
    lv_obj_center(lblSave);

    update_mode_visibility();

    return true;
}

} // namespace ui
} // namespace cbdos
