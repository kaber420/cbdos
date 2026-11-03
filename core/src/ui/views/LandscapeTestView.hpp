#pragma once

#include "BaseView.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class LandscapeTestView : public BaseView {
public:
    LandscapeTestView();
    ~LandscapeTestView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    static void touchTestBtnCb(lv_event_t* e);

    lv_obj_t* m_lblResolution = nullptr;
    lv_obj_t* m_btnTouch = nullptr;
    lv_obj_t* m_lblTouch = nullptr;
    int m_clickCount = 0;
};

} // namespace ui
} // namespace cbdos

