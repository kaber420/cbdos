#pragma once
#include "BaseView.hpp"
#include "cbdos/security.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class SecurityConfigView : public BaseView {
public:
    SecurityConfigView();
    ~SecurityConfigView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onShow() override;

private:
    // Contenedores y widgets dinámicos
    lv_obj_t* m_policyCards[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* m_policyRadios[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* m_pinStatusLabel = nullptr;

    // Callbacks de interacción
    static void policyCardClickedCb(lv_event_t* e);
    static void changePinClickedCb(lv_event_t* e);
    static void lockNowClickedCb(lv_event_t* e);

    // Métodos de refresco de UI
    void refreshPolicySelection();
    void refreshPinStatus();
    void applySelectedPolicy(cbdos::security::LockPolicy policy);
};

} // namespace ui
} // namespace cbdos
