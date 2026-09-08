#pragma once

#include "BaseView.hpp"
#include "cbdos/meshcore/meshcore_client.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

// Vista del Companion Protocol oficial de MeshCore:
// Chats por canal (0-7), gestión de canales y telemetría del dongle.
class MeshCoreView : public BaseView {
public:
    MeshCoreView();
    ~MeshCoreView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    // Pestañas
    void buildChatsTab(lv_obj_t* tab);
    void buildChannelsTab(lv_obj_t* tab);
    void buildRadioTab(lv_obj_t* tab);

    // Actualizaciones reactivas de UI
    void refreshChatLog();
    void refreshChannelsList();
    void refreshRadioStatus();

    // Acciones
    void sendMessage();
    void setActiveChannel(uint8_t idx);
    void updateChannelLabel();

    // Callbacks estáticos LVGL
    static void timerPumpCb(lv_timer_t* timer);
    static void sendBtnCb(lv_event_t* e);
    static void taInputEventCb(lv_event_t* e);
    static void toggleKbBtnCb(lv_event_t* e);
    static void kbEventCb(lv_event_t* e);
    static void channelDropdownCb(lv_event_t* e);
    static void refreshChannelsBtnCb(lv_event_t* e);
    static void channelUseBtnCb(lv_event_t* e);
    static void channelCreateBtnCb(lv_event_t* e);
    static void channelClearBtnCb(lv_event_t* e);
    static void newIdxDropdownCb(lv_event_t* e);
    static void connectBtnCb(lv_event_t* e);
    static void infoBtnCb(lv_event_t* e);
    static void batteryBtnCb(lv_event_t* e);
    static void pollBtnCb(lv_event_t* e);
    static void aliasApplyBtnCb(lv_event_t* e);
    static void rebootBtnCb(lv_event_t* e);
    static void portDropdownCb(lv_event_t* e);

    lv_obj_t* m_tabview = nullptr;
    lv_timer_t* m_pumpTimer = nullptr;

    // Tab 1: Chats por canal
    lv_obj_t* m_lblChannel = nullptr;
    lv_obj_t* m_ddChannel = nullptr;
    lv_obj_t* m_chatContainer = nullptr;
    lv_obj_t* m_taInput = nullptr;
    lv_obj_t* m_btnSend = nullptr;
    lv_obj_t* m_btnKb = nullptr;
    lv_obj_t* m_keyboard = nullptr;
    bool m_keyboardVisible = false;

    // Tab 2: Canales
    lv_obj_t* m_lblChannelsCount = nullptr;
    lv_obj_t* m_channelsContainer = nullptr;
    lv_obj_t* m_ddNewIdx = nullptr;
    lv_obj_t* m_taNewName = nullptr;
    lv_obj_t* m_taNewSecret = nullptr;

    // Tab 3: Radio
    lv_obj_t* m_ddPort = nullptr;
    lv_obj_t* m_btnConnect = nullptr;
    lv_obj_t* m_lblConnStatus = nullptr;
    lv_obj_t* m_lblDeviceDetails = nullptr;
    lv_obj_t* m_lblRadioStats = nullptr;
    lv_obj_t* m_lblBattery = nullptr;
    lv_obj_t* m_taAlias = nullptr;

    // Estado local
    uint8_t m_channelIdx = 0;
    uint8_t m_newChannelIdx = 1;
    std::string m_selectedPort = "jp1";
    uint32_t m_selectedBaud = 115200;

    bool m_chatDirty = false;
    bool m_channelsDirty = false;
    bool m_radioDirty = false;
    size_t m_lastRenderedChCount = 0;
    size_t m_lastRenderedDmCount = 0;
    std::string m_pendingError;
};

} // namespace ui
} // namespace cbdos
