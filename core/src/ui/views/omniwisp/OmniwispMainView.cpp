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

    // 1. Barra de Control Superior con Selector Dropdown de Torres y Dropdown de Acciones
    lv_obj_t* topBar = lv_obj_create(parent);
    lv_obj_set_size(topBar, lv_pct(100), 44);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(topBar, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(topBar, 1, 0);
    lv_obj_set_style_radius(topBar, 6, 0);
    lv_obj_set_style_pad_all(topBar, 4, 0);
    lv_obj_set_flex_flow(topBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(topBar, 6, 0);
    lv_obj_set_flex_align(topBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Dropdown de Filtro de Grupos / PoPs (56% de ancho)
    lv_obj_t* ddGroup = lv_dropdown_create(topBar);
    lv_obj_set_width(ddGroup, lv_pct(56));
    lv_obj_set_height(ddGroup, 34);
    lv_obj_set_style_bg_color(ddGroup, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(ddGroup, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(ddGroup, 1, 0);
    lv_obj_set_style_text_color(ddGroup, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_pad_all(ddGroup, 4, 0);

    std::string groupOptions = "Todos";
    uint32_t activeGroupIdx = 0;

    std::vector<cbdos::omniwisp::NetworkGroup> orderedGroups;
    for (const auto& g : groups) {
        if (g.parentId.empty()) {
            orderedGroups.push_back(g);
            for (const auto& child : groups) {
                if (child.parentId == g.id) {
                    orderedGroups.push_back(child);
                }
            }
        }
    }
    for (const auto& g : groups) {
        bool found = false;
        for (const auto& og : orderedGroups) {
            if (og.id == g.id) { found = true; break; }
        }
        if (!found) orderedGroups.push_back(g);
    }

    for (size_t i = 0; i < orderedGroups.size(); ++i) {
        const auto& g = orderedGroups[i];
        if (g.parentId.empty()) {
            groupOptions += "\n" + g.name;
        } else {
            groupOptions += "\n  " + g.name;
        }
        if (g.id == activeGroupId) {
            activeGroupIdx = static_cast<uint32_t>(i + 1);
        }
    }
    lv_dropdown_set_options(ddGroup, groupOptions.c_str());
    lv_dropdown_set_selected(ddGroup, activeGroupIdx);

    struct FilterHolder {
        std::vector<cbdos::omniwisp::NetworkGroup> groups;
        std::function<void(const std::string&)> cb;
    };
    auto* fHolder = new FilterHolder{orderedGroups, onGroupFilterChanged};
    lv_obj_add_event_cb(ddGroup, [](lv_event_t* e) {
        auto* h = static_cast<FilterHolder*>(lv_event_get_user_data(e));
        auto* dd = static_cast<lv_obj_t*>(lv_event_get_target(e));
        if (h && dd && h->cb) {
            uint32_t sel = lv_dropdown_get_selected(dd);
            if (sel == 0) {
                h->cb("");
            } else if (sel - 1 < h->groups.size()) {
                h->cb(h->groups[sel - 1].id);
            }
        }
    }, LV_EVENT_VALUE_CHANGED, fHolder);
    lv_obj_add_event_cb(ddGroup, [](lv_event_t* e) {
        auto* h = static_cast<FilterHolder*>(lv_event_get_user_data(e));
        delete h;
    }, LV_EVENT_DELETE, fHolder);

    // Dropdown de Acciones (+ Nuevo: AP / PoP / Zona) (40% de ancho)
    lv_obj_t* ddActions = lv_dropdown_create(topBar);
    lv_obj_set_width(ddActions, lv_pct(40));
    lv_obj_set_height(ddActions, 34);
    lv_dropdown_set_options(ddActions, "AP\nPoP\nZona");
    lv_dropdown_set_text(ddActions, LV_SYMBOL_PLUS " Nuevo");
    lv_obj_set_style_bg_color(ddActions, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_set_style_border_width(ddActions, 0, 0);
    lv_obj_set_style_text_color(ddActions, lv_color_hex(0x06080E), 0);
    lv_obj_set_style_pad_all(ddActions, 4, 0);

    struct ActionHolder {
        std::string activeGroupId;
        OnAddSectorRequestCb onAddSector;
        OnAddGroupRequestCb onAddGroup;
    };
    auto* aHolder = new ActionHolder{activeGroupId, onAddSector, onAddGroup};
    lv_obj_add_event_cb(ddActions, [](lv_event_t* e) {
        auto* h = static_cast<ActionHolder*>(lv_event_get_user_data(e));
        auto* dd = static_cast<lv_obj_t*>(lv_event_get_target(e));
        if (h && dd) {
            uint32_t sel = lv_dropdown_get_selected(dd);
            if (sel == 0 && h->onAddSector) {
                h->onAddSector(h->activeGroupId);
            } else if (sel == 1 && h->onAddGroup) {
                h->onAddGroup(h->activeGroupId.empty() ? "pop" : h->activeGroupId);
            } else if (sel == 2 && h->onAddGroup) {
                h->onAddGroup("");
            }
        }
    }, LV_EVENT_VALUE_CHANGED, aHolder);
    lv_obj_add_event_cb(ddActions, [](lv_event_t* e) {
        auto* h = static_cast<ActionHolder*>(lv_event_get_user_data(e));
        delete h;
    }, LV_EVENT_DELETE, aHolder);

    // 2. Contenedor de Sectores / APs (Scrollable)
    lv_obj_t* listSectors = lv_obj_create(parent);
    lv_obj_set_size(listSectors, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(listSectors, 1);
    lv_obj_set_style_bg_opa(listSectors, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(listSectors, 0, 0);
    lv_obj_set_style_pad_all(listSectors, 4, 0);
    lv_obj_set_flex_flow(listSectors, LV_FLEX_FLOW_COLUMN);

    // Filtrar sectores según la selección
    std::vector<cbdos::omniwisp::SectorDevice> filtered;
    for (const auto& s : sectors) {
        if (activeGroupId.empty()) {
            filtered.push_back(s);
        } else if (s.groupId == activeGroupId) {
            filtered.push_back(s);
        } else {
            for (const auto& g : groups) {
                if (g.id == s.groupId && g.parentId == activeGroupId) {
                    filtered.push_back(s);
                    break;
                }
            }
        }
    }

    if (filtered.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(listSectors);
        lv_label_set_text(emptyLbl, "Sin sectores configurados.\nPulsa '+ Nuevo' para agregar.");
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
        lv_obj_set_size(colLeft, LV_SIZE_CONTENT, lv_pct(100));
        lv_obj_set_flex_grow(colLeft, 1);
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
        lv_obj_set_size(btnMon, 84, 34);
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
            if (h && h->cb) {
                auto cb = h->cb;
                auto sec = h->sec;
                cb(sec);
            }
        }, LV_EVENT_CLICKED, sh);
        lv_obj_add_event_cb(btnMon, [](lv_event_t* e) {
            auto* h = static_cast<SecHolder*>(lv_event_get_user_data(e));
            delete h;
        }, LV_EVENT_DELETE, sh);
    }
}

} // namespace ui
} // namespace cbdos
