#pragma once
#include "BaseView.hpp"
#include "../../terminal/SerialStreamAdapter.hpp"
#include "../../terminal/SshStreamAdapter.hpp"
#include "../components/terminal/TerminalDisplay.hpp"
#include "../components/terminal/TerminalCommandBar.hpp"
#include "../components/terminal/SerialControlBar.hpp"
#include "../components/terminal/SshControlBar.hpp"
#include "../modals/SshConnectModal.hpp"
#include <lvgl.h>
#include <memory>
#include <atomic>

namespace cbdos {
namespace ui {

class TerminalView : public BaseView {
public:
    TerminalView();
    ~TerminalView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    void createModeSelector(lv_obj_t* parent);
    void switchMode(cbdos::terminal::StreamType mode);
    void pumpActiveStream();

    static void modeChangedCb(lv_event_t* e);
    static void timerPollCb(lv_timer_t* timer);

    // Componentes Modulares
    TerminalDisplay m_display;
    TerminalCommandBar m_cmdBar;
    SerialControlBar m_serialBar;
    SshControlBar m_sshBar;
    SshConnectModal m_sshModal;

    // Adaptadores de Flujo
    cbdos::terminal::SerialStreamAdapter m_serialStream;
    cbdos::terminal::SshStreamAdapter m_sshStream;
    cbdos::terminal::ITerminalStream* m_activeStream = nullptr;

    // Widgets de Selección de Modo
    lv_obj_t* m_modeRow = nullptr;
    lv_obj_t* m_ddMode = nullptr;
    lv_timer_t* m_pollTimer = nullptr;

    std::atomic<bool> m_portsDirty{false};
    std::string m_lastSshHost;
};
} // namespace ui
} // namespace cbdos
