#pragma once

#include "BaseView.hpp"
#include "../../audio/RadioManager.hpp"
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

class RadioView : public BaseView {
public:
    RadioView();
    virtual ~RadioView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    static void playStation(const audio::RadioStation& station);
    static void stopStream();

private:
    void buildPlayerBar(lv_obj_t* parent);
    void buildSegmentedNav(lv_obj_t* parent);
    void buildFavoritesView(lv_obj_t* parent);
    void buildExploreView(lv_obj_t* parent);
    void buildAddManualView(lv_obj_t* parent);

    void showTab(int tabIndex);
    void refreshFavoritesUI();
    void refreshExploreUI(const std::vector<audio::RadioStation>& stations);
    void refreshPlaylistDropdown();
    void performSearch();

    // Modales de gestión de listas
    void showPlaylistManageModal();
    void showNewPlaylistModal();
    void confirmNewPlaylist();
    void showImportSdModal();

    static void tabNavCb(lv_event_t* e);
    static void playPauseCb(lv_event_t* e);
    
    static void favPlayCb(lv_event_t* e);
    static void favDeleteCb(lv_event_t* e);
    static void playlistDropdownCb(lv_event_t* e);
    static void playlistManageBtnCb(lv_event_t* e);

    static void modalManageCloseCb(lv_event_t* e);
    static void modalBtnNewCb(lv_event_t* e);
    static void modalBtnExportCb(lv_event_t* e);
    static void modalBtnImportCb(lv_event_t* e);
    static void modalBtnDeleteCb(lv_event_t* e);

    static void modalNewConfirmCb(lv_event_t* e);
    static void modalNewCancelCb(lv_event_t* e);
    static void modalImportFileItemCb(lv_event_t* e);

    static void searchBtnCb(lv_event_t* e);
    static void navPrevCb(lv_event_t* e);
    static void navNextCb(lv_event_t* e);
    static void searchPollTimerCb(lv_timer_t* timer);
    static void asyncSearchTask(void* param);

    static void explorePlayCb(lv_event_t* e);
    static void exploreFavCb(lv_event_t* e);

    static void addSaveCb(lv_event_t* e);

    // Barra de Reproducción Superior
    lv_obj_t* m_playerBar = nullptr;
    lv_obj_t* m_currentNameLbl = nullptr;
    lv_obj_t* m_currentStatusLbl = nullptr;
    lv_obj_t* m_playBtn = nullptr;
    lv_obj_t* m_playBtnLbl = nullptr;

    // Botones de Navegación Segmentada
    lv_obj_t* m_tabBtnFav = nullptr;
    lv_obj_t* m_tabBtnExplore = nullptr;
    lv_obj_t* m_tabBtnAdd = nullptr;

    // 3 Contenedores de Vistas
    lv_obj_t* m_favContainer = nullptr;
    lv_obj_t* m_exploreContainer = nullptr;
    lv_obj_t* m_addContainer = nullptr;

    // Pestaña Playlists / Favoritas
    lv_obj_t* m_playlistDropdown = nullptr;
    lv_obj_t* m_favStationsList = nullptr;
    size_t m_selectedPlaylistIdx = 0;
    lv_obj_t* m_modalMask = nullptr;
    lv_obj_t* m_newPlaylistTa = nullptr;
    std::vector<std::string> m_importableFiles;

    // Pestaña Explorar
    lv_obj_t* m_taSearch = nullptr;
    lv_obj_t* m_btnSearch = nullptr;
    lv_obj_t* m_btnPrevPage = nullptr;
    lv_obj_t* m_btnNextPage = nullptr;
    lv_obj_t* m_pageLbl = nullptr;
    lv_obj_t* m_exploreList = nullptr;

    // Pestaña Agregar Manual
    lv_obj_t* m_taName = nullptr;
    lv_obj_t* m_taUrl = nullptr;
    lv_obj_t* m_taGenre = nullptr;

    // Timer de sondeo de estado
    lv_timer_t* m_searchPollTimer = nullptr;

    static RadioView* s_activeInstance;
    static audio::RadioStation s_currentStation;
    static bool s_isPlaying;
    static size_t s_currentSearchOffset;
    static std::string s_currentSearchQuery;
    static std::vector<audio::RadioStation> s_currentExploreStations;
    static volatile bool s_searchInProgress;
    static volatile bool s_searchCompleted;
    static std::vector<audio::RadioStation> s_asyncSearchResults;
};

} // namespace ui
} // namespace cbdos
