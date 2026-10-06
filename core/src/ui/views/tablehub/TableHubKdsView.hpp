#pragma once
#include "../BaseView.hpp"

namespace cbdos {
namespace ui {

class TableHubKdsView : public BaseView {
public:
    TableHubKdsView();
    ~TableHubKdsView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;
    void onUpdate() override;

private:
    lv_obj_t* m_statusBar = nullptr;
    lv_obj_t* m_kanbanContainer = nullptr;
    lv_obj_t* m_colPending = nullptr;
    lv_obj_t* m_colPreparing = nullptr;
    lv_obj_t* m_colReady = nullptr;
};

} // namespace ui
} // namespace cbdos
