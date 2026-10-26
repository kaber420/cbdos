#include "OmniwispAddApModal.hpp"
#include "../../UIManager.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

lv_obj_t* OmniwispAddApModal::s_mask = nullptr;
lv_obj_t* OmniwispAddApModal::s_card = nullptr;
lv_obj_t* OmniwispAddApModal::s_taName = nullptr;
lv_obj_t* OmniwispAddApModal::s_taHost = nullptr;
lv_obj_t* OmniwispAddApModal::s_taUser = nullptr;
lv_obj_t* OmniwispAddApModal::s_taPass = nullptr;
std::string OmniwispAddApModal::s_groupId = "";
OmniwispAddApModal::OnSectorSavedCb OmniwispAddApModal::s_onSave = nullptr;

void OmniwispAddApModal::show(lv_obj_t* parent, const std::string& defaultGroupId, OnSectorSavedCb onSave) {
    close();
    s_groupId = defaultGroupId;
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
    lv_obj_set_size(s_card, 340, 360);
    lv_obj_center(s_card);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(s_card, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_set_style_border_width(s_card, 1, 0);
    lv_obj_set_style_radius(s_card, 10, 0);
    lv_obj_set_style_pad_all(s_card, 12, 0);
    lv_obj_set_flex_flow(s_card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lblTitle = lv_label_create(s_card);
    lv_label_set_text(lblTitle, LV_SYMBOL_WIFI " Agregar Sector / Radio AP");
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    // Nombre / Sector
    lv_obj_t* l1 = lv_label_create(s_card);
    lv_label_set_text(l1, "Nombre del Sector / Antena:");
    lv_obj_set_style_text_color(l1, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taName = lv_textarea_create(s_card);
    lv_obj_set_size(s_taName, lv_pct(100), 36);
    lv_textarea_set_placeholder_text(s_taName, "Sector Norte 90°...");
    lv_textarea_set_one_line(s_taName, true);
    UIManager::attachKeyboard(s_taName);

    // IP en la red ruteada
    lv_obj_t* l2 = lv_label_create(s_card);
    lv_label_set_text(l2, "Direccion IP (Red Ruteada):");
    lv_obj_set_style_text_color(l2, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taHost = lv_textarea_create(s_card);
    lv_obj_set_size(s_taHost, lv_pct(100), 36);
    lv_textarea_set_placeholder_text(s_taHost, "10.200.10.21 o 192.168.1.20");
    lv_textarea_set_one_line(s_taHost, true);
    UIManager::attachKeyboard(s_taHost);

    // Usuario
    lv_obj_t* l3 = lv_label_create(s_card);
    lv_label_set_text(l3, "Usuario AirOS:");
    lv_obj_set_style_text_color(l3, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taUser = lv_textarea_create(s_card);
    lv_obj_set_size(s_taUser, lv_pct(100), 36);
    lv_textarea_set_text(s_taUser, "ubnt");
    lv_textarea_set_one_line(s_taUser, true);
    UIManager::attachKeyboard(s_taUser);

    // Password
    lv_obj_t* l4 = lv_label_create(s_card);
    lv_label_set_text(l4, "Contrasena:");
    lv_obj_set_style_text_color(l4, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taPass = lv_textarea_create(s_card);
    lv_obj_set_size(s_taPass, lv_pct(100), 36);
    lv_textarea_set_password_mode(s_taPass, true);
    lv_textarea_set_one_line(s_taPass, true);
    UIManager::attachKeyboard(s_taPass);

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
        OmniwispAddApModal::close();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* btnSave = lv_button_create(btnRow);
    lv_obj_set_style_bg_color(btnSave, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_t* lblS = lv_label_create(btnSave);
    lv_label_set_text(lblS, "Guardar en Vault");
    lv_obj_set_style_text_color(lblS, lv_color_hex(0x06080E), 0);

    lv_obj_add_event_cb(btnSave, [](lv_event_t*) {
        if (!s_taHost) return;
        const char* hostStr = lv_textarea_get_text(s_taHost);
        if (!hostStr || hostStr[0] == '\0') return;

        const char* nameStr = s_taName ? lv_textarea_get_text(s_taName) : "";
        const char* userStr = s_taUser ? lv_textarea_get_text(s_taUser) : "ubnt";
        const char* passStr = s_taPass ? lv_textarea_get_text(s_taPass) : "";

        cbdos::omniwisp::SectorDevice dev;
        dev.name = (nameStr && nameStr[0] != '\0') ? nameStr : hostStr;
        dev.host = hostStr;
        dev.user = (userStr && userStr[0] != '\0') ? userStr : "ubnt";
        dev.pass = passStr ? passStr : "";
        dev.port = 443;
        dev.groupId = s_groupId;
        dev.id = "sec_" + std::to_string(lv_tick_get());

        if (s_onSave) s_onSave(dev);
        OmniwispAddApModal::close();
    }, LV_EVENT_CLICKED, nullptr);
}

void OmniwispAddApModal::close() {
    UIManager::closeKeyboard();
    if (s_mask && lv_obj_is_valid(s_mask)) {
        lv_obj_delete_async(s_mask);
    }
    s_mask = nullptr;
    s_card = nullptr;
    s_taName = nullptr;
    s_taHost = nullptr;
    s_taUser = nullptr;
    s_taPass = nullptr;
    s_onSave = nullptr;
}

} // namespace ui
} // namespace cbdos
