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

    /** Traduce los controles de la pestana Deauth en una MgmtStrikeConfig lista
     *  para startMgmt(), con la AP elegida en el selector de objetivos. */
    wifi::MgmtStrikeConfig buildDeauthConfig() const;

    /** Repinta metricas, log y estado de la sesion de gestion. */
    void refreshDeauthStatus();

    /** Rellena el selector de objetivos si el Air Recon ha descubierto APs
     *  nuevas. La seleccion vigente se conserva por BSSID, no por indice: el
     *  Air Recon reordena la lista en cuanto cambia el RSSI de una AP. */
    void syncDeauthTargets();

    static void modeCb(lv_event_t* e);
    static void toggleCb(lv_event_t* e);
    static void deauthControlCb(lv_event_t* e);
    static void deauthToggleCb(lv_event_t* e);
    static void scanSliderCb(lv_event_t* e);
    static void scanBtnCb(lv_event_t* e);

    lv_obj_t* m_tabView = nullptr;
    lv_obj_t* m_lblBeaconStatus = nullptr;
    lv_obj_t* m_lblPortalStatus = nullptr;
    lv_obj_t* m_lblPmkidStatus = nullptr;

    // --- Pestana Deauth / Disassoc ---
    lv_obj_t* m_lblDeauthStatus = nullptr;
    lv_obj_t* m_ddDeauthSubtype = nullptr;
    lv_obj_t* m_ddDeauthDirection = nullptr;
    lv_obj_t* m_ddDeauthReason = nullptr;
    lv_obj_t* m_ddDeauthTarget = nullptr;
    lv_obj_t* m_lblDeauthTargets = nullptr;
    lv_obj_t* m_lblDeauthFrames = nullptr;
    lv_obj_t* m_lblDeauthTargeted = nullptr;
    lv_obj_t* m_lblDeauthChannel = nullptr;
    lv_obj_t* m_lblDeauthFailed = nullptr;
    lv_obj_t* m_ledDeauth = nullptr;
    lv_obj_t* m_lblDeauthState = nullptr;
    lv_obj_t* m_lblDeauthHint = nullptr;
    lv_obj_t* m_lblDeauthLog = nullptr;
    lv_obj_t* m_btnDeauthToggle = nullptr;
    lv_obj_t* m_lblDeauthToggle = nullptr;

    // --- Control de Escaneo Bajo Demanda (Air Recon) ---
    lv_obj_t* m_sliderScanDuration = nullptr;
    lv_obj_t* m_lblScanDuration = nullptr;
    lv_obj_t* m_btnScanNow = nullptr;
    lv_obj_t* m_lblScanNow = nullptr;
    uint32_t m_scanDurationSec = 3;
    bool m_isScanningAirRecon = false;
    uint32_t m_lastScanRevision = 0;

    /** Numero de APs con el que se relleno el selector de objetivos por ultima
     *  vez. Es la unica senal barata de que el Air Recon ha cambiado. */
    size_t m_deauthApCount = 0;
    /** BSSID elegido en el selector; vacio = broadcast. */
    std::string m_deauthSelectedBssid;

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
