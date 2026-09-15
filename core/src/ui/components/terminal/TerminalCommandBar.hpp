#pragma once
#include <lvgl.h>
#include <string>
#include <functional>
#include "cbdos/serial.hpp"

namespace cbdos {
namespace ui {

class TerminalCommandBar {
public:
    using SendCommandCallback = std::function<void(const std::string& cmd, cbdos::serial::LineEnding ending, bool localEcho)>;
    using SendRawCallback = std::function<void(const char* bytes, size_t len)>;

    TerminalCommandBar() = default;
    ~TerminalCommandBar() = default;

    bool create(lv_obj_t* parent, SendCommandCallback onSendCmd, SendRawCallback onSendRaw);
    void executeCommand();
    cbdos::serial::LineEnding getLineEnding() const { return m_lineEnding; }
    bool isLocalEcho() const { return m_localEcho; }

private:
    static void lineEndingChangedCb(lv_event_t* e);
    static void echoToggleBtnCb(lv_event_t* e);
    static void sendBtnCb(lv_event_t* e);
    static void kbToggleBtnCb(lv_event_t* e);
    static void quickKeyBtnCb(lv_event_t* e);

    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_ddLineEnding = nullptr;
    lv_obj_t* m_btnEcho = nullptr;
    lv_obj_t* m_lblEcho = nullptr;
    lv_obj_t* m_inputCmd = nullptr;
    lv_obj_t* m_btnSend = nullptr;
    lv_obj_t* m_btnToggleKb = nullptr;

    bool m_localEcho = false;
    cbdos::serial::LineEnding m_lineEnding = cbdos::serial::LineEnding::CRLF;

    SendCommandCallback m_onSendCmd;
    SendRawCallback m_onSendRaw;
};

} // namespace ui
} // namespace cbdos
