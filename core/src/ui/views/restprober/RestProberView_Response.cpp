#include "../RestProberView.hpp"
#include "RestProberCommon.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"

#include <cstdio>

namespace cbdos {
namespace ui {

void RestProberView::setupResponseTab(lv_obj_t* tab) {
    using namespace restprober;

    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 8, 0);
    lv_obj_set_style_pad_gap(tab, 6, 0);

    // Barra de telemetría de respuesta
    lv_obj_t* bar = lv_obj_create(tab);
    lv_obj_set_size(bar, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(bar, 6, 0);
    lv_obj_set_style_pad_gap(bar, 10, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(kColBgCard), 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 8, 0);
    DefaultTheme::disableScroll(bar);

    m_badgeStatus = lv_obj_create(bar);
    lv_obj_set_size(m_badgeStatus, LV_SIZE_CONTENT, 32);
    lv_obj_set_style_radius(m_badgeStatus, 6, 0);
    lv_obj_set_style_bg_color(m_badgeStatus, lv_color_hex(0x272B3C), 0);
    lv_obj_set_style_border_color(m_badgeStatus, lv_color_hex(kColGray), 0);
    lv_obj_set_style_border_width(m_badgeStatus, 1, 0);
    lv_obj_set_style_pad_hor(m_badgeStatus, 8, 0);
    lv_obj_set_style_pad_ver(m_badgeStatus, 4, 0);
    DefaultTheme::disableScroll(m_badgeStatus);

    m_lblStatusCode = lv_label_create(m_badgeStatus);
    lv_label_set_text(m_lblStatusCode, "STATUS: ---");
    lv_obj_set_style_text_color(m_lblStatusCode, lv_color_hex(kColGray), 0);
    lv_obj_center(m_lblStatusCode);

    m_lblRtt = lv_label_create(bar);
    lv_label_set_text(m_lblRtt, "RTT: -- ms");
    lv_obj_set_style_text_color(m_lblRtt, lv_color_hex(kColTextMuted), 0);
    lv_obj_set_style_pad_top(m_lblRtt, 6, 0);

    m_lblSize = lv_label_create(bar);
    lv_label_set_text(m_lblSize, "Size: -- B");
    lv_obj_set_style_text_color(m_lblSize, lv_color_hex(kColTextMuted), 0);
    lv_obj_set_style_pad_top(m_lblSize, 6, 0);

    // Contenedor del cuerpo devuelto
    m_taResponseBody = lv_textarea_create(tab);
    lv_obj_set_size(m_taResponseBody, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_taResponseBody, 1);
    lv_textarea_set_placeholder_text(m_taResponseBody, "Presiona 'SEND' para ejecutar la peticion HTTP...");
    lv_obj_set_style_radius(m_taResponseBody, 8, 0);
    lv_obj_set_style_bg_color(m_taResponseBody, lv_color_hex(0x181B26), 0);
    lv_obj_set_style_border_color(m_taResponseBody, lv_color_hex(kColBorder), 0);
    lv_obj_set_style_text_color(m_taResponseBody, lv_color_hex(0xF1F5F9), 0);
    // Modo solo lectura
    lv_obj_clear_flag(m_taResponseBody, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

void RestProberView::renderResponse(const http::HttpResponse& response) {
    using namespace restprober;

    if (m_lblSend) lv_label_set_text(m_lblSend, "SEND 🚀");
    if (m_btnSend) lv_obj_set_style_bg_color(m_btnSend, lv_color_hex(0x00F5D4), 0);

    char buf[64];
    // 1. Status Code
    if (m_lblStatusCode && lv_obj_is_valid(m_lblStatusCode)) {
        if (response.statusCode > 0) {
            snprintf(buf, sizeof(buf), "STATUS: %d", response.statusCode);
            lv_label_set_text(m_lblStatusCode, buf);
            uint32_t col = (response.statusCode >= 200 && response.statusCode < 300) ? kColGreen : kColRed;
            lv_obj_set_style_text_color(m_lblStatusCode, lv_color_hex(col), 0);
            if (m_badgeStatus) {
                lv_obj_set_style_border_color(m_badgeStatus, lv_color_hex(col), 0);
            }
        } else {
            lv_label_set_text(m_lblStatusCode, "STATUS: ERR");
            lv_obj_set_style_text_color(m_lblStatusCode, lv_color_hex(kColRed), 0);
            if (m_badgeStatus) {
                lv_obj_set_style_border_color(m_badgeStatus, lv_color_hex(kColRed), 0);
            }
        }
    }

    // 2. Latencia RTT
    if (m_lblRtt && lv_obj_is_valid(m_lblRtt)) {
        snprintf(buf, sizeof(buf), "RTT: %u ms", static_cast<unsigned>(response.rttMs));
        lv_label_set_text(m_lblRtt, buf);
    }

    // 3. Tamaño en bytes
    if (m_lblSize && lv_obj_is_valid(m_lblSize)) {
        snprintf(buf, sizeof(buf), "Size: %u B%s",
                 static_cast<unsigned>(response.body.size()),
                 response.truncated ? " (Trunc)" : "");
        lv_label_set_text(m_lblSize, buf);
    }

    // 4. Cuerpo de la respuesta
    if (m_taResponseBody && lv_obj_is_valid(m_taResponseBody)) {
        if (!response.body.empty()) {
            lv_textarea_set_text(m_taResponseBody, response.body.c_str());
        } else if (!response.errorMessage.empty()) {
            std::string errText = "Error: " + response.errorMessage;
            lv_textarea_set_text(m_taResponseBody, errText.c_str());
        } else {
            lv_textarea_set_text(m_taResponseBody, "(Cuerpo de respuesta vacio)");
        }
    }
}

} // namespace ui
} // namespace cbdos
