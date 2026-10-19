#include "WifiStrikeView.hpp"
#include "../UIManager.hpp"
#include "../../security/WifiShieldService.hpp"
#include "../../security/WifiStrikeService.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace cbdos {
namespace ui {

using security::WifiShieldService;
using security::WifiStrikeService;
using wifi::BeaconMode;
using wifi::MgmtDirection;
using wifi::MgmtReason;
using wifi::MgmtStrikeConfig;
using wifi::MgmtSubtype;
using wifi::MgmtTarget;
using radio::WifiApRecord;
using wifi::StrikeConfig;
using wifi::StrikeKind;
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

// Orden del dropdown de la UI -> MgmtSubtype del core.
static const MgmtSubtype kMgmtSubtypes[] = {
    MgmtSubtype::Deauth,
    MgmtSubtype::Disassoc,
    MgmtSubtype::Mixed,
};
static const size_t kMgmtSubtypeCount = sizeof(kMgmtSubtypes) / sizeof(kMgmtSubtypes[0]);

static const MgmtDirection kMgmtDirections[] = {
    MgmtDirection::ApToSta,
    MgmtDirection::StaToAp,
    MgmtDirection::Both,
};
static const size_t kMgmtDirectionCount = sizeof(kMgmtDirections) / sizeof(kMgmtDirections[0]);

// Motivos de desasociacion ofrecidos en el desplegable. El primero es el 0x0007
// de GhostESP/Bruce: el que un cliente WPA2 legitimo acepta sin discusion.
static const MgmtReason kMgmtReasons[] = {
    MgmtReason::NotAuthed,
    MgmtReason::Unspecified,
    MgmtReason::PreviousAuthNotValid,
    MgmtReason::LeavingOrDisabling,
    MgmtReason::Inactivity,
    MgmtReason::ApOverload,
    MgmtReason::SasFailure,
    MgmtReason::BadAuthState,
    MgmtReason::AuthTimeout,
};
static const size_t kMgmtReasonCount = sizeof(kMgmtReasons) / sizeof(kMgmtReasons[0]);

// Cadencia del refresco visual de las pestanas.
static constexpr uint32_t kBeaconRefreshMs = 250;

// SSID de reserva cuando el modo es SSID Fijo y el campo queda vacio: sin el,
// validate() rechazaria la sesion.
static const char* kFallbackSsid = "cbdos-Beacon";

// SSID de reserva para el objetivo sin nombre de las APs ocultas del Air Recon.
static const char* kHiddenSsid = "<oculta>";

/** Formatea una MAC de 6 bytes en la notacion habitual de un analizador. */
static std::string formatMac(const uint8_t mac[6]) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

/** Une las entradas de un desplegable de LVGL (separador \n). */
static std::string joinLines(const std::vector<std::string>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out += '\n';
        out += items[i];
    }
    return out;
}

/** Aplica el aspecto comun a los desplegables de la vista. */
static void styleDropdown(lv_obj_t* dd) {
    lv_obj_set_width(dd, LV_PCT(100));
    lv_obj_set_height(dd, 40);
    lv_obj_set_style_radius(dd, 8, 0);
    lv_obj_set_style_bg_color(dd, kCardBg, 0);
    lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dd, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(dd, 1, 0);
    lv_obj_set_style_text_color(dd, lv_color_white(), 0);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
}

// Los dos targets del proyecto no compilan contra la misma version de LVGL: la
// P4 fija el componente 9.5.0 de ESP-IDF y la S3 resuelve lib_deps contra la
// 9.6.x. El setter dedicado lv_obj_set_hidden()/lv_obj_is_hidden() solo existe
// desde la 9.6, asi que en la 9.5 el estado de visibilidad se sigue managing con
// el flag, que es la unica sintaxis comun a las dos ramas.
#if LV_VERSION_CHECK(9, 6, 0)
static void setHidden(lv_obj_t* obj, bool hidden) {
    lv_obj_set_hidden(obj, hidden);
}

static bool isHidden(const lv_obj_t* obj) {
    return lv_obj_is_hidden(obj);
}
#else
static void setHidden(lv_obj_t* obj, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static bool isHidden(const lv_obj_t* obj) {
    return lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
#endif

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
    m_lblDeauthStatus = nullptr;
    m_ddDeauthSubtype = nullptr;
    m_ddDeauthDirection = nullptr;
    m_ddDeauthReason = nullptr;
    m_ddDeauthTarget = nullptr;
    m_lblDeauthTargets = nullptr;
    m_lblDeauthFrames = nullptr;
    m_lblDeauthTargeted = nullptr;
    m_lblDeauthChannel = nullptr;
    m_lblDeauthFailed = nullptr;
    m_ledDeauth = nullptr;
    m_lblDeauthState = nullptr;
    m_lblDeauthHint = nullptr;
    m_lblDeauthLog = nullptr;
    m_btnDeauthToggle = nullptr;
    m_lblDeauthToggle = nullptr;
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
            self->refreshDeauthStatus();
        }, kBeaconRefreshMs, this);
    } else {
        lv_timer_resume(m_beaconTimer);
    }

    m_statusPending.store(true);
    refreshBeaconStatus();
    refreshDeauthStatus();
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
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblDeauthStatus = lv_label_create(tab);
    lv_label_set_text(m_lblDeauthStatus,
                      "Deauth: evalua la resiliencia de la gestion 802.11");
    lv_obj_set_style_text_color(m_lblDeauthStatus, lv_color_hex(0xE63946), 0);
    lv_obj_set_width(m_lblDeauthStatus, LV_PCT(100));
    lv_obj_set_style_text_font(m_lblDeauthStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_bottom(m_lblDeauthStatus, 6, 0);

    const MgmtStrikeConfig cfg = WifiStrikeService::getInstance().getMgmtConfig();

    // --- Subtipo de trama de gestion ---
    lv_obj_t* lblSubtypeCaption = lv_label_create(tab);
    lv_label_set_text(lblSubtypeCaption, "TRAMA DE GESTION");
    lv_obj_set_style_text_font(lblSubtypeCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblSubtypeCaption, kMuted, 0);

    m_ddDeauthSubtype = lv_dropdown_create(tab);
    styleDropdown(m_ddDeauthSubtype);
    {
        std::vector<std::string> options;
        for (size_t i = 0; i < kMgmtSubtypeCount; ++i) {
            options.push_back(wifi::getMgmtSubtypeName(kMgmtSubtypes[i]));
        }
        const std::string text = joinLines(options);
        lv_dropdown_set_options(m_ddDeauthSubtype, text.c_str());
        for (size_t i = 0; i < kMgmtSubtypeCount; ++i) {
            if (kMgmtSubtypes[i] == cfg.subtype) {
                lv_dropdown_set_selected(m_ddDeauthSubtype, static_cast<uint16_t>(i));
                break;
            }
        }
    }
    lv_obj_add_event_cb(m_ddDeauthSubtype, WifiStrikeView::deauthControlCb,
                        LV_EVENT_VALUE_CHANGED, this);

    // --- Sentido de la inyeccion ---
    lv_obj_t* lblDirectionCaption = lv_label_create(tab);
    lv_label_set_text(lblDirectionCaption, "SENTIDO DE LA SUPLANTACION");
    lv_obj_set_style_text_font(lblDirectionCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDirectionCaption, kMuted, 0);

    m_ddDeauthDirection = lv_dropdown_create(tab);
    styleDropdown(m_ddDeauthDirection);
    {
        std::vector<std::string> options;
        for (size_t i = 0; i < kMgmtDirectionCount; ++i) {
            options.push_back(wifi::getMgmtDirectionName(kMgmtDirections[i]));
        }
        const std::string text = joinLines(options);
        lv_dropdown_set_options(m_ddDeauthDirection, text.c_str());
        for (size_t i = 0; i < kMgmtDirectionCount; ++i) {
            if (kMgmtDirections[i] == cfg.direction) {
                lv_dropdown_set_selected(m_ddDeauthDirection, static_cast<uint16_t>(i));
                break;
            }
        }
    }
    lv_obj_add_event_cb(m_ddDeauthDirection, WifiStrikeView::deauthControlCb,
                        LV_EVENT_VALUE_CHANGED, this);

    // --- Codigo de razon ---
    lv_obj_t* lblReasonCaption = lv_label_create(tab);
    lv_label_set_text(lblReasonCaption, "CODIGO DE RAZON");
    lv_obj_set_style_text_font(lblReasonCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblReasonCaption, kMuted, 0);

    m_ddDeauthReason = lv_dropdown_create(tab);
    styleDropdown(m_ddDeauthReason);
    {
        std::vector<std::string> options;
        for (size_t i = 0; i < kMgmtReasonCount; ++i) {
            options.push_back(wifi::getMgmtReasonName(kMgmtReasons[i]));
        }
        const std::string text = joinLines(options);
        lv_dropdown_set_options(m_ddDeauthReason, text.c_str());
        for (size_t i = 0; i < kMgmtReasonCount; ++i) {
            if (kMgmtReasons[i] == cfg.reason) {
                lv_dropdown_set_selected(m_ddDeauthReason, static_cast<uint16_t>(i));
                break;
            }
        }
    }
    lv_obj_add_event_cb(m_ddDeauthReason, WifiStrikeView::deauthControlCb,
                        LV_EVENT_VALUE_CHANGED, this);

    // --- Tarjeta Objetivo y Air Recon ---
    lv_obj_t* cardRecon = lv_obj_create(tab);
    lv_obj_set_width(cardRecon, LV_PCT(100));
    lv_obj_set_height(cardRecon, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(cardRecon, kCardBg, 0);
    lv_obj_set_style_bg_opa(cardRecon, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cardRecon, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(cardRecon, 1, 0);
    lv_obj_set_style_radius(cardRecon, 8, 0);
    lv_obj_set_style_pad_all(cardRecon, 8, 0);
    lv_obj_set_style_pad_row(cardRecon, 6, 0);
    lv_obj_set_flex_flow(cardRecon, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lblTargetCaption = lv_label_create(cardRecon);
    lv_label_set_text(lblTargetCaption, "OBJETIVO DE INYECCION");
    lv_obj_set_style_text_font(lblTargetCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTargetCaption, kMuted, 0);

    m_ddDeauthTarget = lv_dropdown_create(cardRecon);
    styleDropdown(m_ddDeauthTarget);
    m_deauthApCount = 0;   // fuerza el primer relleno con el estado real
    syncDeauthTargets();
    lv_obj_add_event_cb(m_ddDeauthTarget, WifiStrikeView::deauthControlCb,
                        LV_EVENT_VALUE_CHANGED, this);

    // Fila del Slider de duracion de escaneo
    lv_obj_t* rowDuration = lv_obj_create(cardRecon);
    lv_obj_set_size(rowDuration, LV_PCT(100), 20);
    lv_obj_set_style_bg_opa(rowDuration, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowDuration, 0, 0);
    lv_obj_set_style_pad_all(rowDuration, 0, 0);
    lv_obj_set_flex_flow(rowDuration, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowDuration, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblDurCaption = lv_label_create(rowDuration);
    lv_label_set_text(lblDurCaption, "Duracion escaneo:");
    lv_obj_set_style_text_font(lblDurCaption, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDurCaption, kMuted, 0);

    m_lblScanDuration = lv_label_create(rowDuration);
    lv_label_set_text(m_lblScanDuration, "3 s");
    lv_obj_set_style_text_font(m_lblScanDuration, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblScanDuration, lv_color_hex(0x06B6D4), 0);

    m_sliderScanDuration = lv_slider_create(cardRecon);
    lv_obj_set_width(m_sliderScanDuration, LV_PCT(100));
    lv_obj_set_height(m_sliderScanDuration, 10);
    lv_slider_set_range(m_sliderScanDuration, 1, 10);
    lv_slider_set_value(m_sliderScanDuration, 3, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_sliderScanDuration, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(m_sliderScanDuration, lv_color_hex(0x06B6D4), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(m_sliderScanDuration, lv_color_hex(0x06B6D4), LV_PART_KNOB);
    lv_obj_add_event_cb(m_sliderScanDuration, WifiStrikeView::scanSliderCb,
                        LV_EVENT_VALUE_CHANGED, this);

    // Boton de disparo One-Shot para Air Recon
    m_btnScanNow = lv_button_create(cardRecon);
    lv_obj_set_size(m_btnScanNow, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(m_btnScanNow, lv_color_hex(0x0369A1), 0);
    lv_obj_set_style_radius(m_btnScanNow, 6, 0);
    lv_obj_add_event_cb(m_btnScanNow, WifiStrikeView::scanBtnCb,
                        LV_EVENT_CLICKED, this);

    m_lblScanNow = lv_label_create(m_btnScanNow);
    lv_obj_set_style_text_font(m_lblScanNow, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblScanNow, lv_color_white(), 0);
    lv_obj_center(m_lblScanNow);
    lv_label_set_text(m_lblScanNow, "🔍 Escanear Redes (Air Recon)");

    m_lblDeauthTargets = lv_label_create(cardRecon);
    lv_obj_set_width(m_lblDeauthTargets, LV_PCT(100));
    lv_label_set_text(m_lblDeauthTargets, "APs detectadas: 0");
    lv_obj_set_style_text_font(m_lblDeauthTargets, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeauthTargets, kMuted, 0);

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

    makeMetric("Tramas emitidas", &m_lblDeauthFrames);
    makeMetric("Dirigidas al objetivo", &m_lblDeauthTargeted);
    makeMetric("Canal actual", &m_lblDeauthChannel);
    makeMetric("Fallos", &m_lblDeauthFailed);

    // Fila de estado: LED + nombre del estado de la maquina de emision.
    lv_obj_t* rowState = lv_obj_create(card);
    lv_obj_set_size(rowState, LV_PCT(100), 24);
    lv_obj_set_style_bg_opa(rowState, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowState, 0, 0);
    lv_obj_set_style_pad_all(rowState, 0, 0);
    lv_obj_set_style_pad_column(rowState, 8, 0);
    lv_obj_set_flex_flow(rowState, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowState, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_ledDeauth = lv_led_create(rowState);
    lv_obj_set_size(m_ledDeauth, 10, 10);
    lv_led_set_color(m_ledDeauth, kLive);
    lv_led_off(m_ledDeauth);

    m_lblDeauthState = lv_label_create(rowState);
    lv_label_set_text(m_lblDeauthState, wifi::getStrikeStateName(StrikeState::Idle));
    lv_obj_set_style_text_font(m_lblDeauthState, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeauthState, kMuted, 0);

    m_lblDeauthHint = lv_label_create(card);
    lv_obj_set_width(m_lblDeauthHint, LV_PCT(100));
    lv_label_set_long_mode(m_lblDeauthHint, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(m_lblDeauthHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeauthHint, lv_color_hex(0xE63946), 0);
    lv_label_set_text(m_lblDeauthHint, "");

    // --- Log de sesion ---
    m_lblDeauthLog = lv_label_create(card);
    lv_obj_set_width(m_lblDeauthLog, LV_PCT(100));
    lv_label_set_long_mode(m_lblDeauthLog, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(m_lblDeauthLog, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeauthLog, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(m_lblDeauthLog, "");

    // --- Boton Iniciar / Detener ---
    m_btnDeauthToggle = lv_button_create(tab);
    lv_obj_set_size(m_btnDeauthToggle, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(m_btnDeauthToggle, kStart, 0);
    lv_obj_set_style_radius(m_btnDeauthToggle, 8, 0);
    lv_obj_add_event_cb(m_btnDeauthToggle, WifiStrikeView::deauthToggleCb,
                        LV_EVENT_CLICKED, this);

    m_lblDeauthToggle = lv_label_create(m_btnDeauthToggle);
    lv_obj_set_style_text_font(m_lblDeauthToggle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblDeauthToggle, lv_color_white(), 0);
    lv_obj_center(m_lblDeauthToggle);
    lv_label_set_text(m_lblDeauthToggle, "INICIAR INYECCION");
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
        setHidden(m_ssidRow, mode != BeaconMode::SingleSsid);
    }
    if (m_cloneRow) {
        setHidden(m_cloneRow, mode != BeaconMode::CloneAccessPoints);
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

    if (m_lblCloneInfo && m_cloneRow && !isHidden(m_cloneRow)) {
        snprintf(buf, sizeof(buf), "APs en Air Recon: %u",
                 static_cast<unsigned>(WifiShieldService::getInstance().getAccessPointCount()));
        lv_label_set_text(m_lblCloneInfo, buf);
    }

    const bool cloning = m_cloneRow && !isHidden(m_cloneRow);
    const bool foreignSession = (status.state != StrikeState::Idle) &&
                                (status.kind != StrikeKind::Beacon);
    if (faulted && !status.lastError.empty()) {
        lv_label_set_text(m_lblBeaconHint, status.lastError.c_str());
    } else if (cloning && WifiShieldService::getInstance().getAccessPointCount() == 0) {
        lv_label_set_text(m_lblBeaconHint, "Sin APs en Air Recon: abre Wi-Fi Shield y escanea");
    } else if (foreignSession) {
        // La radio es un recurso unico: la otra pestana esta emitiendo y este
        // boton no puede arrancar nada hasta que se detenga.
        lv_label_set_text(m_lblBeaconHint, "Sesion activa en la pestana Deauth");
    } else {
        lv_label_set_text(m_lblBeaconHint, "");
    }

    if (foreignSession) {
        lv_obj_add_state(m_btnBeaconToggle, LV_STATE_DISABLED);
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
    self->refreshDeauthStatus();
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
    self->refreshDeauthStatus();
}

// ───────────────────────────────────────────────────────────────────────────────
// Pestana Deauth / Disassoc
// ───────────────────────────────────────────────────────────────────────────────

void WifiStrikeView::syncDeauthTargets() {
    if (!m_ddDeauthTarget) return;

    std::vector<WifiApRecord> aps = WifiShieldService::getInstance().getAccessPoints();
    // El Air Recon no ordena por potencia y la mas cercana es la que mas sentido
    // tiene como objetivo: se ordena aqui, en el snapshot, sin tocar el servicio.
    std::sort(aps.begin(), aps.end(),
              [](const WifiApRecord& a, const WifiApRecord& b) { return a.rssi > b.rssi; });
    if (aps.size() > wifi::STRIKE_AP_LIST_MAX) aps.resize(wifi::STRIKE_AP_LIST_MAX);

    std::vector<std::string> options;
    options.push_back("Broadcast (todo el espectro)");
    for (const WifiApRecord& ap : aps) {
        const std::string ssid = ap.ssid.empty() ? std::string(kHiddenSsid) : ap.ssid;
        char tail[24];
        snprintf(tail, sizeof(tail), " | ch%u", static_cast<unsigned>(ap.channel));
        options.push_back(ssid + " " + formatMac(ap.bssid) + tail);
    }

    const std::string text = joinLines(options);
    lv_dropdown_set_options(m_ddDeauthTarget, text.c_str());

    // La seleccion se conserva por BSSID: el indice cambia en cuanto el Air
    // Recon descubre una AP mas fuerte, pero el objetivo elegido es el mismo.
    uint16_t selected = 0;
    if (!m_deauthSelectedBssid.empty()) {
        for (size_t i = 0; i < aps.size(); ++i) {
            if (formatMac(aps[i].bssid) == m_deauthSelectedBssid) {
                selected = static_cast<uint16_t>(i + 1);
                break;
            }
        }
    }
    lv_dropdown_set_selected(m_ddDeauthTarget, selected);
    m_deauthApCount = aps.size();
}

MgmtStrikeConfig WifiStrikeView::buildDeauthConfig() const {
    MgmtStrikeConfig cfg = WifiStrikeService::getInstance().getMgmtConfig();

    const uint32_t selSubtype = m_ddDeauthSubtype ? lv_dropdown_get_selected(m_ddDeauthSubtype) : 0;
    cfg.subtype = kMgmtSubtypes[selSubtype < kMgmtSubtypeCount ? selSubtype : 0];

    const uint32_t selDirection = m_ddDeauthDirection ? lv_dropdown_get_selected(m_ddDeauthDirection) : 0;
    cfg.direction = kMgmtDirections[selDirection < kMgmtDirectionCount ? selDirection : 0];

    const uint32_t selReason = m_ddDeauthReason ? lv_dropdown_get_selected(m_ddDeauthReason) : 0;
    cfg.reason = kMgmtReasons[selReason < kMgmtReasonCount ? selReason : 0];

    std::vector<WifiApRecord> aps = WifiShieldService::getInstance().getAccessPoints();
    std::sort(aps.begin(), aps.end(),
              [](const WifiApRecord& a, const WifiApRecord& b) { return a.rssi > b.rssi; });
    if (aps.size() > wifi::STRIKE_AP_LIST_MAX) aps.resize(wifi::STRIKE_AP_LIST_MAX);

    const uint32_t selTarget = m_ddDeauthTarget ? lv_dropdown_get_selected(m_ddDeauthTarget) : 0;
    if (selTarget == 0 || selTarget > aps.size()) {
        // Indice 0 = broadcast. Sin objetivos el aire, la razon se genera y se
        // suplanta en cada trama: es una inundacion, no un ataque dirigido.
        cfg.target = MgmtTarget::Broadcast;
        cfg.apList.clear();
    } else {
        const WifiApRecord& chosen = aps[selTarget - 1];
        cfg.target = MgmtTarget::AccessPoints;
        // Una unica AP: la sesion se emite enteramente contra ella, con la
        // intensidad que el usuario eligio, en su canal real.
        cfg.apList.clear();
        cfg.apList.push_back(chosen);
    }

    return cfg;
}

void WifiStrikeView::refreshDeauthStatus() {
    if (!m_lblDeauthState || !m_btnDeauthToggle) return;

    auto& shield = WifiShieldService::getInstance();
    const bool scanning = shield.isScanning();
    const uint32_t currentRev = shield.getScanRevision();

    // Sincronizacion reactiva segura en el hilo de LVGL cuando finaliza el escaneo
    if (m_isScanningAirRecon && !scanning) {
        m_isScanningAirRecon = false;
        if (m_btnScanNow) {
            lv_obj_set_style_bg_color(m_btnScanNow, lv_color_hex(0x0369A1), 0);
        }
        if (m_lblScanNow) {
            lv_label_set_text(m_lblScanNow, "🔍 Escanear Redes (Air Recon)");
        }
        syncDeauthTargets();
        char toast[64];
        snprintf(toast, sizeof(toast), "Escaneo listo: %u APs detectadas", static_cast<unsigned>(m_deauthApCount));
        UIManager::showToast(toast);
    } else if (currentRev != m_lastScanRevision) {
        m_lastScanRevision = currentRev;
        syncDeauthTargets();
    }

    WifiStrikeService& svc = WifiStrikeService::getInstance();
    const StrikeStatus status = svc.getStatus();

    char buf[48];
    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(status.stats.framesSent));
    lv_label_set_text(m_lblDeauthFrames, buf);

    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(status.stats.framesTargeted));
    lv_label_set_text(m_lblDeauthTargeted, buf);

    if (status.stats.lastChannel != 0) {
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(status.stats.lastChannel));
    } else {
        snprintf(buf, sizeof(buf), "-");
    }
    lv_label_set_text(m_lblDeauthChannel, buf);

    snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(status.stats.framesFailed));
    lv_label_set_text(m_lblDeauthFailed, buf);

    snprintf(buf, sizeof(buf), "APs detectadas: %u", static_cast<unsigned>(m_deauthApCount));
    lv_label_set_text(m_lblDeauthTargets, buf);

    const bool emitting = (status.state == StrikeState::Running) &&
                          (status.kind == StrikeKind::Mgmt);
    const bool faulted = (status.state == StrikeState::Faulted) &&
                         (status.kind == StrikeKind::Mgmt);

    lv_label_set_text(m_lblDeauthState, wifi::getStrikeStateName(status.state));
    lv_obj_set_style_text_color(m_lblDeauthState,
        faulted ? lv_color_hex(0xE63946) : (emitting ? kLive : kMuted), 0);

    if (m_ledDeauth) {
        if (emitting) {
            lv_led_on(m_ledDeauth);
        } else {
            lv_led_off(m_ledDeauth);
        }
    }

    lv_obj_set_style_bg_color(m_btnDeauthToggle, emitting ? kStop : kStart, 0);
    lv_label_set_text(m_lblDeauthToggle, emitting ? "DETENER" : "INICIAR INYECCION");

    // La radio es un recurso unico: si la otra pestana esta emitiendo, este
    // boton no puede arrancar nada.
    const bool foreignSession = (status.state != StrikeState::Idle) &&
                                (status.kind != StrikeKind::Mgmt);
    if (status.state == StrikeState::Stopping || foreignSession || (scanning && !emitting)) {
        lv_obj_add_state(m_btnDeauthToggle, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(m_btnDeauthToggle, LV_STATE_DISABLED);
    }

    // Gestion de estados de los widgets de escaneo segun la radio
    const bool strikeBusy = (status.state != StrikeState::Idle);
    if (m_btnScanNow) {
        if (strikeBusy && !scanning) {
            lv_obj_add_state(m_btnScanNow, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(m_btnScanNow, LV_STATE_DISABLED);
        }
    }
    if (m_sliderScanDuration) {
        if (strikeBusy || scanning) {
            lv_obj_add_state(m_sliderScanDuration, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(m_sliderScanDuration, LV_STATE_DISABLED);
        }
    }

    if (faulted && !status.lastError.empty()) {
        lv_label_set_text(m_lblDeauthHint, status.lastError.c_str());
    } else if (m_ddDeauthTarget && lv_dropdown_get_selected(m_ddDeauthTarget) > 0 &&
               m_deauthApCount == 0) {
        lv_label_set_text(m_lblDeauthHint, "El objetivo ya no esta en el Air Recon");
    } else if (foreignSession) {
        lv_label_set_text(m_lblDeauthHint, "Sesion activa en la pestana Beacon");
    } else if (scanning) {
        lv_label_set_text(m_lblDeauthHint, "Escaneo Air Recon en progreso...");
    } else {
        lv_label_set_text(m_lblDeauthHint, "");
    }

    if (m_lblDeauthLog) {
        std::string log;
        for (const std::string& line : status.log) {
            if (!log.empty()) log += '\n';
            log += line;
        }
        lv_label_set_text(m_lblDeauthLog, log.c_str());
    }
}

void WifiStrikeView::scanSliderCb(lv_event_t* e) {
    auto* self = static_cast<WifiStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_sliderScanDuration || !self->m_lblScanDuration) return;

    int32_t val = lv_slider_get_value(self->m_sliderScanDuration);
    if (val < 1) val = 1;
    if (val > 10) val = 10;
    self->m_scanDurationSec = static_cast<uint32_t>(val);

    char buf[16];
    snprintf(buf, sizeof(buf), "%ld s", static_cast<long>(val));
    lv_label_set_text(self->m_lblScanDuration, buf);
}

void WifiStrikeView::scanBtnCb(lv_event_t* e) {
    auto* self = static_cast<WifiStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    auto& strike = WifiStrikeService::getInstance();
    if (strike.isActive()) {
        UIManager::showToast("Deten la emision antes de escanear");
        return;
    }

    auto& shield = WifiShieldService::getInstance();
    if (shield.isScanning()) {
        shield.stopScan();
        self->m_isScanningAirRecon = false;
        if (self->m_btnScanNow) {
            lv_obj_set_style_bg_color(self->m_btnScanNow, lv_color_hex(0x0369A1), 0);
        }
        if (self->m_lblScanNow) {
            lv_label_set_text(self->m_lblScanNow, "🔍 Escanear Redes (Air Recon)");
        }
        UIManager::showToast("Escaneo cancelado");
        return;
    }

    if (shield.startScan(self->m_scanDurationSec)) {
        self->m_isScanningAirRecon = true;
        if (self->m_btnScanNow) {
            lv_obj_set_style_bg_color(self->m_btnScanNow, lv_color_hex(0xD97706), 0);
        }
        if (self->m_lblScanNow) {
            char buf[48];
            snprintf(buf, sizeof(buf), "⏳ Buscando (%u s)...", static_cast<unsigned>(self->m_scanDurationSec));
            lv_label_set_text(self->m_lblScanNow, buf);
        }
        UIManager::showToast("Escaneando espectro Wi-Fi...");
    } else {
        UIManager::showToast("No se pudo iniciar el escaneo");
    }
}

void WifiStrikeView::deauthControlCb(lv_event_t* e) {
    auto* self = static_cast<WifiStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    // El desplegable de objetivos es el unico cuyo indice depende de una lista
    // externa, asi que es el que hay que traducir a un BSSID estable.
    if (lv_event_get_target(e) == self->m_ddDeauthTarget) {
        const MgmtStrikeConfig cfg = self->buildDeauthConfig();
        if (cfg.target == MgmtTarget::AccessPoints && !cfg.apList.empty()) {
            self->m_deauthSelectedBssid = formatMac(cfg.apList.front().bssid);
        } else {
            self->m_deauthSelectedBssid.clear();
        }
    }

    // La configuracion se guarda sin arrancar sesion: los controles solo
    // sustituyen a los anteriores cuando el usuario pulse Iniciar.
    WifiStrikeService::getInstance().setMgmtConfig(self->buildDeauthConfig());
    self->refreshDeauthStatus();
}

void WifiStrikeView::deauthToggleCb(lv_event_t* e) {
    auto* self = static_cast<WifiStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    WifiStrikeService& svc = WifiStrikeService::getInstance();
    const StrikeStatus before = svc.getStatus();

    if (before.state == StrikeState::Faulted) {
        // Una sesion caida sigue en Faulted y el servicio no admite otra hasta
        // que se le reconoce la parada: el toque sirve de acknowledge y reinicia.
        svc.stop();
    }

    const StrikeStatus now = svc.getStatus();
    if (now.state != StrikeState::Idle) {
        svc.stop();
        UIManager::showToast("Inyeccion detenida");
    } else {
        const MgmtStrikeConfig cfg = self->buildDeauthConfig();
        if (svc.startMgmt(cfg)) {
            UIManager::showToast("Inyeccion de tramas de gestion iniciada");
        } else {
            UIManager::showToast(svc.getLastError().c_str());
        }
    }

    self->refreshBeaconStatus();
    self->refreshDeauthStatus();
}

} // namespace ui
} // namespace cbdos
