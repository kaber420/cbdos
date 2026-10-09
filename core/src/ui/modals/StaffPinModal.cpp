#include "StaffPinModal.hpp"
#include "LockPinModal.hpp"

namespace cbdos {
namespace ui {

/**
 * @file StaffPinModal.cpp
 * @deprecated Sustituido por LockPinModal. Mantenido por compatibilidad de interfaz previa.
 */

lv_obj_t* StaffPinModal::s_mask = nullptr;
lv_obj_t* StaffPinModal::s_card = nullptr;
lv_obj_t* StaffPinModal::s_dotsContainer = nullptr;
lv_obj_t* StaffPinModal::s_dots[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* StaffPinModal::s_errorLabel = nullptr;
lv_obj_t* StaffPinModal::s_btnMatrix = nullptr;
std::string StaffPinModal::s_enteredPin = "";
StaffPinModal::SuccessCallback StaffPinModal::s_onSuccessCb = nullptr;

bool StaffPinModal::isOpen() {
    return LockPinModal::isOpen();
}

void StaffPinModal::hide() {
    LockPinModal::hide();
}

void StaffPinModal::resetInput() {
}

void StaffPinModal::updateDots() {
}

void StaffPinModal::checkPin() {
}

void StaffPinModal::btnMatrixEventCb(lv_event_t* e) {
    (void)e;
}

void StaffPinModal::show(SuccessCallback onSuccess) {
    LockPinModal::show(nullptr, onSuccess, nullptr, false);
}

} // namespace ui
} // namespace cbdos
