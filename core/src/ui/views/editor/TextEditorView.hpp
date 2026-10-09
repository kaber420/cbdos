#pragma once

#include "BaseView.hpp"
#include "cbdos/storage.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

namespace cbdos {
namespace ui {

class TextEditorView : public BaseView {
public:
    explicit TextEditorView(const std::string& initialPath = "");
    virtual ~TextEditorView() = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;

    void loadFile(const std::string& path);

private:
    // Event callbacks
    static void taEventCb(lv_event_t* e);
    static void btnSaveCb(lv_event_t* e);
    static void btnSaveAsCb(lv_event_t* e);
    static void btnNewCb(lv_event_t* e);
    static void btnOpenCb(lv_event_t* e);
    static void btnToggleKbCb(lv_event_t* e);
    static void btnRunCb(lv_event_t* e);

    // Modals & Navigation callbacks
    void showSaveAsModal();
    void confirmSaveAs();
    void showOpenFileModal();
    static void modalSaveConfirmCb(lv_event_t* e);
    static void modalSaveCancelCb(lv_event_t* e);
    static void modalSaveUnitFlashCb(lv_event_t* e);
    static void modalSaveUnitSdCb(lv_event_t* e);
    static void modalOpenCloseCb(lv_event_t* e);
    static void modalFileItemCb(lv_event_t* e);
    static void modalScrollUpCb(lv_event_t* e);
    static void modalScrollDownCb(lv_event_t* e);

    void updateTitle();
    void updateRunButtonVisibility();
    void updateSaveModalUnitButtons();
    void scanTextFiles();

    // UI Widgets
    lv_obj_t* m_fileLabel;
    lv_obj_t* m_btnRun;
    lv_obj_t* m_btnKb;
    lv_obj_t* m_textArea;
    lv_obj_t* m_modalMask;
    lv_obj_t* m_saveAsTa;
    lv_obj_t* m_btnSaveTargetFlash;
    lv_obj_t* m_btnSaveTargetSd;

    std::string m_currentFilePath;
    cbdos::storage::StorageType m_modalSaveStorage;
    bool m_isModified;
    std::vector<std::string> m_foundFiles;
};

} // namespace ui
} // namespace cbdos
