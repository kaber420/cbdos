#pragma once

#include "BaseView.hpp"
#include "cbdos/http.hpp"

#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class RestProberView : public BaseView {
public:
    explicit RestProberView(const std::string& initialUrl = "");
    ~RestProberView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;

    void setTargetUrl(const std::string& url);

private:
    struct HeaderRowWidget {
        lv_obj_t* rowObj = nullptr;
        lv_obj_t* taKey = nullptr;
        lv_obj_t* taVal = nullptr;
    };

    // Submódulos UI (distribuidos en restprober/)
    void createControlBar(lv_obj_t* parent);
    void createTabs(lv_obj_t* parent);
    void setupHeadersTab(lv_obj_t* tab);
    void setupBodyTab(lv_obj_t* tab);
    void setupResponseTab(lv_obj_t* tab);
    void setupCollectionsTab(lv_obj_t* tab);

    // Callbacks de Eventos LVGL
    static void sendBtnCb(lv_event_t* e);
    static void methodDropdownCb(lv_event_t* e);
    static void addHeaderBtnCb(lv_event_t* e);
    static void clearHeadersBtnCb(lv_event_t* e);
    static void loadPresetCb(lv_event_t* e);
    static void tabChangeCb(lv_event_t* e);

    // Lógica interna modular
    void sendRequest();
    void renderResponse(const http::HttpResponse& response);
    void addHeaderRow(const std::string& key = "", const std::string& val = "");
    void clearHeaders();
    void rebuildCollectionsList();
    void openSaveModal();

    std::string m_initialUrl;

    // Widgets ControlBar
    lv_obj_t* m_dropdownMethod = nullptr;
    lv_obj_t* m_taUrl = nullptr;
    lv_obj_t* m_btnSend = nullptr;
    lv_obj_t* m_lblSend = nullptr;
    lv_obj_t* m_tabView = nullptr;

    // Pestaña Headers
    lv_obj_t* m_headersListCont = nullptr;
    std::vector<HeaderRowWidget> m_headerRows;

    // Pestaña Body
    lv_obj_t* m_taBody = nullptr;

    // Pestaña Response
    lv_obj_t* m_badgeStatus = nullptr;
    lv_obj_t* m_lblStatusCode = nullptr;
    lv_obj_t* m_lblRtt = nullptr;
    lv_obj_t* m_lblSize = nullptr;
    lv_obj_t* m_taResponseBody = nullptr;

    // Pestaña Colecciones
    lv_obj_t* m_listCollections = nullptr;
};

} // namespace ui
} // namespace cbdos
