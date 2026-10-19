#include "WifiStrikeView.hpp"
#include "../UIManager.hpp"
#include "../../security/WifiShieldService.hpp"
#include "../../security/WifiStrikeService.hpp"

#include <cstdio>

namespace cbdos {
namespace ui {

using security::WifiShieldService;
using security::WifiStrikeService;
using wifi::BeaconMode;
using wifi::StrikeConfig;
using wifi::StrikeState;
using wifi::StrikeStatus;

static const lv_color_t kStart = lv_color_hex(0x0E7490);   // Teal: sesion parada
static const lv_color_t kStop  = lv_color_hex(0xB91C1C);   // Rojo: emitiendo
static const lv_color_t kLive  = lv_color_hex(0x22C55E);
static const lv_color_t kCardBg = lv_color_hex(0x111625);
static const lv_color_t kMuted = lv_color_hex(0x94A3B8);

// Orden del dropdown de la UI -> BeaconMode del core.
static const BeaconMode kBeaconModes[] = {
    BeaconMode::Rickroll,
    BeaconMode::Random,
    BeaconMode::CloneAccessPoints,
    BeaconMode::SingleSsid,
};
static const size_t kBeaconModeCount = sizeof(kBeaconModes) / sizeof(kBeaconModes[0]);

// Cadencia del refresco visual de la pestana Beacon.
static constexpr uint32_t kBeaconRefreshMs = 250;

// SSID de reserva cuando el modo es SSID Fijo y el campo queda vacio: sin el,
// validate() rechazaria la sesion.
static const char* kFallbackSsid = "cbdos-Beacon";

WifiStrikeView::WifiStrikeView()
    : BaseView("Wi-Fi Strike", ViewMode::Solid) {
}

WifiStrikeView::~WifiStrikeView() {
    onDestroy();
}

bool WifiStrikeView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);

    createTabs(m_container);
    return true;
}

void WifiStrikeView::onDestroy() {
    WifiStrikeService::getInstance().setStatusCallback(nullptr);
    if (m_beaconTimer) {
        lv_timer_delete(m_beaconTimer);
        m_beaconTimer = nullptr;
    }
    m_ddBeaconMode = nullptr;
    m_ssidRow = nullptr;
    m_taBeaconSsid = nullptr;
    m_cloneRow = nullptr;
    m_lblCloneInfo = nullptr;
    m_ledBeacon = nullptr;
    m_btnBeaconToggle = nullptr;
    m_lblBeaconToggle = nullptr;
    m_lblFrames = nullptr;
    m_lblChannel = nullptr;
    m_lblFailed = nullptr;
    m_lblBeaconState = nullptr;
    m_lblBeaconHint = nullptr;
    m_lblBeaconStatus = nullptr;
    BaseView::onDestroy();
}

void WifiStrikeView::onShow() {
    BaseView::onShow();
    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle("Wi-Fi Strike");
    header.showWifi(true);
    header.showBackButton(true, []() {
        UIManager::getInstance().popView();
    });

    // El servicio publica desde su propia tarea de emision; el callback no
    // toca LVGL: solo marca que hay estado nuevo para el timer de la UI.
    WifiStrikeService::getInstance().setStatusCallback([this](const wifi::StrikeStatus&) {
        m_statusPending.store(true);
    });

    // El timer solo existe mientras la vista esta delante: en segundo plano
    // corre la tarea de emision, no hace falta repintar contadores.
    if (!m_beaconTimer) {
        m_beaconTimer = lv_timer_create([](lv_timer_t* t) {
            auto* self = static_cast<WifiStrikeView*>(lv_timer_get_user_data(t));
            if (!self) return;
            self->refreshBeaconStatus();
        }, kBeaconRefreshMs, this);
    } else {
        lv_timer_resume(m_beaconTimer);
    }

    m_statusPending.store(true);
    refreshBeaconStatus();
}

void WifiStrikeView::onHide() {
    BaseView::onHide();
    WifiStrikeService::getInstance().setStatusCallback(nullptr);
    if (m_beaconTimer) lv_timer_pause(m_beaconTimer);
}

void WifiStrikeView::createTabs(lv_obj_t* parent) {
    m_tabView = lv_tabview_create(parent);
    lv_obj_set_size(m_tabView, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_tabView, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_border_width(m_tabView, 0, 0);

    lv_obj_t* tabBar = lv_tabview_get_tab_bar(m_tabView);
    if (tabBar) {
        lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x18080A), 0);
        lv_obj_set_style_text_color(tabBar, lv_color_hex(0xE63946), static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_ITEMS) | static_cast<uint32_t>(LV_STATE_CHECKED)));
        lv_tabview_set_tab_bar_size(m_tabView, 36);
    }

    lv_obj_t* tabDeauth = lv_tabview_add_tab(m_tabView, "Deauth");
    lv_obj_t* tabBeacon = lv_tabview_add_tab(m_tabView, "Beacon Spam");
    lv_obj_t* tabPortal = lv_tabview_add_tab(m_tabView, "Evil Portal");
    lv_obj_t* tabPmkid  = lv_tabview_add_tab(m_tabView, "PMKID");

    setupDeauthTab(tabDeauth);
    setupBeaconTab(tabBeacon);
    setupPortalTab(tabPortal);
    setupPmkidTab(tabPmkid);
}

void WifiStrikeView::setupDeauthTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    m_lblDeauthStatus = lv_label_create(tab);
    lv_label_set_text(m_lblDeauthStatus, "Deauth: Evaluacion de resiliencia ante desasociacion 802.11");
    lv_obj_set_style_text_color(m_lblDeauthStatus, lv_color_hex(0xE63946), 0);
    lv_obj_align(m_lblDeauthStatus, LV_ALIGN_TOP_LEFT, 0, 0);
}

void WifiStrikeView::setupBeaconTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblBeaconStatus = lv_label_create(tab);
    lv_label_set_text(m_lblBeaconStatus, "Beacon Spam: Emision de tramas de prueba SSIDs");
    lv_obj_set_style_text_color(m_lblBeaconStatus, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_width(m_lblBeaconStatus, LV_PCT(100));
    lv_obj_set_style_text_font(m_lblBeaconStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_bottom(m_lblBeaconStatus, 6, 0);

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

    // --- Modo de emision ---
    lv_obj_t* lblModeCaption = lv_label_create(tab);
    lv_label_set_text(lblModeCaption, "MODO DE BALIZA");
    lv_obj_set_style_text_font(lblModeCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblModeCaption, kMuted, 0);

    m_ddBeaconMode = lv_dropdown_create(tab);
    styleDropdown(m_ddBeaconMode);
    lv_dropdown_set_options(m_ddBeaconMode,
        "Rickroll (Astley)\n"
        "SSIDs Aleatorios\n"
        "Clonar APs Escaneados\n"
        "SSID Fijo");
    const StrikeConfig cfg = WifiStrikeService::getInstance().getConfig();
    for (size_t i = 0; i < kBeaconModeCount; ++i) {
        if (kBeaconModes[i] == cfg.mode) {
            lv_dropdown_set_selected(m_ddBeaconMode, static_cast<uint16_t>(i));
            break;
        }
    }
    lv_obj_add_event_cb(m_ddBeaconMode, WifiStrikeView::modeCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Campo SSID (solo en modo SSID Fijo) ---
    m_ssidRow = lv_obj_create(tab);
    lv_obj_set_size(m_ssidRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_ssidRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_ssidRow, 0, 0);
    lv_obj_set_style_pad_all(m_ssidRow, 0, 0);
    lv_obj_set_flex_flow(m_ssidRow, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lblSsidCaption = lv_label_create(m_ssidRow);
    lv_label_set_text(lblSsidCaption, "SSID A EMITIR");
    lv_obj_set_style_text_font(lblSsidCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblSsidCaption, kMuted, 0);

    m_taBeaconSsid = lv_textarea_create(m_ssidRow);
    lv_obj_set_size(m_taBeaconSsid, LV_PCT(100), 40);
    lv_textarea_set_one_line(m_taBeaconSsid, true);
    lv_textarea_set_max_length(m_taBeaconSsid, static_cast<int32_t>(wifi::STRIKE_SSID_MAX));
    lv_textarea_set_placeholder_text(m_taBeaconSsid, kFallbackSsid);
    if (!cfg.ssid.empty()) {
        lv_textarea_set_text(m_taBeaconSsid, cfg.ssid.c_str());
    }
    lv_obj_add_event_cb(m_taBeaconSsid, [](lv_event_t* e) {
        UIManager::attachKeyboard(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    }, LV_EVENT_FOCUSED, nullptr);

    // --- Contador de APs clonables (solo en modo Clonar APs) ---
    m_cloneRow = lv_obj_create(tab);
    lv_obj_set_size(m_cloneRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_cloneRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_cloneRow, 0, 0);
    lv_obj_set_style_pad_all(m_cloneRow, 0, 0);
    lv_obj_set_flex_flow(m_cloneRow, LV_FLEX_FLOW_COLUMN);

    m_lblCloneInfo = lv_label_create(m_cloneRow);
    lv_obj_set_width(m_lblCloneInfo, LV_PCT(100));
    lv_label_set_text(m_lblCloneInfo, "APs en Air Recon: 0");
    lv_obj_set_style_text_font(m_lblCloneInfo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblCloneInfo, kMuted, 0);

    // --- Metricas reactivas ---
    lv_obj_t* card = lv_obj_create(tab);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, kCardBg, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_hor(card, 10, 0);
    lv_obj_set_style_pad_ver(card, 8, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    auto makeMetric = [card](const char* caption, lv_obj_t** valueOut) {
        lv_obj_t* value = lv_label_create(card);
        lv_label_set_text(value, "-");
        lv_obj_set_style_text_font(value, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(0xE2E8F0), 0);
        *valueOut = value;

        lv_obj_t* cap = lv_label_create(card);
        lv_label_set_text(cap, caption);
        lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(cap, kMuted, 0);
    };

    makeMetric("Tramas emitidas", &m_lblFrames);
    makeMetric("Canal actual", &m_lblChannel);
    makeMetric("Fallos", &m_lblFailed);

    // Fila de estado: LED + nombre del estado de la maquina de emision.
    lv_obj_t* rowState = lv_obj_create(card);
    lv_obj_set_size(rowState, LV_PCT(100), 24);
    lv_obj_set_style_bg_opa(rowState, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowState, 0, 0);
    lv_obj_set_style_pad_all(rowState, 0, 0);
    lv_obj_set_style_pad_column(rowState, 8, 0);
    lv_obj_set_flex_flow(rowState, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowState, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_ledBeacon = lv_led_create(rowState);
    lv_obj_set_size(m_ledBeacon, 10, 10);
    lv_led_set_color(m_ledBeacon, kLive);
    lv_led_off(m_ledBeacon);

    m_lblBeaconState = lv_label_create(rowState);
    lv_label_set_text(m_lblBeaconState, wifi::getStrikeStateName(StrikeState::Idle));
    lv_obj_set_style_text_font(m_lblBeaconState, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblBeaconState, kMuted, 0);

    m_lblBeaconHint = lv_label_create(card);
    lv_obj_set_width(m_lblBeaconHint, LV_PCT(100));
    lv_label_set_long_mode(m_lblBeaconHint, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(m_lblBeaconHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblBeaconHint, lv_color_hex(0xE63946), 0);
    lv_label_set_text(m_lblBeaconHint, "");

    // --- Boton Iniciar / Detener ---
    m_btnBeaconToggle = lv_button_create(tab);
    lv_obj_set_size(m_btnBeaconToggle, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(m_btnBeaconToggle, kStart, 0);
    lv_obj_set_style_radius(m_btnBeaconToggle, 8, 0);
    lv_obj_add_event_cb(m_btnBeaconToggle, WifiStrikeView::toggleCb, LV_EVENT_CLICKED, this);

    m_lblBeaconToggle = lv_label_create(m_btnBeaconToggle);
    lv_obj_set_style_text_font(m_lblBeaconToggle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblBeaconToggle, lv_color_white(), 0);
    lv_obj_center(m_lblBeaconToggle);
    lv_label_set_text(m_lblBeaconToggle, "INICIAR EMISION");

    applyBeaconModeVisibility();
}

void WifiStrikeView::setupPortalTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    m_lblPortalStatus = lv_label_create(tab);
    lv_label_set_text(m_lblPortalStatus, "Evil Portal: Servidor cautivo de auditoria");
    lv_obj_set_style_text_color(m_lblPortalStatus, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(m_lblPortalStatus, LV_ALIGN_TOP_LEFT, 0, 0);
}

void WifiStrikeView::setupPmkidTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    m_lblPmkidStatus = lv_label_create(tab);
    lv_label_set_text(m_lblPmkidStatus, "PMKID: Captura de autenticaciones para analisis offline");
    lv_obj_set_style_text_color(m_lblPmkidStatus, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(m_lblPmkidStatus, LV_ALIGN_TOP_LEFT, 0, 0);
}

void WifiStrikeView::applyBeaconModeVisibility() {
    if (!m_ddBeaconMode) return;
    const uint32_t sel = lv_dropdown_get_selected(m_ddBeaconMode);
    const BeaconMode mode = kBeaconModes[sel < kBeaconModeCount ? sel : 0];

    // Solo el modo SSID Fijo necesita campo de texto y solo el modo clonado
    // consume el Air Recon: el resto de controles se retiran de la pantalla.
    if (m_ssidRow) {
        lv_obj_set_hidden(m_ssidRow, mode != BeaconMode::SingleSsid);
    }
    if (m_cloneRow) {
        lv_obj_set_hidden(m_cloneRow, mode != BeaconMode::CloneAccessPoints);
    }
}

StrikeConfig WifiStrikeView::buildBeaconConfig() const {
    StrikeConfig cfg = WifiStrikeService::getInstance().getConfig();

    const uint32_t sel = m_ddBeaconMode ? lv_dropdown_get_selected(m_ddBeaconMode) : 0;
    cfg.mode = kBeaconModes[sel < kBeaconModeCount ? sel : 0];

    if (cfg.mode == BeaconMode::SingleSsid) {
        const char* typed = m_taBeaconSsid ? lv_textarea_get_text(m_taBeaconSsid) : nullptr;
        cfg.ssid = (typed && typed[0] != '\0') ? std::string(typed) : std::string(kFallbackSsid);
    }

    if (cfg.mode == BeaconMode::CloneAccessPoints) {
        // Snapshot del Air Recon en el instante de arrancar: la emision pausa el
        // escaneo y monopoliza la radio, asi que la lista se congela aqui.
        cfg.apList = WifiShieldService::getInstance().getAccessPoints();
        if (cfg.apList.size() > wifi::STRIKE_AP_LIST_MAX) {
            cfg.apList.resize(wifi::STRIKE_AP_LIST_MAX);
        }
    } else {
        cfg.apList.clear();
    }

    return cfg;
}

void WifiStrikeView::refreshBeaconStatus() {
    if (!m_lblBeaconState || !m_btnBeaconToggle) return;

    WifiStrikeService& svc = WifiStrikeService::getInstance();
    const StrikeStatus status = svc.getStatus();

    char buf[48];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(status.stats.framesSent));
    lv_label_set_text(m_lblFrames, buf);

    if (status.stats.lastChannel != 0) {
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(status.stats.lastChannel));
    } else {
        snprintf(buf, sizeof(buf), "-");
    }
    lv_label_set_text(m_lblChannel, buf);

    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(status.stats.framesFailed));
    lv_label_set_text(m_lblFailed, buf);

    const bool emitting = (status.state == StrikeState::Running);
    const bool faulted = (status.state == StrikeState::Faulted);

    lv_label_set_text(m_lblBeaconState, wifi::getStrikeStateName(status.state));
    lv_obj_set_style_text_color(m_lblBeaconState,
        faulted ? lv_color_hex(0xE63946) : (emitting ? kLive : kMuted), 0);

    if (m_ledBeacon) {
        if (emitting) {
            lv_led_on(m_ledBeacon);
        } else {
            lv_led_off(m_ledBeacon);
        }
    }

    // El boton conmuta de Teal a Rojo segun haya sesion viva o no.
    lv_obj_set_style_bg_color(m_btnBeaconToggle, emitting ? kStop : kStart, 0);
    lv_label_set_text(m_lblBeaconToggle, emitting ? "DETENER" : "INICIAR EMISION");

    if (status.state == StrikeState::Stopping) {
        lv_obj_add_state(m_btnBeaconToggle, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(m_btnBeaconToggle, LV_STATE_DISABLED);
    }

    if (m_lblCloneInfo && m_cloneRow && !lv_obj_is_hidden(m_cloneRow)) {
        snprintf(buf, sizeof(buf), "APs en Air Recon: %u",
                 static_cast<unsigned>(WifiShieldService::getInstance().getAccessPointCount()));
        lv_label_set_text(m_lblCloneInfo, buf);
    }

    const bool cloning = m_cloneRow && !lv_obj_is_hidden(m_cloneRow);
    if (faulted && !status.lastError.empty()) {
        lv_label_set_text(m_lblBeaconHint, status.lastError.c_str());
    } else if (cloning && WifiShieldService::getInstance().getAccessPointCount() == 0) {
        lv_label_set_text(m_lblBeaconHint, "Sin APs en Air Recon: abre Wi-Fi Shield y escanea");
    } else {
        lv_label_set_text(m_lblBeaconHint, "");
    }

    m_statusPending.store(false);
}

void WifiStrikeView::modeCb(lv_event_t* e) {
    auto* self = static_cast<WifiStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;
    self->applyBeaconModeVisibility();
    // La configuracion se guarda sin arrancar sesion: el modo solo sustituye al
    // anterior cuando el usuario pulse Iniciar.
    WifiStrikeService::getInstance().setConfig(self->buildBeaconConfig());
    self->refreshBeaconStatus();
}

void WifiStrikeView::toggleCb(lv_event_t* e) {
    auto* self = static_cast<WifiStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    WifiStrikeService& svc = WifiStrikeService::getInstance();
    const StrikeState state = svc.getStatus().state;

    if (state == StrikeState::Faulted) {
        // Una sesion caida sigue en Faulted y el servicio no admite otra hasta
        // que se le reconoce la parada: el toque sirve de acknowledge y reinicia.
        svc.stop();
    }

    if (svc.getStatus().state != StrikeState::Idle) {
        svc.stop();
        UIManager::showToast("Emision detenida");
    } else {
        if (self->m_taBeaconSsid) UIManager::closeKeyboard();
        const StrikeConfig cfg = self->buildBeaconConfig();
        if (svc.startBeacon(cfg)) {
            UIManager::showToast("Emision de balizas iniciada");
        } else {
            UIManager::showToast(svc.getLastError().c_str());
        }
    }

    self->refreshBeaconStatus();
}

} // namespace ui
} // namespace cbdos
