#include "MeshConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/network.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

MeshConfigView* MeshConfigView::s_instance = nullptr;
lv_obj_t* MeshConfigView::s_towersListContainer = nullptr;
lv_obj_t* MeshConfigView::s_lblScanStatus = nullptr;
lv_obj_t* MeshConfigView::s_lblCurrentMode = nullptr;
lv_obj_t* MeshConfigView::s_lblMac = nullptr;
lv_obj_t* MeshConfigView::s_lblChannel = nullptr;
lv_obj_t* MeshConfigView::s_ddMode = nullptr;
lv_obj_t* MeshConfigView::s_btnScan = nullptr;

MeshConfigView::MeshConfigView()
    : BaseView("Red Malla y Torres") {
    s_instance = this;
}

MeshConfigView::~MeshConfigView() {
    if (s_instance == this) {
        cbdos::mesh::MeshEngine::getInstance().setDiscoveredTowersCallback(nullptr);
        s_instance = nullptr;
    }
}

void MeshConfigView::mode_select_cb(lv_event_t* e) {
    lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
    if (!dd) return;

    uint32_t sel = lv_dropdown_get_selected(dd);
    auto mode = static_cast<cbdos::mesh::RadioMode>(sel);
    cbdos::mesh::MeshEngine::getInstance().setRadioMode(mode);

    char toast[64];
    snprintf(toast, sizeof(toast), "Radio: %s", cbdos::mesh::MeshEngine::getInstance().getRadioModeName());
    UIManager::showToast(toast);

    if (s_lblCurrentMode && lv_obj_is_valid(s_lblCurrentMode)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Modo Activo: %s", cbdos::mesh::MeshEngine::getInstance().getRadioModeName());
        lv_label_set_text(s_lblCurrentMode, buf);
    }
}

void MeshConfigView::scan_btn_cb(lv_event_t* e) {
    (void)e;
    if (s_lblScanStatus && lv_obj_is_valid(s_lblScanStatus)) {
        lv_label_set_text(s_lblScanStatus, "📡 Emitiendo sondeo de radio en Canal 1...");
    }

    cbdos::mesh::MeshEngine::getInstance().sendTowerProbe();
    UIManager::showToast("Buscando torres y nodos...");

    // Refrescar lista después de breve intervalo
    lv_timer_t* t = lv_timer_create([](lv_timer_t* timer) {
        refresh_towers_ui();
        if (s_lblScanStatus && lv_obj_is_valid(s_lblScanStatus)) {
            size_t count = cbdos::mesh::MeshEngine::getInstance().getDiscoveredTowers().size();
            char buf[64];
            snprintf(buf, sizeof(buf), "Escaneo completado: %u torres encontradas", (unsigned int)count);
            lv_label_set_text(s_lblScanStatus, buf);
        }
        lv_timer_delete(timer);
    }, 1200, nullptr);
    (void)t;
}

void MeshConfigView::connect_tower_cb(lv_event_t* e) {
    uint32_t tower_idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    auto& mesh = cbdos::mesh::MeshEngine::getInstance();
    const auto& towers = mesh.getDiscoveredTowers();
    if (tower_idx < towers.size()) {
        const auto& t = towers[tower_idx];
        mesh.setChannel(t.channel);
        mesh.setLocalIdentity(mesh.getLocalShortId(), mesh.getLocalUuid(), t.short_id, 1, 1);
        
        char toast[64];
        snprintf(toast, sizeof(toast), "Conectado a: %s (Ch %u)", t.name, t.channel);
        UIManager::showToast(toast);
    }
}

void MeshConfigView::refresh_towers_ui() {
    if (!s_towersListContainer || !lv_obj_is_valid(s_towersListContainer)) return;
    lv_obj_clean(s_towersListContainer);

    const auto& towers = cbdos::mesh::MeshEngine::getInstance().getDiscoveredTowers();
    if (towers.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_towersListContainer);
        lv_label_set_text(emptyLbl, "No se detectaron torres en este canal.\nToca 'Escanear Torres' para emitir un sondeo de radio.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_width(emptyLbl, LV_PCT(100));
        lv_label_set_long_mode(emptyLbl, LV_LABEL_LONG_MODE_WRAP);
        return;
    }

    for (size_t i = 0; i < towers.size(); i++) {
        const auto& t = towers[i];

        lv_obj_t* card = lv_obj_create(s_towersListContainer);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(card, 12);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_style_pad_row(card, 6, 0);

        // Fila 1: Nombre de la Torre y RSSI badge
        lv_obj_t* rowHeader = lv_obj_create(card);
        lv_obj_set_width(rowHeader, lv_pct(100));
        lv_obj_set_height(rowHeader, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(rowHeader, 0, 0);
        lv_obj_set_style_border_width(rowHeader, 0, 0);
        lv_obj_set_style_pad_all(rowHeader, 0, 0);
        lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        DefaultTheme::disableScroll(rowHeader);

        lv_obj_t* nameLbl = lv_label_create(rowHeader);
        char nameBuf[48];
        snprintf(nameBuf, sizeof(nameBuf), "🗼 %s", t.name);
        lv_label_set_text(nameLbl, nameBuf);
        lv_obj_set_style_text_color(nameLbl, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);

        // Badge RSSI
        lv_obj_t* rssiBadge = lv_obj_create(rowHeader);
        lv_obj_set_size(rssiBadge, LV_SIZE_CONTENT, 22);
        lv_obj_set_style_radius(rssiBadge, 6, 0);
        lv_obj_set_style_pad_hor(rssiBadge, 6, 0);
        lv_obj_set_style_pad_ver(rssiBadge, 2, 0);
        DefaultTheme::disableScroll(rssiBadge);

        const char* sigIcon = "🔴";
        lv_color_t bgCol = lv_color_hex(0x7F1D1D);
        if (t.rssi > -65) {
            sigIcon = "🟢";
            bgCol = lv_color_hex(0x14532D);
        } else if (t.rssi > -80) {
            sigIcon = "🟡";
            bgCol = lv_color_hex(0x713F12);
        }
        lv_obj_set_style_bg_color(rssiBadge, bgCol, 0);
        lv_obj_set_style_bg_opa(rssiBadge, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(rssiBadge, 0, 0);

        lv_obj_t* rssiLbl = lv_label_create(rssiBadge);
        char rssiBuf[32];
        snprintf(rssiBuf, sizeof(rssiBuf), "%s %ddBm", sigIcon, t.rssi);
        lv_label_set_text(rssiLbl, rssiBuf);
        lv_obj_set_style_text_color(rssiLbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(rssiLbl, &lv_font_montserrat_12, 0);

        // Fila 2: Detalles técnicos (Canal, Modos, MAC)
        lv_obj_t* infoLbl = lv_label_create(card);
        char infoBuf[96];
        snprintf(infoBuf, sizeof(infoBuf), "Canal: %u | ShortID: 0x%04X | MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 t.channel, t.short_id, t.mac[0], t.mac[1], t.mac[2], t.mac[3], t.mac[4], t.mac[5]);
        lv_label_set_text(infoLbl, infoBuf);
        lv_obj_set_style_text_color(infoLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(infoLbl, &lv_font_montserrat_12, 0);

        // Fila 3: Botón Conectar
        lv_obj_t* btnConnect = lv_button_create(card);
        lv_obj_set_width(btnConnect, lv_pct(100));
        lv_obj_set_height(btnConnect, 30);
        DefaultTheme::applyButton(btnConnect, 8);
        lv_obj_add_event_cb(btnConnect, connect_tower_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);

        lv_obj_t* btnLbl = lv_label_create(btnConnect);
        lv_label_set_text(btnLbl, "🔗 Conectar a esta Torre");
        lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(btnLbl);
    }
}

bool MeshConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    UIManager::getInstance().getHeaderBar().showWifi(false);
    UIManager::getInstance().getHeaderBar().setTitle("Red Malla y Torres");
    UIManager::getInstance().getHeaderBar().showBackButton(true, []() {
        UIManager::getInstance().popView();
    });
    UIManager::getInstance().getHeaderBar().setRightAction(LV_SYMBOL_REFRESH, [this]() {
        scan_btn_cb(nullptr);
    });

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 20, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // ─────────────────────────────────────────────────────────────
    // TARJETA 1: Estado de la Radio y Selección de Modo
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardRadio = lv_obj_create(m_container);
    lv_obj_set_width(cardRadio, lv_pct(100));
    lv_obj_set_height(cardRadio, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardRadio, 14);
    lv_obj_set_flex_flow(cardRadio, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardRadio, 12, 0);
    lv_obj_set_style_pad_row(cardRadio, 8, 0);

    lv_obj_t* titleRadio = lv_label_create(cardRadio);
    lv_label_set_text(titleRadio, "📻 Estado de Radio & Modulación");
    lv_obj_set_style_text_color(titleRadio, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_font(titleRadio, &lv_font_montserrat_14, 0);

    // MAC Propia
    uint8_t my_mac[6] = {0};
    cbdos::mesh::MeshEngine::getInstance().getMacAddress(my_mac);
    s_lblMac = lv_label_create(cardRadio);
    char macBuf[48];
    snprintf(macBuf, sizeof(macBuf), "MAC Propia: %02X:%02X:%02X:%02X:%02X:%02X | Canal %u",
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5],
             cbdos::mesh::MeshEngine::getInstance().getChannel());
    lv_label_set_text(s_lblMac, macBuf);
    lv_obj_set_style_text_color(s_lblMac, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(s_lblMac, &lv_font_montserrat_12, 0);

    // Selector de Modo Dropdown
    lv_obj_t* lblModeSel = lv_label_create(cardRadio);
    lv_label_set_text(lblModeSel, "Modo de Transmisión:");
    lv_obj_set_style_text_color(lblModeSel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblModeSel, &lv_font_montserrat_12, 0);

    s_ddMode = lv_dropdown_create(cardRadio);
    lv_obj_set_width(s_ddMode, lv_pct(100));
    lv_obj_set_height(s_ddMode, 36);
    lv_dropdown_set_options(s_ddMode,
        "AUTO (Inteligente)\n"
        "ESP-NOW Normal\n"
        "ESP-NOW Long Range\n"
        "Wi-Fi TCP/IP"
    );
    lv_dropdown_set_selected(s_ddMode, static_cast<uint16_t>(cbdos::mesh::MeshEngine::getInstance().getRadioMode()));
    lv_obj_set_style_radius(s_ddMode, 8, 0);
    lv_obj_set_style_bg_color(s_ddMode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(s_ddMode, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_ddMode, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_color(s_ddMode, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(s_ddMode, mode_select_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ─────────────────────────────────────────────────────────────
    // TARJETA 2: Botón de Escaneo y Lista de Torres
    // ─────────────────────────────────────────────────────────────
    s_btnScan = lv_button_create(m_container);
    lv_obj_set_width(s_btnScan, lv_pct(100));
    lv_obj_set_height(s_btnScan, 40);
    DefaultTheme::applyButton(s_btnScan, 10);
    lv_obj_set_style_bg_color(s_btnScan, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(s_btnScan, scan_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lblBtnScan = lv_label_create(s_btnScan);
    lv_label_set_text(lblBtnScan, "📡 Escanear Torres y Nodos en el Aire");
    lv_obj_set_style_text_font(lblBtnScan, &lv_font_montserrat_14, 0);
    lv_obj_center(lblBtnScan);

    s_lblScanStatus = lv_label_create(m_container);
    lv_label_set_text(s_lblScanStatus, "Toca 'Escanear' para buscar antenas y torres.");
    lv_obj_set_style_text_color(s_lblScanStatus, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(s_lblScanStatus, &lv_font_montserrat_12, 0);

    // Contenedor dinámico de Torres
    s_towersListContainer = lv_obj_create(m_container);
    lv_obj_set_width(s_towersListContainer, lv_pct(100));
    lv_obj_set_height(s_towersListContainer, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_towersListContainer, 0, 0);
    lv_obj_set_style_border_width(s_towersListContainer, 0, 0);
    lv_obj_set_style_pad_all(s_towersListContainer, 0, 0);
    lv_obj_set_style_pad_row(s_towersListContainer, 8, 0);
    lv_obj_set_flex_flow(s_towersListContainer, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(s_towersListContainer);

    cbdos::mesh::MeshEngine::getInstance().setDiscoveredTowersCallback([]() {
        refresh_towers_ui();
    });

    refresh_towers_ui();

    return true;
}

} // namespace ui
} // namespace cbdos
