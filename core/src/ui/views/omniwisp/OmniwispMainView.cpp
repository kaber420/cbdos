#include "OmniwispMainView.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

void OmniwispMainView::render(lv_obj_t* parent,
                             const std::vector<cbdos::omniwisp::NetworkGroup>& groups,
                             const std::vector<cbdos::omniwisp::SectorDevice>& sectors,
                             const std::string& activeGroupId,
                             std::function<void(const std::string& newGroupId)> onGroupFilterChanged,
                             OnSelectSectorCb onSelectSector,
                             OnAddGroupRequestCb onAddGroup,
                             OnAddSectorRequestCb onAddSector) {
    lv_obj_clean(parent);

    // 1. Barra de Control Superior con Selector de Grupos/Torres y Botones [+]
    lv_obj_t* topBar = lv_obj_create(parent);
    lv_obj_set_size(topBar, lv_pct(100), 44);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(topBar, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(topBar, 1, 0);
    lv_obj_set_style_radius(topBar, 6, 0);
    lv_obj_set_style_pad_all(topBar, 4, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Contenedor scrollable horizontal de pestañas de grupos
    lv_obj_t* tabScroll = lv_obj_create(topBar);
    lv_obj_set_size(tabScroll, lv_pct(60), lv_pct(100));
    lv_obj_set_style_bg_opa(tabScroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tabScroll, 0, 0);
    lv_obj_set_style_pad_all(tabScroll, 0, 0);
    lv_obj_set_flex_flow(tabScroll, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tabScroll, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Pestaña "Todos"
    lv_obj_t* btnAll = lv_button_create(tabScroll);
    lv_obj_set_size(btnAll, LV_SIZE_CONTENT, 32);
    bool isAll = activeGroupId.empty();
    lv_obj_set_style_bg_color(btnAll, isAll ? lv_color_hex(omni_colors::TEAL_ACCENT) : lv_color_hex(0x1E293B), 0);
    lv_obj_t* lAll = lv_label_create(btnAll);
    lv_label_set_text(lAll, "Todos");
    lv_obj_set_style_text_color(lAll, isAll ? lv_color_hex(0x06080E) : lv_color_hex(0xE2E8F0), 0);

    struct FilterHolder {
        std::string gId;
        std::function<void(const std::string&)> cb;
    };
    auto* allH = new FilterHolder{"", onGroupFilterChanged};
    lv_obj_add_event_cb(btnAll, [](lv_event_t* e) {
        auto* h = static_cast<FilterHolder*>(lv_event_get_user_data(e));
        if (h && h->cb) h->cb(h->gId);
        delete h;
    }, LV_EVENT_CLICKED, allH);

    // Pestañas dinámicas por cada PoP/Torre/Ciudad
    for (const auto& g : groups) {
        lv_obj_t* bGroup = lv_button_create(tabScroll);
        lv_obj_set_size(bGroup, LV_SIZE_CONTENT, 32);
        bool isAct = (activeGroupId == g.id);
        lv_obj_set_style_bg_color(bGroup, isAct ? lv_color_hex(omni_colors::TEAL_ACCENT) : lv_color_hex(0x1E293B), 0);
        lv_obj_t* lG = lv_label_create(bGroup);
        lv_label_set_text(lG, g.name.c_str());
        lv_obj_set_style_text_color(lG, isAct ? lv_color_hex(0x06080E) : lv_color_hex(0xE2E8F0), 0);

        auto* gH = new FilterHolder{g.id, onGroupFilterChanged};
        lv_obj_add_event_cb(bGroup, [](lv_event_t* e) {
            auto* h = static_cast<FilterHolder*>(lv_event_get_user_data(e));
            if (h && h->cb) h->cb(h->gId);
            delete h;
        }, LV_EVENT_CLICKED, gH);
    }

    // Grupo de botones [+] Agregar
    lv_obj_t* btnGroup = lv_obj_create(topBar);
    lv_obj_set_size(btnGroup, LV_SIZE_CONTENT, lv_pct(100));
    lv_obj_set_style_bg_opa(btnGroup, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnGroup, 0, 0);
    lv_obj_set_style_pad_all(btnGroup, 0, 0);
    lv_obj_set_flex_flow(btnGroup, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnGroup, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón "+ PoP/Torre"
    lv_obj_t* btnAddGrp = lv_button_create(btnGroup);
    lv_obj_set_style_bg_color(btnAddGrp, lv_color_hex(0x1E293B), 0);
    lv_obj_t* lblAddG = lv_label_create(btnAddGrp);
    lv_label_set_text(lblAddG, LV_SYMBOL_DIRECTORY " + PoP");
    lv_obj_set_style_text_color(lblAddG, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    struct AddGrpHolder {
        std::string parentId;
        OnAddGroupRequestCb cb;
    };
    auto* agH = new AddGrpHolder{activeGroupId, onAddGroup};
    lv_obj_add_event_cb(btnAddGrp, [](lv_event_t* e) {
        auto* h = static_cast<AddGrpHolder*>(lv_event_get_user_data(e));
        if (h && h->cb) h->cb(h->parentId);
        delete h;
    }, LV_EVENT_CLICKED, agH);

    // Botón "+ Sector / AP"
    lv_obj_t* btnAddAp = lv_button_create(btnGroup);
    lv_obj_set_style_bg_color(btnAddAp, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_t* lblAddAp = lv_label_create(btnAddAp);
    lv_label_set_text(lblAddAp, LV_SYMBOL_PLUS " + Radio AP");
    lv_obj_set_style_text_color(lblAddAp, lv_color_hex(0x06080E), 0);

    struct AddApHolder {
        std::string gId;
        OnAddSectorRequestCb cb;
    };
    auto* apH = new AddApHolder{activeGroupId, onAddSector};
    lv_obj_add_event_cb(btnAddAp, [](lv_event_t* e) {
        auto* h = static_cast<AddApHolder*>(lv_event_get_user_data(e));
        if (h && h->cb) h->cb(h->gId);
        delete h;
    }, LV_EVENT_CLICKED, apH);

    // 2. Contenedor de Sectores / APs (Scrollable)
    lv_obj_t* listSectors = lv_obj_create(parent);
    lv_obj_set_size(listSectors, lv_pct(100), lv_pct(88));
    lv_obj_set_style_bg_opa(listSectors, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(listSectors, 0, 0);
    lv_obj_set_style_pad_all(listSectors, 4, 0);
    lv_obj_set_flex_flow(listSectors, LV_FLEX_FLOW_COLUMN);

    // Filtrar sectores según la pestaña seleccionada
    std::vector<cbdos::omniwisp::SectorDevice> filtered;
    for (const auto& s : sectors) {
        if (activeGroupId.empty() || s.groupId == activeGroupId) {
            filtered.push_back(s);
        }
    }

    if (filtered.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(listSectors);
        lv_label_set_text(emptyLbl, "Sin sectores configurados en esta torre o PoP.\nPulsa '+ Radio AP' para agregar una IP de tu red.");
        lv_obj_set_style_text_color(emptyLbl, lv_color_hex(omni_colors::TEXT_MUTED), 0);
        return;
    }

    for (const auto& sec : filtered) {
        lv_obj_t* card = lv_obj_create(listSectors);
        lv_obj_set_size(card, lv_pct(100), 58);
        lv_obj_set_style_bg_color(card, lv_color_hex(omni_colors::CARD_BG), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(omni_colors::BORDER), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* colLeft = lv_obj_create(card);
        lv_obj_set_size(colLeft, lv_pct(70), lv_pct(100));
        lv_obj_set_style_bg_opa(colLeft, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(colLeft, 0, 0);
        lv_obj_set_style_pad_all(colLeft, 0, 0);
        lv_obj_set_flex_flow(colLeft, LV_FLEX_FLOW_COLUMN);

        lv_obj_t* lblName = lv_label_create(colLeft);
        std::string n = LV_SYMBOL_WIFI " " + sec.name;
        lv_label_set_text(lblName, n.c_str());
        lv_obj_set_style_text_color(lblName, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

        lv_obj_t* lblSub = lv_label_create(colLeft);
        char subBuf[96];
        snprintf(subBuf, sizeof(subBuf), "IP: %s:%d | Usuario: %s", sec.host.c_str(), sec.port, sec.user.c_str());
        lv_label_set_text(lblSub, subBuf);
        lv_obj_set_style_text_color(lblSub, lv_color_hex(omni_colors::TEXT_MUTED), 0);

        // Botón Conectar / Monitor
        lv_obj_t* btnMon = lv_button_create(card);
        lv_obj_set_size(btnMon, 90, 36);
        lv_obj_set_style_bg_color(btnMon, lv_color_hex(0x1E293B), 0);
        lv_obj_t* lM = lv_label_create(btnMon);
        lv_label_set_text(lM, "Monitor " LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(lM, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

        struct SecHolder {
            cbdos::omniwisp::SectorDevice sec;
            OnSelectSectorCb cb;
        };
        auto* sh = new SecHolder{sec, onSelectSector};
        lv_obj_add_event_cb(btnMon, [](lv_event_t* e) {
            auto* h = static_cast<SecHolder*>(lv_event_get_user_data(e));
            if (h && h->cb) h->cb(h->sec);
            delete h;
        }, LV_EVENT_CLICKED, sh);
    }
}

} // namespace ui
} // namespace cbdos
