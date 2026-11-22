#pragma once

#include "BaseView.hpp"
#include "KvmClientService.hpp"
#include "KvmP4Decoder.hpp"
#include <lvgl.h>
#include <string>

namespace cbdos {
namespace ui {

class KvmClientView : public BaseView {
public:
    KvmClientView();
    virtual ~KvmClientView();

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;

private:
    void createToolbar(lv_obj_t* parent);
    void createConnectDialog(lv_obj_t* parent);
    void updateTelemetryUi(float fps, int rttMs, bool inControl);

    // Callbacks de eventos LVGL
    static void touchCanvasCb(lv_event_t* e);
    static void btnCadCb(lv_event_t* e);
    static void btnSuperCb(lv_event_t* e);
    static void btnEscCb(lv_event_t* e);
    static void btnTabCb(lv_event_t* e);
    static void btnKbdCb(lv_event_t* e);
    static void btnConnectCb(lv_event_t* e);
    static void kbdInputCb(lv_event_t* e);

    // Elementos de la interfaz
    lv_obj_t* m_canvasObj{nullptr};
    lv_obj_t* m_toolbar{nullptr};
    lv_obj_t* m_connectModal{nullptr};
    lv_obj_t* m_taHost{nullptr};
    lv_obj_t* m_taPort{nullptr};
    lv_obj_t* m_lblStats{nullptr};
    lv_obj_t* m_taKvmInput{nullptr};

    // Búfer gráfico para renderizado de video en LVGL 9.5
    lv_image_dsc_t m_imgDsc{};
    uint8_t*       m_canvasBuf{nullptr};
    uint32_t       m_canvasW{800};
    uint32_t       m_canvasH{440};

    // Decodificador P4
    kvm::KvmP4Decoder m_decoder;

    bool m_virtualKbdVisible{false};
    bool m_mouseModeRelative{false};
};

} // namespace ui
} // namespace cbdos
