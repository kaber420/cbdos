#pragma once

#include "BaseView.hpp"
#include "cbdos/radio.hpp"
#include "cbdos/network_interface.hpp"
#include "cbdos/config_manager.hpp"
#include <lvgl.h>
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

class NetworkManagerView : public BaseView {
public:
    NetworkManagerView();
    ~NetworkManagerView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    void buildSlot0Radio(lv_obj_t* parent);
    void buildSlotBle(lv_obj_t* parent);
    void buildSlot1Backpack(lv_obj_t* parent);
    void buildSlot2Usb(lv_obj_t* parent);

    void updateModeVisibility();
    void refreshBleUi();
    void refreshWifiListUi();
    void refreshSweepListUi();
    void checkBackpackHotPlug();

    // Callbacks
    static void radioPowerSwCb(lv_event_t* e);
    static void modeSelectCb(lv_event_t* e);
    static void channelSliderCb(lv_event_t* e);
    static void txPowerSliderCb(lv_event_t* e);

    // Wi-Fi Callbacks
    static void wifiScanBtnCb(lv_event_t* e);
    static void wifiApClickCb(lv_event_t* e);
    static void wifiConnectDialogCb(lv_event_t* e);

    // ESP-NOW Sweep Callbacks
    static void sweepBtnCb(lv_event_t* e);

    static NetworkManagerView* s_instance;
    static cbdos::radio::RadioConfig s_radioCfg;
    static WiFiConfig s_wifiCfg;

    // Slot 0 Widgets
    lv_obj_t* m_swPower = nullptr;
    lv_obj_t* m_lblPowerStatus = nullptr;
    lv_obj_t* m_ddMode = nullptr;
    
    // Contenedores según modo
    lv_obj_t* m_boxMeshControls = nullptr;
    lv_obj_t* m_sliderChannel = nullptr;
    lv_obj_t* m_lblChannelVal = nullptr;
    lv_obj_t* m_sliderTx = nullptr;
    lv_obj_t* m_lblTxVal = nullptr;
    lv_obj_t* m_btnSweep = nullptr;
    lv_obj_t* m_lblSweepStatus = nullptr;
    lv_obj_t* m_sweepListContainer = nullptr;

    lv_obj_t* m_boxWifiControls = nullptr;
    lv_obj_t* m_btnScanWifi = nullptr;
    lv_obj_t* m_lblWifiScanStatus = nullptr;
    lv_obj_t* m_wifiListContainer = nullptr;
    lv_obj_t* m_lblIpStatus = nullptr;

    // Slot 1 (Mochila) Widgets
    lv_obj_t* m_cardBackpack = nullptr;
    lv_obj_t* m_lblBackpackStatus = nullptr;

    // Slot 2 (USB) Widgets
    lv_obj_t* m_cardUsb = nullptr;

    // Coexistencia Bluetooth LE
    static void blePowerSwCb(lv_event_t* e);
    lv_obj_t* m_cardBle = nullptr;
    lv_obj_t* m_swBlePower = nullptr;
    lv_obj_t* m_lblBleStatus = nullptr;
};

} // namespace ui
} // namespace cbdos
