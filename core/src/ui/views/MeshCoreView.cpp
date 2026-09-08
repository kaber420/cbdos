#include "MeshCoreView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/serial.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>

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
} // namespace

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

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 4, 0);
    DefaultTheme::disableScroll(m_container);

    m_tabview = lv_tabview_create(m_container);
    lv_tabview_set_tab_bar_position(m_tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(m_tabview, 38);
    lv_obj_set_size(m_tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_tabview, 0, 0);
    lv_obj_set_style_border_width(m_tabview, 0, 0);

    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(m_tabview);
    DefaultTheme::applySunkenCard(tab_bar, 10);
    lv_obj_set_style_pad_all(tab_bar, 2, 0);
    lv_obj_set_style_pad_column(tab_bar, 4, 0);

    lv_obj_t* tab_chats = lv_tabview_add_tab(m_tabview, LV_SYMBOL_EDIT " Chats");
    lv_obj_t* tab_channels = lv_tabview_add_tab(m_tabview, LV_SYMBOL_LIST " Canales");
    lv_obj_t* tab_radio = lv_tabview_add_tab(m_tabview, LV_SYMBOL_WIFI " Radio");

    buildChatsTab(tab_chats);
    buildChannelsTab(tab_channels);
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

    if (!client.isConnected()) {
        client.connect(m_selectedPort, m_selectedBaud);
    }
    // Estado inicial: canales, batería y drenado de mensajes en cola.
    client.queryAllChannels();
    client.queryBattery();
    client.pollMessages();

    setActiveChannel(0);
    refreshChatLog();
    refreshChannelsList();
    refreshRadioStatus();

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

    m_tabview = nullptr;
    m_chatContainer = nullptr;
    m_taInput = nullptr;
    m_keyboard = nullptr;
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
    const auto& dmHist = client.getContactHistory();

    if (chHist.size() == m_lastRenderedChCount &&
        dmHist.size() == m_lastRenderedDmCount &&
        (!chHist.empty() || !dmHist.empty())) {
        return;
    }

    lv_obj_clean(m_chatContainer);
    m_lastRenderedChCount = chHist.size();
    m_lastRenderedDmCount = dmHist.size();

    if (chHist.empty() && dmHist.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_chatContainer);
        lv_label_set_text(emptyLbl, "Sin mensajes.\nEscribe abajo para el canal activo.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (const auto& msg : chHist) {
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
    }

    if (!dmHist.empty()) {
        lv_obj_t* sep = lv_label_create(m_chatContainer);
        lv_label_set_text(sep, "--- Mensajes directos ---");
        lv_obj_set_style_text_color(sep, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(sep, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(sep, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(sep, LV_PCT(100));

        for (const auto& dm : dmHist) {
            lv_obj_t* bubble = lv_obj_create(m_chatContainer);
            lv_obj_set_width(bubble, LV_PCT(85));
            lv_obj_set_height(bubble, LV_SIZE_CONTENT);
            lv_obj_set_style_radius(bubble, 8, 0);
            lv_obj_set_style_pad_all(bubble, 6, 0);
            lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(bubble, 2, 0);
            lv_obj_set_align(bubble, LV_ALIGN_TOP_LEFT);
            lv_obj_set_style_bg_color(bubble, lv_color_hex(0x2A1A34), 0);
            lv_obj_set_style_border_color(bubble, lv_color_hex(0x5A3A6A), 0);
            lv_obj_set_style_border_width(bubble, 1, 0);
            DefaultTheme::disableScroll(bubble);

            lv_obj_t* lblSender = lv_label_create(bubble);
            std::string hdr = "DM " + dm.pubkeyPrefix + " - " + fmtTime(dm.timestamp);
            lv_label_set_text(lblSender, hdr.c_str());
            lv_obj_set_style_text_color(lblSender, lv_color_hex(0xCE93D8), 0);
            lv_obj_set_style_text_font(lblSender, &lv_font_montserrat_12, 0);
            lv_obj_set_width(lblSender, LV_PCT(100));

            lv_obj_t* lblText = lv_label_create(bubble);
            lv_label_set_text(lblText, dm.text.c_str());
            lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(lblText, LV_PCT(100));
            lv_obj_set_style_text_color(lblText, lv_color_hex(0xF0F4F8), 0);
            lv_obj_set_style_text_font(lblText, &lv_font_montserrat_12, 0);
        }
    }

    lv_obj_scroll_to_view(lv_obj_get_child(m_chatContainer, -1), LV_ANIM_OFF);
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

    m_taNewSecret = lv_textarea_create(form);
    lv_obj_set_size(m_taNewSecret, LV_PCT(100), 32);
    DefaultTheme::applySunkenCard(m_taNewSecret, 6);
    lv_textarea_set_one_line(m_taNewSecret, true);
    lv_textarea_set_placeholder_text(m_taNewSecret, "Secreto hex 32 chars (vacio + #nombre = hashtag)");
    lv_obj_set_style_text_font(m_taNewSecret, &lv_font_montserrat_12, 0);

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
    if (!ports.empty()) {
        portOpts.clear();
        for (size_t p = 0; p < ports.size(); ++p) {
            if (p > 0) portOpts += "\n";
            portOpts += ports[p].id;
        }
    }
    lv_dropdown_set_options(m_ddPort, portOpts.c_str());
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
    }

    if (self->m_channelsDirty) {
        self->m_channelsDirty = false;
        self->refreshChannelsList();
    }

    if (self->m_radioDirty) {
        self->m_radioDirty = false;
        self->refreshRadioStatus();
    }
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
    if (self->m_tabview && lv_obj_is_valid(self->m_tabview)) {
        lv_tabview_set_active(self->m_tabview, 0, LV_ANIM_OFF);
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
        client.queryAllChannels();
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

    char buf[32];
    lv_dropdown_get_selected_str(self->m_ddPort, buf, sizeof(buf));
    self->m_selectedPort = buf;
}

} // namespace ui
} // namespace cbdos
