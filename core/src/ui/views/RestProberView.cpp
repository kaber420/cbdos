#include "RestProberView.hpp"
#include "restprober/RestProberCommon.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "../../network/RestProberService.hpp"

namespace cbdos {
namespace ui {

RestProberView::RestProberView(const std::string& initialUrl)
    : BaseView("RestProberView"), m_initialUrl(initialUrl) {
}

RestProberView::~RestProberView() {
    onDestroy();
}

bool RestProberView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 6, 0);
    lv_obj_set_style_pad_gap(m_container, 6, 0);
    DefaultTheme::applyFlatBg(m_container);

    createControlBar(m_container);
    createTabs(m_container);

    return true;
}

void RestProberView::onDestroy() {
    UIManager::closeKeyboard();
    m_headerRows.clear();
    BaseView::onDestroy();
}

void RestProberView::onShow() {
    BaseView::onShow();
    auto& header = UIManager::getInstance().getHeaderBar();
    header.setTitle("RestProber");
    header.showWifi(true);
    header.showBackButton(true, []() {
        UIManager::closeKeyboard();
        UIManager::getInstance().popView();
    });
    header.setRightAction(LV_SYMBOL_SAVE, [this]() {
        openSaveModal();
    });

    rebuildCollectionsList();
}

void RestProberView::onHide() {
    UIManager::closeKeyboard();
    UIManager::getInstance().getHeaderBar().clearRightAction();
    BaseView::onHide();
}

void RestProberView::setTargetUrl(const std::string& url) {
    m_initialUrl = url;
    if (m_taUrl && lv_obj_is_valid(m_taUrl)) {
        lv_textarea_set_text(m_taUrl, url.c_str());
    }
}

// ── Pestañas de Configuración (LVGL TabView) ─────────────────────────────────

void RestProberView::createTabs(lv_obj_t* parent) {
    using namespace restprober;

    m_tabView = lv_tabview_create(parent);
    lv_obj_set_size(m_tabView, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_tabView, 1);
    lv_obj_set_style_bg_color(m_tabView, lv_color_hex(0x13151F), 0);
    lv_obj_set_style_radius(m_tabView, 10, 0);
    lv_obj_set_style_border_color(m_tabView, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(m_tabView, 1, 0);
    lv_obj_add_event_cb(m_tabView, tabChangeCb, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t* tabHeaders = lv_tabview_add_tab(m_tabView, "Headers");
    lv_obj_t* tabBody    = lv_tabview_add_tab(m_tabView, "Body");
    lv_obj_t* tabResp    = lv_tabview_add_tab(m_tabView, "Response");
    lv_obj_t* tabSaved   = lv_tabview_add_tab(m_tabView, "Colecciones");

    setupHeadersTab(tabHeaders);
    setupBodyTab(tabBody);
    setupResponseTab(tabResp);
    setupCollectionsTab(tabSaved);
}

void RestProberView::tabChangeCb(lv_event_t* e) {
    // Al cambiar de pestaña cerramos el teclado para evitar solapamientos visuales
    UIManager::closeKeyboard();
}

// ── Ejecución de la Petición Asíncrona ───────────────────────────────────────

void RestProberView::sendRequest() {
    using namespace restprober;
    UIManager::closeKeyboard();

    if (!m_taUrl) return;
    std::string url = lv_textarea_get_text(m_taUrl);
    if (url.empty() || url == "http://" || url == "https://") {
        UIManager::showToast("Ingresa una URL valida");
        return;
    }

    http::HttpRequest req;
    req.url = url;

    // 1. Obtener método seleccionado
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

    // 2. Extraer cabeceras configuradas
    for (const auto& row : m_headerRows) {
        if (!row.taKey || !lv_obj_is_valid(row.taKey) || !row.taVal || !lv_obj_is_valid(row.taVal)) continue;
        std::string k = lv_textarea_get_text(row.taKey);
        std::string v = lv_textarea_get_text(row.taVal);
        if (!k.empty()) {
            req.headers.push_back({k, v});
        }
    }

    // 3. Extraer cuerpo de la petición
    if (m_taBody) {
        req.body = lv_textarea_get_text(m_taBody);
    }

    // 4. Actualizar UI a estado ocupado
    if (m_lblSend) lv_label_set_text(m_lblSend, "...");
    if (m_btnSend) lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(kColGray), 0);

    // Cambiar a la pestaña Response para mostrar el progreso
    if (m_tabView) {
        lv_tabview_set_active(m_tabView, 2, LV_ANIM_OFF);
    }
    if (m_taResponseBody) {
        lv_textarea_set_text(m_taResponseBody, "Enviando peticion HTTP...");
    }

    bool started = cbdos::network::RestProberService::getInstance().sendRequestAsync(
        req, [this](const http::HttpResponse& resp) {
            // Callback invocado al terminar en Core 0.
            // Para LVGL seguro, programamos en el bucle principal vía lv_async_call
            auto* pResp = new http::HttpResponse(resp);
            lv_async_call([](void* data) {
                auto* r = static_cast<http::HttpResponse*>(data);
                auto currentView = UIManager::getInstance().getCurrentView();
                if (currentView && currentView->getViewName() == "RestProberView" && r) {
                    auto* self = static_cast<RestProberView*>(currentView.get());
                    self->renderResponse(*r);
                }
                delete r;
            }, pResp);
        });

    if (!started) {
        if (m_lblSend) lv_label_set_text(m_lblSend, "SEND 🚀");
        if (m_btnSend) lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x00F5D4), 0);
        UIManager::showToast("Fallo al iniciar peticion (sin red o en curso)");
    }
}

} // namespace ui
} // namespace cbdos
