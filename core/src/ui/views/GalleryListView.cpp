#include "GalleryListView.hpp"
#include "GalleryView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include <algorithm>
#include <cctype>

namespace cbdos {
namespace ui {

GalleryListView::GalleryListView()
    : BaseView("Galeria"),
      m_currentCategoryId(0),
      m_currentItemIndex(0) {
    m_categories = {
        {0, "Todas", LV_SYMBOL_LIST},
        {1, "Fotos", LV_SYMBOL_IMAGE},
        {2, "GIFs",  LV_SYMBOL_PLAY}
    };
}

void GalleryListView::loadDataFromSD() {
    m_mediaItems.clear();

    auto checkAndAddFile = [this](const std::string& folderName, const std::string& fileName, int& fileId) {
        std::string lowerName = fileName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        bool isGif = (lowerName.length() >= 4 && lowerName.substr(lowerName.length() - 4) == ".gif");
        bool isImg = (lowerName.length() >= 4 && (
            lowerName.substr(lowerName.length() - 4) == ".jpg" ||
            lowerName.substr(lowerName.length() - 4) == ".png" ||
            lowerName.substr(lowerName.length() - 4) == ".bmp"
        )) || (lowerName.length() >= 5 && lowerName.substr(lowerName.length() - 5) == ".jpeg");

        if (isGif || isImg) {
            std::string directPath;
            if (folderName == "Raiz" || folderName == "sdcard") {
                directPath = "A:/" + fileName;
            } else {
                directPath = "A:/" + folderName + "/" + fileName;
            }

            m_mediaItems.emplace_back(
                std::to_string(fileId++),
                fileName,
                folderName,
                directPath,
                isGif,
                isGif ? LV_SYMBOL_PLAY : LV_SYMBOL_IMAGE
            );
        }
    };

    int fileId = 1;

    // 1. Escanear directorio raíz de MicroSD
    auto rootEntries = cbdos::storage::listDir("/sdcard");
    for (const auto& entry : rootEntries) {
        if (!entry.isDirectory) {
            checkAndAddFile("Raiz", entry.name, fileId);
        } else {
            // 2. Escanear subcarpetas (ej: /fotos, /wallpapers, /images, /memes)
            std::string subDirPath = std::string("/sdcard/") + entry.name;
            auto subEntries = cbdos::storage::listDir(subDirPath.c_str());
            for (const auto& subEntry : subEntries) {
                if (!subEntry.isDirectory) {
                    checkAndAddFile(entry.name, subEntry.name, fileId);
                }
            }
        }
    }

    if (m_mediaItems.empty()) {
        m_mediaItems.emplace_back(
            "0",
            "Sin imagenes en MicroSD",
            "SD",
            "",
            false,
            LV_SYMBOL_FILE
        );
    }

    printf("[Galeria] MicroSD escaneada. Medios encontrados: %d\n", (int)m_mediaItems.size());
}

void GalleryListView::buildFilteredIndices() {
    m_filteredIndices.clear();

    for (size_t i = 0; i < m_mediaItems.size(); i++) {
        const auto& item = m_mediaItems[i];
        if (m_currentCategoryId == 0) {
            m_filteredIndices.push_back((int)i);
        } else if (m_currentCategoryId == 1 && !item.isGif) {
            m_filteredIndices.push_back((int)i);
        } else if (m_currentCategoryId == 2 && item.isGif) {
            m_filteredIndices.push_back((int)i);
        }
    }

    if (m_filteredIndices.empty()) {
        m_filteredIndices.push_back(0);
    }
}

bool GalleryListView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // 1. Cargar medios si está vacía
    if (m_mediaItems.empty()) {
        loadDataFromSD();
    }
    buildFilteredIndices();

    // 2. Configurar HeaderBar
    UIManager::getInstance().getHeaderBar().showWifi(false);
    UIManager::getInstance().getHeaderBar().setTitle("Galeria");
    UIManager::getInstance().getHeaderBar().showBackButton(true, []() {
        UIManager::getInstance().popView();
    });
    UIManager::getInstance().getHeaderBar().setRightAction(LV_SYMBOL_REFRESH, [this]() {
        printf("[Galeria] Refrescando catalogo de medios desde MicroSD...\n");
        this->refreshGallery();
    });

    // 3. Contenedor principal de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    DefaultTheme::disableScroll(m_container);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);

    // 4. Barra de Categorías / Filtros
    m_categoriesContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_categoriesContainer, LV_PCT(100));
    lv_obj_set_height(m_categoriesContainer, 44);
    lv_obj_set_style_bg_opa(m_categoriesContainer, 0, 0);
    lv_obj_set_style_border_width(m_categoriesContainer, 0, 0);
    lv_obj_set_style_pad_all(m_categoriesContainer, 2, 0);
    lv_obj_set_style_pad_column(m_categoriesContainer, 8, 0);
    lv_obj_set_flex_flow(m_categoriesContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_categoriesContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(m_categoriesContainer, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(m_categoriesContainer, LV_SCROLLBAR_MODE_OFF);

    // 5. Contenedor principal de visualización (tarjeta + controles)
    m_mediaContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_mediaContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_mediaContainer, 1);
    lv_obj_set_style_bg_opa(m_mediaContainer, 0, 0);
    lv_obj_set_style_border_width(m_mediaContainer, 0, 0);
    lv_obj_set_style_pad_all(m_mediaContainer, 0, 0);
    lv_obj_set_style_pad_row(m_mediaContainer, 6, 0);
    lv_obj_set_flex_flow(m_mediaContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_mediaContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(m_mediaContainer);

    renderCategories();
    renderMedia();

    return true;
}

void GalleryListView::onDestroy() {
    UIManager::getInstance().getHeaderBar().showWifi(true);
    m_categoriesContainer = nullptr;
    m_mediaContainer = nullptr;
    m_card = nullptr;
    m_imgArea = nullptr;
    m_iconLbl = nullptr;
    m_imgObj = nullptr;
    m_gifObj = nullptr;
    m_nameLbl = nullptr;
    m_pathLbl = nullptr;
    m_prevBtn = nullptr;
    m_nextBtn = nullptr;
    m_pageIndicatorLbl = nullptr;
    BaseView::onDestroy();
}

void GalleryListView::renderCategories() {
    if (!m_categoriesContainer || !lv_obj_is_valid(m_categoriesContainer)) return;
    lv_obj_clean(m_categoriesContainer);

    for (const auto& cat : m_categories) {
        lv_obj_t* btn = lv_button_create(m_categoriesContainer);
        lv_obj_set_height(btn, 36);
        lv_obj_set_style_pad_hor(btn, 12, 0);

        bool isSelected = (cat.id == m_currentCategoryId);
        if (isSelected) {
            DefaultTheme::applyRaisedCard(btn, 18);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, DefaultTheme::getPrimaryAccent(), 0);
        } else {
            DefaultTheme::applyButton(btn, 18);
        }

        DefaultTheme::disableScroll(btn);
        lv_obj_set_user_data(btn, (void*)this);
        // Guardar category ID en tag o usando evento custom
        lv_obj_add_event_cb(btn, categoryBtnCb, LV_EVENT_CLICKED, (void*)(intptr_t)cat.id);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn, 6, 0);

        lv_obj_t* icon = lv_label_create(btn);
        lv_label_set_text(icon, cat.icon);
        lv_obj_set_style_text_color(icon, isSelected ? DefaultTheme::getPrimaryAccent() : DefaultTheme::getMutedTextColor(), 0);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, cat.name.c_str());
        lv_obj_set_style_text_color(label, isSelected ? DefaultTheme::getPrimaryAccent() : DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }
}

void GalleryListView::renderMedia() {
    if (!m_mediaContainer || !lv_obj_is_valid(m_mediaContainer)) return;
    lv_obj_clean(m_mediaContainer);

    auto caps = cbdos::display::getCapabilities();
    int32_t areaHeight = (caps.width >= 480) ? 380 : 210;

    // Tarjeta central elevada
    m_card = lv_obj_create(m_mediaContainer);
    lv_obj_set_width(m_card, LV_PCT(100));
    lv_obj_set_flex_grow(m_card, 1);
    DefaultTheme::applyRaisedCard(m_card, 16);
    lv_obj_set_style_pad_all(m_card, 8, 0);
    lv_obj_set_flex_flow(m_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(m_card);

    // Área del visualizador de imagen / GIF (hundida)
    m_imgArea = lv_obj_create(m_card);
    lv_obj_set_width(m_imgArea, LV_PCT(100));
    lv_obj_set_height(m_imgArea, areaHeight);
    DefaultTheme::applySunkenCard(m_imgArea, 12);
    DefaultTheme::disableScroll(m_imgArea);
    lv_obj_set_style_pad_all(m_imgArea, 0, 0);
    lv_obj_set_style_clip_corner(m_imgArea, true, 0);

    lv_obj_add_flag(m_imgArea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_imgArea, mediaCardCb, LV_EVENT_CLICKED, this);

    // Icono placeholder
    m_iconLbl = lv_label_create(m_imgArea);
    lv_label_set_text(m_iconLbl, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(m_iconLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(m_iconLbl, &lv_font_montserrat_24, 0);
    lv_obj_center(m_iconLbl);

    // Widget para imagen estática
    m_imgObj = lv_image_create(m_imgArea);
    lv_obj_add_flag(m_imgObj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(m_imgObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(m_imgObj);

#if LV_USE_GIF
    // Widget para GIF animado
    m_gifObj = lv_gif_create(m_imgArea);
    lv_obj_add_flag(m_gifObj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(m_gifObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(m_gifObj);
#endif

    // Nombre de la imagen/archivo
    m_nameLbl = lv_label_create(m_card);
    lv_label_set_text(m_nameLbl, "");
    lv_obj_set_style_text_color(m_nameLbl, DefaultTheme::getTextColor(), 0);
    lv_label_set_long_mode(m_nameLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(m_nameLbl, LV_PCT(100));
    lv_obj_set_height(m_nameLbl, 28);
    lv_obj_set_style_text_align(m_nameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(m_nameLbl, 4, 0);

    // Carpeta contenedora
    m_pathLbl = lv_label_create(m_card);
    lv_label_set_text(m_pathLbl, "");
    lv_obj_set_style_text_color(m_pathLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_pathLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_bottom(m_pathLbl, 4, 0);

    // Barra de navegación inferior [ Anterior ] [ Indicador ] [ Siguiente ]
    lv_obj_t* navRow = lv_obj_create(m_mediaContainer);
    lv_obj_set_width(navRow, LV_PCT(100));
    lv_obj_set_height(navRow, 46);
    lv_obj_set_style_bg_opa(navRow, 0, 0);
    lv_obj_set_style_border_width(navRow, 0, 0);
    lv_obj_set_style_pad_all(navRow, 0, 0);
    DefaultTheme::disableScroll(navRow);
    lv_obj_set_flex_flow(navRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(navRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Anterior (solo-icono: el indicador N/Total ya da el contexto)
    m_prevBtn = lv_button_create(navRow);
    lv_obj_set_size(m_prevBtn, 38, 38);
    DefaultTheme::applyButton(m_prevBtn, 12);
    lv_obj_add_event_cb(m_prevBtn, prevBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* prevLbl = lv_label_create(m_prevBtn);
    lv_label_set_text(prevLbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(prevLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(prevLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(prevLbl);

    // Indicador "1 / Total"
    m_pageIndicatorLbl = lv_label_create(navRow);
    lv_label_set_text(m_pageIndicatorLbl, "");
    lv_obj_set_style_text_color(m_pageIndicatorLbl, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(m_pageIndicatorLbl, &lv_font_montserrat_12, 0);

    // Botón Siguiente (solo-icono)
    m_nextBtn = lv_button_create(navRow);
    lv_obj_set_size(m_nextBtn, 38, 38);
    DefaultTheme::applyButton(m_nextBtn, 12);
    lv_obj_add_event_cb(m_nextBtn, nextBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* nextLbl = lv_label_create(m_nextBtn);
    lv_label_set_text(nextLbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(nextLbl, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(nextLbl, &lv_font_montserrat_12, 0);
    lv_obj_center(nextLbl);

    navigateTo(0);
}

void GalleryListView::navigateTo(int index) {
    int total = (int)m_filteredIndices.size();
    if (total == 0 || m_card == nullptr) return;

    if (index < 0) index = 0;
    if (index >= total) index = total - 1;
    m_currentItemIndex = index;

    int mediaIdx = m_filteredIndices[index];
    if (mediaIdx < 0 || mediaIdx >= (int)m_mediaItems.size()) return;

    const GalleryMediaItem& item = m_mediaItems[mediaIdx];
    printf("[Galeria] Mostrando medio [%d/%d]: %s (Ruta: %s, GIF: %s)\n",
           index + 1, total, item.name.c_str(), item.path.c_str(), item.isGif ? "SI" : "NO");

    // Ocultar elementos visuales
    if (m_iconLbl) lv_obj_add_flag(m_iconLbl, LV_OBJ_FLAG_HIDDEN);
    if (m_imgObj)  lv_obj_add_flag(m_imgObj,  LV_OBJ_FLAG_HIDDEN);
#if LV_USE_GIF
    if (m_gifObj)  lv_obj_add_flag(m_gifObj,  LV_OBJ_FLAG_HIDDEN);
#endif

    if (!item.path.empty()) {
#if LV_USE_GIF
        if (item.isGif && m_gifObj) {
            lv_gif_set_src(m_gifObj, item.path.c_str());
            lv_obj_center(m_gifObj);
            lv_obj_remove_flag(m_gifObj, LV_OBJ_FLAG_HIDDEN);
        } else
#endif
        if (m_imgObj) {
            lv_image_set_src(m_imgObj, item.path.c_str());
            lv_obj_center(m_imgObj);
            lv_obj_remove_flag(m_imgObj, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (m_iconLbl) {
            lv_label_set_text(m_iconLbl, item.icon ? item.icon : LV_SYMBOL_FILE);
            lv_obj_remove_flag(m_iconLbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_label_set_text(m_nameLbl, item.name.c_str());
    std::string folderStr = "Carpeta: " + item.folder;
    lv_label_set_text(m_pathLbl, folderStr.c_str());

    updateNavButtons();
}

void GalleryListView::updateNavButtons() {
    int total = (int)m_filteredIndices.size();

    if (m_currentItemIndex <= 0) {
        lv_obj_add_state(m_prevBtn, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(m_prevBtn, LV_STATE_DISABLED);
    }

    if (m_currentItemIndex >= total - 1) {
        lv_obj_add_state(m_nextBtn, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(m_nextBtn, LV_STATE_DISABLED);
    }

    lv_label_set_text_fmt(m_pageIndicatorLbl, "%d / %d", m_currentItemIndex + 1, total);
}

void GalleryListView::refreshGallery() {
    m_mediaItems.clear();
    loadDataFromSD();
    buildFilteredIndices();
    renderCategories();
    renderMedia();
}

// ────────────────────────────────────────────────────────────────
//  Callbacks de eventos
// ────────────────────────────────────────────────────────────────
void GalleryListView::prevBtnCb(lv_event_t* e) {
    GalleryListView* self = static_cast<GalleryListView*>(lv_event_get_user_data(e));
    if (self) {
        self->navigateTo(self->m_currentItemIndex - 1);
    }
}

void GalleryListView::nextBtnCb(lv_event_t* e) {
    GalleryListView* self = static_cast<GalleryListView*>(lv_event_get_user_data(e));
    if (self) {
        self->navigateTo(self->m_currentItemIndex + 1);
    }
}

void GalleryListView::categoryBtnCb(lv_event_t* e) {
    GalleryListView* self = static_cast<GalleryListView*>(lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e)));
    int catId = (int)(intptr_t)lv_event_get_user_data(e);

    if (self && self->m_currentCategoryId != catId) {
        self->m_currentCategoryId = catId;
        self->buildFilteredIndices();
        self->renderCategories();
        self->renderMedia();
    }
}

void GalleryListView::mediaCardCb(lv_event_t* e) {
    GalleryListView* self = static_cast<GalleryListView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_currentItemIndex >= 0 && self->m_currentItemIndex < (int)self->m_filteredIndices.size()) {
        int mediaIdx = self->m_filteredIndices[self->m_currentItemIndex];
        if (mediaIdx >= 0 && mediaIdx < (int)self->m_mediaItems.size()) {
            const auto& item = self->m_mediaItems[mediaIdx];
            if (!item.path.empty()) {
                UIManager::getInstance().pushView(std::make_shared<GalleryView>(item.path, item.name));
            }
        }
    }
}

void GalleryListView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

} // namespace ui
} // namespace cbdos
