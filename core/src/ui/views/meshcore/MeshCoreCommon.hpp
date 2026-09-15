#pragma once

#include <lvgl.h>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <map>
#include <vector>
#include "cbdos/meshcore/meshcore_client.hpp"
#include "../themes/DefaultTheme.h"

namespace cbdos {
namespace ui {
namespace meshcore_ui {

inline const char* errName(meshcore::ErrCode e) {
    switch (e) {
        case meshcore::ErrCode::UNSUPPORTED_CMD: return "Comando no soportado";
        case meshcore::ErrCode::NOT_FOUND:       return "No encontrado";
        case meshcore::ErrCode::TABLE_FULL:      return "Tabla llena";
        case meshcore::ErrCode::BAD_STATE:       return "Estado invalido";
        case meshcore::ErrCode::FILE_IO_ERROR:   return "Error de E/S";
        case meshcore::ErrCode::ILLEGAL_ARG:     return "Argumento invalido";
    }
    return "Error desconocido";
}

inline std::string fmtTime(uint32_t epoch) {
    if (epoch == 0) return "--:--";
    time_t t = static_cast<time_t>(epoch);
    struct tm* tmv = localtime(&t);
    if (!tmv) return "--:--";
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv->tm_hour, tmv->tm_min);
    return buf;
}

inline std::string fmtAgo(uint32_t epoch) {
    if (epoch == 0) return "nunca";
    time_t now = time(nullptr);
    if (now < (time_t)epoch) return "ahora";
    uint32_t dt = (uint32_t)(now - (time_t)epoch);
    char buf[32];
    if (dt < 90) snprintf(buf, sizeof(buf), "hace %us", (unsigned)dt);
    else if (dt < 5400) snprintf(buf, sizeof(buf), "hace %u min", (unsigned)(dt / 60));
    else if (dt < 172800) snprintf(buf, sizeof(buf), "hace %u h", (unsigned)(dt / 3600));
    else snprintf(buf, sizeof(buf), "hace %u d", (unsigned)(dt / 86400));
    return buf;
}

inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double kR = 6371.0;
    const double kD = 3.141592653589793 / 180.0;
    double dLa = (lat2 - lat1) * kD;
    double dLo = (lon2 - lon1) * kD;
    double a = sin(dLa / 2) * sin(dLa / 2) +
               cos(lat1 * kD) * cos(lat2 * kD) * sin(dLo / 2) * sin(dLo / 2);
    return 2 * kR * asin(sqrt(a > 1.0 ? 1.0 : a));
}

inline const char* contactIcon(uint8_t type) {
    switch (static_cast<meshcore::ContactType>(type)) {
        case meshcore::ContactType::Repeater: return LV_SYMBOL_WIFI;
        case meshcore::ContactType::Room:     return LV_SYMBOL_HOME;
        case meshcore::ContactType::Sensor:   return LV_SYMBOL_CHARGE;
        default:                              return LV_SYMBOL_CALL;
    }
}

inline std::string trimNum(double v, int decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    std::string s = buf;
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

inline bool parseDec(const char* txt, double& out) {
    if (!txt || !*txt) return false;
    char* end = nullptr;
    out = strtod(txt, &end);
    return end != txt && *end == '\0';
}

inline bool containsFold(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = hay, n = needle;
    for (auto& c : h) c = (char)tolower((unsigned char)c);
    for (auto& c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

inline lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int w, int h, uint32_t bg) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    DefaultTheme::applyButton(btn, 6);
    if (bg) lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
    return btn;
}

} // namespace meshcore_ui
} // namespace ui
} // namespace cbdos
