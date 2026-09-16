#include "../RadioView.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"
#include "cbdos/audio.hpp"
#include "cbdos/display.hpp"
#include "cbdos/network.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"
#include "cbdos/rtos.hpp"
#include <cstring>
#include <cstdio>

namespace cbdos {
namespace ui {

static const char* TAG = "RadioView";

void RadioView::buildFavoritesView(lv_obj_t* parent) {
    m_favContainer = lv_obj_create(parent);
    lv_obj_set_width(m_favContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_favContainer, 1);
    lv_obj_set_flex_flow(m_favContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_favContainer, 0, 0);
    lv_obj_set_style_pad_row(m_favContainer, 6, 0);
    lv_obj_set_style_bg_opa(m_favContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_favContainer, 0, 0);

    // Barra de Selección de Playlist y Gestión
    lv_obj_t* topRow = lv_obj_create(m_favContainer);
    lv_obj_set_width(topRow, lv_pct(100));
    lv_obj_set_height(topRow, 42);
    lv_obj_set_style_bg_opa(topRow, 0, 0);
    lv_obj_set_style_border_width(topRow, 0, 0);
    lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(topRow, 0, 0);
    DefaultTheme::disableScroll(topRow);

    // Dropdown de Playlists
    m_playlistDropdown = lv_dropdown_create(topRow);
    lv_obj_set_flex_grow(m_playlistDropdown, 1);
    lv_obj_set_height(m_playlistDropdown, 38);
    DefaultTheme::applySunkenCard(m_playlistDropdown, 8);
    lv_obj_set_style_text_color(m_playlistDropdown, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_playlistDropdown, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_playlistDropdown, playlistDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    // Botón Gestión de Listas (⚙️ Listas)
    lv_obj_t* btnManage = lv_button_create(topRow);
    lv_obj_set_size(btnManage, 42, 38);
    DefaultTheme::applyButton(btnManage, 8);
    lv_obj_set_style_margin_left(btnManage, 6, 0);
    lv_obj_add_event_cb(btnManage, playlistManageBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblManage = lv_label_create(btnManage);
    lv_label_set_text(lblManage, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(lblManage, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_center(lblManage);

    // Contenedor scrollable de estaciones
    m_favStationsList = lv_obj_create(m_favContainer);
    lv_obj_set_width(m_favStationsList, lv_pct(100));
    lv_obj_set_flex_grow(m_favStationsList, 1);
    lv_obj_set_flex_flow(m_favStationsList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_favStationsList, 4, 0);
    lv_obj_set_style_pad_row(m_favStationsList, 6, 0);
    lv_obj_set_style_bg_opa(m_favStationsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_favStationsList, 0, 0);

    refreshPlaylistDropdown();
    refreshFavoritesUI();
}

void RadioView::refreshFavoritesUI() {
    if (!m_favStationsList || !lv_obj_is_valid(m_favStationsList)) return;
    lv_obj_clean(m_favStationsList);

    const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
    if (playlists.empty() || m_selectedPlaylistIdx >= playlists.size()) {
        return;
    }

    const auto& currentPlaylist = playlists[m_selectedPlaylistIdx];
    const auto& stations = currentPlaylist.stations;

    if (stations.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_favStationsList);
        char msg[128];
        snprintf(msg, sizeof(msg), "La lista \"%s\" esta vacia.\nExplora o agrega emisoras.", currentPlaylist.name.c_str());
        lv_label_set_text(emptyLbl, msg);
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_margin_top(emptyLbl, 40, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < stations.size(); ++i) {
        const auto& st = stations[i];

        lv_obj_t* row = lv_obj_create(m_favStationsList);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 8);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 6, 0);
        DefaultTheme::disableScroll(row);

        // Bloque de información (Texto unificado)
        lv_obj_t* nameLbl = lv_label_create(row);
        char txt[160];
        snprintf(txt, sizeof(txt), "%s\n%s | %s", st.name.c_str(), st.genre.c_str(), st.country.c_str());
        lv_label_set_text(nameLbl, txt);
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_width(nameLbl, 240);

        // Botones de acción derecha (Play & Delete)
        lv_obj_t* actionsRow = lv_obj_create(row);
        lv_obj_set_size(actionsRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(actionsRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(actionsRow, 0, 0);
        lv_obj_set_style_pad_column(actionsRow, 6, 0);
        lv_obj_set_style_bg_opa(actionsRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actionsRow, 0, 0);
        DefaultTheme::disableScroll(actionsRow);

        // Botón Play
        lv_obj_t* btnPlay = lv_button_create(actionsRow);
        lv_obj_set_size(btnPlay, 36, 36);
        DefaultTheme::applyButton(btnPlay, 18);
        lv_obj_set_style_bg_color(btnPlay, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_user_data(btnPlay, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnPlay, favPlayCb, LV_EVENT_CLICKED, this);

        lv_obj_t* playIcon = lv_label_create(btnPlay);
        lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(playIcon, lv_color_hex(0x000000), 0);
        lv_obj_center(playIcon);

        // Botón Delete
        lv_obj_t* btnDel = lv_button_create(actionsRow);
        lv_obj_set_size(btnDel, 36, 36);
        DefaultTheme::applyButton(btnDel, 8);
        lv_obj_set_style_bg_color(btnDel, lv_color_hex(0x282C3C), 0);
        lv_obj_set_user_data(btnDel, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnDel, favDeleteCb, LV_EVENT_CLICKED, this);

        lv_obj_t* delIcon = lv_label_create(btnDel);
        lv_label_set_text(delIcon, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(delIcon, lv_color_hex(0xEF4444), 0);
        lv_obj_center(delIcon);
    }
}

void RadioView::refreshPlaylistDropdown() {
    if (!m_playlistDropdown || !lv_obj_is_valid(m_playlistDropdown)) return;

    const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
    std::string options = "";
    for (size_t i = 0; i < playlists.size(); ++i) {
        if (i > 0) options += "\n";
        options += playlists[i].name;
    }

    lv_dropdown_set_options(m_playlistDropdown, options.c_str());
    if (m_selectedPlaylistIdx >= playlists.size()) {
        m_selectedPlaylistIdx = 0;
    }
    lv_dropdown_set_selected(m_playlistDropdown, m_selectedPlaylistIdx);
}

void RadioView::favPlayCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);

    const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
    if (self && self->m_selectedPlaylistIdx < playlists.size()) {
        const auto& stations = playlists[self->m_selectedPlaylistIdx].stations;
        if (idx < stations.size()) {
            playStation(stations[idx]);
        }
    }
}

void RadioView::favDeleteCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);

    const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
    if (self && self->m_selectedPlaylistIdx < playlists.size()) {
        audio::RadioManager::getInstance().removeStationFromPlaylist(playlists[self->m_selectedPlaylistIdx].id, idx);
        UIManager::showToast("Emisora eliminada");
        if (self) {
            lv_async_call([](void* userData) {
                RadioView* view = static_cast<RadioView*>(userData);
                if (view) view->refreshFavoritesUI();
            }, self);
        }
    }
}

void RadioView::playlistDropdownCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self || !self->m_playlistDropdown) return;

    self->m_selectedPlaylistIdx = lv_dropdown_get_selected(self->m_playlistDropdown);
    self->refreshFavoritesUI();
}

void RadioView::playlistManageBtnCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (self) {
        self->showPlaylistManageModal();
    }
}
} // namespace ui
} // namespace cbdos
