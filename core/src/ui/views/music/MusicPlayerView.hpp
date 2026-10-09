#pragma once

#include "BaseView.hpp"
#include "../mascot/RobotMascotWidget.hpp"
#include <vector>
#include <string>

namespace cbdos {
namespace ui {

struct TrackItem {
    std::string name;
    std::string path;
};

class MusicPlayerView : public BaseView {
public:
    MusicPlayerView();
    virtual ~MusicPlayerView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void scanAudioFilesSD();
    void renderPlaylist(lv_obj_t* parent);
    void startTrack(int index);
    void showPlayerScreen();
    void showListScreen();
    void updateNavHeaderBtn();

    static void updateTimerCb(lv_timer_t* timer);
    static void trackClickCb(lv_event_t* e);
    static void playPauseCb(lv_event_t* e);
    static void prevTrackCb(lv_event_t* e);
    static void nextTrackCb(lv_event_t* e);

    lv_obj_t* m_listContainer = nullptr;
    lv_obj_t* m_playerCard = nullptr;

    RobotMascotWidget m_mascot;
    lv_obj_t* m_titleLabel = nullptr;
    lv_obj_t* m_statusLabel = nullptr;
    lv_obj_t* m_playBtn = nullptr;
    lv_obj_t* m_playBtnLabel = nullptr;

    lv_timer_t* m_updateTimer = nullptr;

    std::vector<TrackItem> m_playlist;
    int m_currentTrackIndex = -1;
    bool m_isPlaying = false;
};

} // namespace ui
} // namespace cbdos
