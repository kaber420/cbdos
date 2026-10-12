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

struct FreqPreset {
    const char* label;
    uint32_t freqMHz;
};

static const FreqPreset kPresets[] = {
    {"Ch 1 (2412 MHz)",  2412},
    {"Center (2420 MHz)",2420},
    {"Ch 6 (2437 MHz)",  2437},
    {"Ch 7 (2442 MHz)",  2442},
    {"Ch 11 (2462 MHz)", 2462},
    {"Ch 14 (2484 MHz)", 2484},
    {"2.6 GHz ISM",      2600},
    {"2.7 GHz Max",      2700}
};

static constexpr size_t kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

} // anonymous namespace

uint16_t SdrView::s_colormapLut[256];
bool     SdrView::s_colormapInit = false;

void SdrView::initColormapLut() {
    if (s_colormapInit) return;

    // Gradiente térmico idéntico al visor oficial ESP-WebSDR:
    // 0..63:   Azul profundo -> Cian
    // 64..127: Cian -> Verde brillante
    // 128..191:Verde -> Amarillo intenso
    // 192..255:Amarillo -> Naranja / Blanco cálido
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

        uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        s_colormapLut[i] = rgb565;
    }
    s_colormapInit = true;
}

SdrView::SdrView()
    : BaseView("SDR Analyzer") {
    initColormapLut();

    // Configuración por defecto: 2420 MHz, 80 MS/s - 80 MHz, 2048 bins (On-Chip SPEC)
    m_config.frequencyMHz = 2420;
    m_config.sampleRate   = 80000000;
    m_config.sampleBits   = 8;
    m_config.fftBins      = 2048;
    m_config.gainMode     = cbdos::sdr::GainMode::HardwareAgc;
    m_config.gainIndex    = 40;
    m_config.maxHold      = false;
    m_config.captureMode  = cbdos::sdr::CaptureMode::OnChipSpectrum;

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

    UIManager::getInstance().getHeaderBar().setTitle("SDR Analyzer");

    // Contenedor principal con scroll vertical
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 6, 0);
    lv_obj_set_style_pad_row(m_container, 6, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    createControlBar(m_container);
    createSpectrumPanel(m_container);
    createWaterfallPanel(m_container);
    createBottomBar(m_container);

    updateAxisLabels();

    // Timer de refresco LVGL @ 30 FPS
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
    DefaultTheme::applyRaisedCard(bar, 10);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(bar, 6, 0);
    lv_obj_set_style_pad_row(bar, 6, 0);
    DefaultTheme::disableScroll(bar);

    // --- Fila 1: Play/Pausa + Ajuste Fino Frecuencia (MHz) + Presets ---
    lv_obj_t* row1 = lv_obj_create(bar);
    lv_obj_set_size(row1, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(row1);

    // Botón Iniciar / Pausar
    m_btnPlayPause = lv_button_create(row1);
    lv_obj_set_size(m_btnPlayPause, 90, 36);
    DefaultTheme::applyButton(m_btnPlayPause, 8);
    lv_obj_set_style_bg_color(m_btnPlayPause, lv_color_hex(0x37C964), 0);
    lv_obj_add_event_cb(m_btnPlayPause, onPlayPauseClicked, LV_EVENT_CLICKED, this);

    m_lblPlayPause = lv_label_create(m_btnPlayPause);
    lv_label_set_text(m_lblPlayPause, LV_SYMBOL_PLAY " Start");
    lv_obj_center(m_lblPlayPause);

    // Botón Step -10M
    lv_obj_t* btnM10 = lv_button_create(row1);
    lv_obj_set_size(btnM10, 42, 36);
    DefaultTheme::applyButton(btnM10, 8);
    lv_obj_set_style_bg_color(btnM10, lv_color_hex(0x1F2937), 0);
    lv_obj_set_user_data(btnM10, (void*)(intptr_t)-10);
    lv_obj_add_event_cb(btnM10, onFreqStepClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* lM10 = lv_label_create(btnM10);
    lv_label_set_text(lM10, "-10");
    lv_obj_center(lM10);

    // Botón Step -1M
    lv_obj_t* btnM1 = lv_button_create(row1);
    lv_obj_set_size(btnM1, 38, 36);
    DefaultTheme::applyButton(btnM1, 8);
    lv_obj_set_style_bg_color(btnM1, lv_color_hex(0x1F2937), 0);
    lv_obj_set_user_data(btnM1, (void*)(intptr_t)-1);
    lv_obj_add_event_cb(btnM1, onFreqStepClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* lM1 = lv_label_create(btnM1);
    lv_label_set_text(lM1, "-1");
    lv_obj_center(lM1);

    // Etiqueta Central Frecuencia
    m_lblFreqValue = lv_label_create(row1);
    lv_label_set_text(m_lblFreqValue, "2420 MHz");
    lv_obj_set_style_text_color(m_lblFreqValue, lv_color_hex(0x00F5D4), 0);
    lv_obj_set_style_text_font(m_lblFreqValue, &lv_font_montserrat_14, 0);

    // Botón Step +1M
    lv_obj_t* btnP1 = lv_button_create(row1);
    lv_obj_set_size(btnP1, 38, 36);
    DefaultTheme::applyButton(btnP1, 8);
    lv_obj_set_style_bg_color(btnP1, lv_color_hex(0x1F2937), 0);
    lv_obj_set_user_data(btnP1, (void*)(intptr_t)1);
    lv_obj_add_event_cb(btnP1, onFreqStepClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* lP1 = lv_label_create(btnP1);
    lv_label_set_text(lP1, "+1");
    lv_obj_center(lP1);

    // Botón Step +10M
    lv_obj_t* btnP10 = lv_button_create(row1);
    lv_obj_set_size(btnP10, 42, 36);
    DefaultTheme::applyButton(btnP10, 8);
    lv_obj_set_style_bg_color(btnP10, lv_color_hex(0x1F2937), 0);
    lv_obj_set_user_data(btnP10, (void*)(intptr_t)10);
    lv_obj_add_event_cb(btnP10, onFreqStepClicked, LV_EVENT_CLICKED, this);
    lv_obj_t* lP10 = lv_label_create(btnP10);
    lv_label_set_text(lP10, "+10");
    lv_obj_center(lP10);

    // Presets Dropdown
    m_ddPresets = lv_dropdown_create(row1);
    lv_dropdown_set_options(m_ddPresets,
        "Ch 1 (2412)\n"
        "2420 MHz\n"
        "Ch 6 (2437)\n"
        "Ch 7 (2442)\n"
        "Ch 11 (2462)\n"
        "Ch 14 (2484)\n"
        "2.60 GHz\n"
        "2.70 GHz");
    lv_dropdown_set_selected(m_ddPresets, 1);
    lv_obj_set_size(m_ddPresets, 105, 36);
    lv_obj_set_style_radius(m_ddPresets, 8, 0);
    lv_obj_add_event_cb(m_ddPresets, onPresetFreqSelected, LV_EVENT_VALUE_CHANGED, this);

    // --- Fila 2: Span (Bandwidth) + FFT Bins + Ganancia + Hold ---
    lv_obj_t* row2 = lv_obj_create(bar);
    lv_obj_set_size(row2, LV_PCT(100), 38);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(row2);

    // Dropdown Span / Sample rate
    m_ddSpan = lv_dropdown_create(row2);
    lv_dropdown_set_options(m_ddSpan, "80 MHz\n40 MHz\n20 MHz\n10 MHz\n4 MHz");
    lv_dropdown_set_selected(m_ddSpan, 0); // 80 MHz default
    lv_obj_set_size(m_ddSpan, 105, 36);
    lv_obj_set_style_radius(m_ddSpan, 8, 0);
    lv_obj_add_event_cb(m_ddSpan, onSpanSelected, LV_EVENT_VALUE_CHANGED, this);

    // Dropdown FFT Bins
    m_ddFftBins = lv_dropdown_create(row2);
    lv_dropdown_set_options(m_ddFftBins, "2048\n1024\n512\n256");
    lv_dropdown_set_selected(m_ddFftBins, 0); // 2048 bins default
    lv_obj_set_size(m_ddFftBins, 90, 36);
    lv_obj_set_style_radius(m_ddFftBins, 8, 0);
    lv_obj_add_event_cb(m_ddFftBins, onFftBinsSelected, LV_EVENT_VALUE_CHANGED, this);

    // Botón Gain AGC / Manual
    m_btnGainMode = lv_button_create(row2);
    lv_obj_set_size(m_btnGainMode, 85, 36);
    DefaultTheme::applyButton(m_btnGainMode, 8);
    lv_obj_add_event_cb(m_btnGainMode, onGainModeClicked, LV_EVENT_CLICKED, this);

    m_lblGainMode = lv_label_create(m_btnGainMode);
    lv_label_set_text(m_lblGainMode, "AGC ON");
    lv_obj_center(m_lblGainMode);

    // Botón Max Hold Toggle
    m_btnHold = lv_button_create(row2);
    lv_obj_set_size(m_btnHold, 80, 36);
    DefaultTheme::applyButton(m_btnHold, 8);
    lv_obj_set_style_bg_color(m_btnHold, lv_color_hex(0xE6B969), 0);
    lv_obj_add_event_cb(m_btnHold, onResetMaxHoldClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* lblHold = lv_label_create(m_btnHold);
    lv_label_set_text(lblHold, "Hold ON");
    lv_obj_center(lblHold);
}

void SdrView::createSpectrumPanel(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 235);
    DefaultTheme::applyRaisedCard(card, 10);
    lv_obj_set_style_pad_all(card, 6, 0);
    DefaultTheme::disableScroll(card);

    // Encabezado del Espectro
    lv_obj_t* head = lv_obj_create(card);
    lv_obj_set_size(head, LV_PCT(100), 20);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(head);

    lv_obj_t* title = lv_label_create(head);
    lv_label_set_text(title, "Power spectrum");
    lv_obj_set_style_text_color(title, lv_color_hex(0xC0C9CD), 0);

    m_lblCursor = lv_label_create(head);
    lv_label_set_text(m_lblCursor, "2420.0 MHz · -- dBFS");
    lv_obj_set_style_text_color(m_lblCursor, lv_color_hex(0x00F5D4), 0);

    // Contenedor del Gráfico con Eje Y izquierdo
    m_chartContainer = lv_obj_create(card);
    lv_obj_set_size(m_chartContainer, LV_PCT(100), 140);
    lv_obj_set_style_bg_opa(m_chartContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_chartContainer, 0, 0);
    lv_obj_set_style_pad_all(m_chartContainer, 0, 0);
    lv_obj_set_flex_flow(m_chartContainer, LV_FLEX_FLOW_ROW);
    DefaultTheme::disableScroll(m_chartContainer);

    // Columna Eje Y (dBFS)
    lv_obj_t* axisY = lv_obj_create(m_chartContainer);
    lv_obj_set_size(axisY, 32, LV_PCT(100));
    lv_obj_set_style_bg_opa(axisY, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(axisY, 0, 0);
    lv_obj_set_style_pad_all(axisY, 0, 0);
    lv_obj_set_flex_flow(axisY, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(axisY, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    DefaultTheme::disableScroll(axisY);

    const char* yVals[] = {"-20", "-40", "-60", "-80", "-100"};
    for (int i = 0; i < 5; i++) {
        m_lblAxisY[i] = lv_label_create(axisY);
        lv_label_set_text(m_lblAxisY[i], yVals[i]);
        lv_obj_set_style_text_color(m_lblAxisY[i], lv_color_hex(0x718096), 0);
    }

    // Gráfico de Espectro
    m_chart = lv_chart_create(m_chartContainer);
    lv_obj_set_size(m_chart, LV_PCT(92), LV_PCT(100));
    lv_chart_set_type(m_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(m_chart, 160);
    // Rango dinámico oficial: -100 a -20 dBFS (80 dB span)
    lv_chart_set_range(m_chart, LV_CHART_AXIS_PRIMARY_Y, -100, -20);
    lv_chart_set_div_line_count(m_chart, 5, 9); // Grid lines horizontal y vertical

    lv_obj_set_style_bg_color(m_chart, lv_color_hex(0x182126), 0);
    lv_obj_set_style_border_color(m_chart, lv_color_hex(0x293135), 0);
    lv_obj_set_style_border_width(m_chart, 1, 0);
    lv_obj_set_style_line_color(m_chart, lv_color_hex(0x243038), LV_PART_MAIN); // Grid
    lv_obj_set_style_size(m_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(m_chart, 2, LV_PART_ITEMS);

    // Eventos táctiles para arrastrar horizontalmente (Pan & Drag)
    lv_obj_add_flag(m_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_chart, onChartTouch, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(m_chart, onChartTouch, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(m_chart, onChartTouch, LV_EVENT_RELEASED, this);

    // Series: Live (Verde ESPARGOS #37C964) y Max-Hold (Dorado #E6B969)
    m_serMax  = lv_chart_add_series(m_chart, lv_color_hex(0xE6B969), LV_CHART_AXIS_PRIMARY_Y);
    m_serLive = lv_chart_add_series(m_chart, lv_color_hex(0x37C964), LV_CHART_AXIS_PRIMARY_Y);

    for (int i = 0; i < 160; i++) {
        lv_chart_set_value_by_id(m_chart, m_serMax, i, -100);
        lv_chart_set_value_by_id(m_chart, m_serLive, i, -100);
    }
    lv_chart_refresh(m_chart);

    // Eje X Inferior (Marcas de Frecuencia en MHz)
    lv_obj_t* axisX = lv_obj_create(card);
    lv_obj_set_size(axisX, LV_PCT(100), 20);
    lv_obj_set_style_bg_opa(axisX, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(axisX, 0, 0);
    lv_obj_set_style_pad_left(axisX, 32, 0);
    lv_obj_set_style_pad_right(axisX, 4, 0);
    lv_obj_set_style_pad_top(axisX, 0, 0);
    lv_obj_set_style_pad_bottom(axisX, 0, 0);
    lv_obj_set_flex_flow(axisX, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(axisX, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(axisX);

    for (int i = 0; i < 5; i++) {
        m_lblAxisX[i] = lv_label_create(axisX);
        lv_label_set_text(m_lblAxisX[i], "----");
        if (i == 2) {
            lv_obj_set_style_text_color(m_lblAxisX[i], lv_color_hex(0xE0A040), 0); // Centro naranja
        } else {
            lv_obj_set_style_text_color(m_lblAxisX[i], lv_color_hex(0x8F9CA2), 0);
        }
    }

    // Barra de Medidas ("Meas Bar") estilo oficial ESP-WebSDR
    m_lblMeasBar = lv_label_create(card);
    lv_obj_set_size(m_lblMeasBar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(m_lblMeasBar, lv_color_hex(0x8F9CA2), 0);
    lv_label_set_text(m_lblMeasBar, "Start: 2.380 GHz · Center: 2.420 GHz · Span: 80.0 MHz · Stop: 2.460 GHz · Bins: 2048");
}

void SdrView::createWaterfallPanel(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 195);
    DefaultTheme::applyRaisedCard(card, 10);
    lv_obj_set_style_pad_all(card, 6, 0);
    DefaultTheme::disableScroll(card);

    // Encabezado
    lv_obj_t* head = lv_obj_create(card);
    lv_obj_set_size(head, LV_PCT(100), 20);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(head);

    lv_obj_t* title = lv_label_create(head);
    lv_label_set_text(title, "Waterfall");
    lv_obj_set_style_text_color(title, lv_color_hex(0xC0C9CD), 0);

    lv_obj_t* scale = lv_label_create(head);
    lv_label_set_text(scale, "-100 → -20 dBFS");
    lv_obj_set_style_text_color(scale, lv_color_hex(0x8F9CA2), 0);

    // Canvas de cascada
    m_canvasWaterfall = lv_canvas_create(card);
    lv_obj_set_size(m_canvasWaterfall, kWaterfallW, kWaterfallH);
    lv_obj_align(m_canvasWaterfall, LV_ALIGN_BOTTOM_MID, 0, 0);

    // También permitir arrastre sobre la cascada
    lv_obj_add_flag(m_canvasWaterfall, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_canvasWaterfall, onChartTouch, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(m_canvasWaterfall, onChartTouch, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(m_canvasWaterfall, onChartTouch, LV_EVENT_RELEASED, this);

    if (m_waterfallBuf) {
        lv_canvas_set_buffer(m_canvasWaterfall, m_waterfallBuf, kWaterfallW, kWaterfallH, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(m_canvasWaterfall, lv_color_hex(0x11191E), LV_OPA_COVER);
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
    lv_label_set_text(m_lblStatus, "Listo (S3 USB Host)");
    lv_obj_set_style_text_color(m_lblStatus, lv_color_hex(0x00F5D4), 0);

    m_lblTelemetry = lv_label_create(bar);
    lv_label_set_text(m_lblTelemetry, "200+ spectra/s · 80 MS/s on chip");
    lv_obj_set_style_text_color(m_lblTelemetry, lv_color_hex(0x94A3B8), 0);
}

void SdrView::updateAxisLabels() {
    uint32_t centerFreq = m_config.frequencyMHz;
    uint32_t spanMHz = m_config.sampleRate / 1000000;
    if (spanMHz == 0) spanMHz = 80;

    uint32_t startFreq = (centerFreq > spanMHz / 2) ? (centerFreq - spanMHz / 2) : 0;
    uint32_t stopFreq  = centerFreq + spanMHz / 2;

    // Actualizar etiqueta central del encabezado
    if (m_lblFreqValue) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu MHz", (unsigned long)centerFreq);
        lv_label_set_text(m_lblFreqValue, buf);
    }

    // Actualizar marcas X (Start, 1/4, Center, 3/4, Stop)
    uint32_t fSteps[5] = {
        startFreq,
        startFreq + spanMHz / 4,
        centerFreq,
        startFreq + (spanMHz * 3) / 4,
        stopFreq
    };

    for (int i = 0; i < 5; i++) {
        if (m_lblAxisX[i]) {
            char buf[32];
            if (i == 2) {
                snprintf(buf, sizeof(buf), "▲ %lu", (unsigned long)fSteps[i]);
            } else {
                snprintf(buf, sizeof(buf), "%lu", (unsigned long)fSteps[i]);
            }
            lv_label_set_text(m_lblAxisX[i], buf);
        }
    }

    // Actualizar MeasBar
    if (m_lblMeasBar) {
        char meas[128];
        snprintf(meas, sizeof(meas), "Start %.3f GHz · Center %.3f GHz · Span %lu MHz · Stop %.3f GHz · Bins %u",
                 (float)startFreq / 1000.0f,
                 (float)centerFreq / 1000.0f,
                 (unsigned long)spanMHz,
                 (float)stopFreq / 1000.0f,
                 (unsigned)m_config.fftBins);
        lv_label_set_text(m_lblMeasBar, meas);
    }
}

void SdrView::tuneToFrequency(uint32_t freqMHz) {
    if (freqMHz < 100) freqMHz = 100;
    if (freqMHz > 2750) freqMHz = 2750;

    m_config.frequencyMHz = freqMHz;
    updateAxisLabels();

    auto& client = cbdos::sdr::getEspSdrClient();
    if (m_isRunning) {
        client.stopStreaming();
        client.tune(freqMHz);
        client.startStreaming(m_config, [this](const cbdos::sdr::SdrFrame& frame) {
            this->m_sharedSpectrum = frame.spectrumDbfs;
            this->m_sharedPeakDbfs = frame.peakDbfs;
            this->m_sharedPeakFreqMHz = frame.peakFreqMHz;
            this->m_hasNewFrame.store(true, std::memory_order_release);
        });
    } else {
        client.tune(freqMHz);
    }
}

void SdrView::updateWaterfallRow(const std::vector<float>& spectrum) {
    if (!m_waterfallBuf || spectrum.empty()) return;

    // Desplazar cascada 1 fila hacia abajo
    memmove(m_waterfallBuf + kWaterfallW, m_waterfallBuf, (kWaterfallH - 1) * kWaterfallW * sizeof(uint16_t));

    // Mapeo térmico calibrado exactamente al rango oficial: Floor -100 dBFS, Range 80 dB (-20 dBFS top)
    size_t specSize = spectrum.size();
    for (int32_t x = 0; x < kWaterfallW; x++) {
        size_t binIdx = ((size_t)x * specSize) / kWaterfallW;
        if (binIdx >= specSize) binIdx = specSize - 1;

        float dbfs = spectrum[binIdx];
        float norm = (dbfs - (-100.0f)) / 80.0f;
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

    size_t specSize = m_sharedSpectrum.size();

    // Actualizar Max-Hold con decaimiento suave
    if (m_holdEnabled) {
        if (m_sharedMaxHold.size() != specSize) {
            m_sharedMaxHold = m_sharedSpectrum;
        } else {
            for (size_t k = 0; k < specSize; k++) {
                if (m_sharedSpectrum[k] > m_sharedMaxHold[k]) {
                    m_sharedMaxHold[k] = m_sharedSpectrum[k];
                } else {
                    m_sharedMaxHold[k] -= 0.04f;
                }
            }
        }
    }

    // 1. Actualizar gráfico del espectro (160 puntos para ajustarse al ancho)
    const int chartPoints = 160;
    size_t step = specSize / chartPoints;
    if (step == 0) step = 1;

    for (int i = 0; i < chartPoints; i++) {
        size_t startIdx = (size_t)i * step;
        float maxValLive = -100.0f;
        float maxValHold = -100.0f;

        for (size_t s = 0; s < step && (startIdx + s) < specSize; s++) {
            float vLive = m_sharedSpectrum[startIdx + s];
            if (vLive > maxValLive) maxValLive = vLive;

            if (m_holdEnabled && (startIdx + s) < m_sharedMaxHold.size()) {
                float vHold = m_sharedMaxHold[startIdx + s];
                if (vHold > maxValHold) maxValHold = vHold;
            }
        }

        int32_t valLive = (int32_t)std::round(std::max(-100.0f, std::min(-20.0f, maxValLive)));
        lv_chart_set_value_by_id(m_chart, m_serLive, i, valLive);

        if (m_holdEnabled) {
            int32_t valHold = (int32_t)std::round(std::max(-100.0f, std::min(-20.0f, maxValHold)));
            lv_chart_set_value_by_id(m_chart, m_serMax, i, valHold);
        } else {
            lv_chart_set_value_by_id(m_chart, m_serMax, i, -100);
        }
    }
    lv_chart_refresh(m_chart);

    // 2. Actualizar fila de cascada
    updateWaterfallRow(m_sharedSpectrum);

    // 3. Telemetría y lectura del pico
    if (m_lblCursor) {
        char cur[64];
        snprintf(cur, sizeof(cur), "%.1f MHz · %.1f dBFS", m_sharedPeakFreqMHz, m_sharedPeakDbfs);
        lv_label_set_text(m_lblCursor, cur);
    }

    const auto& stats = cbdos::sdr::getEspSdrClient().getStats();
    if (m_lblTelemetry) {
        char telem[96];
        snprintf(telem, sizeof(telem), "%.0f spectra/s · %.0f kB/s USB", stats.fps, stats.throughputKsps * 2.0f);
        lv_label_set_text(m_lblTelemetry, telem);
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
        bool ok = client.startStreaming(self->m_config, [self](const cbdos::sdr::SdrFrame& frame) {
            self->m_sharedSpectrum = frame.spectrumDbfs;
            self->m_sharedPeakDbfs = frame.peakDbfs;
            self->m_sharedPeakFreqMHz = frame.peakFreqMHz;
            self->m_hasNewFrame.store(true, std::memory_order_release);
        });

        if (ok) {
            self->m_isRunning = true;
            lv_label_set_text(self->m_lblPlayPause, LV_SYMBOL_PAUSE " Pause");
            lv_obj_set_style_bg_color(self->m_btnPlayPause, lv_color_hex(0xEF4444), 0);
            if (self->m_lblStatus) {
                lv_label_set_text(self->m_lblStatus, "SPEC On-Chip");
                lv_obj_set_style_text_color(self->m_lblStatus, lv_color_hex(0x37C964), 0);
            }
        } else {
            if (self->m_lblStatus) {
                lv_label_set_text(self->m_lblStatus, "Error USB S3");
                lv_obj_set_style_text_color(self->m_lblStatus, lv_color_hex(0xEF4444), 0);
            }
        }
    } else {
        client.stopStreaming();
        self->m_isRunning = false;
        lv_label_set_text(self->m_lblPlayPause, LV_SYMBOL_PLAY " Start");
        lv_obj_set_style_bg_color(self->m_btnPlayPause, lv_color_hex(0x37C964), 0);
        if (self->m_lblStatus) {
            lv_label_set_text(self->m_lblStatus, "Pausado");
            lv_obj_set_style_text_color(self->m_lblStatus, lv_color_hex(0x94A3B8), 0);
        }
    }
}

void SdrView::onFreqStepClicked(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    intptr_t stepVal = (intptr_t)lv_obj_get_user_data(target);
    if (stepVal == 0) return;

    int32_t currentFreq = (int32_t)self->m_config.frequencyMHz;
    int32_t newFreq = currentFreq + (int32_t)stepVal;
    self->tuneToFrequency((uint32_t)newFreq);
}

void SdrView::onPresetFreqSelected(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddPresets) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddPresets);
    if (sel < kPresetCount) {
        self->tuneToFrequency(kPresets[sel].freqMHz);
    }
}

void SdrView::onSpanSelected(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddSpan) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddSpan);
    uint32_t rates[] = {80000000, 40000000, 20000000, 10000000, 4000000};
    if (sel < sizeof(rates) / sizeof(rates[0])) {
        self->m_config.sampleRate = rates[sel];
        self->tuneToFrequency(self->m_config.frequencyMHz);
    }
}

void SdrView::onFftBinsSelected(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddFftBins) return;

    uint32_t sel = lv_dropdown_get_selected(self->m_ddFftBins);
    uint16_t bins[] = {2048, 1024, 512, 256};
    if (sel < sizeof(bins) / sizeof(bins[0])) {
        self->m_config.fftBins = bins[sel];
        self->tuneToFrequency(self->m_config.frequencyMHz);
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

void SdrView::onResetMaxHoldClicked(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->m_holdEnabled = !self->m_holdEnabled;
    self->m_sharedMaxHold.clear();

    if (self->m_btnHold) {
        if (self->m_holdEnabled) {
            lv_obj_set_style_bg_color(self->m_btnHold, lv_color_hex(0xE6B969), 0);
            lv_obj_t* lbl = lv_obj_get_child(self->m_btnHold, 0);
            if (lbl) lv_label_set_text(lbl, "Hold ON");
        } else {
            lv_obj_set_style_bg_color(self->m_btnHold, lv_color_hex(0x28334A), 0);
            lv_obj_t* lbl = lv_obj_get_child(self->m_btnHold, 0);
            if (lbl) lv_label_set_text(lbl, "Hold OFF");
        }
    }
}

void SdrView::onChartTouch(lv_event_t* e) {
    auto* self = static_cast<SdrView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) indev = lv_indev_active();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    uint32_t spanMHz = self->m_config.sampleRate / 1000000;
    if (spanMHz == 0) spanMHz = 80;

    if (code == LV_EVENT_PRESSED) {
        self->m_dragStartX = p.x;
        self->m_dragStartFreq = self->m_config.frequencyMHz;
        self->m_isDragging = true;
    } else if (code == LV_EVENT_PRESSING) {
        if (!self->m_isDragging) return;
        int32_t dx = p.x - self->m_dragStartX;
        // Escala: 380 píxeles útiles equivalen a spanMHz
        int32_t deltaMHz = -(dx * (int32_t)spanMHz) / 380;
        int32_t newFreq = (int32_t)self->m_dragStartFreq + deltaMHz;
        if (newFreq < 100) newFreq = 100;
        if (newFreq > 2750) newFreq = 2750;

        // Actualización reactiva inmediata en pantalla
        if (self->m_lblFreqValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%ld MHz", (long)newFreq);
            lv_label_set_text(self->m_lblFreqValue, buf);
        }
        uint32_t halfSpan = spanMHz / 2;
        if (self->m_lblAxisX[0]) {
            char b[32];
            snprintf(b, sizeof(b), "%ld", (long)(newFreq - halfSpan));
            lv_label_set_text(self->m_lblAxisX[0], b);
        }
        if (self->m_lblAxisX[2]) {
            char b[32];
            snprintf(b, sizeof(b), "▲ %ld", (long)newFreq);
            lv_label_set_text(self->m_lblAxisX[2], b);
        }
        if (self->m_lblAxisX[4]) {
            char b[32];
            snprintf(b, sizeof(b), "%ld", (long)(newFreq + halfSpan));
            lv_label_set_text(self->m_lblAxisX[4], b);
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (self->m_isDragging) {
            int32_t dx = p.x - self->m_dragStartX;
            int32_t deltaMHz = -(dx * (int32_t)spanMHz) / 380;
            int32_t newFreq = (int32_t)self->m_dragStartFreq + deltaMHz;
            if (newFreq < 100) newFreq = 100;
            if (newFreq > 2750) newFreq = 2750;

            self->m_isDragging = false;
            self->tuneToFrequency((uint32_t)newFreq);
        }
    }
}

} // namespace ui
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
