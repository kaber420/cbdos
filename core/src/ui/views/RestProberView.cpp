#include "RestProberView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/language.hpp"
#include "cbdos/storage.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace ui {

namespace {

static constexpr uint32_t kColBgCard   = 0x1E2230;
static constexpr uint32_t kColBorder   = 0x2E344A;
static constexpr uint32_t kColGreen    = 0x10B981;
static constexpr uint32_t kColBlue     = 0x3B82F6;
static constexpr uint32_t kColYellow   = 0xF59E0B;
static constexpr uint32_t kColRed      = 0xEF4444;
static constexpr uint32_t kColPurple   = 0x8B5CF6;
static constexpr uint32_t kColGray     = 0x64748B;
static constexpr uint32_t kColTextMuted = 0x94A3B8;

uint32_t getMethodColor(http::HttpMethod method) {
    switch (method) {
        case http::HttpMethod::Get:    return kColGreen;
        case http::HttpMethod::Post:   return kColBlue;
        case http::HttpMethod::Put:    return kColYellow;
        case http::HttpMethod::Delete: return kColRed;
        case http::HttpMethod::Patch:  return kColPurple;
        case http::HttpMethod::Head:   return kColGray;
        default:                       return kColGreen;
    }
}

} // namespace

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

// ── Barra Superior de Control (Método + URL + SEND) ─────────────────────────

void RestProberView::createControlBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_style_pad_gap(bar, 6, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColBgCard), 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 10, 0);
    DefaultTheme::disableScroll(bar);

    // 1. Selector de Método HTTP
    m_dropdownMethod = lv_dropdown_create(bar);
    lv_dropdown_set_options(m_dropdownMethod, "GET\nPOST\nPUT\nDELETE\nPATCH\nHEAD");
    lv_obj_set_width(m_dropdownMethod, 96);
    lv_obj_set_style_radius(m_dropdownMethod, 8, 0);
    lv_obj_set_style_bg_color(m_dropdownMethod, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_text_color(m_dropdownMethod, lv_color_hex(kColGreen), 0);
    lv_obj_set_style_border_color(m_dropdownMethod, lv_color_hex(kColGreen), 0);
    lv_obj_set_style_border_width(m_dropdownMethod, 1, 0);
    lv_obj_add_event_cb(m_dropdownMethod, methodDropdownCb, LV_EVENT_VALUE_CHANGED, this);

    // 2. Campo de URL
    m_taUrl = lv_textarea_create(bar);
    lv_textarea_set_one_line(m_taUrl, true);
    lv_obj_set_flex_grow(m_taUrl, 1);
    lv_textarea_set_placeholder_text(m_taUrl, "http://192.168.1.100/...");
    std::string defaultUrl = m_initialUrl.empty() ? "http://" : m_initialUrl;
    lv_textarea_set_text(m_taUrl, defaultUrl.c_str());
    lv_obj_set_style_radius(m_taUrl, 8, 0);
    lv_obj_set_style_bg_color(m_taUrl, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_taUrl, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(m_taUrl, lv_color_hex(0xF1F5F9), 0);
    UIManager::attachKeyboard(m_taUrl);

    // 3. Botón SEND
    m_btnSend = lv_button_create(bar);
    lv_obj_set_size(m_btnSend, 82, 40);
    lv_obj_set_style_radius(m_btnSend, 8, 0);
    lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x00F5D4), 0);
    lv_obj_add_event_cb(m_btnSend, sendBtnCb, LV_EVENT_CLICKED, this);

    m_lblSend = lv_label_create(m_btnSend);
    lv_label_set_text(m_lblSend, "SEND 🚀");
    lv_obj_set_style_text_color(m_lblSend, lv_color_hex(0x0A0F1D), 0);
    lv_obj_center(m_lblSend);
}

// ── Pestañas de Configuración (LVGL TabView) ─────────────────────────────────

void RestProberView::createTabs(lv_obj_t* parent) {
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

// ── Pestaña 1: Headers / Params ─────────────────────────────────────────────

void RestProberView::setupHeadersTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 6, 0);

    // Barra de acciones de cabeceras
    lv_obj_t* actRow = lv_obj_create(tab);
    lv_obj_set_size(actRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(actRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(actRow, 0, 0);
    lv_obj_set_style_pad_gap(actRow, 8, 0);
    lv_obj_set_style_bg_opa(actRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actRow, 0, 0);
    DefaultTheme::disableScroll(actRow);

    lv_obj_t* btnAdd = lv_button_create(actRow);
    lv_obj_set_size(btnAdd, 140, 36);
    lv_obj_set_style_radius(btnAdd, 8, 0);
    lv_obj_set_style_bg_color(btnAdd, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnAdd, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(btnAdd, 1, 0);
    lv_obj_add_event_cb(btnAdd, addHeaderBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblAdd = lv_label_create(btnAdd);
    lv_label_set_text(lblAdd, "+ Cabecera");
    lv_obj_set_style_text_color(lblAdd, lv_color_hex(0x00F5D4), 0);
    lv_obj_center(lblAdd);

    lv_obj_t* btnClear = lv_button_create(actRow);
    lv_obj_set_size(btnClear, 100, 36);
    lv_obj_set_style_radius(btnClear, 8, 0);
    lv_obj_set_style_bg_color(btnClear, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnClear, lv_color_hex(kColRed), 0);
    lv_obj_set_style_border_width(btnClear, 1, 0);
    lv_obj_add_event_cb(btnClear, clearHeadersBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblClear = lv_label_create(btnClear);
    lv_label_set_text(lblClear, "Limpiar");
    lv_obj_set_style_text_color(lblClear, lv_color_hex(kColRed), 0);
    lv_obj_center(lblClear);

    // Contenedor scrollable de filas clave-valor
    m_headersListCont = lv_obj_create(tab);
    lv_obj_set_size(m_headersListCont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_headersListCont, 1);
    lv_obj_set_flex_flow(m_headersListCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_headersListCont, 4, 0);
    lv_obj_set_style_pad_gap(m_headersListCont, 6, 0);
    lv_obj_set_style_bg_color(m_headersListCont, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_headersListCont, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(m_headersListCont, 1, 0);
    lv_obj_set_style_radius(m_headersListCont, 8, 0);

    // Cabecera por defecto para agilizar APIs REST
    addHeaderRow("Content-Type", "application/json");
}

void RestProberView::addHeaderRow(const std::string& key, const std::string& val) {
    if (!m_headersListCont) return;

    HeaderRowWidget row;
    row.rowObj = lv_obj_create(m_headersListCont);
    lv_obj_set_size(row.rowObj, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row.rowObj, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row.rowObj, 2, 0);
    lv_obj_set_style_pad_gap(row.rowObj, 6, 0);
    lv_obj_set_style_bg_opa(row.rowObj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row.rowObj, 0, 0);
    DefaultTheme::disableScroll(row.rowObj);

    row.taKey = lv_textarea_create(row.rowObj);
    lv_textarea_set_one_line(row.taKey, true);
    lv_obj_set_flex_grow(row.taKey, 1);
    lv_textarea_set_placeholder_text(row.taKey, "Clave (Header)");
    if (!key.empty()) lv_textarea_set_text(row.taKey, key.c_str());
    lv_obj_set_style_radius(row.taKey, 6, 0);
    lv_obj_set_style_bg_color(row.taKey, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(row.taKey, lv_color_hex(kColBorder), 0);
    UIManager::attachKeyboard(row.taKey);

    row.taVal = lv_textarea_create(row.rowObj);
    lv_textarea_set_one_line(row.taVal, true);
    lv_obj_set_flex_grow(row.taVal, 1);
    lv_textarea_set_placeholder_text(row.taVal, "Valor");
    if (!val.empty()) lv_textarea_set_text(row.taVal, val.c_str());
    lv_obj_set_style_radius(row.taVal, 6, 0);
    lv_obj_set_style_bg_color(row.taVal, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(row.taKey, lv_color_hex(kColBorder), 0);
    UIManager::attachKeyboard(row.taVal);

    m_headerRows.push_back(row);
}

void RestProberView::clearHeaders() {
    if (m_headersListCont) {
        lv_obj_clean(m_headersListCont);
    }
    m_headerRows.clear();
}

// ── Pestaña 2: Body (Payload multilínea) ────────────────────────────────────

void RestProberView::setupBodyTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 6, 0);

    // Barra de inserción rápida
    lv_obj_t* btnRow = lv_obj_create(tab);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_style_pad_gap(btnRow, 8, 0);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    DefaultTheme::disableScroll(btnRow);

    lv_obj_t* btnTpl = lv_button_create(btnRow);
    lv_obj_set_size(btnTpl, 150, 34);
    lv_obj_set_style_radius(btnTpl, 8, 0);
    lv_obj_set_style_bg_color(btnTpl, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnTpl, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_border_width(btnTpl, 1, 0);
    lv_obj_add_event_cb(btnTpl, [](lv_event_t* e) {
        auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
        if (view && view->m_taBody) {
            lv_textarea_set_text(view->m_taBody, "{\n  \"turn\": \"toggle\"\n}");
        }
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* lblTpl = lv_label_create(btnTpl);
    lv_label_set_text(lblTpl, "{ } JSON Relay");
    lv_obj_set_style_text_color(lblTpl, lv_color_hex(0x00F5D4), 0);
    lv_obj_center(lblTpl);

    lv_obj_t* btnClr = lv_button_create(btnRow);
    lv_obj_set_size(btnClr, 90, 34);
    lv_obj_set_style_radius(btnClr, 8, 0);
    lv_obj_set_style_bg_color(btnClr, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(btnClr, lv_color_hex(kColRed), 0);
    lv_obj_set_style_border_width(btnClr, 1, 0);
    lv_obj_add_event_cb(btnClr, [](lv_event_t* e) {
        auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
        if (view && view->m_taBody) {
            lv_textarea_set_text(view->m_taBody, "");
        }
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* lblClr = lv_label_create(btnClr);
    lv_label_set_text(lblClr, "Borrar");
    lv_obj_set_style_text_color(lblClr, lv_color_hex(kColRed), 0);
    lv_obj_center(lblClr);

    // Text area multilínea para el cuerpo
    m_taBody = lv_textarea_create(tab);
    lv_obj_set_size(m_taBody, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_taBody, 1);
    lv_textarea_set_placeholder_text(m_taBody, "Cuerpo de la petición (JSON, XML o texto plano)...");
    lv_obj_set_style_radius(m_taBody, 8, 0);
    lv_obj_set_style_bg_color(m_taBody, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_taBody, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(m_taBody, lv_color_hex(0xF1F5F9), 0);
    UIManager::attachKeyboard(m_taBody);
}

// ── Pestaña 3: Response (Salida de estado, RTT y contenido) ─────────────────

void RestProberView::setupResponseTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 6, 0);

    // Barra de telemetría de respuesta
    lv_obj_t* bar = lv_obj_create(tab);
    lv_obj_set_size(bar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(bar, 6, 0);
    lv_obj_set_style_pad_gap(bar, 10, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColBgCard), 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 8, 0);
    DefaultTheme::disableScroll(bar);

    m_badgeStatus = lv_obj_create(bar);
    lv_obj_set_size(m_badgeStatus, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_radius(m_badgeStatus, 6, 0);
    lv_obj_set_style_bg_color(m_badgeStatus, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(m_badgeStatus, lv_color_hex(kColGray), 0);
    lv_obj_set_style_border_width(m_badgeStatus, 1, 0);
    lv_obj_set_style_pad_hor(m_badgeStatus, 8, 0);
    lv_obj_set_style_pad_ver(m_badgeStatus, 4, 0);
    DefaultTheme::disableScroll(m_badgeStatus);

    m_lblStatusCode = lv_label_create(m_badgeStatus);
    lv_label_set_text(m_lblStatusCode, "STATUS: ---");
    lv_obj_set_style_text_color(m_lblStatusCode, lv_color_hex(kColGray), 0);
    lv_obj_center(m_lblStatusCode);

    m_lblRtt = lv_label_create(bar);
    lv_label_set_text(m_lblRtt, "RTT: -- ms");
    lv_obj_set_style_text_color(m_lblRtt, lv_color_hex(kColTextMuted), 0);
    lv_obj_set_style_pad_top(m_lblRtt, 6, 0);

    m_lblSize = lv_label_create(bar);
    lv_label_set_text(m_lblSize, "Size: -- B");
    lv_obj_set_style_text_color(m_lblSize, lv_color_hex(kColTextMuted), 0);
    lv_obj_set_style_pad_top(m_lblSize, 6, 0);

    // Contenedor del cuerpo devuelto
    m_taResponseBody = lv_textarea_create(tab);
    lv_obj_set_size(m_taResponseBody, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_taResponseBody, 1);
    lv_textarea_set_placeholder_text(m_taResponseBody, "Presiona 'SEND' para ejecutar la peticion HTTP...");
    lv_obj_set_style_radius(m_taResponseBody, 8, 0);
    lv_obj_set_style_bg_color(m_taResponseBody, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_taResponseBody, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(m_taResponseBody, lv_color_hex(0xF1F5F9), 0);
    // Modo solo lectura
    lv_obj_clear_flag(m_taResponseBody, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

// ── Pestaña 4: Colecciones y Presets Guardados ───────────────────────────────

void RestProberView::setupCollectionsTab(lv_obj_t* tab) {
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
        lv_obj_set_style_text_color(btn, lv_color_hex(getMethodColor(presets[i].request.method)), 0);
        
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

// ── Ejecución de la Petición y Renderizado de Respuesta ──────────────────────

void RestProberView::sendRequest() {
    UIManager::closeKeyboard();

    if (!m_taUrl) return;
    std::string url = lv_textarea_get_text(m_taUrl);
    if (url.empty() || url == "http://" || url == "https://") {
        UIManager::showToast("Ingresa una URL valida");
        return;
    }

    http::HttpRequest req;
    req.url = url;

    // Obtener método seleccionado
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

    // Cabeceras configuradas
    for (const auto& row : m_headerRows) {
        if (!row.taKey || !row.taVal) continue;
        std::string k = lv_textarea_get_text(row.taKey);
        std::string v = lv_textarea_get_text(row.taVal);
        if (!k.empty()) {
            req.headers.push_back({k, v});
        }
    }

    // Cuerpo
    if (m_taBody) {
        req.body = lv_textarea_get_text(m_taBody);
    }

    // Actualizar UI a estado ocupado
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

void RestProberView::renderResponse(const http::HttpResponse& response) {
    if (m_lblSend) lv_label_set_text(m_lblSend, "SEND 🚀");
    if (m_btnSend) lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x00F5D4), 0);

    char buf[64];
    // 1. Status Code
    if (m_lblStatusCode && lv_obj_is_valid(m_lblStatusCode)) {
        if (response.statusCode > 0) {
            snprintf(buf, sizeof(buf), "STATUS: %d", response.statusCode);
            lv_label_set_text(m_lblStatusCode, buf);
            uint32_t col = (response.statusCode >= 200 && response.statusCode < 300) ? kColGreen : kColRed;
            lv_obj_set_style_text_color(m_lblStatusCode, lv_color_hex(col), 0);
            if (m_badgeStatus) {
                lv_obj_set_style_border_color(m_badgeStatus, lv_color_hex(col), 0);
            }
        } else {
            lv_label_set_text(m_lblStatusCode, "STATUS: ERR");
            lv_obj_set_style_text_color(m_lblStatusCode, lv_color_hex(kColRed), 0);
            if (m_badgeStatus) {
                lv_obj_set_style_border_color(m_badgeStatus, lv_color_hex(kColRed), 0);
            }
        }
    }

    // 2. Latencia RTT
    if (m_lblRtt && lv_obj_is_valid(m_lblRtt)) {
        snprintf(buf, sizeof(buf), "RTT: %u ms", static_cast<unsigned>(response.rttMs));
        lv_label_set_text(m_lblRtt, buf);
    }

    // 3. Tamaño en bytes
    if (m_lblSize && lv_obj_is_valid(m_lblSize)) {
        snprintf(buf, sizeof(buf), "Size: %u B%s",
                 static_cast<unsigned>(response.body.size()),
                 response.truncated ? " (Trunc)" : "");
        lv_label_set_text(m_lblSize, buf);
    }

    // 4. Cuerpo de la respuesta
    if (m_taResponseBody && lv_obj_is_valid(m_taResponseBody)) {
        if (!response.body.empty()) {
            lv_textarea_set_text(m_taResponseBody, response.body.c_str());
        } else if (!response.errorMessage.empty()) {
            std::string errText = "Error: " + response.errorMessage;
            lv_textarea_set_text(m_taResponseBody, errText.c_str());
        } else {
            lv_textarea_set_text(m_taResponseBody, "(Cuerpo de respuesta vacio)");
        }
    }
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

    // Generar nombre automático sugerido basado en la URL
    std::string suggestedName = "req_" + std::string(http::httpMethodToString(req.method));
    bool ok = cbdos::network::RestProberService::getInstance().saveRequest(suggestedName, req);
    if (ok) {
        UIManager::showToast("Peticion guardada en /requests");
        rebuildCollectionsList();
    } else {
        UIManager::showToast("Error al guardar en almacenamiento");
    }
}

// ── Callbacks de Eventos LVGL ───────────────────────────────────────────────

void RestProberView::sendBtnCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (view) {
        view->sendRequest();
    }
}

void RestProberView::methodDropdownCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (!view || !view->m_dropdownMethod) return;

    uint16_t sel = lv_dropdown_get_selected(view->m_dropdownMethod);
    http::HttpMethod method = static_cast<http::HttpMethod>(sel);
    uint32_t col = getMethodColor(method);

    lv_obj_set_style_text_color(view->m_dropdownMethod, lv_color_hex(col), 0);
    lv_obj_set_style_border_color(view->m_dropdownMethod, lv_color_hex(col), 0);
}

void RestProberView::addHeaderBtnCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (view) {
        view->addHeaderRow();
    }
}

void RestProberView::clearHeadersBtnCb(lv_event_t* e) {
    auto* view = static_cast<RestProberView*>(lv_event_get_user_data(e));
    if (view) {
        view->clearHeaders();
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
        uint32_t col = getMethodColor(info->request.method);
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

void RestProberView::tabChangeCb(lv_event_t* e) {
    // Al cambiar de pestaña, cerramos el teclado para evitar solapamientos
    UIManager::closeKeyboard();
}

} // namespace ui
} // namespace cbdos
