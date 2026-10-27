#include "OmniwispView.hpp"
#include "../../UIManager.hpp"
#include "cbdos/vault.hpp"
#include "cbdos/network.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

OmniwispView::OmniwispView() : BaseView("OmniwispView", ViewMode::Solid) {}

OmniwispView::~OmniwispView() {
    if (m_pollTimer) {
        lv_timer_delete(m_pollTimer);
        m_pollTimer = nullptr;
    }
    if (m_workerTask) {
        cbdos::rtos::deleteTask(m_workerTask);
        m_workerTask = nullptr;
    }
}

void OmniwispView::renderTopTabs() {
    m_tabRow = lv_obj_create(m_container);
    lv_obj_set_size(m_tabRow, lv_pct(100), 38);
    lv_obj_set_style_bg_opa(m_tabRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_tabRow, 0, 0);
    lv_obj_set_style_pad_all(m_tabRow, 2, 0);
    lv_obj_set_flex_flow(m_tabRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(m_tabRow, 4, 0);
    lv_obj_set_flex_align(m_tabRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Tab 1: Discovery (100% fluido con flex_grow 1)
    m_btnDiscovery = lv_button_create(m_tabRow);
    lv_obj_set_flex_grow(m_btnDiscovery, 1);
    lv_obj_set_height(m_btnDiscovery, 32);
    lv_obj_set_style_radius(m_btnDiscovery, 6, 0);
    lv_obj_t* lblD = lv_label_create(m_btnDiscovery);
    lv_label_set_text(lblD, "Discovery");
    lv_obj_center(lblD);
    lv_obj_add_event_cb(m_btnDiscovery, [](lv_event_t* e) {
        auto* self = static_cast<OmniwispView*>(lv_event_get_user_data(e));
        if (self) self->switchTab(OmniTab::Discovery);
    }, LV_EVENT_CLICKED, this);

    // Tab 2: Dispositivos (100% fluido con flex_grow 1)
    m_btnDevices = lv_button_create(m_tabRow);
    lv_obj_set_flex_grow(m_btnDevices, 1);
    lv_obj_set_height(m_btnDevices, 32);
    lv_obj_set_style_radius(m_btnDevices, 6, 0);
    lv_obj_t* lblDev = lv_label_create(m_btnDevices);
    lv_label_set_text(lblDev, "Dispositivos");
    lv_obj_center(lblDev);
    lv_obj_add_event_cb(m_btnDevices, [](lv_event_t* e) {
        auto* self = static_cast<OmniwispView*>(lv_event_get_user_data(e));
        if (self) self->switchTab(OmniTab::Devices);
    }, LV_EVENT_CLICKED, this);

    // Tab 3: Ajustes (100% fluido con flex_grow 1)
    m_btnSettings = lv_button_create(m_tabRow);
    lv_obj_set_flex_grow(m_btnSettings, 1);
    lv_obj_set_height(m_btnSettings, 32);
    lv_obj_set_style_radius(m_btnSettings, 6, 0);
    lv_obj_t* lblSet = lv_label_create(m_btnSettings);
    lv_label_set_text(lblSet, "Ajustes");
    lv_obj_center(lblSet);
    lv_obj_add_event_cb(m_btnSettings, [](lv_event_t* e) {
        auto* self = static_cast<OmniwispView*>(lv_event_get_user_data(e));
        if (self) self->switchTab(OmniTab::Settings);
    }, LV_EVENT_CLICKED, this);
}

void OmniwispView::switchTab(OmniTab tab) {
    m_currentTab = tab;
    m_currentScreen = cbdos::omniwisp::OmniScreen::SectorList;

    auto updateBtn = [](lv_obj_t* btn, bool active) {
        if (!btn) return;
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (active) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0x06080E), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(omni_colors::TEXT_LIGHT), 0);
        }
    };

    updateBtn(m_btnDiscovery, m_currentTab == OmniTab::Discovery);
    updateBtn(m_btnDevices, m_currentTab == OmniTab::Devices);
    updateBtn(m_btnSettings, m_currentTab == OmniTab::Settings);

    m_statusDirty = true;
    refreshUI();
}

bool OmniwispView::onCreate(lv_obj_t* parent) {
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(omni_colors::BG_DARK), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);

    renderTopTabs();

    m_viewContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_viewContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_viewContainer, 1);
    lv_obj_set_style_pad_all(m_viewContainer, 4, 0);
    lv_obj_set_style_border_width(m_viewContainer, 0, 0);
    lv_obj_set_style_bg_opa(m_viewContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(m_viewContainer, LV_FLEX_FLOW_COLUMN);

    loadDataFromVault();

    switchTab(m_currentTab);

    // Timer de UI para reactividad suave y refresco seguro
    m_pollTimer = lv_timer_create([](lv_timer_t* t) {
        auto* v = static_cast<OmniwispView*>(lv_timer_get_user_data(t));
        if (!v) return;

        if (v->m_currentTab == OmniTab::Discovery && v->m_currentScreen == cbdos::omniwisp::OmniScreen::SectorList) {
            static size_t lastCount = 0;
            auto devs = cbdos::network::NeighborDiscoveryService::getInstance().getDiscoveredDevices();
            if (devs.size() != lastCount) {
                lastCount = devs.size();
                OmniwispDiscoveryView::refreshList();
                OmniwispDiscoveryView::updateStatus();
            }
        }
        if (v->m_statusDirty) {
            v->refreshUI();
        }
    }, 300, this);

    m_statusDirty = true;
    refreshUI();
    return true;
}

void OmniwispView::loadDataFromVault() {
    m_groups.clear();
    m_sectors.clear();

    if (!cbdos::security::VaultService::getInstance().isUnlocked()) {
        return;
    }

    // Cargar grupos
    std::vector<cbdos::security::VaultItem> gItems;
    if (cbdos::security::VaultService::getInstance().listItems("ubnt_groups", gItems, false)) {
        for (const auto& it : gItems) {
            cbdos::omniwisp::NetworkGroup g;
            g.id = it.id;
            g.name = it.label;
            g.parentId = it.notes;
            m_groups.push_back(g);
        }
    }

    // Cargar sectores
    std::vector<cbdos::security::VaultItem> sItems;
    if (cbdos::security::VaultService::getInstance().listItems("ubnt_sectors", sItems, true)) {
        for (const auto& it : sItems) {
            cbdos::omniwisp::SectorDevice s;
            s.id = it.id;
            s.name = it.label;
            s.host = it.host;
            s.port = (it.port > 0) ? it.port : 443;
            s.user = it.user.empty() ? "ubnt" : it.user;
            s.pass = it.secret;
            s.groupId = it.notes;
            m_sectors.push_back(s);
        }
    }

    if (m_sectors.empty()) {
        std::vector<cbdos::security::VaultItem> oldItems;
        if (cbdos::security::VaultService::getInstance().listItems("ubnt", oldItems, true)) {
            for (const auto& it : oldItems) {
                cbdos::omniwisp::SectorDevice s;
                s.id = it.id;
                s.name = it.label.empty() ? it.host : it.label;
                s.host = it.host;
                s.port = (it.port > 0) ? it.port : 443;
                s.user = it.user.empty() ? "ubnt" : it.user;
                s.pass = it.secret;
                m_sectors.push_back(s);
            }
        }
    }
}

void OmniwispView::saveDataToVault() {
    if (!cbdos::security::VaultService::getInstance().isUnlocked()) return;

    for (const auto& g : m_groups) {
        cbdos::security::VaultItem it;
        it.id = g.id;
        it.label = g.name;
        it.notes = g.parentId;
        it.type = "omni_group";
        cbdos::security::VaultService::getInstance().saveItem("ubnt_groups", it);
    }

    for (const auto& s : m_sectors) {
        cbdos::security::VaultItem it;
        it.id = s.id;
        it.label = s.name;
        it.host = s.host;
        it.port = s.port;
        it.user = s.user;
        it.secret = s.pass;
        it.notes = s.groupId;
        it.type = "omni_sector";
        cbdos::security::VaultService::getInstance().saveItem("ubnt_sectors", it);
    }
}

void OmniwispView::telemetryWorkerTask(void* param) {
    auto* self = static_cast<OmniwispView*>(param);
    if (!self) {
        cbdos::rtos::deleteTask(nullptr);
        return;
    }

    self->m_telemetry = self->m_client.fetchTelemetry(4500);
    self->m_fetching = false;
    self->m_statusDirty = true;
    self->m_workerTask = nullptr;

    cbdos::rtos::deleteTask(nullptr);
}

void OmniwispView::fetchTelemetryAsync() {
    if (m_fetching || m_selectedSector.host.empty()) return;

    if (!cbdos::network::isConnected()) {
        m_telemetry.success = false;
        m_telemetry.errorMessage = "Wi-Fi Desconectado";
        m_statusDirty = true;
        return;
    }

    m_fetching = true;
    m_client.setTarget(m_selectedSector.host, m_selectedSector.user, m_selectedSector.pass, m_selectedSector.port);

    m_workerTask = cbdos::rtos::createTask(
        telemetryWorkerTask,
        "omni_worker",
        16384,
        this,
        4,
        0
    );

    if (!m_workerTask) {
        m_fetching = false;
        m_telemetry.errorMessage = "Error iniciando worker FreeRTOS";
        m_statusDirty = true;
    }
}

void OmniwispView::refreshUI() {
    if (!m_statusDirty || !m_viewContainer) return;
    m_statusDirty = false;

    switch (m_currentScreen) {
        case cbdos::omniwisp::OmniScreen::SectorList:
        case cbdos::omniwisp::OmniScreen::GroupList: {
            if (m_tabRow) lv_obj_remove_flag(m_tabRow, LV_OBJ_FLAG_HIDDEN);

            if (m_currentTab == OmniTab::Discovery) {
                UIManager::getInstance().getHeaderBar().setTitle("OMNIWISP - DISCOVERY");
                OmniwispDiscoveryView::render(
                    m_viewContainer,
                    // onConnect: conectar al vuelo
                    [this](const std::string& host, const std::string& name) {
                        cbdos::omniwisp::SectorDevice tempSec;
                        tempSec.host = host;
                        tempSec.name = name;
                        tempSec.user = m_settings.defaultUser;
                        tempSec.pass = m_settings.defaultPass;
                        tempSec.port = m_settings.defaultPort;
                        tempSec.id = "disc_" + std::to_string(lv_tick_get());

                        m_selectedSector = tempSec;
                        m_currentScreen = cbdos::omniwisp::OmniScreen::SectorMonitor;
                        m_telemetry = cbdos::network::AirOSTelemetry{};
                        fetchTelemetryAsync();
                        m_statusDirty = true;
                    },
                    // onAdd: abrir modal de agregar con IP y nombre precargados
                    [this](const std::string& host, const std::string& name) {
                        OmniwispAddApModal::show(
                            m_container,
                            m_activeGroupId,
                            [this](const cbdos::omniwisp::SectorDevice& dev) {
                                m_sectors.push_back(dev);
                                saveDataToVault();
                                m_statusDirty = true;
                            },
                            host,
                            name,
                            m_settings.defaultUser,
                            m_settings.defaultPass
                        );
                    }
                );
            } else if (m_currentTab == OmniTab::Devices) {
                UIManager::getInstance().getHeaderBar().setTitle("OMNIWISP - DISPOSITIVOS");
                OmniwispMainView::render(
                    m_viewContainer,
                    m_groups,
                    m_sectors,
                    m_activeGroupId,
                    [this](const std::string& newGId) {
                        m_activeGroupId = newGId;
                        m_statusDirty = true;
                    },
                    [this](const cbdos::omniwisp::SectorDevice& sec) {
                        m_selectedSector = sec;
                        m_currentScreen = cbdos::omniwisp::OmniScreen::SectorMonitor;
                        m_telemetry = cbdos::network::AirOSTelemetry{};
                        fetchTelemetryAsync();
                        m_statusDirty = true;
                    },
                    [this](const std::string& parentId) {
                        OmniwispAddGroupModal::show(m_container, parentId, [this](const cbdos::omniwisp::NetworkGroup& grp) {
                            m_groups.push_back(grp);
                            saveDataToVault();
                            m_statusDirty = true;
                        });
                    },
                    [this](const std::string& defaultGId) {
                        OmniwispAddApModal::show(
                            m_container,
                            defaultGId,
                            [this](const cbdos::omniwisp::SectorDevice& dev) {
                                m_sectors.push_back(dev);
                                saveDataToVault();
                                m_statusDirty = true;
                            },
                            "",
                            "",
                            m_settings.defaultUser,
                            m_settings.defaultPass
                        );
                    }
                );
            } else if (m_currentTab == OmniTab::Settings) {
                UIManager::getInstance().getHeaderBar().setTitle("OMNIWISP - AJUSTES");
                OmniwispSettingsView::render(
                    m_viewContainer,
                    m_settings,
                    [this](const OmniSettings& s) {
                        m_settings = s;
                        saveDataToVault();
                        m_statusDirty = true;
                    }
                );
            }
            break;
        }

        case cbdos::omniwisp::OmniScreen::SectorMonitor: {
            if (m_tabRow) lv_obj_add_flag(m_tabRow, LV_OBJ_FLAG_HIDDEN);
            UIManager::getInstance().getHeaderBar().setTitle(m_selectedSector.name.c_str());
            OmniwispSectorDetailView::render(
                m_viewContainer,
                m_selectedSector,
                m_telemetry,
                m_fetching,
                [this]() {
                    m_currentScreen = cbdos::omniwisp::OmniScreen::SectorList;
                    m_statusDirty = true;
                },
                [this]() {
                    fetchTelemetryAsync();
                    m_statusDirty = true;
                },
                [this](const cbdos::network::AirOSStation& cpe) {
                    m_selectedCpe = cpe;
                    m_currentScreen = cbdos::omniwisp::OmniScreen::CpeDiagnostics;
                    m_statusDirty = true;
                }
            );
            break;
        }

        case cbdos::omniwisp::OmniScreen::CpeDiagnostics: {
            if (m_tabRow) lv_obj_add_flag(m_tabRow, LV_OBJ_FLAG_HIDDEN);
            UIManager::getInstance().getHeaderBar().setTitle(m_selectedCpe.name.empty() ? m_selectedCpe.mac.c_str() : m_selectedCpe.name.c_str());
            OmniwispCpeDetailView::render(
                m_viewContainer,
                m_selectedCpe,
                [this]() {
                    m_currentScreen = cbdos::omniwisp::OmniScreen::SectorMonitor;
                    m_statusDirty = true;
                }
            );
            break;
        }
    }
}

void OmniwispView::onShow() {
    BaseView::onShow();
    if (m_pollTimer) lv_timer_resume(m_pollTimer);
    loadDataFromVault();
    m_statusDirty = true;
    refreshUI();
}

void OmniwispView::onHide() {
    if (m_pollTimer) lv_timer_pause(m_pollTimer);
    if (m_workerTask) {
        cbdos::rtos::deleteTask(m_workerTask);
        m_workerTask = nullptr;
        m_fetching = false;
    }
    BaseView::onHide();
}

} // namespace ui
} // namespace cbdos
