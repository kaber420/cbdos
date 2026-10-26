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

bool OmniwispView::onCreate(lv_obj_t* parent) {
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(omni_colors::BG_DARK), 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);

    m_viewContainer = lv_obj_create(m_container);
    lv_obj_set_size(m_viewContainer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(m_viewContainer, 6, 0);
    lv_obj_set_style_border_width(m_viewContainer, 0, 0);
    lv_obj_set_style_bg_opa(m_viewContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(m_viewContainer, LV_FLEX_FLOW_COLUMN);

    loadDataFromVault();

    // Timer de UI para reactividad suave
    m_pollTimer = lv_timer_create([](lv_timer_t* t) {
        auto* v = static_cast<OmniwispView*>(lv_timer_get_user_data(t));
        if (v) v->refreshUI();
    }, 200, this);

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

    // 1. Cargar grupos (PoPs / Torres / Zonas)
    std::vector<cbdos::security::VaultItem> gItems;
    if (cbdos::security::VaultService::getInstance().listItems("ubnt_groups", gItems, false)) {
        for (const auto& it : gItems) {
            cbdos::omniwisp::NetworkGroup g;
            g.id = it.id;
            g.name = it.label;
            g.parentId = it.notes; // usamos notes para jerarquía padre
            m_groups.push_back(g);
        }
    }

    // 2. Cargar sectores / APs
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
            s.groupId = it.notes; // usamos notes para asociar a groupId
            m_sectors.push_back(s);
        }
    }

    // Compatibilidad retroactiva con namespace "ubnt" viejo si no había sectores
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
    // Si la bóveda está bloqueada no se puede persistir
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

    // Tarea FreeRTOS formal en Core 0 (16KB de stack para TLS/HTTPS)
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
            UIManager::getInstance().getHeaderBar().setTitle("OMNIWISP");
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
                    OmniwispAddApModal::show(m_container, defaultGId, [this](const cbdos::omniwisp::SectorDevice& dev) {
                        m_sectors.push_back(dev);
                        saveDataToVault();
                        m_statusDirty = true;
                    });
                }
            );
            break;
        }

        case cbdos::omniwisp::OmniScreen::SectorMonitor: {
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
