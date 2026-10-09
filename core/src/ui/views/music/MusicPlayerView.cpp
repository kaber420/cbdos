#include "MusicPlayerView.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include "cbdos/language.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

namespace cbdos {
namespace ui {

MusicPlayerView::MusicPlayerView()
    : BaseView(cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_TITLE)) {
}

bool MusicPlayerView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    scanAudioFilesSD();
    m_isPlaying = false;
    m_currentTrackIndex = -1;

    // 1. Configurar HeaderBar para esta aplicación (ocultar WiFi y poner botón de navegación)
    UIManager::getInstance().getHeaderBar().showWifi(false);

    // 2. Contenedor principal de la vista (Fondo transparente para ver el Wallpaper)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 10, 0);
    DefaultTheme::disableScroll(m_container);

    // 3. Contenedor principal de la Lista de Canciones SD (con scroll)
    m_listContainer = lv_obj_create(m_container);
    lv_obj_set_size(m_listContainer, LV_PCT(100), LV_PCT(98));
    lv_obj_align(m_listContainer, LV_ALIGN_CENTER, 0, 0);
    DefaultTheme::applyRaisedCard(m_listContainer, 16);
    lv_obj_set_flex_flow(m_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_listContainer, 10, 0);
    lv_obj_set_style_pad_row(m_listContainer, 8, 0);
    lv_obj_add_flag(m_listContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_listContainer, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_listContainer, LV_SCROLLBAR_MODE_ACTIVE);

    renderPlaylist(m_listContainer);

    // 4. Contenedor del Reproductor (inicialmente oculto)
    m_playerCard = lv_obj_create(m_container);
    lv_obj_set_size(m_playerCard, LV_PCT(100), LV_PCT(98));
    lv_obj_align(m_playerCard, LV_ALIGN_CENTER, 0, 0);
    DefaultTheme::applyRaisedCard(m_playerCard, 16);
    lv_obj_set_flex_flow(m_playerCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_playerCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(m_playerCard, 16, 0);
    lv_obj_set_style_pad_row(m_playerCard, 10, 0);
    lv_obj_add_flag(m_playerCard, LV_OBJ_FLAG_HIDDEN);
    DefaultTheme::disableScroll(m_playerCard);

    // Mascota Robot interactiva ("BitBot") en lugar del icono estático
    m_mascot.create(m_playerCard);

    // Título de la canción (con scroll circular)
    m_titleLabel = lv_label_create(m_playerCard);
    lv_label_set_text(m_titleLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_SELECT));
    lv_obj_set_style_text_font(m_titleLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(m_titleLabel, DefaultTheme::getTextColor(), 0);
    lv_label_set_long_mode(m_titleLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(m_titleLabel, LV_PCT(90));
    lv_obj_set_style_text_align(m_titleLabel, LV_TEXT_ALIGN_CENTER, 0);

    // Estado / Formato
    m_statusLabel = lv_label_create(m_playerCard);
    lv_label_set_text(m_statusLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_GREET));
    lv_obj_set_style_text_font(m_statusLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_statusLabel, DefaultTheme::getMutedTextColor(), 0);

    // Fila de Controles [ ◀◀ ] [ ▶ / ❚❚ ] [ ▶▶ ]
    lv_obj_t* ctrlRow = lv_obj_create(m_playerCard);
    lv_obj_set_size(ctrlRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ctrlRow, 0, 0);
    lv_obj_set_style_border_width(ctrlRow, 0, 0);
    lv_obj_set_flex_flow(ctrlRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrlRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrlRow, 18, 0);
    DefaultTheme::disableScroll(ctrlRow);

    // Anterior
    lv_obj_t* prevBtn = lv_button_create(ctrlRow);
    lv_obj_set_size(prevBtn, 50, 50);
    DefaultTheme::applyButton(prevBtn, 25);
    lv_obj_add_event_cb(prevBtn, prevTrackCb, LV_EVENT_CLICKED, this);
    lv_obj_t* prevLbl = lv_label_create(prevBtn);
    lv_label_set_text(prevLbl, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(prevLbl, &lv_font_montserrat_16, 0);
    lv_obj_center(prevLbl);

    // Play/Pause (Central y destacado)
    m_playBtn = lv_button_create(ctrlRow);
    lv_obj_set_size(m_playBtn, 65, 65);
    DefaultTheme::applyButton(m_playBtn, 32);
    lv_obj_set_style_bg_color(m_playBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_bg_opa(m_playBtn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(m_playBtn, playPauseCb, LV_EVENT_CLICKED, this);

    m_playBtnLabel = lv_label_create(m_playBtn);
    lv_label_set_text(m_playBtnLabel, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(m_playBtnLabel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(m_playBtnLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(m_playBtnLabel);

    // Siguiente
    lv_obj_t* nextBtn = lv_button_create(ctrlRow);
    lv_obj_set_size(nextBtn, 50, 50);
    DefaultTheme::applyButton(nextBtn, 25);
    lv_obj_add_event_cb(nextBtn, nextTrackCb, LV_EVENT_CLICKED, this);
    lv_obj_t* nextLbl = lv_label_create(nextBtn);
    lv_label_set_text(nextLbl, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(nextLbl, &lv_font_montserrat_16, 0);
    lv_obj_center(nextLbl);

    updateNavHeaderBtn();

    m_updateTimer = lv_timer_create(updateTimerCb, 300, this);
    return true;
}

void MusicPlayerView::onDestroy() {
    if (m_updateTimer) {
        lv_timer_delete(m_updateTimer);
        m_updateTimer = nullptr;
    }
    m_mascot.destroy();
    UIManager::getInstance().getHeaderBar().clearRightAction();
    UIManager::getInstance().getHeaderBar().showWifi(true);
    BaseView::onDestroy();
}

void MusicPlayerView::scanAudioFilesSD() {
    m_playlist.clear();

    auto checkAndAddFile = [this](const std::string& dir, const std::string& fname) {
        std::string lower = fname;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.ends_with(".mp3") || lower.ends_with(".wav") || lower.ends_with(".aac") || lower.ends_with(".m4a")) {
            std::string fullPath = dir + "/" + fname;
            std::string displayName = fname;
            size_t dotPos = displayName.find_last_of('.');
            if (dotPos != std::string::npos) {
                displayName = displayName.substr(0, dotPos);
            }
            m_playlist.push_back({displayName, fullPath});
            printf("[MusicPlayerView] Cancion agregada: %s\n", displayName.c_str());
        }
    };

    // 1. Escanear directorio raíz de la MicroSD (/sdcard)
    auto rootEntries = cbdos::storage::listDir("/sdcard");
    for (const auto& entry : rootEntries) {
        if (!entry.isDirectory) {
            checkAndAddFile("/sdcard", entry.name);
        } else {
            // 2. Escanear subdirectorios (ej: /sdcard/musica, /sdcard/music, /sdcard/audio)
            std::string subDirPath = std::string("/sdcard/") + entry.name;
            auto subEntries = cbdos::storage::listDir(subDirPath.c_str());
            for (const auto& subEntry : subEntries) {
                if (!subEntry.isDirectory) {
                    checkAndAddFile(subDirPath, subEntry.name);
                }
            }
        }
    }
}

void MusicPlayerView::renderPlaylist(lv_obj_t* parent) {
    if (m_playlist.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(parent);
        lv_label_set_text(emptyLbl, cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_EMPTY));
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
        return;
    }

    for (size_t i = 0; i < m_playlist.size(); i++) {
        lv_obj_t* itemBtn = lv_button_create(parent);
        lv_obj_set_size(itemBtn, LV_PCT(100), 45);
        DefaultTheme::applyButton(itemBtn, 10);
        lv_obj_set_user_data(itemBtn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(itemBtn, trackClickCb, LV_EVENT_CLICKED, this);

        lv_obj_set_flex_flow(itemBtn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(itemBtn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* icon = lv_label_create(itemBtn);
        lv_label_set_text(icon, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

        lv_obj_t* nameLbl = lv_label_create(itemBtn);
        lv_label_set_text(nameLbl, m_playlist[i].name.c_str());
        lv_obj_set_style_text_color(nameLbl, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(nameLbl, LV_PCT(85));
    }
}

void MusicPlayerView::updateNavHeaderBtn() {
    if (!m_playerCard || !lv_obj_is_valid(m_playerCard)) return;

    namespace lang = cbdos::lang;
    if (lv_obj_has_flag(m_playerCard, LV_OBJ_FLAG_HIDDEN)) {
        std::string label = std::string(LV_SYMBOL_AUDIO) + " " + lang::tr(lang::StrId::STR_MUSIC_VIEW_PLAYER);
        UIManager::getInstance().getHeaderBar().setRightAction(label.c_str(), [this]() {
            this->showPlayerScreen();
        });
    } else {
        std::string label = std::string(LV_SYMBOL_LIST) + " " + lang::tr(lang::StrId::STR_MUSIC_VIEW_LIST);
        UIManager::getInstance().getHeaderBar().setRightAction(label.c_str(), [this]() {
            this->showListScreen();
        });
    }
}

void MusicPlayerView::showPlayerScreen() {
    if (m_listContainer && m_playerCard) {
        lv_obj_add_flag(m_listContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m_playerCard, LV_OBJ_FLAG_HIDDEN);
        updateNavHeaderBtn();
    }
}

void MusicPlayerView::showListScreen() {
    if (m_listContainer && m_playerCard) {
        lv_obj_remove_flag(m_listContainer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m_playerCard, LV_OBJ_FLAG_HIDDEN);
        updateNavHeaderBtn();
    }
}

void MusicPlayerView::startTrack(int index) {
    if (index < 0 || index >= (int)m_playlist.size()) return;
    m_currentTrackIndex = index;

    lv_label_set_text(m_titleLabel, m_playlist[index].name.c_str());
    lv_label_set_text(m_statusLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_PLAYING));
    lv_label_set_text(m_playBtnLabel, LV_SYMBOL_PAUSE);

    cbdos::audio::playFile(m_playlist[index].path.c_str());
    m_isPlaying = true;
    m_mascot.setState(MascotState::DANCING);
}

void MusicPlayerView::trackClickCb(lv_event_t* e) {
    MusicPlayerView* self = static_cast<MusicPlayerView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !btn) return;

    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    self->startTrack(idx);
    self->showPlayerScreen();
}

void MusicPlayerView::playPauseCb(lv_event_t* e) {
    MusicPlayerView* self = static_cast<MusicPlayerView*>(lv_event_get_user_data(e));
    if (!self) return;

    auto stats = cbdos::audio::getStats();
    if (stats.isPlaying) {
        cbdos::audio::pause();
        lv_label_set_text(self->m_playBtnLabel, LV_SYMBOL_PLAY);
        lv_label_set_text(self->m_statusLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_PAUSED));
        self->m_mascot.setState(MascotState::IDLE);
    } else {
        if (self->m_currentTrackIndex >= 0) {
            cbdos::audio::resume();
            lv_label_set_text(self->m_playBtnLabel, LV_SYMBOL_PAUSE);
            lv_label_set_text(self->m_statusLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_MUSIC_PLAYING));
            self->m_mascot.setState(MascotState::DANCING);
        } else if (!self->m_playlist.empty()) {
            self->startTrack(0);
        }
    }
}

void MusicPlayerView::prevTrackCb(lv_event_t* e) {
    MusicPlayerView* self = static_cast<MusicPlayerView*>(lv_event_get_user_data(e));
    if (!self || self->m_playlist.empty()) return;
    int prev = (self->m_currentTrackIndex - 1 + self->m_playlist.size()) % self->m_playlist.size();
    self->startTrack(prev);
}

void MusicPlayerView::nextTrackCb(lv_event_t* e) {
    MusicPlayerView* self = static_cast<MusicPlayerView*>(lv_event_get_user_data(e));
    if (!self || self->m_playlist.empty()) return;
    int next = (self->m_currentTrackIndex + 1) % self->m_playlist.size();
    self->startTrack(next);
}

void MusicPlayerView::updateTimerCb(lv_timer_t* timer) {
    MusicPlayerView* self = static_cast<MusicPlayerView*>(lv_timer_get_user_data(timer));
    if (!self) return;

    auto stats = cbdos::audio::getStats();
    if (self->m_playBtnLabel) {
        lv_label_set_text(self->m_playBtnLabel, stats.isPlaying ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }

    if (stats.isPlaying) {
        if (self->m_mascot.getState() == MascotState::IDLE) {
            self->m_mascot.setState(MascotState::DANCING);
        }
    } else {
        if (self->m_mascot.getState() == MascotState::DANCING) {
            self->m_mascot.setState(MascotState::IDLE);
        }
    }
}

void MusicPlayerView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

} // namespace ui
} // namespace cbdos
