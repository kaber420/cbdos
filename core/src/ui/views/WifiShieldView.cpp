#include "WifiShieldView.hpp"
#include "../UIManager.hpp"
#include "../../security/WifiShieldService.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

WifiShieldView::WifiShieldView()
    : BaseView("Wi-Fi Shield", ViewMode::Solid) {
}

WifiShieldView::~WifiShieldView() {
    onDestroy();
}

bool WifiShieldView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);

    createTabs(m_container);
    return true;
}

void WifiShieldView::onDestroy() {
    security::WifiShieldService::getInstance().setApCallback(nullptr);
    if (m_refreshTimer) {
        lv_timer_delete(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
    BaseView::onDestroy();
}

void WifiShieldView::onShow() {
    BaseView::onShow();
    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle("Wi-Fi Shield");
    header.showWifi(true);
    header.showBackButton(true, []() {
        UIManager::getInstance().popView();
    });

    security::WifiShieldService::getInstance().setApCallback([this](const radio::WifiApRecord&) {
        m_apsDirty = true;
    });

    if (!m_refreshTimer) {
        m_refreshTimer = lv_timer_create([](lv_timer_t* t) {
            auto* view = static_cast<WifiShieldView*>(lv_timer_get_user_data(t));
            if (!view) return;
            if (view->m_apsDirty.exchange(false)) {
                view->refreshApList();
            }
            view->updateIdsDisplay();
            view->updateSnifferDisplay();
        }, 250, this);
    } else {
        lv_timer_resume(m_refreshTimer);
    }

    refreshApList();
}

void WifiShieldView::onHide() {
    BaseView::onHide();
    security::WifiShieldService::getInstance().setApCallback(nullptr);
    if (m_refreshTimer) lv_timer_pause(m_refreshTimer);
}

void WifiShieldView::createTabs(lv_obj_t* parent) {
    m_tabView = lv_tabview_create(parent);
    lv_obj_set_size(m_tabView, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_tabView, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_border_width(m_tabView, 0, 0);

    lv_obj_t* tabBar = lv_tabview_get_tab_bar(m_tabView);
    if (tabBar) {
        lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x111625), 0);
        lv_obj_set_style_text_color(tabBar, lv_color_hex(0x00B4D8), static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_ITEMS) | static_cast<uint32_t>(LV_STATE_CHECKED)));
        lv_tabview_set_tab_bar_size(m_tabView, 38);
    }

    lv_obj_t* tabRecon = lv_tabview_add_tab(m_tabView, "Recon");
    lv_obj_t* tabIds = lv_tabview_add_tab(m_tabView, "IDS Sentinel");
    lv_obj_t* tabSniffer = lv_tabview_add_tab(m_tabView, "Sniffer");

    setupReconTab(tabRecon);
    setupIdsTab(tabIds);
    setupSnifferTab(tabSniffer);
}

// ────────────────────────────────────────────────────────────────
// Pestana 1: Recon
// ────────────────────────────────────────────────────────────────

void WifiShieldView::setupReconTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* topBar = lv_obj_create(tab);
    lv_obj_set_size(topBar, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(topBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(topBar, 0, 0);
    lv_obj_set_style_pad_all(topBar, 0, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblReconCount = lv_label_create(topBar);
    lv_label_set_text(m_lblReconCount, "APs: 0");
    lv_obj_set_style_text_color(m_lblReconCount, lv_color_hex(0xE2E8F0), 0);

    m_btnScanToggle = lv_btn_create(topBar);
    lv_obj_set_size(m_btnScanToggle, 130, 34);
    lv_obj_set_style_bg_color(m_btnScanToggle, lv_color_hex(0x0E7490), 0);
    lv_obj_set_style_radius(m_btnScanToggle, 6, 0);
    m_lblScanBtn = lv_label_create(m_btnScanToggle);
    lv_label_set_text(m_lblScanBtn, "Iniciar Scan");
    lv_obj_center(m_lblScanBtn);

    lv_obj_add_event_cb(m_btnScanToggle, [](lv_event_t* e) {
        auto* view = static_cast<WifiShieldView*>(lv_event_get_user_data(e));
        auto& svc = security::WifiShieldService::getInstance();
        if (svc.isScanning()) {
            svc.stopScan();
        } else {
            svc.startScan();
        }
        if (view && view->m_lblScanBtn) {
            lv_label_set_text(view->m_lblScanBtn, svc.isScanning() ? "Detener Scan" : "Iniciar Scan");
            lv_obj_set_style_bg_color(view->m_btnScanToggle,
                svc.isScanning() ? lv_color_hex(0xB91C1C) : lv_color_hex(0x0E7490), 0);
        }
    }, LV_EVENT_CLICKED, this);

    m_listContainer = lv_obj_create(tab);
    lv_obj_set_size(m_listContainer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_listContainer, 1);
    lv_obj_set_style_bg_opa(m_listContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_listContainer, 0, 0);
    lv_obj_set_style_pad_all(m_listContainer, 2, 0);
    lv_obj_set_flex_flow(m_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_listContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

void WifiShieldView::addApCard(const radio::WifiApRecord& ap) {
    if (!m_listContainer) return;

    lv_obj_t* card = lv_obj_create(m_listContainer);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111625), 0);
    lv_obj_set_style_border_color(card, ap.rogueSuspect ? lv_color_hex(0xFF3366) : lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    char title[96];
    const char* ssid = ap.ssid.empty() ? "<oculto>" : ap.ssid.c_str();
    snprintf(title, sizeof(title), "%s%s", ssid, ap.rogueSuspect ? "  [EVIL TWIN?]" : "");
    lv_obj_t* lblTitle = lv_label_create(card);
    lv_label_set_text(lblTitle, title);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(ap.rogueSuspect ? 0xFF3366 : 0x60A5FA), 0);

    char detail[128];
    snprintf(detail, sizeof(detail), "%s | CH %u | %d dBm | %s",
             ap.bssidStr.c_str(), static_cast<unsigned>(ap.channel),
             static_cast<int>(ap.rssi), radio::getWifiAuthName(ap.auth));
    lv_obj_t* lblDetail = lv_label_create(card);
    lv_label_set_text(lblDetail, detail);
    lv_obj_set_style_text_color(lblDetail, lv_color_hex(0x94A3B8), 0);

    lv_obj_t* lblVendor = lv_label_create(card);
    lv_label_set_text(lblVendor, ap.vendor.c_str());
    lv_obj_set_style_text_color(lblVendor, lv_color_hex(0x64748B), 0);
}

void WifiShieldView::refreshApList() {
    if (!m_listContainer) return;
    lv_obj_clean(m_listContainer);

    auto aps = security::WifiShieldService::getInstance().getAccessPoints();
    for (const auto& ap : aps) {
        addApCard(ap);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "APs: %u", static_cast<unsigned>(aps.size()));
    if (m_lblReconCount) lv_label_set_text(m_lblReconCount, buf);
}

// ────────────────────────────────────────────────────────────────
// Pestana 2: IDS Sentinel
// ────────────────────────────────────────────────────────────────

void WifiShieldView::setupIdsTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_cardIdsBanner = lv_obj_create(tab);
    lv_obj_set_size(m_cardIdsBanner, LV_PCT(100), 65);
    lv_obj_set_style_bg_color(m_cardIdsBanner, lv_color_hex(0x064E3B), 0);
    lv_obj_set_style_border_color(m_cardIdsBanner, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(m_cardIdsBanner, 1, 0);
    lv_obj_set_style_radius(m_cardIdsBanner, 8, 0);
    lv_obj_set_flex_flow(m_cardIdsBanner, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_cardIdsBanner, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    m_lblIdsTitle = lv_label_create(m_cardIdsBanner);
    lv_label_set_text(m_lblIdsTitle, LV_SYMBOL_OK " SECTOR LIMPIO - VIGILANCIA ACTIVA");
    lv_obj_set_style_text_color(m_lblIdsTitle, lv_color_hex(0x00F5D4), 0);

    m_lblIdsDesc = lv_label_create(m_cardIdsBanner);
    lv_label_set_text(m_lblIdsDesc, "Sin anomalias 802.11 en el espectro");
    lv_obj_set_style_text_color(m_lblIdsDesc, lv_color_hex(0xE2E8F0), 0);

    lv_obj_t* metricsGrid = lv_obj_create(tab);
    lv_obj_set_size(metricsGrid, LV_PCT(100), 55);
    lv_obj_set_style_bg_opa(metricsGrid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(metricsGrid, 0, 0);
    lv_obj_set_style_pad_all(metricsGrid, 0, 0);
    lv_obj_set_flex_flow(metricsGrid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(metricsGrid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto makeMetricCard = [metricsGrid](const char* label, const char* initVal, lv_obj_t** outValLbl) {
        lv_obj_t* c = lv_obj_create(metricsGrid);
        lv_obj_set_size(c, LV_PCT(48), LV_PCT(100));
        lv_obj_set_style_bg_color(c, lv_color_hex(0x111625), 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_radius(c, 6, 0);
        lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lblTitle = lv_label_create(c);
        lv_label_set_text(lblTitle, label);
        lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x94A3B8), 0);

        *outValLbl = lv_label_create(c);
        lv_label_set_text(*outValLbl, initVal);
        lv_obj_set_style_text_color(*outValLbl, lv_color_hex(0xFFFFFF), 0);
    };

    makeMetricCard("Tramas Analizadas", "0", &m_lblIdsCount);
    makeMetricCard("Tasa de Ataque", "0.0 pps", &m_lblIdsRate);

    lv_obj_t* logCard = lv_obj_create(tab);
    lv_obj_set_size(logCard, LV_PCT(100), 110);
    lv_obj_set_style_bg_color(logCard, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_border_color(logCard, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(logCard, 1, 0);
    lv_obj_set_style_radius(logCard, 6, 0);
    lv_obj_set_style_pad_all(logCard, 6, 0);

    m_lblIdsLog = lv_label_create(logCard);
    lv_label_set_text(m_lblIdsLog, "[IDS] Centinela pasivo iniciado. Esperando eventos...");
    lv_obj_set_style_text_color(m_lblIdsLog, lv_color_hex(0x94A3B8), 0);

    setupIdsConfigSection(tab);

    lv_obj_t* btnReset = lv_btn_create(tab);
    lv_obj_set_size(btnReset, 160, 32);
    lv_obj_set_style_bg_color(btnReset, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(btnReset, 6, 0);
    lv_obj_t* lblR = lv_label_create(btnReset);
    lv_label_set_text(lblR, "Restablecer Alertas");
    lv_obj_center(lblR);

    lv_obj_add_event_cb(btnReset, [](lv_event_t* e) {
        security::WifiShieldService::getInstance().clearAlert();
        auto* view = static_cast<WifiShieldView*>(lv_event_get_user_data(e));
        if (view) view->updateIdsDisplay();
    }, LV_EVENT_CLICKED, this);
}

namespace {
constexpr intptr_t IDS_TOGGLE_MASTER = -1;
constexpr intptr_t IDS_TOGGLE_LOG = -2;

void idsToggleCb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    intptr_t kind = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));

    auto& svc = security::WifiShieldService::getInstance();
    if (kind == IDS_TOGGLE_MASTER) {
        svc.setIdsEnabled(on);
        if (on) svc.setSnifferEnabled(true);   // El IDS necesita captura promiscua
    } else if (kind == IDS_TOGGLE_LOG) {
        auto cfg = svc.getIdsConfig();
        cfg.logging = on;
        svc.setIdsConfig(cfg);
    } else {
        svc.setIdsDetection(static_cast<radio::WifiAlertType>(kind), on);
    }

    auto* view = static_cast<WifiShieldView*>(lv_obj_get_user_data(sw));
    if (view) view->refreshIdsConfigLabels();
}
} // namespace

void WifiShieldView::setupIdsConfigSection(lv_obj_t* tab) {
    auto cfg = security::WifiShieldService::getInstance().getIdsConfig();

    lv_obj_t* card = lv_obj_create(tab);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111625), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "CONFIGURACION IDS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00B4D8), 0);

    auto addRow = [this, card](const char* label, bool initOn, intptr_t kind, lv_obj_t** outSw) {
        lv_obj_t* row = lv_obj_create(card);
        lv_obj_set_size(row, LV_PCT(100), 28);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE2E8F0), 0);

        lv_obj_t* sw = lv_switch_create(row);
        if (initOn) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_set_user_data(sw, this);
        lv_obj_add_event_cb(sw, idsToggleCb, LV_EVENT_VALUE_CHANGED, reinterpret_cast<void*>(kind));
        if (outSw) *outSw = sw;
    };

    addRow("IDS Activo", cfg.enabled, IDS_TOGGLE_MASTER, &m_swIdsMaster);
    addRow("Registrar Eventos", cfg.logging, IDS_TOGGLE_LOG, &m_swIdsLog);
    addRow("Deauth Flood", cfg.detectDeauth, static_cast<intptr_t>(radio::WifiAlertType::DeauthFlood), nullptr);
    addRow("Disassoc Flood", cfg.detectDisassoc, static_cast<intptr_t>(radio::WifiAlertType::DisassocFlood), nullptr);
    addRow("Beacon Spam", cfg.detectBeaconSpam, static_cast<intptr_t>(radio::WifiAlertType::BeaconSpam), nullptr);
    addRow("Rogue AP / Evil Twin", cfg.detectRogueAp, static_cast<intptr_t>(radio::WifiAlertType::RogueAp), nullptr);

    m_lblIdsEnables = lv_label_create(card);
    lv_obj_set_style_text_color(m_lblIdsEnables, lv_color_hex(0x94A3B8), 0);
    refreshIdsConfigLabels();
}

void WifiShieldView::refreshIdsConfigLabels() {
    if (!m_lblIdsEnables) return;
    auto cfg = security::WifiShieldService::getInstance().getIdsConfig();
    char buf[200];
    snprintf(buf, sizeof(buf), "Ventana: %u ms | Umbral: %u pkt | Beacon>=%u BSSID | Estado: %s",
             static_cast<unsigned>(cfg.windowMs),
             static_cast<unsigned>(cfg.minPacketsInWindow),
             static_cast<unsigned>(cfg.beaconUniqueThreshold),
             cfg.enabled ? "ACTIVO" : "INACTIVO");
    lv_label_set_text(m_lblIdsEnables, buf);
}

void WifiShieldView::updateIdsDisplay() {
    auto& svc = security::WifiShieldService::getInstance();
    auto alert = svc.getAlert();

    if (alert.active) {
        if (m_cardIdsBanner) {
            lv_obj_set_style_bg_color(m_cardIdsBanner, lv_color_hex(0x4C0519), 0);
            lv_obj_set_style_border_color(m_cardIdsBanner, lv_color_hex(0xFF3366), 0);
        }
        if (m_lblIdsTitle) {
            lv_label_set_text(m_lblIdsTitle, LV_SYMBOL_WARNING " ALERTA: ATAQUE WI-FI EN CURSO");
            lv_obj_set_style_text_color(m_lblIdsTitle, lv_color_hex(0xFF3366), 0);
        }
        if (m_lblIdsDesc) {
            lv_label_set_text(m_lblIdsDesc, alert.description.c_str());
        }
    } else {
        if (m_cardIdsBanner) {
            lv_obj_set_style_bg_color(m_cardIdsBanner, lv_color_hex(0x064E3B), 0);
            lv_obj_set_style_border_color(m_cardIdsBanner, lv_color_hex(0x00F5D4), 0);
        }
        if (m_lblIdsTitle) {
            lv_label_set_text(m_lblIdsTitle, LV_SYMBOL_OK " SECTOR LIMPIO - VIGILANCIA ACTIVA");
            lv_obj_set_style_text_color(m_lblIdsTitle, lv_color_hex(0x00F5D4), 0);
        }
        if (m_lblIdsDesc) {
            lv_label_set_text(m_lblIdsDesc, "Sin anomalias 802.11 en el espectro");
        }
    }

    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%u", static_cast<unsigned>(svc.getTotalAlertPackets()));
    if (m_lblIdsCount) lv_label_set_text(m_lblIdsCount, countBuf);

    char rateBuf[16];
    snprintf(rateBuf, sizeof(rateBuf), "%.1f pps", alert.packetsPerSec);
    if (m_lblIdsRate) lv_label_set_text(m_lblIdsRate, rateBuf);

    auto log = svc.getAlertLog();
    if (!log.empty() && m_lblIdsLog) {
        std::string fullLog;
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            fullLog += *it + "\n";
        }
        lv_label_set_text(m_lblIdsLog, fullLog.c_str());
    }
}

// ────────────────────────────────────────────────────────────────
// Pestana 3: Sniffer
// ────────────────────────────────────────────────────────────────

void WifiShieldView::setupSnifferTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* ctrlRow = lv_obj_create(tab);
    lv_obj_set_size(ctrlRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(ctrlRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctrlRow, 0, 0);
    lv_obj_set_style_pad_all(ctrlRow, 0, 0);
    lv_obj_set_flex_flow(ctrlRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrlRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl = lv_label_create(ctrlRow);
    lv_label_set_text(lbl, "Captura promiscua 802.11");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xE2E8F0), 0);

    m_swSniffer = lv_switch_create(ctrlRow);
    lv_obj_add_event_cb(m_swSniffer, [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
        auto* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
        bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
        security::WifiShieldService::getInstance().setSnifferEnabled(on);
    }, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* grid = lv_obj_create(tab);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto makeStat = [grid](const char* label, lv_obj_t** outValLbl) {
        lv_obj_t* c = lv_obj_create(grid);
        lv_obj_set_size(c, LV_PCT(48), 60);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x111625), 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(c, 1, 0);
        lv_obj_set_style_radius(c, 6, 0);
        lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lblTitle = lv_label_create(c);
        lv_label_set_text(lblTitle, label);
        lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x94A3B8), 0);

        *outValLbl = lv_label_create(c);
        lv_label_set_text(*outValLbl, "0");
        lv_obj_set_style_text_color(*outValLbl, lv_color_hex(0xFFFFFF), 0);
    };

    makeStat("Tramas Totales", &m_lblSniffTotal);
    makeStat("Balizas (Beacon)", &m_lblSniffBeacon);
    makeStat("Deauth", &m_lblSniffDeauth);
    makeStat("Datos", &m_lblSniffData);

    lv_obj_t* hint = lv_label_create(tab);
    lv_label_set_text(hint, "Monitor pasivo de tramas por tipo. Permite alimentar el IDS Sentinel.");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748B), 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, LV_PCT(100));
}

void WifiShieldView::updateSnifferDisplay() {
    auto& svc = security::WifiShieldService::getInstance();

    if (m_swSniffer) {
        bool on = svc.isSnifferEnabled();
        if (on && !lv_obj_has_state(m_swSniffer, LV_STATE_CHECKED)) lv_obj_add_state(m_swSniffer, LV_STATE_CHECKED);
        else if (!on && lv_obj_has_state(m_swSniffer, LV_STATE_CHECKED)) lv_obj_remove_state(m_swSniffer, LV_STATE_CHECKED);
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(svc.getSniffedFrameCount()));
    if (m_lblSniffTotal) lv_label_set_text(m_lblSniffTotal, buf);

    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(svc.getBeaconCount()));
    if (m_lblSniffBeacon) lv_label_set_text(m_lblSniffBeacon, buf);

    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(svc.getDeauthCount()));
    if (m_lblSniffDeauth) lv_label_set_text(m_lblSniffDeauth, buf);

    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(svc.getDataFrameCount()));
    if (m_lblSniffData) lv_label_set_text(m_lblSniffData, buf);
}

} // namespace ui
} // namespace cbdos
