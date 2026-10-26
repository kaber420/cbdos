#include "OmniwispCpeDetailView.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

void OmniwispCpeDetailView::render(lv_obj_t* parent, const cbdos::network::AirOSStation& cpe, OnBackCb onBack) {
    lv_obj_clean(parent);

    // Barra superior con botón volver
    lv_obj_t* topBar = lv_obj_create(parent);
    lv_obj_set_size(topBar, lv_pct(100), 44);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(topBar, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(topBar, 1, 0);
    lv_obj_set_style_radius(topBar, 6, 0);
    lv_obj_set_style_pad_all(topBar, 4, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnBack = lv_button_create(topBar);
    lv_obj_set_style_bg_color(btnBack, lv_color_hex(0x1E293B), 0);
    lv_obj_t* lblB = lv_label_create(btnBack);
    lv_label_set_text(lblB, LV_SYMBOL_LEFT " Volver al AP");
    lv_obj_set_style_text_color(lblB, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    struct CbHolder { OnBackCb cb; };
    auto* holder = new CbHolder{onBack};
    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* h = static_cast<CbHolder*>(lv_event_get_user_data(e));
        if (h && h->cb) h->cb();
        delete h;
    }, LV_EVENT_CLICKED, holder);

    lv_obj_t* lblHost = lv_label_create(topBar);
    std::string title = " " + (cpe.name.empty() ? cpe.mac : cpe.name) + " (" + cpe.ip + ")";
    lv_label_set_text(lblHost, title.c_str());
    lv_obj_set_style_text_color(lblHost, lv_color_white(), 0);

    // Contenedor scrollable de diagnóstico
    lv_obj_t* scrollBox = lv_obj_create(parent);
    lv_obj_set_size(scrollBox, lv_pct(100), lv_pct(88));
    lv_obj_set_style_bg_opa(scrollBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scrollBox, 0, 0);
    lv_obj_set_style_pad_all(scrollBox, 4, 0);
    lv_obj_set_flex_flow(scrollBox, LV_FLEX_FLOW_COLUMN);

    // 1. TARJETA ALINEACIÓN MIMO (Cadenas 0 y 1)
    lv_obj_t* cardMimo = lv_obj_create(scrollBox);
    lv_obj_set_size(cardMimo, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(cardMimo, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(cardMimo, (cpe.chainDelta > 4) ? lv_color_hex(omni_colors::ALERT_RED) : lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(cardMimo, (cpe.chainDelta > 4) ? 2 : 1, 0);
    lv_obj_set_style_radius(cardMimo, 8, 0);
    lv_obj_set_style_pad_all(cardMimo, 8, 0);
    lv_obj_set_flex_flow(cardMimo, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lMimoTitle = lv_label_create(cardMimo);
    char mimoTitleBuf[128];
    snprintf(mimoTitleBuf, sizeof(mimoTitleBuf), LV_SYMBOL_SETTINGS " Alineacion MIMO | Senal Total: %d dBm (Delta: %d dB)",
             cpe.signalDbm, cpe.chainDelta);
    lv_label_set_text(lMimoTitle, mimoTitleBuf);
    lv_obj_set_style_text_color(lMimoTitle, (cpe.chainDelta > 4) ? lv_color_hex(omni_colors::ALERT_RED) : lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    if (cpe.chainDelta > 4) {
        lv_obj_t* lAlert = lv_label_create(cardMimo);
        lv_label_set_text(lAlert, LV_SYMBOL_WARNING " ALERTA: Antena desalineada! Desbalance de cadenas > 4 dB");
        lv_obj_set_style_text_color(lAlert, lv_color_hex(omni_colors::ALERT_RED), 0);
    }

    char chBuf[128];
    snprintf(chBuf, sizeof(chBuf), "Cadena 0 (V): %d dBm  |  Cadena 1 (H): %d dBm  |  Ruido: %d dBm",
             cpe.signalChain0, cpe.signalChain1, cpe.noiseFloor);
    lv_obj_t* lCh = lv_label_create(cardMimo);
    lv_label_set_text(lCh, chBuf);
    lv_obj_set_style_text_color(lCh, lv_color_hex(omni_colors::TEXT_MUTED), 0);

    // 2. TARJETA ESTADO DE CABLE LAN PoE (Diagnóstico Físico)
    lv_obj_t* cardCable = lv_obj_create(scrollBox);
    lv_obj_set_size(cardCable, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(cardCable, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(cardCable, cpe.cableFault ? lv_color_hex(omni_colors::ALERT_RED) : lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(cardCable, cpe.cableFault ? 2 : 1, 0);
    lv_obj_set_style_radius(cardCable, 8, 0);
    lv_obj_set_style_pad_all(cardCable, 8, 0);
    lv_obj_set_flex_flow(cardCable, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lCableTitle = lv_label_create(cardCable);
    lv_label_set_text(lCableTitle, LV_SYMBOL_USB " Puerto LAN del Cliente (Cable PoE)");
    lv_obj_set_style_text_color(lCableTitle, cpe.cableFault ? lv_color_hex(omni_colors::ALERT_RED) : lv_color_hex(omni_colors::HEALTH_GREEN), 0);

    char cableStatusBuf[160];
    if (!cpe.ethPlugged) {
        snprintf(cableStatusBuf, sizeof(cableStatusBuf), LV_SYMBOL_CLOSE " CABLE DESCONECTADO (Link Down / Sin Alimentacion LAN)");
    } else if (cpe.ethSpeedMbps <= 10) {
        snprintf(cableStatusBuf, sizeof(cableStatusBuf), LV_SYMBOL_WARNING " NEGOCIACION 10 Mbps! (Falla fisica en conectores RJ45 o cable de red)");
    } else {
        snprintf(cableStatusBuf, sizeof(cableStatusBuf), LV_SYMBOL_OK " Enlace LAN: %d Mbps Full-Duplex (Conector y cable saludables)", cpe.ethSpeedMbps);
    }
    lv_obj_t* lCableStatus = lv_label_create(cardCable);
    lv_label_set_text(lCableStatus, cableStatusBuf);
    lv_obj_set_style_text_color(lCableStatus, cpe.cableFault ? lv_color_hex(omni_colors::ALERT_RED) : lv_color_hex(0xE2E8F0), 0);

    // 3. TARJETA CAPACIDAD AIRMAX Y TRÁFICO
    lv_obj_t* cardStats = lv_obj_create(scrollBox);
    lv_obj_set_size(cardStats, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(cardStats, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(cardStats, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(cardStats, 1, 0);
    lv_obj_set_style_radius(cardStats, 8, 0);
    lv_obj_set_style_pad_all(cardStats, 8, 0);
    lv_obj_set_flex_flow(cardStats, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lStatsTitle = lv_label_create(cardStats);
    lv_label_set_text(lStatsTitle, LV_SYMBOL_CHARGE " Capacidad AirMAX y Trafico Actual");
    lv_obj_set_style_text_color(lStatsTitle, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    char capBuf[160];
    snprintf(capBuf, sizeof(capBuf), "Capacidad: DL %d Mbps / UL %d Mbps\nTrafico Instantaneo: Tx %d kbps | Rx %d kbps\nDistancia Estimada: %d metros | CCQ: %d%%",
             cpe.dlCapacityMbps, cpe.ulCapacityMbps,
             cpe.txThroughputKbps, cpe.rxThroughputKbps,
             cpe.distanceM, cpe.ccq);
    lv_obj_t* lCap = lv_label_create(cardStats);
    lv_label_set_text(lCap, capBuf);
    lv_obj_set_style_text_color(lCap, lv_color_hex(0xE2E8F0), 0);
}

} // namespace ui
} // namespace cbdos
