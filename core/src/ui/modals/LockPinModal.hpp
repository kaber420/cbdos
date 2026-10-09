#pragma once

#include <lvgl.h>
#include <functional>
#include <string>

namespace cbdos {
namespace ui {

class LockPinModal {
public:
    using SuccessCallback = std::function<void()>;
    using CancelCallback = std::function<void()>;
    using PinEnteredCallback = std::function<void(const std::string& pin)>;

    static void show(const char* customTitle = nullptr,
                     SuccessCallback onSuccess = nullptr,
                     CancelCallback onCancel = nullptr,
                     bool fullScreen = false);
    static void showCapture(const char* customTitle,
                            PinEnteredCallback onPinEntered,
                            CancelCallback onCancel = nullptr);
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
    static CancelCallback s_onCancelCb;
    static PinEnteredCallback s_onCaptureCb;
    static bool s_fullScreenMode;

    static void btnMatrixEventCb(lv_event_t* e);
    static void updateDots();
    static void checkPin();
    static void resetInput();
};

} // namespace ui
} // namespace cbdos
