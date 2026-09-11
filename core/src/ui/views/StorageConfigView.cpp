#include "StorageConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/storage.hpp"
#include "cbdos/language.hpp"
#include <cstdio>
#include <string>

namespace cbdos {
namespace ui {

using cbdos::lang::tr;
using cbdos::lang::StrId;

StorageConfigView::StorageConfigView()
    : BaseView(tr(StrId::STR_CFG_STORAGE)) {
}

void StorageConfigView::mount_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        StorageConfigView* self = (StorageConfigView*)lv_event_get_user_data(e);
        if (cbdos::storage::mountSd()) {
            UIManager::showToast(tr(StrId::STR_ST_MOUNTED_OK));
        } else {
            UIManager::showToast(tr(StrId::STR_ST_NO_SD));
        }
        if (self) {
            self->onUpdate();
        }
    }
}

void StorageConfigView::unmount_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        StorageConfigView* self = (StorageConfigView*)lv_event_get_user_data(e);
        if (cbdos::storage::unmountSd()) {
            UIManager::showToast(tr(StrId::STR_ST_UNMOUNT_OK));
        } else {
            UIManager::showToast(tr(StrId::STR_ST_UNMOUNT_ERR));
        }
        if (self) {
            self->onUpdate();
        }
    }
}

void StorageConfigView::format_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        StorageConfigView* self = (StorageConfigView*)lv_event_get_user_data(e);
        if (self) {
            self->showFormatConfirmDialog();
        }
    }
}

void StorageConfigView::showFormatConfirmDialog() {
    lv_obj_t* mbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(mbox, tr(StrId::STR_ST_FMT_TITLE));
    lv_msgbox_add_text(mbox, tr(StrId::STR_ST_FMT_TEXT));

    lv_obj_t* btnConfirm = lv_msgbox_add_footer_button(mbox, tr(StrId::STR_ST_FMT_BTN));
    lv_obj_set_style_bg_color(btnConfirm, lv_color_hex(0xEF4444), 0);
    lv_obj_add_event_cb(btnConfirm, [](lv_event_t* e) {
        lv_obj_t* mb = (lv_obj_t*)lv_event_get_user_data(e);
        if (mb) lv_msgbox_close(mb);
        UIManager::showToast(tr(StrId::STR_ST_FORMATTING));
        if (cbdos::storage::formatSd()) {
            UIManager::showToast(tr(StrId::STR_ST_FMT_OK));
        } else {
            UIManager::showToast(tr(StrId::STR_ST_FMT_ERR));
        }
    }, LV_EVENT_CLICKED, mbox);

    lv_obj_t* btnCancel = lv_msgbox_add_footer_button(mbox, tr(StrId::STR_ST_CANCEL));
    lv_obj_add_event_cb(btnCancel, [](lv_event_t* e) {
        lv_obj_t* mb = (lv_obj_t*)lv_event_get_user_data(e);
        if (mb) lv_msgbox_close(mb);
    }, LV_EVENT_CLICKED, mbox);
}

bool StorageConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 24, 0);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    renderStorageUI();
    return true;
}

void StorageConfigView::renderStorageUI() {
    if (!m_container || !lv_obj_is_valid(m_container)) return;

    // Limpiar contenido previo para renderizar fresco
    lv_obj_clean(m_container);

    auto flashStats = cbdos::storage::getFlashStats();
    auto sdStats = cbdos::storage::getSdCardStats();

    // ─────────────────────────────────────────────────────────────
    // 1. Tarjeta: Memoria Flash Interna (SPI NOR)
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* flashCard = lv_obj_create(m_container);
    lv_obj_set_width(flashCard, lv_pct(100));
    DefaultTheme::applyRaisedCard(flashCard, 12);
    lv_obj_set_flex_flow(flashCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(flashCard, 14, 0);
    lv_obj_set_style_pad_row(flashCard, 8, 0);

    // Cabecera Flash
    lv_obj_t* flashHeader = lv_obj_create(flashCard);
    lv_obj_set_size(flashHeader, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(flashHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(flashHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(flashHeader, 0, 0);
    lv_obj_set_style_border_width(flashHeader, 0, 0);
    lv_obj_set_style_pad_all(flashHeader, 0, 0);

    lv_obj_t* flashTitle = lv_label_create(flashHeader);
    lv_label_set_text(flashTitle, (std::string(LV_SYMBOL_DRIVE " ") + tr(StrId::STR_ST_FLASH_TITLE)).c_str());
    lv_obj_set_style_text_color(flashTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(flashTitle, &lv_font_montserrat_14, 0);

    lv_obj_t* flashBadge = lv_label_create(flashHeader);
    lv_label_set_text(flashBadge, tr(StrId::STR_ST_SYSTEM));
    lv_obj_set_style_text_color(flashBadge, lv_color_hex(0x00FFCC), 0);
    lv_obj_set_style_text_font(flashBadge, &lv_font_montserrat_12, 0);

    // Barra de Progreso Flash
    m_flashBar = lv_bar_create(flashCard);
    lv_obj_set_size(m_flashBar, lv_pct(100), 8);
    lv_obj_set_style_bg_color(m_flashBar, lv_color_hex(0x1F2430), 0);
    lv_obj_set_style_bg_color(m_flashBar, lv_color_hex(0x00D2FF), LV_PART_INDICATOR);
    int flashPct = (flashStats.totalBytes > 0) ? (int)((flashStats.usedBytes * 100) / flashStats.totalBytes) : 0;
    lv_bar_set_value(m_flashBar, flashPct, LV_ANIM_OFF);

    // Detalles Flash
    char flashBuf[96];
    snprintf(flashBuf, sizeof(flashBuf), tr(StrId::STR_ST_USED_FMT),
             (float)flashStats.usedBytes / (1024.0f * 1024.0f),
             (float)flashStats.totalBytes / (1024.0f * 1024.0f),
             (float)flashPct);
    m_flashCapacityLabel = lv_label_create(flashCard);
    lv_label_set_text(m_flashCapacityLabel, flashBuf);
    lv_obj_set_style_text_color(m_flashCapacityLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_flashCapacityLabel, &lv_font_montserrat_12, 0);

    // ─────────────────────────────────────────────────────────────
    // 2. Tarjeta: Tarjeta MicroSD (SDMMC Slot 0)
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* sdCard = lv_obj_create(m_container);
    lv_obj_set_width(sdCard, lv_pct(100));
    DefaultTheme::applyRaisedCard(sdCard, 12);
    lv_obj_set_flex_flow(sdCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sdCard, 14, 0);
    lv_obj_set_style_pad_row(sdCard, 10, 0);

    // Cabecera MicroSD
    lv_obj_t* sdHeader = lv_obj_create(sdCard);
    lv_obj_set_size(sdHeader, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sdHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sdHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(sdHeader, 0, 0);
    lv_obj_set_style_border_width(sdHeader, 0, 0);
    lv_obj_set_style_pad_all(sdHeader, 0, 0);

    lv_obj_t* sdTitle = lv_label_create(sdHeader);
    lv_label_set_text(sdTitle, (std::string(LV_SYMBOL_SD_CARD " ") + tr(StrId::STR_ST_SD_TITLE)).c_str());
    lv_obj_set_style_text_color(sdTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(sdTitle, &lv_font_montserrat_14, 0);

    m_sdCardStatusLabel = lv_label_create(sdHeader);
    if (sdStats.isMounted) {
        lv_label_set_text(m_sdCardStatusLabel, tr(StrId::STR_ST_MOUNTED));
        lv_obj_set_style_text_color(m_sdCardStatusLabel, lv_color_hex(0x00FF88), 0);
    } else {
        lv_label_set_text(m_sdCardStatusLabel, tr(StrId::STR_ST_NOTFOUND));
        lv_obj_set_style_text_color(m_sdCardStatusLabel, lv_color_hex(0xFF5555), 0);
    }
    lv_obj_set_style_text_font(m_sdCardStatusLabel, &lv_font_montserrat_12, 0);

    if (sdStats.isMounted && sdStats.totalBytes > 0) {
        // Barra de Progreso MicroSD
        m_sdCardBar = lv_bar_create(sdCard);
        lv_obj_set_size(m_sdCardBar, lv_pct(100), 8);
        lv_obj_set_style_bg_color(m_sdCardBar, lv_color_hex(0x1F2430), 0);
        lv_obj_set_style_bg_color(m_sdCardBar, lv_color_hex(0x00FF88), LV_PART_INDICATOR);
        int sdPct = (int)((sdStats.usedBytes * 100) / sdStats.totalBytes);
        lv_bar_set_value(m_sdCardBar, sdPct, LV_ANIM_OFF);

        // Capacidad MicroSD en MB o GB
        char sdBuf[96];
        float totalGB = (float)sdStats.totalBytes / (1024.0f * 1024.0f * 1024.0f);
        float freeGB = (float)sdStats.freeBytes / (1024.0f * 1024.0f * 1024.0f);
        if (totalGB >= 1.0f) {
            snprintf(sdBuf, sizeof(sdBuf), tr(StrId::STR_ST_FREE_GB), freeGB, totalGB);
        } else {
            snprintf(sdBuf, sizeof(sdBuf), tr(StrId::STR_ST_FREE_MB),
                     (float)sdStats.freeBytes / (1024.0f * 1024.0f),
                     (float)sdStats.totalBytes / (1024.0f * 1024.0f));
        }
        m_sdCardCapacityLabel = lv_label_create(sdCard);
        lv_label_set_text(m_sdCardCapacityLabel, sdBuf);
        lv_obj_set_style_text_color(m_sdCardCapacityLabel, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(m_sdCardCapacityLabel, &lv_font_montserrat_12, 0);
    } else {
        m_sdCardCapacityLabel = lv_label_create(sdCard);
        lv_label_set_text(m_sdCardCapacityLabel, tr(StrId::STR_ST_INSERT));
        lv_obj_set_style_text_color(m_sdCardCapacityLabel, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(m_sdCardCapacityLabel, &lv_font_montserrat_12, 0);
    }

    // Fila de Botones de Acción para MicroSD
    lv_obj_t* btnRow = lv_obj_create(sdCard);
    lv_obj_set_size(btnRow, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_style_pad_column(btnRow, 8, 0);

    // Botón Recargar / Montar
    lv_obj_t* mountBtn = lv_button_create(btnRow);
    lv_obj_set_flex_grow(mountBtn, 1);
    lv_obj_set_height(mountBtn, 42);
    DefaultTheme::applyButton(mountBtn, 10);
    lv_obj_set_style_bg_color(mountBtn, lv_color_hex(0x0078D7), 0);
    lv_obj_add_event_cb(mountBtn, mount_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* mountLbl = lv_label_create(mountBtn);
    lv_label_set_text(mountLbl, (std::string(LV_SYMBOL_REFRESH " ") + tr(StrId::STR_ST_RELOAD)).c_str());
    lv_obj_center(mountLbl);
    lv_obj_set_style_text_color(mountLbl, lv_color_hex(0xFFFFFF), 0);

    // Botón Expulsar / Desmontar
    lv_obj_t* unmountBtn = lv_button_create(btnRow);
    lv_obj_set_flex_grow(unmountBtn, 1);
    lv_obj_set_height(unmountBtn, 42);
    DefaultTheme::applyButton(unmountBtn, 10);
    lv_obj_set_style_bg_color(unmountBtn, lv_color_hex(0x2A2E39), 0);
    lv_obj_add_event_cb(unmountBtn, unmount_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* unmountLbl = lv_label_create(unmountBtn);
    lv_label_set_text(unmountLbl, (std::string(LV_SYMBOL_EJECT " ") + tr(StrId::STR_ST_EJECT)).c_str());
    lv_obj_center(unmountLbl);
    lv_obj_set_style_text_color(unmountLbl, lv_color_hex(0xCCCCCC), 0);

    // Botón Formatear FAT32
    lv_obj_t* formatBtn = lv_button_create(btnRow);
    lv_obj_set_flex_grow(formatBtn, 1);
    lv_obj_set_height(formatBtn, 42);
    DefaultTheme::applyButton(formatBtn, 10);
    lv_obj_set_style_bg_color(formatBtn, lv_color_hex(0x7F1D1D), 0); // Rojo oscuro sutil
    lv_obj_add_event_cb(formatBtn, format_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* formatLbl = lv_label_create(formatBtn);
    lv_label_set_text(formatLbl, (std::string(LV_SYMBOL_TRASH " ") + tr(StrId::STR_ST_FORMAT)).c_str());
    lv_obj_center(formatLbl);
    lv_obj_set_style_text_color(formatLbl, lv_color_hex(0xFCA5A5), 0);

    // ─────────────────────────────────────────────────────────────
    // 3. Tarjeta: Puerto USB Host (OTG / High-Speed)
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* usbCard = lv_obj_create(m_container);
    lv_obj_set_width(usbCard, lv_pct(100));
    DefaultTheme::applyRaisedCard(usbCard, 12);
    lv_obj_set_flex_flow(usbCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(usbCard, 14, 0);
    lv_obj_set_style_pad_row(usbCard, 6, 0);

    lv_obj_t* usbHeader = lv_obj_create(usbCard);
    lv_obj_set_size(usbHeader, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(usbHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(usbHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(usbHeader, 0, 0);
    lv_obj_set_style_border_width(usbHeader, 0, 0);
    lv_obj_set_style_pad_all(usbHeader, 0, 0);

    lv_obj_t* usbTitle = lv_label_create(usbHeader);
    lv_label_set_text(usbTitle, (std::string(LV_SYMBOL_USB " ") + tr(StrId::STR_ST_USB_TITLE)).c_str());
    lv_obj_set_style_text_color(usbTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(usbTitle, &lv_font_montserrat_14, 0);

    lv_obj_t* usbBadge = lv_label_create(usbHeader);
    lv_label_set_text(usbBadge, tr(StrId::STR_ST_STANDBY));
    lv_obj_set_style_text_color(usbBadge, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(usbBadge, &lv_font_montserrat_12, 0);

    lv_obj_t* usbSub = lv_label_create(usbCard);
    lv_label_set_text(usbSub, tr(StrId::STR_ST_USB_SUB));
    lv_obj_set_style_text_color(usbSub, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(usbSub, &lv_font_montserrat_12, 0);
}

void StorageConfigView::onUpdate() {
    renderStorageUI();
}

} // namespace ui
} // namespace cbdos
