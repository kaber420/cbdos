#include "../MeshCoreView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/meshcore/meshcore_store.hpp"
#include "cbdos/meshcore/mesh_emoji.hpp"
#include "../../assets/lottie_sample.h"
#include "../../assets/wink_star_assets.h"
#include "MeshCoreCommon.hpp"
#include <algorithm>
#include <map>
#include <vector>

namespace cbdos {
namespace ui {
using namespace meshcore_ui;

void MeshCoreView::buildRadioTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_row(tab, 8, 0);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);

    // Sección 1: enlace serial
    lv_obj_t* secConn = lv_obj_create(tab);
    lv_obj_set_size(secConn, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secConn, 8);
    lv_obj_set_flex_flow(secConn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secConn, 8, 0);
    lv_obj_set_style_pad_row(secConn, 6, 0);
    DefaultTheme::disableScroll(secConn);

    lv_obj_t* lblConnTitle = lv_label_create(secConn);
    lv_label_set_text(lblConnTitle, LV_SYMBOL_SETTINGS " Enlace Serial / Dongle");
    lv_obj_set_style_text_font(lblConnTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblConnTitle, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* rowPort = lv_obj_create(secConn);
    lv_obj_set_size(rowPort, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(rowPort, 0, 0);
    lv_obj_set_style_border_width(rowPort, 0, 0);
    lv_obj_set_flex_flow(rowPort, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowPort, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowPort, 0, 0);
    DefaultTheme::disableScroll(rowPort);

    m_ddPort = lv_dropdown_create(rowPort);
    lv_obj_set_size(m_ddPort, 140, 36);
    DefaultTheme::applySunkenCard(m_ddPort, 6);

    auto ports = cbdos::serial::getAvailablePorts();
    std::string portOpts = "jp1\nusb0\next_s3";
    m_portIds = {"jp1", "usb0", "ext_s3"};
    if (!ports.empty()) {
        portOpts.clear();
        m_portIds.clear();
        for (size_t p = 0; p < ports.size(); ++p) {
            if (p > 0) portOpts += "\n";
            portOpts += ports[p].id;
            m_portIds.push_back(ports[p].id);
        }
    }
    lv_dropdown_set_options(m_ddPort, portOpts.c_str());
    for (size_t i = 0; i < m_portIds.size(); ++i) {
        if (m_portIds[i] == m_selectedPort) {
            lv_dropdown_set_selected(m_ddPort, (uint16_t)i);
            break;
        }
    }
    lv_obj_add_event_cb(m_ddPort, portDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    m_btnConnect = lv_button_create(rowPort);
    lv_obj_set_size(m_btnConnect, 110, 36);
    DefaultTheme::applyButton(m_btnConnect, 6);
    lv_obj_t* lblConn = lv_label_create(m_btnConnect);
    lv_label_set_text(lblConn, "Reconectar");
    lv_obj_set_style_text_font(lblConn, &lv_font_montserrat_12, 0);
    lv_obj_center(lblConn);
    lv_obj_add_event_cb(m_btnConnect, connectBtnCb, LV_EVENT_CLICKED, this);

    m_lblConnStatus = lv_label_create(secConn);
    lv_label_set_text(m_lblConnStatus, "Estado: --");
    lv_obj_set_style_text_font(m_lblConnStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0x00E676), 0);

    // Sección 2: identidad y firmware
    lv_obj_t* secDev = lv_obj_create(tab);
    lv_obj_set_size(secDev, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secDev, 8);
    lv_obj_set_flex_flow(secDev, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secDev, 8, 0);
    lv_obj_set_style_pad_row(secDev, 6, 0);
    DefaultTheme::disableScroll(secDev);

    lv_obj_t* lblDevTitle = lv_label_create(secDev);
    lv_label_set_text(lblDevTitle, LV_SYMBOL_CHARGE " Dongle MeshCore");
    lv_obj_set_style_text_font(lblDevTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDevTitle, lv_color_hex(0x00E5FF), 0);

    m_lblDeviceDetails = lv_label_create(secDev);
    lv_label_set_text(m_lblDeviceDetails, "Nombre: --\nFirmware: --");
    lv_obj_set_style_text_font(m_lblDeviceDetails, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeviceDetails, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_width(m_lblDeviceDetails, LV_PCT(100));

    m_lblBattery = lv_label_create(secDev);
    lv_label_set_text(m_lblBattery, "Bateria: --");
    lv_obj_set_style_text_font(m_lblBattery, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblBattery, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_width(m_lblBattery, LV_PCT(100));

    // Alias del nodo (CLI: set name)
    lv_obj_t* rowAlias = lv_obj_create(secDev);
    lv_obj_set_size(rowAlias, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(rowAlias, 0, 0);
    lv_obj_set_style_border_width(rowAlias, 0, 0);
    lv_obj_set_flex_flow(rowAlias, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowAlias, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowAlias, 0, 0);
    DefaultTheme::disableScroll(rowAlias);

    m_taAlias = lv_textarea_create(rowAlias);
    lv_obj_set_flex_grow(m_taAlias, 1);
    lv_obj_set_height(m_taAlias, 32);
    DefaultTheme::applySunkenCard(m_taAlias, 6);
    lv_textarea_set_one_line(m_taAlias, true);
    lv_textarea_set_placeholder_text(m_taAlias, "Alias del nodo");
    lv_obj_set_style_text_font(m_taAlias, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taAlias);

    lv_obj_t* btnAlias = lv_button_create(rowAlias);
    lv_obj_set_size(btnAlias, 90, 32);
    DefaultTheme::applyButton(btnAlias, 4);
    lv_obj_t* lblA = lv_label_create(btnAlias);
    lv_label_set_text(lblA, "Aplicar");
    lv_obj_set_style_text_font(lblA, &lv_font_montserrat_12, 0);
    lv_obj_center(lblA);
    lv_obj_add_event_cb(btnAlias, aliasApplyBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* rowDevBtns = lv_obj_create(secDev);
    lv_obj_set_size(rowDevBtns, LV_PCT(100), 34);
    lv_obj_set_style_bg_opa(rowDevBtns, 0, 0);
    lv_obj_set_style_border_width(rowDevBtns, 0, 0);
    lv_obj_set_flex_flow(rowDevBtns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(rowDevBtns, 0, 0);
    lv_obj_set_style_pad_column(rowDevBtns, 6, 0);
    DefaultTheme::disableScroll(rowDevBtns);

    lv_obj_t* btnInfo = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnInfo, 90, 30);
    DefaultTheme::applyButton(btnInfo, 4);
    lv_obj_t* lblQ = lv_label_create(btnInfo);
    lv_label_set_text(lblQ, LV_SYMBOL_REFRESH " Info");
    lv_obj_set_style_text_font(lblQ, &lv_font_montserrat_12, 0);
    lv_obj_center(lblQ);
    lv_obj_add_event_cb(btnInfo, infoBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnBatt = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnBatt, 100, 30);
    DefaultTheme::applyButton(btnBatt, 4);
    lv_obj_t* lblB = lv_label_create(btnBatt);
    lv_label_set_text(lblB, LV_SYMBOL_CHARGE " Bateria");
    lv_obj_set_style_text_font(lblB, &lv_font_montserrat_12, 0);
    lv_obj_center(lblB);
    lv_obj_add_event_cb(btnBatt, batteryBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnPoll = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnPoll, 90, 30);
    DefaultTheme::applyButton(btnPoll, 4);
    lv_obj_t* lblPl = lv_label_create(btnPoll);
    lv_label_set_text(lblPl, LV_SYMBOL_DOWNLOAD " Poll");
    lv_obj_set_style_text_font(lblPl, &lv_font_montserrat_12, 0);
    lv_obj_center(lblPl);
    lv_obj_add_event_cb(btnPoll, pollBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnReboot = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnReboot, 100, 30);
    DefaultTheme::applyButton(btnReboot, 4);
    lv_obj_set_style_bg_color(btnReboot, lv_color_hex(0x6A1B1B), 0);
    lv_obj_t* lblR = lv_label_create(btnReboot);
    lv_label_set_text(lblR, LV_SYMBOL_POWER " Reboot");
    lv_obj_set_style_text_font(lblR, &lv_font_montserrat_12, 0);
    lv_obj_center(lblR);
    lv_obj_add_event_cb(btnReboot, rebootBtnCb, LV_EVENT_CLICKED, this);

    // Sección 3: parámetros de radio
    lv_obj_t* secRadio = lv_obj_create(tab);
    lv_obj_set_size(secRadio, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secRadio, 8);
    lv_obj_set_flex_flow(secRadio, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secRadio, 8, 0);
    lv_obj_set_style_pad_row(secRadio, 4, 0);
    DefaultTheme::disableScroll(secRadio);

    lv_obj_t* lblRadioTitle = lv_label_create(secRadio);
    lv_label_set_text(lblRadioTitle, LV_SYMBOL_WIFI " Radio LoRa (SELF_INFO)");
    lv_obj_set_style_text_font(lblRadioTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblRadioTitle, lv_color_hex(0x00E5FF), 0);

    m_lblRadioStats = lv_label_create(secRadio);
    lv_label_set_text(m_lblRadioStats, "Frecuencia: -- MHz\nSF: -- | BW: -- kHz | TX: -- dBm");
    lv_obj_set_style_text_font(m_lblRadioStats, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblRadioStats, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_width(m_lblRadioStats, LV_PCT(100));

    // Sección 4: parámetros de radio (se envían por CLI, aplican con reboot)
    lv_obj_t* secParams = lv_obj_create(tab);
    lv_obj_set_size(secParams, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secParams, 8);
    lv_obj_set_flex_flow(secParams, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secParams, 8, 0);
    lv_obj_set_style_pad_row(secParams, 6, 0);
    DefaultTheme::disableScroll(secParams);

    lv_obj_t* lblParamsTitle = lv_label_create(secParams);
    lv_label_set_text(lblParamsTitle, LV_SYMBOL_SETTINGS " Parametros de radio");
    lv_obj_set_style_text_font(lblParamsTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblParamsTitle, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* lblParamsHint = lv_label_create(secParams);
    lv_label_set_text(lblParamsHint, "Deben coincidir en toda tu malla. Se aplican con Reboot.");
    lv_obj_set_style_text_font(lblParamsHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblParamsHint, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_width(lblParamsHint, LV_PCT(100));
    lv_label_set_long_mode(lblParamsHint, LV_LABEL_LONG_WRAP);

    auto makeField = [&](const char* hint) -> lv_obj_t* {
        lv_obj_t* ta = lv_textarea_create(secParams);
        lv_obj_set_size(ta, LV_PCT(100), 34);
        DefaultTheme::applySunkenCard(ta, 6);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_placeholder_text(ta, hint);
        lv_obj_set_style_text_font(ta, &lv_font_montserrat_12, 0);
        UIManager::attachKeyboard(ta);
        return ta;
    };
    m_taFreq = makeField("Frecuencia MHz (ej. 910.525)");
    m_taBw = makeField("Ancho de banda kHz (ej. 62.5)");
    m_taTx = makeField("Potencia TX dBm (ej. 22)");

    lv_obj_t* rowSfCr = lv_obj_create(secParams);
    lv_obj_set_size(rowSfCr, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(rowSfCr, 0, 0);
    lv_obj_set_style_border_width(rowSfCr, 0, 0);
    lv_obj_set_flex_flow(rowSfCr, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(rowSfCr, 0, 0);
    lv_obj_set_style_pad_column(rowSfCr, 8, 0);
    DefaultTheme::disableScroll(rowSfCr);

    m_ddSf = lv_dropdown_create(rowSfCr);
    lv_obj_set_size(m_ddSf, 130, 34);
    DefaultTheme::applySunkenCard(m_ddSf, 6);
    lv_dropdown_set_options(m_ddSf, "SF 7\nSF 8\nSF 9\nSF 10\nSF 11\nSF 12");

    m_ddCr = lv_dropdown_create(rowSfCr);
    lv_obj_set_size(m_ddCr, 130, 34);
    DefaultTheme::applySunkenCard(m_ddCr, 6);
    lv_dropdown_set_options(m_ddCr, "CR 5\nCR 6\nCR 7\nCR 8");

    lv_obj_t* btnApply = makeButton(secParams, LV_SYMBOL_OK " Aplicar + Reboot", 220, 38, 0x1B5E20);
    lv_obj_add_event_cb(btnApply, radioApplyBtnCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::refreshRadioStatus() {
    auto& client = meshcore::MeshCoreClient::getInstance();

    if (m_lblConnStatus && lv_obj_is_valid(m_lblConnStatus)) {
        if (!client.isConnected()) {
            lv_label_set_text(m_lblConnStatus, "Estado: Desconectado");
            lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0xEF4444), 0);
        } else if (client.hasHandshake()) {
            std::string text = "Estado: Dongle MeshCore en " + client.getActivePort();
            lv_label_set_text(m_lblConnStatus, text.c_str());
            lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0x00E676), 0);
        } else {
            std::string text = "Estado: Puerto " + client.getActivePort() +
                               " abierto, sin respuesta del dongle";
            lv_label_set_text(m_lblConnStatus, text.c_str());
            lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0xFFB300), 0);
        }
    }

    if (m_lblDeviceDetails && lv_obj_is_valid(m_lblDeviceDetails)) {
        const auto& self = client.getSelfInfo();
        const auto& dev = client.getDeviceInfo();
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Nombre: %s\nModelo: %s | FW: %s (build %s)\nContactos: %u | Canales: %u",
            self.valid ? self.name.c_str() : "--",
            dev.valid ? dev.model.c_str() : "--",
            dev.valid ? dev.version.c_str() : "--",
            dev.valid ? dev.firmwareBuild.c_str() : "--",
            (unsigned)(dev.valid ? dev.maxContacts : 0),
            (unsigned)(dev.valid ? dev.maxChannels : 0));
        lv_label_set_text(m_lblDeviceDetails, buf);
    }

    if (m_lblBattery && lv_obj_is_valid(m_lblBattery)) {
        const auto& batt = client.getBattery();
        char buf[128];
        if (!batt.valid) {
            snprintf(buf, sizeof(buf), "Bateria: --");
        } else if (batt.hasStorage) {
            snprintf(buf, sizeof(buf), "Bateria: %u mV | Flash: %u / %u KB",
                     (unsigned)batt.voltageMv,
                     (unsigned)batt.usedStorageKb,
                     (unsigned)batt.totalStorageKb);
        } else {
            snprintf(buf, sizeof(buf), "Bateria: %u mV", (unsigned)batt.voltageMv);
        }
        if (!m_pendingError.empty()) {
            char withErr[192];
            snprintf(withErr, sizeof(withErr), "%s\nError: %s", buf, m_pendingError.c_str());
            lv_label_set_text(m_lblBattery, withErr);
        } else {
            lv_label_set_text(m_lblBattery, buf);
        }
    }

    if (m_lblRadioStats && lv_obj_is_valid(m_lblRadioStats)) {
        const auto& self = client.getSelfInfo();
        char rbuf[160];
        if (!self.valid) {
            snprintf(rbuf, sizeof(rbuf), "Frecuencia: -- MHz\nSF: -- | BW: -- kHz | TX: -- dBm");
        } else {
            snprintf(rbuf, sizeof(rbuf),
                "Frec: %.3f MHz | BW: %.0f kHz\nSF: %d | CR: %d | TX: %d/%d dBm",
                (double)self.frequencyHz / 1000000.0,
                (double)self.bandwidthHz / 1000.0,
                (int)self.spreadingFactor,
                (int)self.codingRate,
                (int)self.txPowerDbm,
                (int)self.maxTxPowerDbm);
        }
        lv_label_set_text(m_lblRadioStats, rbuf);

        // Prerrellenar el formulario con lo que reporta el dongle (sin
        // pisar lo que el usuario esté escribiendo).
        if (self.valid) {
            auto fillEmpty = [](lv_obj_t* ta, const char* v) {
                if (ta && lv_obj_is_valid(ta)) {
                    const char* cur = lv_textarea_get_text(ta);
                    if (!cur || strlen(cur) == 0) lv_textarea_set_text(ta, v);
                }
            };
            char f[16], b[16], t[16];
            snprintf(f, sizeof(f), "%s", trimNum((double)self.frequencyHz / 1000000.0, 3).c_str());
            snprintf(b, sizeof(b), "%s", trimNum((double)self.bandwidthHz / 1000.0, 2).c_str());
            snprintf(t, sizeof(t), "%d", (int)self.txPowerDbm);
            fillEmpty(m_taFreq, f);
            fillEmpty(m_taBw, b);
            fillEmpty(m_taTx, t);
            if (m_ddSf && lv_obj_is_valid(m_ddSf) && self.spreadingFactor >= 7 &&
                self.spreadingFactor <= 12) {
                lv_dropdown_set_selected(m_ddSf, (uint16_t)(self.spreadingFactor - 7));
            }
            if (m_ddCr && lv_obj_is_valid(m_ddCr) && self.codingRate >= 5 &&
                self.codingRate <= 8) {
                lv_dropdown_set_selected(m_ddCr, (uint16_t)(self.codingRate - 5));
            }
        }
    }
}

// ────────────────────────────────────────────────────────────────
// Acciones y manejadores de eventos
// ────────────────────────────────────────────────────────────────


void MeshCoreView::connectBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;

    auto& client = meshcore::MeshCoreClient::getInstance();
    client.disconnect();
    bool ok = client.connect(self->m_selectedPort, self->m_selectedBaud);
    if (ok) {
        saveLastPort(self->m_selectedPort);
        client.queryAllChannels();
        client.queryContacts();
        client.queryBattery();
        client.pollMessages();
        UIManager::showToast("Dongle conectado");
    } else {
        UIManager::showToast("Fallo al conectar puerto");
    }
    self->refreshRadioStatus();
}

void MeshCoreView::infoBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    client.sendAppStart();
    client.queryDeviceInfo();
    client.queryBattery();
    client.queryAllChannels();
    client.queryContacts();
}

void MeshCoreView::batteryBtnCb(lv_event_t* e) {
    (void)e;
    meshcore::MeshCoreClient::getInstance().queryBattery();
}

void MeshCoreView::pollBtnCb(lv_event_t* e) {
    (void)e;
    meshcore::MeshCoreClient::getInstance().pollMessages();
    UIManager::showToast("Drenando mensajes...");
}

void MeshCoreView::aliasApplyBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taAlias) return;
    const char* txt = lv_textarea_get_text(self->m_taAlias);
    if (!txt || strlen(txt) == 0) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (client.setName(txt)) {
        // Refrescar SELF_INFO para mostrar el nuevo alias.
        client.sendAppStart();
        UIManager::showToast("Alias enviado. Refrescando...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::rebootBtnCb(lv_event_t* e) {
    (void)e;
    if (meshcore::MeshCoreClient::getInstance().reboot()) {
        UIManager::showToast("Reiniciando dongle...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::portDropdownCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddPort) return;

    uint16_t sel = lv_dropdown_get_selected(self->m_ddPort);
    if (sel < self->m_portIds.size()) {
        self->m_selectedPort = self->m_portIds[sel];
    } else {
        char buf[32];
        lv_dropdown_get_selected_str(self->m_ddPort, buf, sizeof(buf));
        self->m_selectedPort = buf;
    }
}

void MeshCoreView::applyRadioParams() {
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (!client.isConnected()) {
        UIManager::showToast("Dongle desconectado.");
        return;
    }
    const char* fTxt = (m_taFreq && lv_obj_is_valid(m_taFreq)) ? lv_textarea_get_text(m_taFreq) : "";
    const char* bTxt = (m_taBw && lv_obj_is_valid(m_taBw)) ? lv_textarea_get_text(m_taBw) : "";
    const char* tTxt = (m_taTx && lv_obj_is_valid(m_taTx)) ? lv_textarea_get_text(m_taTx) : "";
    double freq = 0, bw = 0;
    long tx = tTxt ? atol(tTxt) : 0;
    uint8_t sf = (m_ddSf && lv_obj_is_valid(m_ddSf))
                     ? (uint8_t)(lv_dropdown_get_selected(m_ddSf) + 7)
                     : 7;
    uint8_t cr = (m_ddCr && lv_obj_is_valid(m_ddCr))
                     ? (uint8_t)(lv_dropdown_get_selected(m_ddCr) + 5)
                     : 5;
    std::string fStr = fTxt ? fTxt : "";
    std::string bStr = bTxt ? bTxt : "";
    if (!parseDec(fStr.c_str(), freq) || freq < 400.0 || freq > 2500.0 ||
        !parseDec(bStr.c_str(), bw) || bw < 7.0 || bw > 500.0 || tx < -9 || tx > 22) {
        UIManager::showToast("Valores invalidos. Revisa freq/BW/TX.");
        return;
    }
    // `set freq` persiste tras reboot (doc oficial); `set radio` fija el resto.
    if (!client.setFrequencyStr(trimNum(freq, 3))) {
        UIManager::showToast("Fallo al enviar frecuencia.");
        return;
    }
    client.setRadioStr(trimNum(freq, 3), trimNum(bw, 2), sf, cr);
    client.setTxPower((int)tx);
    client.reboot();
    UIManager::showToast("Aplicado. Dongle reiniciando...");
}

void MeshCoreView::radioApplyBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->applyRadioParams();
}

// ────────────────────────────────────────────────────────────────
// POC Emoji + Winks estilo Telegram (prueba con estrella + gato)
// En el aire viajan 5-6 B (":star:", ":cat:"); la animacion se
// reproduce en local desde flash (estrella) o SD (catmov.json).
// ────────────────────────────────────────────────────────────────


} // namespace ui
} // namespace cbdos
