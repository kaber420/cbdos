#pragma once

#include "BaseView.hpp"
#include "cbdos/storage.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class FileManagerView : public BaseView {
public:
    explicit FileManagerView(const std::string& initialPath = "/");
    virtual ~FileManagerView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    void refreshCurrentView();

private:
    bool m_forensicMode = false;
    cbdos::storage::StorageType m_currentStorage;
    std::string m_currentPath;

    // UI Widgets
    lv_obj_t* m_unitInfoLabel = nullptr;
    lv_obj_t* m_pathLabel = nullptr;
    lv_obj_t* m_btnUnitSD = nullptr;
    lv_obj_t* m_btnUnitFlash = nullptr;
    lv_obj_t* m_btnForensic = nullptr;
    lv_obj_t* m_listContainer = nullptr;
    lv_obj_t* m_modalMask = nullptr;

    // Helper methods
    void renderUnitSelector(lv_obj_t* parent);
    void renderPathBar(lv_obj_t* parent);
    void renderFileList(lv_obj_t* parent);
    void updateUnitButtons();
    void updatePathLabel();
    void updateStorageInfo();
    void updateForensicButton();
    void showDeleteConfirmModal(const cbdos::storage::FileEntry& file);
    void showRecoveryModal(const cbdos::storage::FileEntry& file);

    // Callbacks
    static void unitSdClickCb(lv_event_t* e);
    static void unitFlashClickCb(lv_event_t* e);
    static void btnUpClickCb(lv_event_t* e);
    static void btnRefreshClickCb(lv_event_t* e);
    static void btnForensicClickCb(lv_event_t* e);
    static void itemClickCb(lv_event_t* e);
    static void itemDeleteClickCb(lv_event_t* e);
    static void modalCancelCb(lv_event_t* e);
    static void modalConfirmDeleteCb(lv_event_t* e);
};

} // namespace ui
} // namespace cbdos
