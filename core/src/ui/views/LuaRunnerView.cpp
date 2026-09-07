#include "LuaRunnerView.hpp"
#include "TextEditorView.hpp"
#include "../../lua/LuaRunner.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/display.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace cbdos {
namespace ui {

LuaRunnerView::LuaRunnerView(const std::string& initialScript)
    : BaseView("Lua Runner"),
      m_logContainer(nullptr),
      m_statusBadge(nullptr),
      m_scriptLabel(nullptr),
      m_refreshTimer(nullptr),
      m_modalMask(nullptr),
      m_modalTitle(nullptr),
      m_modalBody(nullptr),
      m_activeScript(initialScript),
      m_currentFolderIdx(0) {
}

void LuaRunnerView::setScript(const std::string& path) {
    m_activeScript = path;
    if (m_scriptLabel && lv_obj_is_valid(m_scriptLabel)) {
        lv_label_set_text(m_scriptLabel, m_activeScript.empty() ? "(Sin script seleccionado)" : m_activeScript.c_str());
    }
}

void LuaRunnerView::updateStatusBadge() {
    if (!m_statusBadge || !lv_obj_is_valid(m_statusBadge)) return;

    LuaRunnerState state = LuaRunner::getInstance().getState();
    const char* text = LuaRunner::getInstance().getStateString();
    uint32_t color = 0x90A4AE; // Gris

    if (state == LuaRunnerState::RUNNING) {
        color = 0x00E676; // Verde
    } else if (state == LuaRunnerState::FINISHED) {
        color = 0x40C4FF; // Azul
    } else if (state == LuaRunnerState::ERROR) {
        color = 0xFF5252; // Rojo
    } else if (state == LuaRunnerState::STOPPED) {
        color = 0xFFB74D; // Ámbar
    }

    lv_label_set_text(m_statusBadge, text);
    lv_obj_set_style_text_color(m_statusBadge, lv_color_hex(color), 0);
}

void LuaRunnerView::appendLogLine(const std::string& line) {
    if (!m_logContainer || !lv_obj_is_valid(m_logContainer)) return;

    // Limitar cantidad máxima de líneas en el contenedor para fluidez gráfica
    uint32_t childCount = lv_obj_get_child_count(m_logContainer);
    if (childCount > 150) {
        lv_obj_t* firstChild = lv_obj_get_child(m_logContainer, 0);
        if (firstChild) {
            lv_obj_delete(firstChild);
        }
    }

    lv_obj_t* lbl = lv_label_create(m_logContainer);
    lv_label_set_text(lbl, line.c_str());
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    // Colorear según el tipo de log
    if (line.rfind("[Error]", 0) == 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF5252), 0);
    } else if (line.rfind("[Sistema]", 0) == 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x40C4FF), 0);
    } else if (line.rfind("[OK]", 0) == 0) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x00E676), 0);
    } else {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE0E6ED), 0);
    }

    // Auto-scroll al final
    lv_obj_scroll_to_y(m_logContainer, LV_COORD_MAX, LV_ANIM_OFF);
}

void LuaRunnerView::timerCb(lv_timer_t* timer) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_timer_get_user_data(timer));
    if (!self) return;

    std::vector<std::string> logs;
    if (LuaRunner::getInstance().drainLogs(logs)) {
        for (const auto& l : logs) {
            self->appendLogLine(l);
        }
    }
    self->updateStatusBadge();
}

void LuaRunnerView::scanLuaFilesSD() {
    m_folderCategories = {
        {"Scripts", "/sdcard/scripts", {}},
        {"Apps", "/sdcard/apps", {}},
        {"Paquetes Luapp", "/sdcard/luapp", {}},
        {"Raíz MicroSD", "/sdcard", {}}
    };

    if (!cbdos::storage::isSdMounted()) {
        cbdos::storage::mountSd();
    }

    if (!cbdos::storage::isSdMounted()) {
        return;
    }

    for (auto& cat : m_folderCategories) {
        auto entries = cbdos::storage::listDir(cat.path.c_str());
        for (const auto& f : entries) {
            if (f.isDirectory) continue;

            std::string nameLower = f.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            bool isLua = (nameLower.size() >= 4 && nameLower.rfind(".lua") == nameLower.size() - 4);
            bool isLuapp = (nameLower.size() >= 6 && nameLower.rfind(".luapp") == nameLower.size() - 6);

            if (isLua || isLuapp) {
                std::string fullPath = cat.path;
                if (fullPath.back() != '/') fullPath += '/';
                fullPath += f.name;
                cat.files.push_back(fullPath);
            }
        }
    }
}

void LuaRunnerView::createDemoScripts() {
    if (!cbdos::storage::isSdMounted()) {
        cbdos::storage::mountSd();
    }

    const char* script1 = 
        "-- ==========================================\n"
        "-- CBDos - Script de Diagnostico del Sistema\n"
        "-- ==========================================\n"
        "print('=== CBDos Lua 5.4 - Diagnostico ===')\n"
        "print('Tiempo activo : ' .. cbdos.millis() .. ' ms')\n"
        "print('PSRAM libre   : ' .. cbdos.free_psram() .. ' bytes')\n"
        "print('SRAM libre    : ' .. cbdos.free_heap() .. ' bytes')\n"
        "print('WiFi conectado: ' .. tostring(cbdos.wifi_status()))\n"
        "print('IP asignada   : ' .. cbdos.get_ip())\n"
        "print('------------------------------------')\n"
        "for i = 1, 5 do\n"
        "    print('Contador: ' .. i .. ' / 5 | Tick: ' .. cbdos.millis() .. ' ms')\n"
        "    cbdos.beep(600 + (i * 100), 80)\n"
        "    cbdos.delay(500)\n"
        "end\n"
        "print('=== Test Completado con Exito ===')\n";

    const char* script2 = 
        "-- ==========================================\n"
        "-- CBDos - Super Mario Bros Theme\n"
        "-- ==========================================\n"
        "print('=== Super Mario Bros Theme (CBDos Lua) ===')\n"
        "cbdos.set_volume(85)\n"
        "print('Volumen: ' .. cbdos.get_volume() .. '%')\n"
        "local notas = {\n"
        "    { n = 'Do5',  f = 523.25, d = 180 },\n"
        "    { n = 'Sol4', f = 392.00, d = 180 },\n"
        "    { n = 'Mi4',  f = 329.63, d = 180 },\n"
        "    { n = 'La4',  f = 440.00, d = 160 },\n"
        "    { n = 'Si4',  f = 493.88, d = 160 },\n"
        "    { n = 'La#4', f = 466.16, d = 140 },\n"
        "    { n = 'La4',  f = 440.00, d = 180 },\n"
        "    { n = 'Sol4', f = 392.00, d = 150 },\n"
        "    { n = 'Mi5',  f = 659.25, d = 150 },\n"
        "    { n = 'Sol5', f = 783.99, d = 150 },\n"
        "    { n = 'La5',  f = 880.00, d = 180 },\n"
        "    { n = 'Fa5',  f = 698.46, d = 150 },\n"
        "    { n = 'Sol5', f = 783.99, d = 150 },\n"
        "    { n = 'Mi5',  f = 659.25, d = 180 },\n"
        "    { n = 'Do5',  f = 523.25, d = 150 },\n"
        "    { n = 'Re5',  f = 587.33, d = 150 },\n"
        "    { n = 'Si4',  f = 493.88, d = 220 }\n"
        "}\n"
        "for i, item in ipairs(notas) do\n"
        "    print('Nota [' .. i .. '/' .. #notas .. ']: ' .. item.n .. ' (' .. item.f .. ' Hz)')\n"
        "    cbdos.beep(item.f, item.d)\n"
        "    cbdos.delay(item.d + 40)\n"
        "end\n"
        "print('=== Melodia de Mario Finalizada ===')\n";

    FILE* f1 = fopen("/sdcard/demo_sistema.lua", "w");
    if (f1) {
        fputs(script1, f1);
        fclose(f1);
    }

    FILE* f2 = fopen("/sdcard/mario_theme.lua", "w");
    if (f2) {
        fputs(script2, f2);
        fclose(f2);
    }

    const char* script3 = 
        "-- ===============================================\n"
        "-- CBDos - Test Completo de Graficos y Touch Lua\n"
        "-- ===============================================\n"
        "print('=== CBDos Lua - Canvas Grafico 2D ===')\n"
        "print('Dimensiones pantalla: ' .. cbdos.gfx.width() .. ' x ' .. cbdos.gfx.height())\n"
        "cbdos.gfx.pause_ui(30)\n"
        "local W = cbdos.gfx.width()\n"
        "local H = cbdos.gfx.height()\n"
        "local C_FONDO = cbdos.gfx.rgb(10, 15, 26)\n"
        "local C_AZUL  = cbdos.gfx.rgb(33, 150, 243)\n"
        "local C_CIAN  = cbdos.gfx.rgb(0, 229, 255)\n"
        "local C_VERDE = cbdos.gfx.rgb(0, 230, 118)\n"
        "local C_AMARILLO = cbdos.gfx.rgb(255, 214, 0)\n"
        "local C_ROJO  = cbdos.gfx.rgb(255, 82, 82)\n"
        "local C_BLANCO= cbdos.gfx.rgb(255, 255, 255)\n"
        "local C_GRIS  = cbdos.gfx.rgb(60, 70, 85)\n"
        "cbdos.gfx.clear(C_FONDO)\n"
        "cbdos.gfx.draw_rect(0, 0, W, 45, C_AZUL, true)\n"
        "cbdos.gfx.draw_text(15, 12, 'CBDos Graphics Engine', C_BLANCO, 2)\n"
        "cbdos.gfx.draw_rect(10, 55, W - 20, 115, C_GRIS, false)\n"
        "cbdos.gfx.draw_text(20, 65, 'Primitivas 2D en Lua:', C_AMARILLO, 1)\n"
        "cbdos.gfx.draw_rect(20, 85, 45, 35, C_VERDE, true)\n"
        "cbdos.gfx.draw_rect(75, 85, 45, 35, C_CIAN, false)\n"
        "cbdos.gfx.draw_circle(155, 102, 18, C_ROJO, true)\n"
        "cbdos.gfx.draw_circle(155, 102, 9, C_AMARILLO, true)\n"
        "cbdos.gfx.draw_circle(195, 102, 18, C_CIAN, false)\n"
        "cbdos.gfx.draw_line(230, 85, 280, 120, C_BLANCO)\n"
        "cbdos.gfx.draw_line(230, 120, 280, 85, C_AMARILLO)\n"
        "cbdos.gfx.draw_rect(10, 180, W - 20, H - 240, C_CIAN, false)\n"
        "cbdos.gfx.draw_text(20, 190, 'Lienzo Tactil (Dibuja aqui):', C_BLANCO, 1)\n"
        "cbdos.gfx.draw_rect(10, H - 55, W - 20, 45, C_ROJO, true)\n"
        "cbdos.gfx.draw_text((W / 2) - 40, H - 42, '[ SALIR ]', C_BLANCO, 2)\n"
        "cbdos.beep(1000, 60)\n"
        "print('Canvas listo. Dibuja en pantalla o toca [SALIR].')\n"
        "local contadorPuntos = 0\n"
        "local activo = true\n"
        "while activo do\n"
        "    local t = cbdos.gfx.touch()\n"
        "    if t.touched then\n"
        "        if t.y >= (H - 55) and t.y <= H and t.x >= 10 and t.x <= (W - 10) then\n"
        "            cbdos.beep(600, 100)\n"
        "            activo = false\n"
        "        elseif t.y >= 180 and t.y <= (H - 65) and t.x >= 10 and t.x <= (W - 10) then\n"
        "            contadorPuntos = contadorPuntos + 1\n"
        "            local r = math.floor((t.x / W) * 255)\n"
        "            local g = math.floor((t.y / H) * 255)\n"
        "            local col = cbdos.gfx.rgb(r, g, 220)\n"
        "            cbdos.gfx.draw_circle(t.x, t.y, 6, col, true)\n"
        "            if contadorPuntos % 15 == 0 then cbdos.beep(1200, 10) end\n"
        "        end\n"
        "    end\n"
        "    cbdos.delay(10)\n"
        "end\n"
        "cbdos.gfx.clear(C_FONDO)\n"
        "cbdos.gfx.draw_text(70, H / 2, 'Regresando a CBDos...', C_VERDE, 2)\n"
        "cbdos.delay(400)\n"
        "cbdos.gfx.resume_ui()\n"
        "print('Test de graficos completado.')\n";

    FILE* f3 = fopen("/sdcard/demo_graficos.lua", "w");
    if (f3) {
        fputs(script3, f3);
        fclose(f3);
    }
}

void LuaRunnerView::btnCreateDemoCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->createDemoScripts();
    UIManager::showToast("Scripts demo creados en SD");

    self->scanLuaFilesSD();
    self->renderFolderListView();
}

void LuaRunnerView::btnRunCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_activeScript.empty()) {
        UIManager::showToast("Selecciona un script primero (📁)");
        return;
    }

    if (LuaRunner::getInstance().getState() == LuaRunnerState::RUNNING) {
        UIManager::showToast("El script ya esta corriendo");
        return;
    }

    LuaRunner::getInstance().startScript(self->m_activeScript);
    self->updateStatusBadge();
}

void LuaRunnerView::btnStopCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (LuaRunner::getInstance().getState() == LuaRunnerState::RUNNING) {
        LuaRunner::getInstance().stop();
        UIManager::showToast("Deteniendo script...");
    } else {
        UIManager::showToast("No hay script corriendo");
    }
    self->updateStatusBadge();
}

void LuaRunnerView::btnEditCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_activeScript.empty()) {
        UIManager::getInstance().pushView(std::make_shared<TextEditorView>());
    } else {
        UIManager::getInstance().pushView(std::make_shared<TextEditorView>(self->m_activeScript));
    }
}

void LuaRunnerView::btnClearCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->m_logContainer && lv_obj_is_valid(self->m_logContainer)) {
        lv_obj_clean(self->m_logContainer);
    }
    LuaRunner::getInstance().clearLogs();
}

void LuaRunnerView::btnSdCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (self) {
        self->showFilePickerModal();
    }
}

void LuaRunnerView::modalCloseCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (self && self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
        self->m_modalTitle = nullptr;
        self->m_modalBody = nullptr;
    }
}

void LuaRunnerView::folderSelectCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!self || !btn) return;

    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    self->renderFileListView(idx);
}

void LuaRunnerView::folderBackCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    if (!self) return;

    self->renderFolderListView();
}

void LuaRunnerView::fileSelectCb(lv_event_t* e) {
    LuaRunnerView* self = static_cast<LuaRunnerView*>(lv_event_get_user_data(e));
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!self || !btn) return;

    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(btn);
    if (self->m_currentFolderIdx < self->m_folderCategories.size()) {
        const auto& files = self->m_folderCategories[self->m_currentFolderIdx].files;
        if (idx < files.size()) {
            self->setScript(files[idx]);
            UIManager::showToast("Script seleccionado");
        }
    }

    if (self->m_modalMask && lv_obj_is_valid(self->m_modalMask)) {
        lv_obj_delete_async(self->m_modalMask);
        self->m_modalMask = nullptr;
        self->m_modalTitle = nullptr;
        self->m_modalBody = nullptr;
    }
}

void LuaRunnerView::showFilePickerModal() {
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
        m_modalTitle = nullptr;
        m_modalBody = nullptr;
    }

    scanLuaFilesSD();

    auto caps = cbdos::display::getCapabilities();
    int32_t screenW = caps.width > 0 ? caps.width : 480;
    int32_t screenH = caps.height > 0 ? caps.height : 800;

    m_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(m_modalMask, screenW, screenH);
    lv_obj_set_pos(m_modalMask, 0, 0);
    lv_obj_set_style_bg_color(m_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(m_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(m_modalMask, 0, 0);
    lv_obj_set_style_pad_all(m_modalMask, 0, 0);

    int32_t modalW = (screenW >= 480) ? 400 : 300;
    int32_t modalH = (screenH >= 800) ? 520 : 400;

    lv_obj_t* modal = lv_obj_create(m_modalMask);
    lv_obj_set_size(modal, modalW, modalH);
    DefaultTheme::applyRaisedCard(modal, 16);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 12, 0);
    lv_obj_set_style_pad_row(modal, 8, 0);

    // Cabecera Modal
    lv_obj_t* header = lv_obj_create(modal);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(header, 0, 0);
    DefaultTheme::disableScroll(header);

    m_modalTitle = lv_label_create(header);
    lv_label_set_text(m_modalTitle, "Explorador de Scripts");
    lv_obj_set_style_text_color(m_modalTitle, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_modalTitle, &lv_font_montserrat_14, 0);

    lv_obj_t* btnClose = lv_button_create(header);
    lv_obj_set_size(btnClose, 30, 30);
    DefaultTheme::applyButton(btnClose, 15);
    lv_obj_t* lblX = lv_label_create(btnClose);
    lv_label_set_text(lblX, LV_SYMBOL_CLOSE);
    lv_obj_center(lblX);
    lv_obj_add_event_cb(btnClose, modalCloseCb, LV_EVENT_CLICKED, this);

    // Cuerpo con scroll
    m_modalBody = lv_obj_create(modal);
    lv_obj_set_size(m_modalBody, LV_PCT(100), LV_PCT(82));
    DefaultTheme::applySunkenCard(m_modalBody, 10);
    lv_obj_set_flex_flow(m_modalBody, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_modalBody, 8, 0);
    lv_obj_set_style_pad_row(m_modalBody, 6, 0);
    lv_obj_set_scroll_dir(m_modalBody, LV_DIR_VER);

    renderFolderListView();
}

void LuaRunnerView::renderFolderListView() {
    if (!m_modalBody || !lv_obj_is_valid(m_modalBody)) return;
    lv_obj_clean(m_modalBody);

    if (m_modalTitle && lv_obj_is_valid(m_modalTitle)) {
        lv_label_set_text(m_modalTitle, "Explorador de Scripts");
    }

    size_t totalScripts = 0;
    for (const auto& cat : m_folderCategories) {
        totalScripts += cat.files.size();
    }

    lv_obj_t* hint = lv_label_create(m_modalBody);
    lv_label_set_text(hint, "Selecciona una carpeta para explorar:");
    lv_obj_set_style_text_color(hint, DefaultTheme::getMutedTextColor(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_margin_ver(hint, 2, 0);

    for (size_t i = 0; i < m_folderCategories.size(); i++) {
        const auto& cat = m_folderCategories[i];

        lv_obj_t* item = lv_button_create(m_modalBody);
        lv_obj_set_size(item, LV_PCT(100), 50);
        DefaultTheme::applyButton(item, 8);
        lv_obj_set_user_data(item, (void*)(intptr_t)i);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(item, 10, 0);
        lv_obj_set_style_pad_ver(item, 4, 0);
        lv_obj_add_event_cb(item, folderSelectCb, LV_EVENT_CLICKED, this);

        // Lado izquierdo: Icono + Título + Ruta
        lv_obj_t* leftBox = lv_obj_create(item);
        lv_obj_set_size(leftBox, LV_PCT(70), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(leftBox, 0, 0);
        lv_obj_set_style_border_width(leftBox, 0, 0);
        lv_obj_set_style_pad_all(leftBox, 0, 0);
        lv_obj_set_flex_flow(leftBox, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(leftBox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        DefaultTheme::disableScroll(leftBox);

        lv_obj_t* icon = lv_label_create(leftBox);
        lv_label_set_text(icon, LV_SYMBOL_DIRECTORY);
        lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

        lv_obj_t* textBox = lv_obj_create(leftBox);
        lv_obj_set_size(textBox, LV_PCT(80), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(textBox, 0, 0);
        lv_obj_set_style_border_width(textBox, 0, 0);
        lv_obj_set_style_pad_all(textBox, 0, 0);
        lv_obj_set_style_margin_left(textBox, 6, 0);
        lv_obj_set_flex_flow(textBox, LV_FLEX_FLOW_COLUMN);
        DefaultTheme::disableScroll(textBox);

        lv_obj_t* lblTitle = lv_label_create(textBox);
        lv_label_set_text(lblTitle, cat.title.c_str());
        lv_obj_set_style_text_color(lblTitle, DefaultTheme::getTextColor(), 0);
        lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);

        lv_obj_t* lblPath = lv_label_create(textBox);
        lv_label_set_text(lblPath, cat.path.c_str());
        lv_obj_set_style_text_color(lblPath, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblPath, &lv_font_montserrat_12, 0);

        // Lado derecho: Badge contador [ N ] + Flecha
        lv_obj_t* rightBox = lv_obj_create(item);
        lv_obj_set_size(rightBox, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(rightBox, 0, 0);
        lv_obj_set_style_border_width(rightBox, 0, 0);
        lv_obj_set_style_pad_all(rightBox, 0, 0);
        lv_obj_set_flex_flow(rightBox, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(rightBox, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        DefaultTheme::disableScroll(rightBox);

        char badgeBuf[16];
        snprintf(badgeBuf, sizeof(badgeBuf), "[ %d ]", (int)cat.files.size());
        lv_obj_t* lblBadge = lv_label_create(rightBox);
        lv_label_set_text(lblBadge, badgeBuf);
        uint32_t badgeColor = cat.files.empty() ? 0x90A4AE : 0x00E676;
        lv_obj_set_style_text_color(lblBadge, lv_color_hex(badgeColor), 0);
        lv_obj_set_style_text_font(lblBadge, &lv_font_montserrat_12, 0);

        lv_obj_t* arrow = lv_label_create(rightBox);
        lv_label_set_text(arrow, " " LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, DefaultTheme::getMutedTextColor(), 0);
    }

    if (totalScripts == 0) {
        lv_obj_t* btnDemo = lv_button_create(m_modalBody);
        lv_obj_set_size(btnDemo, LV_PCT(100), 42);
        DefaultTheme::applyButton(btnDemo, 8);
        lv_obj_set_style_margin_top(btnDemo, 12, 0);
        lv_obj_add_event_cb(btnDemo, btnCreateDemoCb, LV_EVENT_CLICKED, this);

        lv_obj_t* lblDemo = lv_label_create(btnDemo);
        lv_label_set_text(lblDemo, LV_SYMBOL_PLUS " Generar Scripts Demo");
        lv_obj_set_style_text_color(lblDemo, DefaultTheme::getPrimaryAccent(), 0);
        lv_obj_center(lblDemo);
    }
}

void LuaRunnerView::renderFileListView(size_t folderIdx) {
    if (folderIdx >= m_folderCategories.size()) return;
    m_currentFolderIdx = folderIdx;

    if (!m_modalBody || !lv_obj_is_valid(m_modalBody)) return;
    lv_obj_clean(m_modalBody);

    const auto& cat = m_folderCategories[folderIdx];

    if (m_modalTitle && lv_obj_is_valid(m_modalTitle)) {
        char titleBuf[64];
        snprintf(titleBuf, sizeof(titleBuf), "%s (%d)", cat.title.c_str(), (int)cat.files.size());
        lv_label_set_text(m_modalTitle, titleBuf);
    }

    // Botón de Volver
    lv_obj_t* btnBack = lv_button_create(m_modalBody);
    lv_obj_set_size(btnBack, LV_PCT(100), 38);
    DefaultTheme::applyButton(btnBack, 8);
    lv_obj_set_flex_flow(btnBack, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnBack, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(btnBack, 10, 0);
    lv_obj_add_event_cb(btnBack, folderBackCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, LV_SYMBOL_LEFT " Volver a Carpetas");
    lv_obj_set_style_text_color(lblBack, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(lblBack, &lv_font_montserrat_12, 0);

    if (cat.files.empty()) {
        lv_obj_t* emptyLbl = lv_label_create(m_modalBody);
        lv_label_set_text(emptyLbl, "No se encontraron scripts (.lua / .luapp)\nen este directorio.");
        lv_obj_set_style_text_color(emptyLbl, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_margin_top(emptyLbl, 20, 0);
        lv_obj_set_width(emptyLbl, LV_PCT(100));
    } else {
        for (size_t i = 0; i < cat.files.size(); i++) {
            const std::string& fullPath = cat.files[i];
            size_t slashPos = fullPath.find_last_of('/');
            std::string fileName = (slashPos != std::string::npos) ? fullPath.substr(slashPos + 1) : fullPath;

            lv_obj_t* item = lv_button_create(m_modalBody);
            lv_obj_set_size(item, LV_PCT(100), 40);
            DefaultTheme::applyButton(item, 8);
            lv_obj_set_user_data(item, (void*)(intptr_t)i);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(item, 6, 0);

            lv_obj_t* icon = lv_label_create(item);
            lv_label_set_text(icon, LV_SYMBOL_FILE);
            lv_obj_set_style_text_color(icon, DefaultTheme::getPrimaryAccent(), 0);

            lv_obj_t* name = lv_label_create(item);
            lv_label_set_text(name, fileName.c_str());
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
            lv_obj_set_flex_grow(name, 1);
            lv_obj_set_style_text_color(name, DefaultTheme::getTextColor(), 0);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
            lv_obj_set_style_margin_left(name, 6, 0);

            lv_obj_add_event_cb(item, fileSelectCb, LV_EVENT_CLICKED, this);
        }
    }
}

bool LuaRunnerView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Apertura instantánea: el escaneo de SD se realiza bajo demanda al abrir el selector (📁)

    // Configurar HeaderBar para esta app
    UIManager::getInstance().getHeaderBar().showWifi(false);
    UIManager::getInstance().getHeaderBar().clearRightAction();

    // Contenedor principal de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 6, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    DefaultTheme::disableScroll(m_container);

    // 1. Info Bar (Script activo + Badge de Estado)
    lv_obj_t* infoBar = lv_obj_create(m_container);
    lv_obj_set_size(infoBar, LV_PCT(100), 34);
    DefaultTheme::applySunkenCard(infoBar, 8);
    lv_obj_set_flex_flow(infoBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(infoBar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(infoBar, 10, 0);
    lv_obj_set_style_pad_ver(infoBar, 2, 0);
    DefaultTheme::disableScroll(infoBar);

    m_scriptLabel = lv_label_create(infoBar);
    lv_label_set_text(m_scriptLabel, m_activeScript.empty() ? "(Sin script seleccionado)" : m_activeScript.c_str());
    lv_label_set_long_mode(m_scriptLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(m_scriptLabel, 1);
    lv_obj_set_style_text_color(m_scriptLabel, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(m_scriptLabel, &lv_font_montserrat_12, 0);

    m_statusBadge = lv_label_create(infoBar);
    lv_label_set_text(m_statusBadge, "IDLE");
    lv_obj_set_style_text_color(m_statusBadge, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(m_statusBadge, &lv_font_montserrat_12, 0);

    // 2. Consola de Logs (Centro)
    m_logContainer = lv_obj_create(m_container);
    lv_obj_set_width(m_logContainer, LV_PCT(100));
    lv_obj_set_flex_grow(m_logContainer, 1);
    DefaultTheme::applySunkenCard(m_logContainer, 12);
    lv_obj_set_style_bg_color(m_logContainer, lv_color_hex(0x0E1217), 0);
    lv_obj_set_flex_flow(m_logContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_logContainer, 8, 0);
    lv_obj_set_style_pad_row(m_logContainer, 4, 0);
    lv_obj_set_scroll_dir(m_logContainer, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_logContainer, LV_SCROLLBAR_MODE_AUTO);

    appendLogLine("[Sistema] Consola de Lua 5.4 lista.");

    // 3. Barra de Acciones Inferior (Dock de botones táctiles)
    lv_obj_t* dock = lv_obj_create(m_container);
    lv_obj_set_size(dock, LV_PCT(100), 52);
    lv_obj_set_style_bg_opa(dock, 0, 0);
    lv_obj_set_style_border_width(dock, 0, 0);
    lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dock, 0, 0);
    DefaultTheme::disableScroll(dock);

    auto caps = cbdos::display::getCapabilities();
    int32_t btnW = (caps.width >= 480) ? 80 : 54;

    // Botón Run (Play)
    lv_obj_t* btnRun = lv_button_create(dock);
    lv_obj_set_size(btnRun, btnW, 44);
    DefaultTheme::applyButton(btnRun, 8);
    lv_obj_set_style_bg_color(btnRun, lv_color_hex(0x1B5E20), 0); // Verde oscuro
    lv_obj_t* lblRun = lv_label_create(btnRun);
    lv_label_set_text(lblRun, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(lblRun, lv_color_hex(0x00E676), 0);
    lv_obj_center(lblRun);
    lv_obj_add_event_cb(btnRun, btnRunCb, LV_EVENT_CLICKED, this);

    // Botón Stop
    lv_obj_t* btnStop = lv_button_create(dock);
    lv_obj_set_size(btnStop, btnW, 44);
    DefaultTheme::applyButton(btnStop, 8);
    lv_obj_set_style_bg_color(btnStop, lv_color_hex(0xB71C1C), 0); // Rojo oscuro
    lv_obj_t* lblStop = lv_label_create(btnStop);
    lv_label_set_text(lblStop, LV_SYMBOL_STOP);
    lv_obj_set_style_text_color(lblStop, lv_color_hex(0xFF5252), 0);
    lv_obj_center(lblStop);
    lv_obj_add_event_cb(btnStop, btnStopCb, LV_EVENT_CLICKED, this);

    // Botón Editar
    lv_obj_t* btnEdit = lv_button_create(dock);
    lv_obj_set_size(btnEdit, btnW, 44);
    DefaultTheme::applyButton(btnEdit, 8);
    lv_obj_t* lblEdit = lv_label_create(btnEdit);
    lv_label_set_text(lblEdit, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(lblEdit, lv_color_hex(0xFFB800), 0);
    lv_obj_center(lblEdit);
    lv_obj_add_event_cb(btnEdit, btnEditCb, LV_EVENT_CLICKED, this);

    // Botón Cargar SD
    lv_obj_t* btnSd = lv_button_create(dock);
    lv_obj_set_size(btnSd, btnW, 44);
    DefaultTheme::applyButton(btnSd, 8);
    lv_obj_t* lblSd = lv_label_create(btnSd);
    lv_label_set_text(lblSd, LV_SYMBOL_DIRECTORY);
    lv_obj_set_style_text_color(lblSd, DefaultTheme::getTextColor(), 0);
    lv_obj_center(lblSd);
    lv_obj_add_event_cb(btnSd, btnSdCb, LV_EVENT_CLICKED, this);

    // Botón Limpiar Consola
    lv_obj_t* btnClear = lv_button_create(dock);
    lv_obj_set_size(btnClear, btnW, 44);
    DefaultTheme::applyButton(btnClear, 8);
    lv_obj_t* lblClear = lv_label_create(btnClear);
    lv_label_set_text(lblClear, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_color(lblClear, DefaultTheme::getTextColor(), 0);
    lv_obj_center(lblClear);
    lv_obj_add_event_cb(btnClear, btnClearCb, LV_EVENT_CLICKED, this);

    // 4. Timer de refresco de logs y estado cada 50ms
    m_refreshTimer = lv_timer_create(timerCb, 50, this);

    updateStatusBadge();
    return true;
}

void LuaRunnerView::onDestroy() {
    if (m_refreshTimer) {
        lv_timer_delete(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
    if (m_modalMask && lv_obj_is_valid(m_modalMask)) {
        lv_obj_delete(m_modalMask);
        m_modalMask = nullptr;
    }
    m_modalTitle = nullptr;
    m_modalBody = nullptr;
    m_logContainer = nullptr;
    m_statusBadge = nullptr;
    m_scriptLabel = nullptr;

    UIManager::getInstance().getHeaderBar().clearRightAction();
    UIManager::getInstance().getHeaderBar().showWifi(true);
    BaseView::onDestroy();
}

void LuaRunnerView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    (void)theme;
    (void)palette;
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    }
}

} // namespace ui
} // namespace cbdos
