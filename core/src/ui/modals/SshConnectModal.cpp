#include "SshConnectModal.hpp"
#include "../../themes/DefaultTheme.h"
#include "../UIManager.hpp"
#include "cbdos/network.hpp"
#include "cbdos/display.hpp"
#include "cbdos/storage.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>

namespace cbdos {
namespace ui {

static const char* SSH_PROFILES_FILE = "/sdcard/system/ssh_hosts.txt";

void SshConnectModal::loadProfiles() {
    m_profiles.clear();

    if (cbdos::storage::isSdMounted() && cbdos::storage::fileExists(SSH_PROFILES_FILE)) {
        std::string content = cbdos::storage::readFile(SSH_PROFILES_FILE);
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream lineStream(line);
            std::string name, host, portStr, user, authStr, keyPath;
            if (std::getline(lineStream, name, '|') &&
                std::getline(lineStream, host, '|') &&
                std::getline(lineStream, portStr, '|') &&
                std::getline(lineStream, user, '|') &&
                std::getline(lineStream, authStr, '|') &&
                std::getline(lineStream, keyPath)) {
                SshHostProfile p;
                p.name = name;
                p.host = host;
                p.port = (uint16_t)atoi(portStr.c_str());
                p.username = user;
                p.authType = (authStr == "key") ? cbdos::ssh::SshAuthType::PublicKey : cbdos::ssh::SshAuthType::Password;
                p.keyPath = keyPath;
                m_profiles.push_back(p);
            }
        }
    }

    if (m_profiles.empty()) {
        m_profiles.push_back({"Nuevo Servidor", "", 22, "root", cbdos::ssh::SshAuthType::Password, "", "/sdcard/keys/id_ed25519", ""});
    }
}

void SshConnectModal::saveCurrentProfile() {
    if (!cbdos::storage::isSdMounted()) {
        setStatus("Error: Inserta una MicroSD para guardar perfiles.", true);
        return;
    }

    std::string host = m_taHost ? lv_textarea_get_text(m_taHost) : "";
    if (host.empty()) {
        setStatus("Error: Escribe un Host o IP antes de guardar.", true);
        return;
    }

    std::string user = m_taUser ? lv_textarea_get_text(m_taUser) : "root";
    std::string port = m_taPort ? lv_textarea_get_text(m_taPort) : "22";
    uint32_t authSel = m_ddAuthType ? lv_dropdown_get_selected(m_ddAuthType) : 0;
    std::string keyPath = m_taKeyPath ? lv_textarea_get_text(m_taKeyPath) : "/sdcard/keys/id_ed25519";

    SshHostProfile p;
    p.name = user + "@" + host + ":" + port;
    p.host = host;
    p.port = (uint16_t)atoi(port.c_str());
    p.username = user;
    p.authType = (authSel == 1) ? cbdos::ssh::SshAuthType::PublicKey : cbdos::ssh::SshAuthType::Password;
    p.keyPath = keyPath;

    // Buscar si ya existe para actualizar o agregar
    bool found = false;
    for (auto& existing : m_profiles) {
        if (existing.host == p.host && existing.port == p.port && existing.username == p.username) {
            existing = p;
            found = true;
            break;
        }
    }
    if (!found) {
        m_profiles.push_back(p);
    }

    // Guardar archivo en MicroSD
    cbdos::storage::makeDir("/sdcard/system");
    std::string outData = "# CBDos SSH Profiles\n";
    for (const auto& item : m_profiles) {
        outData += item.name + "|" + item.host + "|" + std::to_string(item.port) + "|" +
                   item.username + "|" + (item.authType == cbdos::ssh::SshAuthType::PublicKey ? "key" : "pass") + "|" +
                   item.keyPath + "\n";
    }
    bool ok = cbdos::storage::writeFile(SSH_PROFILES_FILE, outData);
    if (ok) {
        updateProfileDropdown();
        setStatus("Perfil guardado en MicroSD.", false);
    } else {
        setStatus("Error al escribir en /sdcard/system/ssh_hosts.txt", true);
    }
}

void SshConnectModal::updateProfileDropdown() {
    if (!m_ddProfile) return;
    std::string options = "";
    for (size_t i = 0; i < m_profiles.size(); ++i) {
        options += m_profiles[i].name;
        if (i + 1 < m_profiles.size()) options += "\n";
    }
    lv_dropdown_set_options(m_ddProfile, options.c_str());
    lv_dropdown_set_selected(m_ddProfile, (uint32_t)(m_profiles.size() - 1));
}

void SshConnectModal::applySelectedProfile(size_t index) {
    if (index >= m_profiles.size()) return;
    const auto& p = m_profiles[index];
    if (m_taHost) lv_textarea_set_text(m_taHost, p.host.c_str());
    if (m_taPort) lv_textarea_set_text(m_taPort, std::to_string(p.port).c_str());
    if (m_taUser) lv_textarea_set_text(m_taUser, p.username.c_str());
    if (m_taKeyPath) lv_textarea_set_text(m_taKeyPath, p.keyPath.c_str());
    if (m_ddAuthType) {
        uint32_t sel = (p.authType == cbdos::ssh::SshAuthType::PublicKey) ? 1 : 0;
        lv_dropdown_set_selected(m_ddAuthType, sel);
        if (sel == 0) {
            lv_obj_remove_flag(m_boxPassword, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(m_boxKey, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_boxPassword, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(m_boxKey, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void SshConnectModal::show(lv_obj_t* parent, ConnectCallback onConnect) {
    (void)parent;
    close();
    m_onConnect = onConnect;
    m_passVisible = false;
    loadProfiles();

    auto caps = cbdos::display::getCapabilities();
    bool isCompact = (caps.width < 400);

    // 1. CAPA SUPERIOR ABSOLUTA (Flota por encima de todo el sistema)
    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);
    lv_obj_clear_flag(m_modalMask, LV_OBJ_FLAG_SCROLLABLE);

    // 2. Tarjeta del Formulario (con auto-scroll para teclado global de sistema)
    int cardWidth = isCompact ? 308 : 440;
    int cardHeight = isCompact ? (caps.height - 24) : 480;
    if (cardHeight < 280) cardHeight = 280;

    m_card = lv_obj_create(m_modalMask);
    lv_obj_set_width(m_card, cardWidth);
    lv_obj_set_height(m_card, cardHeight);
    lv_obj_align(m_card, LV_ALIGN_TOP_MID, 0, 8);
    DefaultTheme::applyRaisedCard(m_card, 12);
    lv_obj_set_style_pad_all(m_card, isCompact ? 8 : 14, 0);
    lv_obj_set_flex_flow(m_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_card, isCompact ? 6 : 8, 0);
    lv_obj_add_flag(m_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(m_card, LV_DIR_VER);

    // Cabecera: Título + Botón Cerrar
    lv_obj_t* header = lv_obj_create(m_card);
    lv_obj_set_size(header, LV_PCT(100), 30);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_WIFI " Conexión SSH");
    lv_obj_set_style_text_font(title, isCompact ? &lv_font_montserrat_14 : &lv_font_montserrat_16, 0);

    lv_obj_t* btnClose = lv_button_create(header);
    DefaultTheme::applyButton(btnClose, 6);
    lv_obj_set_size(btnClose, 30, 26);
    lv_obj_t* lblClose = lv_label_create(btnClose);
    lv_label_set_text(lblClose, LV_SYMBOL_CLOSE);
    lv_obj_center(lblClose);
    lv_obj_add_event_cb(btnClose, closeBtnCb, LV_EVENT_CLICKED, this);

    // Fila 1: Perfiles Guardados en SD
    lv_obj_t* rowProf = lv_obj_create(m_card);
    lv_obj_set_size(rowProf, LV_PCT(100), 36);
    lv_obj_set_style_bg_opa(rowProf, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowProf, 0, 0);
    lv_obj_set_style_pad_all(rowProf, 0, 0);
    lv_obj_set_flex_flow(rowProf, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rowProf, 6, 0);

    m_ddProfile = lv_dropdown_create(rowProf);
    lv_obj_set_flex_grow(m_ddProfile, 1);
    lv_obj_set_style_text_font(m_ddProfile, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddProfile, profileChangedCb, LV_EVENT_VALUE_CHANGED, this);

    m_btnSaveProfile = lv_button_create(rowProf);
    DefaultTheme::applyButton(m_btnSaveProfile, 6);
    lv_obj_set_size(m_btnSaveProfile, 40, 34);
    lv_obj_set_style_bg_color(m_btnSaveProfile, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_t* lblSave = lv_label_create(m_btnSaveProfile);
    lv_label_set_text(lblSave, LV_SYMBOL_SAVE);
    lv_obj_center(lblSave);
    lv_obj_add_event_cb(m_btnSaveProfile, saveProfileBtnCb, LV_EVENT_CLICKED, this);

    // Fila 2: Servidor / IP (100% de ancho para ver la IP completa sin scroll)
    lv_obj_t* lblHost = lv_label_create(m_card);
    lv_label_set_text(lblHost, "Servidor / IP:");
    lv_obj_set_style_text_font(lblHost, &lv_font_montserrat_12, 0);

    m_taHost = lv_textarea_create(m_card);
    lv_obj_set_size(m_taHost, LV_PCT(100), isCompact ? 40 : 44);
    DefaultTheme::applyTextArea(m_taHost, 6);
    lv_textarea_set_one_line(m_taHost, true);
    lv_obj_set_scrollbar_mode(m_taHost, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_placeholder_text(m_taHost, "192.168.1.50");
    lv_obj_set_style_text_font(m_taHost, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(m_taHost, 8, 0);
    lv_obj_set_style_pad_bottom(m_taHost, 8, 0);
    lv_obj_set_style_pad_left(m_taHost, 10, 0);
    lv_obj_set_style_pad_right(m_taHost, 10, 0);
    UIManager::attachKeyboard(m_taHost, { .nextFocusTarget = m_taPort });

    // Fila 3: Puerto y Usuario
    lv_obj_t* rowPortUser = lv_obj_create(m_card);
    lv_obj_set_size(rowPortUser, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(rowPortUser, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowPortUser, 0, 0);
    lv_obj_set_style_pad_all(rowPortUser, 0, 0);
    lv_obj_set_flex_flow(rowPortUser, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rowPortUser, 6, 0);

    // Box Puerto
    lv_obj_t* boxPort = lv_obj_create(rowPortUser);
    lv_obj_set_size(boxPort, isCompact ? 75 : 90, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(boxPort, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(boxPort, 0, 0);
    lv_obj_set_style_pad_all(boxPort, 0, 0);
    lv_obj_set_flex_flow(boxPort, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(boxPort, 2, 0);

    lv_obj_t* lblPort = lv_label_create(boxPort);
    lv_label_set_text(lblPort, "Puerto:");
    lv_obj_set_style_text_font(lblPort, &lv_font_montserrat_12, 0);

    m_taPort = lv_textarea_create(boxPort);
    lv_obj_set_size(m_taPort, LV_PCT(100), isCompact ? 40 : 44);
    DefaultTheme::applyTextArea(m_taPort, 6);
    lv_textarea_set_one_line(m_taPort, true);
    lv_obj_set_scrollbar_mode(m_taPort, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_text(m_taPort, "22");
    lv_obj_set_style_text_font(m_taPort, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(m_taPort, 8, 0);
    lv_obj_set_style_pad_bottom(m_taPort, 8, 0);
    lv_obj_set_style_pad_left(m_taPort, 8, 0);
    lv_obj_set_style_pad_right(m_taPort, 8, 0);
    UIManager::attachKeyboard(m_taPort, { .nextFocusTarget = m_taUser });

    // Box Usuario
    lv_obj_t* boxUser = lv_obj_create(rowPortUser);
    lv_obj_set_size(boxUser, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(boxUser, 1);
    lv_obj_set_style_bg_opa(boxUser, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(boxUser, 0, 0);
    lv_obj_set_style_pad_all(boxUser, 0, 0);
    lv_obj_set_flex_flow(boxUser, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(boxUser, 2, 0);

    lv_obj_t* lblUser = lv_label_create(boxUser);
    lv_label_set_text(lblUser, "Usuario:");
    lv_obj_set_style_text_font(lblUser, &lv_font_montserrat_12, 0);

    m_taUser = lv_textarea_create(boxUser);
    lv_obj_set_size(m_taUser, LV_PCT(100), isCompact ? 40 : 44);
    DefaultTheme::applyTextArea(m_taUser, 6);
    lv_textarea_set_one_line(m_taUser, true);
    lv_obj_set_scrollbar_mode(m_taUser, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_placeholder_text(m_taUser, "root");
    lv_obj_set_style_text_font(m_taUser, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(m_taUser, 8, 0);
    lv_obj_set_style_pad_bottom(m_taUser, 8, 0);
    lv_obj_set_style_pad_left(m_taUser, 10, 0);
    lv_obj_set_style_pad_right(m_taUser, 10, 0);
    UIManager::attachKeyboard(m_taUser, { .nextFocusTarget = m_taPassword });

    // Fila 4: Tipo de Autenticación
    lv_obj_t* lblAuth = lv_label_create(m_card);
    lv_label_set_text(lblAuth, "Autenticación:");
    lv_obj_set_style_text_font(lblAuth, &lv_font_montserrat_12, 0);

    m_ddAuthType = lv_dropdown_create(m_card);
    lv_dropdown_set_options(m_ddAuthType, "Contraseña\nClave Privada / Certificado");
    lv_obj_set_size(m_ddAuthType, LV_PCT(100), 38);
    lv_obj_set_style_text_font(m_ddAuthType, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(m_ddAuthType, authTypeChangedCb, LV_EVENT_VALUE_CHANGED, this);

    // Fila 5a: Modo Contraseña
    m_boxPassword = lv_obj_create(m_card);
    lv_obj_set_size(m_boxPassword, LV_PCT(100), isCompact ? 42 : 46);
    lv_obj_set_style_bg_opa(m_boxPassword, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_boxPassword, 0, 0);
    lv_obj_set_style_pad_all(m_boxPassword, 0, 0);
    lv_obj_set_flex_flow(m_boxPassword, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(m_boxPassword, 6, 0);

    m_taPassword = lv_textarea_create(m_boxPassword);
    lv_obj_set_flex_grow(m_taPassword, 1);
    lv_obj_set_height(m_taPassword, isCompact ? 40 : 44);
    DefaultTheme::applyTextArea(m_taPassword, 6);
    lv_textarea_set_one_line(m_taPassword, true);
    lv_textarea_set_password_mode(m_taPassword, true);
    lv_obj_set_scrollbar_mode(m_taPassword, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_placeholder_text(m_taPassword, "Contraseña...");
    lv_obj_set_style_text_font(m_taPassword, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(m_taPassword, 8, 0);
    lv_obj_set_style_pad_bottom(m_taPassword, 8, 0);
    lv_obj_set_style_pad_left(m_taPassword, 10, 0);
    lv_obj_set_style_pad_right(m_taPassword, 10, 0);
    UIManager::attachKeyboard(m_taPassword, { .autoCloseOnSubmit = true });

    m_btnPassVis = lv_button_create(m_boxPassword);
    DefaultTheme::applyButton(m_btnPassVis, 6);
    lv_obj_set_size(m_btnPassVis, 44, isCompact ? 40 : 44);
    lv_obj_set_style_bg_color(m_btnPassVis, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t* lblEye = lv_label_create(m_btnPassVis);
    lv_label_set_text(lblEye, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(lblEye);
    lv_obj_add_event_cb(m_btnPassVis, togglePassVisCb, LV_EVENT_CLICKED, this);

    // Fila 5b: Modo Clave Privada / Certificado en SD
    m_boxKey = lv_obj_create(m_card);
    lv_obj_set_size(m_boxKey, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(m_boxKey, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_boxKey, 0, 0);
    lv_obj_set_style_pad_all(m_boxKey, 0, 0);
    lv_obj_set_flex_flow(m_boxKey, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(m_boxKey, 4, 0);
    lv_obj_add_flag(m_boxKey, LV_OBJ_FLAG_HIDDEN);

    m_taKeyPath = lv_textarea_create(m_boxKey);
    lv_obj_set_size(m_taKeyPath, LV_PCT(100), isCompact ? 40 : 44);
    DefaultTheme::applyTextArea(m_taKeyPath, 6);
    lv_textarea_set_one_line(m_taKeyPath, true);
    lv_obj_set_scrollbar_mode(m_taKeyPath, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_placeholder_text(m_taKeyPath, "/sdcard/keys/id_ed25519");
    lv_textarea_set_text(m_taKeyPath, "/sdcard/keys/id_ed25519");
    lv_obj_set_style_text_font(m_taKeyPath, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(m_taKeyPath, 8, 0);
    lv_obj_set_style_pad_bottom(m_taKeyPath, 8, 0);
    lv_obj_set_style_pad_left(m_taKeyPath, 10, 0);
    lv_obj_set_style_pad_right(m_taKeyPath, 10, 0);
    UIManager::attachKeyboard(m_taKeyPath, { .nextFocusTarget = m_taPassphrase });

    m_taPassphrase = lv_textarea_create(m_boxKey);
    lv_obj_set_size(m_taPassphrase, LV_PCT(100), isCompact ? 40 : 44);
    DefaultTheme::applyTextArea(m_taPassphrase, 6);
    lv_textarea_set_one_line(m_taPassphrase, true);
    lv_textarea_set_password_mode(m_taPassphrase, true);
    lv_obj_set_scrollbar_mode(m_taPassphrase, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_placeholder_text(m_taPassphrase, "Passphrase de clave (opcional)...");
    lv_obj_set_style_text_font(m_taPassphrase, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(m_taPassphrase, 8, 0);
    lv_obj_set_style_pad_bottom(m_taPassphrase, 8, 0);
    lv_obj_set_style_pad_left(m_taPassphrase, 10, 0);
    lv_obj_set_style_pad_right(m_taPassphrase, 10, 0);
    UIManager::attachKeyboard(m_taPassphrase, { .autoCloseOnSubmit = true });

    // Emulación Terminal PTY
    lv_obj_t* lblTerm = lv_label_create(m_card);
    lv_label_set_text(lblTerm, "Emulación de Terminal:");
    lv_obj_set_style_text_font(lblTerm, &lv_font_montserrat_12, 0);

    m_ddTerm = lv_dropdown_create(m_card);
    lv_dropdown_set_options(m_ddTerm, "vt100 (Universal / Routers)\nxterm (Linux / Servidores)");
    lv_obj_set_size(m_ddTerm, LV_PCT(100), 38);
    lv_obj_set_style_text_font(m_ddTerm, &lv_font_montserrat_12, 0);
    lv_dropdown_set_selected(m_ddTerm, 0);

    // Estado interactivo
    m_lblStatus = lv_label_create(m_card);
    lv_label_set_text(m_lblStatus, "Listo para conectar.");
    lv_obj_set_style_text_font(m_lblStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblStatus, lv_palette_main(LV_PALETTE_AMBER), 0);

    // Botones de Acción
    lv_obj_t* btnBar = lv_obj_create(m_card);
    lv_obj_set_size(btnBar, LV_PCT(100), 40);
    lv_obj_set_style_bg_opa(btnBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnBar, 0, 0);
    lv_obj_set_style_pad_all(btnBar, 0, 0);
    lv_obj_set_flex_flow(btnBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnCancel = lv_button_create(btnBar);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_set_size(btnCancel, isCompact ? 85 : 110, 36);
    lv_obj_set_style_bg_color(btnCancel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_t* lblC = lv_label_create(btnCancel);
    lv_label_set_text(lblC, "Cancelar");
    lv_obj_center(lblC);
    lv_obj_add_event_cb(btnCancel, closeBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnConnect = lv_button_create(btnBar);
    DefaultTheme::applyButton(btnConnect, 8);
    lv_obj_set_size(btnConnect, isCompact ? 150 : 180, 36);
    lv_obj_set_style_bg_color(btnConnect, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_t* lblConn = lv_label_create(btnConnect);
    lv_label_set_text(lblConn, LV_SYMBOL_PLAY " Conectar");
    lv_obj_center(lblConn);
    lv_obj_add_event_cb(btnConnect, connectBtnCb, LV_EVENT_CLICKED, this);

    updateProfileDropdown();
    applySelectedProfile(0);
}

void SshConnectModal::close() {
    UIManager::closeKeyboard();
    if (m_modalMask) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
        m_card = nullptr;
    }
}

void SshConnectModal::setStatus(const std::string& msg, bool isError) {
    if (!m_lblStatus) return;
    lv_label_set_text(m_lblStatus, msg.c_str());
    if (isError) {
        lv_obj_set_style_text_color(m_lblStatus, lv_palette_main(LV_PALETTE_RED), 0);
    } else {
        lv_obj_set_style_text_color(m_lblStatus, lv_palette_main(LV_PALETTE_GREEN), 0);
    }
}

void SshConnectModal::profileChangedCb(lv_event_t* e) {
    auto* self = static_cast<SshConnectModal*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddProfile) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddProfile);
    self->applySelectedProfile(sel);
}

void SshConnectModal::authTypeChangedCb(lv_event_t* e) {
    auto* self = static_cast<SshConnectModal*>(lv_event_get_user_data(e));
    if (!self || !self->m_ddAuthType) return;
    uint32_t sel = lv_dropdown_get_selected(self->m_ddAuthType);
    if (sel == 0) { // Contraseña
        lv_obj_remove_flag(self->m_boxPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(self->m_boxKey, LV_OBJ_FLAG_HIDDEN);
    } else { // Clave Privada
        lv_obj_add_flag(self->m_boxPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(self->m_boxKey, LV_OBJ_FLAG_HIDDEN);
    }
}

void SshConnectModal::saveProfileBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshConnectModal*>(lv_event_get_user_data(e));
    if (self) self->saveCurrentProfile();
}

void SshConnectModal::togglePassVisCb(lv_event_t* e) {
    auto* self = static_cast<SshConnectModal*>(lv_event_get_user_data(e));
    if (!self || !self->m_taPassword || !self->m_btnPassVis) return;
    self->m_passVisible = !self->m_passVisible;
    lv_textarea_set_password_mode(self->m_taPassword, !self->m_passVisible);
    lv_obj_t* lbl = lv_obj_get_child(self->m_btnPassVis, 0);
    if (lbl) {
        lv_label_set_text(lbl, self->m_passVisible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    }
}

void SshConnectModal::closeBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshConnectModal*>(lv_event_get_user_data(e));
    if (self) self->close();
}

void SshConnectModal::connectBtnCb(lv_event_t* e) {
    auto* self = static_cast<SshConnectModal*>(lv_event_get_user_data(e));
    if (!self) return;

    if (!cbdos::network::isConnected()) {
        self->setStatus("Error: Wi-Fi desconectado.", true);
        return;
    }

    cbdos::ssh::SshConfig cfg;
    cfg.host = lv_textarea_get_text(self->m_taHost);
    cfg.username = lv_textarea_get_text(self->m_taUser);
    const char* portStr = lv_textarea_get_text(self->m_taPort);
    cfg.port = portStr ? (uint16_t)atoi(portStr) : 22;

    uint32_t authSel = lv_dropdown_get_selected(self->m_ddAuthType);
    if (authSel == 0) {
        cfg.authType = cbdos::ssh::SshAuthType::Password;
        cfg.password = lv_textarea_get_text(self->m_taPassword);
    } else {
        cfg.authType = cbdos::ssh::SshAuthType::PublicKey;
        cfg.privateKeyPath = lv_textarea_get_text(self->m_taKeyPath);
        cfg.passphrase = lv_textarea_get_text(self->m_taPassphrase);
    }

    if (self->m_ddTerm) {
        uint32_t termSel = lv_dropdown_get_selected(self->m_ddTerm);
        cfg.termType = (termSel == 1) ? "xterm" : "vt100";
    }

    if (cfg.host.empty() || cfg.username.empty()) {
        self->setStatus("Error: Host y Usuario son requeridos.", true);
        return;
    }

    self->setStatus("Conectando sesión SSH...", false);
    if (self->m_onConnect) {
        self->m_onConnect(cfg);
    }
    self->close();
}

} // namespace ui
} // namespace cbdos
