#include "MeshCoreView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/meshcore/meshcore_store.hpp"
#include "cbdos/meshcore/mesh_emoji.hpp"
#include "../../assets/lottie_sample.h"
#include "../../assets/wink_star_assets.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <map>

#include "meshcore/MeshCoreCommon.hpp"
#include <algorithm>
#include <map>
#include <vector>

namespace cbdos {
namespace ui {

using namespace meshcore_ui;


std::string MeshCoreView::loadLastPort() {
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("meshcore", true)) {
        std::string port = backend->getString("port", "jp1");
        backend->end();
        if (!port.empty()) return port;
    }
    return "jp1";
}

void MeshCoreView::saveLastPort(const std::string& portId) {
    if (portId.empty()) return;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("meshcore", false)) {
        backend->setString("port", portId);
        backend->end();
    }
}

MeshCoreView::MeshCoreView()
    : BaseView("MeshCore"),
      m_selectedPort("jp1"),
      m_selectedBaud(115200) {
}

MeshCoreView::~MeshCoreView() {
    onDestroy();
}

bool MeshCoreView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    UIManager::getInstance().getHeaderBar().setTitle("MeshCore");
    UIManager::getInstance().getHeaderBar().showWifi(false);

    // Puerto recordado (NVS): no hay que ir a Radio en cada arranque.
    m_selectedPort = loadLastPort();

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 4, 0);
    DefaultTheme::disableScroll(m_container);

    m_tabview = lv_tabview_create(m_container);
    lv_tabview_set_tab_bar_position(m_tabview, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(m_tabview, 52);
    lv_obj_set_size(m_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_tabview, 0, 0);
    lv_obj_set_style_border_width(m_tabview, 0, 0);

    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_pad_column(tab_bar, 4, 0);

    lv_obj_t* tab_contacts = lv_tabview_add_tab(m_tabview, LV_SYMBOL_CALL " Contactos");
    lv_obj_t* tab_channels = lv_tabview_add_tab(m_tabview, LV_SYMBOL_LIST " Canales");
    lv_obj_t* tab_chat = lv_tabview_add_tab(m_tabview, LV_SYMBOL_EDIT " Chat");
    lv_obj_t* tab_map = lv_tabview_add_tab(m_tabview, LV_SYMBOL_GPS " Mapa");
    lv_obj_t* tab_radio = lv_tabview_add_tab(m_tabview, LV_SYMBOL_WIFI " Radio");

    buildContactsTab(tab_contacts);
    buildChannelsTab(tab_channels);
    buildChatsTab(tab_chat);  // chat del canal activo, a pantalla completa
    buildMapTab(tab_map);
    buildRadioTab(tab_radio);

    auto& client = meshcore::MeshCoreClient::getInstance();

    client.setOnChannelMessage([this](const meshcore::ChannelMessage&) { m_chatDirty = true; });
    client.setOnContactMessage([this](const meshcore::ContactMessage&) { m_chatDirty = true; });
    client.setOnMsgSent([this](const meshcore::MsgSentInfo&) { m_chatDirty = true; });
    client.setOnChannelInfo([this](const meshcore::MeshChannel&) {
        m_channelsDirty = true;
        m_chatDirty = true;
    });
    client.setOnSelfInfo([this](const meshcore::SelfInfo&) { m_radioDirty = true; });
    client.setOnDeviceInfo([this](const meshcore::DeviceInfo&) { m_radioDirty = true; });
    client.setOnBattery([this](const meshcore::BatteryInfo&) { m_radioDirty = true; });
    client.setOnError([this](meshcore::ErrCode e) {
        m_pendingError = errName(e);
        m_radioDirty = true;
    });
    client.setOnConnectionState([this](bool) { m_radioDirty = true; });
    client.setOnContactsChanged([this]() { m_contactsDirty = true; });
    client.setOnAck([this](uint32_t, bool delivered) {
        m_chatDirty = true;
        m_contactsDirty = true;
        if (!delivered) m_pendingError = "DM no entregado (sin ACK). Reintenta.";
    });
    client.setOnAdvert([this](const meshcore::MeshContact&) { m_contactsDirty = true; });

    // Agenda cacheada (offline-first): arranca sin dongle con lo guardado.
    {
        std::vector<meshcore::MeshContact> cached;
        if (meshcore::MeshStore::loadContacts(cached) && !cached.empty()) {
            client.setContactsForTest(cached);
        }
        std::map<std::string, meshcore::DMThread> threads;
        if (meshcore::MeshStore::loadThreads(threads) && !threads.empty()) {
            client.setThreadsForTest(threads);
        }
        m_contactsDirty = true;
        m_chatDirty = true;
    }

    if (!client.isConnected()) {
        if (client.connect(m_selectedPort, m_selectedBaud)) {
            saveLastPort(m_selectedPort);
        }
    }
    // Estado inicial: canales, agenda, batería y drenado de mensajes en cola.
    client.queryAllChannels();
    client.queryContacts();
    client.queryBattery();
    client.pollMessages();

    setActiveChannel(0);
    refreshChatLog();
    refreshChannelsList();
    refreshRadioStatus();

    // Autoconexión al (des)conectar el USB: si aparece el puerto recordado
    // y no hay enlace, conecta solo; si cae el activo, refresca estado.
    cbdos::serial::setHotplugCallback([this](bool connected, const std::string& portId) {
        auto& client = meshcore::MeshCoreClient::getInstance();
        if (connected && !client.isConnected() && portId == m_selectedPort) {
            if (client.connect(m_selectedPort, m_selectedBaud)) {
                client.queryAllChannels();
                client.queryContacts();
                client.queryBattery();
                client.pollMessages();
                UIManager::showToast("Dongle MeshCore reconectado");
            }
        } else if (!connected && client.getActivePort() == portId) {
            client.disconnect();
        }
        m_radioDirty = true;
    });

    m_pumpTimer = lv_timer_create(timerPumpCb, 40, this);

    return true;
}

void MeshCoreView::onDestroy() {
    if (m_pumpTimer) {
        lv_timer_delete(m_pumpTimer);
        m_pumpTimer = nullptr;
    }
    // POC Winks inline: al salir de la vista mueren objetos+anims y se
    // liberan los draw bufs de ambos contenedores -> cero CPU fuera.
    freeChatWinkBufs();
    freeConvWinkBufs();
    m_catJson.clear();
    m_starSd.clear();

    auto& client = meshcore::MeshCoreClient::getInstance();
    client.setOnChannelMessage(nullptr);
    client.setOnContactMessage(nullptr);
    client.setOnMsgSent(nullptr);
    client.setOnChannelInfo(nullptr);
    client.setOnSelfInfo(nullptr);
    client.setOnDeviceInfo(nullptr);
    client.setOnBattery(nullptr);
    client.setOnError(nullptr);
    client.setOnMessagesWaiting(nullptr);
    client.setOnConnectionState(nullptr);
    client.setOnContactsChanged(nullptr);
    client.setOnAck(nullptr);
    client.setOnAdvert(nullptr);
    cbdos::serial::setHotplugCallback(nullptr);

    m_tabview = nullptr;
    m_chatContainer = nullptr;
    m_taInput = nullptr;
    m_taDmInput = nullptr;
    m_btnDmKb = nullptr;
    m_channelsContainer = nullptr;

    UIManager::getInstance().getHeaderBar().showWifi(true);
    BaseView::onDestroy();
}

void MeshCoreView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

// ────────────────────────────────────────────────────────────────
// Pestaña 1: Chats por canal
// ────────────────────────────────────────────────────────────────


void MeshCoreView::timerPumpCb(lv_timer_t* timer) {
    auto* self = static_cast<MeshCoreView*>(lv_timer_get_user_data(timer));
    if (!self) return;

    meshcore::MeshCoreClient::getInstance().process();

    if (!self->m_pendingError.empty()) {
        UIManager::showToast(self->m_pendingError.c_str());
        self->m_pendingError.clear();
    }

    if (self->m_chatDirty) {
        self->m_chatDirty = false;
        self->refreshChatLog();
        // El nombre del canal puede haberse conocido después.
        self->updateChannelLabel();
        if (self->m_contactsPane == ContactsPane::Conversation) {
            self->refreshConversation();
        }
    }

    if (self->m_channelsDirty) {
        self->m_channelsDirty = false;
        self->refreshChannelsList();
    }

    if (self->m_radioDirty) {
        self->m_radioDirty = false;
        self->refreshRadioStatus();
    }

    if (self->m_contactsDirty) {
        self->m_contactsDirty = false;
        self->refreshContactsList();
        self->refreshMapList();
        if (self->m_contactsPane == ContactsPane::Conversation) {
            self->refreshConversation();
        } else if (self->m_contactsPane == ContactsPane::Details) {
            self->refreshDetails();
        }
    }

    // Persistencia coalescente de agenda + hilos (offline-first).
    self->autosaveCache((uint32_t)((uint64_t)time(nullptr) * 1000u));
}

void MeshCoreView::autosaveCache(uint32_t nowMs) {
    auto& client = meshcore::MeshCoreClient::getInstance();
    bool dirty = client.isContactsDirty() || client.isChatDirty();
    if (!meshcore::MeshStore::shouldSave(dirty, nowMs, m_lastStoreSaveMs)) return;
    meshcore::MeshStore::saveContacts(client.getContacts());
    std::map<std::string, meshcore::DMThread> threads;
    for (const auto& t : client.getAllThreads()) threads[t.prefixHex12] = t;
    meshcore::MeshStore::saveThreads(threads);
    client.clearContactsDirty();
    client.clearChatDirty();
    m_lastStoreSaveMs = nowMs;
}

// — Callbacks estáticos Fase 2 —


} // namespace ui
} // namespace cbdos
