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

void RadioView::buildExploreView(lv_obj_t* parent) {
    m_exploreContainer = lv_obj_create(parent);
    lv_obj_set_width(m_exploreContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_exploreContainer, 1);
    lv_obj_set_flex_flow(m_exploreContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_exploreContainer, 4, 0);
    lv_obj_set_style_pad_row(m_exploreContainer, 6, 0);
    lv_obj_set_style_bg_opa(m_exploreContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_exploreContainer, 0, 0);

    // Barra de Búsqueda
    lv_obj_t* searchRow = lv_obj_create(m_exploreContainer);
    lv_obj_set_width(searchRow, lv_pct(100));
    lv_obj_set_height(searchRow, 42);
    lv_obj_set_flex_flow(searchRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(searchRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(searchRow, 0, 0);
    lv_obj_set_style_bg_opa(searchRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(searchRow, 0, 0);
    DefaultTheme::disableScroll(searchRow);

    m_taSearch = lv_textarea_create(searchRow);
    lv_obj_set_width(m_taSearch, lv_pct(76));
    lv_obj_set_height(m_taSearch, 40);
    DefaultTheme::applySunkenCard(m_taSearch, 8);
    lv_textarea_set_placeholder_text(m_taSearch, "Buscar emisora o genero...");
    lv_textarea_set_one_line(m_taSearch, true);
    lv_obj_set_style_text_color(m_taSearch, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taSearch);

    m_btnSearch = lv_button_create(searchRow);
    lv_obj_set_width(m_btnSearch, lv_pct(22));
    lv_obj_set_height(m_btnSearch, 40);
    DefaultTheme::applyButton(m_btnSearch, 8);
    lv_obj_set_style_bg_color(m_btnSearch, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_btnSearch, searchBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* searchLbl = lv_label_create(m_btnSearch);
    lv_label_set_text(searchLbl, "Buscar");
    lv_obj_set_style_text_color(searchLbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(searchLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(searchLbl);

    // Contenedor de Resultados con Scroll
    m_exploreList = lv_obj_create(m_exploreContainer);
    lv_obj_set_width(m_exploreList, lv_pct(100));
    lv_obj_set_flex_grow(m_exploreList, 1);
    lv_obj_set_flex_flow(m_exploreList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_exploreList, 0, 0);
    lv_obj_set_style_pad_row(m_exploreList, 6, 0);
    lv_obj_set_style_bg_opa(m_exploreList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_exploreList, 0, 0);

    // Barra de Paginación Inferior
    lv_obj_t* pageRow = lv_obj_create(m_exploreContainer);
    lv_obj_set_width(pageRow, lv_pct(100));
    lv_obj_set_height(pageRow, 38);
    lv_obj_set_flex_flow(pageRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pageRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(pageRow, 0, 0);
    lv_obj_set_style_bg_opa(pageRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pageRow, 0, 0);
    DefaultTheme::disableScroll(pageRow);

    m_btnPrevPage = lv_button_create(pageRow);
    lv_obj_set_size(m_btnPrevPage, 90, 36);
    DefaultTheme::applyButton(m_btnPrevPage, 8);
    lv_obj_add_event_cb(m_btnPrevPage, navPrevCb, LV_EVENT_CLICKED, this);
    lv_obj_t* prevLbl = lv_label_create(m_btnPrevPage);
    lv_label_set_text(prevLbl, LV_SYMBOL_LEFT " Ant");
    lv_obj_center(prevLbl);

    m_pageLbl = lv_label_create(pageRow);
    lv_label_set_text(m_pageLbl, "Pagina 1");
    lv_obj_set_style_text_color(m_pageLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_pageLbl, &lv_font_montserrat_12, 0);

    m_btnNextPage = lv_button_create(pageRow);
    lv_obj_set_size(m_btnNextPage, 90, 36);
    DefaultTheme::applyButton(m_btnNextPage, 8);
    lv_obj_add_event_cb(m_btnNextPage, navNextCb, LV_EVENT_CLICKED, this);
    lv_obj_t* nextLbl = lv_label_create(m_btnNextPage);
    lv_label_set_text(nextLbl, "Sig " LV_SYMBOL_RIGHT);
    lv_obj_center(nextLbl);
}

void RadioView::refreshExploreUI(const std::vector<audio::RadioStation>& stations) {
    if (!m_exploreList) return;
    lv_obj_clean(m_exploreList);

    if (stations.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_exploreList);
        lv_label_set_text(emptyLbl, "Escribe un genero o nombre y toca 'Buscar'.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_margin_top(emptyLbl, 30, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < stations.size(); ++i) {
        const auto& st = stations[i];

        lv_obj_t* row = lv_obj_create(m_exploreList);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        DefaultTheme::applyRaisedCard(row, 8);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 6, 0);
        DefaultTheme::disableScroll(row);

        // Info unificada estilo espOS32
        lv_obj_t* nameLbl = lv_label_create(row);
        char txt[160];
        snprintf(txt, sizeof(txt), "%s\n%s | %s (%dk)", st.name.c_str(), st.genre.c_str(), st.country.c_str(), st.bitrate);
        lv_label_set_text(nameLbl, txt);
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_width(nameLbl, 240);

        // Actions Row
        lv_obj_t* actionsRow = lv_obj_create(row);
        lv_obj_set_size(actionsRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(actionsRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(actionsRow, 0, 0);
        lv_obj_set_style_pad_column(actionsRow, 6, 0);
        lv_obj_set_style_bg_opa(actionsRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(actionsRow, 0, 0);
        DefaultTheme::disableScroll(actionsRow);

        // Play Button
        lv_obj_t* btnPlay = lv_button_create(actionsRow);
        lv_obj_set_size(btnPlay, 36, 36);
        DefaultTheme::applyButton(btnPlay, 18);
        lv_obj_set_style_bg_color(btnPlay, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_user_data(btnPlay, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnPlay, explorePlayCb, LV_EVENT_CLICKED, this);

        lv_obj_t* playIcon = lv_label_create(btnPlay);
        lv_label_set_text(playIcon, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(playIcon, lv_color_hex(0x000000), 0);
        lv_obj_center(playIcon);

        // Favorite Button (+)
        lv_obj_t* btnFav = lv_button_create(actionsRow);
        lv_obj_set_size(btnFav, 36, 36);
        DefaultTheme::applyButton(btnFav, 8);
        lv_obj_set_style_bg_color(btnFav, st.isFavorite ? DefaultTheme::getSecondaryAccent() : lv_color_hex(0x282C3C), 0);
        lv_obj_set_user_data(btnFav, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btnFav, exploreFavCb, LV_EVENT_CLICKED, this);

        lv_obj_t* starIcon = lv_label_create(btnFav);
        lv_label_set_text(starIcon, LV_SYMBOL_PLUS);
        lv_obj_set_style_text_color(starIcon, lv_color_hex(0xFFB800), 0);
        lv_obj_center(starIcon);
    }
}

void RadioView::performSearch() {
    if (s_searchInProgress) return;

    if (!cbdos::network::isConnected()) {
        UIManager::showToast("Sin conexion WiFi. Conecta primero.");
        return;
    }

    if (s_currentSearchQuery.empty()) {
        UIManager::showToast("Escribe algo para buscar");
        return;
    }

    s_searchInProgress = true;
    s_searchCompleted = false;

    // Mostrar estado de carga en la lista (exacto a espOS32)
    if (m_exploreList && lv_obj_is_valid(m_exploreList)) {
        lv_obj_clean(m_exploreList);
        lv_obj_t* loadingLbl = lv_label_create(m_exploreList);
        lv_label_set_text(loadingLbl, "Buscando emisoras en linea...");
        lv_obj_set_style_text_color(loadingLbl, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(loadingLbl);
    }

    if (m_currentStatusLbl && lv_obj_is_valid(m_currentStatusLbl)) {
        lv_label_set_text(m_currentStatusLbl, "Buscando emisoras...");
    }

    // Iniciar temporizador de sondeo de LVGL (60ms exacto a espOS32)
    m_searchPollTimer = lv_timer_create(searchPollTimerCb, 60, this);

    // Lanzar tarea de red en Core 0 con prioridad 1
    cbdos::rtos::TaskHandle taskH = cbdos::rtos::createTask(
        asyncSearchTask,
        "RadioSearch",
        8192,
        NULL,
        1,
        0
    );

    if (!taskH) {
        s_searchInProgress = false;
        if (m_searchPollTimer) {
            lv_timer_delete(m_searchPollTimer);
            m_searchPollTimer = nullptr;
        }
        if (m_exploreList && lv_obj_is_valid(m_exploreList)) {
            lv_obj_clean(m_exploreList);
            lv_obj_t* errLbl = lv_label_create(m_exploreList);
            lv_label_set_text(errLbl, "Error al iniciar busqueda");
            lv_obj_set_style_text_color(errLbl, lv_color_hex(0xFF5555), 0);
            lv_obj_center(errLbl);
        }
    }
}

void RadioView::searchBtnCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taSearch) return;

    const char* txt = lv_textarea_get_text(self->m_taSearch);
    if (!txt || strlen(txt) == 0) {
        UIManager::showToast("Escribe algo para buscar");
        return;
    }
    s_currentSearchQuery = txt;
    s_currentSearchOffset = 0;
    self->performSearch();
}

void RadioView::navPrevCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (s_currentSearchOffset >= 10) {
        s_currentSearchOffset -= 10;
        if (self) self->performSearch();
    }
}

void RadioView::navNextCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    s_currentSearchOffset += 10;
    if (self) self->performSearch();
}

void RadioView::searchPollTimerCb(lv_timer_t* timer) {
    if (s_searchCompleted) {
        s_searchCompleted = false;
        lv_timer_delete(timer);
        s_searchInProgress = false;

        RadioView* self = static_cast<RadioView*>(lv_timer_get_user_data(timer));
        if (self) {
            self->m_searchPollTimer = nullptr;
            s_currentExploreStations = std::move(s_asyncSearchResults);
            s_asyncSearchResults.clear();

            if (self->m_pageLbl && lv_obj_is_valid(self->m_pageLbl)) {
                char pBuf[32];
                snprintf(pBuf, sizeof(pBuf), "Pagina %d", (int)(s_currentSearchOffset / 10) + 1);
                lv_label_set_text(self->m_pageLbl, pBuf);
            }
            if (self->m_currentStatusLbl && lv_obj_is_valid(self->m_currentStatusLbl)) {
                lv_label_set_text(self->m_currentStatusLbl, s_isPlaying ? "Reproduciendo" : "Listo");
            }

            self->refreshExploreUI(s_currentExploreStations);
        }
    }
}

void RadioView::asyncSearchTask(void* param) {
    s_asyncSearchResults = audio::RadioManager::getInstance().searchStations(
        s_currentSearchQuery, s_currentSearchOffset, 10);
    s_searchCompleted = true;
    cbdos::rtos::deleteTask(nullptr);
}

void RadioView::explorePlayCb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < s_currentExploreStations.size()) {
        playStation(s_currentExploreStations[idx]);
    }
}

void RadioView::exploreFavCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (idx < s_currentExploreStations.size()) {
        const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
        std::string targetId = "fav";
        if (self && self->m_selectedPlaylistIdx < playlists.size()) {
            targetId = playlists[self->m_selectedPlaylistIdx].id;
        }

        audio::RadioManager::getInstance().addStationToPlaylist(targetId, s_currentExploreStations[idx]);
        s_currentExploreStations[idx].isFavorite = true;
        lv_obj_set_style_bg_color(btn, DefaultTheme::getSecondaryAccent(), 0);
        UIManager::showToast("Guardada en la lista activa");
    }
}
} // namespace ui
} // namespace cbdos
