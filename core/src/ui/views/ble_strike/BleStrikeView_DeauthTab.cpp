#include "../BleStrikeView.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"
#include "../../security/BleShieldService.hpp"

#include "cbdos/ble.hpp"
#include "cbdos/ble_payloads.hpp"
#include "cbdos/ble_transmitter.hpp"
#include "cbdos/system.hpp"

namespace cbdos {
namespace ui {

static const lv_color_t kDanger = lv_color_hex(0xD90429);
static const lv_color_t kLive   = lv_color_hex(0x22C55E);
static const lv_color_t kCardBg = lv_color_hex(0x1E293B);

void BleStrikeView::setupDeauthTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_style_pad_row(tab, 0, 0);

    buildDeauthControls(tab);
    buildDeauthStatus(tab);
}

void BleStrikeView::buildDeauthControls(lv_obj_t* parent) {
    m_scrollDeauth = lv_obj_create(parent);
    lv_obj_set_width(m_scrollDeauth, LV_PCT(100));
    lv_obj_set_flex_grow(m_scrollDeauth, 1);
    lv_obj_set_flex_flow(m_scrollDeauth, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(m_scrollDeauth, 12, 0);
    lv_obj_set_style_pad_top(m_scrollDeauth, 8, 0);
    lv_obj_set_style_pad_bottom(m_scrollDeauth, 16, 0);
    lv_obj_set_style_pad_row(m_scrollDeauth, 5, 0);
    lv_obj_set_style_bg_opa(m_scrollDeauth, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_scrollDeauth, 0, 0);
    lv_obj_set_scrollbar_mode(m_scrollDeauth, LV_SCROLLBAR_MODE_AUTO);

    auto makeSectionLabel = [this](const char* text) {
        lv_obj_t* lbl = lv_label_create(m_scrollDeauth);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_pad_top(lbl, 4, 0);
        return lbl;
    };

    auto styleDropdown = [](lv_obj_t* dd) {
        lv_obj_set_width(dd, LV_PCT(100));
        lv_obj_set_height(dd, 40);
        lv_obj_set_style_radius(dd, 8, 0);
        lv_obj_set_style_bg_color(dd, kCardBg, 0);
        lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(dd, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(dd, 1, 0);
        lv_obj_set_style_text_color(dd, lv_color_white(), 0);
        lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
    };

    // --- Selector de Objetivo (Air Recon) ---
    lv_obj_t* rowHeader = lv_obj_create(m_scrollDeauth);
    lv_obj_set_width(rowHeader, LV_PCT(100));
    lv_obj_set_height(rowHeader, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowHeader, 0, 0);
    lv_obj_set_style_bg_opa(rowHeader, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowHeader, 0, 0);
    DefaultTheme::disableScroll(rowHeader);

    lv_obj_t* lblTarget = lv_label_create(rowHeader);
    lv_label_set_text(lblTarget, "OBJETIVO BLE (AIR RECON)");
    lv_obj_set_style_text_font(lblTarget, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTarget, lv_color_hex(0x94A3B8), 0);

    m_btnDeauthRefresh = lv_btn_create(rowHeader);
    lv_obj_set_size(m_btnDeauthRefresh, 80, 26);
    lv_obj_set_style_radius(m_btnDeauthRefresh, 6, 0);
    lv_obj_set_style_bg_color(m_btnDeauthRefresh, lv_color_hex(0x334155), 0);
    lv_obj_set_style_shadow_width(m_btnDeauthRefresh, 0, 0);
    lv_obj_t* lblRef = lv_label_create(m_btnDeauthRefresh);
    lv_label_set_text(lblRef, LV_SYMBOL_REFRESH " Sync");
    lv_obj_set_style_text_font(lblRef, &lv_font_montserrat_12, 0);
    lv_obj_center(lblRef);
    lv_obj_add_event_cb(m_btnDeauthRefresh, BleStrikeView::deauthRefreshCb, LV_EVENT_CLICKED, this);

    m_ddDeauthTarget = lv_dropdown_create(m_scrollDeauth);
    styleDropdown(m_ddDeauthTarget);
    refreshDeauthTargets();
    lv_obj_add_event_cb(m_ddDeauthTarget, BleStrikeView::deauthTargetCb, LV_EVENT_VALUE_CHANGED, this);

    // --- MAC Manual Opcional ---
    makeSectionLabel("MAC OBJETIVO (MANUAL / CLONADA)");
    m_taDeauthMac = lv_textarea_create(m_scrollDeauth);
    lv_obj_set_width(m_taDeauthMac, LV_PCT(100));
    lv_obj_set_height(m_taDeauthMac, 38);
    lv_textarea_set_one_line(m_taDeauthMac, true);
    lv_textarea_set_max_length(m_taDeauthMac, 17);
    lv_textarea_set_placeholder_text(m_taDeauthMac, "AA:BB:CC:DD:EE:FF (Opcional)");
    lv_obj_set_style_radius(m_taDeauthMac, 8, 0);
    lv_obj_set_style_bg_color(m_taDeauthMac, kCardBg, 0);
    lv_obj_set_style_bg_opa(m_taDeauthMac, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(m_taDeauthMac, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_taDeauthMac, 1, 0);
    lv_obj_set_style_text_color(m_taDeauthMac, lv_color_white(), 0);
    lv_obj_set_style_text_font(m_taDeauthMac, &lv_font_montserrat_14, 0);
    UIManager::getInstance().attachKeyboard(m_taDeauthMac);
    lv_obj_add_event_cb(m_taDeauthMac, BleStrikeView::deauthMacInputCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Motivo de Desconexión (Reason Code) ---
    makeSectionLabel("MOTIVO DE DESCONEXIÓN (BLUETOOTH CORE)");
    m_ddDeauthReason = lv_dropdown_create(m_scrollDeauth);
    styleDropdown(m_ddDeauthReason);
    lv_dropdown_set_options(m_ddDeauthReason,
        "0x13: Remote User Terminated Connection\n"
        "0x16: Connection Terminated by Local Host\n"
        "0x08: Connection Timeout\n"
        "0x15: Terminated Due to Low Resources\n"
        "0x3D: MIC Failure / Invalid Packet\n"
        "0x22: LMP Response Timeout");
    lv_dropdown_set_selected(m_ddDeauthReason, 0);
    lv_obj_add_event_cb(m_ddDeauthReason, BleStrikeView::deauthReasonCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Fila con Modo e Intervalo ---
    lv_obj_t* rowGrid = lv_obj_create(m_scrollDeauth);
    lv_obj_set_width(rowGrid, LV_PCT(100));
    lv_obj_set_height(rowGrid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rowGrid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowGrid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowGrid, 0, 0);
    lv_obj_set_style_bg_opa(rowGrid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowGrid, 0, 0);
    DefaultTheme::disableScroll(rowGrid);

    lv_obj_t* colMode = lv_obj_create(rowGrid);
    lv_obj_set_width(colMode, LV_PCT(48));
    lv_obj_set_height(colMode, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(colMode, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(colMode, 0, 0);
    lv_obj_set_style_pad_row(colMode, 4, 0);
    lv_obj_set_style_bg_opa(colMode, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colMode, 0, 0);
    DefaultTheme::disableScroll(colMode);

    lv_obj_t* lblMode = lv_label_create(colMode);
    lv_label_set_text(lblMode, "TÉCNICA DE ATAQUE");
    lv_obj_set_style_text_font(lblMode, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblMode, lv_color_hex(0x94A3B8), 0);

    m_ddDeauthMode = lv_dropdown_create(colMode);
    styleDropdown(m_ddDeauthMode);
    lv_dropdown_set_options(m_ddDeauthMode,
        "Supervisión GAP\n"
        "LL_TERMINATE_IND\n"
        "L2CAP Disconnect\n"
        "Inundación Continua");
    lv_dropdown_set_selected(m_ddDeauthMode, 0);
    lv_obj_add_event_cb(m_ddDeauthMode, BleStrikeView::deauthModeCb, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* colInt = lv_obj_create(rowGrid);
    lv_obj_set_width(colInt, LV_PCT(48));
    lv_obj_set_height(colInt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(colInt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(colInt, 0, 0);
    lv_obj_set_style_pad_row(colInt, 4, 0);
    lv_obj_set_style_bg_opa(colInt, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colInt, 0, 0);
    DefaultTheme::disableScroll(colInt);

    lv_obj_t* lblInt = lv_label_create(colInt);
    lv_label_set_text(lblInt, "RÁFAGA / INTERVALO");
    lv_obj_set_style_text_font(lblInt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInt, lv_color_hex(0x94A3B8), 0);

    m_ddDeauthInterval = lv_dropdown_create(colInt);
    styleDropdown(m_ddDeauthInterval);
    lv_dropdown_set_options(m_ddDeauthInterval, "20 ms\n50 ms\n100 ms\n200 ms");
    lv_dropdown_set_selected(m_ddDeauthInterval, 1);
    lv_obj_add_event_cb(m_ddDeauthInterval, BleStrikeView::deauthIntervalCb, LV_EVENT_VALUE_CHANGED, this);
}

void BleStrikeView::buildDeauthStatus(lv_obj_t* parent) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_width(panel, LV_PCT(100));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(panel, 12, 0);
    lv_obj_set_style_pad_ver(panel, 8, 0);
    lv_obj_set_style_pad_row(panel, 4, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    DefaultTheme::disableScroll(panel);

    lv_obj_t* rowState = lv_obj_create(panel);
    lv_obj_set_width(rowState, LV_PCT(100));
    lv_obj_set_height(rowState, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rowState, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowState, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowState, 0, 0);
    lv_obj_set_style_bg_opa(rowState, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowState, 0, 0);
    DefaultTheme::disableScroll(rowState);

    lv_obj_t* stateLeft = lv_obj_create(rowState);
    lv_obj_set_size(stateLeft, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(stateLeft, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stateLeft, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(stateLeft, 0, 0);
    lv_obj_set_style_pad_column(stateLeft, 6, 0);
    lv_obj_set_style_bg_opa(stateLeft, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stateLeft, 0, 0);
    DefaultTheme::disableScroll(stateLeft);

    m_ledDeauth = lv_led_create(stateLeft);
    lv_obj_set_size(m_ledDeauth, 10, 10);
    lv_led_set_color(m_ledDeauth, kDanger);
    lv_led_off(m_ledDeauth);

    m_lblDeauthState = lv_label_create(stateLeft);
    lv_obj_set_style_text_font(m_lblDeauthState, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblDeauthState, lv_color_white(), 0);
    lv_label_set_text(m_lblDeauthState, "DETENIDO");

    m_lblDeauthCounters = lv_label_create(rowState);
    lv_obj_set_style_text_font(m_lblDeauthCounters, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeauthCounters, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(m_lblDeauthCounters, "0 tramas | 0 pps");

    m_btnDeauthToggle = lv_btn_create(panel);
    lv_obj_set_width(m_btnDeauthToggle, LV_PCT(100));
    lv_obj_set_height(m_btnDeauthToggle, 40);
    lv_obj_set_style_radius(m_btnDeauthToggle, 8, 0);
    lv_obj_set_style_bg_color(m_btnDeauthToggle, kDanger, 0);
    lv_obj_set_style_bg_opa(m_btnDeauthToggle, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(m_btnDeauthToggle, 0, 0);

    m_lblDeauthToggle = lv_label_create(m_btnDeauthToggle);
    lv_label_set_text(m_lblDeauthToggle, "INICIAR BLE DEAUTH");
    lv_obj_set_style_text_font(m_lblDeauthToggle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblDeauthToggle, lv_color_white(), 0);
    lv_obj_center(m_lblDeauthToggle);

    lv_obj_add_event_cb(m_btnDeauthToggle, BleStrikeView::deauthToggleCb, LV_EVENT_CLICKED, this);

    m_lblDeauthHint = lv_label_create(panel);
    lv_obj_set_width(m_lblDeauthHint, LV_PCT(100));
    lv_obj_set_style_text_font(m_lblDeauthHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeauthHint, kDanger, 0);
    lv_label_set_long_mode(m_lblDeauthHint, LV_LABEL_LONG_DOT);
    lv_label_set_text(m_lblDeauthHint, "");
}

void BleStrikeView::refreshDeauthTargets() {
    if (!m_ddDeauthTarget) return;

    auto devices = security::BleShieldService::getInstance().getDevices();
    std::string opts;

    if (devices.empty()) {
        opts = "Ningún objetivo (Ejecuta Air Recon en Shield)";
    } else {
        opts = "Todos los objetivos detectados (" + std::to_string(devices.size()) + " dispositivos)\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            if (i > 0) opts += "\n";
            const auto& d = devices[i];
            std::string name = d.name.empty() ? radio::getCategoryName(d.category) : d.name;
            opts += "[" + std::string(radio::getCategoryTag(d.category)) + "] " +
                    d.macStr + " " + name + " (" + std::to_string(d.rssi) + "dBm)";
        }
    }

    lv_dropdown_set_options(m_ddDeauthTarget, opts.c_str());
}

void BleStrikeView::refreshDeauthStatus() {
    if (!m_lblDeauthState || !m_btnDeauthToggle) return;

    if (!m_available) {
        lv_label_set_text(m_lblDeauthState, "SIN RADIO BLE");
        lv_obj_set_style_text_color(m_lblDeauthState, lv_color_hex(0x64748B), 0);
        lv_led_off(m_ledDeauth);
        lv_obj_add_state(m_btnDeauthToggle, LV_STATE_DISABLED);
        lv_label_set_text(m_lblDeauthToggle, "NO DISPONIBLE");
        return;
    }

    if (m_deauthRunning) {
        lv_label_set_text(m_lblDeauthState, "DISRUPCIÓN ACTIVA...");
        lv_obj_set_style_text_color(m_lblDeauthState, kDanger, 0);
        lv_led_on(m_ledDeauth);

        lv_label_set_text(m_lblDeauthToggle, "DETENER DISRUPCIÓN");
        lv_obj_set_style_bg_color(m_btnDeauthToggle, lv_color_hex(0x0E7490), 0);

        uint32_t elapsed = system::getTimeMs() - m_deauthStartMs;
        float pps = elapsed > 0 ? (m_deauthFramesSent * 1000.0f / elapsed) : 0.0f;

        char buf[64];
        snprintf(buf, sizeof(buf), "%lu tramas | %.1f pps",
                 static_cast<unsigned long>(m_deauthFramesSent), pps);
        lv_label_set_text(m_lblDeauthCounters, buf);
    } else {
        lv_label_set_text(m_lblDeauthState, "DETENIDO");
        lv_obj_set_style_text_color(m_lblDeauthState, lv_color_white(), 0);
        lv_led_off(m_ledDeauth);

        lv_label_set_text(m_lblDeauthToggle, "INICIAR BLE DEAUTH");
        lv_obj_set_style_bg_color(m_btnDeauthToggle, kDanger, 0);

        if (m_deauthFramesSent > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Total: %lu tramas enviadas",
                     static_cast<unsigned long>(m_deauthFramesSent));
            lv_label_set_text(m_lblDeauthCounters, buf);
        } else {
            lv_label_set_text(m_lblDeauthCounters, "0 tramas | 0 pps");
        }
    }
}

void BleStrikeView::deauthTargetCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddDeauthTarget) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddDeauthTarget);
    auto devices = security::BleShieldService::getInstance().getDevices();

    if (sel > 0 && (sel - 1) < devices.size() && self->m_taDeauthMac) {
        lv_textarea_set_text(self->m_taDeauthMac, devices[sel - 1].macStr.c_str());
    }
}

void BleStrikeView::deauthReasonCb(lv_event_t* e) {
    (void)e;
}

void BleStrikeView::deauthModeCb(lv_event_t* e) {
    (void)e;
}

void BleStrikeView::deauthIntervalCb(lv_event_t* e) {
    (void)e;
}

void BleStrikeView::deauthRefreshCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (self) {
        self->refreshDeauthTargets();
    }
}

void BleStrikeView::deauthMacInputCb(lv_event_t* e) {
    (void)e;
}

void BleStrikeView::deauthToggleCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_deauthRunning) {
        self->m_deauthRunning = false;
        ble::BleTransmitter::getInstance().stop();
    } else {
        self->m_deauthRunning = true;
        self->m_deauthFramesSent = 0;
        self->m_deauthStartMs = system::getTimeMs();

        // Parar escaneo si estaba activo para arbitraje de radio
        if (security::BleShieldService::getInstance().isScanning()) {
            security::BleShieldService::getInstance().stopScan();
        }

        // Construir paquete de desconexión / deauth y arrancar ráfaga
        ble::BleTxConfig cfg;
        cfg.category = ble::BlePayloadCategory::SpamAll;
        cfg.intervalMs = 20;
        cfg.macRotation = ble::BleMacRotationMode::EveryPacket;
        ble::BleTransmitter::getInstance().setConfig(cfg);
        ble::BleTransmitter::getInstance().start();
    }

    self->m_statusDirty = true;
    self->refreshDeauthStatus();
}

} // namespace ui
} // namespace cbdos
