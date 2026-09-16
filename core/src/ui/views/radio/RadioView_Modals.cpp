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

void RadioView::showPlaylistManageModal() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width > 0 ? caps.width : 480;
    int32_t screenH = caps.height > 0 ? caps.height : 800;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    int32_t modalW = (screenW >= 480) ? 380 : 280;
    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_size(modal, modalW, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Título
    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Gestion de Listas de Radio");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_center(title);

    auto createActionBtn = [&](const char* icon, const char* text, lv_event_cb_t cb, uint32_t color = 0) -> lv_obj_t* {
        lv_obj_t* btn = lv_button_create(modal);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        DefaultTheme::applyButton(btn, 8);
        if (color != 0) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
        }
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(btn, 12, 0);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);

        lv_obj_t* ic = lv_label_create(btn);
        lv_label_set_text(ic, icon);
        lv_obj_set_style_text_color(ic, DefaultTheme::getPrimaryAccent(), 0);

        lv_obj_t* lb = lv_label_create(btn);
        lv_label_set_text(lb, text);
        lv_obj_set_style_text_color(lb, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_left(lb, 8, 0);
        return btn;
    };

    createActionBtn(LV_SYMBOL_PLUS, "Crear Nueva Lista", modalBtnNewCb);
    createActionBtn(LV_SYMBOL_SAVE, "Exportar Lista a MicroSD", modalBtnExportCb);
    createActionBtn(LV_SYMBOL_DIRECTORY, "Importar Lista desde MicroSD", modalBtnImportCb);
    createActionBtn(LV_SYMBOL_TRASH, "Eliminar Lista Actual", modalBtnDeleteCb, 0x3B1D22);
    createActionBtn(LV_SYMBOL_CLOSE, "Cerrar", modalManageCloseCb);
}

void RadioView::showNewPlaylistModal() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width > 0 ? caps.width : 480;
    int32_t screenH = caps.height > 0 ? caps.height : 800;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    int32_t modalW = (screenW >= 480) ? 380 : 280;
    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_size(modal, modalW, 160);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_set_align(modal, LV_ALIGN_TOP_MID);
    lv_obj_set_style_margin_top(modal, (screenH >= 800) ? 40 : 16, 0);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 10, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "Nombre de la Nueva Lista:");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);

    m_newPlaylistTa = lv_textarea_create(modal);
    lv_obj_set_size(m_newPlaylistTa, LV_PCT(100), 38);
    DefaultTheme::applySunkenCard(m_newPlaylistTa, 8);
    lv_textarea_set_placeholder_text(m_newPlaylistTa, "Ej. Synthwave / Noticias");
    lv_textarea_set_one_line(m_newPlaylistTa, true);
    lv_obj_set_style_text_font(m_newPlaylistTa, &lv_font_montserrat_12, 0);

    lv_obj_t* btnRow = lv_obj_create(modal);
    lv_obj_set_size(btnRow, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnRow, 0, 0);

    int32_t btnW = (modalW / 2) - 8;

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_size(btnCancel, btnW, 34);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, modalNewCancelCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnCreate = lv_button_create(btnRow);
    lv_obj_set_size(btnCreate, btnW, 34);
    DefaultTheme::applyButton(btnCreate, 8);
    lv_obj_set_style_bg_color(btnCreate, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_t* lblCr = lv_label_create(btnCreate);
    lv_label_set_text(lblCr, "Crear");
    lv_obj_set_style_text_color(lblCr, lv_color_hex(0x000000), 0);
    lv_obj_center(lblCr);
    lv_obj_add_event_cb(btnCreate, modalNewConfirmCb, LV_EVENT_CLICKED, this);

    // Teclado virtual unificado
    UIManager::attachKeyboard(m_newPlaylistTa, {
        .autoCloseOnSubmit = true,
        .onSubmit = [this](const char*) {
            this->confirmNewPlaylist();
        }
    });
    UIManager::openKeyboard(m_newPlaylistTa);
}

void RadioView::confirmNewPlaylist() {
    if (!m_newPlaylistTa || !lv_obj_is_valid(m_newPlaylistTa)) return;

    const char* txt = lv_textarea_get_text(m_newPlaylistTa);
    if (txt && strlen(txt) > 0) {
        bool ok = audio::RadioManager::getInstance().createPlaylist(txt);
        if (ok) {
            const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
            m_selectedPlaylistIdx = playlists.size() - 1;
            refreshPlaylistDropdown();
            refreshFavoritesUI();
            UIManager::showToast("Lista creada con exito");
        } else {
            UIManager::showToast("Error al crear lista");
        }
    }

    UIManager::closeKeyboard();
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete_async(m_modalMask);
        m_modalMask = nullptr;
    }
    m_newPlaylistTa = nullptr;
}

void RadioView::showImportSdModal() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    if (!cbdos::storage::isSdMounted()) {
        UIManager::showToast("MicroSD no detectada o no montada");
        return;
    }

    m_importableFiles = audio::RadioManager::getInstance().scanPlaylistsOnSd();

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width > 0 ? caps.width : 480;
    int32_t screenH = caps.height > 0 ? caps.height : 800;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    int32_t modalW = (screenW >= 480) ? 400 : 290;
    int32_t modalH = (screenH >= 800) ? 440 : 340;

    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_size(modal, modalW, modalH);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 10, 0);
    lv_obj_set_style_pad_row(modal, 6, 0);

    // Header con Cerrar
    lv_obj_t* header = lv_obj_create(modal);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Importar Listas desde SD");
    lv_obj_set_style_text_color(title, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);

    lv_obj_t* btnClose = lv_button_create(header);
    lv_obj_set_size(btnClose, 30, 28);
    DefaultTheme::applyButton(btnClose, 6);
    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_center(lblX);
    lv_obj_add_event_cb(btnClose, modalManageCloseCb, LV_EVENT_CLICKED, this);

    // Lista de archivos
    lv_obj_t* list = lv_obj_create(modal);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(82));
    DefaultTheme::applySunkenCard(list, 8);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_pad_row(list, 4, 0);

    if (m_importableFiles.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(list);
        lv_label_set_text(emptyLbl, "No se encontraron listas (.msgpack o .m3u)\nen la tarjeta MicroSD.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(emptyLbl);
    } else {
        for (size_t i = 0; i < m_importableFiles.size(); i++) {
            lv_obj_t* item = lv_button_create(list);
            lv_obj_set_size(item, LV_PCT(100), 38);
            DefaultTheme::applyButton(item, 6);
            lv_obj_set_user_data(item, (void*)(intptr_t)i);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(item, 4, 0);

            lv_obj_t* icon = lv_label_create(item);
            lv_label_set_text(icon, LV_SYMBOL_AUDIO);
            lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

            lv_obj_t* name = lv_label_create(item);
            lv_label_set_text(name, m_importableFiles[i].c_str());
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_flex_grow(name, 1);
            lv_obj_set_style_text_color(name, DefaultTheme::getTextColor(), 0);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
            lv_obj_set_style_margin_left(name, 6, 0);

            lv_obj_add_event_cb(item, modalImportFileItemCb, LV_EVENT_CLICKED, this);
        }
    }
}

void RadioView::modalManageCloseCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (self && self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
}

void RadioView::modalBtnNewCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (self) {
        self->showNewPlaylistModal();
    }
}

void RadioView::modalBtnExportCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (!cbdos::storage::isSdMounted()) {
        UIManager::showToast("MicroSD no detectada o no montada");
        return;
    }

    const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
    if (self->m_selectedPlaylistIdx < playlists.size()) {
        const auto& pl = playlists[self->m_selectedPlaylistIdx];
        std::string sdPath = "/sdcard/audio/playlists/" + pl.name + ".msgpack";
        bool ok = audio::RadioManager::getInstance().exportPlaylistToSd(pl.id, sdPath);
        if (ok) {
            UIManager::showToast("Lista exportada a MicroSD");
        } else {
            UIManager::showToast("Error al exportar lista");
        }
    }

    if (self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
}

void RadioView::modalBtnImportCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (self) {
        self->showImportSdModal();
    }
}

void RadioView::modalBtnDeleteCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (!self) return;

    const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
    if (self->m_selectedPlaylistIdx < playlists.size()) {
        const auto& pl = playlists[self->m_selectedPlaylistIdx];
        if (pl.isDefault) {
            UIManager::showToast("No se puede eliminar la lista protegida");
            return;
        }

        audio::RadioManager::getInstance().deletePlaylist(pl.id);
        self->m_selectedPlaylistIdx = 0;
        self->refreshPlaylistDropdown();
        self->refreshFavoritesUI();
        UIManager::showToast("Lista eliminada");
    }

    if (self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
}

void RadioView::modalNewConfirmCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    if (self) self->confirmNewPlaylist();
}

void RadioView::modalNewCancelCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    UIManager::closeKeyboard();
    if (self && self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
        self->m_newPlaylistTa = nullptr;
    }
}

void RadioView::modalImportFileItemCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    if (!self || !btn) return;

    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (idx >= 0 && idx < (int)self->m_importableFiles.size()) {
        std::string path = self->m_importableFiles[idx];
        bool ok = false;
        if (path.rfind(".m3u") != std::string::npos || path.rfind(".M3U") != std::string::npos) {
            ok = audio::RadioManager::getInstance().importM3uFromSd(path);
        } else {
            ok = audio::RadioManager::getInstance().importPlaylistFromSd(path);
        }

        if (ok) {
            const auto& playlists = audio::RadioManager::getInstance().getPlaylists();
            self->m_selectedPlaylistIdx = playlists.size() - 1;
            self->refreshPlaylistDropdown();
            self->refreshFavoritesUI();
            UIManager::showToast("Lista importada con exito");
        } else {
            UIManager::showToast("Error al importar lista");
        }
    }

    if (self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
    }
}
} // namespace ui
} // namespace cbdos
