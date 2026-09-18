// ==========================================================================
// CartridgeView.cpp — Gestor y Lanzador Multi-Slot de Cartuchos con MicroSD
// Adaptado a la arquitectura agnóstica de CBDos v0.2.0 desde espOS32
// ==========================================================================

#include "CartridgeView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/cartridge.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"

#include "cbdos/rtos.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace ui {

CartridgeView* CartridgeView::s_instance = nullptr;

CartridgeView::CartridgeView()
    : BaseView("Cartuchos") {
    s_instance = this;
}

bool CartridgeView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;
    s_instance = this;

    // Contenedor principal con scroll vertical suave (Fondo transparente para ver el Wallpaper)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_style_pad_row(m_container, 14, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // Contenedor de ranuras
    m_slotsContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_slotsContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_slotsContainer, 1);
    lv_obj_set_style_bg_opa(m_slotsContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_slotsContainer, 0, 0);
    lv_obj_set_style_pad_all(m_slotsContainer, 0, 0);
    lv_obj_set_style_pad_row(m_slotsContainer, 14, 0);
    lv_obj_set_flex_flow(m_slotsContainer, LV_FLEX_FLOW_COLUMN);

    refreshSlots();
    return true;
}

void CartridgeView::onDestroy() {
    closeCurrentModal();
    m_slotsContainer = nullptr;
    if (s_instance == this) {
        s_instance = nullptr;
    }
    BaseView::onDestroy();
}

void CartridgeView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

void CartridgeView::refreshSlots() {
    if (!m_slotsContainer || !lv_obj_is_valid(m_slotsContainer)) return;

    lv_obj_clean(m_slotsContainer);

    // 1. Ranura Grande (app1 - 4.0 MB, ej. DOOM)
    createSlotCard(m_slotsContainer, ESP_PARTITION_SUBTYPE_APP_OTA_1,
                   "RANURA 1", "4.0 MB", 0x1A1F36, 0x3F68D9);

    // 2. Ranura Pequeña (app2 - 2.0 MB, ej. Game Boy Color)
    createSlotCard(m_slotsContainer, ESP_PARTITION_SUBTYPE_APP_OTA_2,
                   "RANURA 2", "2.0 MB", 0x142826, 0x1DB89C);
}

void CartridgeView::createSlotCard(lv_obj_t* parent, esp_partition_subtype_t subtype, 
                                  const char* slotTitle, const char* capacityStr, 
                                  uint32_t bgHex, uint32_t borderHex) {
    cbdos::cartridge::CartridgeSlotInfo info = cbdos::cartridge::CartridgeManager::getSlotInfo(subtype);

    char capBuf[16];
    const char* displayCap = capacityStr;
    if (info.partitionSize > 0) {
        float mb = (float)info.partitionSize / (1024.0f * 1024.0f);
        snprintf(capBuf, sizeof(capBuf), "%.1f MB", mb);
        displayCap = capBuf;
    }

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 165);
    lv_obj_set_style_bg_color(card, lv_color_hex(bgHex), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(borderHex), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    DefaultTheme::disableScroll(card);

    // Cabecera de la Ranura
    lv_obj_t* tag = lv_label_create(card);
    char headerBuf[64];
    snprintf(headerBuf, sizeof(headerBuf), "%s  [%s]", slotTitle, displayCap);
    lv_label_set_text(tag, headerBuf);
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tag, lv_color_hex(borderHex), 0);
    lv_obj_align(tag, LV_ALIGN_TOP_LEFT, 0, 0);

    // Nombre del Cartucho instalado
    lv_obj_t* nameLbl = lv_label_create(card);
    lv_label_set_text(nameLbl, info.projectName.c_str());
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(nameLbl, info.isInstalled ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x888888), 0);
    lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 0, 22);

    // Detalle de versión / estado
    lv_obj_t* detailLbl = lv_label_create(card);
    if (info.isInstalled) {
        char detailBuf[64];
        snprintf(detailBuf, sizeof(detailBuf), "v%s • %s", 
                 info.version.empty() ? "1.0" : info.version.c_str(), 
                 info.compileDate.empty() ? "Instalado" : info.compileDate.c_str());
        lv_label_set_text(detailLbl, detailBuf);
        lv_obj_set_style_text_color(detailLbl, lv_color_hex(0x00FF88), 0);
    } else {
        lv_label_set_text(detailLbl, "Disponible para instalar .bin");
        lv_obj_set_style_text_color(detailLbl, lv_color_hex(0xAAAAAA), 0);
    }
    lv_obj_set_style_text_font(detailLbl, &lv_font_montserrat_12, 0);
    lv_obj_align(detailLbl, LV_ALIGN_TOP_LEFT, 0, 48);

    // Fila de Botones
    lv_obj_t* btnRow = lv_obj_create(card);
    lv_obj_set_size(btnRow, LV_PCT(100), 50);
    lv_obj_align(btnRow, LV_ALIGN_BOTTOM_MID, 0, 5);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Iniciar
    lv_obj_t* runBtn = lv_button_create(btnRow);
    lv_obj_set_width(runBtn, LV_PCT(48));
    lv_obj_set_height(runBtn, 42);
    lv_obj_set_style_bg_color(runBtn, info.isInstalled ? lv_color_hex(borderHex) : lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_radius(runBtn, 10, 0);
    lv_obj_add_event_cb(runBtn, bootSlotCb, LV_EVENT_CLICKED, (void*)(intptr_t)subtype);

    lv_obj_t* runLbl = lv_label_create(runBtn);
    lv_label_set_text(runLbl, LV_SYMBOL_PLAY " Iniciar");
    lv_obj_set_style_text_font(runLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(runLbl, info.isInstalled ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x666666), 0);
    lv_obj_center(runLbl);

    // Botón Instalar SD
    lv_obj_t* instBtn = lv_button_create(btnRow);
    lv_obj_set_width(instBtn, LV_PCT(48));
    lv_obj_set_height(instBtn, 42);
    lv_obj_set_style_bg_color(instBtn, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_width(instBtn, 1, 0);
    lv_obj_set_style_border_color(instBtn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_radius(instBtn, 10, 0);
    lv_obj_add_event_cb(instBtn, openInstallModalCb, LV_EVENT_CLICKED, (void*)(intptr_t)subtype);

    lv_obj_t* instLbl = lv_label_create(instBtn);
    lv_label_set_text(instLbl, LV_SYMBOL_SD_CARD " Instalar");
    lv_obj_set_style_text_font(instLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instLbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_center(instLbl);
}

void CartridgeView::showSDPickerDialog(esp_partition_subtype_t targetSlot) {
    closeCurrentModal();
    m_selectedSlot = targetSlot;

    lv_obj_t* screen = lv_screen_active();
    if (!screen) return;

    // Fondo Oscuro de Modal
    m_modalBg = lv_obj_create(screen);
    lv_obj_set_size(m_modalBg, LV_PCT(100), LV_PCT(100));
    lv_obj_center(m_modalBg);
    lv_obj_set_style_bg_color(m_modalBg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalBg, LV_OPA_70, 0);
    lv_obj_set_style_border_width(m_modalBg, 0, 0);

    // Caja de Diálogo
    lv_obj_t* dialog = lv_obj_create(m_modalBg);
    lv_obj_set_size(dialog, 300, 340);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(0x3F68D9), 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_radius(dialog, 14, 0);
    lv_obj_set_style_pad_all(dialog, 12, 0);

    // Título
    lv_obj_t* title = lv_label_create(dialog);
    lv_label_set_text(title, "Seleccionar Cartucho .BIN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    // Botón Cerrar
    lv_obj_t* closeBtn = lv_button_create(dialog);
    lv_obj_set_size(closeBtn, 30, 30);
    lv_obj_align(closeBtn, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_style_bg_color(closeBtn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(closeBtn, 15, 0);
    lv_obj_add_event_cb(closeBtn, closeModalCb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* closeIcon = lv_label_create(closeBtn);
    lv_label_set_text(closeIcon, LV_SYMBOL_CLOSE);
    lv_obj_center(closeIcon);

    // Lista de Archivos
    lv_obj_t* list = lv_obj_create(dialog);
    lv_obj_set_size(list, LV_PCT(100), 260);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    m_binFiles = cbdos::cartridge::CartridgeManager::listBinFilesOnSD();

    if (m_binFiles.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(list);
        lv_label_set_text(emptyLbl, "No se encontraron archivos .bin\n\nColoca tus cartuchos en:\n/cartridges/ o en la raiz de la SD.");
        lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(emptyLbl, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(emptyLbl, LV_ALIGN_CENTER, 0, 40);
        return;
    }

    for (size_t i = 0; i < m_binFiles.size(); i++) {
        const std::string& file = m_binFiles[i];
        size_t slashIdx = file.rfind('/');
        std::string fName = (slashIdx != std::string::npos) ? file.substr(slashIdx + 1) : file;

        lv_obj_t* itemBtn = lv_button_create(list);
        lv_obj_set_width(itemBtn, LV_PCT(100));
        lv_obj_set_height(itemBtn, 48);
        lv_obj_set_style_bg_color(itemBtn, lv_color_hex(0x282828), 0);
        lv_obj_set_style_radius(itemBtn, 8, 0);
        lv_obj_set_style_border_width(itemBtn, 1, 0);
        lv_obj_set_style_border_color(itemBtn, lv_color_hex(0x404040), 0);

        lv_obj_t* itemIcon = lv_label_create(itemBtn);
        lv_label_set_text(itemIcon, LV_SYMBOL_FILE);
        lv_obj_set_style_text_color(itemIcon, lv_color_hex(0x00E5FF), 0);
        lv_obj_align(itemIcon, LV_ALIGN_LEFT_MID, 6, 0);

        lv_obj_t* itemLbl = lv_label_create(itemBtn);
        lv_label_set_text(itemLbl, fName.c_str());
        lv_obj_set_style_text_font(itemLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(itemLbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(itemLbl, LV_ALIGN_LEFT_MID, 30, 0);

        lv_obj_add_event_cb(itemBtn, fileSelectedCb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

void CartridgeView::closeCurrentModal() {
    if (m_modalBg && lv_obj_is_valid(m_modalBg)) {
        lv_obj_delete(m_modalBg);
        m_modalBg = nullptr;
    }
}

void CartridgeView::startFlashing(const std::string& binPath, esp_partition_subtype_t targetSlot) {
    closeCurrentModal();

    lv_obj_t* screen = lv_screen_active();
    if (!screen) return;

    // Modal de Progreso de Flasheo (Exacto como en espOS32)
    m_modalBg = lv_obj_create(screen);
    lv_obj_set_size(m_modalBg, 280, 190);
    lv_obj_center(m_modalBg);
    lv_obj_set_style_bg_color(m_modalBg, lv_color_hex(0x1F1F1F), 0);
    lv_obj_set_style_border_color(m_modalBg, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(m_modalBg, 2, 0);
    lv_obj_set_style_radius(m_modalBg, 14, 0);
    DefaultTheme::disableScroll(m_modalBg);

    lv_obj_t* pTitle = lv_label_create(m_modalBg);
    lv_label_set_text(pTitle, "Flasheando Cartucho");
    lv_obj_set_style_text_font(pTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pTitle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(pTitle, LV_ALIGN_TOP_MID, 0, 10);

    size_t lastSlash = binPath.rfind('/');
    std::string fileName = (lastSlash != std::string::npos) ? binPath.substr(lastSlash + 1) : binPath;

    lv_obj_t* pSub = lv_label_create(m_modalBg);
    lv_label_set_text(pSub, fileName.c_str());
    lv_obj_set_style_text_font(pSub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pSub, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(pSub, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t* pBar = lv_bar_create(m_modalBg);
    lv_obj_set_size(pBar, 230, 18);
    lv_obj_align(pBar, LV_ALIGN_CENTER, 0, 5);
    lv_bar_set_range(pBar, 0, 100);
    lv_bar_set_value(pBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(pBar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(pBar, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(pBar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(pBar, 6, LV_PART_INDICATOR);

    lv_obj_t* pPercent = lv_label_create(m_modalBg);
    lv_label_set_text(pPercent, "0 %");
    lv_obj_set_style_text_font(pPercent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pPercent, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(pPercent, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_timer_handler();

    struct FlashContext {
        CartridgeView* view;
        std::string binPath;
        esp_partition_subtype_t targetSlot;
        lv_obj_t* bar;
        lv_obj_t* percent;
    };

    cbdos::rtos::createTask(
        [](void* param) {
            auto* ctx = static_cast<FlashContext*>(param);
            
            bool ok = cbdos::cartridge::CartridgeManager::flashFromSD(
                ctx->binPath, 
                ctx->targetSlot, 
                [ctx](size_t written, size_t total) {
                    if (total > 0 && cbdos::display::lock(50)) {
                        int pct = (int)((written * 100) / total);
                        if (ctx->bar && lv_obj_is_valid(ctx->bar)) {
                            lv_bar_set_value(ctx->bar, pct, LV_ANIM_OFF);
                        }
                        if (ctx->percent && lv_obj_is_valid(ctx->percent)) {
                            char pctBuf[16];
                            snprintf(pctBuf, sizeof(pctBuf), "%d %%", pct);
                            lv_label_set_text(ctx->percent, pctBuf);
                        }
                        cbdos::display::unlock();
                    }
                }
            );

            if (cbdos::display::lock(500)) {
                if (ctx->view && ctx->view == CartridgeView::getInstance()) {
                    ctx->view->closeCurrentModal();
                    if (ok) {
                        UIManager::getInstance().showToast("Instalacion exitosa!");
                    } else {
                        std::string err = cbdos::cartridge::CartridgeManager::getLastError();
                        if (err.empty()) err = "Fallo al escribir en Flash";
                        UIManager::getInstance().showToast(err.c_str());
                    }
                    ctx->view->refreshSlots();
                }
                cbdos::display::unlock();
            }
            delete ctx;
            cbdos::rtos::deleteTask(nullptr);
        },
        "cart_flash_task",
        8192,
        new FlashContext{this, binPath, targetSlot, pBar, pPercent},
        3,
        0 // Core 0
    );
}

void CartridgeView::bootSlotCb(lv_event_t* e) {
    esp_partition_subtype_t subtype = (esp_partition_subtype_t)(intptr_t)lv_event_get_user_data(e);

    if (!cbdos::cartridge::CartridgeManager::isSlotInstalled(subtype)) {
        UIManager::getInstance().showToast("Ranura vacia. Instala un .bin primero");
        return;
    }

    cbdos::cartridge::CartridgeSlotInfo info = cbdos::cartridge::CartridgeManager::getSlotInfo(subtype);
    char buf[64];
    snprintf(buf, sizeof(buf), "Iniciando %s...", info.projectName.c_str());
    UIManager::getInstance().showToast(buf);

    cbdos::system::sleepMs(500);
    cbdos::cartridge::CartridgeManager::bootSlot(subtype);
}

void CartridgeView::openInstallModalCb(lv_event_t* e) {
    esp_partition_subtype_t subtype = (esp_partition_subtype_t)(intptr_t)lv_event_get_user_data(e);
    if (s_instance) {
        s_instance->showSDPickerDialog(subtype);
    }
}

void CartridgeView::fileSelectedCb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (s_instance && idx >= 0 && idx < (int)s_instance->m_binFiles.size()) {
        std::string binPath = s_instance->m_binFiles[idx];
        esp_partition_subtype_t targetSlot = s_instance->m_selectedSlot;
        s_instance->startFlashing(binPath, targetSlot);
    }
}

void CartridgeView::closeModalCb(lv_event_t* e) {
    (void)e;
    if (s_instance) {
        s_instance->closeCurrentModal();
    }
}

} // namespace ui
} // namespace cbdos
