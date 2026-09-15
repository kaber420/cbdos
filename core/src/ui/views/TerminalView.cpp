#include "TerminalView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace ui {

TerminalView::TerminalView()
    : BaseView("Terminal"),
      m_activeStream(&m_serialStream) {
}

TerminalView::~TerminalView() {
    onDestroy();
}

bool TerminalView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 4, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_container, 4, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_OFF);

    createModeSelector(m_container);

    // Barra Serial / USB
    m_serialBar.create(m_container,
        [this](bool doConnect, const cbdos::serial::SerialConfig& cfg) {
            if (doConnect) {
                bool ok = m_serialStream.open(cfg);
                m_serialBar.setConnected(ok);
                std::string msg = ok ? "\n[SYS] Conectado en '" + cfg.portId + "' @ " + std::to_string(cfg.baudrate) + " bps.\n"
                                     : "\n[ERR] Fallo al abrir '" + cfg.portId + "'.\n";
                m_display.appendText(msg.c_str(), msg.size());
            } else {
                m_serialStream.close();
                m_serialBar.setConnected(false);
                std::string msg = "\n[SYS] Puerto cerrado. Hardware liberado.\n";
                m_display.appendText(msg.c_str(), msg.size());
            }
        },
        [this](bool enterBootloader) {
            m_serialStream.pulseControlPin(100, enterBootloader);
            std::string msg = enterBootloader ? "\n[📥 DFU] Reinicio a Bootloader enviado.\n"
                                              : "\n[⚡ RST] Reinicio normal enviado (Run Mode).\n";
            m_display.appendText(msg.c_str(), msg.size());
        },
        [this]() {
            bool hold = m_display.toggleHold();
            m_serialBar.setHoldActive(hold);
        },
        [this]() { m_display.clear(); },
        [this]() {
            std::string logMsg;
            m_display.saveLogToSd(logMsg);
            m_display.appendText(logMsg.c_str(), logMsg.size());
        },
        [this](const std::string& msg) { m_display.appendText(msg.c_str(), msg.size()); }
    );

    // Barra SSH Remoto (oculta inicialmente)
    m_sshBar.create(m_container,
        [this]() {
            m_sshModal.show(m_container, [this](const cbdos::ssh::SshConfig& cfg) {
                m_lastSshHost = cfg.username + "@" + cfg.host + ":" + std::to_string(cfg.port);
                std::string connMsg = "\n[SSH] Conectando a " + m_lastSshHost + "...\n";
                m_display.appendText(connMsg.c_str(), connMsg.size());
                bool ok = m_sshStream.connect(cfg);
                m_sshBar.setConnected(ok, m_lastSshHost);
                std::string resMsg = ok ? "\n[SSH] Sesión interactiva establecida.\n"
                                        : "\n[ERR] Fallo al iniciar sesión SSH.\n";
                m_display.appendText(resMsg.c_str(), resMsg.size());
            });
        },
        [this](bool doConnect) {
            if (!doConnect) {
                m_sshStream.close();
                m_sshBar.setConnected(false);
                std::string msg = "\n[SSH] Sesión cerrada por el usuario.\n";
                m_display.appendText(msg.c_str(), msg.size());
            } else {
                m_sshModal.show(m_container, [this](const cbdos::ssh::SshConfig& cfg) {
                    m_lastSshHost = cfg.username + "@" + cfg.host + ":" + std::to_string(cfg.port);
                    bool ok = m_sshStream.connect(cfg);
                    m_sshBar.setConnected(ok, m_lastSshHost);
                });
            }
        },
        [this]() {
            bool hold = m_display.toggleHold();
            m_sshBar.setHoldActive(hold);
        },
        [this]() { m_display.clear(); },
        [this]() {
            std::string logMsg;
            m_display.saveLogToSd(logMsg);
            m_display.appendText(logMsg.c_str(), logMsg.size());
        }
    );
    m_sshBar.show(false);

    // Área de Texto
    m_display.create(m_container);
    if (m_display.getObject()) {
        lv_obj_add_event_cb(m_display.getObject(), [](lv_event_t* e) {
            auto* self = static_cast<TerminalView*>(lv_event_get_user_data(e));
            if (self) UIManager::closeKeyboard();
        }, LV_EVENT_CLICKED, this);
    }

    // Barra de Comandos y Protocolo
    m_cmdBar.create(m_container,
        [this](const std::string& cmd, cbdos::serial::LineEnding ending, bool localEcho) {
            if (!m_activeStream || !m_activeStream->isConnected()) {
                std::string warn = "\n[WARN] Transporte desconectado. Pulsa Abrir primero.\n";
                m_display.appendText(warn.c_str(), warn.size());
                return;
            }
            std::string payload = cmd;
            switch (ending) {
                case cbdos::serial::LineEnding::CRLF: payload += "\r\n"; break;
                case cbdos::serial::LineEnding::LF:   payload += "\n";   break;
                case cbdos::serial::LineEnding::CR:   payload += "\r";   break;
                case cbdos::serial::LineEnding::NONE:                    break;
            }
            m_activeStream->write(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
            if (localEcho) {
                std::string echo = "\n[TX] > " + cmd + "\n";
                m_display.appendText(echo.c_str(), echo.size());
            }
        },
        [this](const char* bytes, size_t len) {
            if (m_activeStream && m_activeStream->isConnected()) {
                m_activeStream->write(reinterpret_cast<const uint8_t*>(bytes), len);
            }
        }
    );

    // Detección hotplug USB por hardware
    cbdos::serial::setHotplugCallback([this](bool connected, const std::string& portId) {
        (void)connected;
        (void)portId;
        m_portsDirty = true;
    });

    m_pollTimer = lv_timer_create(timerPollCb, 30, this);
    return true;
}

void TerminalView::onDestroy() {
    cbdos::serial::setHotplugCallback(nullptr);
    if (m_pollTimer) {
        lv_timer_delete(m_pollTimer);
        m_pollTimer = nullptr;
    }
    m_serialStream.close();
    m_sshStream.close();
    m_sshModal.close();
    BaseView::onDestroy();
}

void TerminalView::onShow() {
    BaseView::onShow();
    HeaderBar& hb = UIManager::getInstance().getHeaderBar();
    hb.setTitle("Terminal Universal");
    hb.showWifi(m_activeStream == &m_sshStream);
    if (m_pollTimer) lv_timer_resume(m_pollTimer);
}

void TerminalView::onHide() {
    if (m_pollTimer) lv_timer_pause(m_pollTimer);
    BaseView::onHide();
}

void TerminalView::createModeSelector(lv_obj_t* parent) {
    m_modeRow = lv_obj_create(parent);
    lv_obj_set_size(m_modeRow, LV_PCT(100), 32);
    lv_obj_set_style_bg_opa(m_modeRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_modeRow, 0, 0);
    lv_obj_set_style_pad_all(m_modeRow, 0, 0);
    lv_obj_set_flex_flow(m_modeRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_modeRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(m_modeRow, 6, 0);
    DefaultTheme::disableScroll(m_modeRow);

    lv_obj_t* lbl = lv_label_create(m_modeRow);
    lv_label_set_text(lbl, "Modo:");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    m_ddMode = lv_dropdown_create(m_modeRow);
    lv_dropdown_set_options(m_ddMode, "UART / USB Serie\nSSH Remoto (Wi-Fi)");
    lv_dropdown_set_selected(m_ddMode, 0);
    lv_obj_set_width(m_ddMode, 175);
    lv_obj_set_style_text_font(m_ddMode, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddMode, modeChangedCb, LV_EVENT_VALUE_CHANGED, this);
}

void TerminalView::switchMode(cbdos::terminal::StreamType mode) {
    HeaderBar& hb = UIManager::getInstance().getHeaderBar();
    if (mode == cbdos::terminal::StreamType::Serial) {
        m_activeStream = &m_serialStream;
        m_sshBar.show(false);
        m_serialBar.show(true);
        hb.showWifi(false);
    } else {
        m_activeStream = &m_sshStream;
        m_serialBar.show(false);
        m_sshBar.show(true);
        hb.showWifi(true);
    }
}

void TerminalView::modeChangedCb(lv_event_t* e) {
    auto* self = static_cast<TerminalView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddMode) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddMode);
    self->switchMode(sel == 0 ? cbdos::terminal::StreamType::Serial : cbdos::terminal::StreamType::Ssh);
}

void TerminalView::pumpActiveStream() {
    if (!m_activeStream) return;
    size_t avail = m_activeStream->available();
    if (avail > 0) {
        uint8_t buffer[256];
        size_t toRead = std::min(avail, sizeof(buffer) - 1);
        size_t readBytes = m_activeStream->read(buffer, toRead);
        if (readBytes > 0) {
            buffer[readBytes] = '\0';
            m_display.appendText(reinterpret_cast<const char*>(buffer), readBytes);
        }
    }
}

void TerminalView::timerPollCb(lv_timer_t* timer) {
    auto* self = static_cast<TerminalView*>(lv_timer_get_user_data(timer));
    if (!self) return;

    if (self->m_portsDirty.exchange(false)) {
        self->m_serialBar.updatePortList();
        if (self->m_serialStream.isConnected() && !cbdos::serial::isOpen()) {
            self->m_serialBar.setConnected(false);
            std::string msg = "\n[SYS] Dispositivo USB desconectado físicamente.\n";
            self->m_display.appendText(msg.c_str(), msg.size());
        }
    }

    self->pumpActiveStream();
}

void TerminalView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    }
}

} // namespace ui
} // namespace cbdos
