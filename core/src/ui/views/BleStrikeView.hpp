#pragma once

#include "BaseView.hpp"
#include <atomic>
#include <lvgl.h>
#include <string>

namespace cbdos {
namespace ui {

/**
 * @brief BLE Lab — emisor de balizas publicitarias BLE
 *
 * Reemplaza la antigua pantalla de 4 etiquetas estaticas. Controla el
 * BleTransmitter del core: categoria y variante del payload, potencia TX,
 * intervalo, politica de rotacion de MAC, y estado en vivo de la emision.
 */
class BleStrikeView : public BaseView {
public:
    BleStrikeView();
    ~BleStrikeView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;
    void onUpdate() override;

private:
    void buildUi(lv_obj_t* parent);
    void buildControls(lv_obj_t* parent);
    void buildStatus(lv_obj_t* parent);

    void refreshCategories();
    void refreshVariants();
    void refreshStatus();

    static void categoryCb(lv_event_t* e);
    static void variantCb(lv_event_t* e);
    static void powerCb(lv_event_t* e);
    static void intervalCb(lv_event_t* e);
    static void macCb(lv_event_t* e);
    static void toggleCb(lv_event_t* e);

    // Widgets
    lv_obj_t* m_scroll = nullptr;
    lv_obj_t* m_ddCategory = nullptr;
    lv_obj_t* m_ddVariant = nullptr;
    lv_obj_t* m_ddPower = nullptr;
    lv_obj_t* m_ddInterval = nullptr;
    lv_obj_t* m_ddMac = nullptr;
    lv_obj_t* m_btnToggle = nullptr;
    lv_obj_t* m_lblToggle = nullptr;
    lv_obj_t* m_lblState = nullptr;
    lv_obj_t* m_lblCounters = nullptr;
    lv_obj_t* m_lblHint = nullptr;
    lv_obj_t* m_ledActive = nullptr;
    lv_timer_t* m_timer = nullptr;

    // Estado
    std::atomic<bool> m_statusDirty{true};
    bool m_available = false;
};

} // namespace ui
} // namespace cbdos