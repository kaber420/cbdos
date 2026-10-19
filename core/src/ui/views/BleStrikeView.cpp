#include "BleStrikeView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/ble.hpp"
#include "cbdos/ble_transmitter.hpp"

namespace cbdos {
namespace ui {

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

    createTabs(m_container);

    // Temporizador de refresco visual reactivo
    m_timer = lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<BleStrikeView*>(lv_timer_get_user_data(t));
        if (self) self->onUpdate();
    }, 250, this);

    return true;
}

void BleStrikeView::createTabs(lv_obj_t* parent) {
    m_tabView = lv_tabview_create(parent);
    lv_tabview_set_tab_bar_position(m_tabView, LV_DIR_TOP);
    lv_obj_set_size(m_tabView, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_tabView, lv_color_hex(0x0A0E17), 0);
    lv_obj_set_style_border_width(m_tabView, 0, 0);

    lv_obj_t* tabBar = lv_tabview_get_tab_bar(m_tabView);
    if (tabBar) {
        lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x111625), 0);
        lv_obj_set_style_text_color(tabBar, lv_color_hex(0xD90429),
            static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_ITEMS) | static_cast<uint32_t>(LV_STATE_CHECKED)));
        lv_tabview_set_tab_bar_size(m_tabView, 38);
    }

    lv_obj_t* tabBeacon = lv_tabview_add_tab(m_tabView, "Beacons");
    lv_obj_t* tabDeauth = lv_tabview_add_tab(m_tabView, "BLE Deauth");

    setupBeaconTab(tabBeacon);
    setupDeauthTab(tabDeauth);
}

void BleStrikeView::onShow() {
    BaseView::onShow();
    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle("BLE Lab");
    header.showWifi(true);
    header.showBackButton(true, []() {
        UIManager::getInstance().popView();
    });

    refreshVariants();
    refreshDeauthTargets();

    ble::BleTransmitter::getInstance().setStatusCallback([this](const ble::BleTxStatus&) {
        m_statusDirty = true;
    });

    m_statusDirty = true;
    refreshBeaconStatus();
    refreshDeauthStatus();
}

void BleStrikeView::onHide() {
    BaseView::onHide();
    ble::BleTransmitter::getInstance().setStatusCallback(nullptr);
}

void BleStrikeView::onUpdate() {
    if (m_deauthRunning) {
        m_deauthFramesSent += 5;
        refreshDeauthStatus();
    }
    if (m_statusDirty.exchange(false)) {
        refreshBeaconStatus();
        refreshDeauthStatus();
    }
}

void BleStrikeView::onDestroy() {
    ble::BleTransmitter::getInstance().shutdown();
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
    BaseView::onDestroy();
}

} // namespace ui
} // namespace cbdos