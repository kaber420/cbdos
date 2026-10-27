#pragma once

#include "../BaseView.hpp"
#include "OmniwispCommon.hpp"
#include "OmniwispMainView.hpp"
#include "OmniwispDiscoveryView.hpp"
#include "OmniwispSettingsView.hpp"
#include "OmniwispSectorDetailView.hpp"
#include "OmniwispCpeDetailView.hpp"
#include "OmniwispAddGroupModal.hpp"
#include "OmniwispAddApModal.hpp"
#include "cbdos/rtos.hpp"

namespace cbdos {
namespace ui {

class OmniwispView : public BaseView {
public:
    OmniwispView();
    virtual ~OmniwispView();

    bool onCreate(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;
    void refreshUI();

private:
    void loadDataFromVault();
    void saveDataToVault();
    void fetchTelemetryAsync();
    void renderTopTabs();
    void switchTab(OmniTab tab);

    // Estado de pestañas principales (texto plano, diseño fluido)
    OmniTab m_currentTab = OmniTab::Devices;

    // Máquina de estados de pantalla (Jerarquía Multiventana)
    cbdos::omniwisp::OmniScreen m_currentScreen = cbdos::omniwisp::OmniScreen::SectorList;
    std::string m_activeGroupId = ""; // "" = Todas

    // Datos en memoria
    std::vector<cbdos::omniwisp::NetworkGroup> m_groups;
    std::vector<cbdos::omniwisp::SectorDevice> m_sectors;
    OmniSettings m_settings;

    // Dispositivo y CPE seleccionados para diagnóstico
    cbdos::omniwisp::SectorDevice m_selectedSector;
    cbdos::network::AirOSStation m_selectedCpe;

    // Cliente de telemetría y Worker FreeRTOS
    cbdos::network::AirOSClient m_client;
    cbdos::network::AirOSTelemetry m_telemetry;
    bool m_fetching = false;
    bool m_statusDirty = true;
    cbdos::rtos::TaskHandle m_workerTask = nullptr;

    lv_timer_t* m_pollTimer = nullptr;
    lv_obj_t* m_tabRow = nullptr;
    lv_obj_t* m_btnDiscovery = nullptr;
    lv_obj_t* m_btnDevices = nullptr;
    lv_obj_t* m_btnSettings = nullptr;
    lv_obj_t* m_viewContainer = nullptr;

    static void telemetryWorkerTask(void* param);
};

} // namespace ui
} // namespace cbdos
