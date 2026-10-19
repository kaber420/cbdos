#pragma once

#include "BaseView.hpp"
#include <atomic>
#include <lvgl.h>
#include <string>

namespace cbdos {
namespace ui {

class BleStrikeView : public BaseView {
public:
    BleStrikeView();
    ~BleStrikeView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onUpdate() override;

private:
    void createTabs(lv_obj_t* parent);

    // --- Pestaña 1: Beacons (BleStrikeView_BeaconTab.cpp) ---
    void setupBeaconTab(lv_obj_t* tab);
    void buildBeaconControls(lv_obj_t* parent);
    void buildBeaconStatus(lv_obj_t* parent);
    void refreshCategories();
    void refreshVariants();
    void refreshBeaconStatus();

    static void categoryCb(lv_event_t* e);
    static void variantCb(lv_event_t* e);
    static void powerCb(lv_event_t* e);
    static void intervalCb(lv_event_t* e);
    static void macCb(lv_event_t* e);
    static void customNameCb(lv_event_t* e);
    static void toggleCb(lv_event_t* e);

    // --- Pestaña 2: BLE Deauth (BleStrikeView_DeauthTab.cpp) ---
    void setupDeauthTab(lv_obj_t* tab);
    void buildDeauthControls(lv_obj_t* parent);
    void buildDeauthStatus(lv_obj_t* parent);
    void refreshDeauthTargets();
    void refreshDeauthStatus();

    static void deauthTargetCb(lv_event_t* e);
    static void deauthReasonCb(lv_event_t* e);
    static void deauthModeCb(lv_event_t* e);
    static void deauthIntervalCb(lv_event_t* e);
    static void deauthRefreshCb(lv_event_t* e);
    static void deauthToggleCb(lv_event_t* e);
    static void deauthMacInputCb(lv_event_t* e);

    // Widgets Contenedor
    lv_obj_t* m_tabView = nullptr;
    lv_timer_t* m_timer = nullptr;

    // Widgets Pestaña Beacons
    lv_obj_t* m_scrollBeacon = nullptr;
    lv_obj_t* m_ddCategory = nullptr;
    lv_obj_t* m_ddVariant = nullptr;
    lv_obj_t* m_ddPower = nullptr;
    lv_obj_t* m_ddInterval = nullptr;
    lv_obj_t* m_ddMac = nullptr;
    lv_obj_t* m_taCustomName = nullptr;
    lv_obj_t* m_btnToggle = nullptr;
    lv_obj_t* m_lblToggle = nullptr;
    lv_obj_t* m_lblState = nullptr;
    lv_obj_t* m_lblCounters = nullptr;
    lv_obj_t* m_lblHint = nullptr;
    lv_obj_t* m_ledActive = nullptr;

    // Widgets Pestaña BLE Deauth
    lv_obj_t* m_scrollDeauth = nullptr;
    lv_obj_t* m_ddDeauthTarget = nullptr;
    lv_obj_t* m_lblDeauthCount = nullptr;
    lv_obj_t* m_btnDeauthRefresh = nullptr;
    lv_obj_t* m_taDeauthMac = nullptr;
    lv_obj_t* m_ddDeauthReason = nullptr;
    lv_obj_t* m_ddDeauthMode = nullptr;
    lv_obj_t* m_ddDeauthInterval = nullptr;
    lv_obj_t* m_btnDeauthToggle = nullptr;
    lv_obj_t* m_lblDeauthToggle = nullptr;
    lv_obj_t* m_lblDeauthState = nullptr;
    lv_obj_t* m_lblDeauthCounters = nullptr;
    lv_obj_t* m_lblDeauthHint = nullptr;
    lv_obj_t* m_ledDeauth = nullptr;

    // Estado interno
    std::atomic<bool> m_statusDirty{true};
    bool m_available = false;
    bool m_deauthRunning = false;
    uint32_t m_deauthFramesSent = 0;
    uint32_t m_deauthStartMs = 0;
};

} // namespace ui
} // namespace cbdos