#pragma once

#include "BaseView.hpp"
#include "cbdos/meshcore/meshcore_client.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

// Vista MeshCore LoRa (Fase 2, paridad Android):
// Tabs abajo Contactos / Canales / Mapa / Radio. Contactos con agenda real
// del dongle (GET_CONTACTS), conversación DM 1-1, detalles, advert y mapa.
class MeshCoreView : public BaseView {
public:
    MeshCoreView();
    ~MeshCoreView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

private:
    enum class ContactsPane {
        List,
        Conversation,
        Details,
    };

    // Pestañas
    void buildContactsTab(lv_obj_t* tab);
    void buildChatsTab(lv_obj_t* tab);  // chat del canal activo (dentro de Canales)
    void buildChannelsTab(lv_obj_t* tab);
    void buildMapTab(lv_obj_t* tab);
    void buildRadioTab(lv_obj_t* tab);

    // Contactos
    void showContactsPane(ContactsPane pane);
    void refreshContactsList();
    void refreshConversation();
    void refreshDetails();
    void openConversation(const std::string& prefixHex12);
    void openDetails(const std::string& prefixHex12);
    void sendDirectMessage();
    void showOverlayAdvert();
    void showOverlayPlus();
    void showOverlayManualAdd();
    void showOverlayImportCard();
    void hideOverlay();

    // Actualizaciones reactivas de UI
    void refreshChatLog();
    void refreshChannelsList();
    void refreshMapList();
    void refreshRadioStatus();
    void exportGpx();

    // Acciones
    void sendMessage();
    void setActiveChannel(uint8_t idx);
    void updateChannelLabel();
    void autosaveCache(uint32_t nowMs);
    void applyRadioParams();
    static std::string loadLastPort();
    static void saveLastPort(const std::string& portId);

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
    static void radioApplyBtnCb(lv_event_t* e);
    // Fase 2
    static void searchTaCb(lv_event_t* e);
    static void contactRowCb(lv_event_t* e);
    static void contactMenuCb(lv_event_t* e);
    static void convBackCb(lv_event_t* e);
    static void convDetailsCb(lv_event_t* e);
    static void convSendCb(lv_event_t* e);
    static void convRetryCb(lv_event_t* e);
    static void dmInputEventCb(lv_event_t* e);
    static void toggleDmKbBtnCb(lv_event_t* e);
    static void dmKbEventCb(lv_event_t* e);
    static void detailsBackCb(lv_event_t* e);
    static void detailsSaveNameCb(lv_event_t* e);
    static void detailsSavePathCb(lv_event_t* e);
    static void detailsResetPathCb(lv_event_t* e);
    static void detailsShareCb(lv_event_t* e);
    static void detailsRemoveCb(lv_event_t* e);
    static void detailsFavCb(lv_event_t* e);
    static void advertBtnCb(lv_event_t* e);
    static void plusBtnCb(lv_event_t* e);
    static void overlayCloseCb(lv_event_t* e);
    static void advertZeroCb(lv_event_t* e);
    static void advertFloodCb(lv_event_t* e);
    static void plusDiscoverCb(lv_event_t* e);
    static void plusManualCb(lv_event_t* e);
    static void plusImportCb(lv_event_t* e);
    static void manualSaveCb(lv_event_t* e);
    static void importSaveCb(lv_event_t* e);
    static void mapExportCb(lv_event_t* e);
    static void discoverBtnCb(lv_event_t* e);

    lv_obj_t* m_tabview = nullptr;
    lv_timer_t* m_pumpTimer = nullptr;

    // Tab Contactos: lista
    lv_obj_t* m_listPane = nullptr;
    lv_obj_t* m_taSearch = nullptr;
    lv_obj_t* m_contactsContainer = nullptr;
    lv_obj_t* m_lblContactsCount = nullptr;
    std::vector<std::string> m_rowPrefixes;
    // Tab Contactos: conversación
    lv_obj_t* m_convPane = nullptr;
    lv_obj_t* m_lblConvTitle = nullptr;
    lv_obj_t* m_convContainer = nullptr;
    lv_obj_t* m_lblConvStatus = nullptr;
    lv_obj_t* m_taDmInput = nullptr;
    lv_obj_t* m_btnDmKb = nullptr;
    lv_obj_t* m_keyboardDm = nullptr;
    bool m_keyboardDmVisible = false;
    // Tab Contactos: detalles
    lv_obj_t* m_detailsPane = nullptr;
    lv_obj_t* m_lblDetailsTitle = nullptr;
    lv_obj_t* m_lblDetailsBody = nullptr;
    lv_obj_t* m_taDetailName = nullptr;
    lv_obj_t* m_taDetailPath = nullptr;
    // Overlay genérico (advert / + / formularios)
    lv_obj_t* m_overlay = nullptr;
    lv_obj_t* m_overlayCard = nullptr;
    lv_obj_t* m_taManualKey = nullptr;
    lv_obj_t* m_taManualName = nullptr;
    lv_obj_t* m_ddManualType = nullptr;
    lv_obj_t* m_taImportCard = nullptr;

    // Tab Canales: chat del canal activo
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

    // Tab Mapa
    lv_obj_t* m_mapContainer = nullptr;
    lv_obj_t* m_lblMapCount = nullptr;

    // Tab 3: Radio
    lv_obj_t* m_ddPort = nullptr;
    lv_obj_t* m_btnConnect = nullptr;
    lv_obj_t* m_lblConnStatus = nullptr;
    lv_obj_t* m_lblDeviceDetails = nullptr;
    lv_obj_t* m_lblRadioStats = nullptr;
    lv_obj_t* m_lblBattery = nullptr;
    lv_obj_t* m_taAlias = nullptr;
    // Parámetros de radio editables (CLI + reboot)
    lv_obj_t* m_taFreq = nullptr;
    lv_obj_t* m_taBw = nullptr;
    lv_obj_t* m_ddSf = nullptr;
    lv_obj_t* m_ddCr = nullptr;
    lv_obj_t* m_taTx = nullptr;
    std::vector<std::string> m_portIds;

    // Estado local
    uint8_t m_channelIdx = 0;
    uint8_t m_newChannelIdx = 1;
    std::string m_selectedPort = "jp1";
    uint32_t m_selectedBaud = 115200;
    ContactsPane m_contactsPane = ContactsPane::List;
    ContactsPane m_detailsReturn = ContactsPane::List;
    std::string m_activePrefix;  // contacto en conversación/detalles
    std::string m_searchFilter;
    uint32_t m_lastStoreSaveMs = 0;

    bool m_chatDirty = false;
    bool m_channelsDirty = false;
    bool m_radioDirty = false;
    bool m_contactsDirty = false;
    size_t m_lastRenderedChCount = 0;
    size_t m_lastRenderedDmCount = 0;
    uint8_t m_lastRenderedChannel = 0xFF;
    size_t m_lastRenderedContactsSig = 0;
    size_t m_lastRenderedConvMsgs = 0;
    std::string m_lastRenderedConvPrefix;
    size_t m_lastRenderedConvPending = 0;
    std::string m_pendingError;
};

} // namespace ui
} // namespace cbdos
