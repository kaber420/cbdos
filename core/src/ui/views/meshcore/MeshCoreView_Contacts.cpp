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
    UIManager::attachKeyboard(m_taDmInput, {
        .autoCloseOnSubmit = true,
        .onSubmit = [this](const char* txt) {
            this->sendDirectMessage();
        }
    });

    m_btnDmKb = makeButton(dmRow, LV_SYMBOL_KEYBOARD, 40, 36, 0);
    lv_obj_add_event_cb(m_btnDmKb, toggleDmKbBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnDmEmoji = makeButton(dmRow, ":)", 40, 36, 0);
    lv_obj_set_user_data(btnDmEmoji, (void*)(uintptr_t)1);  // target 1 = DM
    lv_obj_add_event_cb(btnDmEmoji, emojiBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnDmSend = makeButton(dmRow, LV_SYMBOL_OK " Enviar", 80, 36, 0x1B5E20);
    lv_obj_add_event_cb(btnDmSend, convSendCb, LV_EVENT_CLICKED, this);
    lv_obj_t* btnRetry = makeButton(dmRow, LV_SYMBOL_REFRESH, 40, 36, 0);
    lv_obj_add_event_cb(btnRetry, convRetryCb, LV_EVENT_CLICKED, this);

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
    m_lastRenderedConvFirstTs = 0;
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
    // Vía delta: mismo hilo, solo anexiones al final (sin poda por tope) y
    // mismos hijos ya creados. Sin clean, sin flicker, sin releer historial.
    bool sameThread = th && (m_lastRenderedConvPrefix == m_activePrefix);
    bool noShift = (nMsgs == 0) || (!th->msgs.empty() &&
                    th->msgs[0].timestamp == m_lastRenderedConvFirstTs);
    if (sameThread && noShift && nMsgs >= m_lastRenderedConvMsgs &&
        lv_obj_get_child_cnt(m_convContainer) == (uint32_t)m_lastRenderedConvMsgs) {
        if (nPending != m_lastRenderedConvPending) {
            m_lastRenderedConvPending = nPending;
            updateConvPendingLabels(nPending);
            updateConvStatusLabel(nPending);
        }
        if (nMsgs > m_lastRenderedConvMsgs) {
            for (size_t i = m_lastRenderedConvMsgs; i < nMsgs; ++i) {
                createDmBubble(th->msgs[i], nPending);
            }
            m_lastRenderedConvMsgs = nMsgs;
            lv_obj_scroll_to_view(lv_obj_get_child(m_convContainer, -1), LV_ANIM_OFF);
        }
        return;
    }
    m_lastRenderedConvMsgs = nMsgs;
    m_lastRenderedConvPrefix = m_activePrefix;
    m_lastRenderedConvPending = nPending;
    m_lastRenderedConvFirstTs = (nMsgs > 0 && th) ? th->msgs[0].timestamp : 0;

    freeConvWinkBufs();  // los draw bufs del DM mueren con sus burbujas
    lv_obj_clean(m_convContainer);
    if (!th || th->msgs.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_convContainer);
        lv_label_set_text(emptyLbl, "Sin mensajes. Escribe abajo para enviar un DM cifrado.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (const auto& msg : th->msgs) {
            createDmBubble(msg, nPending);
        }
        lv_obj_scroll_to_view(lv_obj_get_child(m_convContainer, -1), LV_ANIM_OFF);
    }

    if (m_lblConvStatus && lv_obj_is_valid(m_lblConvStatus)) {
        updateConvStatusLabel(nPending);
    }
}

lv_obj_t* MeshCoreView::createDmBubble(const meshcore::ContactMessage& msg, size_t nPending) {
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

    // POC Emoji + Winks inline (misma logica que el chat de canal).
    int winkId = meshcore::emoji::winkIdForText(msg.text);
    int faceIdx = (winkId < 0) ? meshcore::emoji::singleEmojiIndex(msg.text) : -1;
    if (winkId >= 0) {
        createInlineWink(bubble, winkId, true);
    } else if (faceIdx >= 0) {
        size_t nFaces = 0;
        const auto* faces = meshcore::emoji::emojiTable(nFaces);
        lv_obj_t* face = lv_obj_create(bubble);
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
        lv_obj_t* lblFace = lv_label_create(bubble);
        lv_label_set_text(lblFace, faces[(size_t)faceIdx].name);
        lv_obj_set_style_text_color(lblFace, lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_style_text_font(lblFace, &lv_font_montserrat_12, 0);
        lv_obj_set_width(lblFace, LV_PCT(100));
    } else {
        lv_obj_t* lblText = lv_label_create(bubble);
        lv_label_set_text(lblText, msg.text.c_str());
        lv_label_set_long_mode(lblText, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lblText, LV_PCT(100));
        lv_obj_set_style_text_color(lblText, lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_style_text_font(lblText, &lv_font_montserrat_12, 0);
    }

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
    return bubble;
}

void MeshCoreView::updateConvPendingLabels(size_t nPending) {
    if (!m_convContainer || !lv_obj_is_valid(m_convContainer)) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const meshcore::DMThread* th = client.getThread(m_activePrefix);
    if (!th || th->msgs.empty()) return;
    // Los hijos van en el mismo orden de creación que th->msgs.
    uint32_t childCount = lv_obj_get_child_cnt(m_convContainer);
    if (childCount != (uint32_t)th->msgs.size()) {
        // Desincronizado: forzar reconstrucción completa.
        m_lastRenderedConvMsgs = (size_t)-1;
        m_lastRenderedConvPending = (size_t)-1;
        refreshConversation();
        return;
    }
    for (uint32_t i = 0; i < childCount; ++i) {
        const auto& msg = th->msgs[(size_t)i];
        lv_obj_t* bubble = lv_obj_get_child(m_convContainer, (int32_t)i);
        if (!bubble || !lv_obj_is_valid(bubble)) continue;
        lv_obj_t* lblMeta = lv_obj_get_child(bubble, -1);  // última: la meta
        if (!lblMeta || !lv_obj_is_valid(lblMeta)) continue;
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
    }
}

void MeshCoreView::updateConvStatusLabel(size_t nPending) {
    if (!m_lblConvStatus || !lv_obj_is_valid(m_lblConvStatus)) return;
    if (nPending > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Enviando... %u pendiente(s) de ACK", (unsigned)nPending);
        lv_label_set_text(m_lblConvStatus, buf);
    } else {
        lv_label_set_text(m_lblConvStatus, "");
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
        m_lastRenderedConvFirstTs = 0;
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

void MeshCoreView::toggleDmKbBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taDmInput) return;
    UIManager::openKeyboard(self->m_taDmInput);
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

void MeshCoreView::discoverBtnCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (!self) return;
    if (meshcore::MeshCoreClient::getInstance().queryContacts()) {
        UIManager::showToast("Descubriendo contactos...");
    } else {
        UIManager::showToast("Dongle desconectado.");
    }
}


} // namespace ui
} // namespace cbdos
