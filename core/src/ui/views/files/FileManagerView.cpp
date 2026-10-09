#include "FileManagerView.hpp"
#include "GalleryView.hpp"
#include "TextEditorView.hpp"
#include "LuaRunnerView.hpp"
#include "MusicPlayerView.hpp"
#include "CartridgeView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/language.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

static std::string formatBytes(uint64_t bytes) {
    char buf[32];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f KB", (float)bytes / 1024.0f);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f MB", (float)bytes / (1024.0f * 1024.0f));
    } else {
        snprintf(buf, sizeof(buf), "%.2f GB", (float)bytes / (1024.0f * 1024.0f * 1024.0f));
    }
    return std::string(buf);
}

static void getFileIconAndColor(const std::string& name, bool isDir, const char** outIcon, uint32_t* outColor) {
    if (isDir) {
        *outIcon = LV_SYMBOL_DIRECTORY;
        *outColor = 0xFFB800; // Amarillo / Ámbar
        return;
    }

    std::string ext = "";
    size_t dotPos = name.rfind('.');
    if (dotPos != std::string::npos) {
        ext = name.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".mp3" || ext == ".wav") {
        *outIcon = LV_SYMBOL_AUDIO;
        *outColor = 0x70E000; // Verde Lima
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
        *outIcon = LV_SYMBOL_IMAGE;
        *outColor = 0xFF2E93; // Rosa / Fucsia
    } else if (ext == ".gbc" || ext == ".gb" || ext == ".wad") {
        *outIcon = LV_SYMBOL_PLAY;
        *outColor = 0xE50000; // Rojo
    } else if (ext == ".lua") {
        *outIcon = LV_SYMBOL_FILE;
        *outColor = 0x06B6D4; // Celeste / Cyan
    } else if (ext == ".txt" || ext == ".json" || ext == ".enc" || ext == ".log" || ext == ".csv" || ext == ".md" || ext == ".c" || ext == ".h" || ext == ".cpp") {
        *outIcon = LV_SYMBOL_FILE;
        *outColor = 0x00F5D4; // Turquesa
    } else {
        *outIcon = LV_SYMBOL_FILE;
        *outColor = 0x9D4EDD; // Violeta
    }
}

struct ItemContext {
    FileManagerView* view;
    cbdos::storage::FileEntry fileEntry;
};

struct DeleteModalContext {
    FileManagerView* view;
    cbdos::storage::FileEntry fileEntry;
    std::string fullPath;
    lv_obj_t* mask;
};

FileManagerView::FileManagerView(const std::string& initialPath)
    : BaseView("Explorador"),
      m_currentStorage(cbdos::storage::StorageType::SdCard),
      m_currentPath(initialPath.empty() ? "/" : initialPath) {
}

bool FileManagerView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Configurar cabecera
    UIManager::getInstance().getHeaderBar().setTitle("Explorador");
    UIManager::getInstance().getHeaderBar().showWifi(false);
    UIManager::getInstance().getHeaderBar().showBackButton(true, []() {
        UIManager::getInstance().popView();
    });
    UIManager::getInstance().getHeaderBar().setRightAction(LV_SYMBOL_REFRESH, [this]() {
        this->refreshCurrentView();
        UIManager::showToast("Directorio actualizado");
    });

    // Contenedor base
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 6, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(m_container);

    // 1. Selector de unidad (MicroSD / Flash)
    renderUnitSelector(m_container);

    // 2. Barra de ruta (Breadcrumb + Subir nivel + Refrescar)
    renderPathBar(m_container);

    // 3. Contenedor scrollable con la lista de archivos
    m_listContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_listContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_listContainer, 1);
    DefaultTheme::applyRaisedCard(m_listContainer, 12);
    lv_obj_set_flex_flow(m_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_listContainer, 6, 0);
    lv_obj_set_style_pad_row(m_listContainer, 6, 0);
    lv_obj_add_flag(m_listContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_listContainer, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_listContainer, LV_SCROLLBAR_MODE_ACTIVE);

    renderFileList(m_listContainer);
    updateUnitButtons();
    updateStorageInfo();

    return true;
}

void FileManagerView::onDestroy() {
    UIManager::getInstance().getHeaderBar().showWifi(true);
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }
    m_unitInfoLabel = nullptr;
    m_pathLabel = nullptr;
    m_btnUnitSD = nullptr;
    m_btnUnitFlash = nullptr;
    m_listContainer = nullptr;
    BaseView::onDestroy();
}

void FileManagerView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

void FileManagerView::renderUnitSelector(lv_obj_t* parent) {
    lv_obj_t* unitCard = lv_obj_create(parent);
    lv_obj_set_width(unitCard, LV_PCT(100));
    lv_obj_set_height(unitCard, LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(unitCard, 10);
    DefaultTheme::disableScroll(unitCard);
    lv_obj_set_flex_flow(unitCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(unitCard, 6, 0);
    lv_obj_set_style_pad_row(unitCard, 4, 0);

    // Fila de botones de unidad
    lv_obj_t* btnRow = lv_obj_create(unitCard);
    lv_obj_set_width(btnRow, LV_PCT(100));
    lv_obj_set_height(btnRow, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, 0, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    DefaultTheme::disableScroll(btnRow);

    // Botón MicroSD
    m_btnUnitSD = lv_button_create(btnRow);
    lv_obj_set_width(m_btnUnitSD, LV_PCT(48));
    lv_obj_set_height(m_btnUnitSD, 34);
    DefaultTheme::applyButton(m_btnUnitSD, 8);
    lv_obj_set_user_data(m_btnUnitSD, this);
    lv_obj_add_event_cb(m_btnUnitSD, unitSdClickCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblSD = lv_label_create(m_btnUnitSD);
    lv_label_set_text_fmt(lblSD, "%s %s", LV_SYMBOL_SD_CARD, cbdos::lang::tr(cbdos::lang::StrId::STR_TXT_SD));
    lv_obj_set_style_text_font(lblSD, &lv_font_montserrat_12, 0);
    lv_obj_center(lblSD);

    // Botón Flash Interna
    m_btnUnitFlash = lv_button_create(btnRow);
    lv_obj_set_width(m_btnUnitFlash, LV_PCT(48));
    lv_obj_set_height(m_btnUnitFlash, 34);
    DefaultTheme::applyButton(m_btnUnitFlash, 8);
    lv_obj_set_user_data(m_btnUnitFlash, this);
    lv_obj_add_event_cb(m_btnUnitFlash, unitFlashClickCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblFlash = lv_label_create(m_btnUnitFlash);
    lv_label_set_text_fmt(lblFlash, "%s %s", LV_SYMBOL_SAVE, cbdos::lang::tr(cbdos::lang::StrId::STR_TXT_FLASH));
    lv_obj_set_style_text_font(lblFlash, &lv_font_montserrat_12, 0);
    lv_obj_center(lblFlash);

    // Etiqueta de información de almacenamiento
    m_unitInfoLabel = lv_label_create(unitCard);
    lv_label_set_text(m_unitInfoLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_CALC));
    lv_obj_set_style_text_color(m_unitInfoLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_unitInfoLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_left(m_unitInfoLabel, 2, 0);
}

void FileManagerView::renderPathBar(lv_obj_t* parent) {
    lv_obj_t* pathBar = lv_obj_create(parent);
    lv_obj_set_width(pathBar, LV_PCT(100));
    lv_obj_set_height(pathBar, 36);
    DefaultTheme::applySunkenCard(pathBar, 8);
    DefaultTheme::disableScroll(pathBar);
    lv_obj_set_flex_flow(pathBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pathBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(pathBar, 2, 0);
    lv_obj_set_style_pad_column(pathBar, 6, 0);

    // Botón Subir nivel
    lv_obj_t* btnUp = lv_button_create(pathBar);
    lv_obj_set_size(btnUp, 32, 30);
    DefaultTheme::applyButton(btnUp, 6);
    lv_obj_add_event_cb(btnUp, btnUpClickCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblUp = lv_label_create(btnUp);
    lv_label_set_text(lblUp, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lblUp, &lv_font_montserrat_12, 0);
    lv_obj_center(lblUp);

    // Etiqueta de ruta
    m_pathLabel = lv_label_create(pathBar);
    lv_obj_set_flex_grow(m_pathLabel, 1);
    lv_label_set_long_mode(m_pathLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(m_pathLabel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_pathLabel, &lv_font_montserrat_12, 0);
    updatePathLabel();

    // Botón Modo Forense (Toggle)
    m_btnForensic = lv_button_create(pathBar);
    lv_obj_set_size(m_btnForensic, 32, 30);
    DefaultTheme::applyButton(m_btnForensic, 6);
    lv_obj_add_event_cb(m_btnForensic, btnForensicClickCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblForen = lv_label_create(m_btnForensic);
    lv_label_set_text(lblForen, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(lblForen, &lv_font_montserrat_12, 0);
    lv_obj_center(lblForen);
    updateForensicButton();

    // Botón Refrescar
    lv_obj_t* btnRef = lv_button_create(pathBar);
    lv_obj_set_size(btnRef, 32, 30);
    DefaultTheme::applyButton(btnRef, 6);
    lv_obj_add_event_cb(btnRef, btnRefreshClickCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblRef = lv_label_create(btnRef);
    lv_label_set_text(lblRef, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(lblRef, &lv_font_montserrat_12, 0);
    lv_obj_center(lblRef);
}

void FileManagerView::updateForensicButton() {
    if (!m_btnForensic || !lv_obj_is_valid(m_btnForensic)) return;

    if (m_forensicMode) {
        lv_obj_set_style_bg_color(m_btnForensic, lv_color_hex(0x5C2B09), 0);
        lv_obj_set_style_border_color(m_btnForensic, lv_color_hex(0xF59E0B), 0);
        lv_obj_set_style_border_width(m_btnForensic, 1, 0);
    } else {
        lv_obj_set_style_bg_color(m_btnForensic, lv_color_hex(0x1B1E29), 0);
        lv_obj_set_style_border_width(m_btnForensic, 0, 0);
    }
}

void FileManagerView::updatePathLabel() {
    if (!m_pathLabel || !lv_obj_is_valid(m_pathLabel)) return;
    std::string prefix = (m_currentStorage == cbdos::storage::StorageType::SdCard) ? "[SD] " : "[Flash] ";
    std::string display = prefix + m_currentPath;
    if (m_forensicMode) {
        display += " [REC-ON]";
    }
    lv_label_set_text(m_pathLabel, display.c_str());
}

void FileManagerView::updateUnitButtons() {
    if (!m_btnUnitSD || !m_btnUnitFlash || !lv_obj_is_valid(m_btnUnitSD) || !lv_obj_is_valid(m_btnUnitFlash)) return;

    if (m_currentStorage == cbdos::storage::StorageType::SdCard) {
        lv_obj_set_style_bg_color(m_btnUnitSD, lv_color_hex(0x242838), 0);
        lv_obj_set_style_border_color(m_btnUnitSD, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_border_width(m_btnUnitSD, 1, 0);

        lv_obj_set_style_bg_color(m_btnUnitFlash, lv_color_hex(0x1B1E29), 0);
        lv_obj_set_style_border_width(m_btnUnitFlash, 0, 0);
    } else {
        lv_obj_set_style_bg_color(m_btnUnitFlash, lv_color_hex(0x242838), 0);
        lv_obj_set_style_border_color(m_btnUnitFlash, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_set_style_border_width(m_btnUnitFlash, 1, 0);

        lv_obj_set_style_bg_color(m_btnUnitSD, lv_color_hex(0x1B1E29), 0);
        lv_obj_set_style_border_width(m_btnUnitSD, 0, 0);
    }
}

void FileManagerView::updateStorageInfo() {
    if (!m_unitInfoLabel || !lv_obj_is_valid(m_unitInfoLabel)) return;

    char infoBuf[128];
    if (m_currentStorage == cbdos::storage::StorageType::SdCard) {
        cbdos::storage::StorageStats sdStats = cbdos::storage::getSdCardStats();
        if (sdStats.isMounted) {
            snprintf(infoBuf, sizeof(infoBuf), "SD: %s libre de %s (%s usado)",
                     formatBytes(sdStats.freeBytes).c_str(),
                     formatBytes(sdStats.totalBytes).c_str(),
                     formatBytes(sdStats.usedBytes).c_str());
        } else {
            snprintf(infoBuf, sizeof(infoBuf), "MicroSD: No insertada o no montada");
        }
    } else {
        cbdos::storage::StorageStats flashStats = cbdos::storage::getFlashStats();
        snprintf(infoBuf, sizeof(infoBuf), "Flash: %s libre de %s (%s usado)",
                 formatBytes(flashStats.freeBytes).c_str(),
                 formatBytes(flashStats.totalBytes).c_str(),
                 formatBytes(flashStats.usedBytes).c_str());
    }
    lv_label_set_text(m_unitInfoLabel, infoBuf);
}

void FileManagerView::renderFileList(lv_obj_t* parent) {
    if (!parent || !lv_obj_is_valid(parent)) return;

    lv_obj_clean(parent);

    std::string queryPath;
    if (m_currentStorage == cbdos::storage::StorageType::SdCard) {
        if (m_currentPath.empty() || m_currentPath == "/") {
            queryPath = "/sdcard";
        } else if (m_currentPath.rfind("/sdcard", 0) != 0) {
            queryPath = "/sdcard" + (m_currentPath[0] == '/' ? m_currentPath : "/" + m_currentPath);
        } else {
            queryPath = m_currentPath;
        }
    } else {
        if (m_currentPath.empty() || m_currentPath == "/") {
            queryPath = "/spiffs";
        } else if (m_currentPath.rfind("/spiffs", 0) != 0) {
            queryPath = "/spiffs" + (m_currentPath[0] == '/' ? m_currentPath : "/" + m_currentPath);
        } else {
            queryPath = m_currentPath;
        }
    }

    std::vector<cbdos::storage::FileEntry> files = cbdos::storage::listDir(queryPath.c_str(), m_forensicMode);

    // Ordenar: primero directorios, luego archivos alfabéticamente
    std::sort(files.begin(), files.end(), [](const cbdos::storage::FileEntry& a, const cbdos::storage::FileEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name < b.name;
    });

    if (files.empty()) {
        lv_obj_t* emptyLabel = lv_label_create(parent);
        lv_label_set_text(emptyLabel, m_forensicMode ? cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_EMPTY_DEL) : cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_EMPTY));
        lv_obj_set_style_text_color(emptyLabel, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(emptyLabel, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(emptyLabel, 20, 0);
        lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    for (const auto& item : files) {
        lv_obj_t* itemRow = lv_obj_create(parent);
        lv_obj_set_width(itemRow, LV_PCT(100));
        lv_obj_set_height(itemRow, 46);
        DefaultTheme::applySunkenCard(itemRow, 8);
        DefaultTheme::disableScroll(itemRow);
        lv_obj_set_flex_flow(itemRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(itemRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(itemRow, 4, 0);
        lv_obj_set_style_pad_column(itemRow, 8, 0);

        if (item.isDeleted) {
            lv_obj_set_style_bg_color(itemRow, lv_color_hex(0x2E1B11), 0);
            lv_obj_set_style_border_color(itemRow, lv_color_hex(0xF59E0B), 0);
            lv_obj_set_style_border_width(itemRow, 1, 0);
        }

        // Icono según tipo
        const char* icon = LV_SYMBOL_FILE;
        uint32_t iconColor = 0xFFFFFF;
        if (item.isDeleted) {
            icon = LV_SYMBOL_TRASH;
            iconColor = 0xF59E0B; // Ámbar forense
        } else {
            getFileIconAndColor(item.name, item.isDirectory, &icon, &iconColor);
        }

        lv_obj_t* iconLbl = lv_label_create(itemRow);
        lv_label_set_text(iconLbl, icon);
        lv_obj_set_style_text_color(iconLbl, lv_color_hex(iconColor), 0);
        lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_16, 0);

        // Contenedor de Textos (Nombre + Tamaño/Info)
        lv_obj_t* textCont = lv_obj_create(itemRow);
        lv_obj_set_flex_grow(textCont, 1);
        lv_obj_set_height(textCont, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(textCont, 0, 0);
        lv_obj_set_style_border_width(textCont, 0, 0);
        lv_obj_set_flex_flow(textCont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(textCont, 0, 0);
        lv_obj_set_style_pad_row(textCont, 2, 0);
        DefaultTheme::disableScroll(textCont);

        lv_obj_t* nameLbl = lv_label_create(textCont);
        std::string displayName = item.isDeleted ? ("[REC] " + item.name) : item.name;
        lv_label_set_text(nameLbl, displayName.c_str());
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(nameLbl, item.isDeleted ? lv_color_hex(0xF59E0B) : DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_12, 0);

        lv_obj_t* sizeLbl = lv_label_create(textCont);
        if (item.isDirectory) {
            lv_label_set_text(sizeLbl, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_DIR));
        } else if (item.isDeleted) {
            char recBuf[64];
            snprintf(recBuf, sizeof(recBuf), "%s (Borrador / Recuperable)", formatBytes(item.size).c_str());
            lv_label_set_text(sizeLbl, recBuf);
        } else {
            lv_label_set_text(sizeLbl, formatBytes(item.size).c_str());
        }
        lv_obj_set_style_text_color(sizeLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(sizeLbl, &lv_font_montserrat_12, 0);

        // Contexto para clicks
        ItemContext* ctx = new ItemContext{this, item};
        lv_obj_add_event_cb(itemRow, [](lv_event_t* e) {
            ItemContext* pCtx = (ItemContext*)lv_event_get_user_data(e);
            delete pCtx;
        }, LV_EVENT_DELETE, ctx);

        // Click en la fila para abrir
        lv_obj_add_flag(itemRow, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(itemRow, itemClickCb, LV_EVENT_CLICKED, ctx);

        if (!item.isDeleted) {
            // Botón Eliminar (Papelera)
            lv_obj_t* delBtn = lv_button_create(itemRow);
            lv_obj_set_size(delBtn, 32, 32);
            DefaultTheme::applyButton(delBtn, 6);
            lv_obj_set_style_bg_color(delBtn, lv_color_hex(0x3B1D22), 0);
            lv_obj_set_style_border_color(delBtn, lv_color_hex(0xFF453A), 0);
            lv_obj_set_style_border_width(delBtn, 1, 0);

            lv_obj_t* delIcon = lv_label_create(delBtn);
            lv_label_set_text(delIcon, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_color(delIcon, lv_color_hex(0xFF453A), 0);
            lv_obj_set_style_text_font(delIcon, &lv_font_montserrat_12, 0);
            lv_obj_center(delIcon);

            lv_obj_add_event_cb(delBtn, itemDeleteClickCb, LV_EVENT_CLICKED, ctx);
        }
    }
}

void FileManagerView::refreshCurrentView() {
    if (m_listContainer && lv_obj_is_valid(m_listContainer)) {
        renderFileList(m_listContainer);
    }
    updatePathLabel();
    updateStorageInfo();
    updateForensicButton();
}

void FileManagerView::btnForensicClickCb(lv_event_t* e) {
    FileManagerView* self = static_cast<FileManagerView*>(lv_event_get_user_data(e));
    if (self) {
        self->m_forensicMode = !self->m_forensicMode;
        self->refreshCurrentView();
        UIManager::showToast(self->m_forensicMode ? "Modo Forense: Escaneo activado" : "Modo Normal: Vista limpia");
    }
}

void FileManagerView::unitSdClickCb(lv_event_t* e) {
    FileManagerView* self = static_cast<FileManagerView*>(lv_event_get_user_data(e));
    if (self && self->m_currentStorage != cbdos::storage::StorageType::SdCard) {
        self->m_currentStorage = cbdos::storage::StorageType::SdCard;
        self->m_currentPath = "/";
        self->updateUnitButtons();
        self->refreshCurrentView();
    }
}

void FileManagerView::unitFlashClickCb(lv_event_t* e) {
    FileManagerView* self = static_cast<FileManagerView*>(lv_event_get_user_data(e));
    if (self && self->m_currentStorage != cbdos::storage::StorageType::InternalFlash) {
        self->m_currentStorage = cbdos::storage::StorageType::InternalFlash;
        self->m_currentPath = "/";
        self->updateUnitButtons();
        self->refreshCurrentView();
    }
}

void FileManagerView::btnUpClickCb(lv_event_t* e) {
    FileManagerView* self = static_cast<FileManagerView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_currentPath == "/" || self->m_currentPath.empty()) {
        UIManager::showToast("Ya estas en el directorio raiz");
        return;
    }

    size_t lastSlash = self->m_currentPath.rfind('/');
    if (lastSlash == 0 || lastSlash == std::string::npos) {
        self->m_currentPath = "/";
    } else {
        self->m_currentPath = self->m_currentPath.substr(0, lastSlash);
    }
    self->refreshCurrentView();
}

void FileManagerView::btnRefreshClickCb(lv_event_t* e) {
    FileManagerView* self = static_cast<FileManagerView*>(lv_event_get_user_data(e));
    if (self) {
        self->refreshCurrentView();
        UIManager::showToast("Directorio actualizado");
    }
}

void FileManagerView::itemClickCb(lv_event_t* e) {
    ItemContext* ctx = static_cast<ItemContext*>(lv_event_get_user_data(e));
    if (!ctx || !ctx->view) return;

    FileManagerView* self = ctx->view;
    const auto& item = ctx->fileEntry;

    if (item.isDeleted) {
        self->showRecoveryModal(item);
        return;
    }

    if (item.isDirectory) {
        if (self->m_currentPath == "/") {
            self->m_currentPath = "/" + item.name;
        } else {
            self->m_currentPath = self->m_currentPath + "/" + item.name;
        }
        self->refreshCurrentView();
    } else {
        std::string fullFilePath;
        std::string unitPrefix = (self->m_currentStorage == cbdos::storage::StorageType::SdCard) ? "/sdcard" : "/spiffs";
        if (self->m_currentPath == "/") {
            fullFilePath = unitPrefix + "/" + item.name;
        } else {
            fullFilePath = unitPrefix + self->m_currentPath + "/" + item.name;
        }

        // Detectar tipo y realizar acción contextual
        std::string ext = "";
        size_t dotPos = item.name.rfind('.');
        if (dotPos != std::string::npos) {
            ext = item.name.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        if (ext == ".jpg" || ext == ".jpeg" || ext == ".bin" || ext == ".raw") {
            if (self->m_currentStorage == cbdos::storage::StorageType::SdCard) {
                std::string relPath = (self->m_currentPath == "/") ? ("/" + item.name) : (self->m_currentPath + "/" + item.name);
                std::string lvglPath = "A:" + relPath;
                UIManager::getInstance().pushView(std::make_shared<GalleryView>(lvglPath, item.name));
            } else {
                std::string lvglPath = fullFilePath;
                UIManager::getInstance().pushView(std::make_shared<GalleryView>(lvglPath, item.name));
            }
        } else if (ext == ".txt" || ext == ".json" || ext == ".ini" || ext == ".enc" || ext == ".log" || ext == ".csv" || ext == ".md" || ext == ".c" || ext == ".h" || ext == ".cpp") {
            UIManager::showToast("Abriendo en Editor...");
            UIManager::getInstance().pushView(std::make_shared<TextEditorView>(fullFilePath));
        } else if (ext == ".lua") {
            UIManager::showToast("Abriendo en Lua Runner...");
            UIManager::getInstance().pushView(std::make_shared<LuaRunnerView>(fullFilePath));
        } else if (ext == ".mp3" || ext == ".wav") {
            UIManager::showToast("Abriendo reproductor de musica...");
            UIManager::getInstance().pushView(std::make_shared<MusicPlayerView>());
        } else if (ext == ".wad" || ext == ".gbc" || ext == ".gb") {
            UIManager::showToast("Abriendo Cartuchos...");
            UIManager::getInstance().pushView(std::make_shared<CartridgeView>());
        } else {
            char msg[96];
            snprintf(msg, sizeof(msg), "Archivo: %s (%s)", item.name.c_str(), formatBytes(item.size).c_str());
            UIManager::showToast(msg);
        }
    }
}

void FileManagerView::itemDeleteClickCb(lv_event_t* e) {
    ItemContext* ctx = static_cast<ItemContext*>(lv_event_get_user_data(e));
    if (!ctx || !ctx->view) return;
    ctx->view->showDeleteConfirmModal(ctx->fileEntry);
}

void FileManagerView::modalCancelCb(lv_event_t* e) {
    DeleteModalContext* ctx = static_cast<DeleteModalContext*>(lv_event_get_user_data(e));
    if (ctx) {
        if (ctx->view) {
            ctx->view->m_modalMask = nullptr;
        }
        if (ctx->mask && lv_obj_is_valid(ctx->mask)) {
            lv_obj_delete_async(ctx->mask);
        }
        delete ctx;
    }
}

void FileManagerView::modalConfirmDeleteCb(lv_event_t* e) {
    DeleteModalContext* ctx = static_cast<DeleteModalContext*>(lv_event_get_user_data(e));
    if (ctx) {
        bool ok = cbdos::storage::deleteFile(ctx->fullPath.c_str());
        if (ok) {
            UIManager::showToast("Elemento eliminado con exito");
            if (ctx->view) {
                ctx->view->refreshCurrentView();
            }
        } else {
            UIManager::showToast("Error al eliminar elemento");
        }

        if (ctx->view) {
            ctx->view->m_modalMask = nullptr;
        }
        if (ctx->mask && lv_obj_is_valid(ctx->mask)) {
            lv_obj_delete_async(ctx->mask);
        }
        delete ctx;
    }
}

void FileManagerView::showDeleteConfirmModal(const cbdos::storage::FileEntry& file) {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width;
    int32_t screenH = caps.height;

    std::string unitPrefix = (m_currentStorage == cbdos::storage::StorageType::SdCard) ? "/sdcard" : "/spiffs";
    std::string itemFullPath;
    if (m_currentPath == "/") {
        itemFullPath = unitPrefix + "/" + file.name;
    } else {
        itemFullPath = unitPrefix + m_currentPath + "/" + file.name;
    }

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_width(modal, screenW >= 480 ? 380 : 280);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modal, 16, 0);
    lv_obj_set_style_pad_row(modal, 12, 0);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text_fmt(title, "%s %s", LV_SYMBOL_WARNING, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_DEL_TITLE));
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF453A), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    char msgBuf[160];
    snprintf(msgBuf, sizeof(msgBuf), "¿Deseas eliminar permanentemente \"%s\"?", file.name.c_str());
    lv_obj_t* msg = lv_label_create(modal);
    lv_label_set_text(msg, msgBuf);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, screenW >= 480 ? 340 : 240);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(msg, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);

    // Botones de acción
    lv_obj_t* btnCont = lv_obj_create(modal);
    lv_obj_set_width(btnCont, screenW >= 480 ? 320 : 250);
    lv_obj_set_height(btnCont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnCont, 0, 0);
    lv_obj_set_style_border_width(btnCont, 0, 0);
    lv_obj_set_flex_flow(btnCont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnCont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btnCont, 0, 0);

    DeleteModalContext* ctx = new DeleteModalContext{this, file, itemFullPath, m_modalMask};

    int32_t btnW = screenW >= 480 ? 140 : 110;

    // Botón Cancelar
    lv_obj_t* btnCancel = lv_button_create(btnCont);
    lv_obj_set_size(btnCancel, btnW, 36);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_CANCEL));
    lv_obj_set_style_text_color(lblC, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lblC, &lv_font_montserrat_12, 0);
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, modalCancelCb, LV_EVENT_CLICKED, ctx);

    // Botón Eliminar
    lv_obj_t* btnDel = lv_button_create(btnCont);
    lv_obj_set_size(btnDel, btnW, 36);
    DefaultTheme::applyButton(btnDel, 8);
    lv_obj_set_style_bg_color(btnDel, lv_color_hex(0xFF453A), 0);
    lv_obj_t* lblD = lv_label_create(btnDel);
    lv_label_set_text(lblD, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_DEL_BTN));
    lv_obj_set_style_text_color(lblD, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lblD, &lv_font_montserrat_12, 0);
    lv_obj_center(lblD);
    lv_obj_add_event_cb(btnDel, modalConfirmDeleteCb, LV_EVENT_CLICKED, ctx);
}

void FileManagerView::showRecoveryModal(const cbdos::storage::FileEntry& file) {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width;
    int32_t screenH = caps.height;

    std::string itemFullPath = "/sdcard/" + file.name;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_width(modal, screenW >= 480 ? 380 : 280);
    lv_obj_set_height(modal, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(modal, 16, 0);
    lv_obj_set_style_pad_row(modal, 10, 0);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text_fmt(title, "%s %s", LV_SYMBOL_EYE_OPEN, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_FOR_TITLE));
    lv_obj_set_style_text_color(title, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    char msgBuf[192];
    snprintf(msgBuf, sizeof(msgBuf), "Archivo eliminado detectado:\n%s\nTamano: %s\nCluster FAT: #%u",
             file.name.c_str(), formatBytes(file.size).c_str(), (unsigned)file.startCluster);
    lv_obj_t* msg = lv_label_create(modal);
    lv_label_set_text(msg, msgBuf);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, screenW >= 480 ? 340 : 240);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(msg, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);

    DeleteModalContext* ctx = new DeleteModalContext{this, file, itemFullPath, m_modalMask};

    // Botón 1: Backup / Extraer a Flash Interna
    lv_obj_t* btnBackup = lv_button_create(modal);
    lv_obj_set_size(btnBackup, screenW >= 480 ? 320 : 240, 36);
    DefaultTheme::applyButton(btnBackup, 8);
    lv_obj_set_style_bg_color(btnBackup, lv_color_hex(0x0284C7), 0);
    lv_obj_t* lblB = lv_label_create(btnBackup);
    lv_label_set_text_fmt(lblB, "%s %s", LV_SYMBOL_SAVE, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_BACKUP_FLASH));
    lv_obj_set_style_text_font(lblB, &lv_font_montserrat_12, 0);
    lv_obj_center(lblB);
    lv_obj_add_event_cb(btnBackup, [](lv_event_t* e) {
        DeleteModalContext* pCtx = (DeleteModalContext*)lv_event_get_user_data(e);
        if (pCtx) {
            std::string dst = "/spiffs/" + pCtx->fileEntry.name;
            if (cbdos::storage::recoverFile(pCtx->fullPath.c_str(), dst.c_str())) {
                UIManager::showToast("Copiado con exito a Flash!");
            } else {
                UIManager::showToast("Error al volcar backup a Flash");
            }
            if (pCtx->view) pCtx->view->m_modalMask = nullptr;
            if (pCtx->mask && lv_obj_is_valid(pCtx->mask)) lv_obj_delete_async(pCtx->mask);
            delete pCtx;
        }
    }, LV_EVENT_CLICKED, ctx);

    // Botón 2: Undelete / Restaurar en MicroSD
    lv_obj_t* btnUndelete = lv_button_create(modal);
    lv_obj_set_size(btnUndelete, screenW >= 480 ? 320 : 240, 36);
    DefaultTheme::applyButton(btnUndelete, 8);
    lv_obj_set_style_bg_color(btnUndelete, lv_color_hex(0x059669), 0);
    lv_obj_t* lblU = lv_label_create(btnUndelete);
    lv_label_set_text_fmt(lblU, "%s %s", LV_SYMBOL_REFRESH, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_RESTORE_SD));
    lv_obj_set_style_text_font(lblU, &lv_font_montserrat_12, 0);
    lv_obj_center(lblU);
    lv_obj_add_event_cb(btnUndelete, [](lv_event_t* e) {
        DeleteModalContext* pCtx = (DeleteModalContext*)lv_event_get_user_data(e);
        if (pCtx) {
            if (cbdos::storage::undeleteFile(pCtx->fullPath.c_str())) {
                UIManager::showToast("Entrada FAT restaurada con exito!");
                if (pCtx->view) pCtx->view->refreshCurrentView();
            } else {
                UIManager::showToast("Error al restaurar entrada FAT");
            }
            if (pCtx->view) pCtx->view->m_modalMask = nullptr;
            if (pCtx->mask && lv_obj_is_valid(pCtx->mask)) lv_obj_delete_async(pCtx->mask);
            delete pCtx;
        }
    }, LV_EVENT_CLICKED, ctx);

    // Botón 3: Cerrar
    lv_obj_t* btnClose = lv_button_create(modal);
    lv_obj_set_size(btnClose, screenW >= 480 ? 320 : 240, 36);
    DefaultTheme::applyButton(btnClose, 8);
    lv_obj_t* lblCl = lv_label_create(btnClose);
    lv_label_set_text(lblCl, cbdos::lang::tr(cbdos::lang::StrId::STR_FILE_CLOSE));
    lv_obj_set_style_text_font(lblCl, &lv_font_montserrat_12, 0);
    lv_obj_center(lblCl);
    lv_obj_add_event_cb(btnClose, modalCancelCb, LV_EVENT_CLICKED, ctx);
}

} // namespace ui
} // namespace cbdos
