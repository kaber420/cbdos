#include "cbdos_build_profile.h"

#if CBDOS_FEATURE_SDR

#include "SdrView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/system.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace cbdos {
namespace ui {

namespace {

struct WifiChannel {
    int channel;
    uint32_t freqMHz;
};

static const WifiChannel kChannels[] = {
    {1,  2412}, {2,  2417}, {3,  2422}, {4,  2427},
    {5,  2432}, {6,  2437}, {7,  2442}, {8,  2447},
    {9,  2452}, {10, 2457}, {11, 2462}, {12, 2467},
    {13, 2472}, {14, 2484}
};

static constexpr size_t kChannelCount = sizeof(kChannels) / sizeof(kChannels[0]);

} // anonymous namespace

uint16_t SdrView::s_colormapLut[256];
bool     SdrView::s_colormapInit = false;

void SdrView::initColormapLut() {
    if (s_colormapInit) return;

    // Generar gradiente térmico de 256 niveles (ESPARGOS style)
    // 0..63:   Azul profundo -> Cian
    // 64..127: Cian -> Verde brillante
    // 128..191:Verde -> Amarillo intenso
    // 192..255:Amarillo -> Rojo / Blanco cálido
    for (int i = 0; i < 256; i++) {
        float t = (float)i / 255.0f;
        uint8_t r = 0, g = 0, b = 0;

        if (t < 0.25f) {
            float f = t / 0.25f;
            r = (uint8_t)(10 * (1.0f - f));
            g = (uint8_t)(20 + 160 * f);
            b = (uint8_t)(80 + 175 * f);
        } else if (t < 0.50f) {
            float f = (t - 0.25f) / 0.25f;
            r = 0;
            g = (uint8_t)(180 + 55 * f);
            b = (uint8_t)(255 * (1.0f - f));
        } else if (t < 0.75f) {
            float f = (t - 0.50f) / 0.25f;
            r = (uint8_t)(240 * f);
            g = 235;
            b = 10;
        } else {
            float f = (t - 0.75f) / 0.25f;
            r = 255;
            g = (uint8_t)(235 * (1.0f - f) + 240 * f);
            b = (uint8_t)(10 * (1.0f - f) + 220 * f);
        }

        // Convertir RGB888 a RGB565
        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        s_colormapLut[i] = rgb565;
    }
    s_colormapInit = true;
}

SdrView::SdrView()
    : BaseView("SDR Analyzer") {
    initColormapLut();

    m_config.frequencyMHz = 2412;
    m_config.sampleRate   = 40000000;
    m_config.sampleBits   = 8;
    m_config.fftBins      = 512;
    m_config.gainMode     = cbdos::sdr::GainMode::HardwareAgc;
    m_config.gainIndex    = 40;

    m_waterfallBufSize = kWaterfallW * kWaterfallH * sizeof(uint16_t);
    m_waterfallBuf = (uint16_t*)malloc(m_waterfallBufSize);
    if (m_waterfallBuf) {
        memset(m_waterfallBuf, 0, m_waterfallBufSize);
    }
}

SdrView::~SdrView() {
    onDestroy();
}

bool SdrView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Configuración de HeaderBar
    UIManager::getInstance().getHeaderBar().setTitle("SDR Analyzer");

    // Contenedor principal con scroll vertical
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // Construcción de la interfaz
    createControlBar(m_container);
    createSpectrumPanel(m_container);
    createWaterfallPanel(m_container);
    createBottomBar(m_container);

    // Timer de refresco LVGL @ ~30 FPS (33 ms)
    m_renderTimer = lv_timer_create(onRefreshTimer, 33, this);

    return true;
}

void SdrView::onDestroy() {
    if (m_renderTimer) {
        lv_timer_delete(m_renderTimer);
        m_renderTimer = nullptr;
    }

    if (m_isRunning) {
        cbdos::sdr::getEspSdrClient().stopStreaming();
        m_isRunning = false;
    }

    if (m_waterfallBuf) {
        free(m_waterfallBuf);
        m_waterfallBuf = nullptr;
    }

    BaseView::onDestroy();
}

void SdrView::onShow() {
    BaseView::onShow();
    UIManager::getInstance().getHeaderBar().setTitle("SDR Analyzer");
}

void SdrView::onHide() {
    if (m_isRunning) {
        cbdos::sdr::getEspSdrClient().stopStreaming();
        m_isRunning = false;
        if (m_lblPlayPause) {
            lv_label_set_text(m_lblPlayPause, LV_SYMBOL_PLAY " Iniciar");
        }
        if (m_lblStatus) {
            lv_label_set_text(m_lblStatus, "Pausado");
            lv_obj_set_style_text_color(m_lblStatus, lv_color_hex(0x94A3B8), 0);
        }
    }
    BaseView::onHide();
}

void SdrView::createControlBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_height(bar, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(bar, 12);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bar, 8, 0);
    DefaultTheme::disableScroll(bar);

    // 1. Botón Play/Pausa
    m_btnPlayPause = lv_button_create(bar);
    lv_obj_set_size(m_btnPlayPause, 95, 36);
    DefaultTheme::applyButton(m_btnPlayPause, 8);
    lv_obj_set_style_bg_color(m_btnPlayPause, lv_color_hex(0x37C964), 0);
    lv_obj_add_event_cb(m_btnPlayPause, onPlayPauseClicked, LV_EVENT_CLICKED, this);

    m_lblPlayPause = lv_label_create(m_btnPlayPause);
    lv_label_set_text(m_lblPlayPause, LV_SYMBOL_PLAY " Iniciar");
    lv_obj_center(m_lblPlayPause);

    // 2. Selector de Canales Wi-Fi 2.4 GHz
    m_ddChannels = lv_dropdown_create(bar);
    lv_dropdown_set_options(m_ddChannels,
        "Ch 1 (2412)\nCh 2 (2417)\nCh 3 (2422)\nCh 4 (2427)\n"
        "Ch 5 (2432)\nCh 6 (2437)\nCh 7 (2442)\nCh 8 (2447)\n"
        "Ch 9 (2452)\nCh 10 (2457)\nCh 11 (2462)\nCh 12 (2467)\n"
        "Ch 13 (2472)\nCh 14 (2484)");
    lv_dropdown_set_selected(m_ddChannels, 0);
    lv_obj_set_size(m_ddChannels, 120, 36);
    lv_obj_set_style_radius(m_ddChannels, 8, 0);
    lv_obj_add_event_cb(m_ddChannels, onChannelSelected, LV_EVENT_VALUE_CHANGED, this);

    // 3. Botón de Ganancia AGC/Manual
    m_btnGainMode = lv_button_create(bar);
    lv_obj_set_size(m_btnGainMode, 85, 36);
    DefaultTheme::applyButton(m_btnGainMode, 8);
    lv_obj_add_event_cb(m_btnGainMode, onGainModeClicked, LV_EVENT_CLICKED, this);

    m_lblGainMode = lv_label_create(m_btnGainMode);
    lv_label_set_text(m_lblGainMode, "AGC ON");
    lv_obj_center(m_lblGainMode);

    // 4. Selector de Resolución FFT
    m_ddFftBins = lv_dropdown_create(bar);
    lv_dropdown_set_options(m_ddFftBins, "256\n512\n1024");
    lv_dropdown_set_selected(m_ddFftBins, 1); // 512 bins default
    lv_obj_set_size(m_ddFftBins, 80, 36);
    lv_obj_set_style_radius(m_ddFftBins, 8, 0);
    lv_obj_add_event_cb(m_ddFftBins, onFftBinsSelected, LV_EVENT_VALUE_CHANGED, this);

    // 5. Botón Reset Max-Hold
    lv_obj_t* btnReset = lv_button_create(bar);
    lv_obj_set_size(btnReset, 36, 36);
    DefaultTheme::applyButton(btnReset, 8);
    lv_obj_set_style_bg_color(btnReset, lv_color_hex(0x28334A), 0);
    lv_obj_add_event_cb(btnReset, onResetMaxHoldClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* lblReset = lv_label_create(btnReset);
    lv_label_set_text(lblReset, LV_SYMBOL_REFRESH);
    lv_obj_center(lblReset);
}

void SdrView::createSpectrumPanel(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 210);
    DefaultTheme::applyRaisedCard(card, 12);
    lv_obj_set_style_pad_all(card, 8, 0);
    DefaultTheme::disableScroll(card);

    // Barra superior del panel con títulos y leyendas
    lv_obj_t* topBox = lv_obj_create(card);
    lv_obj_set_size(topBox, LV_PCT(100), 22);
    lv_obj_set_style_bg_opa(topBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(topBox, 0, 0);
    lv_obj_set_style_pad_all(topBox, 0, 0);
    lv_obj_set_flex_flow(topBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBox, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(topBox);

    m_lblCenterFreq = lv_label_create(topBox);
    lv_label_set_text(m_lblCenterFreq, "2412 MHz (Ch 1)");
    lv_obj_set_style_text_color(m_lblCenterFreq, lv_color_hex(0x00F5D4), 0);

    lv_obj_t* legendBox = lv_obj_create(topBox);
    lv_obj_set_size(legendBox, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_bg_opa(legendBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(legendBox, 0, 0);
    lv_obj_set_style_pad_all(legendBox, 0, 0);
    lv_obj_set_flex_flow(legendBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legendBox, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(legendBox);

    lv_obj_t* lblLive = lv_label_create(legendBox);
    lv_label_set_text(lblLive, "● Vivo ");
    lv_obj_set_style_text_color(lblLive, lv_color_hex(0x37C964), 0);

    lv_obj_t* lblMax = lv_label_create(legendBox);
    lv_label_set_text(lblMax, "● Max-Hold");
    lv_obj_set_style_text_color(lblMax, lv_color_hex(0xE6B969), 0);

    // Gráfico de espectro lv_chart
    m_chart = lv_chart_create(card);
    lv_obj_set_size(m_chart, LV_PCT(100), 165);
    lv_obj_align(m_chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_chart_set_type(m_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(m_chart, 128);
    lv_chart_set_range(m_chart, LV_CHART_AXIS_PRIMARY_Y, -120, 0);

    lv_obj_set_style_bg_color(m_chart, lv_color_hex(0x0F131D), 0);
    lv_obj_set_style_border_color(m_chart, lv_color_hex(0x28334A), 0);
    lv_obj_set_style_border_width(m_chart, 1, 0);
    lv_obj_set_style_size(m_chart, 0, 0, LV_PART_INDICATOR); // Ocultar puntos, trazo continuo
    lv_obj_set_style_line_width(m_chart, 2, LV_PART_ITEMS);

    // Series
    m_serMax  = lv_chart_add_series(m_chart, lv_color_hex(0xE6B969), LV_CHART_AXIS_PRIMARY_Y);
    m_serLive = lv_chart_add_series(m_chart, lv_color_hex(0x37C964), LV_CHART_AXIS_PRIMARY_Y);

    // Inicializar valores en el piso de ruido (-120 dBFS)
    for (int i = 0; i < 128; i++) {
        lv_chart_set_value_by_id(m_chart, m_serMax, i, -120);
        lv_chart_set_value_by_id(m_chart, m_serLive, i, -120);
    }
    lv_chart_refresh(m_chart);
}

void SdrView::createWaterfallPanel(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 215);
    DefaultTheme::applyRaisedCard(card, 12);
    lv_obj_set_style_pad_all(card, 6, 0);
    DefaultTheme::disableScroll(card);

    // Canvas de cascada
    m_canvasWaterfall = lv_canvas_create(card);
    lv_obj_set_size(m_canvasWaterfall, kWaterfallW, kWaterfallH);
    lv_obj_align(m_canvasWaterfall, LV_ALIGN_CENTER, 0, 0);

    if (m_waterfallBuf) {
        lv_canvas_set_buffer(m_canvasWaterfall, m_waterfallBuf, kWaterfallW, kWaterfallH, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(m_canvasWaterfall, lv_color_hex(0x000520), LV_OPA_COVER);
    }
}

void SdrView::createBottomBar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_height(bar, LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(bar, 8);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bar, 6, 0);
    DefaultTheme::disableScroll(bar);

    m_lblStatus = lv_label_create(bar);
    lv_label_set_text(m_lblStatus, "Listo (S3 USB)");
    lv_obj_set_style_text_color(m_lblStatus, lv_color_hex(0x00F5D4), 0);

    m_lblTelemetry = lv_label_create(bar);
    lv_label_set_text(m_lblTelemetry, "Pico: -- dBFS | FFT: 0 µs | 0 FPS");
    lv_obj_set_style_text_color(m_lblTelemetry, lv_color_hex(0x94A3B8), 0);
}

void SdrView::updateWaterfallRow(const std::vector<float>& spectrum) {
    if (!m_waterfallBuf || spectrum.empty()) return;

    // 1. Desplazar cascada hacia abajo 1 fila
    memmove(m_waterfallBuf + kWaterfallW, m_waterfallBuf, (kWaterfallH - 1) * kWaterfallW * sizeof(uint16_t));

    // 2. Dibujar fila 0 mapeando el vector de espectro a la LUT RGB565
    size_t specSize = spectrum.size();
    for (int32_t x = 0; x < kWaterfallW; x++) {
        size_t binIdx = ((size_t)x * specSize) / kWaterfallW;
        if (binIdx >= specSize) binIdx = specSize - 1;

        float dbfs = spectrum[binIdx];
        // Rango térmico: -110 dBFS a -20 dBFS
        float norm = (dbfs - (-110.0f)) / 90.0f;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        uint8_t lutIdx = (uint8_t)(norm * 255.0f);
        m_waterfallBuf[x] = s_colormapLut[lutIdx];
    }

    lv_obj_invalidate(m_canvasWaterfall);
}

void SdrView::updateUiFromFrame() {
    if (!m_hasNewFrame.load(std::memory_order_acquire)) return;
    m_hasNewFrame.store(false, std::memory_order_release);

    if (m_sharedSpectrum.empty()) return;

    // 1. Actualizar gráfico de espectro (downsampling a 128 puntos para el chart)
    const int chartPoints = 128;
    size_t specSize = m_sharedSpectrum.size();
    size_t step = specSize / chartPoints;
    if (step == 0) step = 1;

    for (int i = 0; i < chartPoints; i++) {
        size_t startIdx = (size_t)i * step;
        float maxValLive = -120.0f;
        float maxValHold = -120.0f;

        for (size_t s = 0; s < step && (startIdx + s) < specSize; s++) {
            maxValLive = std::max(maxValLive, m_sharedSpectrum[startIdx + s]);
            if (!m_sharedMaxHold.empty() && (startIdx + s) < m_sharedMaxHold.size()) {
                maxValHold = std::max(maxValHold, m_sharedMaxHold[startIdx + s]);
            }
        }
        lv_chart_set_value_by_id(m_chart, m_serLive, i, (int16_t)maxValLive);
        lv_chart_set_value_by_id(m_chart, m_serMax, i, (int16_t)maxValHold);
    }
    lv_chart_refresh(m_chart);

    // 2. Actualizar cascada
    updateWaterfallRow(m_sharedSpectrum);

    // 3. Actualizar telemetría inferior
    const auto& stats = cbdos::sdr::getEspSdrClient().getStats();
    char telemBuf[128];
    snprintf(telemBuf, sizeof(telemBuf), "Pico: %.1f dBFS (%.1f MHz) | DSP: %lu µs | %.1f FPS",
             m_sharedPeakDbfs, m_sharedPeakFreqMHz,
             (unsigned long)m_sharedDspUs,
             stats.fps);
    if (m_lblTelemetry) {
        lv_label_set_text(m_lblTelemetry, telemBuf);
    }
}

void SdrView::onRefreshTimer(lv_timer_t* timer) {
    auto* self = static_cast<SdrView*>(lv_timer_get_user_data(timer));
    if (self && self->isVisible()) {
        self->updateUiFromFrame();
    }
}

void SdrView::onPlayPauseClicked(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self) return;

    auto& client = cbdos::sdr::getEspSdrClient();

    if (!self->m_isRunning) {
        // Iniciar streaming en Core 1
        bool ok = client.startStreaming(self->m_config, [self](const cbdos::sdr::SdrFrame& frame) {
            self->m_sharedSpectrum = frame.spectrumDbfs;
            self->m_sharedMaxHold  = cbdos::sdr::getSdrDspEngine().getMaxHoldSpectrum();
            self->m_sharedPeakDbfs = frame.peakDbfs;
            self->m_sharedPeakFreqMHz = frame.peakFreqMHz;
            self->m_sharedDspUs    = cbdos::sdr::getSdrDspEngine().getLastDspTimeUs();
            self->m_hasNewFrame.store(true, std::memory_order_release);
        });

        if (ok) {
            self->m_isRunning = true;
            lv_label_set_text(self->m_lblPlayPause, LV_SYMBOL_PAUSE " Pausa");
            lv_obj_set_style_bg_color(self->m_btnPlayPause, lv_color_hex(0xEF4444), 0);
            if (self->m_lblStatus) {
                lv_label_set_text(self->m_lblStatus, "En vivo (40 MSa/s)");
                lv_obj_set_style_text_color(self->m_lblStatus, lv_color_hex(0x37C964), 0);
            }
        } else {
            if (self->m_lblStatus) {
                lv_label_set_text(self->m_lblStatus, "Error USB S3");
                lv_obj_set_style_text_color(self->m_lblStatus, lv_color_hex(0xEF4444), 0);
            }
        }
    } else {
        // Pausar streaming
        client.stopStreaming();
        self->m_isRunning = false;
        lv_label_set_text(self->m_lblPlayPause, LV_SYMBOL_PLAY " Iniciar");
        lv_obj_set_style_bg_color(self->m_btnPlayPause, lv_color_hex(0x37C964), 0);
        if (self->m_lblStatus) {
            lv_label_set_text(self->m_lblStatus, "Pausado");
            lv_obj_set_style_text_color(self->m_lblStatus, lv_color_hex(0x94A3B8), 0);
        }
    }
}

void SdrView::onChannelSelected(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddChannels) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddChannels);
    if (sel < kChannelCount) {
        uint32_t freq = kChannels[sel].freqMHz;
        self->m_config.frequencyMHz = freq;
        cbdos::sdr::getEspSdrClient().tune(freq);

        if (self->m_lblCenterFreq) {
            char freqStr[64];
            snprintf(freqStr, sizeof(freqStr), "%lu MHz (Ch %d)", (unsigned long)freq, kChannels[sel].channel);
            lv_label_set_text(self->m_lblCenterFreq, freqStr);
        }
        cbdos::sdr::getSdrDspEngine().reset();
    }
}

void SdrView::onGainModeClicked(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_config.gainMode == cbdos::sdr::GainMode::HardwareAgc) {
        self->m_config.gainMode = cbdos::sdr::GainMode::Manual;
        self->m_config.gainIndex = 40;
        cbdos::sdr::getEspSdrClient().setGain(cbdos::sdr::GainMode::Manual, 40);
        if (self->m_lblGainMode) {
            lv_label_set_text(self->m_lblGainMode, "MAN: 40");
        }
    } else {
        self->m_config.gainMode = cbdos::sdr::GainMode::HardwareAgc;
        cbdos::sdr::getEspSdrClient().setGain(cbdos::sdr::GainMode::HardwareAgc);
        if (self->m_lblGainMode) {
            lv_label_set_text(self->m_lblGainMode, "AGC ON");
        }
    }
}

void SdrView::onFftBinsSelected(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddFftBins) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddFftBins);
    uint16_t bins = 512;
    if (sel == 0) bins = 256;
    else if (sel == 1) bins = 512;
    else if (sel == 2) bins = 1024;

    self->m_config.fftBins = bins;
    cbdos::sdr::getSdrDspEngine().configure({bins, 0.35f, true, 0.05f});
}

void SdrView::onResetMaxHoldClicked(lv_event_t* e) {
    (void)e;
    cbdos::sdr::getSdrDspEngine().resetMaxHold();
}

} // namespace ui
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
