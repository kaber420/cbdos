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

void RadioView::buildAddManualView(lv_obj_t* parent) {
    m_addContainer = lv_obj_create(parent);
    lv_obj_set_width(m_addContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_addContainer, 1);
    DefaultTheme::applyRaisedCard(m_addContainer, 12);
    lv_obj_set_flex_flow(m_addContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_addContainer, 10, 0);
    lv_obj_set_style_pad_row(m_addContainer, 8, 0);

    lv_obj_t* nameHeader = lv_label_create(m_addContainer);
    lv_label_set_text(nameHeader, "Nombre de la Emisora:");
    lv_obj_set_style_text_color(nameHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(nameHeader, &lv_font_montserrat_12, 0);

    m_taName = lv_textarea_create(m_addContainer);
    lv_obj_set_width(m_taName, lv_pct(100));
    lv_obj_set_height(m_taName, 40);
    DefaultTheme::applySunkenCard(m_taName, 8);
    lv_textarea_set_placeholder_text(m_taName, "Ej. Mi Radio Chill");
    lv_textarea_set_one_line(m_taName, true);
    lv_obj_set_style_text_color(m_taName, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taName);

    lv_obj_t* urlHeader = lv_label_create(m_addContainer);
    lv_label_set_text(urlHeader, "URL Stream MP3 (http://...):");
    lv_obj_set_style_text_color(urlHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(urlHeader, &lv_font_montserrat_12, 0);

    m_taUrl = lv_textarea_create(m_addContainer);
    lv_obj_set_width(m_taUrl, lv_pct(100));
    lv_obj_set_height(m_taUrl, 40);
    DefaultTheme::applySunkenCard(m_taUrl, 8);
    lv_textarea_set_text(m_taUrl, "http://");
    lv_textarea_set_one_line(m_taUrl, true);
    lv_obj_set_style_text_color(m_taUrl, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taUrl);

    lv_obj_t* genreHeader = lv_label_create(m_addContainer);
    lv_label_set_text(genreHeader, "Genero / Categoria:");
    lv_obj_set_style_text_color(genreHeader, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(genreHeader, &lv_font_montserrat_12, 0);

    m_taGenre = lv_textarea_create(m_addContainer);
    lv_obj_set_width(m_taGenre, lv_pct(100));
    lv_obj_set_height(m_taGenre, 40);
    DefaultTheme::applySunkenCard(m_taGenre, 8);
    lv_textarea_set_placeholder_text(m_taGenre, "Ej. Electronic / Jazz");
    lv_textarea_set_one_line(m_taGenre, true);
    lv_obj_set_style_text_color(m_taGenre, DefaultTheme::getTextColor(), 0);
    UIManager::attachKeyboard(m_taGenre);

    lv_obj_t* saveBtn = lv_button_create(m_addContainer);
    lv_obj_set_width(saveBtn, lv_pct(100));
    lv_obj_set_height(saveBtn, 44);
    DefaultTheme::applyButton(saveBtn, 10);
    lv_obj_set_style_bg_color(saveBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(saveBtn, addSaveCb, LV_EVENT_CLICKED, this);

    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "Guardar en Favoritas");
    lv_obj_set_style_text_color(saveLbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(saveLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(saveLbl);
}

void RadioView::addSaveCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taName || !self->m_taUrl) return;

    const char* name = lv_textarea_get_text(self->m_taName);
    const char* url = lv_textarea_get_text(self->m_taUrl);
    const char* genre = self->m_taGenre ? lv_textarea_get_text(self->m_taGenre) : "Web";

    if (name && strlen(name) > 0 && url && strlen(url) > 5) {
        audio::RadioStation st;
        st.name = name;
        st.url = url;
        st.genre = (genre && strlen(genre) > 0) ? genre : "Personalizada";
        st.country = "Manual";
        st.bitrate = 128;
        st.isFavorite = true;

        const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
        std::string targetId = "fav";
        if (self->m_selectedPlaylistIdx < playlists.size()) {
            targetId = playlists[self->m_selectedPlaylistIdx].id;
        }

        audio::RadioManager::getInstance().addStationToPlaylist(targetId, st);
        UIManager::showToast("Emisora agregada a la lista");

        lv_textarea_set_text(self->m_taName, "");
        lv_textarea_set_text(self->m_taUrl, "http://");
        if (self->m_taGenre) lv_textarea_set_text(self->m_taGenre, "");

        self->showTab(0);
    } else {
        UIManager::showToast("Completa los datos requeridos");
    }
}
} // namespace ui
} // namespace cbdos
