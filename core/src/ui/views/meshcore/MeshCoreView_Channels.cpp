#include "../MeshCoreView.hpp"
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
#include "MeshCoreCommon.hpp"
#include <algorithm>
#include <map>
#include <vector>

namespace cbdos {
namespace ui {
using namespace meshcore_ui;

void MeshCoreView::buildChatsTab(lv_obj_t* tab) {
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

    lv_obj_t* btnEmoji = lv_button_create(inputRow);
    lv_obj_set_size(btnEmoji, 40, 36);
    DefaultTheme::applyButton(btnEmoji, 6);
    lv_obj_t* lblEmoji = lv_label_create(btnEmoji);
    lv_label_set_text(lblEmoji, ":)");
    lv_obj_set_style_text_font(lblEmoji, &lv_font_montserrat_12, 0);
    lv_obj_center(lblEmoji);
    lv_obj_set_user_data(btnEmoji, (void*)(uintptr_t)0);  // target 0 = canal
    lv_obj_add_event_cb(btnEmoji, emojiBtnCb, LV_EVENT_CLICKED, this);

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
    UIManager::attachKeyboard(m_taInput, {
        .autoCloseOnSubmit = true,
        .onSubmit = [this](const char* txt) {
            this->sendMessage();
        }
    });

    m_btnSend = lv_button_create(inputRow);
    lv_obj_set_size(m_btnSend, 64, 36);
    DefaultTheme::applyButton(m_btnSend, 6);
    lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x1B5E20), 0);
    lv_obj_t* lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(lblSend, LV_SYMBOL_OK " Enviar");
    lv_obj_set_style_text_font(lblSend, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSend);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);
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

    freeChatWinkBufs();  // los draw bufs del chat mueren con sus burbujas
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

        // POC Emoji + Winks inline: token de 5-6 B -> animacion 96px con
        // autoplay de 1 pasada; tap = loop mientras este en vista.
        int winkId = meshcore::emoji::winkIdForText(msg.text);
        int faceIdx = (winkId < 0) ? meshcore::emoji::singleEmojiIndex(msg.text) : -1;
        if (winkId >= 0) {
            createInlineWink(bubble, winkId, false);
        } else if (faceIdx >= 0) {
            size_t nFaces = 0;
            const auto* faces = meshcore::emoji::emojiTable(nFaces);
            // Carita amarilla vectorial (sin fuente emoji): circulo + ojos + boca.
            lv_obj_t* faceRow = lv_obj_create(bubble);
            lv_obj_set_size(faceRow, LV_PCT(100), 56);
            lv_obj_set_style_bg_opa(faceRow, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(faceRow, 0, 0);
            lv_obj_set_style_pad_all(faceRow, 0, 0);
            lv_obj_set_flex_flow(faceRow, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(faceRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(faceRow, 8, 0);
            DefaultTheme::disableScroll(faceRow);
            lv_obj_t* face = lv_obj_create(faceRow);
            lv_obj_set_size(face, 48, 48);
            lv_obj_set_style_radius(face, 24, 0);
            lv_obj_set_style_bg_color(face, lv_color_hex(0xFFD93B), 0);
            lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(face, 0, 0);
            DefaultTheme::disableScroll(face);
            lv_obj_t* eyeL = lv_obj_create(face);
            lv_obj_set_size(eyeL, 7, 9);
            lv_obj_set_style_radius(eyeL, 3, 0);
            lv_obj_set_style_bg_color(eyeL, lv_color_hex(0x111111), 0);
            lv_obj_set_style_border_width(eyeL, 0, 0);
            lv_obj_set_pos(eyeL, 11, 13);
            lv_obj_t* eyeR = lv_obj_create(face);
            lv_obj_set_size(eyeR, 7, 9);
            lv_obj_set_style_radius(eyeR, 3, 0);
            lv_obj_set_style_bg_color(eyeR, lv_color_hex(0x111111), 0);
            lv_obj_set_style_border_width(eyeR, 0, 0);
            lv_obj_set_pos(eyeR, 30, 13);
            lv_obj_t* mouth = lv_obj_create(face);
            lv_obj_set_size(mouth, 22, 7);
            lv_obj_set_style_radius(mouth, 3, 0);
            lv_obj_set_style_bg_color(mouth, lv_color_hex(0x111111), 0);
            lv_obj_set_style_border_width(mouth, 0, 0);
            lv_obj_set_pos(mouth, 13, 30);
            lv_obj_t* lblFace = lv_label_create(faceRow);
            lv_label_set_text(lblFace, faces[(size_t)faceIdx].name);
            lv_obj_set_style_text_color(lblFace, lv_color_hex(0xF0F4F8), 0);
            lv_obj_set_style_text_font(lblFace, &lv_font_montserrat_12, 0);
        } else {
            lv_obj_t* lblText = lv_label_create(bubble);
            lv_label_set_text(lblText, msg.text.c_str());
            lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(lblText, LV_PCT(100));
            lv_obj_set_style_text_color(lblText, lv_color_hex(0xF0F4F8), 0);
            lv_obj_set_style_text_font(lblText, &lv_font_montserrat_12, 0);
        }
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
void MeshCoreView::sendBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->sendMessage();
}

void MeshCoreView::toggleKbBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taInput) return;
    UIManager::openKeyboard(self->m_taInput);
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

} // namespace ui
} // namespace cbdos
