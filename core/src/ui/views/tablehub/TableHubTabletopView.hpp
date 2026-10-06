#pragma once
#include "../BaseView.hpp"

namespace cbdos {
namespace ui {

class TableHubTabletopView : public BaseView {
public:
    TableHubTabletopView();
    ~TableHubTabletopView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) override;
    void onUpdate() override;

private:
    lv_obj_t* m_serviceBar = nullptr;
    lv_obj_t* m_menuContainer = nullptr;
    lv_obj_t* m_btnCallWaiter = nullptr;
    lv_obj_t* m_btnAskBill = nullptr;
};

} // namespace ui
} // namespace cbdos
