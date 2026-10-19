#pragma once

#include "BaseView.hpp"
#include "cbdos/ble_types.hpp"
#include <lvgl.h>
#include <string>
#include <vector>
#include <atomic>

namespace cbdos {
namespace ui {

class BleShieldView : public BaseView {
public:
    BleShieldView();
    ~BleShieldView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;

    // Métodos de actualización reactiva
    void refreshDeviceList();
    void updateGeigerDisplay();
    void updateSpamDisplay();
    void refreshIdsConfigLabels();

private:
    void createTabs(lv_obj_t* parent);
    void setupAirReconTab(lv_obj_t* tab);
    void setupGeigerTab(lv_obj_t* tab);
    void setupSpamIdsTab(lv_obj_t* tab);
    void setupIdsConfigSection(lv_obj_t* tab);

    // Helpers de creación de widgets
    void addDeviceCard(const radio::BleDeviceRecord& dev);
    void applyFilter(radio::BleDeviceCategory cat, bool all = false);

    // Bandera reactiva de actualización de dispositivos
    std::atomic<bool> m_devicesDirty{false};


    // Widgets principales
    lv_obj_t* m_tabView = nullptr;
    lv_timer_t* m_refreshTimer = nullptr;

    // Pestaña 1: Air Recon
    lv_obj_t* m_lblDeviceCount = nullptr;
    lv_obj_t* m_btnScanToggle = nullptr;
    lv_obj_t* m_lblScanBtn = nullptr;
    lv_obj_t* m_listContainer = nullptr;
    radio::BleDeviceCategory m_activeFilter = radio::BleDeviceCategory::Unknown;
    bool m_filterAll = true;

    // Pestaña 2: Geiger Tracker
    lv_obj_t* m_lblGeigerTarget = nullptr;
    lv_obj_t* m_lblGeigerMac = nullptr;
    lv_obj_t* m_arcGeiger = nullptr;
    lv_obj_t* m_lblGeigerPercent = nullptr;
    lv_obj_t* m_lblGeigerRssi = nullptr;
    lv_obj_t* m_lblGeigerDistance = nullptr;
    lv_obj_t* m_swGeigerAudio = nullptr;

    // Pestaña 3: Spam IDS
    lv_obj_t* m_cardSpamBanner = nullptr;
    lv_obj_t* m_lblSpamTitle = nullptr;
    lv_obj_t* m_lblSpamDesc = nullptr;
    lv_obj_t* m_lblSpamCount = nullptr;
    lv_obj_t* m_lblSpamRate = nullptr;
    lv_obj_t* m_lblSpamLog = nullptr;
    lv_obj_t* m_swIdsMaster = nullptr;
    lv_obj_t* m_swIdsLog = nullptr;
    lv_obj_t* m_lblIdsEnables = nullptr;
};

} // namespace ui
} // namespace cbdos
