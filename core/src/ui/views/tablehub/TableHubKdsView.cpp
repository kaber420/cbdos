#include "TableHubKdsView.hpp"
#include "TableHubLanguage.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"
#include "cbdos/system.hpp"

namespace cbdos {
namespace ui {

static const char* TAG = "TableHubKds";

TableHubKdsView::TableHubKdsView()
    : BaseView(tablehub::lang::tr(tablehub::lang::Str::KDS_TITLE)) {
}

static lv_obj_t* createColumn(lv_obj_t* parent, const char* title, lv_color_t headerColor) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_set_height(col, lv_pct(100));
    lv_obj_set_flex_grow(col, 1);
    DefaultTheme::applySunkenCard(col, 12);
    lv_obj_set_style_pad_all(col, 10, 0);
    lv_obj_set_style_pad_row(col, 8, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* colHeader = lv_obj_create(col);
    lv_obj_set_size(colHeader, lv_pct(100), 32);
    lv_obj_set_style_bg_color(colHeader, headerColor, 0);
    lv_obj_set_style_bg_opa(colHeader, LV_OPA_30, 0);
    lv_obj_set_style_border_color(colHeader, headerColor, 0);
    lv_obj_set_style_border_width(colHeader, 1, 0);
    lv_obj_set_style_radius(colHeader, 8, 0);
    lv_obj_set_style_pad_hor(colHeader, 10, 0);
    lv_obj_set_style_pad_ver(colHeader, 0, 0);
    lv_obj_remove_flag(colHeader, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(colHeader);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    return col;
}

bool TableHubKdsView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Creando TableHubKdsView...");

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, lv_pct(100), lv_pct(100));
    DefaultTheme::applyFlatBg(m_container);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // Barra de Estado Superior KDS
    m_statusBar = lv_obj_create(m_container);
    lv_obj_set_size(m_statusBar, lv_pct(100), 38);
    DefaultTheme::applyRaisedCard(m_statusBar, 10);
    lv_obj_set_style_pad_hor(m_statusBar, 14, 0);
    lv_obj_set_style_pad_ver(m_statusBar, 0, 0);
    lv_obj_set_flex_flow(m_statusBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_statusBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(m_statusBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblBadge = lv_label_create(m_statusBar);
    lv_label_set_text(lblBadge, tablehub::lang::tr(tablehub::lang::Str::KDS_STATUS_ACTIVE));
    lv_obj_set_style_text_color(lblBadge, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblBadge, &lv_font_montserrat_14, 0);

    lv_obj_t* lblOrdersCount = lv_label_create(m_statusBar);
    lv_label_set_text(lblOrdersCount, tablehub::lang::tr(tablehub::lang::Str::KDS_NO_ORDERS));
    lv_obj_set_style_text_color(lblOrdersCount, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(lblOrdersCount, &lv_font_montserrat_12, 0);

    // Contenedor Kanban (3 columnas)
    m_kanbanContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_kanbanContainer, lv_pct(100));
    lv_obj_set_flex_grow(m_kanbanContainer, 1);
    lv_obj_set_style_bg_opa(m_kanbanContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_kanbanContainer, 0, 0);
    lv_obj_set_style_pad_all(m_kanbanContainer, 0, 0);
    lv_obj_set_style_pad_column(m_kanbanContainer, 8, 0);
    lv_obj_set_flex_flow(m_kanbanContainer, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(m_kanbanContainer, LV_OBJ_FLAG_SCROLLABLE);

    m_colPending = createColumn(m_kanbanContainer, tablehub::lang::tr(tablehub::lang::Str::KDS_COL_PENDING), lv_color_hex(0xF59E0B));
    m_colPreparing = createColumn(m_kanbanContainer, tablehub::lang::tr(tablehub::lang::Str::KDS_COL_PREPARING), lv_color_hex(0x3B82F6));
    m_colReady = createColumn(m_kanbanContainer, tablehub::lang::tr(tablehub::lang::Str::KDS_COL_READY), lv_color_hex(0x10B981));

    return true;
}

void TableHubKdsView::onDestroy() {
    BaseView::onDestroy();
}

void TableHubKdsView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
}

void TableHubKdsView::onUpdate() {
}

} // namespace ui
} // namespace cbdos
