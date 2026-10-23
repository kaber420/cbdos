#pragma once
#include <lvgl.h>
#include <string>
#include <functional>

namespace cbdos {
namespace ui {

class SshControlBar {
public:
    using ActionCallback = std::function<void()>;
    using ConnectToggleCallback = std::function<void(bool doConnect)>;

    SshControlBar() = default;
    ~SshControlBar() = default;

    bool create(lv_obj_t* parent,
                ActionCallback onOpenModal,
                ConnectToggleCallback onToggleConnect,
                ActionCallback onHold,
                ActionCallback onClear,
                ActionCallback onSave);

    void setConnected(bool connected, const std::string& hostInfo = "");
    void setHoldActive(bool active);
    void show(bool visible);
    bool isConnected() const { return m_isConnected; }

private:
    static void hostConfigBtnCb(lv_event_t* e);
    static void holdBtnCb(lv_event_t* e);
    static void clearBtnCb(lv_event_t* e);
    static void saveBtnCb(lv_event_t* e);

    lv_obj_t* m_toolbar = nullptr;
    lv_obj_t* m_lblHost = nullptr;
    lv_obj_t* m_btnConfig = nullptr;
    lv_obj_t* m_lblConfig = nullptr;
    lv_obj_t* m_btnHold = nullptr;
    lv_obj_t* m_lblHold = nullptr;
    lv_obj_t* m_btnClear = nullptr;
    lv_obj_t* m_btnSave = nullptr;

    bool m_isConnected = false;
    bool m_isHoldActive = false;

    ActionCallback m_onOpenModal;
    ConnectToggleCallback m_onToggleConnect;
    ActionCallback m_onHold;
    ActionCallback m_onClear;
    ActionCallback m_onSave;
};

} // namespace ui
} // namespace cbdos
