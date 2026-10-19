#pragma once

#include "BaseView.hpp"
#include "cbdos/wifi_strike.hpp"
#include <atomic>
#include <lvgl.h>
#include <string>

namespace cbdos {
namespace ui {

class WifiStrikeView : public BaseView {
public:
    WifiStrikeView();
    ~WifiStrikeView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;

private:
    void createTabs(lv_obj_t* parent);
    void setupDeauthTab(lv_obj_t* tab);
    void setupBeaconTab(lv_obj_t* tab);
    void setupPortalTab(lv_obj_t* tab);
    void setupPmkidTab(lv_obj_t* tab);

    /** Traduce los controles de la pestana Beacon en una StrikeConfig lista
     *  para startBeacon(), incluyendo el snapshot de APs del Air Recon. */
    wifi::StrikeConfig buildBeaconConfig() const;

    /** Repinta metricas, estado y boton desde el snapshot del servicio. */
    void refreshBeaconStatus();

    /** Muestra u oculta los controles que solo aplican a un modo concreto. */
    void applyBeaconModeVisibility();

    static void modeCb(lv_event_t* e);
    static void toggleCb(lv_event_t* e);

    lv_obj_t* m_tabView = nullptr;
    lv_obj_t* m_lblDeauthStatus = nullptr;
    lv_obj_t* m_lblBeaconStatus = nullptr;
    lv_obj_t* m_lblPortalStatus = nullptr;
    lv_obj_t* m_lblPmkidStatus = nullptr;

    // --- Pestana Beacon Spam ---
    lv_obj_t* m_ddBeaconMode = nullptr;
    lv_obj_t* m_ssidRow = nullptr;
    lv_obj_t* m_taBeaconSsid = nullptr;
    lv_obj_t* m_cloneRow = nullptr;
    lv_obj_t* m_lblCloneInfo = nullptr;
    lv_obj_t* m_ledBeacon = nullptr;
    lv_obj_t* m_btnBeaconToggle = nullptr;
    lv_obj_t* m_lblBeaconToggle = nullptr;
    lv_obj_t* m_lblFrames = nullptr;
    lv_obj_t* m_lblChannel = nullptr;
    lv_obj_t* m_lblFailed = nullptr;
    lv_obj_t* m_lblBeaconState = nullptr;
    lv_obj_t* m_lblBeaconHint = nullptr;
    lv_timer_t* m_beaconTimer = nullptr;

    /** El servicio tiene estado nuevo pendiente de repintar. */
    std::atomic<bool> m_statusPending{true};
};

} // namespace ui
} // namespace cbdos
