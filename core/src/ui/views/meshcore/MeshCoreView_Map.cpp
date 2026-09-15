#include "../MeshCoreView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/serial.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/persistence.hpp"
#include "cbdos/meshcore/meshcore_store.hpp"
#include "cbdos/meshcore/mesh_emoji.hpp"
#include "../../assets/lottie_sample.h"
#include "../../assets/wink_star_assets.h"
#include "MeshCoreCommon.hpp"
#include <algorithm>
#include <map>
#include <vector>

namespace cbdos {
namespace ui {
using namespace meshcore_ui;

void MeshCoreView::buildMapTab(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, 4, 0);
    lv_obj_set_style_pad_row(tab, 4, 0);
    DefaultTheme::disableScroll(tab);

    m_lblMapCount = lv_label_create(tab);
    lv_label_set_text(m_lblMapCount, "Sin posiciones");
    lv_obj_set_style_text_font(m_lblMapCount, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblMapCount, DefaultTheme::getMutedTextColor(), 0);

    m_mapContainer = lv_obj_create(tab);
    lv_obj_set_width(m_mapContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_mapContainer, 1);
    DefaultTheme::applySunkenCard(m_mapContainer, 8);
    lv_obj_set_flex_flow(m_mapContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_mapContainer, 6, 0);
    lv_obj_set_style_pad_row(m_mapContainer, 4, 0);
    lv_obj_set_scroll_dir(m_mapContainer, LV_DIR_VER);

    lv_obj_t* btnGpx = makeButton(tab, LV_SYMBOL_DOWNLOAD " Exportar GPX", 200, 36, 0);
    lv_obj_add_event_cb(btnGpx, mapExportCb, LV_EVENT_CLICKED, this);
}

void MeshCoreView::refreshMapList() {
    if (!m_mapContainer || !lv_obj_is_valid(m_mapContainer)) return;
    auto& client = meshcore::MeshCoreClient::getInstance();
    const auto& contacts = client.getContacts();
    const auto& self = client.getSelfInfo();

    std::vector<const meshcore::MeshContact*> withGps;
    for (const auto& c : contacts) {
        if (c.lat != 0.0 || c.lon != 0.0) withGps.push_back(&c);
    }
    if (m_lblMapCount && lv_obj_is_valid(m_lblMapCount)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%u nodos con posicion", (unsigned)withGps.size());
        lv_label_set_text(m_lblMapCount, buf);
    }
    lv_obj_clean(m_mapContainer);
    if (withGps.empty()) {
        lv_obj_t* lbl = lv_label_create(m_mapContainer);
        lv_label_set_text(lbl, "Ningun contacto comparte posicion.\nActiva compartir posicion en tu advert\ny pide a tus contactos que hagan lo mismo.");
        lv_obj_set_style_text_color(lbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        return;
    }
    for (const auto* c : withGps) {
        lv_obj_t* card = lv_obj_create(m_mapContainer);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        DefaultTheme::applySunkenCard(card, 8);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_t* lbl = lv_label_create(card);
        char buf[160];
        if (self.valid && (self.advLatitude != 0.0 || self.advLongitude != 0.0)) {
            snprintf(buf, sizeof(buf), "%s\n%.5f, %.5f · %.1f km · %s",
                     c->name.empty() ? c->prefixHex12.c_str() : c->name.c_str(), c->lat, c->lon,
                     haversineKm(self.advLatitude, self.advLongitude, c->lat, c->lon),
                     fmtAgo(c->lastAdvert ? c->lastAdvert : c->lastmod).c_str());
        } else {
            snprintf(buf, sizeof(buf), "%s\n%.5f, %.5f · %s",
                     c->name.empty() ? c->prefixHex12.c_str() : c->name.c_str(), c->lat, c->lon,
                     fmtAgo(c->lastAdvert ? c->lastAdvert : c->lastmod).c_str());
        }
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_width(lbl, LV_PCT(100));
    }
}

void MeshCoreView::exportGpx() {
    auto& client = meshcore::MeshCoreClient::getInstance();
    std::string gpx = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\">\n";
    int n = 0;
    for (const auto& c : client.getContacts()) {
        if (c.lat == 0.0 && c.lon == 0.0) continue;
        char wpt[256];
        snprintf(wpt, sizeof(wpt), "  <wpt lat=\"%.6f\" lon=\"%.6f\"><name>%s</name></wpt>\n", c.lat,
                 c.lon, (c.name.empty() ? c.prefixHex12 : c.name).c_str());
        gpx += wpt;
        ++n;
    }
    gpx += "</gpx>\n";
    if (n == 0) {
        UIManager::showToast("Sin posiciones para exportar.");
        return;
    }
    cbdos::storage::makeDir("/flash/data/meshcore");
    if (cbdos::storage::writeFile("/flash/data/meshcore/map.gpx", gpx)) {
        char buf[80];
        snprintf(buf, sizeof(buf), "GPX exportado: %d nodos.", n);
        UIManager::showToast(buf);
    } else {
        UIManager::showToast("Fallo al escribir GPX.");
    }
}

void MeshCoreView::mapExportCb(lv_event_t* e) {
    auto* self = static_cast<MeshCoreView*>(lv_event_get_user_data(e));
    if (self) self->exportGpx();
}


} // namespace ui
} // namespace cbdos
