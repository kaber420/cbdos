#include "DiagnosticsModal.hpp"
#include "C6FlasherModal.hpp"
#include "../UiMetrics.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "cbdos/display.hpp"
#include "cbdos/network.hpp"
#include "cbdos/radio.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/system.hpp"
#include "cbdos/storage.hpp"
#include <cstdio>
#include <time.h>



namespace cbdos {
namespace ui {

lv_obj_t* DiagnosticsModal::s_modalMask = nullptr;

void DiagnosticsModal::close_btn_cb(lv_event_t* e) {
    hide();
}

void DiagnosticsModal::hide() {
    if (s_modalMask && lv_obj_is_valid(s_modalMask)) {
        lv_obj_delete_async(s_modalMask);
        s_modalMask = nullptr;
    }
}

void DiagnosticsModal::show(lv_obj_t* parent) {
    hide();

    auto caps = cbdos::display::getCapabilities();
    const auto metrics = UiMetrics::current();

    s_modalMask = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_modalMask, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_modalMask, 0, 0);
    lv_obj_set_style_bg_color(s_modalMask, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_modalMask, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_modalMask, 0, 0);
    lv_obj_set_style_pad_all(s_modalMask, 10, 0);

    // Tarjeta Modal con Glassmorphism
    lv_obj_t* card = lv_obj_create(s_modalMask);
    applyAdaptiveModalSize(card, metrics, 480, 620);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_center(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);

    // 1. Título con Icono / Acento
    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Diagnostico del Sistema");
    lv_obj_set_style_text_color(title, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_margin_bottom(title, 6, 0);

    // Helper para formatear bytes en KB, MB o GB
    auto formatBytes = [](uint64_t bytes, char* buf, size_t bufSize) {
        if (bytes >= (1024ULL * 1024ULL * 1024ULL)) {
            double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
            snprintf(buf, bufSize, "%.2f GB", gb);
        } else if (bytes >= (1024ULL * 1024ULL)) {
            double mb = (double)bytes / (1024.0 * 1024.0);
            snprintf(buf, bufSize, "%.1f MB", mb);
        } else if (bytes >= 1024ULL) {
            snprintf(buf, bufSize, "%u KB", (unsigned int)(bytes / 1024));
        } else {
            snprintf(buf, bufSize, "%u B", (unsigned int)bytes);
        }
    };

    // Helper para añadir filas de clave-valor
    auto addInfoRow = [](lv_obj_t* p, const char* label, const char* val, bool isOk = true) {
        lv_obj_t* row = lv_obj_create(p);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(row, 1, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lblKey = lv_label_create(row);
        lv_label_set_text(lblKey, label);
        lv_obj_set_style_text_color(lblKey, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblKey, &lv_font_montserrat_12, 0);

        lv_obj_t* lblVal = lv_label_create(row);
        lv_label_set_text(lblVal, val);
        lv_obj_set_style_text_font(lblVal, &lv_font_montserrat_12, 0);
        if (isOk) {
            lv_obj_set_style_text_color(lblVal, DefaultTheme::getTextColor(), 0);
        } else {
            lv_obj_set_style_text_color(lblVal, lv_color_hex(0xEF4444), 0);
        }
    };

    // Helper para añadir separador visual de sección
    auto addSectionHeader = [](lv_obj_t* p, const char* titleText) {
        lv_obj_t* lbl = lv_label_create(p);
        lv_label_set_text(lbl, titleText);
        lv_obj_set_style_text_color(lbl, DefaultTheme::getSecondaryAccent(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_margin_top(lbl, 4, 0);
        lv_obj_set_style_margin_bottom(lbl, 2, 0);
    };

    // Helper para añadir barra de progreso de almacenamiento / memoria
    auto addStorageBar = [&](lv_obj_t* p, const char* title, uint64_t usedBytes, uint64_t totalBytes, 
                             bool isPresent, lv_color_t barColor) {
        lv_obj_t* box = lv_obj_create(p);
        lv_obj_set_width(box, lv_pct(100));
        lv_obj_set_height(box, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(box, 0, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(box, 1, 0);
        lv_obj_set_style_pad_row(box, 3, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        // Fila 1: Título y Estado / Porcentaje
        lv_obj_t* topRow = lv_obj_create(box);
        lv_obj_set_width(topRow, lv_pct(100));
        lv_obj_set_height(topRow, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(topRow, 0, 0);
        lv_obj_set_style_border_width(topRow, 0, 0);
        lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(topRow, 0, 0);
        lv_obj_remove_flag(topRow, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lblName = lv_label_create(topRow);
        lv_label_set_text(lblName, title);
        lv_obj_set_style_text_color(lblName, DefaultTheme::getMutedTextColor(), 0);
        lv_obj_set_style_text_font(lblName, &lv_font_montserrat_12, 0);

        lv_obj_t* lblStatus = lv_label_create(topRow);
        lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_12, 0);

        if (!isPresent) {
            lv_label_set_text(lblStatus, "No detectada");
            lv_obj_set_style_text_color(lblStatus, lv_color_hex(0xEF4444), 0);
            return;
        }

        uint32_t percent = (totalBytes > 0) ? (uint32_t)((usedBytes * 100ULL) / totalBytes) : 0;
        if (percent > 100) percent = 100;

        char pctBuf[32];
        snprintf(pctBuf, sizeof(pctBuf), "%u%% Usado", (unsigned int)percent);
        lv_label_set_text(lblStatus, pctBuf);
        lv_obj_set_style_text_color(lblStatus, DefaultTheme::getTextColor(), 0);

        // Fila 2: Barra de progreso gráfica
        lv_obj_t* bar = lv_bar_create(box);
        lv_obj_set_size(bar, lv_pct(100), 7);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, percent, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x1E293B), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, barColor, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

        // Fila 3: Detalle Usado / Libre / Total
        char uBuf[16], fBuf[16], tBuf[16];
        uint64_t freeBytes = (totalBytes > usedBytes) ? (totalBytes - usedBytes) : 0;
        formatBytes(usedBytes, uBuf, sizeof(uBuf));
        formatBytes(freeBytes, fBuf, sizeof(fBuf));
        formatBytes(totalBytes, tBuf, sizeof(tBuf));

        char detailBuf[128];
        snprintf(detailBuf, sizeof(detailBuf), "U: %s | L: %s | Total: %s", uBuf, fBuf, tBuf);

        lv_obj_t* lblDetail = lv_label_create(box);
        lv_label_set_text(lblDetail, detailBuf);
        lv_obj_set_style_text_color(lblDetail, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(lblDetail, &lv_font_montserrat_12, 0);
    };

    // --- SECCIÓN 1: HARDWARE & SISTEMA ---
    addSectionHeader(card, "--- Hardware y Sistema ---");
    char dispBuf[64];
    if (caps.hasHardware2D) {
        addInfoRow(card, "Target Chip:", "ESP32-P4", true);
        snprintf(dispBuf, sizeof(dispBuf), "%ux%u MIPI-DPI @ %u FPS",
                 (unsigned int)caps.width, (unsigned int)caps.height, (unsigned int)caps.targetFps);
        addInfoRow(card, "Pantalla:", dispBuf, true);
    } else {
        addInfoRow(card, "Target Chip:", "ESP32-S3 (JC3248W535)", true);
        snprintf(dispBuf, sizeof(dispBuf), "%ux%u QSPI @ %u FPS",
                 (unsigned int)caps.width, (unsigned int)caps.height, (unsigned int)caps.targetFps);
        addInfoRow(card, "Pantalla:", dispBuf, true);
    }

    uint32_t uptimeSec = cbdos::system::getTimeMs() / 1000;
    uint32_t mins = uptimeSec / 60;
    uint32_t secs = uptimeSec % 60;
    char upBuf[32];
    snprintf(upBuf, sizeof(upBuf), "%um %us", (unsigned int)mins, (unsigned int)secs);
    addInfoRow(card, "Tiempo Activo:", upBuf, true);

    float cpuTemp = cbdos::system::getCpuTemperature();
    if (cpuTemp > 0.0f) {
        char tempBuf[32];
        snprintf(tempBuf, sizeof(tempBuf), "%.1f °C", cpuTemp);
        addInfoRow(card, "Temperatura CPU:", tempBuf, cpuTemp < 70.0f);
    } else {
        addInfoRow(card, "Temperatura CPU:", "N/D", false);
    }

    // --- SECCIÓN 2: MEMORIA RAM & PSRAM ---
    addSectionHeader(card, "--- Memoria de Trabajo ---");
    size_t freeHeap = cbdos::system::getFreeHeap();
    size_t totalHeap = cbdos::system::getTotalHeap();
    size_t usedHeap = (totalHeap > freeHeap) ? (totalHeap - freeHeap) : 0;
    addStorageBar(card, "RAM (Heap Interno)", usedHeap, totalHeap, true, DefaultTheme::getPrimaryAccent());

    size_t freePsram = cbdos::system::getFreePsram();
    size_t totalPsram = cbdos::system::getTotalPsram();
    size_t usedPsram = (totalPsram > freePsram) ? (totalPsram - freePsram) : 0;
    if (totalPsram > 0) {
        addStorageBar(card, "PSRAM Externa", usedPsram, totalPsram, true, DefaultTheme::getSecondaryAccent());
    }

    // --- SECCIÓN 3: ALMACENAMIENTO (FLASH & SD CARD) ---
    addSectionHeader(card, "--- Almacenamiento y Medios ---");
    auto flashStats = cbdos::storage::getFlashStats();
    addStorageBar(card, "[Flash] Memoria Interna", flashStats.usedBytes, flashStats.totalBytes, 
                  flashStats.isMounted, lv_color_hex(0x00F5D4));

    auto sdStats = cbdos::storage::getSdCardStats();
    addStorageBar(card, "[SD Card] Tarjeta MicroSD", sdStats.usedBytes, sdStats.totalBytes, 
                  sdStats.isMounted, lv_color_hex(0x10B981));

    // --- SECCIÓN 4: CONECTIVIDAD & RED ---
    addSectionHeader(card, "--- Conectividad y Red ---");
    auto radioMode = cbdos::radio::getMode();
    bool isPowered = cbdos::radio::isRadioPowered();

    if (!isPowered || radioMode == cbdos::radio::RadioMode::Off) {
        addInfoRow(card, "Modo Radio:", "Apagada (OFF)", false);
        addInfoRow(card, "Estado:", "Ahorro de Energia", false);
    } else if (radioMode == cbdos::radio::RadioMode::WifiSta) {
        bool wifiConn = cbdos::network::isConnected();
        addInfoRow(card, "Modo Radio:", "Wi-Fi Estacion (TCP/IP)", true);
        addInfoRow(card, "WiFi Estado:", wifiConn ? "Conectado" : "Desconectado", wifiConn);
        
        std::string ip = cbdos::network::getIpAddress();
        addInfoRow(card, "Direccion IP:", wifiConn ? ip.c_str() : "--", wifiConn);

        int8_t rssi = cbdos::network::getRssi();
        char rssiBuf[16];
        snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", rssi);
        addInfoRow(card, "Nivel Senal:", wifiConn ? rssiBuf : "--", wifiConn);
    } else {
        // Modos ESP-NOW / Mesh (Normal, LR o Híbrido)
        auto& mesh = cbdos::mesh::MeshEngine::getInstance();
        const char* modeName = (radioMode == cbdos::radio::RadioMode::EspNowLR) ? "ESP-NOW Long Range (LR)" :
                               ((radioMode == cbdos::radio::RadioMode::EspNow) ? "ESP-NOW Normal" : "Hibrido (Mesh+WiFi)");
        addInfoRow(card, "Modo Radio:", modeName, true);

        char chanBuf[32];
        snprintf(chanBuf, sizeof(chanBuf), "Canal %u (%d dBm)", cbdos::radio::getChannel(), cbdos::radio::getTxPower());
        addInfoRow(card, "Canal / TX:", chanBuf, true);

        // IPv4 Mesh (UUID)
        uint32_t uuid = mesh.getLocalUuid();
        char ipBuf[32];
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", 
                 (unsigned int)((uuid >> 24) & 0xFF), 
                 (unsigned int)((uuid >> 16) & 0xFF), 
                 (unsigned int)((uuid >> 8) & 0xFF), 
                 (unsigned int)(uuid & 0xFF));
        addInfoRow(card, "IP Mesh Global:", ipBuf, true);

        // Short ID & MAC
        uint8_t mac[6] = {0};
        mesh.getMacAddress(mac);
        char idBuf[48];
        snprintf(idBuf, sizeof(idBuf), "0x%04X | %02X:%02X:%02X:%02X:%02X:%02X",
                 mesh.getLocalShortId(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        addInfoRow(card, "Short ID / MAC:", idBuf, true);

        // Señal RSSI del último paquete recibido de la Torre
        int8_t meshRssi = mesh.getLastRssi();
        if (meshRssi > -120 && meshRssi < 0) {
            char meshRssiBuf[32];
            const char* quality = (meshRssi > -60) ? "Excelente" : ((meshRssi > -80) ? "Buena" : "Debil");
            snprintf(meshRssiBuf, sizeof(meshRssiBuf), "%d dBm (%s)", meshRssi, quality);
            addInfoRow(card, "Senal Torre (RSSI):", meshRssiBuf, meshRssi > -85);
        } else {
            addInfoRow(card, "Senal Torre (RSSI):", "Esperando trama...", false);
        }

        // Tráfico TX / RX
        char pktsBuf[48];
        snprintf(pktsBuf, sizeof(pktsBuf), "TX: %u | RX: %u", (unsigned int)mesh.getTxPackets(), (unsigned int)mesh.getRxPackets());
        addInfoRow(card, "Trafico Malla:", pktsBuf, true);
    }

    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);
    char timeBuf[32];
    if (timeinfo && timeinfo->tm_year > (2020 - 1900)) {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d (NTP OK)", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        addInfoRow(card, "Hora Sistema:", timeBuf, true);
    } else {
        addInfoRow(card, "Hora Sistema:", "Sin sincronizar", false);
    }

    // Botón Probar Wi-Fi SDIO
    lv_obj_t* btnTestWifi = lv_button_create(card);
    lv_obj_set_size(btnTestWifi, lv_pct(100), 38);
    DefaultTheme::applyButton(btnTestWifi, 10);
    lv_obj_set_style_bg_color(btnTestWifi, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_margin_top(btnTestWifi, 8, 0);

    lv_obj_t* lblTw = lv_label_create(btnTestWifi);
    lv_label_set_text(lblTw, "Probar Conexion C6 (SDIO)");
    lv_obj_set_style_text_color(lblTw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblTw);

    auto test_wifi_cb = [](lv_event_t* e) {
        cbdos::system::log(cbdos::system::LogLevel::Info, "Diagnostics", "Iniciando prueba manual de Wi-Fi con ESP32-C6 (GPIO 54 Reset)...");
        bool ok = cbdos::network::connectWifi("romero24", "guzman420");
        if (ok) {
            UIManager::showToast("Iniciando comunicacion SDIO...");
        } else {
            UIManager::showToast("Fallo al iniciar SDIO");
        }
    };
    lv_obj_add_event_cb(btnTestWifi, test_wifi_cb, LV_EVENT_CLICKED, nullptr);

    // Botón Abrir Flasheador Autónomo C6
    lv_obj_t* btnOpenFlasher = lv_button_create(card);

    lv_obj_set_size(btnOpenFlasher, lv_pct(100), 38);
    DefaultTheme::applyButton(btnOpenFlasher, 10);
    lv_obj_set_style_bg_color(btnOpenFlasher, lv_color_hex(0x7C3AED), 0);
    lv_obj_set_style_margin_top(btnOpenFlasher, 8, 0);

    lv_obj_t* lblFl = lv_label_create(btnOpenFlasher);
    lv_label_set_text(lblFl, "Abrir Flasheador Coprocesador C6");
    lv_obj_set_style_text_color(lblFl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblFl);

    auto open_flasher_cb = [](lv_event_t* e) {
        DiagnosticsModal::hide();
        C6FlasherModal::show();
    };
    lv_obj_add_event_cb(btnOpenFlasher, open_flasher_cb, LV_EVENT_CLICKED, nullptr);

    // Botón Cerrar
    lv_obj_t* btnClose = lv_button_create(card);
    lv_obj_set_size(btnClose, lv_pct(100), 38);
    DefaultTheme::applyButton(btnClose, 10);
    lv_obj_set_style_bg_color(btnClose, DefaultTheme::getPrimaryAccent(), 0);
    lv_obj_set_style_margin_top(btnClose, 6, 0);

    lv_obj_t* lblC = lv_label_create(btnClose);
    lv_label_set_text(lblC, "Cerrar Diagnostico");
    lv_obj_set_style_text_color(lblC, lv_color_hex(0x0F172A), 0);
    lv_obj_center(lblC);

    lv_obj_add_event_cb(btnClose, close_btn_cb, LV_EVENT_CLICKED, nullptr);


}

} // namespace ui
} // namespace cbdos
