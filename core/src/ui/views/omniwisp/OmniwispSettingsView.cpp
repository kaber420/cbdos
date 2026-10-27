#include "OmniwispSettingsView.hpp"
#include "../../UIManager.hpp"
#include <cstdio>
#include <cstdlib>

namespace cbdos {
namespace ui {

lv_obj_t* OmniwispSettingsView::s_taUser = nullptr;
lv_obj_t* OmniwispSettingsView::s_taPass = nullptr;
lv_obj_t* OmniwispSettingsView::s_taPort = nullptr;
lv_obj_t* OmniwispSettingsView::s_taTimeout = nullptr;

void OmniwispSettingsView::render(lv_obj_t* parent,
                                  const OmniSettings& currentSettings,
                                  OnSaveSettingsCb onSave) {
    lv_obj_clean(parent);

    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_width(container, lv_pct(100));
    lv_obj_set_flex_grow(container, 1);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 4, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(container, 6, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t* card = lv_obj_create(container);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(omni_colors::CARD_BG), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(omni_colors::BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);

    lv_obj_t* lblTitle = lv_label_create(card);
    lv_label_set_text(lblTitle, "Credenciales de Campo por Defecto");
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(omni_colors::TEAL_ACCENT), 0);

    // Usuario
    lv_obj_t* l1 = lv_label_create(card);
    lv_label_set_text(l1, "Usuario predeterminado:");
    lv_obj_set_style_text_color(l1, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taUser = lv_textarea_create(card);
    lv_obj_set_size(s_taUser, lv_pct(100), 34);
    lv_textarea_set_text(s_taUser, currentSettings.defaultUser.c_str());
    lv_textarea_set_one_line(s_taUser, true);
    UIManager::attachKeyboard(s_taUser);

    // Contraseña
    lv_obj_t* l2 = lv_label_create(card);
    lv_label_set_text(l2, "Contrasena predeterminada:");
    lv_obj_set_style_text_color(l2, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taPass = lv_textarea_create(card);
    lv_obj_set_size(s_taPass, lv_pct(100), 34);
    lv_textarea_set_text(s_taPass, currentSettings.defaultPass.c_str());
    lv_textarea_set_password_mode(s_taPass, true);
    lv_textarea_set_one_line(s_taPass, true);
    UIManager::attachKeyboard(s_taPass);

    // Puerto y Timeout en Fila Flex
    lv_obj_t* rowNet = lv_obj_create(card);
    lv_obj_set_size(rowNet, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowNet, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowNet, 0, 0);
    lv_obj_set_style_pad_all(rowNet, 0, 0);
    lv_obj_set_flex_flow(rowNet, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rowNet, 8, 0);

    // Columna Puerto
    lv_obj_t* colPort = lv_obj_create(rowNet);
    lv_obj_set_flex_grow(colPort, 1);
    lv_obj_set_height(colPort, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(colPort, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colPort, 0, 0);
    lv_obj_set_style_pad_all(colPort, 0, 0);
    lv_obj_set_flex_flow(colPort, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* l3 = lv_label_create(colPort);
    lv_label_set_text(l3, "Puerto Web:");
    lv_obj_set_style_text_color(l3, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taPort = lv_textarea_create(colPort);
    lv_obj_set_size(s_taPort, lv_pct(100), 34);
    lv_textarea_set_text(s_taPort, std::to_string(currentSettings.defaultPort).c_str());
    lv_textarea_set_one_line(s_taPort, true);
    UIManager::attachKeyboard(s_taPort);

    // Columna Timeout
    lv_obj_t* colTimeout = lv_obj_create(rowNet);
    lv_obj_set_flex_grow(colTimeout, 1);
    lv_obj_set_height(colTimeout, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(colTimeout, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(colTimeout, 0, 0);
    lv_obj_set_style_pad_all(colTimeout, 0, 0);
    lv_obj_set_flex_flow(colTimeout, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* l4 = lv_label_create(colTimeout);
    lv_label_set_text(l4, "Timeout Escaneo (ms):");
    lv_obj_set_style_text_color(l4, lv_color_hex(omni_colors::TEXT_MUTED), 0);
    s_taTimeout = lv_textarea_create(colTimeout);
    lv_obj_set_size(s_taTimeout, lv_pct(100), 34);
    lv_textarea_set_text(s_taTimeout, std::to_string(currentSettings.scanTimeoutMs).c_str());
    lv_textarea_set_one_line(s_taTimeout, true);
    UIManager::attachKeyboard(s_taTimeout);

    // Botón Guardar Ajustes
    lv_obj_t* btnSave = lv_button_create(card);
    lv_obj_set_size(btnSave, lv_pct(100), 38);
    lv_obj_set_style_bg_color(btnSave, lv_color_hex(omni_colors::TEAL_ACCENT), 0);
    lv_obj_set_style_radius(btnSave, 6, 0);
    lv_obj_t* lblBtn = lv_label_create(btnSave);
    lv_label_set_text(lblBtn, "Guardar Ajustes");
    lv_obj_set_style_text_color(lblBtn, lv_color_hex(0x06080E), 0);
    lv_obj_center(lblBtn);

    struct SaveHolder {
        OnSaveSettingsCb cb;
    };
    auto* holder = new SaveHolder{onSave};

    lv_obj_add_event_cb(btnSave, [](lv_event_t* e) {
        auto* h = static_cast<SaveHolder*>(lv_event_get_user_data(e));
        if (!h || !h->cb) return;

        OmniSettings s;
        if (s_taUser) {
            const char* u = lv_textarea_get_text(s_taUser);
            if (u) s.defaultUser = u;
        }
        if (s_taPass) {
            const char* p = lv_textarea_get_text(s_taPass);
            if (p) s.defaultPass = p;
        }
        if (s_taPort) {
            const char* pt = lv_textarea_get_text(s_taPort);
            if (pt) s.defaultPort = static_cast<uint16_t>(std::atoi(pt));
            if (s.defaultPort == 0) s.defaultPort = 443;
        }
        if (s_taTimeout) {
            const char* to = lv_textarea_get_text(s_taTimeout);
            if (to) s.scanTimeoutMs = static_cast<uint32_t>(std::atoi(to));
            if (s.scanTimeoutMs < 500) s.scanTimeoutMs = 3000;
        }

        h->cb(s);
        UIManager::closeKeyboard();
    }, LV_EVENT_CLICKED, holder);

    lv_obj_add_event_cb(btnSave, [](lv_event_t* e) {
        auto* h = static_cast<SaveHolder*>(lv_event_get_user_data(e));
        delete h;
    }, LV_EVENT_DELETE, holder);
}

} // namespace ui
} // namespace cbdos
