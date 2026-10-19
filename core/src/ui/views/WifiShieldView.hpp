#pragma once

#include "BaseView.hpp"
#include "cbdos/wifi_types.hpp"
#include <lvgl.h>
#include <string>
#include <vector>
#include <atomic>

namespace cbdos {
namespace ui {

class WifiShieldView : public BaseView {
public:
    WifiShieldView();
    ~WifiShieldView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;

    // Actualizacion reactiva
    void refreshApList();
    void updateIdsDisplay();
    void updateSnifferDisplay();
    void refreshIdsConfigLabels();

private:
    void createTabs(lv_obj_t* parent);
    void setupReconTab(lv_obj_t* tab);
    void setupIdsTab(lv_obj_t* tab);
    void setupSnifferTab(lv_obj_t* tab);
    void setupIdsConfigSection(lv_obj_t* tab);
    void addApCard(const radio::WifiApRecord& ap);

    std::atomic<bool> m_apsDirty{false};

    lv_obj_t* m_tabView = nullptr;
    lv_timer_t* m_refreshTimer = nullptr;

    // Recon
    lv_obj_t* m_btnScanToggle = nullptr;
    lv_obj_t* m_lblScanBtn = nullptr;
    lv_obj_t* m_lblReconCount = nullptr;
    lv_obj_t* m_listContainer = nullptr;

    // IDS
    lv_obj_t* m_cardIdsBanner = nullptr;
    lv_obj_t* m_lblIdsTitle = nullptr;
    lv_obj_t* m_lblIdsDesc = nullptr;
    lv_obj_t* m_lblIdsCount = nullptr;
    lv_obj_t* m_lblIdsRate = nullptr;
    lv_obj_t* m_lblIdsLog = nullptr;
    lv_obj_t* m_swIdsMaster = nullptr;
    lv_obj_t* m_swIdsLog = nullptr;
    lv_obj_t* m_lblIdsEnables = nullptr;

    // Sniffer
    lv_obj_t* m_swSniffer = nullptr;
    lv_obj_t* m_lblSniffTotal = nullptr;
    lv_obj_t* m_lblSniffBeacon = nullptr;
    lv_obj_t* m_lblSniffDeauth = nullptr;
    lv_obj_t* m_lblSniffData = nullptr;
};

} // namespace ui
} // namespace cbdos
