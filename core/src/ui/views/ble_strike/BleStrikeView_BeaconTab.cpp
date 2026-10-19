#include "../BleStrikeView.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"

#include "cbdos/ble.hpp"
#include "cbdos/ble_payloads.hpp"
#include "cbdos/ble_transmitter.hpp"

namespace cbdos {
namespace ui {

using namespace cbdos::ble;

static const lv_color_t kDanger = lv_color_hex(0xD90429);
static const lv_color_t kLive   = lv_color_hex(0x22C55E);
static const lv_color_t kCardBg = lv_color_hex(0x1E293B);

static const BlePayloadCategory kCategories[] = {
    BlePayloadCategory::ApplePairing,
    BlePayloadCategory::AppleAction,
    BlePayloadCategory::AppleNotYourDevice,
    BlePayloadCategory::AppleCrash,
    BlePayloadCategory::AndroidFastPair,
    BlePayloadCategory::WindowsSwiftPair,
    BlePayloadCategory::SamsungBuds,
    BlePayloadCategory::SamsungWatch,
    BlePayloadCategory::GenericBeacon,
    BlePayloadCategory::HidPeripherals,
    BlePayloadCategory::SpoofAirTag,
    BlePayloadCategory::SpamAll,
};
static const size_t kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

static const uint16_t kIntervals[] = {50, 80, 100, 200, 500, 1000};
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

static bool isAppleCategory(BlePayloadCategory cat) {
    return cat == BlePayloadCategory::ApplePairing ||
           cat == BlePayloadCategory::AppleAction ||
           cat == BlePayloadCategory::AppleNotYourDevice ||
           cat == BlePayloadCategory::AppleCrash;
}

void BleStrikeView::setupBeaconTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_style_pad_row(tab, 0, 0);

    buildBeaconControls(tab);
    buildBeaconStatus(tab);
}

void BleStrikeView::buildBeaconControls(lv_obj_t* parent) {
    m_scrollBeacon = lv_obj_create(parent);
    lv_obj_set_width(m_scrollBeacon, LV_PCT(100));
    lv_obj_set_flex_grow(m_scrollBeacon, 1);
    lv_obj_set_flex_flow(m_scrollBeacon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(m_scrollBeacon, 12, 0);
    lv_obj_set_style_pad_top(m_scrollBeacon, 8, 0);
    lv_obj_set_style_pad_bottom(m_scrollBeacon, 16, 0);
    lv_obj_set_style_pad_row(m_scrollBeacon, 5, 0);
    lv_obj_set_style_bg_opa(m_scrollBeacon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_scrollBeacon, 0, 0);
    lv_obj_set_scrollbar_mode(m_scrollBeacon, LV_SCROLLBAR_MODE_AUTO);

    auto makeSectionLabel = [this](const char* text) {
        lv_obj_t* lbl = lv_label_create(m_scrollBeacon);
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
    m_ddCategory = lv_dropdown_create(m_scrollBeacon);
    styleDropdown(m_ddCategory);
    refreshCategories();
    lv_obj_add_event_cb(m_ddCategory, BleStrikeView::categoryCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Dispositivo / Variante ---
    makeSectionLabel("DISPOSITIVO / VARIANTE");
    m_ddVariant = lv_dropdown_create(m_scrollBeacon);
    styleDropdown(m_ddVariant);
    refreshVariants();
    lv_obj_add_event_cb(m_ddVariant, BleStrikeView::variantCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Nombre Personalizado ---
    makeSectionLabel("NOMBRE ANUNCIADO (OPCIONAL)");
    m_taCustomName = lv_textarea_create(m_scrollBeacon);
    lv_obj_set_width(m_taCustomName, LV_PCT(100));
    lv_obj_set_height(m_taCustomName, 38);
    lv_textarea_set_one_line(m_taCustomName, true);
    lv_textarea_set_max_length(m_taCustomName, 21);
    lv_textarea_set_placeholder_text(m_taCustomName, "Vacio = nombres automaticos");
    lv_obj_set_style_radius(m_taCustomName, 8, 0);
    lv_obj_set_style_bg_color(m_taCustomName, kCardBg, 0);
    lv_obj_set_style_bg_opa(m_taCustomName, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(m_taCustomName, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_taCustomName, 1, 0);
    lv_obj_set_style_text_color(m_taCustomName, lv_color_white(), 0);
    lv_obj_set_style_text_font(m_taCustomName, &lv_font_montserrat_14, 0);
    UIManager::getInstance().attachKeyboard(m_taCustomName);
    lv_obj_add_event_cb(m_taCustomName, BleStrikeView::customNameCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Fila con Potencia TX e Intervalo ---
    lv_obj_t* rowGrid = lv_obj_create(m_scrollBeacon);
    lv_obj_set_width(rowGrid, LV_PCT(100));
    lv_obj_set_height(rowGrid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rowGrid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowGrid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowGrid, 0, 0);
    lv_obj_set_style_bg_opa(rowGrid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowGrid, 0, 0);
    DefaultTheme::disableScroll(rowGrid);

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
    lv_dropdown_set_selected(m_ddPower, 0);
    lv_obj_add_event_cb(m_ddPower, BleStrikeView::powerCb, LV_EVENT_VALUE_CHANGED, this);

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
    lv_label_set_text(lblInt, "VENTANA ADV");
    lv_obj_set_style_text_font(lblInt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInt, lv_color_hex(0x94A3B8), 0);

    m_ddInterval = lv_dropdown_create(colInt);
    styleDropdown(m_ddInterval);
    lv_dropdown_set_options(m_ddInterval, "50 ms\n80 ms\n100 ms\n200 ms\n500 ms\n1000 ms");
    lv_dropdown_set_selected(m_ddInterval, 3);
    lv_obj_add_event_cb(m_ddInterval, BleStrikeView::intervalCb, LV_EVENT_VALUE_CHANGED, this);

    // --- Rotación MAC ---
    makeSectionLabel("POLÍTICA DE ROTACIÓN DE MAC");
    m_ddMac = lv_dropdown_create(m_scrollBeacon);
    styleDropdown(m_ddMac);
    lv_dropdown_set_options(m_ddMac,
        "Estable / Publica (GhostESP Apple)\n"
        "Cada paquete\n"
        "Cada 2 paquetes\n"
        "Cada 3 paquetes\n"
        "Cada 5 paquetes\n"
        "Cada 10 paquetes\n"
        "Cada 25 paquetes\n"
        "Cada 50 paquetes");
    lv_dropdown_set_selected(m_ddMac, 0);
    lv_obj_add_event_cb(m_ddMac, BleStrikeView::macCb, LV_EVENT_VALUE_CHANGED, this);
}

void BleStrikeView::buildBeaconStatus(lv_obj_t* parent) {
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

    m_ledActive = lv_led_create(stateLeft);
    lv_obj_set_size(m_ledActive, 10, 10);
    lv_led_set_color(m_ledActive, kLive);
    lv_led_off(m_ledActive);

    m_lblState = lv_label_create(stateLeft);
    lv_obj_set_style_text_font(m_lblState, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblState, lv_color_white(), 0);
    lv_label_set_text(m_lblState, "DETENIDO");

    m_lblCounters = lv_label_create(rowState);
    lv_obj_set_style_text_font(m_lblCounters, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblCounters, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text(m_lblCounters, "0 pkts | 0 pps");

    m_btnToggle = lv_btn_create(panel);
    lv_obj_set_width(m_btnToggle, LV_PCT(100));
    lv_obj_set_height(m_btnToggle, 40);
    lv_obj_set_style_radius(m_btnToggle, 8, 0);
    lv_obj_set_style_bg_color(m_btnToggle, kDanger, 0);
    lv_obj_set_style_bg_opa(m_btnToggle, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(m_btnToggle, 0, 0);

    m_lblToggle = lv_label_create(m_btnToggle);
    lv_label_set_text(m_lblToggle, "INICIAR EMISIÓN");
    lv_obj_set_style_text_font(m_lblToggle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblToggle, lv_color_white(), 0);
    lv_obj_center(m_lblToggle);

    lv_obj_add_event_cb(m_btnToggle, BleStrikeView::toggleCb, LV_EVENT_CLICKED, this);

    m_lblHint = lv_label_create(panel);
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
        lv_dropdown_set_selected(m_ddVariant, 0);
    }
}

void BleStrikeView::refreshBeaconStatus() {
    if (!m_lblState || !m_btnToggle) return;

    BleTransmitter& tx = BleTransmitter::getInstance();
    BleTxStatus status = tx.getStatus();

    if (!m_available) {
        lv_label_set_text(m_lblState, "SIN HARDWARE BLE");
        lv_obj_set_style_text_color(m_lblState, lv_color_hex(0x64748B), 0);
        lv_led_off(m_ledActive);
        lv_obj_add_state(m_btnToggle, LV_STATE_DISABLED);
        lv_label_set_text(m_lblToggle, "NO DISPONIBLE");
        lv_label_set_text(m_lblHint, "Este target no tiene radio Bluetooth");
        return;
    }

    if (status.transmitting) {
        lv_label_set_text(m_lblState, "EMITIENDO...");
        lv_obj_set_style_text_color(m_lblState, kLive, 0);
        lv_led_on(m_ledActive);

        lv_label_set_text(m_lblToggle, "DETENER EMISIÓN");
        lv_obj_set_style_bg_color(m_btnToggle, lv_color_hex(0x0E7490), 0);

        char buf[64];
        snprintf(buf, sizeof(buf), "%lu pkts | %.1f pps",
                 static_cast<unsigned long>(status.stats.packetsSent),
                 status.stats.packetsPerSec);
        lv_label_set_text(m_lblCounters, buf);

        if (!status.lastError.empty()) {
            lv_label_set_text(m_lblHint, status.lastError.c_str());
        } else {
            lv_label_set_text(m_lblHint, "");
        }
    } else {
        lv_label_set_text(m_lblState, "DETENIDO");
        lv_obj_set_style_text_color(m_lblState, lv_color_white(), 0);
        lv_led_off(m_ledActive);

        lv_label_set_text(m_lblToggle, "INICIAR EMISIÓN");
        lv_obj_set_style_bg_color(m_btnToggle, kDanger, 0);

        if (status.stats.packetsSent > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Total: %lu pkts (%lu s)",
                     static_cast<unsigned long>(status.stats.packetsSent),
                     static_cast<unsigned long>(status.stats.elapsedMs / 1000));
            lv_label_set_text(m_lblCounters, buf);
        } else {
            lv_label_set_text(m_lblCounters, "0 pkts | 0 pps");
        }

        if (!status.lastError.empty()) {
            lv_label_set_text(m_lblHint, status.lastError.c_str());
        } else {
            lv_label_set_text(m_lblHint, "");
        }
    }
}

void BleStrikeView::categoryCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddCategory) return;

    self->refreshVariants();

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    uint32_t catSel = lv_dropdown_get_selected(self->m_ddCategory);
    cfg.category = kCategories[catSel < kCategoryCount ? catSel : 0];
    cfg.variantIndex = 0;

    if (isAppleCategory(cfg.category)) {
        lv_dropdown_set_selected(self->m_ddMac, 0);
        lv_dropdown_set_selected(self->m_ddInterval, 3);
        cfg.macRotation = BleMacRotationMode::Off;
        cfg.intervalMs = 200;
    } else {
        lv_dropdown_set_selected(self->m_ddMac, 1);
        lv_dropdown_set_selected(self->m_ddInterval, 1);
        cfg.macRotation = BleMacRotationMode::EveryPacket;
        cfg.intervalMs = 80;
    }

    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::variantCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddVariant) return;

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    cfg.variantIndex = static_cast<int>(lv_dropdown_get_selected(self->m_ddVariant));
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::powerCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddPower) return;

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    uint32_t powSel = lv_dropdown_get_selected(self->m_ddPower);
    cfg.txPower = kPowers[powSel < kPowerCount ? powSel : 0];
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::intervalCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddInterval) return;

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    uint32_t intSel = lv_dropdown_get_selected(self->m_ddInterval);
    cfg.intervalMs = kIntervals[intSel < kIntervalCount ? intSel : 0];
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::macCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddMac) return;

    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    uint32_t macSel = lv_dropdown_get_selected(self->m_ddMac);
    cfg.macRotation = kMacModes[macSel < kMacModeCount ? macSel : 0];
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::customNameCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taCustomName) return;

    const char* txt = lv_textarea_get_text(self->m_taCustomName);
    BleTxConfig cfg = BleTransmitter::getInstance().getConfig();
    cfg.customName = txt ? txt : "";
    BleTransmitter::getInstance().setConfig(cfg);
}

void BleStrikeView::toggleCb(lv_event_t* e) {
    auto* self = static_cast<BleStrikeView*>(lv_event_get_user_data(e));
    if (!self) return;

    BleTransmitter& tx = BleTransmitter::getInstance();

    if (tx.isTransmitting()) {
        tx.stop();
    } else {
        BleTxConfig cfg = tx.getConfig();
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
        if (self->m_taCustomName) {
            const char* txt = lv_textarea_get_text(self->m_taCustomName);
            cfg.customName = txt ? txt : "";
        }
        tx.setConfig(cfg);

        if (tx.start()) {
            lv_label_set_text(self->m_lblHint, "");
        } else {
            lv_label_set_text(self->m_lblHint, tx.getLastError().c_str());
        }
    }

    self->m_statusDirty = true;
    self->refreshBeaconStatus();
}

} // namespace ui
} // namespace cbdos
