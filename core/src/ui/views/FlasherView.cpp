#include "cbdos_build_profile.h"
#if CBDOS_FEATURE_FLASHER

#include "FlasherView.hpp"
#include "../themes/DefaultTheme.h"
#include "../UIManager.hpp"
#include "../components/HeaderBar.hpp"
#include "cbdos/flasher.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include "cbdos/storage.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

namespace cbdos {
namespace ui {

static FlasherView* s_activeFlasherView = nullptr;

struct PinOption {
    int gpio;
    const char* label;
};

static const std::vector<PinOption> s_txPins = {
    {32, "TX: GPIO 32 (JP1 Pin 19)"},
    {28, "TX: GPIO 28 (JP1 Pin 21)"},
    {35, "TX: GPIO 35 (JP1 Pin 15)"},
    {49, "TX: GPIO 49 (JP1 Pin 13)"},
    {50, "TX: GPIO 50 (JP1 Pin 11)"},
    {51, "TX: GPIO 51 (JP1 Pin 9)"},
    {52, "TX: GPIO 52 (JP1 Pin 7)"},
    {29, "TX: GPIO 29 (JP1 Pin 14)"},
    {30, "TX: GPIO 30 (JP1 Pin 12)"},
    {31, "TX: GPIO 31 (JP1 Pin 10)"},
    {33, "TX: GPIO 33 (JP1 Pin 8)"},
    {15, "TX: GPIO 15 (S3 Header)"}
};

static const std::vector<PinOption> s_rxPins = {
    {28, "RX: GPIO 28 (JP1 Pin 21)"},
    {32, "RX: GPIO 32 (JP1 Pin 19)"},
    {35, "RX: GPIO 35 (JP1 Pin 15)"},
    {49, "RX: GPIO 49 (JP1 Pin 13)"},
    {50, "RX: GPIO 50 (JP1 Pin 11)"},
    {51, "RX: GPIO 51 (JP1 Pin 9)"},
    {52, "RX: GPIO 52 (JP1 Pin 7)"},
    {29, "RX: GPIO 29 (JP1 Pin 14)"},
    {30, "RX: GPIO 30 (JP1 Pin 12)"},
    {31, "RX: GPIO 31 (JP1 Pin 10)"},
    {33, "RX: GPIO 33 (JP1 Pin 8)"},
    {16, "RX: GPIO 16 (S3 Header)"}
};

static const std::vector<PinOption> s_bootPins = {
    {34, "BOOT: GPIO 34 (C6 IO9 Auto)"},
    {-1, "BOOT: Manual (Sin Pin)"},
    {0,  "BOOT: GPIO 0 (S3)"},
    {35, "BOOT: GPIO 35 (JP1 Pin 15)"},
    {49, "BOOT: GPIO 49 (JP1 Pin 13)"},
    {50, "BOOT: GPIO 50 (JP1 Pin 11)"},
    {51, "BOOT: GPIO 51 (JP1 Pin 9)"},
    {52, "BOOT: GPIO 52 (JP1 Pin 7)"}
};

static const std::vector<PinOption> s_rstPins = {
    {54, "RST: GPIO 54 (C6 RST Auto)"},
    {-1, "RST: Manual (Sin Pin)"},
    {35, "RST: GPIO 35 (JP1 Pin 15)"},
    {49, "RST: GPIO 49 (JP1 Pin 13)"},
    {50, "RST: GPIO 50 (JP1 Pin 11)"},
    {51, "RST: GPIO 51 (JP1 Pin 9)"},
    {52, "RST: GPIO 52 (JP1 Pin 7)"}
};

static std::string buildDropdownString(const std::vector<PinOption>& list) {
    std::string s = "";
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) s += "\n";
        s += list[i].label;
    }
    return s;
}

static int findPinIndex(const std::vector<PinOption>& list, int gpio) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].gpio == gpio) return (int)i;
    }
    return 0;
}

FlasherView::FlasherView()
    : BaseView("Flasheador Universal") {
}

bool FlasherView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;
    s_activeFlasherView = this;

    m_presets = cbdos::flasher::getPresets();
    m_selectedPresetIndex = 0;
    m_currentConfig = cbdos::flasher::getDefaultConfig();

    // Contenedor principal scrollable
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 12, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_container, 12, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // ─────────────────────────────────────────────────────────────
    // 1. Tarjeta de Selección de Preset
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardPreset = lv_obj_create(m_container);
    lv_obj_set_width(cardPreset, lv_pct(100));
    lv_obj_set_height(cardPreset, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardPreset, 14);
    lv_obj_set_flex_flow(cardPreset, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardPreset, 12, 0);
    lv_obj_set_style_pad_row(cardPreset, 8, 0);

    lv_obj_t* lblPresetTitle = lv_label_create(cardPreset);
    lv_label_set_text(lblPresetTitle, LV_SYMBOL_LIST " Plantilla / Preset de Hardware:");
    lv_obj_set_style_text_font(lblPresetTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblPresetTitle, DefaultTheme::getPrimaryAccent(), 0);

    std::string dropdownOptions = "";
    for (size_t i = 0; i < m_presets.size(); ++i) {
        if (i > 0) dropdownOptions += "\n";
        dropdownOptions += m_presets[i].name;
    }

    m_ddPresets = lv_dropdown_create(cardPreset);
    lv_dropdown_set_options(m_ddPresets, dropdownOptions.c_str());
    lv_obj_set_width(m_ddPresets, lv_pct(100));
    lv_obj_set_style_bg_color(m_ddPresets, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(m_ddPresets, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(m_ddPresets, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_ddPresets, 1, 0);
    lv_obj_set_style_text_font(m_ddPresets, &lv_font_montserrat_14, 0);
    lv_dropdown_set_selected(m_ddPresets, m_selectedPresetIndex);
    lv_obj_add_event_cb(m_ddPresets, presetChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_lblPresetDesc = lv_label_create(cardPreset);
    lv_obj_set_width(m_lblPresetDesc, lv_pct(100));
    lv_obj_set_style_text_font(m_lblPresetDesc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblPresetDesc, DefaultTheme::getMutedTextColor(), 0);

    // ─────────────────────────────────────────────────────────────
    // 2. Tarjeta de Conexiones / Diagrama de Hardware
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardWiring = lv_obj_create(m_container);
    lv_obj_set_width(cardWiring, lv_pct(100));
    lv_obj_set_height(cardWiring, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardWiring, 14);
    lv_obj_set_flex_flow(cardWiring, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardWiring, 12, 0);
    lv_obj_set_style_pad_row(cardWiring, 6, 0);

    lv_obj_t* lblWiringHeader = lv_label_create(cardWiring);
    lv_label_set_text(lblWiringHeader, LV_SYMBOL_SETTINGS " Guia y Diagrama de Conexion:");
    lv_obj_set_style_text_font(lblWiringHeader, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblWiringHeader, DefaultTheme::getPrimaryAccent(), 0);

    lv_obj_t* wiringBox = lv_obj_create(cardWiring);
    lv_obj_set_width(wiringBox, lv_pct(100));
    lv_obj_set_height(wiringBox, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(wiringBox, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(wiringBox, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(wiringBox, 1, 0);
    lv_obj_set_style_radius(wiringBox, 8, 0);
    lv_obj_set_style_pad_all(wiringBox, 8, 0);

    m_lblWiring = lv_label_create(wiringBox);
    lv_obj_set_width(m_lblWiring, lv_pct(100));
    lv_obj_set_style_text_font(m_lblWiring, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblWiring, lv_color_hex(0x38BDF8), 0);

    // ─────────────────────────────────────────────────────────────
    // 3. Tarjeta de Mapeo de Pines & Parámetros (Dropdowns Seguros)
    // ─────────────────────────────────────────────────────────────
    m_cardPinConfig = lv_obj_create(m_container);
    lv_obj_set_width(m_cardPinConfig, lv_pct(100));
    lv_obj_set_height(m_cardPinConfig, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(m_cardPinConfig, 14);
    lv_obj_set_flex_flow(m_cardPinConfig, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_cardPinConfig, 12, 0);
    lv_obj_set_style_pad_row(m_cardPinConfig, 8, 0);

    lv_obj_t* lblPinTitle = lv_label_create(m_cardPinConfig);
    lv_label_set_text(lblPinTitle, LV_SYMBOL_EDIT " Configuración de Pines y UART:");
    lv_obj_set_style_text_font(lblPinTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblPinTitle, DefaultTheme::getPrimaryAccent(), 0);

    // Fila 1: TX y RX Dropdowns
    lv_obj_t* row1 = lv_obj_create(m_cardPinConfig);
    lv_obj_set_size(row1, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row1, 0, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);

    // TX Dropdown
    m_ddTxPin = lv_dropdown_create(row1);
    lv_dropdown_set_options(m_ddTxPin, buildDropdownString(s_txPins).c_str());
    lv_obj_set_width(m_ddTxPin, lv_pct(48));
    lv_obj_set_style_bg_color(m_ddTxPin, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(m_ddTxPin, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(m_ddTxPin, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_ddTxPin, 1, 0);
    lv_obj_set_style_text_font(m_ddTxPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddTxPin, pinDropdownChangedCb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)1);

    // RX Dropdown
    m_ddRxPin = lv_dropdown_create(row1);
    lv_dropdown_set_options(m_ddRxPin, buildDropdownString(s_rxPins).c_str());
    lv_obj_set_width(m_ddRxPin, lv_pct(48));
    lv_obj_set_style_bg_color(m_ddRxPin, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(m_ddRxPin, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(m_ddRxPin, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_ddRxPin, 1, 0);
    lv_obj_set_style_text_font(m_ddRxPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddRxPin, pinDropdownChangedCb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)2);

    // Fila 2: BOOT y RST Dropdowns
    lv_obj_t* row2 = lv_obj_create(m_cardPinConfig);
    lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row2, 0, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);

    // BOOT Dropdown
    m_ddBootPin = lv_dropdown_create(row2);
    lv_dropdown_set_options(m_ddBootPin, buildDropdownString(s_bootPins).c_str());
    lv_obj_set_width(m_ddBootPin, lv_pct(48));
    lv_obj_set_style_bg_color(m_ddBootPin, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(m_ddBootPin, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(m_ddBootPin, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_ddBootPin, 1, 0);
    lv_obj_set_style_text_font(m_ddBootPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddBootPin, pinDropdownChangedCb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)3);

    // RST Dropdown
    m_ddRstPin = lv_dropdown_create(row2);
    lv_dropdown_set_options(m_ddRstPin, buildDropdownString(s_rstPins).c_str());
    lv_obj_set_width(m_ddRstPin, lv_pct(48));
    lv_obj_set_style_bg_color(m_ddRstPin, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(m_ddRstPin, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(m_ddRstPin, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_ddRstPin, 1, 0);
    lv_obj_set_style_text_font(m_ddRstPin, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddRstPin, pinDropdownChangedCb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)4);

    // Fila 3: Baudrate
    lv_obj_t* row3 = lv_obj_create(m_cardPinConfig);
    lv_obj_set_size(row3, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row3, 0, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);

    lv_obj_t* lblBaudTitle = lv_label_create(row3);
    lv_label_set_text(lblBaudTitle, "Velocidad UART:");
    lv_obj_set_style_text_font(lblBaudTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblBaudTitle, DefaultTheme::getTextColor(), 0);

    m_ddBaud = lv_dropdown_create(row3);
    lv_dropdown_set_options(m_ddBaud, "115200\n230400\n460800\n921600");
    lv_obj_set_width(m_ddBaud, lv_pct(55));
    lv_obj_set_style_bg_color(m_ddBaud, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(m_ddBaud, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(m_ddBaud, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddBaud, baudChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // ─────────────────────────────────────────────────────────────
    // 4. Tarjeta de Selección de Archivo Firmware (.bin)
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardFirmware = lv_obj_create(m_container);
    lv_obj_set_width(cardFirmware, lv_pct(100));
    lv_obj_set_height(cardFirmware, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardFirmware, 14);
    lv_obj_set_flex_flow(cardFirmware, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardFirmware, 12, 0);
    lv_obj_set_style_pad_row(cardFirmware, 8, 0);

    lv_obj_t* rowFwHeader = lv_obj_create(cardFirmware);
    lv_obj_set_size(rowFwHeader, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rowFwHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowFwHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(rowFwHeader, 0, 0);
    lv_obj_set_style_border_width(rowFwHeader, 0, 0);
    lv_obj_set_style_pad_all(rowFwHeader, 0, 0);

    lv_obj_t* lblFwTitle = lv_label_create(rowFwHeader);
    lv_label_set_text(lblFwTitle, LV_SYMBOL_FILE " Firmware a Grabar:");
    lv_obj_set_style_text_font(lblFwTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblFwTitle, DefaultTheme::getPrimaryAccent(), 0);

    m_btnPickFile = lv_button_create(rowFwHeader);
    lv_obj_set_width(m_btnPickFile, lv_pct(45));
    lv_obj_set_height(m_btnPickFile, 36);
    DefaultTheme::applyButton(m_btnPickFile, 8);
    lv_obj_set_style_bg_color(m_btnPickFile, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(m_btnPickFile, pickFileBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblPick = lv_label_create(m_btnPickFile);
    lv_label_set_text(lblPick, LV_SYMBOL_DIRECTORY " Explorar SD");
    lv_obj_set_style_text_font(lblPick, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPick, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblPick);

    // Contenedor visual para mostrar el archivo seleccionado
    lv_obj_t* fwPathBox = lv_obj_create(cardFirmware);
    lv_obj_set_width(fwPathBox, lv_pct(100));
    lv_obj_set_height(fwPathBox, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(fwPathBox, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(fwPathBox, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(fwPathBox, 1, 0);
    lv_obj_set_style_radius(fwPathBox, 8, 0);
    lv_obj_set_style_pad_all(fwPathBox, 8, 0);

    m_lblFirmwareSource = lv_label_create(fwPathBox);
    lv_obj_set_width(m_lblFirmwareSource, lv_pct(100));
    lv_label_set_long_mode(m_lblFirmwareSource, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(m_lblFirmwareSource, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblFirmwareSource, lv_color_hex(0x38BDF8), 0);

    // ─────────────────────────────────────────────────────────────
    // 5. Tarjeta de Control y Progreso
    // ─────────────────────────────────────────────────────────────
    lv_obj_t* cardControl = lv_obj_create(m_container);
    lv_obj_set_width(cardControl, lv_pct(100));
    lv_obj_set_height(cardControl, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(cardControl, 14);
    lv_obj_set_flex_flow(cardControl, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cardControl, 14, 0);
    lv_obj_set_style_pad_row(cardControl, 10, 0);

    m_lblStatus = lv_label_create(cardControl);
    lv_label_set_text(m_lblStatus, "Listo para iniciar flasheo.");
    lv_obj_set_style_text_font(m_lblStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblStatus, DefaultTheme::getTextColor(), 0);

    // Barra de Progreso
    m_barProgress = lv_bar_create(cardControl);
    lv_obj_set_width(m_barProgress, lv_pct(100));
    lv_obj_set_height(m_barProgress, 18);
    lv_bar_set_range(m_barProgress, 0, 100);
    lv_bar_set_value(m_barProgress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_barProgress, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_barProgress, DefaultTheme::getPrimaryAccent(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(m_barProgress, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(m_barProgress, 8, LV_PART_INDICATOR);

    // Log de texto
    m_lblLog = lv_label_create(cardControl);
    lv_label_set_text(m_lblLog, "Presiona el botón para iniciar la transferencia.");
    lv_obj_set_style_text_font(m_lblLog, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblLog, DefaultTheme::getMutedTextColor(), 0);

    // Botón Iniciar Flasheo
    m_btnStart = lv_button_create(cardControl);
    lv_obj_set_width(m_btnStart, lv_pct(100));
    lv_obj_set_height(m_btnStart, 48);
    DefaultTheme::applyButton(m_btnStart, 12);
    lv_obj_set_style_bg_color(m_btnStart, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(m_btnStart, startFlashCb, LV_EVENT_CLICKED, this);

    m_lblBtn = lv_label_create(m_btnStart);
    lv_label_set_text(m_lblBtn, LV_SYMBOL_DOWNLOAD " Iniciar Flasheo");
    lv_obj_set_style_text_font(m_lblBtn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblBtn, lv_color_hex(0x0F172A), 0);
    lv_obj_center(m_lblBtn);

    updateUIFromConfig();
    return true;
}

void FlasherView::onDestroy() {
    s_activeFlasherView = nullptr;
    BaseView::onDestroy();
}

void FlasherView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
}

void FlasherView::updateUIFromConfig() {
    if (m_selectedPresetIndex >= 0 && m_selectedPresetIndex < (int)m_presets.size()) {
        const auto& preset = m_presets[m_selectedPresetIndex];
        if (m_lblPresetDesc) lv_label_set_text(m_lblPresetDesc, preset.description.c_str());
        if (m_lblWiring) lv_label_set_text(m_lblWiring, preset.wiringInfo.c_str());
    }

    // Si el preset usa USB-Serial Nativo, ocultamos o deshabilitamos la tarjeta de pines GPIO
    if (m_cardPinConfig) {
        if (m_currentConfig.transport == cbdos::flasher::FlasherTransport::USB_CDC_NATIVE) {
            lv_obj_add_flag(m_cardPinConfig, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(m_cardPinConfig, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (m_ddTxPin) lv_dropdown_set_selected(m_ddTxPin, findPinIndex(s_txPins, m_currentConfig.txPin));
    if (m_ddRxPin) lv_dropdown_set_selected(m_ddRxPin, findPinIndex(s_rxPins, m_currentConfig.rxPin));
    if (m_ddBootPin) lv_dropdown_set_selected(m_ddBootPin, findPinIndex(s_bootPins, m_currentConfig.bootPin));
    if (m_ddRstPin) lv_dropdown_set_selected(m_ddRstPin, findPinIndex(s_rstPins, m_currentConfig.rstPin));

    if (m_lblFirmwareSource) {
        if (m_currentConfig.binPath.empty()) {
            lv_label_set_text(m_lblFirmwareSource, "FW: (No seleccionado)");
        } else {
            lv_label_set_text(m_lblFirmwareSource, ("FW: " + m_currentConfig.binPath).c_str());
        }
    }
}

void FlasherView::presetChangedCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (!view || !view->m_ddPresets) return;

    view->m_selectedPresetIndex = lv_dropdown_get_selected(view->m_ddPresets);
    if (view->m_selectedPresetIndex >= 0 && view->m_selectedPresetIndex < (int)view->m_presets.size()) {
        view->m_currentConfig = view->m_presets[view->m_selectedPresetIndex].config;
        view->updateUIFromConfig();
    }
}

void FlasherView::baudChangedCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (!view || !view->m_ddBaud) return;

    uint32_t sel = lv_dropdown_get_selected(view->m_ddBaud);
    switch (sel) {
        case 0: view->m_currentConfig.baudRate = 115200; break;
        case 1: view->m_currentConfig.baudRate = 230400; break;
        case 2: view->m_currentConfig.baudRate = 460800; break;
        case 3: view->m_currentConfig.baudRate = 921600; break;
        default: view->m_currentConfig.baudRate = 115200; break;
    }
}

void FlasherView::pinDropdownChangedCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (!view) return;

    int whichPin = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dd);

    switch (whichPin) {
        case 1: // TX
            if (sel < s_txPins.size()) view->m_currentConfig.txPin = s_txPins[sel].gpio;
            break;
        case 2: // RX
            if (sel < s_rxPins.size()) view->m_currentConfig.rxPin = s_rxPins[sel].gpio;
            break;
        case 3: // BOOT
            if (sel < s_bootPins.size()) view->m_currentConfig.bootPin = s_bootPins[sel].gpio;
            break;
        case 4: // RST
            if (sel < s_rstPins.size()) view->m_currentConfig.rstPin = s_rstPins[sel].gpio;
            break;
    }
}

void FlasherView::startFlashCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (!view || cbdos::flasher::isBusy()) return;

    if (view->m_btnStart) {
        lv_obj_add_state(view->m_btnStart, LV_STATE_DISABLED);
    }
    if (view->m_lblBtn) {
        lv_label_set_text(view->m_lblBtn, "Flasheando...");
    }
    if (view->m_barProgress) {
        lv_bar_set_value(view->m_barProgress, 0, LV_ANIM_OFF);
    }
    if (view->m_lblStatus) {
        lv_label_set_text(view->m_lblStatus, "Iniciando proceso...");
        lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0x60A5FA), 0);
    }

    cbdos::flasher::startFlash(view->m_currentConfig, [view](cbdos::flasher::FlasherStatus status, int percent, const char* message) {
        if (!s_activeFlasherView || s_activeFlasherView != view) return;

        if (cbdos::display::lock(200)) {
            if (view->m_barProgress && lv_obj_is_valid(view->m_barProgress)) {
                lv_bar_set_value(view->m_barProgress, percent, LV_ANIM_OFF);
            }
            if (view->m_lblLog && lv_obj_is_valid(view->m_lblLog) && message) {
                lv_label_set_text(view->m_lblLog, message);
            }
            if (view->m_lblStatus && lv_obj_is_valid(view->m_lblStatus)) {
                if (status == cbdos::flasher::FlasherStatus::Failed) {
                    lv_label_set_text(view->m_lblStatus, "Fallo durante el flasheo");
                    lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0xEF4444), 0);
                } else if (status == cbdos::flasher::FlasherStatus::Success) {
                    lv_label_set_text(view->m_lblStatus, "¡Programación completada exitosamente!");
                    lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0x10B981), 0);
                } else {
                    lv_label_set_text(view->m_lblStatus, "Programando memoria Flash...");
                    lv_obj_set_style_text_color(view->m_lblStatus, lv_color_hex(0x60A5FA), 0);
                }
            }

            if (status == cbdos::flasher::FlasherStatus::Success || status == cbdos::flasher::FlasherStatus::Failed) {
                if (view->m_btnStart && lv_obj_is_valid(view->m_btnStart)) {
                    lv_obj_remove_state(view->m_btnStart, LV_STATE_DISABLED);
                }
                if (view->m_lblBtn && lv_obj_is_valid(view->m_lblBtn)) {
                    lv_label_set_text(view->m_lblBtn, (status == cbdos::flasher::FlasherStatus::Success) ? LV_SYMBOL_REFRESH " Flashear Nuevamente" : LV_SYMBOL_REFRESH " Reintentar");
                }
            }
            cbdos::display::unlock();
        }
    });
}

void FlasherView::openFilePickerModal(const std::string& startPath) {
    if (m_filePickerMask) closeFilePickerModal();

    m_pickerPath = startPath.empty() ? "/sdcard" : startPath;

    // Fondo modal oscurecido
    m_filePickerMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_filePickerMask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(m_filePickerMask, 0, 0);
    lv_obj_set_style_bg_color(m_filePickerMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_filePickerMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_filePickerMask, 0, 0);
    lv_obj_set_style_pad_all(m_filePickerMask, 12, 0);
    lv_obj_set_flex_flow(m_filePickerMask, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_filePickerMask, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Tarjeta modal
    lv_obj_t* card = lv_obj_create(m_filePickerMask);
    lv_obj_set_width(card, LV_PCT(95));
    lv_obj_set_height(card, LV_PCT(85));
    DefaultTheme::applyRaisedCard(card, 14);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 8, 0);

    // Cabecera: Título y Botón Cerrar
    lv_obj_t* header = lv_obj_create(card);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);
    DefaultTheme::disableScroll(header);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_FILE " Seleccionar Firmware");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTitle, DefaultTheme::getPrimaryAccent(), 0);

    lv_obj_t* btnClose = lv_button_create(header);
    lv_obj_set_size(btnClose, 30, 30);
    DefaultTheme::applyButton(btnClose, 6);
    lv_obj_set_style_bg_color(btnClose, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btnClose, pickerCloseBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblClose = lv_label_create(btnClose);
    lv_label_set_text(lblClose, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lblClose, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblClose);

    // Barra de ruta y botón subir directorio
    lv_obj_t* pathBar = lv_obj_create(card);
    lv_obj_set_width(pathBar, LV_PCT(100));
    lv_obj_set_height(pathBar, 34);
    DefaultTheme::applySunkenCard(pathBar, 6);
    DefaultTheme::disableScroll(pathBar);
    lv_obj_set_flex_flow(pathBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pathBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(pathBar, 2, 0);
    lv_obj_set_style_pad_column(pathBar, 6, 0);

    lv_obj_t* btnUp = lv_button_create(pathBar);
    lv_obj_set_size(btnUp, 28, 28);
    DefaultTheme::applyButton(btnUp, 4);
    lv_obj_add_event_cb(btnUp, pickerUpBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblUp = lv_label_create(btnUp);
    lv_label_set_text(lblUp, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lblUp, &lv_font_montserrat_12, 0);
    lv_obj_center(lblUp);

    m_pickerPathLabel = lv_label_create(pathBar);
    lv_obj_set_flex_grow(m_pickerPathLabel, 1);
    lv_label_set_long_mode(m_pickerPathLabel, LV_LABEL_LONG_DOT);
    lv_label_set_text(m_pickerPathLabel, m_pickerPath.c_str());
    lv_obj_set_style_text_color(m_pickerPathLabel, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(m_pickerPathLabel, &lv_font_montserrat_12, 0);

    // Contenedor scrollable de lista de archivos
    m_pickerListCont = lv_obj_create(card);
    lv_obj_set_width(m_pickerListCont, LV_PCT(100));
    lv_obj_set_flex_grow(m_pickerListCont, 1);
    DefaultTheme::applySunkenCard(m_pickerListCont, 8);
    lv_obj_set_flex_flow(m_pickerListCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_pickerListCont, 6, 0);
    lv_obj_set_style_pad_row(m_pickerListCont, 6, 0);
    lv_obj_set_scrollbar_mode(m_pickerListCont, LV_SCROLLBAR_MODE_AUTO);

    renderPickerFileList();
}

void FlasherView::closeFilePickerModal() {
    if (m_filePickerMask && lv_obj_is_valid(m_filePickerMask)) {
        lv_obj_delete_async(m_filePickerMask);
        m_filePickerMask = nullptr;
        m_pickerListCont = nullptr;
        m_pickerPathLabel = nullptr;
    }
}

void FlasherView::renderPickerFileList() {
    if (!m_pickerListCont || !lv_obj_is_valid(m_pickerListCont)) return;

    lv_obj_clean(m_pickerListCont);

    if (m_pickerPathLabel && lv_obj_is_valid(m_pickerPathLabel)) {
        lv_label_set_text(m_pickerPathLabel, m_pickerPath.c_str());
    }

    std::vector<cbdos::storage::FileEntry> files = cbdos::storage::listDir(m_pickerPath.c_str());

    // Ordenar: primero carpetas, luego archivos
    std::sort(files.begin(), files.end(), [](const cbdos::storage::FileEntry& a, const cbdos::storage::FileEntry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return a.name < b.name;
    });

    if (files.empty()) {
        lv_obj_t* lblEmpty = lv_label_create(m_pickerListCont);
        lv_label_set_text(lblEmpty, "(No hay archivos o tarjeta inaccesible)");
        lv_obj_set_style_text_color(lblEmpty, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblEmpty, &lv_font_montserrat_12, 0);
        lv_obj_center(lblEmpty);
        return;
    }

    for (const auto& item : files) {
        bool isBin = false;
        if (!item.isDirectory) {
            std::string n = item.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.size() >= 4 && (n.rfind(".bin") == n.size() - 4 || n.rfind(".hex") == n.size() - 4)) {
                isBin = true;
            }
        }

        lv_obj_t* btnItem = lv_button_create(m_pickerListCont);
        lv_obj_set_width(btnItem, LV_PCT(100));
        lv_obj_set_height(btnItem, 40);
        DefaultTheme::applyButton(btnItem, 6);

        if (item.isDirectory) {
            lv_obj_set_style_bg_color(btnItem, lv_color_hex(0x1E293B), 0);
        } else if (isBin) {
            lv_obj_set_style_bg_color(btnItem, lv_color_hex(0x064E3B), 0); // Verde sutil para binarios
        } else {
            lv_obj_set_style_bg_color(btnItem, lv_color_hex(0x0F172A), 0);
        }

        lv_obj_set_flex_flow(btnItem, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btnItem, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(btnItem, 6, 0);
        lv_obj_set_style_pad_column(btnItem, 8, 0);

        // Icono
        lv_obj_t* lblIcon = lv_label_create(btnItem);
        if (item.isDirectory) {
            lv_label_set_text(lblIcon, LV_SYMBOL_DIRECTORY);
            lv_obj_set_style_text_color(lblIcon, lv_color_hex(0xF59E0B), 0);
        } else if (isBin) {
            lv_label_set_text(lblIcon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(lblIcon, lv_color_hex(0x10B981), 0);
        } else {
            lv_label_set_text(lblIcon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(lblIcon, DefaultTheme::getMutedTextColor(), 0);
        }
        lv_obj_set_style_text_font(lblIcon, &lv_font_montserrat_14, 0);

        // Nombre y tamaño
        lv_obj_t* lblName = lv_label_create(btnItem);
        lv_obj_set_flex_grow(lblName, 1);
        lv_label_set_long_mode(lblName, LV_LABEL_LONG_DOT);
        lv_label_set_text(lblName, item.name.c_str());
        lv_obj_set_style_text_color(lblName, lv_color_hex(0xF8FAFC), 0);
        lv_obj_set_style_text_font(lblName, &lv_font_montserrat_12, 0);

        if (!item.isDirectory) {
            lv_obj_t* lblSize = lv_label_create(btnItem);
            char szBuf[32];
            if (item.size >= 1024 * 1024) {
                snprintf(szBuf, sizeof(szBuf), "%.1f MB", (double)item.size / (1024.0 * 1024.0));
            } else {
                snprintf(szBuf, sizeof(szBuf), "%u KB", (unsigned int)(item.size / 1024));
            }
            lv_label_set_text(lblSize, szBuf);
            lv_obj_set_style_text_color(lblSize, DefaultTheme::getMutedTextColor(), 0);
            lv_obj_set_style_text_font(lblSize, &lv_font_montserrat_12, 0);
        }

        // Contexto para callback
        struct PickerCtx {
            FlasherView* view;
            cbdos::storage::FileEntry entry;
        };
        auto* ctx = new PickerCtx{this, item};
        lv_obj_set_user_data(btnItem, ctx);
        lv_obj_add_event_cb(btnItem, pickerItemClickCb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(btnItem, [](lv_event_t* e) {
            auto* p = (PickerCtx*)lv_event_get_user_data(e);
            delete p;
        }, LV_EVENT_DELETE, ctx);
    }
}

void FlasherView::pickFileBtnCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (!view) return;
    std::string start = view->m_currentConfig.binPath;
    if (start.empty()) start = "/sdcard";
    // Extraer carpeta padre si es archivo
    size_t lastSlash = start.rfind('/');
    if (lastSlash != std::string::npos && lastSlash > 0) {
        start = start.substr(0, lastSlash);
    }
    view->openFilePickerModal(start);
}

void FlasherView::pickerCloseBtnCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (view) view->closeFilePickerModal();
}

void FlasherView::pickerUpBtnCb(lv_event_t* e) {
    auto* view = s_activeFlasherView;
    if (!view) return;
    if (view->m_pickerPath == "/sdcard" || view->m_pickerPath == "/" || view->m_pickerPath.empty()) {
        return;
    }
    size_t lastSlash = view->m_pickerPath.rfind('/');
    if (lastSlash != std::string::npos && lastSlash > 0) {
        view->m_pickerPath = view->m_pickerPath.substr(0, lastSlash);
    } else {
        view->m_pickerPath = "/sdcard";
    }
    view->renderPickerFileList();
}

void FlasherView::pickerItemClickCb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    if (!btn) return;
    struct PickerCtx {
        FlasherView* view;
        cbdos::storage::FileEntry entry;
    };
    auto* ctx = (PickerCtx*)lv_obj_get_user_data(btn);
    if (!ctx || !ctx->view) return;

    FlasherView* view = ctx->view;
    if (ctx->entry.isDirectory) {
        if (view->m_pickerPath == "/" || view->m_pickerPath.empty()) {
            view->m_pickerPath = "/" + ctx->entry.name;
        } else {
            view->m_pickerPath = view->m_pickerPath + "/" + ctx->entry.name;
        }
        view->renderPickerFileList();
    } else {
        // Seleccionar archivo
        std::string fullPath = view->m_pickerPath;
        if (fullPath.back() != '/') fullPath += "/";
        fullPath += ctx->entry.name;

        view->m_currentConfig.binPath = fullPath;
        view->updateUIFromConfig();
        view->closeFilePickerModal();
    }
}

} // namespace ui
} // namespace cbdos

#endif // CBDOS_FEATURE_FLASHER


