#include "TimeConfigView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/config_manager.hpp"
#include "cbdos/time.hpp"
#include "cbdos/network.hpp"
#include "cbdos/language.hpp"
#include <cstdio>
#include <cstring>

namespace cbdos {
namespace ui {

static const std::vector<TimeConfigView::TzPreset> s_tzPresets = {
    {"UTC-8:00 (Baja California / Tijuana - PST)", -28800},
    {"UTC-7:00 (Baja Calif. / Sonora / Chihuahua - PDT/MST)", -25200},
    {"UTC-6:00 (Centro de Mexico - CDMX, GDL, MTY)", -21600},
    {"UTC-5:00 (Sureste / Cancun / Bogota / EST)", -18000},
    {"UTC-4:00 (Caribe / La Paz / Santiago - AST)", -14400},
    {"UTC-3:00 (Argentina / Brasilia / Montevideo)", -10800},
    {"UTC+0:00 (GMT / Londres / Lisboa / UTC)", 0},
    {"UTC+1:00 (Espana / Madrid / Paris / Roma - CET)", 3600},
    {"UTC+2:00 (Atenas / El Cairo / Jerusalen - EET)", 7200},
    {"UTC+3:00 (Moscu / Riad / Estambul - MSK)", 10800},
    {"UTC+8:00 (China / Singapur / Hong Kong - CST)", 28800},
    {"UTC+9:00 (Japon / Tokio / Seul - JST)", 32400}
};

static const char* s_ntpServers[] = {
    "pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com",
    "time.nist.gov",
    "time.windows.com"
};

const std::vector<TimeConfigView::TzPreset>& TimeConfigView::getTzPresets() {
    return s_tzPresets;
}

TimeConfigView::TimeConfigView()
    : BaseView(cbdos::lang::tr(cbdos::lang::StrId::STR_CFG_DATETIME)),
      m_timeLabel(nullptr),
      m_dateLabel(nullptr),
      m_statusLabel(nullptr),
      m_tzDropdown(nullptr),
      m_dstSwitch(nullptr),
      m_ntpSwitch(nullptr),
      m_ntpDropdown(nullptr),
      m_clockTimer(nullptr) {
}

void TimeConfigView::onDestroy() {
    if (m_clockTimer) {
        lv_timer_delete(m_clockTimer);
        m_clockTimer = nullptr;
    }
    BaseView::onDestroy();
}

void TimeConfigView::timer_cb(lv_timer_t* timer) {
    TimeConfigView* view = static_cast<TimeConfigView*>(lv_timer_get_user_data(timer));
    if (view) {
        view->updateClockDisplay();
    }
}

void TimeConfigView::updateClockDisplay() {
    if (!m_timeLabel || !lv_obj_is_valid(m_timeLabel)) return;

    char timeBuf[32];
    char dateBuf[32];
    cbdos::time::getFormattedTime(timeBuf, sizeof(timeBuf), "%H:%M:%S");
    cbdos::time::getFormattedDate(dateBuf, sizeof(dateBuf), "%d/%m/%Y");

    lv_label_set_text(m_timeLabel, timeBuf);
    if (m_dateLabel && lv_obj_is_valid(m_dateLabel)) {
        lv_label_set_text(m_dateLabel, dateBuf);
    }

    if (m_statusLabel && lv_obj_is_valid(m_statusLabel)) {
        if (cbdos::time::isSynced()) {
            cbdos::time::TimeSource src = cbdos::time::getSource();
            if (src == cbdos::time::TimeSource::SNTP) {
                lv_label_set_text_fmt(m_statusLabel, "%s %s", LV_SYMBOL_OK, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_ST_WIFI));
            } else if (src == cbdos::time::TimeSource::Tower) {
                lv_label_set_text_fmt(m_statusLabel, "%s %s", LV_SYMBOL_OK, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_ST_TOWER));
            } else {
                lv_label_set_text_fmt(m_statusLabel, "%s %s", LV_SYMBOL_OK, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_ST_LOCAL));
            }
            lv_obj_set_style_text_color(m_statusLabel, lv_color_hex(0x10B981), 0); // Verde
        } else {
            lv_label_set_text_fmt(m_statusLabel, "%s %s", LV_SYMBOL_WARNING, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_ST_UNSYNC));
            lv_obj_set_style_text_color(m_statusLabel, lv_color_hex(0xF59E0B), 0); // Amarillo
        }
    }
}

void TimeConfigView::save_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        TimeConfigView* view = static_cast<TimeConfigView*>(lv_event_get_user_data(e));
        if (view) {
            view->saveAndApply();
        }
    }
}

void TimeConfigView::sync_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        TimeConfigView* view = static_cast<TimeConfigView*>(lv_event_get_user_data(e));
        if (view) {
            view->saveAndApply();
            cbdos::time::syncNtp();
            UIManager::showToast("Forzando ciclo de sincronización...");
            view->updateClockDisplay();
        }
    }
}

void TimeConfigView::saveAndApply() {
    TimeConfig cfg;
    ConfigManager::getInstance().loadTime(cfg);

    // 1. Zona Horaria seleccionada
    if (m_tzDropdown && lv_obj_is_valid(m_tzDropdown)) {
        uint32_t selIdx = lv_dropdown_get_selected(m_tzDropdown);
        if (selIdx < s_tzPresets.size()) {
            cfg.gmtOffsetSeconds = s_tzPresets[selIdx].offsetSec;
        }
    }

    // 2. Horario de verano (DST)
    if (m_dstSwitch && lv_obj_is_valid(m_dstSwitch)) {
        bool dstEnabled = lv_obj_has_state(m_dstSwitch, LV_STATE_CHECKED);
        cfg.daylightOffsetSeconds = dstEnabled ? 3600 : 0;
    }

    // 3. Sincronización Automática NTP
    if (m_ntpSwitch && lv_obj_is_valid(m_ntpSwitch)) {
        cfg.enabled = lv_obj_has_state(m_ntpSwitch, LV_STATE_CHECKED);
    }

    // 4. Servidor NTP
    if (m_ntpDropdown && lv_obj_is_valid(m_ntpDropdown)) {
        uint32_t ntpIdx = lv_dropdown_get_selected(m_ntpDropdown);
        if (ntpIdx < sizeof(s_ntpServers) / sizeof(s_ntpServers[0])) {
            cfg.ntpServer = s_ntpServers[ntpIdx];
        }
    }

    // Guardar en NVS y aplicar
    ConfigManager::getInstance().saveTime(cfg);
    cbdos::time::setAutoSyncEnabled(cfg.enabled);
    cbdos::time::setTimezone(cfg.gmtOffsetSeconds, cfg.daylightOffsetSeconds);
    if (cfg.enabled && cbdos::network::isConnected()) {
        cbdos::time::syncNtp();
    }
    updateClockDisplay();
    UIManager::showToast("Zona horaria y NTP guardados");
}

bool TimeConfigView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor principal scrollable
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_bottom(m_container, 24, 0);
    lv_obj_set_style_pad_row(m_container, 10, 0);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // Cargar configuración actual
    TimeConfig cfg;
    ConfigManager::getInstance().loadTime(cfg);

    // ──────────────────────────────────────────────────────────
    // 1. Tarjeta Reloj en Tiempo Real
    // ──────────────────────────────────────────────────────────
    lv_obj_t* clockCard = lv_obj_create(m_container);
    lv_obj_set_width(clockCard, lv_pct(100));
    DefaultTheme::applyRaisedCard(clockCard, 14);
    lv_obj_set_flex_flow(clockCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(clockCard, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(clockCard, 12, 0);
    lv_obj_set_style_pad_row(clockCard, 4, 0);

    m_timeLabel = lv_label_create(clockCard);
    lv_obj_set_style_text_font(m_timeLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(m_timeLabel, DefaultTheme::getPrimaryAccent(), 0);

    m_dateLabel = lv_label_create(clockCard);
    lv_obj_set_style_text_font(m_dateLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_dateLabel, DefaultTheme::getTextColor(), 0);

    m_statusLabel = lv_label_create(clockCard);
    lv_obj_set_style_text_font(m_statusLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_top(m_statusLabel, 4, 0);

    updateClockDisplay();

    // ──────────────────────────────────────────────────────────
    // 2. Selector de Zona Horaria
    // ──────────────────────────────────────────────────────────
    lv_obj_t* tzLabel = lv_label_create(m_container);
    lv_label_set_text(tzLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_TZ));
    lv_obj_set_style_text_color(tzLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(tzLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_top(tzLabel, 4, 0);

    m_tzDropdown = lv_dropdown_create(m_container);
    lv_obj_set_width(m_tzDropdown, lv_pct(100));
    DefaultTheme::applyRaisedCard(m_tzDropdown, 12);
    lv_obj_set_style_pad_ver(m_tzDropdown, 10, 0);
    lv_obj_set_style_text_font(m_tzDropdown, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_tzDropdown, DefaultTheme::getTextColor(), 0);

    std::string tzOptionsStr;
    uint32_t selectedTzIdx = 2; // Por defecto UTC-6
    for (size_t i = 0; i < s_tzPresets.size(); i++) {
        if (i > 0) tzOptionsStr += "\n";
        tzOptionsStr += s_tzPresets[i].name;
        if (s_tzPresets[i].offsetSec == cfg.gmtOffsetSeconds) {
            selectedTzIdx = i;
        }
    }
    lv_dropdown_set_options(m_tzDropdown, tzOptionsStr.c_str());
    lv_dropdown_set_selected(m_tzDropdown, selectedTzIdx);

    // ──────────────────────────────────────────────────────────
    // 3. Tarjeta de Horario de Verano (DST)
    // ──────────────────────────────────────────────────────────
    lv_obj_t* dstCard = lv_obj_create(m_container);
    lv_obj_set_width(dstCard, lv_pct(100));
    lv_obj_set_height(dstCard, 52);
    DefaultTheme::applyRaisedCard(dstCard, 12);
    lv_obj_set_flex_flow(dstCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dstCard, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(dstCard, 14, 0);
    lv_obj_set_style_pad_ver(dstCard, 6, 0);

    lv_obj_t* dstTextCont = lv_obj_create(dstCard);
    lv_obj_set_flex_grow(dstTextCont, 1);
    lv_obj_set_style_bg_opa(dstTextCont, 0, 0);
    lv_obj_set_style_border_width(dstTextCont, 0, 0);
    lv_obj_set_flex_flow(dstTextCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dstTextCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(dstTextCont, 0, 0);
    lv_obj_remove_flag(dstTextCont, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* dstTitle = lv_label_create(dstTextCont);
    lv_label_set_text(dstTitle, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_DST));
    lv_obj_set_style_text_color(dstTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(dstTitle, &lv_font_montserrat_14, 0);

    lv_obj_t* dstSub = lv_label_create(dstTextCont);
    lv_label_set_text(dstSub, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_DST_SUB));
    lv_obj_set_style_text_color(dstSub, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(dstSub, &lv_font_montserrat_12, 0);

    m_dstSwitch = lv_switch_create(dstCard);
    if (cfg.daylightOffsetSeconds > 0) {
        lv_obj_add_state(m_dstSwitch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(m_dstSwitch, LV_STATE_CHECKED);
    }

    // ──────────────────────────────────────────────────────────
    // 4. Tarjeta de Sincronización Automática NTP
    // ──────────────────────────────────────────────────────────
    lv_obj_t* ntpCard = lv_obj_create(m_container);
    lv_obj_set_width(ntpCard, lv_pct(100));
    lv_obj_set_height(ntpCard, 52);
    DefaultTheme::applyRaisedCard(ntpCard, 12);
    lv_obj_set_flex_flow(ntpCard, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ntpCard, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(ntpCard, 14, 0);
    lv_obj_set_style_pad_ver(ntpCard, 6, 0);

    lv_obj_t* ntpTextCont = lv_obj_create(ntpCard);
    lv_obj_set_flex_grow(ntpTextCont, 1);
    lv_obj_set_style_bg_opa(ntpTextCont, 0, 0);
    lv_obj_set_style_border_width(ntpTextCont, 0, 0);
    lv_obj_set_flex_flow(ntpTextCont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ntpTextCont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(ntpTextCont, 0, 0);
    lv_obj_remove_flag(ntpTextCont, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* ntpTitle = lv_label_create(ntpTextCont);
    lv_label_set_text(ntpTitle, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_NTP));
    lv_obj_set_style_text_color(ntpTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(ntpTitle, &lv_font_montserrat_14, 0);

    lv_obj_t* ntpSub = lv_label_create(ntpTextCont);
    lv_label_set_text(ntpSub, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_NTP_SUB));
    lv_obj_set_style_text_color(ntpSub, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(ntpSub, &lv_font_montserrat_12, 0);

    m_ntpSwitch = lv_switch_create(ntpCard);
    if (cfg.enabled) {
        lv_obj_add_state(m_ntpSwitch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(m_ntpSwitch, LV_STATE_CHECKED);
    }

    // ──────────────────────────────────────────────────────────
    // 5. Selector de Servidor NTP
    // ──────────────────────────────────────────────────────────
    lv_obj_t* ntpLabel = lv_label_create(m_container);
    lv_label_set_text(ntpLabel, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_NTP_SRV));
    lv_obj_set_style_text_color(ntpLabel, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(ntpLabel, &lv_font_montserrat_14, 0);

    m_ntpDropdown = lv_dropdown_create(m_container);
    lv_obj_set_width(m_ntpDropdown, lv_pct(100));
    DefaultTheme::applyRaisedCard(m_ntpDropdown, 12);
    lv_obj_set_style_pad_ver(m_ntpDropdown, 10, 0);
    lv_obj_set_style_text_font(m_ntpDropdown, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_ntpDropdown, DefaultTheme::getTextColor(), 0);

    std::string ntpOptionsStr;
    uint32_t selectedNtpIdx = 0;
    size_t ntpCount = sizeof(s_ntpServers) / sizeof(s_ntpServers[0]);
    for (size_t i = 0; i < ntpCount; i++) {
        if (i > 0) ntpOptionsStr += "\n";
        ntpOptionsStr += s_ntpServers[i];
        if (cfg.ntpServer == s_ntpServers[i]) {
            selectedNtpIdx = i;
        }
    }
    lv_dropdown_set_options(m_ntpDropdown, ntpOptionsStr.c_str());
    lv_dropdown_set_selected(m_ntpDropdown, selectedNtpIdx);

    // ──────────────────────────────────────────────────────────
    // 6. Botones de Acción
    // ──────────────────────────────────────────────────────────
    lv_obj_t* saveBtn = lv_button_create(m_container);
    lv_obj_set_width(saveBtn, lv_pct(100));
    lv_obj_set_height(saveBtn, 48);
    DefaultTheme::applyButton(saveBtn, 14);
    lv_obj_set_style_bg_color(saveBtn, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_add_event_cb(saveBtn, save_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text_fmt(saveLbl, "%s %s", LV_SYMBOL_SAVE, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_SAVE));
    lv_obj_set_style_text_color(saveLbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(saveLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(saveLbl);

    lv_obj_t* syncBtn = lv_button_create(m_container);
    lv_obj_set_width(syncBtn, lv_pct(100));
    lv_obj_set_height(syncBtn, 48);
    DefaultTheme::applyButton(syncBtn, 14);
    lv_obj_add_event_cb(syncBtn, sync_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* syncLbl = lv_label_create(syncBtn);
    lv_label_set_text_fmt(syncLbl, "%s %s", LV_SYMBOL_REFRESH, cbdos::lang::tr(cbdos::lang::StrId::STR_TIME_SYNC_NOW));
    lv_obj_set_style_text_color(syncLbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(syncLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(syncLbl);

    // Timer periódico de 1 segundo para actualizar la hora en pantalla
    m_clockTimer = lv_timer_create(timer_cb, 1000, this);

    return true;
}

} // namespace ui
} // namespace cbdos
