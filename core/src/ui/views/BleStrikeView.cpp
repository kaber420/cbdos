#include "BleStrikeView.hpp"

#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"

#include "cbdos/ble.hpp"
#include "cbdos/ble_payloads.hpp"
#include "cbdos/ble_transmitter.hpp"

namespace cbdos {
namespace ui {

using namespace cbdos::ble;

static const lv_color_t kDanger = lv_color_hex(0xD90429);
static const lv_color_t kLive   = lv_color_hex(0x22C55E);
static const lv_color_t kCardBg = lv_color_hex(0x1E293B);

// Índice de la categoría seleccionada -> valor del enum.
static const BlePayloadCategory kCategories[] = {
    BlePayloadCategory::ApplePairing,
    BlePayloadCategory::AppleAction,
    BlePayloadCategory::AppleNotYourDevice,
    BlePayloadCategory::AndroidFastPair,
    BlePayloadCategory::WindowsSwiftPair,
    BlePayloadCategory::SamsungBuds,
    BlePayloadCategory::SamsungWatch,
    BlePayloadCategory::GenericBeacon,
    BlePayloadCategory::HidPeripherals,
    BlePayloadCategory::SpamAll,
};
static const size_t kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

// Intervalos ofrecidos en el dropdown, en ms.
static const uint16_t kIntervals[] = {20, 50, 100, 250, 500, 1000};
static const size_t kIntervalCount = sizeof(kIntervals) / sizeof(kIntervals[0]);

static const BleTxPower kPowers[] = {
    BleTxPower::Max, BleTxPower::High, BleTxPower::Medium, BleTxPower::Low
};
static const size_t kPowerCount = sizeof(kPowers) / sizeof(kPowers[0]);

static const BleMacRotationMode kMacModes[] = {
    BleMacRotationMode::Off,       BleMacRotationMode::EveryPacket,
    BleMacRotationMode::Every2,    BleMacRotationMode::Every3,
    BleMacRotationMode::Every5,    BleMacRotationMode::Every10,
    BleMacRotationMode::Every25,   BleMacRotationMode::Every50,
};
static const size_t kMacModeCount = sizeof(kMacModes) / sizeof(kMacModes[0]);

BleStrikeView::BleStrikeView()
    : BaseView("BLE Lab", ViewMode::Solid) {
}

BleStrikeView::~BleStrikeView() {
    onDestroy();
}

bool BleStrikeView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_available = cbdos::ble::isAvailable();

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    DefaultTheme::disableScroll(m_container);

    buildUi(m_container);
    return true;
}

void BleStrikeView::buildUi(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);

    buildControls(parent);
    buildStatus(parent);

    // Temporizador de refresco visual reactivo (solo repinta si m_statusDirty es true)
    m_timer = lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<BleStrikeView*>(lv_timer_get_user_data(t));
        if (self) self->onUpdate();
    }, 250, this);
}

void BleStrikeView::buildControls(lv_obj_t* parent) {
    m_scroll = lv_obj_create(parent);
    lv_obj_set_width(m_scroll, LV_PCT(100));
    lv_obj_set_flex_grow(m_scroll, 1);
    lv_obj_set_flex_flow(m_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(m_scroll, 12, 0);
    lv_obj_set_style_pad_top(m_scroll, 8, 0);
    lv_obj_set_style_pad_bottom(m_scroll, 16, 0);
    lv_obj_set_style_pad_row(m_scroll, 5, 0);
    lv_obj_set_style_bg_opa(m_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_scroll, 0, 0);
    lv_obj_set_scrollbar_mode(m_scroll, LV_SCROLLBAR_MODE_AUTO);

    auto makeSectionLabel = [this](const char* text) {
        lv_obj_t* lbl = lv_label_create(m_scroll);
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

    // --- Categoría ---
    makeSectionLabel("CATEGORÍA DE PAYLOAD");
    m_ddCategory = lv_dropdown_create(m_scroll);
    styleDropdown(m_ddCategory);
    refreshCategories();
    lv_obj_add_event_cb(m_ddCategory, BleStrikeView::categoryCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Dispositivo / Variante ---
    makeSectionLabel("DISPOSITIVO / VARIANTE");
    m_ddVariant = lv_dropdown_create(m_scroll);
    styleDropdown(m_ddVariant);
    refreshVariants();
    lv_obj_add_event_cb(m_ddVariant, BleStrikeView::variantCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Fila con Potencia TX e Intervalo lado a lado ---
    lv_obj_t* rowGrid = lv_obj_create(m_scroll);
    lv_obj_set_width(rowGrid, LV_PCT(100));
    lv_obj_set_height(rowGrid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rowGrid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowGrid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowGrid, 0, 0);
    lv_obj_set_style_bg_opa(rowGrid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowGrid, 0, 0);
    DefaultTheme::disableScroll(rowGrid);

    // Columna Potencia (48%)
    lv_obj_t* colPower = lv_obj_create(rowGrid);
    lv_obj_set_width(colPower, LV_PCT(48));
    lv_obj_set_height(colPower, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(colPower, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(colPower, 0, 0);
    lv_obj_set_style_pad_row(colPower, 4, 0);
    lv_obj_set_style_bg_opa(colPower, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colPower, 0, 0);
    DefaultTheme::disableScroll(colPower);

    lv_obj_t* lblPow = lv_label_create(colPower);
    lv_label_set_text(lblPow, "POTENCIA TX");
    lv_obj_set_style_text_font(lblPow, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPow, lv_color_hex(0x94A3B8), 0);

    m_ddPower = lv_dropdown_create(colPower);
    styleDropdown(m_ddPower);
    lv_dropdown_set_options(m_ddPower,
        "+21 dBm\n"
        "+12 dBm\n"
        "+6 dBm\n"
        "+3 dBm");
    lv_obj_add_event_cb(m_ddPower, BleStrikeView::powerCb, LV_EVENT_VALUE_CHANGED, this);

    // Columna Intervalo (48%)
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
    lv_label_set_text(lblInt, "INTERVALO");
    lv_obj_set_style_text_font(lblInt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInt, lv_color_hex(0x94A3B8), 0);

    m_ddInterval = lv_dropdown_create(colInt);
    styleDropdown(m_ddInterval);
    lv_dropdown_set_options(m_ddInterval, "20 ms\n50 ms\n100 ms\n250 ms\n500 ms\n1000 ms");
    lv_dropdown_set_selected(m_ddInterval, 2); // 100 ms por defecto
    lv_obj_add_event_cb(m_ddInterval, BleStrikeView::intervalCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Rotación MAC ---
    makeSectionLabel("ROTACIÓN DE DIRECCIÓN MAC");
    m_ddMac = lv_dropdown_create(m_scroll);
    styleDropdown(m_ddMac);
    lv_dropdown_set_options(m_ddMac,
        "Desactivada\n"
        "Cada paquete\n"
        "Cada 2 paquetes\n"
        "Cada 3 paquetes\n"
        "Cada 5 paquetes\n"
        "Cada 10 paquetes\n"
        "Cada 25 paquetes\n"
        "Cada 50 paquetes");
    lv_dropdown_set_selected(m_ddMac, 1); // "Cada paquete" por defecto
    lv_obj_add_event_cb(m_ddMac, BleStrikeView::macCb, LV_EVENT_VALUE_CHANGED, this);
}

void BleStrikeView::buildStatus(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 114);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111625), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1F293D), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 0, 0);
    lv_obj_set_style_pad_hor(card, 12, 0);
    lv_obj_set_style_pad_ver(card, 8, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(card);

    // Fila 1: LED + Estado + Contadores
    lv_obj_t* rowStatus = lv_obj_create(card);
    lv_obj_set_width(rowStatus, LV_PCT(100));
    lv_obj_set_height(rowStatus, 24);
    lv_obj_set_flex_flow(rowStatus, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowStatus, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowStatus, 0, 0);
    lv_obj_set_style_pad_column(rowStatus, 8, 0);
    lv_obj_set_style_bg_opa(rowStatus, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowStatus, 0, 0);
    DefaultTheme::disableScroll(rowStatus);

    m_ledActive = lv_led_create(rowStatus);
    lv_obj_set_size(m_ledActive, 10, 10);
    lv_led_off(m_ledActive);

    m_lblState = lv_label_create(rowStatus);
    lv_obj_set_style_text_font(m_lblState, &lv_font_montserrat_12, 0);
    lv_label_set_text(m_lblState, "EN ESPERA");

    m_lblCounters = lv_label_create(rowStatus);
    lv_obj_set_flex_grow(m_lblCounters, 1);
    lv_obj_set_style_text_align(m_lblCounters, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(m_lblCounters, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblCounters, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(m_lblCounters, "0 pkt | 0.0/s");

    // Fila 2: Botón de Toggle
    m_btnToggle = lv_button_create(card);
    lv_obj_set_width(m_btnToggle, LV_PCT(100));
    lv_obj_set_height(m_btnToggle, 42);
    lv_obj_set_style_radius(m_btnToggle, 8, 0);
    lv_obj_set_style_bg_color(m_btnToggle, lv_color_hex(0x1E293B), 0);
    lv_obj_add_event_cb(m_btnToggle, BleStrikeView::toggleCb, LV_EVENT_CLICKED, this);

    m_lblToggle = lv_label_create(m_btnToggle);
    lv_obj_set_style_text_font(m_lblToggle, &lv_font_montserrat_14, 0);
    lv_obj_center(m_lblToggle);
    lv_label_set_text(m_lblToggle, "INICIAR EMISIÓN");

    // Hint / Mensaje de error
    m_lblHint = lv_label_create(card);
    lv_obj_set_width(m_lblHint, LV_PCT(100));
    lv_obj_set_style_text_font(m_lblHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblHint, kDanger, 0);
    lv_label_set_long_mode(m_lblHint, LV_LABEL_LONG_DOT);
    lv_label_set_text(m_lblHint, "");
}

void BleStrikeView::refreshCategories() {
    std::string opts;
    for (size_t i = 0; i < kCategoryCount; ++i) {
        if (i > 0) opts += "\n";
        opts += BlePayloadRegistry::getCategoryName(kCategories[i]);
    }
    lv_dropdown_set_options(m_ddCategory, opts.c_str());
}

void BleStrikeView::refreshVariants() {
    uint32_t sel = lv_dropdown_get_selected(m_ddCategory);
    BlePayloadCategory cat = kCategories[sel < kCategoryCount ? sel : 0];

    std::vector<std::string> names = BlePayloadRegistry::getVariantNames(cat);
    std::string opts;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) opts += "\n";
        opts += names[i];
    }
    if (opts.empty()) opts = "-";

    uint32_t selected = lv_dropdown_get_selected(m_ddVariant);
    lv_dropdown_set_options(m_ddVariant, opts.c_str());

    if (selected < names.size()) {
        lv_dropdown_set_selected(m_ddVariant, selected);
    } else {
        lv_dropdown_set_selected(m_ddVariant, names.empty() ? 0 : names.size() - 1);
    }
}

void BleStrikeView::onShow() {
    BaseView::onShow();

    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle("BLE Lab");
    header.showWifi(false);
    header.showBackButton(true, []() {
        UIManager::getInstance().popView();
    });

    // Plantilla inicial: MAC cada paquete, 100ms, Random/All
    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    cfg.macRotation = BleMacRotationMode::EveryPacket;
    cfg.variantIndex = 0;
    cfg.intervalMs = 100;
    BleTransmitter::getInstance().setConfig(cfg);

    BleTransmitter::getInstance().setStatusCallback([this](const BleTxStatus&) {
        m_statusDirty.store(true);
    });

    refreshStatus();
}

void BleStrikeView::onHide() {
    BaseView::onHide();
    BleTransmitter::getInstance().setStatusCallback(nullptr);
}

void BleStrikeView::onUpdate() {
    if (m_statusDirty.exchange(false)) {
        refreshStatus();
    }
}

void BleStrikeView::refreshStatus() {
    BleTxStatus st = BleTransmitter::getInstance().getStatus();

    if (!m_available) {
        lv_label_set_text(m_lblState, "RADIO NO DISPONIBLE");
        lv_obj_set_style_text_color(m_lblState, kDanger, 0);
        lv_led_off(m_ledActive);
        lv_obj_add_state(m_btnToggle, LV_STATE_DISABLED);
        lv_label_set_text(m_lblToggle, "NO DISPONIBLE");
        return;
    }

    if (st.transmitting) {
        lv_label_set_text(m_lblState, "TRANSMITIENDO");
        lv_obj_set_style_text_color(m_lblState, kLive, 0);
        lv_led_set_color(m_ledActive, kLive);
        lv_led_on(m_ledActive);
        lv_obj_set_style_bg_color(m_btnToggle, kDanger, 0);
        lv_label_set_text(m_lblToggle, "DETENER");
    } else {
        lv_label_set_text(m_lblState, "EN ESPERA");
        lv_obj_set_style_text_color(m_lblState, lv_color_hex(0x94A3B8), 0);
        lv_led_off(m_ledActive);
        lv_obj_set_style_bg_color(m_btnToggle, lv_color_hex(0x1E293B), 0);
        lv_label_set_text(m_lblToggle, "INICIAR EMISIÓN");
    }

    char counters[64];
    snprintf(counters, sizeof(counters), "%u pkt | %.1f/s | %us",
             static_cast<unsigned>(st.stats.packetsSent),
             static_cast<double>(st.stats.packetsPerSec),
             static_cast<unsigned>(st.stats.elapsedMs / 1000));
    lv_label_set_text(m_lblCounters, counters);

    if (st.adapterState == BleAdapterState::Scanning) {
        lv_label_set_text(m_lblHint, "Escaneo activo pausado");
        lv_obj_set_style_text_color(m_lblHint, lv_color_hex(0x94A3B8), 0);
    } else if (!st.lastError.empty()) {
        lv_label_set_text(m_lblHint, st.lastError.c_str());
        lv_obj_set_style_text_color(m_lblHint, kDanger, 0);
    } else {
        lv_label_set_text(m_lblHint, "");
    }
}

void BleStrikeView::onDestroy() {
    BleTransmitter::getInstance().setStatusCallback(nullptr);

    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }

    m_scroll = nullptr;
    m_ddCategory = nullptr;
    m_ddVariant = nullptr;
    m_ddPower = nullptr;
    m_ddInterval = nullptr;
    m_ddMac = nullptr;
    m_btnToggle = nullptr;
    m_lblToggle = nullptr;
    m_lblState = nullptr;
    m_lblCounters = nullptr;
    m_lblHint = nullptr;
    m_ledActive = nullptr;

    BaseView::onDestroy();
}

// ---------------------------------------------------------------------------
// Callbacks LVGL
// ---------------------------------------------------------------------------

void BleStrikeView::categoryCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->refreshVariants();

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    uint32_t catSel = lv_dropdown_get_selected(self->m_ddCategory);
    cfg.category = kCategories[catSel < kCategoryCount ? catSel : 0];
    cfg.variantIndex = 0; // Random / All

    // Plantilla inteligente: para SwiftPair, Apple, HID o SpamAll, asegurar rotación de MAC por paquete
    if (cfg.category == BlePayloadCategory::WindowsSwiftPair ||
        cfg.category == BlePayloadCategory::ApplePairing ||
        cfg.category == BlePayloadCategory::HidPeripherals ||
        cfg.category == BlePayloadCategory::SpamAll) {
        lv_dropdown_set_selected(self->m_ddMac, 1); // Cada paquete
        cfg.macRotation = BleMacRotationMode::EveryPacket;
    }

    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::variantCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    uint32_t catSel = lv_dropdown_get_selected(self->m_ddCategory);
    cfg.category = kCategories[catSel < kCategoryCount ? catSel : 0];
    cfg.variantIndex = static_cast<int>(lv_dropdown_get_selected(self->m_ddVariant));
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::powerCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddPower);
    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    cfg.txPower = kPowers[sel < kPowerCount ? sel : 0];
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::intervalCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddInterval);
    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    cfg.intervalMs = kIntervals[sel < kIntervalCount ? sel : 0];
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::macCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddMac);
    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    cfg.macRotation = kMacModes[sel < kMacModeCount ? sel : 0];
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::toggleCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    BleTransmitter& tx = BleTransmitter::getInstance();

    if (tx.isTransmitting()) {
        tx.stop();
        UIManager::showToast("Emisión detenida");
    } else {
        BleTxConfig cfg;
        uint32_t catSel = lv_dropdown_get_selected(self->m_ddCategory);
        uint32_t varSel = lv_dropdown_get_selected(self->m_ddVariant);
        uint32_t powSel = lv_dropdown_get_selected(self->m_ddPower);
        uint32_t intSel = lv_dropdown_get_selected(self->m_ddInterval);
        uint32_t macSel = lv_dropdown_get_selected(self->m_ddMac);
        cfg.category = kCategories[catSel < kCategoryCount ? catSel : 0];
        cfg.variantIndex = static_cast<int>(varSel);
        cfg.txPower = kPowers[powSel < kPowerCount ? powSel : 0];
        cfg.intervalMs = kIntervals[intSel < kIntervalCount ? intSel : 0];
        cfg.macRotation = kMacModes[macSel < kMacModeCount ? macSel : 0];
        tx.setConfig(cfg);

        if (tx.start()) {
            UIManager::showToast("Emisión iniciada");
        } else {
            UIManager::showToast(tx.getLastError().c_str());
        }
    }

    self->m_statusDirty.store(true);
}

} // namespace ui
} // namespace cbdos