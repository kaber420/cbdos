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

void RadioView::buildPlayerBar(lv_obj_t* parent) {
    m_playerBar = lv_obj_create(parent);
    lv_obj_set_width(m_playerBar, lv_pct(100));
    lv_obj_set_height(m_playerBar, 64);
    DefaultTheme::applyRaisedCard(m_playerBar, 12);
    lv_obj_set_flex_flow(m_playerBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_playerBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(m_playerBar, 8, 0);
    DefaultTheme::disableScroll(m_playerBar);

    // Columna izquierda: Icono + Textos (Nombre y Estado)
    lv_obj_t* leftCol = lv_obj_create(m_playerBar);
    lv_obj_set_size(leftCol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(leftCol, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftCol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(leftCol, 0, 0);
    lv_obj_set_style_pad_column(leftCol, 8, 0);
    lv_obj_set_style_bg_opa(leftCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftCol, 0, 0);
    DefaultTheme::disableScroll(leftCol);

    lv_obj_t* radioIcon = lv_label_create(leftCol);
    lv_label_set_text(radioIcon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(radioIcon, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(radioIcon, &lv_font_montserrat_24, 0);

    lv_obj_t* textCol = lv_obj_create(leftCol);
    lv_obj_set_size(textCol, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(textCol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(textCol, 0, 0);
    lv_obj_set_style_pad_row(textCol, 2, 0);
    lv_obj_set_style_bg_opa(textCol, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(textCol, 0, 0);
    DefaultTheme::disableScroll(textCol);

    m_currentNameLbl = lv_label_create(textCol);
    lv_label_set_text(m_currentNameLbl, s_currentStation.name.empty() ? "Selecciona una emisora" : s_currentStation.name.c_str());
    lv_obj_set_style_text_color(m_currentNameLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_currentNameLbl, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(m_currentNameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(m_currentNameLbl, 250);

    m_currentStatusLbl = lv_label_create(textCol);
    lv_label_set_text(m_currentStatusLbl, s_isPlaying ? "Reproduciendo stream..." : "Detenido");
    lv_obj_set_style_text_color(m_currentStatusLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_currentStatusLbl, &lv_font_montserrat_12, 0);

    // Botón Play / Pause circular
    m_playBtn = lv_button_create(m_playerBar);
    lv_obj_set_size(m_playBtn, 44, 44);
    DefaultTheme::applyButton(m_playBtn, 22);
    lv_obj_set_style_bg_color(m_playBtn, s_isPlaying ? lv_color_hex(0xEF4444) : DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_playBtn, playPauseCb, LV_EVENT_CLICKED, this);

    m_playBtnLbl = lv_label_create(m_playBtn);
    lv_label_set_text(m_playBtnLbl, s_isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(m_playBtnLbl, s_isPlaying ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000), 0);
    lv_obj_center(m_playBtnLbl);
}

void RadioView::playStation(const audio::RadioStation& station) {
    // Verificar que la red esté lista antes de intentar streaming
    if (!cbdos::network::isConnected()) {
        UIManager::showToast("Sin conexion WiFi. Conecta primero.");
        return;
    }

    s_currentStation = station;
    s_isPlaying = true;

    if (s_activeInstance) {
        if (s_activeInstance->m_currentNameLbl && lv_obj_is_valid(s_activeInstance->m_currentNameLbl)) {
            lv_label_set_text(s_activeInstance->m_currentNameLbl, station.name.c_str());
        }
        if (s_activeInstance->m_currentStatusLbl && lv_obj_is_valid(s_activeInstance->m_currentStatusLbl)) {
            char stBuf[64];
            snprintf(stBuf, sizeof(stBuf), "Conectando stream (%s)...", station.genre.c_str());
            lv_label_set_text(s_activeInstance->m_currentStatusLbl, stBuf);
        }
        if (s_activeInstance->m_playBtn && lv_obj_is_valid(s_activeInstance->m_playBtn)) {
            lv_obj_set_style_bg_color(s_activeInstance->m_playBtn, lv_color_hex(0xEF4444), 0);
        }
        if (s_activeInstance->m_playBtnLbl && lv_obj_is_valid(s_activeInstance->m_playBtnLbl)) {
            lv_label_set_text(s_activeInstance->m_playBtnLbl, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(s_activeInstance->m_playBtnLbl, lv_color_hex(0xFFFFFF), 0);
        }
    }

    cbdos::audio::playStream(station.url.c_str());
}

void RadioView::stopStream() {
    s_isPlaying = false;
    cbdos::audio::stop();

    if (s_activeInstance) {
        if (s_activeInstance->m_currentStatusLbl && lv_obj_is_valid(s_activeInstance->m_currentStatusLbl)) {
            lv_label_set_text(s_activeInstance->m_currentStatusLbl, "Detenido");
        }
        if (s_activeInstance->m_playBtn && lv_obj_is_valid(s_activeInstance->m_playBtn)) {
            lv_obj_set_style_bg_color(s_activeInstance->m_playBtn, DefaultTheme::getPrimaryAccent(), 0);
        }
        if (s_activeInstance->m_playBtnLbl && lv_obj_is_valid(s_activeInstance->m_playBtnLbl)) {
            lv_label_set_text(s_activeInstance->m_playBtnLbl, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(s_activeInstance->m_playBtnLbl, lv_color_hex(0x000000), 0);
        }
    }
}

void RadioView::playPauseCb(lv_event_t* e) {
    if (s_isPlaying) {
        stopStream();
    } else {
        if (!s_currentStation.url.empty()) {
            playStation(s_currentStation);
        } else {
            const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
            if (!playlists.empty() && !playlists[0].stations.empty()) {
                playStation(playlists[0].stations[0]);
            } else {
                UIManager::showToast("Selecciona una emisora primero");
            }
        }
    }
}
} // namespace ui
} // namespace cbdos
