#pragma once
#include <lvgl.h>
#include <functional>
#include <string>

namespace cbdos {
namespace ui {

class StaffPinModal {
public:
    using SuccessCallback = std::function<void()>;

    static void show(SuccessCallback onSuccess = nullptr);
    static void hide();
    static bool isOpen();

private:
    static lv_obj_t* s_mask;
    static lv_obj_t* s_card;
    static lv_obj_t* s_dotsContainer;
    static lv_obj_t* s_dots[4];
    static lv_obj_t* s_errorLabel;
    static lv_obj_t* s_btnMatrix;
    static std::string s_enteredPin;
    static SuccessCallback s_onSuccessCb;

    static void btnMatrixEventCb(lv_event_t* e);
    static void updateDots();
    static void checkPin();
    static void resetInput();
};

} // namespace ui
} // namespace cbdos
