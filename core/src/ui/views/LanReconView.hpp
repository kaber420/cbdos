#pragma once

#include "BaseView.hpp"
#include "cbdos/lan_recon.hpp"

#include <lvgl.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

// Vista grafica tactil de reconocimiento LAN (LVGL 9.5).
// El escaneo pesado corre en Core 0 (LanScannerService); esta vista solo
// sondea el estado compartido con un timer LVGL de baja frecuencia (Core 1)
// y reconstruye la lista de tarjetas unicamente cuando hay cambios.
class LanReconView : public BaseView {
public:
    LanReconView();
    ~LanReconView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;

private:
    static void scanBtnCb(lv_event_t* e);
    static void exportBtnCb(lv_event_t* e);
    static void pollTimerCb(lv_timer_t* timer);
    static void sshBtnCb(lv_event_t* e);
    static void pingBtnCb(lv_event_t* e);
    static void pingHostBtnCb(lv_event_t* e);

    void refreshFromService(bool force = false);
    void rebuildHostList(const std::vector<cbdos::network::LanHostInfo>& hosts);
    void updateScanButton(bool scanning);

    static const char* phaseToText(cbdos::network::LanScanPhase phase);
    static const char* hostIcon(const cbdos::network::LanHostInfo& host);
    static const char* portShortName(uint16_t port);

    lv_obj_t* m_lblNetInfo = nullptr;
    lv_obj_t* m_lblCount = nullptr;
    lv_obj_t* m_btnScan = nullptr;
    lv_obj_t* m_lblScan = nullptr;
    lv_obj_t* m_btnExport = nullptr;
    lv_obj_t* m_bar = nullptr;
    lv_obj_t* m_lblPhase = nullptr;
    lv_obj_t* m_list = nullptr;
    // v2: CIDR arbitrario (red ruteada) + ping individual.
    lv_obj_t* m_taCidr = nullptr;
    lv_obj_t* m_taPing = nullptr;
    lv_obj_t* m_lblPingResult = nullptr;

    lv_timer_t* m_pollTimer = nullptr;

    // Ultimo estado renderizado: evita reconstruir la lista sin cambios.
    size_t m_lastCount = static_cast<size_t>(-1);
    int m_lastPct = -1;
    int m_lastPhaseInt = -1;
    bool m_lastScanning = false;
    bool m_firstRefresh = true;
};

} // namespace ui
} // namespace cbdos
