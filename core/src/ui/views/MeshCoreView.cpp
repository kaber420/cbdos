#include "MeshCoreView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/meshcore/meshcore_store.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <map>

namespace cbdos {
namespace ui {

namespace {
const char* errName(meshcore::ErrCode e) {
    switch (e) {
        case meshcore::ErrCode::UNSUPPORTED_CMD: return "Comando no soportado";
        case meshcore::ErrCode::NOT_FOUND:       return "No encontrado";
        case meshcore::ErrCode::TABLE_FULL:      return "Tabla llena";
        case meshcore::ErrCode::BAD_STATE:       return "Estado invalido";
        case meshcore::ErrCode::FILE_IO_ERROR:   return "Error de E/S";
        case meshcore::ErrCode::ILLEGAL_ARG:     return "Argumento invalido";
    }
    return "Error desconocido";
}

std::string fmtTime(uint32_t epoch) {
    if (epoch == 0) return "--:--";
    time_t t = static_cast<time_t>(epoch);
    struct tm* tmv = localtime(&t);
    if (!tmv) return "--:--";
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv->tm_hour, tmv->tm_min);
    return buf;
}

// Definido abajo (sección Fase 2); se usa en buildRadioTab.
lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int w, int h, uint32_t bg);

std::string fmtAgo(uint32_t epoch) {
    if (epoch == 0) return "nunca";
    time_t now = time(nullptr);
    if (now < (time_t)epoch) return "ahora";
    uint32_t dt = (uint32_t)(now - (time_t)epoch);
    char buf[32];
    if (dt < 90) snprintf(buf, sizeof(buf), "hace %us", (unsigned)dt);
    else if (dt < 5400) snprintf(buf, sizeof(buf), "hace %u min", (unsigned)(dt / 60));
    else if (dt < 172800) snprintf(buf, sizeof(buf), "hace %u h", (unsigned)(dt / 3600));
    else snprintf(buf, sizeof(buf), "hace %u d", (unsigned)(dt / 86400));
    return buf;
}

double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double kR = 6371.0;
    const double kD = 3.141592653589793 / 180.0;
    double dLa = (lat2 - lat1) * kD;
    double dLo = (lon2 - lon1) * kD;
    double a = sin(dLa / 2) * sin(dLa / 2) +
               cos(lat1 * kD) * cos(lat2 * kD) * sin(dLo / 2) * sin(dLo / 2);
    return 2 * kR * asin(sqrt(a > 1.0 ? 1.0 : a));
}

const char* contactIcon(uint8_t type) {
    switch (static_cast<meshcore::ContactType>(type)) {
        case meshcore::ContactType::Repeater: return LV_SYMBOL_WIFI;
        case meshcore::ContactType::Room:     return LV_SYMBOL_HOME;
        case meshcore::ContactType::Sensor:   return LV_SYMBOL_CHARGE;
        default:                              return LV_SYMBOL_CALL;
    }
}

// Formatea MHz/kHz con decimales recortando ceros ("910.525", "62.5").
std::string trimNum(double v, int decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    std::string s = buf;
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

bool parseDec(const char* txt, double& out) {
    if (!txt || !*txt) return false;
    char* end = nullptr;
    out = strtod(txt, &end);
    return end != txt && *end == '\0';
}
} // namespace

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
    m_keyboard = nullptr;
    m_taDmInput = nullptr;
    m_btnDmKb = nullptr;
    m_keyboardDm = nullptr;
    m_keyboardDmVisible = false;
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

void MeshCoreView::buildChatsTab(lv_obj_t* tab) {
    auto caps = cbdos::display::getCapabilities();

    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 4, 0);
    lv_obj_set_style_pad_row(tab, 4, 0);
    DefaultTheme::disableScroll(tab);

    // Barra de canal activo (0-7)
    lv_obj_t* chanBar = lv_obj_create(tab);
    lv_obj_set_size(chanBar, LV_PCT(100), 36);
    DefaultTheme::applySunkenCard(chanBar, 6);
    lv_obj_set_flex_flow(chanBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chanBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(chanBar, 8, 0);
    lv_obj_set_style_pad_ver(chanBar, 2, 0);
    DefaultTheme::disableScroll(chanBar);

    m_lblChannel = lv_label_create(chanBar);
    lv_label_set_text(m_lblChannel, "Canal 0");
    lv_obj_set_style_text_font(m_lblChannel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblChannel, lv_color_hex(0x00E5FF), 0);

    m_ddChannel = lv_dropdown_create(chanBar);
    lv_obj_set_size(m_ddChannel, 96, 30);
    DefaultTheme::applySunkenCard(m_ddChannel, 6);
    lv_dropdown_set_options(m_ddChannel, "Canal 0\nCanal 1\nCanal 2\nCanal 3\nCanal 4\nCanal 5\nCanal 6\nCanal 7");
    lv_obj_add_event_cb(m_ddChannel, channelDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    // Contenedor scrolleable de mensajes
    m_chatContainer = lv_obj_create(tab);
    lv_obj_set_width(m_chatContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_chatContainer, 1);
    DefaultTheme::applySunkenCard(m_chatContainer, 8);
    lv_obj_set_style_bg_color(m_chatContainer, lv_color_hex(0x0E1218), 0);
    lv_obj_set_flex_flow(m_chatContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_chatContainer, 6, 0);
    lv_obj_set_style_pad_row(m_chatContainer, 6, 0);
    lv_obj_set_scroll_dir(m_chatContainer, LV_DIR_VER);

    // Barra de entrada inferior
    lv_obj_t* inputRow = lv_obj_create(tab);
    lv_obj_set_size(inputRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(inputRow, 0, 0);
    lv_obj_set_style_border_width(inputRow, 0, 0);
    lv_obj_set_flex_flow(inputRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(inputRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(inputRow, 0, 0);
    DefaultTheme::disableScroll(inputRow);

    m_btnKb = lv_button_create(inputRow);
    lv_obj_set_size(m_btnKb, 36, 36);
    DefaultTheme::applyButton(m_btnKb, 6);
    lv_obj_t* lblKb = lv_label_create(m_btnKb);
    lv_label_set_text(lblKb, LV_SYMBOL_KEYBOARD);
    lv_obj_center(lblKb);
    lv_obj_add_event_cb(m_btnKb, toggleKbBtnCb, LV_EVENT_CLICKED, this);

    m_taInput = lv_textarea_create(inputRow);
    lv_obj_set_flex_grow(m_taInput, 1);
    lv_obj_set_height(m_taInput, 36);
    DefaultTheme::applySunkenCard(m_taInput, 6);
    lv_textarea_set_one_line(m_taInput, true);
    lv_textarea_set_placeholder_text(m_taInput, "Mensaje al canal...");
    lv_obj_set_style_text_font(m_taInput, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_taInput, lv_color_hex(0xF0F4F8), 0);
    lv_obj_set_style_text_color(m_taInput, lv_color_hex(0x94A3B8), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(m_taInput, lv_color_hex(0x00F5D4), LV_PART_CURSOR);
    lv_obj_add_event_cb(m_taInput, taInputEventCb, LV_EVENT_ALL, this);

    m_btnSend = lv_button_create(inputRow);
    lv_obj_set_size(m_btnSend, 64, 36);
    DefaultTheme::applyButton(m_btnSend, 6);
    lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(lblSend, LV_SYMBOL_OK " Enviar");
    lv_obj_set_style_text_font(lblSend, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSend);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);

    m_keyboard = lv_keyboard_create(tab);
    int32_t kbHeight = (caps.height >= 800) ? 280 : 190;
    lv_obj_set_size(m_keyboard, LV_PCT(100), kbHeight);
    lv_obj_set_style_bg_color(m_keyboard, lv_color_hex(0x1A1E29), 0);
    lv_keyboard_set_textarea(m_keyboard, m_taInput);
    lv_obj_add_event_cb(m_keyboard, kbEventCb, LV_EVENT_ALL, this);
    lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    m_keyboardVisible = false;
}

void MeshCoreView::refreshChatLog() {
    if (!m_chatContainer || !lv_obj_is_valid(m_chatContainer)) return;

    auto& client = meshcore::MeshCoreClient::getInstance();
    const auto& chHist = client.getChannelHistory();

    // Solo mensajes del canal activo (los DM viven en su conversación).
    size_t shown = 0;
    for (const auto& m : chHist) {
        if (m.channelIndex == m_channelIdx) ++shown;
    }

    if (shown == m_lastRenderedChCount && m_lastRenderedChannel == m_channelIdx &&
        (shown > 0)) {
        return;
    }

    lv_obj_clean(m_chatContainer);
    m_lastRenderedChCount = shown;
    m_lastRenderedChannel = m_channelIdx;
    // m_lastRenderedDmCount ya no se usa aquí (DMs en conversación).
    m_lastRenderedDmCount = client.getContactHistory().size();

    if (shown == 0) {
        lv_obj_t* emptyLbl = lv_label_create(m_chatContainer);
        const auto& ch = client.getChannelInfo(m_channelIdx);
        char buf[96];
        if (ch.known && !ch.name.empty()) {
            snprintf(buf, sizeof(buf), "Canal %d: %s\nSin mensajes. Escribe abajo para enviar.",
                     (int)m_channelIdx, ch.name.c_str());
        } else {
            snprintf(buf, sizeof(buf),
                     "Canal %d sin configurar o sin mensajes.\nDefine nombre+secreto en la lista de arriba.",
                     (int)m_channelIdx);
        }
        lv_label_set_text(emptyLbl, buf);
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (const auto& msg : chHist) {
        if (msg.channelIndex != m_channelIdx) continue;
        lv_obj_t* bubble = lv_obj_create(m_chatContainer);
        lv_obj_set_width(bubble, LV_PCT(85));
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(bubble, 8, 0);
        lv_obj_set_style_pad_all(bubble, 6, 0);
        lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(bubble, 2, 0);
        DefaultTheme::disableScroll(bubble);

        if (msg.outgoing) {
            lv_obj_set_align(bubble, LV_ALIGN_TOP_RIGHT);
            lv_obj_set_style_bg_color(bubble, lv_color_hex(0x13382C), 0);
            lv_obj_set_style_border_color(bubble, lv_color_hex(0x1F6B52), 0);
            lv_obj_set_style_border_width(bubble, 1, 0);

            lv_obj_t* lblSender = lv_label_create(bubble);
            char hdr[48];
            snprintf(hdr, sizeof(hdr), "Tu > Canal %d", (int)msg.channelIndex);
            lv_label_set_text(lblSender, hdr);
            lv_obj_set_style_text_color(lblSender, lv_color_hex(0x00E676), 0);
            lv_obj_set_style_text_font(lblSender, &lv_font_montserrat_12, 0);
            lv_obj_set_width(lblSender, LV_PCT(100));
        } else {
            lv_obj_set_align(bubble, LV_ALIGN_TOP_LEFT);
            lv_obj_set_style_bg_color(bubble, lv_color_hex(0x1A2234), 0);
            lv_obj_set_style_border_color(bubble, lv_color_hex(0x283854), 0);
            lv_obj_set_style_border_width(bubble, 1, 0);

            lv_obj_t* lblSender = lv_label_create(bubble);
            char hdr[64];
            snprintf(hdr, sizeof(hdr), "Canal %d - %s", (int)msg.channelIndex,
                     fmtTime(msg.timestamp).c_str());
            lv_label_set_text(lblSender, hdr);
            lv_obj_set_style_text_color(lblSender, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_text_font(lblSender, &lv_font_montserrat_12, 0);
            lv_obj_set_width(lblSender, LV_PCT(100));
        }

        lv_obj_t* lblText = lv_label_create(bubble);
        lv_label_set_text(lblText, msg.text.c_str());
        lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lblText, LV_PCT(100));
        lv_obj_set_style_text_color(lblText, lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_style_text_font(lblText, &lv_font_montserrat_12, 0);
        if (msg.hasSnr) {
            lv_obj_t* lblMeta = lv_label_create(bubble);
            char meta[48];
            snprintf(meta, sizeof(meta), "SNR %.1f dB | %u saltos", (double)msg.snrDb,
                     (unsigned)msg.pathLength);
            lv_label_set_text(lblMeta, meta);
            lv_obj_set_style_text_color(lblMeta, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_text_font(lblMeta, &lv_font_montserrat_12, 0);
            lv_obj_set_width(lblMeta, LV_PCT(100));
        }
    }

    if (lv_obj_get_child_cnt(m_chatContainer) > 0) {
        lv_obj_scroll_to_view(lv_obj_get_child(m_chatContainer, -1), LV_ANIM_OFF);
    }
}

// ────────────────────────────────────────────────────────────────
// Pestaña 2: Canales
// ────────────────────────────────────────────────────────────────

void MeshCoreView::buildChannelsTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 4, 0);
    lv_obj_set_style_pad_row(tab, 4, 0);
    DefaultTheme::disableScroll(tab);

    lv_obj_t* topBar = lv_obj_create(tab);
    lv_obj_set_size(topBar, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(topBar, 6);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(topBar, 8, 0);
    DefaultTheme::disableScroll(topBar);

    m_lblChannelsCount = lv_label_create(topBar);
    lv_label_set_text(m_lblChannelsCount, "Canales: 0/8 conocidos");
    lv_obj_set_style_text_font(m_lblChannelsCount, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblChannelsCount, DefaultTheme::getTextColor(), 0);

    lv_obj_t* btnScan = lv_button_create(topBar);
    lv_obj_set_size(btnScan, 100, 26);
    DefaultTheme::applyButton(btnScan, 4);
    lv_obj_t* lblScan = lv_label_create(btnScan);
    lv_label_set_text(lblScan, LV_SYMBOL_REFRESH " Leer");
    lv_obj_set_style_text_font(lblScan, &lv_font_montserrat_12, 0);
    lv_obj_center(lblScan);
    lv_obj_add_event_cb(btnScan, refreshChannelsBtnCb, LV_EVENT_CLICKED, this);

    m_channelsContainer = lv_obj_create(tab);
    lv_obj_set_width(m_channelsContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_channelsContainer, 1);
    DefaultTheme::applySunkenCard(m_channelsContainer, 8);
    lv_obj_set_flex_flow(m_channelsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_channelsContainer, 6, 0);
    lv_obj_set_style_pad_row(m_channelsContainer, 4, 0);
    lv_obj_set_scroll_dir(m_channelsContainer, LV_DIR_VER);

    // Formulario de creación / borrado
    lv_obj_t* form = lv_obj_create(tab);
    lv_obj_set_size(form, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(form, 8);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(form, 6, 0);
    lv_obj_set_style_pad_row(form, 4, 0);
    DefaultTheme::disableScroll(form);

    lv_obj_t* rowIdx = lv_obj_create(form);
    lv_obj_set_size(rowIdx, LV_PCT(100), 32);
    lv_obj_set_style_bg_opa(rowIdx, 0, 0);
    lv_obj_set_style_border_width(rowIdx, 0, 0);
    lv_obj_set_flex_flow(rowIdx, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowIdx, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowIdx, 0, 0);
    DefaultTheme::disableScroll(rowIdx);

    lv_obj_t* lblNew = lv_label_create(rowIdx);
    lv_label_set_text(lblNew, "Nuevo / editar canal:");
    lv_obj_set_style_text_font(lblNew, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblNew, lv_color_hex(0x00E5FF), 0);

    m_ddNewIdx = lv_dropdown_create(rowIdx);
    lv_obj_set_size(m_ddNewIdx, 72, 28);
    DefaultTheme::applySunkenCard(m_ddNewIdx, 6);
    lv_dropdown_set_options(m_ddNewIdx, "0\n1\n2\n3\n4\n5\n6\n7");
    lv_dropdown_set_selected(m_ddNewIdx, 1);
    lv_obj_add_event_cb(m_ddNewIdx, newIdxDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    m_taNewName = lv_textarea_create(form);
    lv_obj_set_size(m_taNewName, LV_PCT(100), 32);
    DefaultTheme::applySunkenCard(m_taNewName, 6);
    lv_textarea_set_one_line(m_taNewName, true);
    lv_textarea_set_placeholder_text(m_taNewName, "Nombre (vacio = borrar)");
    lv_obj_set_style_text_font(m_taNewName, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taNewName);

    m_taNewSecret = lv_textarea_create(form);
    lv_obj_set_size(m_taNewSecret, LV_PCT(100), 32);
    DefaultTheme::applySunkenCard(m_taNewSecret, 6);
    lv_textarea_set_one_line(m_taNewSecret, true);
    lv_textarea_set_placeholder_text(m_taNewSecret, "Secreto hex 32 chars (vacio + #nombre = hashtag)");
    lv_obj_set_style_text_font(m_taNewSecret, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taNewSecret);

    lv_obj_t* rowBtns = lv_obj_create(form);
    lv_obj_set_size(rowBtns, LV_PCT(100), 32);
    lv_obj_set_style_bg_opa(rowBtns, 0, 0);
    lv_obj_set_style_border_width(rowBtns, 0, 0);
    lv_obj_set_flex_flow(rowBtns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(rowBtns, 0, 0);
    lv_obj_set_style_pad_column(rowBtns, 6, 0);
    DefaultTheme::disableScroll(rowBtns);

    lv_obj_t* btnCreate = lv_button_create(rowBtns);
    lv_obj_set_size(btnCreate, 110, 30);
    DefaultTheme::applyButton(btnCreate, 4);
    lv_obj_set_style_bg_color(btnCreate, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblC = lv_label_create(btnCreate);
    lv_label_set_text(lblC, LV_SYMBOL_OK " Guardar");
    lv_obj_set_style_text_font(lblC, &lv_font_montserrat_12, 0);
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCreate, channelCreateBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnClear = lv_button_create(rowBtns);
    lv_obj_set_size(btnClear, 100, 30);
    DefaultTheme::applyButton(btnClear, 4);
    lv_obj_set_style_bg_color(btnClear, lv_color_hex(0x6A1B1B), 0);
    lv_obj_t* lblD = lv_label_create(btnClear);
    lv_label_set_text(lblD, LV_SYMBOL_TRASH " Borrar");
    lv_obj_set_style_text_font(lblD, &lv_font_montserrat_12, 0);
    lv_obj_center(lblD);
    lv_obj_add_event_cb(btnClear, channelClearBtnCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::refreshChannelsList() {
    if (!m_channelsContainer || !lv_obj_is_valid(m_channelsContainer)) return;

    auto& client = meshcore::MeshCoreClient::getInstance();
    int known = 0;
    for (uint8_t i = 0; i < meshcore::CHANNEL_COUNT; ++i) {
        if (client.getChannelInfo(i).known) ++known;
    }

    if (m_lblChannelsCount && lv_obj_is_valid(m_lblChannelsCount)) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Canales: %d/8 conocidos", known);
        lv_label_set_text(m_lblChannelsCount, buf);
    }

    lv_obj_clean(m_channelsContainer);

    for (uint8_t i = 0; i < meshcore::CHANNEL_COUNT; ++i) {
        const auto& ch = client.getChannelInfo(i);

        lv_obj_t* card = lv_button_create(m_channelsContainer);
        lv_obj_set_size(card, LV_PCT(100), 44);
        DefaultTheme::applyButton(card, 8);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(card, 8, 0);
        lv_obj_set_user_data(card, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(card, channelUseBtnCb, LV_EVENT_CLICKED, this);

        lv_obj_t* colLeft = lv_obj_create(card);
        lv_obj_set_size(colLeft, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(colLeft, 0, 0);
        lv_obj_set_style_border_width(colLeft, 0, 0);
        lv_obj_set_flex_flow(colLeft, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(colLeft, 0, 0);
        DefaultTheme::disableScroll(colLeft);

        lv_obj_t* lblName = lv_label_create(colLeft);
        char nameBuf[48];
        if (!ch.known) {
            snprintf(nameBuf, sizeof(nameBuf), "%d - sin leer", (int)i);
        } else if (ch.name.empty()) {
            snprintf(nameBuf, sizeof(nameBuf), "%d - (vacio)", (int)i);
        } else {
            snprintf(nameBuf, sizeof(nameBuf), "%d - %s", (int)i, ch.name.c_str());
        }
        lv_label_set_text(lblName, nameBuf);
        lv_obj_set_style_text_font(lblName, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblName, (i == m_channelIdx) ? lv_color_hex(0x00E676) : lv_color_hex(0x00E5FF), 0);

        lv_obj_t* lblKind = lv_label_create(colLeft);
        lv_label_set_text(lblKind, meshcore::MeshCoreClient::channelKindName(ch.kind));
        lv_obj_set_style_text_font(lblKind, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblKind, DefaultTheme::getMutedTextColor(), 0);

        lv_obj_t* lblUse = lv_label_create(card);
        lv_label_set_text(lblUse, (i == m_channelIdx) ? LV_SYMBOL_OK " Activo" : "Usar " LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(lblUse, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblUse, lv_color_hex(0x00E676), 0);
    }
}

// ────────────────────────────────────────────────────────────────
// Pestaña 3: Radio y enlace serial
// ────────────────────────────────────────────────────────────────

void MeshCoreView::buildRadioTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_row(tab, 8, 0);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);

    // Sección 1: enlace serial
    lv_obj_t* secConn = lv_obj_create(tab);
    lv_obj_set_size(secConn, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secConn, 8);
    lv_obj_set_flex_flow(secConn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secConn, 8, 0);
    lv_obj_set_style_pad_row(secConn, 6, 0);
    DefaultTheme::disableScroll(secConn);

    lv_obj_t* lblConnTitle = lv_label_create(secConn);
    lv_label_set_text(lblConnTitle, LV_SYMBOL_SETTINGS " Enlace Serial / Dongle");
    lv_obj_set_style_text_font(lblConnTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblConnTitle, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* rowPort = lv_obj_create(secConn);
    lv_obj_set_size(rowPort, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(rowPort, 0, 0);
    lv_obj_set_style_border_width(rowPort, 0, 0);
    lv_obj_set_flex_flow(rowPort, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowPort, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowPort, 0, 0);
    DefaultTheme::disableScroll(rowPort);

    m_ddPort = lv_dropdown_create(rowPort);
    lv_obj_set_size(m_ddPort, 140, 36);
    DefaultTheme::applySunkenCard(m_ddPort, 6);

    auto ports = cbdos::serial::getAvailablePorts();
    std::string portOpts = "jp1\nusb0\next_s3";
    m_portIds = {"jp1", "usb0", "ext_s3"};
    if (!ports.empty()) {
        portOpts.clear();
        m_portIds.clear();
        for (size_t p = 0; p < ports.size(); ++p) {
            if (p > 0) portOpts += "\n";
            portOpts += ports[p].id;
            m_portIds.push_back(ports[p].id);
        }
    }
    lv_dropdown_set_options(m_ddPort, portOpts.c_str());
    for (size_t i = 0; i < m_portIds.size(); ++i) {
        if (m_portIds[i] == m_selectedPort) {
            lv_dropdown_set_selected(m_ddPort, (uint16_t)i);
            break;
        }
    }
    lv_obj_add_event_cb(m_ddPort, portDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    m_btnConnect = lv_button_create(rowPort);
    lv_obj_set_size(m_btnConnect, 110, 36);
    DefaultTheme::applyButton(m_btnConnect, 6);
    lv_obj_t* lblConn = lv_label_create(m_btnConnect);
    lv_label_set_text(lblConn, "Reconectar");
    lv_obj_set_style_text_font(lblConn, &lv_font_montserrat_12, 0);
    lv_obj_center(lblConn);
    lv_obj_add_event_cb(m_btnConnect, connectBtnCb, LV_EVENT_CLICKED, this);

    m_lblConnStatus = lv_label_create(secConn);
    lv_label_set_text(m_lblConnStatus, "Estado: --");
    lv_obj_set_style_text_font(m_lblConnStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0x00E676), 0);

    // Sección 2: identidad y firmware
    lv_obj_t* secDev = lv_obj_create(tab);
    lv_obj_set_size(secDev, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secDev, 8);
    lv_obj_set_flex_flow(secDev, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secDev, 8, 0);
    lv_obj_set_style_pad_row(secDev, 6, 0);
    DefaultTheme::disableScroll(secDev);

    lv_obj_t* lblDevTitle = lv_label_create(secDev);
    lv_label_set_text(lblDevTitle, LV_SYMBOL_CHARGE " Dongle MeshCore");
    lv_obj_set_style_text_font(lblDevTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDevTitle, lv_color_hex(0x00E5FF), 0);

    m_lblDeviceDetails = lv_label_create(secDev);
    lv_label_set_text(m_lblDeviceDetails, "Nombre: --\nFirmware: --");
    lv_obj_set_style_text_font(m_lblDeviceDetails, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDeviceDetails, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_width(m_lblDeviceDetails, LV_PCT(100));

    m_lblBattery = lv_label_create(secDev);
    lv_label_set_text(m_lblBattery, "Bateria: --");
    lv_obj_set_style_text_font(m_lblBattery, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblBattery, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_width(m_lblBattery, LV_PCT(100));

    // Alias del nodo (CLI: set name)
    lv_obj_t* rowAlias = lv_obj_create(secDev);
    lv_obj_set_size(rowAlias, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(rowAlias, 0, 0);
    lv_obj_set_style_border_width(rowAlias, 0, 0);
    lv_obj_set_flex_flow(rowAlias, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowAlias, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rowAlias, 0, 0);
    DefaultTheme::disableScroll(rowAlias);

    m_taAlias = lv_textarea_create(rowAlias);
    lv_obj_set_flex_grow(m_taAlias, 1);
    lv_obj_set_height(m_taAlias, 32);
    DefaultTheme::applySunkenCard(m_taAlias, 6);
    lv_textarea_set_one_line(m_taAlias, true);
    lv_textarea_set_placeholder_text(m_taAlias, "Alias del nodo");
    lv_obj_set_style_text_font(m_taAlias, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taAlias);

    lv_obj_t* btnAlias = lv_button_create(rowAlias);
    lv_obj_set_size(btnAlias, 90, 32);
    DefaultTheme::applyButton(btnAlias, 4);
    lv_obj_t* lblA = lv_label_create(btnAlias);
    lv_label_set_text(lblA, "Aplicar");
    lv_obj_set_style_text_font(lblA, &lv_font_montserrat_12, 0);
    lv_obj_center(lblA);
    lv_obj_add_event_cb(btnAlias, aliasApplyBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* rowDevBtns = lv_obj_create(secDev);
    lv_obj_set_size(rowDevBtns, LV_PCT(100), 34);
    lv_obj_set_style_bg_opa(rowDevBtns, 0, 0);
    lv_obj_set_style_border_width(rowDevBtns, 0, 0);
    lv_obj_set_flex_flow(rowDevBtns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(rowDevBtns, 0, 0);
    lv_obj_set_style_pad_column(rowDevBtns, 6, 0);
    DefaultTheme::disableScroll(rowDevBtns);

    lv_obj_t* btnInfo = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnInfo, 90, 30);
    DefaultTheme::applyButton(btnInfo, 4);
    lv_obj_t* lblQ = lv_label_create(btnInfo);
    lv_label_set_text(lblQ, LV_SYMBOL_REFRESH " Info");
    lv_obj_set_style_text_font(lblQ, &lv_font_montserrat_12, 0);
    lv_obj_center(lblQ);
    lv_obj_add_event_cb(btnInfo, infoBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnBatt = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnBatt, 100, 30);
    DefaultTheme::applyButton(btnBatt, 4);
    lv_obj_t* lblB = lv_label_create(btnBatt);
    lv_label_set_text(lblB, LV_SYMBOL_CHARGE " Bateria");
    lv_obj_set_style_text_font(lblB, &lv_font_montserrat_12, 0);
    lv_obj_center(lblB);
    lv_obj_add_event_cb(btnBatt, batteryBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnPoll = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnPoll, 90, 30);
    DefaultTheme::applyButton(btnPoll, 4);
    lv_obj_t* lblPl = lv_label_create(btnPoll);
    lv_label_set_text(lblPl, LV_SYMBOL_DOWNLOAD " Poll");
    lv_obj_set_style_text_font(lblPl, &lv_font_montserrat_12, 0);
    lv_obj_center(lblPl);
    lv_obj_add_event_cb(btnPoll, pollBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnReboot = lv_button_create(rowDevBtns);
    lv_obj_set_size(btnReboot, 100, 30);
    DefaultTheme::applyButton(btnReboot, 4);
    lv_obj_set_style_bg_color(btnReboot, lv_color_hex(0x6A1B1B), 0);
    lv_obj_t* lblR = lv_label_create(btnReboot);
    lv_label_set_text(lblR, LV_SYMBOL_POWER " Reboot");
    lv_obj_set_style_text_font(lblR, &lv_font_montserrat_12, 0);
    lv_obj_center(lblR);
    lv_obj_add_event_cb(btnReboot, rebootBtnCb, LV_EVENT_CLICKED, this);

    // Sección 3: parámetros de radio
    lv_obj_t* secRadio = lv_obj_create(tab);
    lv_obj_set_size(secRadio, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secRadio, 8);
    lv_obj_set_flex_flow(secRadio, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secRadio, 8, 0);
    lv_obj_set_style_pad_row(secRadio, 4, 0);
    DefaultTheme::disableScroll(secRadio);

    lv_obj_t* lblRadioTitle = lv_label_create(secRadio);
    lv_label_set_text(lblRadioTitle, LV_SYMBOL_WIFI " Radio LoRa (SELF_INFO)");
    lv_obj_set_style_text_font(lblRadioTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblRadioTitle, lv_color_hex(0x00E5FF), 0);

    m_lblRadioStats = lv_label_create(secRadio);
    lv_label_set_text(m_lblRadioStats, "Frecuencia: -- MHz\nSF: -- | BW: -- kHz | TX: -- dBm");
    lv_obj_set_style_text_font(m_lblRadioStats, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblRadioStats, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_width(m_lblRadioStats, LV_PCT(100));

    // Sección 4: parámetros de radio (se envían por CLI, aplican con reboot)
    lv_obj_t* secParams = lv_obj_create(tab);
    lv_obj_set_size(secParams, LV_PCT(100), LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(secParams, 8);
    lv_obj_set_flex_flow(secParams, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(secParams, 8, 0);
    lv_obj_set_style_pad_row(secParams, 6, 0);
    DefaultTheme::disableScroll(secParams);

    lv_obj_t* lblParamsTitle = lv_label_create(secParams);
    lv_label_set_text(lblParamsTitle, LV_SYMBOL_SETTINGS " Parametros de radio");
    lv_obj_set_style_text_font(lblParamsTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblParamsTitle, lv_color_hex(0x00E5FF), 0);

    lv_obj_t* lblParamsHint = lv_label_create(secParams);
    lv_label_set_text(lblParamsHint, "Deben coincidir en toda tu malla. Se aplican con Reboot.");
    lv_obj_set_style_text_font(lblParamsHint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblParamsHint, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_width(lblParamsHint, LV_PCT(100));
    lv_label_set_long_mode(lblParamsHint, LV_LABEL_LONG_WRAP);

    auto makeField = [&](const char* hint) -> lv_obj_t* {
        lv_obj_t* ta = lv_textarea_create(secParams);
        lv_obj_set_size(ta, LV_PCT(100), 34);
        DefaultTheme::applySunkenCard(ta, 6);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_placeholder_text(ta, hint);
        lv_obj_set_style_text_font(ta, &lv_font_montserrat_12, 0);
        UIManager::attachKeyboard(ta);
        return ta;
    };
    m_taFreq = makeField("Frecuencia MHz (ej. 910.525)");
    m_taBw = makeField("Ancho de banda kHz (ej. 62.5)");
    m_taTx = makeField("Potencia TX dBm (ej. 22)");

    lv_obj_t* rowSfCr = lv_obj_create(secParams);
    lv_obj_set_size(rowSfCr, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(rowSfCr, 0, 0);
    lv_obj_set_style_border_width(rowSfCr, 0, 0);
    lv_obj_set_flex_flow(rowSfCr, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(rowSfCr, 0, 0);
    lv_obj_set_style_pad_column(rowSfCr, 8, 0);
    DefaultTheme::disableScroll(rowSfCr);

    m_ddSf = lv_dropdown_create(rowSfCr);
    lv_obj_set_size(m_ddSf, 130, 34);
    DefaultTheme::applySunkenCard(m_ddSf, 6);
    lv_dropdown_set_options(m_ddSf, "SF 7\nSF 8\nSF 9\nSF 10\nSF 11\nSF 12");

    m_ddCr = lv_dropdown_create(rowSfCr);
    lv_obj_set_size(m_ddCr, 130, 34);
    DefaultTheme::applySunkenCard(m_ddCr, 6);
    lv_dropdown_set_options(m_ddCr, "CR 5\nCR 6\nCR 7\nCR 8");

    lv_obj_t* btnApply = makeButton(secParams, LV_SYMBOL_OK " Aplicar + Reboot", 220, 38, 0x1B5E20);
    lv_obj_add_event_cb(btnApply, radioApplyBtnCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::refreshRadioStatus() {
    auto& client = meshcore::MeshCoreClient::getInstance();

    if (m_lblConnStatus && lv_obj_is_valid(m_lblConnStatus)) {
        if (!client.isConnected()) {
            lv_label_set_text(m_lblConnStatus, "Estado: Desconectado");
            lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0xEF4444), 0);
        } else if (client.hasHandshake()) {
            std::string text = "Estado: Dongle MeshCore en " + client.getActivePort();
            lv_label_set_text(m_lblConnStatus, text.c_str());
            lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0x00E676), 0);
        } else {
            std::string text = "Estado: Puerto " + client.getActivePort() +
                               " abierto, sin respuesta del dongle";
            lv_label_set_text(m_lblConnStatus, text.c_str());
            lv_obj_set_style_text_color(m_lblConnStatus, lv_color_hex(0xFFB300), 0);
        }
    }

    if (m_lblDeviceDetails && lv_obj_is_valid(m_lblDeviceDetails)) {
        const auto& self = client.getSelfInfo();
        const auto& dev = client.getDeviceInfo();
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Nombre: %s\nModelo: %s | FW: %s (build %s)\nContactos: %u | Canales: %u",
            self.valid ? self.name.c_str() : "--",
            dev.valid ? dev.model.c_str() : "--",
            dev.valid ? dev.version.c_str() : "--",
            dev.valid ? dev.firmwareBuild.c_str() : "--",
            (unsigned)(dev.valid ? dev.maxContacts : 0),
            (unsigned)(dev.valid ? dev.maxChannels : 0));
        lv_label_set_text(m_lblDeviceDetails, buf);
    }

    if (m_lblBattery && lv_obj_is_valid(m_lblBattery)) {
        const auto& batt = client.getBattery();
        char buf[128];
        if (!batt.valid) {
            snprintf(buf, sizeof(buf), "Bateria: --");
        } else if (batt.hasStorage) {
            snprintf(buf, sizeof(buf), "Bateria: %u mV | Flash: %u / %u KB",
                     (unsigned)batt.voltageMv,
                     (unsigned)batt.usedStorageKb,
                     (unsigned)batt.totalStorageKb);
        } else {
            snprintf(buf, sizeof(buf), "Bateria: %u mV", (unsigned)batt.voltageMv);
        }
        if (!m_pendingError.empty()) {
            char withErr[192];
            snprintf(withErr, sizeof(withErr), "%s\nError: %s", buf, m_pendingError.c_str());
            lv_label_set_text(m_lblBattery, withErr);
        } else {
            lv_label_set_text(m_lblBattery, buf);
        }
    }

    if (m_lblRadioStats && lv_obj_is_valid(m_lblRadioStats)) {
        const auto& self = client.getSelfInfo();
        char rbuf[160];
        if (!self.valid) {
            snprintf(rbuf, sizeof(rbuf), "Frecuencia: -- MHz\nSF: -- | BW: -- kHz | TX: -- dBm");
        } else {
            snprintf(rbuf, sizeof(rbuf),
                "Frec: %.3f MHz | BW: %.0f kHz\nSF: %d | CR: %d | TX: %d/%d dBm",
                (double)self.frequencyHz / 1000000.0,
                (double)self.bandwidthHz / 1000.0,
                (int)self.spreadingFactor,
                (int)self.codingRate,
                (int)self.txPowerDbm,
                (int)self.maxTxPowerDbm);
        }
        lv_label_set_text(m_lblRadioStats, rbuf);

        // Prerrellenar el formulario con lo que reporta el dongle (sin
        // pisar lo que el usuario esté escribiendo).
        if (self.valid) {
            auto fillEmpty = [](lv_obj_t* ta, const char* v) {
                if (ta && lv_obj_is_valid(ta)) {
                    const char* cur = lv_textarea_get_text(ta);
                    if (!cur || strlen(cur) == 0) lv_textarea_set_text(ta, v);
                }
            };
            char f[16], b[16], t[16];
            snprintf(f, sizeof(f), "%s", trimNum((double)self.frequencyHz / 1000000.0, 3).c_str());
            snprintf(b, sizeof(b), "%s", trimNum((double)self.bandwidthHz / 1000.0, 2).c_str());
            snprintf(t, sizeof(t), "%d", (int)self.txPowerDbm);
            fillEmpty(m_taFreq, f);
            fillEmpty(m_taBw, b);
            fillEmpty(m_taTx, t);
            if (m_ddSf && lv_obj_is_valid(m_ddSf) && self.spreadingFactor >= 7 &&
                self.spreadingFactor <= 12) {
                lv_dropdown_set_selected(m_ddSf, (uint16_t)(self.spreadingFactor - 7));
            }
            if (m_ddCr && lv_obj_is_valid(m_ddCr) && self.codingRate >= 5 &&
                self.codingRate <= 8) {
                lv_dropdown_set_selected(m_ddCr, (uint16_t)(self.codingRate - 5));
            }
        }
    }
}

// ────────────────────────────────────────────────────────────────
// Acciones y manejadores de eventos
// ────────────────────────────────────────────────────────────────

void MeshCoreView::sendMessage() {
    if (!m_taInput || !lv_obj_is_valid(m_taInput)) return;

    const char* txt = lv_textarea_get_text(m_taInput);
    if (!txt || strlen(txt) == 0) return;

    auto& client = meshcore::MeshCoreClient::getInstance();
    if (client.sendChannelMessage(m_channelIdx, txt)) {
        lv_textarea_set_text(m_taInput, "");
        refreshChatLog();
    } else {
        UIManager::showToast("Dongle desconectado. Revisa enlace Radio.");
    }
}

void MeshCoreView::setActiveChannel(uint8_t idx) {
    if (idx >= meshcore::CHANNEL_COUNT) return;
    m_channelIdx = idx;
    updateChannelLabel();
}

void MeshCoreView::updateChannelLabel() {
    if (m_lblChannel && lv_obj_is_valid(m_lblChannel)) {
        auto& client = meshcore::MeshCoreClient::getInstance();
        const auto& ch = client.getChannelInfo(m_channelIdx);
        char buf[64];
        if (ch.known && !ch.name.empty()) {
            snprintf(buf, sizeof(buf), "Canal %d: %s", (int)m_channelIdx, ch.name.c_str());
        } else {
            snprintf(buf, sizeof(buf), "Canal %d", (int)m_channelIdx);
        }
        lv_label_set_text(m_lblChannel, buf);
    }
    if (m_ddChannel && lv_obj_is_valid(m_ddChannel)) {
        lv_dropdown_set_selected(m_ddChannel, m_channelIdx);
    }
}

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

void MeshCoreView::sendBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->sendMessage();
}

void MeshCoreView::taInputEventCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        self->sendMessage();
    } else if (code == LV_EVENT_CLICKED) {
        if (self->m_keyboard && lv_obj_is_valid(self->m_keyboard)) {
            lv_obj_remove_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
            self->m_keyboardVisible = true;
        }
    }
}

void MeshCoreView::toggleKbBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_keyboard || !lv_obj_is_valid(self->m_keyboard)) return;

    self->m_keyboardVisible = !self->m_keyboardVisible;
    if (self->m_keyboardVisible) {
        lv_obj_remove_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void MeshCoreView::kbEventCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        self->sendMessage();
    } else if (code == LV_EVENT_CANCEL) {
        if (self->m_keyboard && lv_obj_is_valid(self->m_keyboard)) {
            lv_obj_add_flag(self->m_keyboard, LV_OBJ_FLAG_HIDDEN);
            self->m_keyboardVisible = false;
        }
    }
}

void MeshCoreView::channelDropdownCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddChannel) return;
    uint16_t sel = lv_dropdown_get_selected(self->m_ddChannel);
    if (sel < meshcore::CHANNEL_COUNT) {
        self->m_channelIdx = static_cast<uint8_t>(sel);
        self->setActiveChannel(self->m_channelIdx);
        self->refreshChannelsList();
        self->refreshChatLog();
    }
}

void MeshCoreView::refreshChannelsBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    if (!meshcore::MeshCoreClient::getInstance().queryAllChannels()) {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::channelUseBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !target) return;
    uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(target);
    self->setActiveChannel(idx);
    self->refreshChannelsList();
    self->refreshChatLog();
    if (self->m_tabview && lv_obj_is_valid(self->m_tabview)) {
        lv_tabview_set_active(self->m_tabview, 2, LV_ANIM_OFF);  // ir al Chat
    }
}

void MeshCoreView::channelCreateBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taNewName || !self->m_taNewSecret) return;

    const char* name = lv_textarea_get_text(self->m_taNewName);
    const char* secret = lv_textarea_get_text(self->m_taNewSecret);
    std::string sName = name ? name : "";
    std::string sSecret = secret ? secret : "";
    uint8_t idx = self->m_newChannelIdx;

    auto& client = meshcore::MeshCoreClient::getInstance();
    bool ok = false;
    if (sName.empty()) {
        UIManager::showToast("Nombre vacio: usa Borrar para liberar.");
        return;
    } else if (sSecret.empty() && !sName.empty() && sName[0] == '#') {
        ok = client.setHashtagChannel(idx, sName);
    } else if (sSecret.empty()) {
        UIManager::showToast("Falta secreto hex de 32 chars.");
        return;
    } else {
        ok = client.setChannelHex(idx, sName, sSecret);
        if (!ok) {
            UIManager::showToast("Secreto invalido (32 hex).");
            return;
        }
    }

    if (ok) {
        client.getChannel(idx);  // releer para confirmar
        UIManager::showToast("Canal guardado. Leyendo...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::channelClearBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (client.clearChannel(self->m_newChannelIdx)) {
        client.getChannel(self->m_newChannelIdx);
        UIManager::showToast("Canal liberado.");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::newIdxDropdownCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddNewIdx) return;
    uint16_t sel = lv_dropdown_get_selected(self->m_ddNewIdx);
    if (sel < meshcore::CHANNEL_COUNT) {
        self->m_newChannelIdx = static_cast<uint8_t>(sel);
    }
}

void MeshCoreView::connectBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;

    auto& client = meshcore::MeshCoreClient::getInstance();
    client.disconnect();
    bool ok = client.connect(self->m_selectedPort, self->m_selectedBaud);
    if (ok) {
        saveLastPort(self->m_selectedPort);
        client.queryAllChannels();
        client.queryContacts();
        client.queryBattery();
        client.pollMessages();
        UIManager::showToast("Dongle conectado");
    } else {
        UIManager::showToast("Fallo al conectar puerto");
    }
    self->refreshRadioStatus();
}

void MeshCoreView::infoBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    client.sendAppStart();
    client.queryDeviceInfo();
    client.queryBattery();
    client.queryAllChannels();
    client.queryContacts();
}

void MeshCoreView::batteryBtnCb(lv_event_t* e) {
    (void)e;
    meshcore::MeshCoreClient::getInstance().queryBattery();
}

void MeshCoreView::pollBtnCb(lv_event_t* e) {
    (void)e;
    meshcore::MeshCoreClient::getInstance().pollMessages();
    UIManager::showToast("Drenando mensajes...");
}

void MeshCoreView::aliasApplyBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taAlias) return;
    const char* txt = lv_textarea_get_text(self->m_taAlias);
    if (!txt || strlen(txt) == 0) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (client.setName(txt)) {
        // Refrescar SELF_INFO para mostrar el nuevo alias.
        client.sendAppStart();
        UIManager::showToast("Alias enviado. Refrescando...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::rebootBtnCb(lv_event_t* e) {
    (void)e;
    if (meshcore::MeshCoreClient::getInstance().reboot()) {
        UIManager::showToast("Reiniciando dongle...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::portDropdownCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddPort) return;

    uint16_t sel = lv_dropdown_get_selected(self->m_ddPort);
    if (sel < self->m_portIds.size()) {
        self->m_selectedPort = self->m_portIds[sel];
    } else {
        char buf[32];
        lv_dropdown_get_selected_str(self->m_ddPort, buf, sizeof(buf));
        self->m_selectedPort = buf;
    }
}

// ────────────────────────────────────────────────────────────────
// Fase 2: Contactos / Conversación / Detalles / Mapa
// ────────────────────────────────────────────────────────────────

namespace {
bool containsFold(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = hay, n = needle;
    for (auto& c : h) c = (char)tolower((unsigned char)c);
    for (auto& c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int w, int h, uint32_t bg) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    DefaultTheme::applyButton(btn, 6);
    if (bg) lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
    return btn;
}
}  // namespace

void MeshCoreView::buildContactsTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 4, 0);
    lv_obj_set_style_pad_row(tab, 4, 0);
    DefaultTheme::disableScroll(tab);

    // — Lista —
    m_listPane = lv_obj_create(tab);
    lv_obj_set_size(m_listPane, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_listPane, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_listPane, 0, 0);
    lv_obj_set_style_pad_all(m_listPane, 0, 0);
    lv_obj_set_flex_flow(m_listPane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_listPane, 4, 0);
    DefaultTheme::disableScroll(m_listPane);

    lv_obj_t* topBar = lv_obj_create(m_listPane);
    lv_obj_set_size(topBar, LV_PCT(100), 38);
    DefaultTheme::applySunkenCard(topBar, 6);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(topBar, 6, 0);
    lv_obj_set_style_pad_ver(topBar, 2, 0);
    DefaultTheme::disableScroll(topBar);

    lv_obj_t* btnAdvert = makeButton(topBar, LV_SYMBOL_VOLUME_MID " Advert", 100, 30, 0);
    lv_obj_add_event_cb(btnAdvert, advertBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* btnPlus = makeButton(topBar, LV_SYMBOL_PLUS " Anadir", 100, 30, 0x1B5E20);
    lv_obj_add_event_cb(btnPlus, plusBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* btnDisc = makeButton(topBar, LV_SYMBOL_REFRESH " Descubrir", 110, 30, 0);
    lv_obj_add_event_cb(btnDisc, discoverBtnCb, LV_EVENT_CLICKED, this);

    m_taSearch = lv_textarea_create(m_listPane);
    lv_obj_set_size(m_taSearch, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(m_taSearch, 6);
    lv_textarea_set_one_line(m_taSearch, true);
    lv_textarea_set_placeholder_text(m_taSearch, "Buscar por nombre o pubkey...");
    lv_obj_set_style_text_font(m_taSearch, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_taSearch, searchTaCb, LV_EVENT_VALUE_CHANGED, this);
    UIManager::attachKeyboard(m_taSearch);

    m_lblContactsCount = lv_label_create(m_listPane);
    lv_label_set_text(m_lblContactsCount, "0 contactos");
    lv_obj_set_style_text_font(m_lblContactsCount, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblContactsCount, DefaultTheme::getMutedTextColor(), 0);

    m_contactsContainer = lv_obj_create(m_listPane);
    lv_obj_set_width(m_contactsContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_contactsContainer, 1);
    DefaultTheme::applySunkenCard(m_contactsContainer, 8);
    lv_obj_set_flex_flow(m_contactsContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_contactsContainer, 6, 0);
    lv_obj_set_style_pad_row(m_contactsContainer, 4, 0);
    lv_obj_set_scroll_dir(m_contactsContainer, LV_DIR_VER);

    // — Conversación —
    m_convPane = lv_obj_create(tab);
    lv_obj_set_size(m_convPane, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_convPane, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_convPane, 0, 0);
    lv_obj_set_style_pad_all(m_convPane, 0, 0);
    lv_obj_set_flex_flow(m_convPane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_convPane, 4, 0);
    lv_obj_add_flag(m_convPane, LV_OBJ_FLAG_HIDDEN);
    DefaultTheme::disableScroll(m_convPane);

    lv_obj_t* convHead = lv_obj_create(m_convPane);
    lv_obj_set_size(convHead, LV_PCT(100), 38);
    DefaultTheme::applySunkenCard(convHead, 6);
    lv_obj_set_flex_flow(convHead, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(convHead, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(convHead, 6, 0);
    DefaultTheme::disableScroll(convHead);

    lv_obj_t* btnBack = makeButton(convHead, LV_SYMBOL_LEFT " Atras", 90, 30, 0);
    lv_obj_add_event_cb(btnBack, convBackCb, LV_EVENT_CLICKED, this);
    m_lblConvTitle = lv_label_create(convHead);
    lv_label_set_text(m_lblConvTitle, "--");
    lv_obj_set_style_text_font(m_lblConvTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblConvTitle, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_flex_grow(m_lblConvTitle, 1);
    lv_obj_t* btnDet = makeButton(convHead, LV_SYMBOL_LIST, 40, 30, 0);
    lv_obj_add_event_cb(btnDet, convDetailsCb, LV_EVENT_CLICKED, this);

    m_convContainer = lv_obj_create(m_convPane);
    lv_obj_set_width(m_convContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_convContainer, 1);
    DefaultTheme::applySunkenCard(m_convContainer, 8);
    lv_obj_set_style_bg_color(m_convContainer, lv_color_hex(0x0E1218), 0);
    lv_obj_set_flex_flow(m_convContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_convContainer, 6, 0);
    lv_obj_set_style_pad_row(m_convContainer, 6, 0);
    lv_obj_set_scroll_dir(m_convContainer, LV_DIR_VER);

    m_lblConvStatus = lv_label_create(m_convPane);
    lv_label_set_text(m_lblConvStatus, "");
    lv_obj_set_style_text_font(m_lblConvStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblConvStatus, lv_color_hex(0xFFB300), 0);

    lv_obj_t* dmRow = lv_obj_create(m_convPane);
    lv_obj_set_size(dmRow, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(dmRow, 0, 0);
    lv_obj_set_style_border_width(dmRow, 0, 0);
    lv_obj_set_flex_flow(dmRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dmRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dmRow, 0, 0);
    lv_obj_set_style_pad_column(dmRow, 4, 0);
    DefaultTheme::disableScroll(dmRow);

    m_taDmInput = lv_textarea_create(dmRow);
    lv_obj_set_flex_grow(m_taDmInput, 1);
    lv_obj_set_height(m_taDmInput, 36);
    DefaultTheme::applySunkenCard(m_taDmInput, 6);
    lv_textarea_set_one_line(m_taDmInput, true);
    lv_textarea_set_placeholder_text(m_taDmInput, "Mensaje directo...");
    lv_obj_set_style_text_font(m_taDmInput, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_taDmInput, lv_color_hex(0xF0F4F8), 0);
    lv_obj_set_style_text_color(m_taDmInput, lv_color_hex(0x94A3B8), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(m_taDmInput, lv_color_hex(0x00F5D4), LV_PART_CURSOR);
    lv_obj_add_event_cb(m_taDmInput, dmInputEventCb, LV_EVENT_ALL, this);

    m_btnDmKb = makeButton(dmRow, LV_SYMBOL_KEYBOARD, 40, 36, 0);
    lv_obj_add_event_cb(m_btnDmKb, toggleDmKbBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnDmSend = makeButton(dmRow, LV_SYMBOL_OK " Enviar", 90, 36, 0x1B5E20);
    lv_obj_add_event_cb(btnDmSend, convSendCb, LV_EVENT_CLICKED, this);
    lv_obj_t* btnRetry = makeButton(dmRow, LV_SYMBOL_REFRESH, 40, 36, 0);
    lv_obj_add_event_cb(btnRetry, convRetryCb, LV_EVENT_CLICKED, this);

    // Teclado virtual del chat privado (paridad con el tab Chat de canal).
    {
        auto caps = cbdos::display::getCapabilities();
        m_keyboardDm = lv_keyboard_create(m_convPane);
        int32_t kbHeight = (caps.height >= 800) ? 280 : 190;
        lv_obj_set_size(m_keyboardDm, LV_PCT(100), kbHeight);
        lv_obj_set_style_bg_color(m_keyboardDm, lv_color_hex(0x1A1E29), 0);
        lv_keyboard_set_textarea(m_keyboardDm, m_taDmInput);
        lv_obj_add_event_cb(m_keyboardDm, dmKbEventCb, LV_EVENT_ALL, this);
        lv_obj_add_flag(m_keyboardDm, LV_OBJ_FLAG_HIDDEN);
        m_keyboardDmVisible = false;
    }

    // — Detalles —
    m_detailsPane = lv_obj_create(tab);
    lv_obj_set_size(m_detailsPane, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_detailsPane, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_detailsPane, 0, 0);
    lv_obj_set_style_pad_all(m_detailsPane, 0, 0);
    lv_obj_set_flex_flow(m_detailsPane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_detailsPane, 4, 0);
    lv_obj_add_flag(m_detailsPane, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_scroll_dir(m_detailsPane, LV_DIR_VER);

    lv_obj_t* detHead = lv_obj_create(m_detailsPane);
    lv_obj_set_size(detHead, LV_PCT(100), 38);
    DefaultTheme::applySunkenCard(detHead, 6);
    lv_obj_set_flex_flow(detHead, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(detHead, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(detHead, 6, 0);
    DefaultTheme::disableScroll(detHead);

    lv_obj_t* btnDetBack = makeButton(detHead, LV_SYMBOL_LEFT " Atras", 90, 30, 0);
    lv_obj_add_event_cb(btnDetBack, detailsBackCb, LV_EVENT_CLICKED, this);
    m_lblDetailsTitle = lv_label_create(detHead);
    lv_label_set_text(m_lblDetailsTitle, "Detalles");
    lv_obj_set_style_text_font(m_lblDetailsTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDetailsTitle, lv_color_hex(0x00E5FF), 0);

    m_lblDetailsBody = lv_label_create(m_detailsPane);
    lv_label_set_long_mode(m_lblDetailsBody, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_lblDetailsBody, LV_PCT(100));
    lv_obj_set_style_text_font(m_lblDetailsBody, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDetailsBody, lv_color_hex(0xB0BEC5), 0);

    m_taDetailName = lv_textarea_create(m_detailsPane);
    lv_obj_set_size(m_taDetailName, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(m_taDetailName, 6);
    lv_textarea_set_one_line(m_taDetailName, true);
    lv_textarea_set_placeholder_text(m_taDetailName, "Nombre del contacto");
    lv_obj_set_style_text_font(m_taDetailName, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taDetailName);
    lv_obj_t* btnSaveName = makeButton(m_detailsPane, "Guardar nombre", 160, 32, 0x1B5E20);
    lv_obj_add_event_cb(btnSaveName, detailsSaveNameCb, LV_EVENT_CLICKED, this);

    m_taDetailPath = lv_textarea_create(m_detailsPane);
    lv_obj_set_size(m_taDetailPath, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(m_taDetailPath, 6);
    lv_textarea_set_one_line(m_taDetailPath, true);
    lv_textarea_set_placeholder_text(m_taDetailPath, "Ruta hex (vacio = flood)");
    lv_obj_set_style_text_font(m_taDetailPath, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taDetailPath);
    lv_obj_t* btnSavePath = makeButton(m_detailsPane, "Guardar ruta", 160, 32, 0x1B5E20);
    lv_obj_add_event_cb(btnSavePath, detailsSavePathCb, LV_EVENT_CLICKED, this);

    lv_obj_t* detBtns = lv_obj_create(m_detailsPane);
    lv_obj_set_size(detBtns, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(detBtns, 0, 0);
    lv_obj_set_style_border_width(detBtns, 0, 0);
    lv_obj_set_flex_flow(detBtns, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(detBtns, 0, 0);
    lv_obj_set_style_pad_column(detBtns, 6, 0);
    lv_obj_set_style_pad_row(detBtns, 6, 0);
    DefaultTheme::disableScroll(detBtns);

    lv_obj_t* bReset = makeButton(detBtns, "Reset ruta", 110, 32, 0);
    lv_obj_add_event_cb(bReset, detailsResetPathCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bShare = makeButton(detBtns, "Compartir", 110, 32, 0);
    lv_obj_add_event_cb(bShare, detailsShareCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bFav = makeButton(detBtns, "Favorito", 110, 32, 0);
    lv_obj_add_event_cb(bFav, detailsFavCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bDel = makeButton(detBtns, "Borrar", 110, 32, 0x6A1B1B);
    lv_obj_add_event_cb(bDel, detailsRemoveCb, LV_EVENT_CLICKED, this);

    // — Overlay genérico (encima de todo) —
    m_overlay = lv_obj_create(m_container);
    lv_obj_set_size(m_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(m_overlay, 0, 0);
    lv_obj_set_style_pad_all(m_overlay, 24, 0);
    lv_obj_add_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    m_overlayCard = lv_obj_create(m_overlay);
    lv_obj_set_size(m_overlayCard, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_center(m_overlayCard);
    DefaultTheme::applySunkenCard(m_overlayCard, 10);
    lv_obj_set_flex_flow(m_overlayCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_overlayCard, 10, 0);
    lv_obj_set_style_pad_row(m_overlayCard, 8, 0);
}

void MeshCoreView::showContactsPane(ContactsPane pane) {
    m_contactsPane = pane;
    if (m_listPane && lv_obj_is_valid(m_listPane)) {
        if (pane == ContactsPane::List) lv_obj_remove_flag(m_listPane, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(m_listPane, LV_OBJ_FLAG_HIDDEN);
    }
    if (m_convPane && lv_obj_is_valid(m_convPane)) {
        if (pane == ContactsPane::Conversation) lv_obj_remove_flag(m_convPane, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(m_convPane, LV_OBJ_FLAG_HIDDEN);
    }
    if (m_detailsPane && lv_obj_is_valid(m_detailsPane)) {
        if (pane == ContactsPane::Details) lv_obj_remove_flag(m_detailsPane, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(m_detailsPane, LV_OBJ_FLAG_HIDDEN);
    }
}

void MeshCoreView::refreshContactsList() {
    if (!m_contactsContainer || !lv_obj_is_valid(m_contactsContainer)) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const auto& contacts = client.getContacts();

    // Firma barata para no reconstruir sin cambios.
    size_t sig = contacts.size() * 1000003u + m_searchFilter.size() * 9176u;
    for (const auto& c : contacts) {
        sig += c.unread * 131u + c.lastmod + (c.favourite ? 7919u : 0u) + c.name.size();
    }
    if (sig == m_lastRenderedContactsSig && m_contactsPane == ContactsPane::List) return;
    m_lastRenderedContactsSig = sig;

    std::vector<const meshcore::MeshContact*> rows;
    for (const auto& c : contacts) {
        if (containsFold(c.name, m_searchFilter) || containsFold(c.prefixHex12, m_searchFilter)) {
            rows.push_back(&c);
        }
    }
    m_rowPrefixes.clear();
    for (auto* r : rows) m_rowPrefixes.push_back(r->prefixHex12);

    if (m_lblContactsCount && lv_obj_is_valid(m_lblContactsCount)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u contactos%s", (unsigned)contacts.size(),
                 client.isContactSyncInProgress() ? " (sincronizando...)" : "");
        lv_label_set_text(m_lblContactsCount, buf);
    }

    lv_obj_clean(m_contactsContainer);
    if (rows.empty()) {
        lv_obj_t* lbl = lv_label_create(m_contactsContainer);
        lv_label_set_text(lbl, contacts.empty()
                              ? "Sin contactos.\nPulsa Descubrir con el dongle conectado,\no anade uno con + Anadir."
                              : "Sin coincidencias.");
        lv_obj_set_style_text_color(lbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        return;
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        const auto* c = rows[i];
        lv_obj_t* row = lv_obj_create(m_contactsContainer);
        lv_obj_set_size(row, LV_PCT(100), 56);
        DefaultTheme::applySunkenCard(row, 8);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(row, 6, 0);
        lv_obj_set_style_pad_ver(row, 2, 0);
        DefaultTheme::disableScroll(row);

        lv_obj_t* main = lv_button_create(row);
        lv_obj_set_flex_grow(main, 1);
        lv_obj_set_height(main, 48);
        DefaultTheme::applyButton(main, 6);
        lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(main, 4, 0);
        lv_obj_set_user_data(main, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(main, contactRowCb, LV_EVENT_CLICKED, this);

        std::string title = std::string(contactIcon(c->type)) + "  " +
                            (c->name.empty() ? c->prefixHex12 : c->name);
        if (c->favourite) title += "  ★";
        if (c->unread > 0) {
            char b[16];
            snprintf(b, sizeof(b), " (%u)", (unsigned)c->unread);
            title += b;
        }
        lv_obj_t* lTitle = lv_label_create(main);
        lv_label_set_text(lTitle, title.c_str());
        lv_obj_set_style_text_font(lTitle, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lTitle, c->unread > 0 ? lv_color_hex(0x00E676) : lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_width(lTitle, LV_PCT(100));

        std::string sub = c->prefixHex12 + " · ";
        sub += (c->outPathLen < 0) ? "Flood" : (c->hops == 0 ? "Directo" : std::to_string((int)c->hops) + " saltos");
        sub += " · " + fmtAgo(c->lastAdvert ? c->lastAdvert : c->lastmod);
        if (c->hasSnr) {
            char s[24];
            snprintf(s, sizeof(s), " · %.1f dB", (double)c->lastSnr);
            sub += s;
        }
        lv_obj_t* lSub = lv_label_create(main);
        lv_label_set_text(lSub, sub.c_str());
        lv_obj_set_style_text_font(lSub, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lSub, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_width(lSub, LV_PCT(100));

        lv_obj_t* menu = makeButton(row, LV_SYMBOL_LIST, 40, 48, 0);
        lv_obj_set_user_data(menu, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(menu, contactMenuCb, LV_EVENT_CLICKED, this);
    }
}

void MeshCoreView::openConversation(const std::string& prefixHex12) {
    m_activePrefix = prefixHex12;
    m_lastRenderedConvMsgs = (size_t)-1;  // forzar render
    m_lastRenderedConvPending = (size_t)-1;
    showContactsPane(ContactsPane::Conversation);
    refreshConversation();
}

void MeshCoreView::openDetails(const std::string& prefixHex12) {
    m_activePrefix = prefixHex12;
    showContactsPane(ContactsPane::Details);
    refreshDetails();
}

void MeshCoreView::refreshConversation() {
    if (!m_convContainer || !lv_obj_is_valid(m_convContainer)) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::MeshContact* c = client.findContact(m_activePrefix);
    const meshcore::DMThread* th = client.getThread(m_activePrefix);

    if (m_lblConvTitle && lv_obj_is_valid(m_lblConvTitle)) {
        std::string t = c && !c->name.empty() ? c->name : m_activePrefix;
        lv_label_set_text(m_lblConvTitle, t.c_str());
    }

    size_t nMsgs = th ? th->msgs.size() : 0;
    size_t nPending = 0;
    for (const auto& kv : client.getPendingDMs()) {
        if (kv.second.destPrefix == m_activePrefix) ++nPending;
    }
    if (th && nMsgs == m_lastRenderedConvMsgs && m_lastRenderedConvPrefix == m_activePrefix &&
        nPending == m_lastRenderedConvPending) {
        return;
    }
    m_lastRenderedConvMsgs = nMsgs;
    m_lastRenderedConvPrefix = m_activePrefix;
    m_lastRenderedConvPending = nPending;

    lv_obj_clean(m_convContainer);
    if (!th || th->msgs.empty()) {
        lv_obj_t* lbl = lv_label_create(m_convContainer);
        lv_label_set_text(lbl, "Sin mensajes. Escribe abajo para enviar un DM cifrado.");
        lv_obj_set_style_text_color(lbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
    } else {
        for (const auto& msg : th->msgs) {
            lv_obj_t* bubble = lv_obj_create(m_convContainer);
            lv_obj_set_width(bubble, LV_PCT(85));
            lv_obj_set_height(bubble, LV_SIZE_CONTENT);
            lv_obj_set_style_radius(bubble, 8, 0);
            lv_obj_set_style_pad_all(bubble, 6, 0);
            lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(bubble, 2, 0);
            DefaultTheme::disableScroll(bubble);
            if (msg.outgoing) {
                lv_obj_set_align(bubble, LV_ALIGN_TOP_RIGHT);
                lv_obj_set_style_bg_color(bubble, lv_color_hex(0x13382C), 0);
                lv_obj_set_style_border_color(bubble, lv_color_hex(0x1F6B52), 0);
            } else {
                lv_obj_set_align(bubble, LV_ALIGN_TOP_LEFT);
                lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2A1A34), 0);
                lv_obj_set_style_border_color(bubble, lv_color_hex(0x5A3A6A), 0);
            }
            lv_obj_set_style_border_width(bubble, 1, 0);

            lv_obj_t* lblText = lv_label_create(bubble);
            lv_label_set_text(lblText, msg.text.c_str());
            lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(lblText, LV_PCT(100));
            lv_obj_set_style_text_color(lblText, lv_color_hex(0xF0F4F8), 0);
            lv_obj_set_style_text_font(lblText, &lv_font_montserrat_12, 0);

            lv_obj_t* lblMeta = lv_label_create(bubble);
            char meta[64];
            if (msg.hasSnr) {
                snprintf(meta, sizeof(meta), "%s · %.1f dB · %u saltos", fmtTime(msg.timestamp).c_str(),
                         (double)msg.snrDb, (unsigned)msg.pathLength);
            } else {
                const char* outState = "";
                if (msg.outgoing) {
                    outState = (nPending > 0) ? " · pendiente de ACK" : " · en dongle";
                }
                snprintf(meta, sizeof(meta), "%s%s", fmtTime(msg.timestamp).c_str(), outState);
            }
            lv_label_set_text(lblMeta, meta);
            lv_obj_set_style_text_color(lblMeta, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_text_font(lblMeta, &lv_font_montserrat_12, 0);
            lv_obj_set_width(lblMeta, LV_PCT(100));
        }
        lv_obj_scroll_to_view(lv_obj_get_child(m_convContainer, -1), LV_ANIM_OFF);
    }

    if (m_lblConvStatus && lv_obj_is_valid(m_lblConvStatus)) {
        if (nPending > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Enviando... %u pendiente(s) de ACK", (unsigned)nPending);
            lv_label_set_text(m_lblConvStatus, buf);
        } else {
            lv_label_set_text(m_lblConvStatus, "");
        }
    }
}

void MeshCoreView::refreshDetails() {
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::MeshContact* c = client.findContact(m_activePrefix);
    if (!c) {
        if (m_lblDetailsBody && lv_obj_is_valid(m_lblDetailsBody)) {
            lv_label_set_text(m_lblDetailsBody, "Contacto no encontrado.");
        }
        return;
    }
    if (m_lblDetailsTitle && lv_obj_is_valid(m_lblDetailsTitle)) {
        std::string t = c->name.empty() ? c->prefixHex12 : c->name;
        lv_label_set_text(m_lblDetailsTitle, t.c_str());
    }
    if (m_lblDetailsBody && lv_obj_is_valid(m_lblDetailsBody)) {
        char pkHex[65];
        for (int i = 0; i < 32; ++i) snprintf(pkHex + i * 2, 3, "%02x", c->pubkey[i]);
        std::string body = "Tipo: ";
        body += meshcore::MeshCoreClient::contactTypeName(c->type);
        body += "\nPubkey: ";
        body += m_activePrefix;
        body += "…\nCompleta: ";
        body += pkHex;
        const auto& self = client.getSelfInfo();
        if ((c->lat != 0.0 || c->lon != 0.0)) {
            char g[96];
            snprintf(g, sizeof(g), "\nGPS: %.5f, %.5f", c->lat, c->lon);
            body += g;
            if (self.valid && (self.advLatitude != 0.0 || self.advLongitude != 0.0)) {
                char d[48];
                snprintf(d, sizeof(d), "\nDistancia: %.1f km",
                         haversineKm(self.advLatitude, self.advLongitude, c->lat, c->lon));
                body += d;
            }
        } else {
            body += "\nGPS: sin posicion";
        }
        body += "\nUltimo advert: ";
        body += fmtAgo(c->lastAdvert ? c->lastAdvert : c->lastmod);
        body += c->outPathLen < 0 ? "\nRuta: flood" : "\nRuta: directa";
        if (c->hasSnr) {
            char s[32];
            snprintf(s, sizeof(s), "\nSNR: %.1f dB", (double)c->lastSnr);
            body += s;
        }
        body += c->favourite ? "\nFavorito: si" : "\nFavorito: no";
        lv_label_set_text(m_lblDetailsBody, body.c_str());
    }
    if (m_taDetailName && lv_obj_is_valid(m_taDetailName)) {
        lv_textarea_set_text(m_taDetailName, c->name.c_str());
    }
    if (m_taDetailPath && lv_obj_is_valid(m_taDetailPath)) {
        if (c->outPathLen < 0) {
            lv_textarea_set_text(m_taDetailPath, "");
        } else {
            std::string hex;
            int n = c->outPathLen < 0 ? 0 : (c->outPathLen > 64 ? 64 : c->outPathLen);
            char b[3];
            for (int i = 0; i < n; ++i) {
                snprintf(b, sizeof(b), "%02x", c->outPath[i]);
                hex += b;
            }
            lv_textarea_set_text(m_taDetailPath, hex.c_str());
        }
    }
}

void MeshCoreView::sendDirectMessage() {
    if (!m_taDmInput || !lv_obj_is_valid(m_taDmInput)) return;
    const char* txt = lv_textarea_get_text(m_taDmInput);
    if (!txt || strlen(txt) == 0 || m_activePrefix.empty()) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (client.sendDMByPrefix(m_activePrefix, txt)) {
        lv_textarea_set_text(m_taDmInput, "");
        m_lastRenderedConvMsgs = (size_t)-1;
        m_lastRenderedConvPending = (size_t)-1;
        refreshConversation();
    } else {
        UIManager::showToast("No se pudo enviar. Revisa el enlace Radio.");
    }
}

void MeshCoreView::hideOverlay() {
    if (m_overlay && lv_obj_is_valid(m_overlay)) {
        lv_obj_add_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (m_overlayCard && lv_obj_is_valid(m_overlayCard)) {
        lv_obj_clean(m_overlayCard);
    }
    m_taManualKey = nullptr;
    m_taManualName = nullptr;
    m_ddManualType = nullptr;
    m_taImportCard = nullptr;
}

void MeshCoreView::showOverlayAdvert() {
    if (!m_overlay || !m_overlayCard) return;
    hideOverlay();
    lv_obj_remove_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* title = lv_label_create(m_overlayCard);
    lv_label_set_text(title, "Anunciar mi presencia (advert)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00E5FF), 0);
    lv_obj_t* bZero = makeButton(m_overlayCard, "Zero Hop (cercanos)", 220, 40, 0x1B5E20);
    lv_obj_add_event_cb(bZero, advertZeroCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bFlood = makeButton(m_overlayCard, "Flood (toda la malla)", 220, 40, 0);
    lv_obj_add_event_cb(bFlood, advertFloodCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bClose = makeButton(m_overlayCard, "Cerrar", 220, 36, 0);
    lv_obj_add_event_cb(bClose, overlayCloseCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::showOverlayPlus() {
    if (!m_overlay || !m_overlayCard) return;
    hideOverlay();
    lv_obj_remove_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* title = lv_label_create(m_overlayCard);
    lv_label_set_text(title, "Anadir contacto");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00E5FF), 0);
    lv_obj_t* bDisc = makeButton(m_overlayCard, "Descubrir (del dongle)", 220, 40, 0x1B5E20);
    lv_obj_add_event_cb(bDisc, plusDiscoverCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bMan = makeButton(m_overlayCard, "Anadir manual", 220, 40, 0);
    lv_obj_add_event_cb(bMan, plusManualCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bImp = makeButton(m_overlayCard, "Importar tarjeta", 220, 40, 0);
    lv_obj_add_event_cb(bImp, plusImportCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bClose = makeButton(m_overlayCard, "Cerrar", 220, 36, 0);
    lv_obj_add_event_cb(bClose, overlayCloseCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::showOverlayManualAdd() {
    if (!m_overlay || !m_overlayCard) return;
    hideOverlay();
    lv_obj_remove_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* title = lv_label_create(m_overlayCard);
    lv_label_set_text(title, "Contacto manual");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00E5FF), 0);
    m_taManualKey = lv_textarea_create(m_overlayCard);
    lv_obj_set_size(m_taManualKey, LV_PCT(100), 36);
    DefaultTheme::applySunkenCard(m_taManualKey, 6);
    lv_textarea_set_one_line(m_taManualKey, true);
    lv_textarea_set_placeholder_text(m_taManualKey, "Pubkey hex 64 chars");
    lv_obj_set_style_text_font(m_taManualKey, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taManualKey);
    m_taManualName = lv_textarea_create(m_overlayCard);
    lv_obj_set_size(m_taManualName, LV_PCT(100), 36);
    DefaultTheme::applySunkenCard(m_taManualName, 6);
    lv_textarea_set_one_line(m_taManualName, true);
    lv_textarea_set_placeholder_text(m_taManualName, "Nombre");
    lv_obj_set_style_text_font(m_taManualName, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taManualName);
    m_ddManualType = lv_dropdown_create(m_overlayCard);
    lv_dropdown_set_options(m_ddManualType, "Chat\nRepetidor\nSala\nSensor");
    lv_obj_t* bSave = makeButton(m_overlayCard, "Guardar", 220, 40, 0x1B5E20);
    lv_obj_add_event_cb(bSave, manualSaveCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bClose = makeButton(m_overlayCard, "Cerrar", 220, 36, 0);
    lv_obj_add_event_cb(bClose, overlayCloseCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::showOverlayImportCard() {
    if (!m_overlay || !m_overlayCard) return;
    hideOverlay();
    lv_obj_remove_flag(m_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* title = lv_label_create(m_overlayCard);
    lv_label_set_text(title, "Importar tarjeta (hex)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00E5FF), 0);
    m_taImportCard = lv_textarea_create(m_overlayCard);
    lv_obj_set_size(m_taImportCard, LV_PCT(100), 80);
    lv_textarea_set_placeholder_text(m_taImportCard, "Pega la tarjeta en hex...");
    lv_obj_set_style_text_font(m_taImportCard, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taImportCard);
    lv_obj_t* bSave = makeButton(m_overlayCard, "Importar", 220, 40, 0x1B5E20);
    lv_obj_add_event_cb(bSave, importSaveCb, LV_EVENT_CLICKED, this);
    lv_obj_t* bClose = makeButton(m_overlayCard, "Cerrar", 220, 36, 0);
    lv_obj_add_event_cb(bClose, overlayCloseCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::buildMapTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 4, 0);
    lv_obj_set_style_pad_row(tab, 4, 0);
    DefaultTheme::disableScroll(tab);

    m_lblMapCount = lv_label_create(tab);
    lv_label_set_text(m_lblMapCount, "Sin posiciones");
    lv_obj_set_style_text_font(m_lblMapCount, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblMapCount, DefaultTheme::getMutedTextColor(), 0);

    m_mapContainer = lv_obj_create(tab);
    lv_obj_set_width(m_mapContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_mapContainer, 1);
    DefaultTheme::applySunkenCard(m_mapContainer, 8);
    lv_obj_set_flex_flow(m_mapContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_mapContainer, 6, 0);
    lv_obj_set_style_pad_row(m_mapContainer, 4, 0);
    lv_obj_set_scroll_dir(m_mapContainer, LV_DIR_VER);

    lv_obj_t* btnGpx = makeButton(tab, LV_SYMBOL_DOWNLOAD " Exportar GPX", 200, 36, 0);
    lv_obj_add_event_cb(btnGpx, mapExportCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::refreshMapList() {
    if (!m_mapContainer || !lv_obj_is_valid(m_mapContainer)) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const auto& contacts = client.getContacts();
    const auto& self = client.getSelfInfo();

    std::vector<const meshcore::MeshContact*> withGps;
    for (const auto& c : contacts) {
        if (c.lat != 0.0 || c.lon != 0.0) withGps.push_back(&c);
    }
    if (m_lblMapCount && lv_obj_is_valid(m_lblMapCount)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u nodos con posicion", (unsigned)withGps.size());
        lv_label_set_text(m_lblMapCount, buf);
    }
    lv_obj_clean(m_mapContainer);
    if (withGps.empty()) {
        lv_obj_t* lbl = lv_label_create(m_mapContainer);
        lv_label_set_text(lbl, "Ningun contacto comparte posicion.\nActiva compartir posicion en tu advert\ny pide a tus contactos que hagan lo mismo.");
        lv_obj_set_style_text_color(lbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        return;
    }
    for (const auto* c : withGps) {
        lv_obj_t* card = lv_obj_create(m_mapContainer);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        DefaultTheme::applySunkenCard(card, 8);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_t* lbl = lv_label_create(card);
        char buf[160];
        if (self.valid && (self.advLatitude != 0.0 || self.advLongitude != 0.0)) {
            snprintf(buf, sizeof(buf), "%s\n%.5f, %.5f · %.1f km · %s",
                     c->name.empty() ? c->prefixHex12.c_str() : c->name.c_str(), c->lat, c->lon,
                     haversineKm(self.advLatitude, self.advLongitude, c->lat, c->lon),
                     fmtAgo(c->lastAdvert ? c->lastAdvert : c->lastmod).c_str());
        } else {
            snprintf(buf, sizeof(buf), "%s\n%.5f, %.5f · %s",
                     c->name.empty() ? c->prefixHex12.c_str() : c->name.c_str(), c->lat, c->lon,
                     fmtAgo(c->lastAdvert ? c->lastAdvert : c->lastmod).c_str());
        }
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_width(lbl, LV_PCT(100));
    }
}

void MeshCoreView::exportGpx() {
    auto& client = meshcore::MeshCoreClient::getInstance();
    std::string gpx = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\">\n";
    int n = 0;
    for (const auto& c : client.getContacts()) {
        if (c.lat == 0.0 && c.lon == 0.0) continue;
        char wpt[256];
        snprintf(wpt, sizeof(wpt), "  <wpt lat=\"%.6f\" lon=\"%.6f\"><name>%s</name></wpt>\n", c.lat,
                 c.lon, (c.name.empty() ? c.prefixHex12 : c.name).c_str());
        gpx += wpt;
        ++n;
    }
    gpx += "</gpx>\n";
    if (n == 0) {
        UIManager::showToast("Sin posiciones para exportar.");
        return;
    }
    cbdos::storage::makeDir("/flash/data/meshcore");
    if (cbdos::storage::writeFile("/flash/data/meshcore/map.gpx", gpx)) {
        char buf[80];
        snprintf(buf, sizeof(buf), "GPX exportado: %d nodos.", n);
        UIManager::showToast(buf);
    } else {
        UIManager::showToast("Fallo al escribir GPX.");
    }
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

void MeshCoreView::searchTaCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taSearch) return;
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    const char* txt = lv_textarea_get_text(self->m_taSearch);
    self->m_searchFilter = txt ? txt : "";
    self->m_lastRenderedContactsSig = 0;  // forzar reconstrucción
    self->refreshContactsList();
}

void MeshCoreView::contactRowCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !target) return;
    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    if (idx >= self->m_rowPrefixes.size()) return;
    self->openConversation(self->m_rowPrefixes[idx]);
}

void MeshCoreView::contactMenuCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !target) return;
    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(target);
    if (idx >= self->m_rowPrefixes.size()) return;
    self->m_detailsReturn = ContactsPane::List;
    self->openDetails(self->m_rowPrefixes[idx]);
}

void MeshCoreView::convBackCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->showContactsPane(ContactsPane::List);
}

void MeshCoreView::convDetailsCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    self->m_detailsReturn = ContactsPane::Conversation;
    self->openDetails(self->m_activePrefix);
}

void MeshCoreView::convSendCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->sendDirectMessage();
}

void MeshCoreView::convRetryCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || self->m_activePrefix.empty()) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    uint32_t nowMs = (uint32_t)((uint64_t)time(nullptr) * 1000u);
    int n = 0;
    for (const auto& kv : client.getPendingDMs()) {
        if (kv.second.destPrefix == self->m_activePrefix) {
            if (client.retryPendingDm(kv.first, nowMs)) ++n;
        }
    }
    UIManager::showToast(n > 0 ? "Reintentando DM..." : "Sin pendientes.");
}

void MeshCoreView::dmInputEventCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        self->sendDirectMessage();
    } else if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if (self->m_keyboardDm && lv_obj_is_valid(self->m_keyboardDm)) {
            lv_keyboard_set_textarea(self->m_keyboardDm, self->m_taDmInput);
            lv_obj_remove_flag(self->m_keyboardDm, LV_OBJ_FLAG_HIDDEN);
            self->m_keyboardDmVisible = true;
        }
    }
}

void MeshCoreView::toggleDmKbBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_keyboardDm || !lv_obj_is_valid(self->m_keyboardDm)) return;

    self->m_keyboardDmVisible = !self->m_keyboardDmVisible;
    if (self->m_keyboardDmVisible) {
        lv_keyboard_set_textarea(self->m_keyboardDm, self->m_taDmInput);
        lv_obj_remove_flag(self->m_keyboardDm, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(self->m_keyboardDm, LV_OBJ_FLAG_HIDDEN);
    }
}

void MeshCoreView::dmKbEventCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        self->sendDirectMessage();
    } else if (code == LV_EVENT_CANCEL) {
        if (self->m_keyboardDm && lv_obj_is_valid(self->m_keyboardDm)) {
            lv_obj_add_flag(self->m_keyboardDm, LV_OBJ_FLAG_HIDDEN);
            self->m_keyboardDmVisible = false;
        }
    }
}

void MeshCoreView::detailsBackCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->showContactsPane(self->m_detailsReturn);
}

void MeshCoreView::detailsSaveNameCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taDetailName || self->m_activePrefix.empty()) return;
    const char* txt = lv_textarea_get_text(self->m_taDetailName);
    std::string name = txt ? txt : "";
    if (name.empty()) {
        UIManager::showToast("Nombre vacio.");
        return;
    }
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::MeshContact* c = client.findContact(self->m_activePrefix);
    if (!c) return;
    meshcore::MeshContact upd = *c;
    upd.name = name;
    if (client.isConnected()) {
        if (client.addOrUpdateContact(upd)) {
            UIManager::showToast("Nombre enviado al dongle.");
        } else {
            UIManager::showToast("Fallo al enviar.");
        }
    } else {
        // Offline-first: cambio local, se conserva en flash.
        auto all = client.getContacts();
        for (auto& x : all) {
            if (x.prefixHex12 == self->m_activePrefix) x.name = name;
        }
        client.setContactsForTest(all);
        UIManager::showToast("Guardado local (sin dongle).");
    }
    self->m_lastRenderedContactsSig = 0;
    self->refreshDetails();
}

void MeshCoreView::detailsSavePathCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taDetailPath || self->m_activePrefix.empty()) return;
    const char* txt = lv_textarea_get_text(self->m_taDetailPath);
    std::string hex = txt ? txt : "";
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::MeshContact* c = client.findContact(self->m_activePrefix);
    if (!c) return;
    meshcore::MeshContact upd = *c;
    if (hex.empty()) {
        upd.outPathLen = -1;
        memset(upd.outPath, 0, sizeof(upd.outPath));
    } else {
        if (hex.size() % 2 != 0 || hex.size() > 128) {
            UIManager::showToast("Ruta invalida (hex par, max 128).");
            return;
        }
        uint8_t raw[64] = {0};
        for (size_t i = 0; i < hex.size() / 2; ++i) {
            unsigned v = 0;
            if (sscanf(hex.c_str() + i * 2, "%2x", &v) != 1) {
                UIManager::showToast("Ruta invalida.");
                return;
            }
            raw[i] = (uint8_t)v;
        }
        upd.outPathLen = (int8_t)(hex.size() / 2);
        memcpy(upd.outPath, raw, sizeof(raw));
    }
    if (client.isConnected()) {
        UIManager::showToast(client.addOrUpdateContact(upd) ? "Ruta enviada." : "Fallo al enviar.");
    } else {
        auto all = client.getContacts();
        for (auto& x : all) {
            if (x.prefixHex12 == self->m_activePrefix) {
                x.outPathLen = upd.outPathLen;
                memcpy(x.outPath, upd.outPath, sizeof(x.outPath));
            }
        }
        client.setContactsForTest(all);
        UIManager::showToast("Guardado local (sin dongle).");
    }
    self->refreshDetails();
}

void MeshCoreView::detailsResetPathCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || self->m_activePrefix.empty()) return;
    if (meshcore::MeshCoreClient::getInstance().resetPathByPrefix(self->m_activePrefix)) {
        UIManager::showToast("Ruta reseteada a flood.");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::detailsShareCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || self->m_activePrefix.empty()) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::MeshContact* c = client.findContact(self->m_activePrefix);
    if (c && c->hasPubkey && client.shareContact(c->pubkey)) {
        UIManager::showToast("Advert re-emitido.");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::detailsRemoveCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || self->m_activePrefix.empty()) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::MeshContact* c = client.findContact(self->m_activePrefix);
    if (!c) return;
    if (client.isConnected() && c->hasPubkey && client.removeContact(c->pubkey)) {
        UIManager::showToast("Contacto borrado.");
    } else {
        auto all = client.getContacts();
        all.erase(std::remove_if(all.begin(), all.end(), [&](const meshcore::MeshContact& x) {
                      return x.prefixHex12 == self->m_activePrefix;
                  }),
                  all.end());
        client.setContactsForTest(all);
        UIManager::showToast("Borrado local (dongle no disponible).");
    }
    self->m_lastRenderedContactsSig = 0;
    self->showContactsPane(ContactsPane::List);
}

void MeshCoreView::detailsFavCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || self->m_activePrefix.empty()) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    auto all = client.getContacts();
    for (auto& x : all) {
        if (x.prefixHex12 == self->m_activePrefix) x.favourite = !x.favourite;
    }
    client.setContactsForTest(all);
    self->m_lastRenderedContactsSig = 0;
    self->refreshDetails();
    UIManager::showToast("Favorito actualizado.");
}

void MeshCoreView::advertBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->showOverlayAdvert();
}

void MeshCoreView::plusBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->showOverlayPlus();
}

void MeshCoreView::overlayCloseCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->hideOverlay();
}

void MeshCoreView::advertZeroCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    if (meshcore::MeshCoreClient::getInstance().sendAdvert(false)) {
        UIManager::showToast("Advert zero-hop enviado.");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
    self->hideOverlay();
}

void MeshCoreView::advertFloodCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    if (meshcore::MeshCoreClient::getInstance().sendAdvert(true)) {
        UIManager::showToast("Advert flood enviado.");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
    self->hideOverlay();
}

void MeshCoreView::plusDiscoverCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    if (meshcore::MeshCoreClient::getInstance().queryContacts()) {
        UIManager::showToast("Descubriendo contactos...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
    self->hideOverlay();
}

void MeshCoreView::plusManualCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->showOverlayManualAdd();
}

void MeshCoreView::plusImportCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->showOverlayImportCard();
}

void MeshCoreView::manualSaveCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taManualKey || !self->m_taManualName) return;
    const char* keyTxt = lv_textarea_get_text(self->m_taManualKey);
    const char* nameTxt = lv_textarea_get_text(self->m_taManualName);
    std::string keyHex = keyTxt ? keyTxt : "";
    std::string name = nameTxt ? nameTxt : "";
    uint8_t pk[meshcore::PUBKEY_LEN];
    if (!meshcore::MeshCoreClient::parsePubkeyHex64(keyHex, pk)) {
        UIManager::showToast("Pubkey invalida (64 hex).");
        return;
    }
    if (name.empty()) {
        UIManager::showToast("Falta el nombre.");
        return;
    }
    uint16_t typeSel = self->m_ddManualType ? lv_dropdown_get_selected(self->m_ddManualType) : 0;
    meshcore::MeshContact c;
    memcpy(c.pubkey, pk, sizeof(pk));
    c.hasPubkey = true;
    char hex[13];
    snprintf(hex, sizeof(hex), "%02x%02x%02x%02x%02x%02x", pk[0], pk[1], pk[2], pk[3], pk[4],
             pk[5]);
    c.prefixHex12 = hex;
    c.name = name;
    c.type = (uint8_t)(typeSel + 1);  // 1=Chat 2=Repetidor 3=Sala 4=Sensor
    c.outPathLen = -1;
    c.valid = true;
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (client.isConnected()) {
        UIManager::showToast(client.addOrUpdateContact(c) ? "Contacto guardado." : "Fallo al guardar.");
    } else {
        auto all = client.getContacts();
        all.push_back(c);
        client.setContactsForTest(all);
        UIManager::showToast("Guardado local (sin dongle).");
    }
    self->m_lastRenderedContactsSig = 0;
    self->hideOverlay();
}

void MeshCoreView::importSaveCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taImportCard) return;
    const char* txt = lv_textarea_get_text(self->m_taImportCard);
    std::string hex = txt ? txt : "";
    if (hex.size() < 2 || hex.size() % 2 != 0 || hex.size() > 512) {
        UIManager::showToast("Tarjeta invalida (hex).");
        return;
    }
    std::vector<uint8_t> card(hex.size() / 2);
    for (size_t i = 0; i < card.size(); ++i) {
        unsigned v = 0;
        if (sscanf(hex.c_str() + i * 2, "%2x", &v) != 1) {
            UIManager::showToast("Tarjeta invalida.");
            return;
        }
        card[i] = (uint8_t)v;
    }
    if (meshcore::MeshCoreClient::getInstance().importContact(card.data(), card.size())) {
        UIManager::showToast("Tarjeta enviada al dongle.");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
    self->hideOverlay();
}

void MeshCoreView::mapExportCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->exportGpx();
}

void MeshCoreView::discoverBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    if (meshcore::MeshCoreClient::getInstance().queryContacts()) {
        UIManager::showToast("Descubriendo contactos...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}

void MeshCoreView::applyRadioParams() {
    auto& client = meshcore::MeshCoreClient::getInstance();
    if (!client.isConnected()) {
        UIManager::showToast("Dongle desconectado.");
        return;
    }
    const char* fTxt = (m_taFreq && lv_obj_is_valid(m_taFreq)) ? lv_textarea_get_text(m_taFreq) : "";
    const char* bTxt = (m_taBw && lv_obj_is_valid(m_taBw)) ? lv_textarea_get_text(m_taBw) : "";
    const char* tTxt = (m_taTx && lv_obj_is_valid(m_taTx)) ? lv_textarea_get_text(m_taTx) : "";
    double freq = 0, bw = 0;
    long tx = tTxt ? atol(tTxt) : 0;
    uint8_t sf = (m_ddSf && lv_obj_is_valid(m_ddSf))
                     ? (uint8_t)(lv_dropdown_get_selected(m_ddSf) + 7)
                     : 7;
    uint8_t cr = (m_ddCr && lv_obj_is_valid(m_ddCr))
                     ? (uint8_t)(lv_dropdown_get_selected(m_ddCr) + 5)
                     : 5;
    std::string fStr = fTxt ? fTxt : "";
    std::string bStr = bTxt ? bTxt : "";
    if (!parseDec(fStr.c_str(), freq) || freq < 400.0 || freq > 2500.0 ||
        !parseDec(bStr.c_str(), bw) || bw < 7.0 || bw > 500.0 || tx < -9 || tx > 22) {
        UIManager::showToast("Valores invalidos. Revisa freq/BW/TX.");
        return;
    }
    // `set freq` persiste tras reboot (doc oficial); `set radio` fija el resto.
    if (!client.setFrequencyStr(trimNum(freq, 3))) {
        UIManager::showToast("Fallo al enviar frecuencia.");
        return;
    }
    client.setRadioStr(trimNum(freq, 3), trimNum(bw, 2), sf, cr);
    client.setTxPower((int)tx);
    client.reboot();
    UIManager::showToast("Aplicado. Dongle reiniciando...");
}

void MeshCoreView::radioApplyBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->applyRadioParams();
}

} // namespace ui
} // namespace cbdos
