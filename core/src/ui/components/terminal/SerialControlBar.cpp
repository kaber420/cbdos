#include "SerialControlBar.hpp"
#include "../../themes/DefaultTheme.h"
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

static const uint32_t s_baudRates[] = {
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};

static const int s_txPins[] = {32, 38, 50, 52, 15};
static const int s_rxPins[] = {28, 37, 49, 51, 16};
static std::string s_lastSelectedPortId = "";

bool SerialControlBar::create(lv_obj_t* parent,
                              ConnectCallback onConnect,
                              ResetCallback onReset,
                              ActionCallback onHold,
                              ActionCallback onClear,
                              ActionCallback onSave,
                              LogCallback onLog) {
    if (!parent) return false;
    m_onConnect = onConnect;
    m_onReset = onReset;
    m_onHold = onHold;
    m_onClear = onClear;
    m_onSave = onSave;
    m_onLog = onLog;

    // 1. Fila Principal (Organizada en 2 sub-filas compactas para encajar perfectamente en 320px y 800px)
    m_toolbar = lv_obj_create(parent);
    lv_obj_set_size(m_toolbar, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(m_toolbar, 8);
    lv_obj_set_style_pad_all(m_toolbar, 4, 0);
    lv_obj_set_flex_flow(m_toolbar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_toolbar, 4, 0);
    DefaultTheme::disableScroll(m_toolbar);

    // Sub-fila 1: Parámetros de Enlace (Puerto + Baudios + Conectar)
    lv_obj_t* rowConn = lv_obj_create(m_toolbar);
    lv_obj_set_size(rowConn, LV_PCT(100), 30);
    lv_obj_set_style_bg_opa(rowConn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowConn, 0, 0);
    lv_obj_set_style_pad_all(rowConn, 0, 0);
    lv_obj_set_flex_flow(rowConn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowConn, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rowConn, 4, 0);
    DefaultTheme::disableScroll(rowConn);

    m_ddPort = lv_dropdown_create(rowConn);
    lv_obj_set_flex_grow(m_ddPort, 1);
    lv_obj_set_style_text_font(m_ddPort, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddPort, portChangedCb, LV_EVENT_VALUE_CHANGED, this);
    updatePortList();

    m_ddBaud = lv_dropdown_create(rowConn);
    lv_dropdown_set_options(m_ddBaud, "9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600");
    lv_dropdown_set_selected(m_ddBaud, 4); // 115200
    lv_obj_set_width(m_ddBaud, 76);
    lv_obj_set_style_text_font(m_ddBaud, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddBaud, baudChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_btnConnect = lv_button_create(rowConn);
    DefaultTheme::applyButton(m_btnConnect, 6);
    lv_obj_set_size(m_btnConnect, 36, 28);
    lv_obj_set_style_bg_color(m_btnConnect, lv_palette_main(LV_PALETTE_GREEN), 0);
    m_lblConnect = lv_label_create(m_btnConnect);
    lv_label_set_text(m_lblConnect, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(m_lblConnect, &lv_font_montserrat_12, 0);
    lv_obj_center(m_lblConnect);
    lv_obj_add_event_cb(m_btnConnect, connectBtnCb, LV_EVENT_CLICKED, this);

    // Sub-fila 2: Acciones Rápidas de Hardware (RST + DFU + Hold + Limpiar + SD)
    lv_obj_t* rowActions = lv_obj_create(m_toolbar);
    lv_obj_set_size(rowActions, LV_PCT(100), 28);
    lv_obj_set_style_bg_opa(rowActions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowActions, 0, 0);
    lv_obj_set_style_pad_all(rowActions, 0, 0);
    lv_obj_set_flex_flow(rowActions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowActions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rowActions, 4, 0);
    DefaultTheme::disableScroll(rowActions);

    m_btnReset = lv_button_create(rowActions);
    DefaultTheme::applyButton(m_btnReset, 6);
    lv_obj_set_size(m_btnReset, 52, 26);
    lv_obj_set_style_bg_color(m_btnReset, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_t* lblReset = lv_label_create(m_btnReset);
    lv_label_set_text(lblReset, "⚡ RST");
    lv_obj_set_style_text_font(lblReset, &lv_font_montserrat_12, 0);
    lv_obj_center(lblReset);
    lv_obj_add_event_cb(m_btnReset, resetBtnCb, LV_EVENT_CLICKED, this);

    m_btnDfu = lv_button_create(rowActions);
    DefaultTheme::applyButton(m_btnDfu, 6);
    lv_obj_set_size(m_btnDfu, 52, 26);
    lv_obj_set_style_bg_color(m_btnDfu, lv_palette_main(LV_PALETTE_DEEP_PURPLE), 0);
    lv_obj_t* lblDfu = lv_label_create(m_btnDfu);
    lv_label_set_text(lblDfu, "📥 DFU");
    lv_obj_set_style_text_font(lblDfu, &lv_font_montserrat_12, 0);
    lv_obj_center(lblDfu);
    lv_obj_add_event_cb(m_btnDfu, dfuBtnCb, LV_EVENT_CLICKED, this);

    m_btnHold = lv_button_create(rowActions);
    DefaultTheme::applyButton(m_btnHold, 6);
    lv_obj_set_size(m_btnHold, 36, 26);
    lv_obj_set_style_bg_color(m_btnHold, lv_palette_main(LV_PALETTE_GREY), 0);
    m_lblHold = lv_label_create(m_btnHold);
    lv_label_set_text(m_lblHold, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(m_lblHold, &lv_font_montserrat_12, 0);
    lv_obj_center(m_lblHold);
    lv_obj_add_event_cb(m_btnHold, holdBtnCb, LV_EVENT_CLICKED, this);

    m_btnClear = lv_button_create(rowActions);
    DefaultTheme::applyButton(m_btnClear, 6);
    lv_obj_set_size(m_btnClear, 32, 26);
    lv_obj_t* lblClear = lv_label_create(m_btnClear);
    lv_label_set_text(lblClear, LV_SYMBOL_TRASH);
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(m_btnClear, clearBtnCb, LV_EVENT_CLICKED, this);

    m_btnSave = lv_button_create(rowActions);
    DefaultTheme::applyButton(m_btnSave, 6);
    lv_obj_set_size(m_btnSave, 32, 26);
    lv_obj_t* lblSave = lv_label_create(m_btnSave);
    lv_label_set_text(lblSave, LV_SYMBOL_SD_CARD);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(m_btnSave, saveBtnCb, LV_EVENT_CLICKED, this);

    // 2. Fila Secundaria: Pines GPIO TX / RX
    m_pinToolbar = lv_obj_create(parent);
    lv_obj_set_size(m_pinToolbar, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(m_pinToolbar, 6);
    lv_obj_set_style_pad_all(m_pinToolbar, 2, 0);
    lv_obj_set_flex_flow(m_pinToolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_pinToolbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(m_pinToolbar, 6, 0);
    DefaultTheme::disableScroll(m_pinToolbar);

    lv_obj_t* lblPines = lv_label_create(m_pinToolbar);
    lv_label_set_text(lblPines, "Pines:");
    lv_obj_set_style_text_font(lblPines, &lv_font_montserrat_12, 0);

    m_ddTxPin = lv_dropdown_create(m_pinToolbar);
    lv_dropdown_set_options(m_ddTxPin, "TX: 32\nTX: 38\nTX: 50\nTX: 52\nTX: 15");
    lv_dropdown_set_selected(m_ddTxPin, 0);
    lv_obj_set_width(m_ddTxPin, 88);
    lv_obj_set_style_text_font(m_ddTxPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddTxPin, txPinChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_ddRxPin = lv_dropdown_create(m_pinToolbar);
    lv_dropdown_set_options(m_ddRxPin, "RX: 28\nRX: 37\nRX: 49\nRX: 51\nRX: 16");
    lv_dropdown_set_selected(m_ddRxPin, 0);
    lv_obj_set_width(m_ddRxPin, 88);
    lv_obj_set_style_text_font(m_ddRxPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddRxPin, rxPinChangedCb, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* btnJp1 = lv_button_create(m_pinToolbar);
    DefaultTheme::applyButton(btnJp1, 6);
    lv_obj_set_size(btnJp1, 64, 26);
    lv_obj_t* lblJp1 = lv_label_create(btnJp1);
    lv_label_set_text(lblJp1, "JP1 (32)");
    lv_obj_set_style_text_font(lblJp1, &lv_font_montserrat_12, 0);
    lv_obj_center(lblJp1);
    lv_obj_add_event_cb(btnJp1, presetJp1BtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnU0 = lv_button_create(m_pinToolbar);
    DefaultTheme::applyButton(btnU0, 6);
    lv_obj_set_size(btnU0, 68, 26);
    lv_obj_t* lblU0 = lv_label_create(btnU0);
    lv_label_set_text(lblU0, "MX (38)");
    lv_obj_set_style_text_font(lblU0, &lv_font_montserrat_12, 0);
    lv_obj_center(lblU0);
    lv_obj_add_event_cb(btnU0, presetUart0BtnCb, LV_EVENT_CLICKED, this);

    updateControlPinUi();
    return true;
}

void SerialControlBar::show(bool visible) {
    if (!m_toolbar) return;
    if (visible) {
        lv_obj_remove_flag(m_toolbar, LV_OBJ_FLAG_HIDDEN);
        updateControlPinUi();
    } else {
        lv_obj_add_flag(m_toolbar, LV_OBJ_FLAG_HIDDEN);
        if (m_pinToolbar) lv_obj_add_flag(m_pinToolbar, LV_OBJ_FLAG_HIDDEN);
    }
}

void SerialControlBar::updatePortList() {
    if (!m_ddPort) return;
    std::string targetId = m_selectedPortId.empty() ? s_lastSelectedPortId : m_selectedPortId;
    m_ports = cbdos::serial::getAvailablePorts();
    std::string portOptions = "";
    int selectIdx = 0;
    bool foundTarget = false;
    for (size_t i = 0; i < m_ports.size(); i++) {
        portOptions += m_ports[i].displayName;
        if (!targetId.empty() && m_ports[i].id == targetId) {
            selectIdx = (int)i;
            foundTarget = true;
        }
        if (i + 1 < m_ports.size()) portOptions += "\n";
    }
    if (!foundTarget) {
        for (size_t i = 0; i < m_ports.size(); i++) {
            if (m_ports[i].type == cbdos::serial::PortType::UsbCdcAcm || m_ports[i].id == "usb_otg") {
                selectIdx = (int)i;
                break;
            }
        }
    }
    if (portOptions.empty()) portOptions = "Sin Puertos";
    lv_dropdown_set_options(m_ddPort, portOptions.c_str());
    lv_dropdown_set_selected(m_ddPort, selectIdx);
    if (!m_ports.empty() && selectIdx < (int)m_ports.size()) {
        m_selectedPortId = m_ports[selectIdx].id;
        s_lastSelectedPortId = m_selectedPortId;
        m_currentControlPin = m_ports[selectIdx].controlPin;
        if (m_ports[selectIdx].defaultTx >= 0) m_currentTxPin = m_ports[selectIdx].defaultTx;
        if (m_ports[selectIdx].defaultRx >= 0) m_currentRxPin = m_ports[selectIdx].defaultRx;
        updateControlPinUi();
    }
}

void SerialControlBar::updateControlPinUi() {
    bool isUsb = (m_selectedPortId.rfind("usb", 0) == 0);
    if (m_pinToolbar) {
        if (isUsb) lv_obj_add_flag(m_pinToolbar, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(m_pinToolbar, LV_OBJ_FLAG_HIDDEN);
    }
}

void SerialControlBar::setConnected(bool connected) {
    m_isConnected = connected;
    if (m_btnConnect && m_lblConnect) {
        if (connected) {
            lv_obj_set_style_bg_color(m_btnConnect, lv_palette_main(LV_PALETTE_RED), 0);
            lv_label_set_text(m_lblConnect, LV_SYMBOL_STOP);
        } else {
            lv_obj_set_style_bg_color(m_btnConnect, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_label_set_text(m_lblConnect, LV_SYMBOL_PLAY);
        }
    }
}

void SerialControlBar::setHoldActive(bool active) {
    m_isHoldActive = active;
    if (m_btnHold && m_lblHold) {
        if (active) {
            lv_obj_set_style_bg_color(m_btnHold, lv_palette_main(LV_PALETTE_AMBER), 0);
            lv_label_set_text(m_lblHold, LV_SYMBOL_PLAY);
        } else {
            lv_obj_set_style_bg_color(m_btnHold, lv_palette_main(LV_PALETTE_GREY), 0);
            lv_label_set_text(m_lblHold, LV_SYMBOL_PAUSE);
        }
    }
}

cbdos::serial::SerialConfig SerialControlBar::getCurrentConfig() const {
    cbdos::serial::SerialConfig cfg;
    cfg.portId = m_selectedPortId;
    cfg.baudrate = m_currentBaud;
    cfg.txPin = m_currentTxPin;
    cfg.rxPin = m_currentRxPin;
    cfg.controlPin = m_currentControlPin;
    return cfg;
}

void SerialControlBar::connectBtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onConnect) {
        self->m_onConnect(!self->m_isConnected, self->getCurrentConfig());
    }
}

void SerialControlBar::portChangedCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddPort) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddPort);
    if (sel < self->m_ports.size()) {
        self->m_selectedPortId = self->m_ports[sel].id;
        s_lastSelectedPortId = self->m_selectedPortId;
        self->m_currentControlPin = self->m_ports[sel].controlPin;
        if (self->m_ports[sel].defaultTx >= 0) self->m_currentTxPin = self->m_ports[sel].defaultTx;
        if (self->m_ports[sel].defaultRx >= 0) self->m_currentRxPin = self->m_ports[sel].defaultRx;
        self->updateControlPinUi();
        if (self->m_isConnected && self->m_onConnect) {
            self->m_onConnect(true, self->getCurrentConfig());
        }
    }
}

void SerialControlBar::baudChangedCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddBaud) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddBaud);
    if (sel < sizeof(s_baudRates) / sizeof(s_baudRates[0])) {
        self->m_currentBaud = s_baudRates[sel];
        if (self->m_onLog) {
            char msg[64];
            snprintf(msg, sizeof(msg), "\n[SYS] Baudrate configurado: %u bps\n", (unsigned)self->m_currentBaud);
            self->m_onLog(msg);
        }
        if (self->m_isConnected && self->m_onConnect) {
            self->m_onConnect(true, self->getCurrentConfig());
        }
    }
}

void SerialControlBar::txPinChangedCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddTxPin) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddTxPin);
    if (sel < sizeof(s_txPins) / sizeof(s_txPins[0])) {
        self->m_currentTxPin = s_txPins[sel];
        if (self->m_onLog) {
            char msg[64];
            snprintf(msg, sizeof(msg), "\n[CONFIG] TX fijado en GPIO %d\n", self->m_currentTxPin);
            self->m_onLog(msg);
        }
        if (self->m_isConnected && self->m_onConnect) self->m_onConnect(true, self->getCurrentConfig());
    }
}

void SerialControlBar::rxPinChangedCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddRxPin) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddRxPin);
    if (sel < sizeof(s_rxPins) / sizeof(s_rxPins[0])) {
        self->m_currentRxPin = s_rxPins[sel];
        if (self->m_onLog) {
            char msg[64];
            snprintf(msg, sizeof(msg), "\n[CONFIG] RX fijado en GPIO %d\n", self->m_currentRxPin);
            self->m_onLog(msg);
        }
        if (self->m_isConnected && self->m_onConnect) self->m_onConnect(true, self->getCurrentConfig());
    }
}

void SerialControlBar::presetJp1BtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (!self) return;
    self->m_currentTxPin = 32;
    self->m_currentRxPin = 28;
    self->m_currentControlPin = 34;
    if (self->m_ddTxPin) lv_dropdown_set_selected(self->m_ddTxPin, 0);
    if (self->m_ddRxPin) lv_dropdown_set_selected(self->m_ddRxPin, 0);
    if (self->m_onLog) self->m_onLog("\n[PRESET] JP1 aplicado: TX=32, RX=28, RST=34\n");
    if (self->m_isConnected && self->m_onConnect) self->m_onConnect(true, self->getCurrentConfig());
}

void SerialControlBar::presetUart0BtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (!self) return;
    self->m_currentTxPin = 38;
    self->m_currentRxPin = 37;
    self->m_currentControlPin = -1;
    if (self->m_ddTxPin) lv_dropdown_set_selected(self->m_ddTxPin, 1);
    if (self->m_ddRxPin) lv_dropdown_set_selected(self->m_ddRxPin, 1);
    if (self->m_onLog) self->m_onLog("\n[PRESET] UART0 MX aplicado: TX=38, RX=37\n");
    if (self->m_isConnected && self->m_onConnect) self->m_onConnect(true, self->getCurrentConfig());
}

void SerialControlBar::resetBtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onReset) self->m_onReset(false);
}

void SerialControlBar::dfuBtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onReset) self->m_onReset(true);
}

void SerialControlBar::holdBtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onHold) self->m_onHold();
}

void SerialControlBar::clearBtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onClear) self->m_onClear();
}

void SerialControlBar::saveBtnCb(lv_event_t* e) {
    auto* self = static_cast<SerialControlBar*>(lv_event_get_user_data(e));
    if (self && self->m_onSave) self->m_onSave();
}

} // namespace ui
} // namespace cbdos
