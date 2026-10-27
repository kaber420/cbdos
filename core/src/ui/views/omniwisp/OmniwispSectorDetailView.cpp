#include "OmniwispSectorDetailView.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

void OmniwispSectorDetailView::render(lv_obj_t* parent,
                                    const cbdos::omniwisp::SectorDevice& sector,
                                    const cbdos::network::AirOSTelemetry& telemetry,
                                    bool isFetching,
                                    OnBackCb onBack,
                                    OnBackCb onRefresh,
                                    OnSelectCpeCb onSelectCpe) {
    lv_obj_clean(parent);

    // 1. Barra de Control Superior
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, lv_pct(100), 44);
    lv_obj_set_style_bg_color(bar, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 6, 0);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Volver
    lv_obj_t* btnBack = lv_button_create(bar);
    lv_obj_set_style_bg_color(btnBack, lv_color_hex(0x1E293B), 0);
    lv_obj_t* lblB = lv_label_create(btnBack);
    lv_label_set_text(lblB, LV_SYMBOL_LEFT " Sectores");
    lv_obj_set_style_text_color(lblB, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    struct NavCbHolder { OnBackCb onBack; OnBackCb onRefresh; };
    auto* nav = new NavCbHolder{onBack, onRefresh};
    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* n = static_cast<NavCbHolder*>(lv_event_get_user_data(e));
        if (n && n->onBack) {
            auto cb = n->onBack;
            cb();
        }
    }, LV_EVENT_CLICKED, nav);
    lv_obj_add_event_cb(btnBack, [](lv_event_t* e) {
        auto* n = static_cast<NavCbHolder*>(lv_event_get_user_data(e));
        delete n;
    }, LV_EVENT_DELETE, nav);

    lv_obj_t* lblTitle = lv_label_create(bar);
    std::string t = sector.name + " (" + sector.host + ")";
    lv_label_set_text(lblTitle, t.c_str());
    lv_obj_set_style_text_color(lblTitle, lv_color_white(), 0);

    // Botón Refrescar
    lv_obj_t* btnRefresh = lv_button_create(bar);
    lv_obj_set_style_bg_color(btnRefresh, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_t* lblR = lv_label_create(btnRefresh);
    lv_label_set_text(lblR, isFetching ? LV_SYMBOL_REFRESH " ..." : LV_SYMBOL_REFRESH " Actualizar");
    lv_obj_set_style_text_color(lblR, lv_color_hex(0x06080E), 0);

    auto* navRef = new NavCbHolder{onBack, onRefresh};
    lv_obj_add_event_cb(btnRefresh, [](lv_event_t* e) {
        auto* n = static_cast<NavCbHolder*>(lv_event_get_user_data(e));
        if (n && n->onRefresh) {
            auto cb = n->onRefresh;
            cb();
        }
    }, LV_EVENT_CLICKED, navRef);
    lv_obj_add_event_cb(btnRefresh, [](lv_event_t* e) {
        auto* n = static_cast<NavCbHolder*>(lv_event_get_user_data(e));
        delete n;
    }, LV_EVENT_DELETE, navRef);

    // 2. Tarjeta Resumen RF y Saturación del Sector
    lv_obj_t* cardRf = lv_obj_create(parent);
    lv_obj_set_size(cardRf, lv_pct(100), 54);
    lv_obj_set_style_bg_color(cardRf, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(cardRf, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_set_style_border_width(cardRf, 1, 0);
    lv_obj_set_style_radius(cardRf, 6, 0);
    lv_obj_set_style_pad_all(cardRf, 6, 0);

    lv_obj_t* lblRf = lv_label_create(cardRf);
    char rfBuf[160];
    if (telemetry.success) {
        snprintf(rfBuf, sizeof(rfBuf), "SSID: %s | %d MHz (BW: %d MHz) | Ruido: %d dBm\nAirtime: %d%% (Tx %d%% / Rx %d%%) | CPEs Activos: %d | GPS: %d Sats",
                 telemetry.ssid.c_str(), telemetry.frequency, telemetry.channelWidth, telemetry.noiseFloor,
                 telemetry.airtimeTotal, telemetry.airtimeTx, telemetry.airtimeRx,
                 (int)telemetry.stations.size(), telemetry.gpsSats);
    } else {
        snprintf(rfBuf, sizeof(rfBuf), "Estado: %s\nPulsa Actualizar para reconectar",
                 telemetry.errorMessage.empty() ? "Desconectado" : telemetry.errorMessage.c_str());
    }
    lv_label_set_text(lblRf, rfBuf);
    lv_obj_set_style_text_color(lblRf, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    // 3. Listado Scrollable de Clientes CPE
    lv_obj_t* listCpe = lv_obj_create(parent);
    lv_obj_set_size(listCpe, lv_pct(100), lv_pct(72));
    lv_obj_set_style_bg_opa(listCpe, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(listCpe, 0, 0);
    lv_obj_set_style_pad_all(listCpe, 4, 0);
    lv_obj_set_flex_flow(listCpe, LV_FLEX_FLOW_COLUMN);

    if (telemetry.stations.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(listCpe);
        lv_label_set_text(emptyLbl, telemetry.success ? "No hay estaciones CPE conectadas a este sector." : "Sin datos de telemetria. Pulsa Actualizar.");
        lv_obj_set_style_text_color(emptyLbl, lv_color_hex(omni_colors::TEXT_MUTED), 0);
        return;
    }

    for (const auto& sta : telemetry.stations) {
        lv_obj_t* row = lv_obj_create(listCpe);
        lv_obj_set_size(row, lv_pct(100), 56);
        lv_obj_set_style_bg_color(row, lv_color_hex(omni_colors::CARD_BG), 0);
        
        // Borde rojo si hay alerta de cable o desalineación severa
        bool hasAlert = sta.cableFault || (sta.chainDelta > 4);
        lv_obj_set_style_border_color(row, hasAlert ? lv_color_hex(omni_colors::ALERT_RED) : lv_color_hex(omni_colors::BORDER), 0);
        lv_obj_set_style_border_width(row, hasAlert ? 2 : 1, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* colLeft = lv_obj_create(row);
        lv_obj_set_size(colLeft, lv_pct(75), lv_pct(100));
        lv_obj_set_style_bg_opa(colLeft, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(colLeft, 0, 0);
        lv_obj_set_style_pad_all(colLeft, 0, 0);
        lv_obj_set_flex_flow(colLeft, LV_FLEX_FLOW_COLUMN);

        lv_obj_t* lblName = lv_label_create(colLeft);
        std::string dName = (sta.name.empty() ? sta.mac : sta.name) + "  [" + sta.ip + "]";
        lv_label_set_text(lblName, dName.c_str());
        lv_obj_set_style_text_color(lblName, lv_color_white(), 0);

        lv_obj_t* lblSub = lv_label_create(colLeft);
        char subBuf[128];
        if (sta.cableFault) {
            snprintf(subBuf, sizeof(subBuf), "! ALERTA CABLE LAN: %s | %d/%d Mbps",
                     (!sta.ethPlugged ? "Desconectado" : "10 Mbps"), sta.dlCapacityMbps, sta.ulCapacityMbps);
            lv_obj_set_style_text_color(lblSub, lv_color_hex(omni_colors::ALERT_RED), 0);
        } else if (sta.chainDelta > 4) {
            snprintf(subBuf, sizeof(subBuf), "! DESALINEADO Δ%d dB (Ch0:%d/Ch1:%d) | %d/%d Mbps",
                     sta.chainDelta, sta.signalChain0, sta.signalChain1, sta.dlCapacityMbps, sta.ulCapacityMbps);
            lv_obj_set_style_text_color(lblSub, lv_color_hex(omni_colors::ALERT_RED), 0);
        } else {
            snprintf(subBuf, sizeof(subBuf), "%d dBm (Ch0:%d/Ch1:%d) | %d/%d Mbps | LAN %dM",
                     sta.signalDbm, sta.signalChain0, sta.signalChain1, sta.dlCapacityMbps, sta.ulCapacityMbps, sta.ethSpeedMbps);
            lv_obj_set_style_text_color(lblSub, lv_color_hex(omni_colors::TEXT_MUTED), 0);
        }
        lv_label_set_text(lblSub, subBuf);

        // Botón Diagnosticar CPE
        lv_obj_t* btnDiag = lv_button_create(row);
        lv_obj_set_size(btnDiag, 75, 34);
        lv_obj_set_style_bg_color(btnDiag, lv_color_hex(0x1E293B), 0);
        lv_obj_t* lD = lv_label_create(btnDiag);
        lv_label_set_text(lD, "Ver " LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(lD, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

        struct CpeHolder {
            cbdos::network::AirOSStation cpe;
            OnSelectCpeCb cb;
        };
        auto* ch = new CpeHolder{sta, onSelectCpe};
        lv_obj_add_event_cb(btnDiag, [](lv_event_t* e) {
            auto* h = static_cast<CpeHolder*>(lv_event_get_user_data(e));
            if (h && h->cb) {
                auto cb = h->cb;
                auto cpe = h->cpe;
                cb(cpe);
            }
        }, LV_EVENT_CLICKED, ch);
        lv_obj_add_event_cb(btnDiag, [](lv_event_t* e) {
            auto* h = static_cast<CpeHolder*>(lv_event_get_user_data(e));
            delete h;
        }, LV_EVENT_DELETE, ch);
    }
}

} // namespace ui
} // namespace cbdos
