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
    static void onChannelSelected(lv_event_t* e);
    static void onGainModeClicked(lv_event_t* e);
    static void onGainSliderChanged(lv_event_t* e);
    static void onFftBinsSelected(lv_event_t* e);
    static void onResetMaxHoldClicked(lv_event_t* e);
    static void onRefreshTimer(lv_timer_t* timer);

    // Helpers de creación de paneles
    void createControlBar(lv_obj_t* parent);
    void createSpectrumPanel(lv_obj_t* parent);
    void createWaterfallPanel(lv_obj_t* parent);
    void createBottomBar(lv_obj_t* parent);

    // Actualización gráfica reactiva
    void updateUiFromFrame();
    void updateWaterfallRow(const std::vector<float>& spectrum);
    static void initColormapLut();

    // Estado y controles
    lv_obj_t* m_btnPlayPause     = nullptr;
    lv_obj_t* m_lblPlayPause     = nullptr;
    lv_obj_t* m_lblStatus        = nullptr;
    lv_obj_t* m_ddChannels       = nullptr;
    lv_obj_t* m_btnGainMode      = nullptr;
    lv_obj_t* m_lblGainMode      = nullptr;
    lv_obj_t* m_sliderGain       = nullptr;
    lv_obj_t* m_ddFftBins        = nullptr;

    // Panel de espectro
    lv_obj_t* m_chart            = nullptr;
    lv_chart_series_t* m_serLive = nullptr;
    lv_chart_series_t* m_serMax  = nullptr;
    lv_obj_t* m_lblCenterFreq    = nullptr;
    lv_obj_t* m_lblSpan          = nullptr;

    // Panel de cascada
    lv_obj_t* m_canvasWaterfall  = nullptr;
    uint16_t* m_waterfallBuf     = nullptr;
    size_t    m_waterfallBufSize = 0;
    static constexpr int32_t kWaterfallW = 440;
    static constexpr int32_t kWaterfallH = 180;

    // Telemetría inferior
    lv_obj_t* m_lblTelemetry     = nullptr;

    // Timer de refresco LVGL (30 FPS)
    lv_timer_t* m_renderTimer    = nullptr;

    // Datos compartidos entre streaming (Core 1) y render LVGL (Core 0)
    std::atomic<bool>  m_hasNewFrame{false};
    std::vector<float> m_sharedSpectrum;
    std::vector<float> m_sharedMaxHold;
    float              m_sharedPeakDbfs = -120.0f;
    float              m_sharedPeakFreqMHz = 2412.0f;
    uint32_t           m_sharedDspUs = 0;

    // LUT RGB565 para colormap de cascada (256 niveles)
    static uint16_t    s_colormapLut[256];
    static bool        s_colormapInit;

    // Configuración activa
    cbdos::sdr::SdrConfig m_config;
    bool m_isRunning = false;
};

} // namespace ui
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
