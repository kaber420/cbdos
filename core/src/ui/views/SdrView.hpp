#pragma once

#include "cbdos_build_profile.h"

#if CBDOS_FEATURE_SDR

#include "BaseView.hpp"
#include "cbdos/sdr/esp_sdr_client.hpp"
#include "cbdos/sdr/sdr_dsp_engine.hpp"

#include <lvgl.h>

#include <cstdint>
#include <vector>
#include <atomic>
#include <string>

namespace cbdos {
namespace ui {

class SdrView : public BaseView {
public:
    SdrView();
    ~SdrView() override;

    bool onCreate(lv_obj_t* parent) override;
    void onDestroy() override;
    void onShow() override;
    void onHide() override;

private:
    // Callbacks de eventos UI (LVGL v9.5)
    static void onPlayPauseClicked(lv_event_t* e);
    static void onFreqStepClicked(lv_event_t* e);
    static void onPresetFreqSelected(lv_event_t* e);
    static void onSpanSelected(lv_event_t* e);
    static void onFilterBwSelected(lv_event_t* e);
    static void onFftBinsSelected(lv_event_t* e);
    static void onGainModeClicked(lv_event_t* e);
    static void onResetMaxHoldClicked(lv_event_t* e);
    static void onChartTouch(lv_event_t* e);
    static void onRefreshTimer(lv_timer_t* timer);

    // Helpers de creación de paneles
    void createControlBar(lv_obj_t* parent);
    void createSpectrumPanel(lv_obj_t* parent);
    void createWaterfallPanel(lv_obj_t* parent);
    void createBottomBar(lv_obj_t* parent);

    // Control y reactividad
    void applyWithStreamPause(std::function<void()> action);
    void tuneToFrequency(uint32_t freqMHz);
    void updateAxisLabels();
    void updateUiFromFrame();
    void updateWaterfallRow(const std::vector<float>& spectrum);
    static void initColormapLut();

    // Controles superiores
    lv_obj_t* m_btnPlayPause     = nullptr;
    lv_obj_t* m_lblPlayPause     = nullptr;
    lv_obj_t* m_lblFreqValue     = nullptr;
    lv_obj_t* m_ddPresets        = nullptr;
    lv_obj_t* m_ddSpan           = nullptr;
    lv_obj_t* m_ddFilterBw       = nullptr;
    lv_obj_t* m_ddFftBins        = nullptr;
    lv_obj_t* m_btnGainMode      = nullptr;
    lv_obj_t* m_lblGainMode      = nullptr;
    lv_obj_t* m_btnHold          = nullptr;

    // Panel de espectro y ejes
    lv_obj_t* m_chartContainer   = nullptr;
    lv_obj_t* m_chart            = nullptr;
    lv_chart_series_t* m_serLive = nullptr;
    lv_chart_series_t* m_serMax  = nullptr;
    lv_obj_t* m_lblAxisY[5]      = {nullptr};
    lv_obj_t* m_lblAxisX[5]      = {nullptr};
    lv_obj_t* m_lblCursor        = nullptr;
    lv_obj_t* m_lblMeasBar       = nullptr;

    // Panel de cascada
    lv_obj_t* m_canvasWaterfall  = nullptr;
    uint16_t* m_waterfallBuf     = nullptr;
    size_t    m_waterfallBufSize = 0;
    static constexpr int32_t kWaterfallW = 440;
    static constexpr int32_t kWaterfallH = 160;

    // Telemetría inferior
    lv_obj_t* m_lblStatus        = nullptr;
    lv_obj_t* m_lblTelemetry     = nullptr;

    // Timer de refresco LVGL (30 FPS)
    lv_timer_t* m_renderTimer    = nullptr;

    // Gestión del gesto de arrastre táctil (Pan & Drag)
    int32_t  m_dragStartX        = 0;
    uint32_t m_dragStartFreq     = 2420;
    bool     m_isDragging        = false;

    // Datos compartidos entre streaming (Core 0/1) y render LVGL
    std::atomic<bool>  m_hasNewFrame{false};
    void*              m_frameMutex = nullptr; // SemaphoreHandle_t para proteger el vector compartido
    std::vector<float> m_sharedSpectrum;
    std::vector<float> m_sharedMaxHold;
    float              m_sharedPeakDbfs = -100.0f;
    float              m_sharedPeakFreqMHz = 2420.0f;
    uint32_t           m_sharedDspUs = 0;

    // LUT RGB565 para colormap de cascada (256 niveles)
    static uint16_t    s_colormapLut[256];
    static bool        s_colormapInit;

    // Configuración activa
    cbdos::sdr::SdrConfig m_config;
    bool m_isRunning = false;
    bool m_holdEnabled = true;
};

} // namespace ui
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
