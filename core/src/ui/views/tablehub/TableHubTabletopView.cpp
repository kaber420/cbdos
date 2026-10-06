#include "TableHubTabletopView.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"
#include "cbdos/system.hpp"

namespace cbdos {
namespace ui {

static const char* TAG = "TableHubTabletop";

TableHubTabletopView::TableHubTabletopView()
    : BaseView("TableHub Mesa") {
}

static void serviceBtnCb(lv_event_t* e) {
    const char* action = static_cast<const char*>(lv_event_get_user_data(e));
    if (action) {
        char toastMsg[64];
        snprintf(toastMsg, sizeof(toastMsg), "Solicitud enviada: %s", action);
        UIManager::showToast(toastMsg);
    }
}

bool TableHubTabletopView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Creando TableHubTabletopView...");

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, lv_pct(100), lv_pct(100));
    DefaultTheme::applyFlatBg(m_container);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Barra de Servicios de Mesa
    m_serviceBar = lv_obj_create(m_container);
    lv_obj_set_size(m_serviceBar, lv_pct(100), 44);
    DefaultTheme::applyRaisedCard(m_serviceBar, 12);
    lv_obj_set_style_pad_hor(m_serviceBar, 12, 0);
    lv_obj_set_style_pad_ver(m_serviceBar, 0, 0);
    lv_obj_set_flex_flow(m_serviceBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_serviceBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(m_serviceBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTable = lv_label_create(m_serviceBar);
    lv_label_set_text(lblTable, LV_SYMBOL_HOME " MESA 01 - CARTA DIGITAL");
    lv_obj_set_style_text_color(lblTable, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblTable, &lv_font_montserrat_14, 0);

    lv_obj_t* btnBox = lv_obj_create(m_serviceBar);
    lv_obj_set_size(btnBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_opa(btnBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnBox, 0, 0);
    lv_obj_set_style_pad_all(btnBox, 0, 0);
    lv_obj_set_flex_flow(btnBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnBox, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btnBox, 8, 0);
    lv_obj_remove_flag(btnBox, LV_OBJ_FLAG_SCROLLABLE);

    m_btnCallWaiter = lv_button_create(btnBox);
    lv_obj_set_size(m_btnCallWaiter, 130, 32);
    DefaultTheme::applyButton(m_btnCallWaiter, 8);
    lv_obj_set_style_bg_color(m_btnCallWaiter, lv_color_hex(0x0284C7), 0);
    lv_obj_t* lblWaiter = lv_label_create(m_btnCallWaiter);
    lv_label_set_text(lblWaiter, LV_SYMBOL_CALL " Llamar Camarero");
    lv_obj_set_style_text_font(lblWaiter, &lv_font_montserrat_12, 0);
    lv_obj_center(lblWaiter);
    lv_obj_add_event_cb(m_btnCallWaiter, serviceBtnCb, LV_EVENT_CLICKED, (void*)"Llamar Camarero");

    m_btnAskBill = lv_button_create(btnBox);
    lv_obj_set_size(m_btnAskBill, 120, 32);
    DefaultTheme::applyButton(m_btnAskBill, 8);
    lv_obj_set_style_bg_color(m_btnAskBill, lv_color_hex(0x10B981), 0);
    lv_obj_t* lblBill = lv_label_create(m_btnAskBill);
    lv_label_set_text(lblBill, LV_SYMBOL_CHARGE " Pedir Cuenta");
    lv_obj_set_style_text_font(lblBill, &lv_font_montserrat_12, 0);
    lv_obj_center(lblBill);
    lv_obj_add_event_cb(m_btnAskBill, serviceBtnCb, LV_EVENT_CLICKED, (void*)"Pedir Cuenta");

    // Catálogo / Contenedor de Menú
    m_menuContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_menuContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_menuContainer, 1);
    DefaultTheme::applySunkenCard(m_menuContainer, 14);
    lv_obj_set_style_pad_all(m_menuContainer, 16, 0);
    lv_obj_set_style_pad_row(m_menuContainer, 12, 0);
    lv_obj_set_flex_flow(m_menuContainer, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lblMenuTitle = lv_label_create(m_menuContainer);
    lv_label_set_text(lblMenuTitle, "Categorías de la Carta");
    lv_obj_set_style_text_color(lblMenuTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblMenuTitle, &lv_font_montserrat_16, 0);

    const char* categories[] = {"Entrantes y Tapas", "Platos Principales", "Bebidas y Cócteles", "Postres y Cafés"};
    const char* icons[] = {LV_SYMBOL_LIST, LV_SYMBOL_PLAY, LV_SYMBOL_REFRESH, LV_SYMBOL_OK};

    for (int i = 0; i < 4; i++) {
        lv_obj_t* catCard = lv_obj_create(m_menuContainer);
        lv_obj_set_size(catCard, lv_pct(100), 50);
        DefaultTheme::applyRaisedCard(catCard, 10);
        lv_obj_set_style_pad_hor(catCard, 14, 0);
        lv_obj_set_style_pad_ver(catCard, 0, 0);
        lv_obj_set_flex_flow(catCard, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(catCard, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(catCard, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lblName = lv_label_create(catCard);
        char catText[64];
        snprintf(catText, sizeof(catText), "%s  %s", icons[i], categories[i]);
        lv_label_set_text(lblName, catText);
        lv_obj_set_style_text_color(lblName, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblName, &lv_font_montserrat_14, 0);

        lv_obj_t* lblChevron = lv_label_create(catCard);
        lv_label_set_text(lblChevron, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(lblChevron, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_text_font(lblChevron, &lv_font_montserrat_14, 0);

        lv_obj_add_event_cb(catCard, [](lv_event_t* e) {
            UIManager::showToast("Abriendo categoría...");
        }, LV_EVENT_CLICKED, nullptr);
    }

    return true;
}

void TableHubTabletopView::onDestroy() {
    BaseView::onDestroy();
}

void TableHubTabletopView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
}

void TableHubTabletopView::onUpdate() {
}

} // namespace ui
} // namespace cbdos
