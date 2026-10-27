#include "OmniwispAddGroupModal.hpp"
#include "../../UIManager.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* OmniwispAddGroupModal::s_mask = nullptr;
lv_obj_t* OmniwispAddGroupModal::s_card = nullptr;
lv_obj_t* OmniwispAddGroupModal::s_taName = nullptr;
lv_obj_t* OmniwispAddGroupModal::s_taNotes = nullptr;
std::string OmniwispAddGroupModal::s_parentId = "";
OmniwispAddGroupModal::OnGroupSavedCb OmniwispAddGroupModal::s_onSave = nullptr;

void OmniwispAddGroupModal::show(lv_obj_t* parent, const std::string& parentId, OnGroupSavedCb onSave) {
    close();
    s_parentId = parentId;
    s_onSave = onSave;

    lv_obj_t* root = parent ? parent : lv_screen_active();
    s_mask = lv_obj_create(root);
    lv_obj_set_size(s_mask, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_mask, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_mask, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_mask, 0, 0);
    lv_obj_set_style_pad_all(s_mask, 0, 0);
    lv_obj_center(s_mask);

    s_card = lv_obj_create(s_mask);
    lv_obj_set_size(s_card, 300, 260);
    lv_obj_center(s_card);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(s_card, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_set_style_border_width(s_card, 1, 0);
    lv_obj_set_style_radius(s_card, 10, 0);
    lv_obj_set_style_pad_all(s_card, 10, 0);
    lv_obj_set_flex_flow(s_card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lblTitle = lv_label_create(s_card);
    if (s_parentId.empty()) {
        lv_label_set_text(lblTitle, LV_SYMBOL_DIRECTORY " Crear Zona");
    } else {
        lv_label_set_text(lblTitle, LV_SYMBOL_DIRECTORY " Crear PoP");
    }
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    lv_obj_t* lblN = lv_label_create(s_card);
    lv_label_set_text(lblN, s_parentId.empty() ? "Nombre de la Zona:" : "Nombre del PoP:");
    lv_obj_set_style_text_color(lblN, lv_color_hex(omni_colors::TEXT_MUTED), 0);

    s_taName = lv_textarea_create(s_card);
    lv_obj_set_size(s_taName, lv_pct(100), 38);
    lv_textarea_set_placeholder_text(s_taName, s_parentId.empty() ? "Zona Norte..." : "PoP Centro...");
    lv_textarea_set_one_line(s_taName, true);
    UIManager::attachKeyboard(s_taName);

    lv_obj_t* lblNot = lv_label_create(s_card);
    lv_label_set_text(lblNot, "Notas o Ubicacion (opcional):");
    lv_obj_set_style_text_color(lblNot, lv_color_hex(omni_colors::TEXT_MUTED), 0);

    s_taNotes = lv_textarea_create(s_card);
    lv_obj_set_size(s_taNotes, lv_pct(100), 38);
    lv_textarea_set_placeholder_text(s_taNotes, "Km 14, Acceso Llave...");
    lv_textarea_set_one_line(s_taNotes, true);
    UIManager::attachKeyboard(s_taNotes);

    // Botones
    lv_obj_t* btnRow = lv_obj_create(s_card);
    lv_obj_set_size(btnRow, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0x334155), 0);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_add_event_cb(btnCancel, [](lv_event_t*) {
        OmniwispAddGroupModal::close();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btnSave = lv_button_create(btnRow);
    lv_obj_set_style_bg_color(btnSave, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_t* lblS = lv_label_create(btnSave);
    lv_label_set_text(lblS, "Guardar");
    lv_obj_set_style_text_color(lblS, lv_color_hex(0x06080E), 0);

    lv_obj_add_event_cb(btnSave, [](lv_event_t*) {
        if (!s_taName) return;
        const char* nameStr = lv_textarea_get_text(s_taName);
        if (!nameStr || nameStr[0] == '\0') return;

        const char* notesStr = s_taNotes ? lv_textarea_get_text(s_taNotes) : "";

        cbdos::omniwisp::NetworkGroup grp;
        grp.name = nameStr;
        grp.notes = notesStr ? notesStr : "";
        grp.parentId = s_parentId;
        grp.id = (s_parentId.empty() ? "city_" : "pop_") + std::to_string(lv_tick_get());

        if (s_onSave) s_onSave(grp);
        OmniwispAddGroupModal::close();
    }, LV_EVENT_CLICKED, nullptr);
}

void OmniwispAddGroupModal::close() {
    UIManager::closeKeyboard();
    if (s_mask && lv_obj_is_valid(s_mask)) {
        lv_obj_delete_async(s_mask);
    }
    s_mask = nullptr;
    s_card = nullptr;
    s_taName = nullptr;
    s_taNotes = nullptr;
    s_onSave = nullptr;
}

} // namespace ui
} // namespace cbdos
