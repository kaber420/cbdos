#include "RadioView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
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

RadioView* RadioView::s_activeInstance = nullptr;
audio::RadioStation RadioView::s_currentStation;
bool RadioView::s_isPlaying = false;
size_t RadioView::s_currentSearchOffset = 0;
std::string RadioView::s_currentSearchQuery = "";
std::vector<audio::RadioStation> RadioView::s_currentExploreStations;
volatile bool RadioView::s_searchInProgress = false;
volatile bool RadioView::s_searchCompleted = false;
std::vector<audio::RadioStation> RadioView::s_asyncSearchResults;

RadioView::RadioView()
    : BaseView("Radio Online") {
    s_activeInstance = this;
}

bool RadioView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Inicializar subsistema de radio y favoritos
    audio::RadioManager::getInstance().init();

    // Mostrar icono de WiFi en la barra superior
    UIManager::getInstance().getHeaderBar().showWifi(true);

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    DefaultTheme::disableScroll(m_container);

    // 1. Barra de Reproducción Superior
    buildPlayerBar(m_container);

    // 2. Selector Segmentado de Pestañas (Favoritas / Explorar / Agregar)
    buildSegmentedNav(m_container);

    // 3. Contenedor de Favoritas
    buildFavoritesView(m_container);

    // 4. Contenedor de Explorar (Online Search)
    buildExploreView(m_container);

    // 5. Contenedor de Agregar Manualmente
    buildAddManualView(m_container);

    // Iniciar en la pestaña Favoritas
    showTab(0);

    return true;
}

void RadioView::onDestroy() {
    if (m_searchPollTimer) {
        lv_timer_delete(m_searchPollTimer);
        m_searchPollTimer = nullptr;
    }
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }
    m_newPlaylistTa = nullptr;
    s_activeInstance = nullptr;
    BaseView::onDestroy();
}

void RadioView::buildSegmentedNav(lv_obj_t* parent) {
    lv_obj_t* navRow = lv_obj_create(parent);
    lv_obj_set_width(navRow, lv_pct(100));
    lv_obj_set_height(navRow, 40);
    DefaultTheme::applySunkenCard(navRow, 10);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(navRow, 2, 0);
    DefaultTheme::disableScroll(navRow);

    auto createNavBtn = [&](const char* title, int id) -> lv_obj_t* {
        lv_obj_t* btn = lv_button_create(navRow);
        lv_obj_set_size(btn, lv_pct(32), 34);
        DefaultTheme::applyButton(btn, 8);
        lv_obj_set_user_data(btn, (void*)(intptr_t)id);
        lv_obj_add_event_cb(btn, tabNavCb, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        return btn;
    };

    m_tabBtnFav = createNavBtn("Listas", 0);
    m_tabBtnExplore = createNavBtn("Explorar", 1);
    m_tabBtnAdd = createNavBtn("Agregar", 2);
}

void RadioView::showTab(int tabIndex) {
    if (m_favContainer) lv_obj_add_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
    if (m_exploreContainer) lv_obj_add_flag(m_exploreContainer, LV_OBJ_FLAG_HIDDEN);
    if (m_addContainer) lv_obj_add_flag(m_addContainer, LV_OBJ_FLAG_HIDDEN);

    auto updateBtnStyle = [](lv_obj_t* btn, bool active) {
        if (!btn) return;
        lv_obj_set_style_bg_color(btn, active ? DefaultTheme::getPrimaryAccent() : lv_color_hex(0x1F2430), 0);
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, active ? lv_color_hex(0x000000) : DefaultTheme::getMutedTextColor(), 0);
        }
    };

    updateBtnStyle(m_tabBtnFav, tabIndex == 0);
    updateBtnStyle(m_tabBtnExplore, tabIndex == 1);
    updateBtnStyle(m_tabBtnAdd, tabIndex == 2);

    if (tabIndex == 0) {
        if (m_favContainer) {
            lv_obj_remove_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
            refreshFavoritesUI();
        }
    } else if (tabIndex == 1) {
        if (m_favContainer) lv_obj_add_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_exploreContainer) lv_obj_remove_flag(m_exploreContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_addContainer) lv_obj_add_flag(m_addContainer, LV_OBJ_FLAG_HIDDEN);
        if (s_currentExploreStations.empty()) {
            refreshExploreUI(s_currentExploreStations);
        }
    } else if (tabIndex == 2) {
        if (m_favContainer) lv_obj_add_flag(m_favContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_exploreContainer) lv_obj_add_flag(m_exploreContainer, LV_OBJ_FLAG_HIDDEN);
        if (m_addContainer) lv_obj_remove_flag(m_addContainer, LV_OBJ_FLAG_HIDDEN);
    }
}

void RadioView::tabNavCb(lv_event_t* e) {
    RadioView* self = static_cast<RadioView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_current_target(e);
    int tabId = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (self) self->showTab(tabId);
}

// ── Modales de Gestión de Listas de Reproducción ──────────────────
void RadioView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

} // namespace ui
} // namespace cbdos
