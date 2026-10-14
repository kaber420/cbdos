#include "../RestProberView.hpp"
#include "RestProberCommon.hpp"
#include "../../UIManager.hpp"
#include "../../../network/RestProberService.hpp"

namespace cbdos {
namespace ui {

void RestProberView::setupCollectionsTab(lv_obj_t* tab) {
    using namespace restprober;

    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 8, 0);

    m_listCollections = lv_list_create(tab);
    lv_obj_set_size(m_listCollections, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_listCollections, 1);
    lv_obj_set_style_bg_color(m_listCollections, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_listCollections, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_radius(m_listCollections, 8, 0);

    rebuildCollectionsList();
}

void RestProberView::rebuildCollectionsList() {
    if (!m_listCollections || !lv_obj_is_valid(m_listCollections)) return;

    lv_obj_clean(m_listCollections);

    // 1. Plantillas IoT Integradas
    lv_list_add_text(m_listCollections, "── PLANTILLAS IOT (PRESETS) ──");
    auto presets = cbdos::network::RestProberService::getInstance().getBuiltinPresets();
    for (size_t i = 0; i < presets.size(); ++i) {
        std::string label = "[" + std::string(http::httpMethodToString(presets[i].request.method)) + "] " + presets[i].name;
        lv_obj_t* btn = lv_list_add_button(m_listCollections, LV_SYMBOL_PLAY, label.c_str());
        lv_obj_set_style_text_color(btn, lv_color_hex(restprober::getMethodColor(presets[i].request.method)), 0);
        
        auto* copyReq = new cbdos::network::SavedRequestInfo(presets[i]);
        lv_obj_add_event_cb(btn, loadPresetCb, LV_EVENT_CLICKED, copyReq);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_DELETE) {
                delete static_cast<cbdos::network::SavedRequestInfo*>(lv_event_get_user_data(e));
            }
        }, LV_EVENT_DELETE, copyReq);
    }

    // 2. Peticiones Guardadas por el Usuario (Almacenamiento Local)
    lv_list_add_text(m_listCollections, "── PETICIONES GUARDADAS (/requests) ──");
    auto savedList = cbdos::network::RestProberService::getInstance().listSavedRequests();
    if (savedList.empty()) {
        lv_list_add_text(m_listCollections, "Sin peticiones guardadas. Usa el boton 💾 para guardar.");
    } else {
        for (size_t i = 0; i < savedList.size(); ++i) {
            std::string label = "[" + std::string(http::httpMethodToString(savedList[i].request.method)) + "] " + savedList[i].name;
            lv_obj_t* btn = lv_list_add_button(m_listCollections, LV_SYMBOL_FILE, label.c_str());
            lv_obj_set_style_text_color(btn, lv_color_hex(0x00F5D4), 0);

            auto* copyReq = new cbdos::network::SavedRequestInfo(savedList[i]);
            lv_obj_add_event_cb(btn, loadPresetCb, LV_EVENT_CLICKED, copyReq);
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                if (lv_event_get_code(e) == LV_EVENT_DELETE) {
                    delete static_cast<cbdos::network::SavedRequestInfo*>(lv_event_get_user_data(e));
                }
            }, LV_EVENT_DELETE, copyReq);
        }
    }
}

void RestProberView::loadPresetCb(lv_event_t* e) {
    auto* info = static_cast<cbdos::network::SavedRequestInfo*>(lv_event_get_user_data(e));
    auto currentView = UIManager::getInstance().getCurrentView();
    if (!currentView || currentView->getViewName() != "RestProberView" || !info) return;
    auto* view = static_cast<RestProberView*>(currentView.get());

    // 1. Cargar URL
    if (view->m_taUrl) {
        lv_textarea_set_text(view->m_taUrl, info->request.url.c_str());
    }

    // 2. Cargar Método
    if (view->m_dropdownMethod) {
        uint16_t idx = static_cast<uint16_t>(info->request.method);
        lv_dropdown_set_selected(view->m_dropdownMethod, idx);
        uint32_t col = restprober::getMethodColor(info->request.method);
        lv_obj_set_style_text_color(view->m_dropdownMethod, lv_color_hex(col), 0);
        lv_obj_set_style_border_color(view->m_dropdownMethod, lv_color_hex(col), 0);
    }

    // 3. Cargar Body
    if (view->m_taBody) {
        lv_textarea_set_text(view->m_taBody, info->request.body.c_str());
    }

    // 4. Cambiar a pestaña Body si tiene cuerpo, o Headers
    if (view->m_tabView) {
        if (!info->request.body.empty()) {
            lv_tabview_set_active(view->m_tabView, 1, LV_ANIM_OFF);
        } else {
            lv_tabview_set_active(view->m_tabView, 0, LV_ANIM_OFF);
        }
    }

    UIManager::showToast("Peticion cargada");
}

void RestProberView::openSaveModal() {
    UIManager::closeKeyboard();
    if (!m_taUrl) return;
    std::string url = lv_textarea_get_text(m_taUrl);
    if (url.empty() || url == "http://") {
        UIManager::showToast("Nada que guardar: URL vacia");
        return;
    }

    http::HttpRequest req;
    req.url = url;
    uint16_t sel = lv_dropdown_get_selected(m_dropdownMethod);
    switch (sel) {
        case 0: req.method = http::HttpMethod::Get; break;
        case 1: req.method = http::HttpMethod::Post; break;
        case 2: req.method = http::HttpMethod::Put; break;
        case 3: req.method = http::HttpMethod::Delete; break;
        case 4: req.method = http::HttpMethod::Patch; break;
        case 5: req.method = http::HttpMethod::Head; break;
        default: req.method = http::HttpMethod::Get; break;
    }
    if (m_taBody) req.body = lv_textarea_get_text(m_taBody);

    // Generar nombre automático sugerido basado en el verbo HTTP
    std::string suggestedName = "req_" + std::string(http::httpMethodToString(req.method));
    bool ok = cbdos::network::RestProberService::getInstance().saveRequest(suggestedName, req);
    if (ok) {
        UIManager::showToast("Peticion guardada en /requests");
        rebuildCollectionsList();
    } else {
        UIManager::showToast("Error al guardar en almacenamiento");
    }
}

} // namespace ui
} // namespace cbdos
