#include "BleShieldView.hpp"
#include "../UIManager.hpp"
#include "../../security/BleShieldService.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

BleShieldView::BleShieldView()
    : BaseView("BLE Shield", ViewMode::Solid) {
}

BleShieldView::~BleShieldView() {
    onDestroy();
}

bool BleShieldView::onCreate(lv_obj_t* parent) {
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

void BleShieldView::onDestroy() {
    security::BleShieldService::getInstance().setDeviceCallback(nullptr);
    if (m_refreshTimer) {
        lv_timer_delete(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
    BaseView::onDestroy();
}

void BleShieldView::onShow() {
    BaseView::onShow();
    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle("BLE Shield");
    header.showWifi(true);
    header.showBackButton(true, []() {
        UIManager::getInstance().popView();
    });

    // Registrar callback de actualizacion reactiva de dispositivos
    security::BleShieldService::getInstance().setDeviceCallback([this](const radio::BleDeviceRecord&) {
        m_devicesDirty = true;
    });

    // Iniciar timer de refresco táctico a 4 Hz (cada 250 ms)
    if (!m_refreshTimer) {
        m_refreshTimer = lv_timer_create([](lv_timer_t* t) {
            auto* view = static_cast<BleShieldView*>(lv_timer_get_user_data(t));
            if (view) {
                if (view->m_devicesDirty.exchange(false)) {
                    view->refreshDeviceList();
                }
                view->updateGeigerDisplay();
                view->updateSpamDisplay();
            }
        }, 250, this);
    } else {
        lv_timer_resume(m_refreshTimer);
    }

    refreshDeviceList();
}

void BleShieldView::onHide() {
    BaseView::onHide();
    security::BleShieldService::getInstance().setDeviceCallback(nullptr);
    if (m_refreshTimer) {
        lv_timer_pause(m_refreshTimer);
    }
}


void BleShieldView::createTabs(lv_obj_t* parent) {
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

    lv_obj_t* tabAirRecon = lv_tabview_add_tab(m_tabView, "Air Recon");
    lv_obj_t* tabGeiger = lv_tabview_add_tab(m_tabView, "Geiger Tracker");
    lv_obj_t* tabSpamIds = lv_tabview_add_tab(m_tabView, "Spam IDS");

    setupAirReconTab(tabAirRecon);
    setupGeigerTab(tabGeiger);
    setupSpamIdsTab(tabSpamIds);
}

void BleShieldView::setupAirReconTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Barra superior: Métricas y acciones
    lv_obj_t* topBar = lv_obj_create(tab);
    lv_obj_set_size(topBar, LV_PCT(100), 42);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(0x111625), 0);
    lv_obj_set_style_border_color(topBar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(topBar, 1, 0);
    lv_obj_set_style_radius(topBar, 6, 0);
    lv_obj_set_style_pad_all(topBar, 4, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblDeviceCount = lv_label_create(topBar);
    lv_label_set_text(m_lblDeviceCount, "Dispositivos: 0");
    lv_obj_set_style_text_color(m_lblDeviceCount, lv_color_hex(0x00F5D4), 0);

    lv_obj_t* actionsBox = lv_obj_create(topBar);
    lv_obj_set_size(actionsBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_opa(actionsBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actionsBox, 0, 0);
    lv_obj_set_style_pad_all(actionsBox, 0, 0);
    lv_obj_set_flex_flow(actionsBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actionsBox, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Iniciar Escáner
    lv_obj_t* btnStart = lv_btn_create(actionsBox);
    lv_obj_set_size(btnStart, 75, 30);
    lv_obj_set_style_bg_color(btnStart, lv_color_hex(0x064E3B), 0);
    lv_obj_set_style_radius(btnStart, 4, 0);
    lv_obj_t* lblStart = lv_label_create(btnStart);
    lv_label_set_text(lblStart, LV_SYMBOL_PLAY " Iniciar");
    lv_obj_center(lblStart);
    lv_obj_add_event_cb(btnStart, [](lv_event_t* e) {
        security::BleShieldService::getInstance().startScan();
        auto* view = static_cast<BleShieldView*>(lv_event_get_user_data(e));
        if (view) view->refreshDeviceList();
    }, LV_EVENT_CLICKED, this);

    // Botón Detener Escáner
    lv_obj_t* btnStop = lv_btn_create(actionsBox);
    lv_obj_set_size(btnStop, 75, 30);
    lv_obj_set_style_bg_color(btnStop, lv_color_hex(0x4C0519), 0);
    lv_obj_set_style_radius(btnStop, 4, 0);
    lv_obj_t* lblStop = lv_label_create(btnStop);
    lv_label_set_text(lblStop, LV_SYMBOL_STOP " Detener");
    lv_obj_center(lblStop);
    lv_obj_add_event_cb(btnStop, [](lv_event_t* e) {
        security::BleShieldService::getInstance().stopScan();
        auto* view = static_cast<BleShieldView*>(lv_event_get_user_data(e));
        if (view) view->refreshDeviceList();
    }, LV_EVENT_CLICKED, this);

    // Botón Limpiar
    lv_obj_t* btnClear = lv_btn_create(actionsBox);
    lv_obj_set_size(btnClear, 65, 30);
    lv_obj_set_style_bg_color(btnClear, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(btnClear, 4, 0);
    lv_obj_t* lblClear = lv_label_create(btnClear);
    lv_label_set_text(lblClear, "Limpiar");
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(btnClear, [](lv_event_t* e) {
        security::BleShieldService::getInstance().clearDevices();
        auto* view = static_cast<BleShieldView*>(lv_event_get_user_data(e));
        if (view) view->refreshDeviceList();
    }, LV_EVENT_CLICKED, this);

    // Barra de chips de filtrado táctil
    lv_obj_t* chipsBar = lv_obj_create(tab);
    lv_obj_set_size(chipsBar, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(chipsBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chipsBar, 0, 0);
    lv_obj_set_style_pad_all(chipsBar, 0, 0);
    lv_obj_set_flex_flow(chipsBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chipsBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto addChip = [this, chipsBar](const char* label, radio::BleDeviceCategory cat, bool all) {
        lv_obj_t* chip = lv_btn_create(chipsBar);
        lv_obj_set_size(chip, LV_SIZE_CONTENT, 28);
        lv_obj_set_style_bg_color(chip, lv_color_hex(0x161D30), 0);
        lv_obj_set_style_border_color(chip, lv_color_hex(0x00B4D8), 0);
        lv_obj_set_style_border_width(chip, 1, 0);
        lv_obj_set_style_radius(chip, 14, 0);
        lv_obj_set_style_pad_hor(chip, 10, 0);
        lv_obj_t* lbl = lv_label_create(chip);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE2E8F0), 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(chip, [](lv_event_t* e) {
            auto* view = static_cast<BleShieldView*>(lv_event_get_user_data(e));
            // Toggle de filtro
            auto catVal = static_cast<radio::BleDeviceCategory>(reinterpret_cast<uintptr_t>(lv_event_get_param(e)));
            if (view) {
                view->applyFilter(catVal, catVal == radio::BleDeviceCategory::Unknown);
            }
        }, LV_EVENT_CLICKED, this);
    };

    addChip("Todos", radio::BleDeviceCategory::Unknown, true);
    addChip("Apple", radio::BleDeviceCategory::AppleDevice, false);
    addChip("Samsung", radio::BleDeviceCategory::SamsungDevice, false);
    addChip("Meta", radio::BleDeviceCategory::MetaSmartGlasses, false);
    addChip("Trackers", radio::BleDeviceCategory::AppleAirTag, false);
    addChip("HID", radio::BleDeviceCategory::PeripheralMouse, false);

    // Contenedor scrolleable para las tarjetas de dispositivos
    m_listContainer = lv_obj_create(tab);
    lv_obj_set_size(m_listContainer, LV_PCT(100), LV_PCT(80));
    lv_obj_set_style_bg_color(m_listContainer, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_border_width(m_listContainer, 0, 0);
    lv_obj_set_style_pad_all(m_listContainer, 0, 0);
    lv_obj_set_flex_flow(m_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_listContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

void BleShieldView::applyFilter(radio::BleDeviceCategory cat, bool all) {
    m_filterAll = all;
    m_activeFilter = cat;
    refreshDeviceList();
}

void BleShieldView::addDeviceCard(const radio::BleDeviceRecord& dev) {
    lv_obj_t* card = lv_obj_create(m_listContainer);
    lv_obj_set_size(card, LV_PCT(100), 68);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111625), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Columna izquierda: Icono + Datos
    lv_obj_t* leftCol = lv_obj_create(card);
    lv_obj_set_size(leftCol, LV_PCT(70), LV_PCT(100));
    lv_obj_set_style_bg_opa(leftCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftCol, 0, 0);
    lv_obj_set_style_pad_all(leftCol, 0, 0);
    lv_obj_set_flex_flow(leftCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(leftCol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Fila 1: Icono + Nombre + Badge
    char titleBuf[64];
    snprintf(titleBuf, sizeof(titleBuf), "%s %s %s",
             LV_SYMBOL_BLUETOOTH, radio::getCategoryTag(dev.category), dev.name.c_str());
    lv_obj_t* lblTitle = lv_label_create(leftCol);
    lv_label_set_text(lblTitle, titleBuf);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0xFFFFFF), 0);

    // Fila 2: Fabricante + MAC + Detalles
    char subBuf[96];
    snprintf(subBuf, sizeof(subBuf), "%s | %s", dev.vendor.c_str(), dev.macStr.c_str());
    lv_obj_t* lblSub = lv_label_create(leftCol);
    lv_label_set_text(lblSub, subBuf);
    lv_obj_set_style_text_color(lblSub, lv_color_hex(0x94A3B8), 0);

    // Columna derecha: RSSI + Botón Rastrear
    lv_obj_t* rightCol = lv_obj_create(card);
    lv_obj_set_size(rightCol, LV_PCT(28), LV_PCT(100));
    lv_obj_set_style_bg_opa(rightCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rightCol, 0, 0);
    lv_obj_set_style_pad_all(rightCol, 0, 0);
    lv_obj_set_flex_flow(rightCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rightCol, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    char rssiBuf[16];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", dev.rssi);
    lv_obj_t* lblRssi = lv_label_create(rightCol);
    lv_label_set_text(lblRssi, rssiBuf);
    lv_obj_set_style_text_color(lblRssi, dev.rssi > -65 ? lv_color_hex(0x00F5D4) : lv_color_hex(0xF59E0B), 0);

    lv_obj_t* btnTrack = lv_btn_create(rightCol);
    lv_obj_set_size(btnTrack, 60, 26);
    lv_obj_set_style_bg_color(btnTrack, lv_color_hex(0x0077B6), 0);
    lv_obj_set_style_radius(btnTrack, 4, 0);
    lv_obj_t* lblTrack = lv_label_create(btnTrack);
    lv_label_set_text(lblTrack, "Geiger");
    lv_obj_center(lblTrack);

    // Copiar la MAC al evento para establecer como objetivo del Geiger
    struct TrackContext {
        BleShieldView* view;
        uint8_t mac[6];
    };
    auto* ctx = new TrackContext{this, {dev.mac[0], dev.mac[1], dev.mac[2], dev.mac[3], dev.mac[4], dev.mac[5]}};

    lv_obj_add_event_cb(btnTrack, [](lv_event_t* e) {
        auto* c = static_cast<TrackContext*>(lv_event_get_user_data(e));
        if (c && c->view) {
            security::BleShieldService::getInstance().setGeigerTarget(c->mac);
            // Cambiar a pestaña Geiger (índice 1)
            lv_tabview_set_active(c->view->m_tabView, 1, LV_ANIM_ON);
            c->view->updateGeigerDisplay();
        }
    }, LV_EVENT_CLICKED, ctx);

    lv_obj_add_event_cb(card, [](lv_event_t* e) {
        auto* c = static_cast<TrackContext*>(lv_event_get_user_data(e));
        if (c) delete c;
    }, LV_EVENT_DELETE, ctx);
}

void BleShieldView::refreshDeviceList() {
    if (!m_listContainer) return;
    lv_obj_clean(m_listContainer);

    auto devices = security::BleShieldService::getInstance().getDevices();

    bool scanning = security::BleShieldService::getInstance().isScanning();
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), "Dispositivos: %u %s",
             static_cast<unsigned>(devices.size()),
             scanning ? "[ESCANEANDO...]" : "[DETENIDO]");
    if (m_lblDeviceCount) {
        lv_label_set_text(m_lblDeviceCount, countBuf);
        lv_obj_set_style_text_color(m_lblDeviceCount, scanning ? lv_color_hex(0x00F5D4) : lv_color_hex(0x94A3B8), 0);
    }

    for (const auto& dev : devices) {
        if (!m_filterAll) {
            if (m_activeFilter == radio::BleDeviceCategory::AppleDevice &&
                (dev.category != radio::BleDeviceCategory::AppleDevice &&
                 dev.category != radio::BleDeviceCategory::AppleAirPods &&
                 dev.category != radio::BleDeviceCategory::AppleAirTag)) continue;

            if (m_activeFilter == radio::BleDeviceCategory::SamsungDevice &&
                (dev.category != radio::BleDeviceCategory::SamsungDevice &&
                 dev.category != radio::BleDeviceCategory::SamsungSmartTag)) continue;

            if (m_activeFilter == radio::BleDeviceCategory::AppleAirTag && !dev.isTracker) continue;

            if (m_activeFilter == radio::BleDeviceCategory::PeripheralMouse &&
                (dev.category != radio::BleDeviceCategory::PeripheralMouse &&
                 dev.category != radio::BleDeviceCategory::PeripheralKeyboard &&
                 dev.category != radio::BleDeviceCategory::PeripheralGamepad)) continue;

            if (m_activeFilter == radio::BleDeviceCategory::MetaSmartGlasses &&
                dev.category != radio::BleDeviceCategory::MetaSmartGlasses) continue;
        }
        addDeviceCard(dev);
    }
}

void BleShieldView::setupGeigerTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Tarjeta del Objetivo
    lv_obj_t* targetCard = lv_obj_create(tab);
    lv_obj_set_size(targetCard, LV_PCT(100), 54);
    lv_obj_set_style_bg_color(targetCard, lv_color_hex(0x111625), 0);
    lv_obj_set_style_border_color(targetCard, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(targetCard, 1, 0);
    lv_obj_set_style_radius(targetCard, 8, 0);
    lv_obj_set_flex_flow(targetCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(targetCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblGeigerTarget = lv_label_create(targetCard);
    lv_label_set_text(m_lblGeigerTarget, "Objetivo: Ninguno seleccionado");
    lv_obj_set_style_text_color(m_lblGeigerTarget, lv_color_hex(0x00F5D4), 0);

    m_lblGeigerMac = lv_label_create(targetCard);
    lv_label_set_text(m_lblGeigerMac, "Selecciona un dispositivo en Air Recon");
    lv_obj_set_style_text_color(m_lblGeigerMac, lv_color_hex(0x94A3B8), 0);

    // Medidor Central de Arco (VU Proximidad)
    m_arcGeiger = lv_arc_create(tab);
    lv_obj_set_size(m_arcGeiger, 170, 170);
    lv_arc_set_rotation(m_arcGeiger, 135);
    lv_arc_set_bg_angles(m_arcGeiger, 0, 270);
    lv_arc_set_range(m_arcGeiger, 0, 100);
    lv_arc_set_value(m_arcGeiger, 0);
    lv_obj_set_style_arc_color(m_arcGeiger, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_arc_color(m_arcGeiger, lv_color_hex(0x00F5D4), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(m_arcGeiger, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(m_arcGeiger, 14, LV_PART_INDICATOR);
    lv_obj_remove_style(m_arcGeiger, nullptr, LV_PART_KNOB);

    // Contenedor de datos en el centro del arco
    lv_obj_t* centerBox = lv_obj_create(m_arcGeiger);
    lv_obj_set_size(centerBox, 110, 110);
    lv_obj_set_style_bg_opa(centerBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(centerBox, 0, 0);
    lv_obj_center(centerBox);
    lv_obj_set_flex_flow(centerBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(centerBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_lblGeigerPercent = lv_label_create(centerBox);
    lv_label_set_text(m_lblGeigerPercent, "0%");
    lv_obj_set_style_text_color(m_lblGeigerPercent, lv_color_hex(0xFFFFFF), 0);

    m_lblGeigerRssi = lv_label_create(centerBox);
    lv_label_set_text(m_lblGeigerRssi, "-100 dBm");
    lv_obj_set_style_text_color(m_lblGeigerRssi, lv_color_hex(0x00B4D8), 0);

    m_lblGeigerDistance = lv_label_create(centerBox);
    lv_label_set_text(m_lblGeigerDistance, "~ -- m");
    lv_obj_set_style_text_color(m_lblGeigerDistance, lv_color_hex(0x94A3B8), 0);

    // Fila inferior de controles
    lv_obj_t* bottomRow = lv_obj_create(tab);
    lv_obj_set_size(bottomRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(bottomRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottomRow, 0, 0);
    lv_obj_set_style_pad_all(bottomRow, 0, 0);
    lv_obj_set_flex_flow(bottomRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottomRow, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnClearTarget = lv_btn_create(bottomRow);
    lv_obj_set_size(btnClearTarget, 130, 32);
    lv_obj_set_style_bg_color(btnClearTarget, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(btnClearTarget, 6, 0);
    lv_obj_t* lblClearT = lv_label_create(btnClearTarget);
    lv_label_set_text(lblClearT, "Deseleccionar");
    lv_obj_center(lblClearT);

    lv_obj_add_event_cb(btnClearTarget, [](lv_event_t* e) {
        security::BleShieldService::getInstance().clearGeigerTarget();
        auto* view = static_cast<BleShieldView*>(lv_event_get_user_data(e));
        if (view) view->updateGeigerDisplay();
    }, LV_EVENT_CLICKED, this);
}

void BleShieldView::updateGeigerDisplay() {
    auto& svc = security::BleShieldService::getInstance();
    if (!svc.hasGeigerTarget()) {
        if (m_lblGeigerTarget) lv_label_set_text(m_lblGeigerTarget, "Objetivo: Ninguno seleccionado");
        if (m_lblGeigerMac) lv_label_set_text(m_lblGeigerMac, "Pulsa [Geiger] en una tarjeta de Air Recon");
        if (m_arcGeiger) lv_arc_set_value(m_arcGeiger, 0);
        if (m_lblGeigerPercent) lv_label_set_text(m_lblGeigerPercent, "0%");
        if (m_lblGeigerRssi) lv_label_set_text(m_lblGeigerRssi, "-100 dBm");
        if (m_lblGeigerDistance) lv_label_set_text(m_lblGeigerDistance, "~ -- m");
        return;
    }

    uint8_t mac[6];
    svc.getGeigerTargetMac(mac);
    char macBuf[24];
    snprintf(macBuf, sizeof(macBuf), "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (m_lblGeigerMac) lv_label_set_text(m_lblGeigerMac, macBuf);
    if (m_lblGeigerTarget) lv_label_set_text(m_lblGeigerTarget, "Rastreando Dispositivo Objetivo");

    uint8_t prox = svc.getGeigerProximity();
    int8_t rssi = svc.getGeigerRssi();
    float dist = svc.getGeigerDistanceMeters();

    if (m_arcGeiger) lv_arc_set_value(m_arcGeiger, prox);

    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%u%%", prox);
    if (m_lblGeigerPercent) lv_label_set_text(m_lblGeigerPercent, pctBuf);

    char rssiBuf[16];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", rssi);
    if (m_lblGeigerRssi) lv_label_set_text(m_lblGeigerRssi, rssiBuf);

    char distBuf[16];
    snprintf(distBuf, sizeof(distBuf), "~ %.1f m", dist);
    if (m_lblGeigerDistance) lv_label_set_text(m_lblGeigerDistance, distBuf);
}

void BleShieldView::setupSpamIdsTab(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Banner Superior de Estado
    m_cardSpamBanner = lv_obj_create(tab);
    lv_obj_set_size(m_cardSpamBanner, LV_PCT(100), 65);
    lv_obj_set_style_bg_color(m_cardSpamBanner, lv_color_hex(0x064E3B), 0);
    lv_obj_set_style_border_color(m_cardSpamBanner, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(m_cardSpamBanner, 1, 0);
    lv_obj_set_style_radius(m_cardSpamBanner, 8, 0);
    lv_obj_set_flex_flow(m_cardSpamBanner, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_cardSpamBanner, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    m_lblSpamTitle = lv_label_create(m_cardSpamBanner);
    lv_label_set_text(m_lblSpamTitle, LV_SYMBOL_OK " SECTOR LIMPIO - VIGILANCIA ACTIVA");
    lv_obj_set_style_text_color(m_lblSpamTitle, lv_color_hex(0x00F5D4), 0);

    m_lblSpamDesc = lv_label_create(m_cardSpamBanner);
    lv_label_set_text(m_lblSpamDesc, "Cero ráfagas maliciosas en el espectro BLE");
    lv_obj_set_style_text_color(m_lblSpamDesc, lv_color_hex(0xE2E8F0), 0);

    // Rejilla de Métricas
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

    makeMetricCard("Paquetes Spam", "0", &m_lblSpamCount);
    makeMetricCard("Tasa de Ataque", "0.0 pps", &m_lblSpamRate);

    // Registro de eventos (Log)
    lv_obj_t* logCard = lv_obj_create(tab);
    lv_obj_set_size(logCard, LV_PCT(100), 120);
    lv_obj_set_style_bg_color(logCard, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_border_color(logCard, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(logCard, 1, 0);
    lv_obj_set_style_radius(logCard, 6, 0);
    lv_obj_set_style_pad_all(logCard, 6, 0);

    m_lblSpamLog = lv_label_create(logCard);
    lv_label_set_text(m_lblSpamLog, "[IDS] Centinela pasivo iniciado. Esperando eventos...");
    lv_obj_set_style_text_color(m_lblSpamLog, lv_color_hex(0x94A3B8), 0);

    // Botón de restablecer
    lv_obj_t* btnReset = lv_btn_create(tab);
    lv_obj_set_size(btnReset, 160, 32);
    lv_obj_set_style_bg_color(btnReset, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(btnReset, 6, 0);
    lv_obj_t* lblR = lv_label_create(btnReset);
    lv_label_set_text(lblR, "Restablecer Alertas");
    lv_obj_center(lblR);

    lv_obj_add_event_cb(btnReset, [](lv_event_t* e) {
        security::BleShieldService::getInstance().clearSpamAlert();
        auto* view = static_cast<BleShieldView*>(lv_event_get_user_data(e));
        if (view) view->updateSpamDisplay();
    }, LV_EVENT_CLICKED, this);

    setupIdsConfigSection(tab);
}

namespace {
constexpr intptr_t IDS_TOGGLE_MASTER = -1;
constexpr intptr_t IDS_TOGGLE_LOG = -2;

void idsToggleCb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    intptr_t kind = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));

    auto& svc = security::BleShieldService::getInstance();
    if (kind == IDS_TOGGLE_MASTER) {
        svc.setIdsEnabled(on);
    } else if (kind == IDS_TOGGLE_LOG) {
        auto cfg = svc.getIdsConfig();
        cfg.logging = on;
        svc.setIdsConfig(cfg);
    } else {
        svc.setIdsDetection(static_cast<radio::BleSpamType>(kind), on);
    }

    auto* view = static_cast<BleShieldView*>(lv_obj_get_user_data(sw));
    if (view) view->refreshIdsConfigLabels();
}
} // namespace

void BleShieldView::setupIdsConfigSection(lv_obj_t* tab) {
    auto cfg = security::BleShieldService::getInstance().getIdsConfig();

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
    addRow("AppleJuice", cfg.detectAppleJuice, static_cast<intptr_t>(radio::BleSpamType::AppleJuice), nullptr);
    addRow("FastPair", cfg.detectFastPair, static_cast<intptr_t>(radio::BleSpamType::AndroidFastPair), nullptr);
    addRow("SwiftPair", cfg.detectSwiftPair, static_cast<intptr_t>(radio::BleSpamType::WindowsSwiftPair), nullptr);
    addRow("Samsung Buds", cfg.detectSamsungBuds, static_cast<intptr_t>(radio::BleSpamType::SamsungBuds), nullptr);
    addRow("Flipper / Evil-M5", cfg.detectFlipperSpam, static_cast<intptr_t>(radio::BleSpamType::FlipperSpam), nullptr);

    m_lblIdsEnables = lv_label_create(card);
    lv_obj_set_style_text_color(m_lblIdsEnables, lv_color_hex(0x94A3B8), 0);
    refreshIdsConfigLabels();
}

void BleShieldView::refreshIdsConfigLabels() {
    if (!m_lblIdsEnables) return;
    auto cfg = security::BleShieldService::getInstance().getIdsConfig();
    char buf[160];
    snprintf(buf, sizeof(buf),
             "Ventana: %u ms | Umbral: %u pkt | Estado: %s",
             static_cast<unsigned>(cfg.windowMs),
             static_cast<unsigned>(cfg.minPacketsInWindow),
             cfg.enabled ? "ACTIVO" : "INACTIVO");
    lv_label_set_text(m_lblIdsEnables, buf);
}

void BleShieldView::updateSpamDisplay() {
    auto& svc = security::BleShieldService::getInstance();
    auto alert = svc.getSpamAlert();

    if (alert.active) {
        if (m_cardSpamBanner) {
            lv_obj_set_style_bg_color(m_cardSpamBanner, lv_color_hex(0x4C0519), 0);
            lv_obj_set_style_border_color(m_cardSpamBanner, lv_color_hex(0xFF3366), 0);
        }
        if (m_lblSpamTitle) {
            lv_label_set_text(m_lblSpamTitle, LV_SYMBOL_WARNING " ALERTA: ATAQUE BLE EN CURSO");
            lv_obj_set_style_text_color(m_lblSpamTitle, lv_color_hex(0xFF3366), 0);
        }
        if (m_lblSpamDesc) {
            lv_label_set_text(m_lblSpamDesc, alert.description.c_str());
        }
    } else {
        if (m_cardSpamBanner) {
            lv_obj_set_style_bg_color(m_cardSpamBanner, lv_color_hex(0x064E3B), 0);
            lv_obj_set_style_border_color(m_cardSpamBanner, lv_color_hex(0x00F5D4), 0);
        }
        if (m_lblSpamTitle) {
            lv_label_set_text(m_lblSpamTitle, LV_SYMBOL_OK " SECTOR LIMPIO - VIGILANCIA ACTIVA");
            lv_obj_set_style_text_color(m_lblSpamTitle, lv_color_hex(0x00F5D4), 0);
        }
        if (m_lblSpamDesc) {
            lv_label_set_text(m_lblSpamDesc, "Cero ráfagas maliciosas en el espectro BLE");
        }
    }

    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%u", svc.getTotalSpamPackets());
    if (m_lblSpamCount) lv_label_set_text(m_lblSpamCount, countBuf);

    char rateBuf[16];
    snprintf(rateBuf, sizeof(rateBuf), "%.1f pps", alert.packetsPerSec);
    if (m_lblSpamRate) lv_label_set_text(m_lblSpamRate, rateBuf);

    auto log = svc.getSpamLog();
    if (!log.empty() && m_lblSpamLog) {
        std::string fullLog;
        for (auto it = log.rbegin(); it != log.rend(); ++it) {
            fullLog += *it + "\n";
        }
        lv_label_set_text(m_lblSpamLog, fullLog.c_str());
    }
}

} // namespace ui
} // namespace cbdos
