#include "OmniwispDiscoveryView.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* OmniwispDiscoveryView::s_listContainer = nullptr;
lv_obj_t* OmniwispDiscoveryView::s_lblStatus = nullptr;
OmniwispDiscoveryView::OnConnectDeviceCb OmniwispDiscoveryView::s_onConnect = nullptr;
OmniwispDiscoveryView::OnAddDeviceCb OmniwispDiscoveryView::s_onAdd = nullptr;

void OmniwispDiscoveryView::updateStatus() {
    if (!s_lblStatus) return;
    auto& disco = cbdos::network::NeighborDiscoveryService::getInstance();
    if (disco.isScanning()) {
        lv_label_set_text(s_lblStatus, "Buscando broadcast...");
        lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    } else {
        auto devs = disco.getDiscoveredDevices();
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%u equipos detectados", static_cast<unsigned int>(devs.size()));
        lv_label_set_text(s_lblStatus, buf);
        lv_obj_set_style_text_color(s_lblStatus, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    }
}

void OmniwispDiscoveryView::refreshList() {
    if (!s_listContainer) return;
    lv_obj_clean(s_listContainer);

    auto& disco = cbdos::network::NeighborDiscoveryService::getInstance();
    auto devices = disco.getDiscoveredDevices();

    if (devices.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(s_listContainer);
        if (disco.isScanning()) {
            lv_label_set_text(emptyLbl, "Sondeando broadcast UDP (10001 / 5678)...");
        } else {
            lv_label_set_text(emptyLbl, "Sin vecinos detectados en la red.\nPresione [Escanear] para iniciar sondeo.");
        }
        lv_obj_set_style_text_color(emptyLbl, lv_color_hex(omni_colors::TEXT_MUTED), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(emptyLbl, lv_pct(100));
        lv_obj_set_style_pad_top(emptyLbl, 30, 0);
        return;
    }

    for (const auto& dev : devices) {
        lv_obj_t* card = lv_obj_create(s_listContainer);
        lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(omni_colors::CARD_BG), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(omni_colors::BORDER), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 4, 0);

        // Fila 1: Marca + Identidad / Modelo
        lv_obj_t* rowHeader = lv_obj_create(card);
        lv_obj_set_size(rowHeader, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(rowHeader, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rowHeader, 0, 0);
        lv_obj_set_style_pad_all(rowHeader, 0, 0);
        lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lblBrand = lv_label_create(rowHeader);
        std::string brandTag = "[" + dev.getBrandString() + "] ";
        lv_label_set_text(lblBrand, brandTag.c_str());
        uint32_t bColor = (dev.brand == cbdos::network::NeighborBrand::Ubiquiti) ? omni_colors::TEAL_ACCENT : 0xF59E0B;
        lv_obj_set_style_text_color(lblBrand, lv_color_hex(bColor), 0);

        lv_obj_t* lblTitle = lv_label_create(rowHeader);
        std::string titleStr;
        if (!dev.identity.empty() && !dev.model.empty()) {
            titleStr = dev.identity + " (" + dev.model + ")";
        } else if (!dev.identity.empty()) {
            titleStr = dev.identity;
        } else if (!dev.model.empty()) {
            titleStr = dev.model;
        } else {
            titleStr = "Dispositivo";
        }
        lv_label_set_text(lblTitle, titleStr.c_str());
        lv_obj_set_style_text_color(lblTitle, lv_color_hex(omni_colors::TEXT_LIGHT), 0);
        lv_label_set_long_mode(lblTitle, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(lblTitle, 1);

        // Fila 2: IP, Puerto/Interfaz, Firmware, MAC
        lv_obj_t* lblDetails = lv_label_create(card);
        char detBuf[160];
        if (!dev.interface.empty()) {
            std::snprintf(detBuf, sizeof(detBuf), "IP: %s  |  Pto: %s  |  FW: %s  |  MAC: %s",
                          dev.ip.c_str(),
                          dev.interface.c_str(),
                          dev.firmware.empty() ? "-" : dev.firmware.c_str(),
                          dev.getMacString().c_str());
        } else {
            std::snprintf(detBuf, sizeof(detBuf), "IP: %s  |  FW: %s  |  MAC: %s",
                          dev.ip.c_str(),
                          dev.firmware.empty() ? "-" : dev.firmware.c_str(),
                          dev.getMacString().c_str());
        }
        lv_label_set_text(lblDetails, detBuf);
        lv_obj_set_style_text_color(lblDetails, lv_color_hex(omni_colors::TEXT_MUTED), 0);

        // Fila 3: Botones de Acción (Conectar / Agregar)
        lv_obj_t* btnRow = lv_obj_create(card);
        lv_obj_set_size(btnRow, lv_pct(100), 34);
        lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btnRow, 0, 0);
        lv_obj_set_style_pad_all(btnRow, 0, 0);
        lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(btnRow, 8, 0);
        lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        struct DeviceActionCtx {
            std::string host;
            std::string name;
        };
        auto* ctx = new DeviceActionCtx{dev.ip, titleStr};

        // Boton Conectar
        lv_obj_t* btnConnect = lv_button_create(btnRow);
        lv_obj_set_flex_grow(btnConnect, 1);
        lv_obj_set_height(btnConnect, 30);
        lv_obj_set_style_bg_color(btnConnect, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_radius(btnConnect, 6, 0);
        lv_obj_t* lblConn = lv_label_create(btnConnect);
        lv_label_set_text(lblConn, "Conectar");
        lv_obj_set_style_text_color(lblConn, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
        lv_obj_center(lblConn);

        lv_obj_add_event_cb(btnConnect, [](lv_event_t* e) {
            auto* d = static_cast<DeviceActionCtx*>(lv_event_get_user_data(e));
            if (d && s_onConnect) {
                s_onConnect(d->host, d->name);
            }
        }, LV_EVENT_CLICKED, ctx);

        // Boton Agregar
        lv_obj_t* btnAdd = lv_button_create(btnRow);
        lv_obj_set_flex_grow(btnAdd, 1);
        lv_obj_set_height(btnAdd, 30);
        lv_obj_set_style_bg_color(btnAdd, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
        lv_obj_set_style_radius(btnAdd, 6, 0);
        lv_obj_t* lblAdd = lv_label_create(btnAdd);
        lv_label_set_text(lblAdd, "Agregar");
        lv_obj_set_style_text_color(lblAdd, lv_color_hex(0x06080E), 0);
        lv_obj_center(lblAdd);

        lv_obj_add_event_cb(btnAdd, [](lv_event_t* e) {
            auto* d = static_cast<DeviceActionCtx*>(lv_event_get_user_data(e));
            if (d && s_onAdd) {
                s_onAdd(d->host, d->name);
            }
        }, LV_EVENT_CLICKED, ctx);

        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            auto* d = static_cast<DeviceActionCtx*>(lv_event_get_user_data(e));
            delete d;
        }, LV_EVENT_DELETE, ctx);
    }
}

void OmniwispDiscoveryView::render(lv_obj_t* parent,
                                   OnConnectDeviceCb onConnect,
                                   OnAddDeviceCb onAdd) {
    lv_obj_clean(parent);
    s_onConnect = onConnect;
    s_onAdd = onAdd;

    // Barra de control superior
    lv_obj_t* topBar = lv_obj_create(parent);
    lv_obj_set_size(topBar, lv_pct(100), 40);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(topBar, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(topBar, 1, 0);
    lv_obj_set_style_radius(topBar, 6, 0);
    lv_obj_set_style_pad_all(topBar, 4, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnScan = lv_button_create(topBar);
    lv_obj_set_height(btnScan, 32);
    lv_obj_set_style_bg_color(btnScan, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(btnScan, 6, 0);
    lv_obj_t* lblBtn = lv_label_create(btnScan);
    lv_label_set_text(lblBtn, LV_SYMBOL_REFRESH " Escanear");
    lv_obj_set_style_text_color(lblBtn, lv_color_hex(omni_colors::TEXT_LIGHT), 0);
    lv_obj_center(lblBtn);

    lv_obj_add_event_cb(btnScan, [](lv_event_t*) {
        cbdos::network::NeighborDiscoveryService::getInstance().triggerScan(3000);
        OmniwispDiscoveryView::updateStatus();
    }, LV_EVENT_CLICKED, nullptr);

    s_lblStatus = lv_label_create(topBar);
    updateStatus();

    // Contenedor scrollable que toma todo el espacio vertical sobrante (flex_grow 1)
    s_listContainer = lv_obj_create(parent);
    lv_obj_set_width(s_listContainer, lv_pct(100));
    lv_obj_set_flex_grow(s_listContainer, 1);
    lv_obj_set_style_bg_opa(s_listContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_listContainer, 0, 0);
    lv_obj_set_style_pad_all(s_listContainer, 4, 0);
    lv_obj_set_flex_flow(s_listContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_listContainer, 6, 0);
    lv_obj_set_scrollbar_mode(s_listContainer, LV_SCROLLBAR_MODE_AUTO);

    refreshList();
}

} // namespace ui
} // namespace cbdos
