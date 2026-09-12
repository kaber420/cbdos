#include "LanReconView.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"
#include "TerminalView.hpp"
#include "../../network/LanScannerService.hpp"
#include "cbdos/lan_recon.hpp"

#include <lvgl.h>

#include <cstdio>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

namespace cbdos {
namespace ui {

namespace {
constexpr uint32_t kAccent = 0x00F5D4;
constexpr uint32_t kCardBg = 0x161C28;
constexpr uint32_t kCardBorder = 0x28334A;
constexpr uint32_t kChipBg = 0x1F293D;
} // namespace

LanReconView::LanReconView() : BaseView("LAN Recon") {}

LanReconView::~LanReconView() {
    if (m_pollTimer) {
        lv_timer_delete(m_pollTimer);
        m_pollTimer = nullptr;
    }
}

bool LanReconView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;
    UIManager::getInstance().getHeaderBar().setTitle("LAN Recon");

    // Contenedor raiz: columna vertical con scroll.
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_radius(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 8, 0);
    lv_obj_set_style_pad_row(m_container, 8, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // ── Header / Control bar ──────────────────────────────────────
    lv_obj_t* header = lv_obj_create(m_container);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    DefaultTheme::applyRaisedCard(header, 12);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(header, 10, 0);
    lv_obj_set_style_pad_row(header, 6, 0);
    DefaultTheme::disableScroll(header);

    m_lblNetInfo = lv_label_create(header);
    lv_label_set_text(m_lblNetInfo, "Sin red");
    lv_obj_set_style_text_color(m_lblNetInfo, lv_color_white(), 0);
    lv_obj_set_style_text_font(m_lblNetInfo, &lv_font_montserrat_12, 0);

    m_lblCount = lv_label_create(header);
    lv_label_set_text(m_lblCount, "0 hosts");
    lv_obj_set_style_text_color(m_lblCount, lv_color_hex(kAccent), 0);
    lv_obj_set_style_text_font(m_lblCount, &lv_font_montserrat_12, 0);

    lv_obj_t* btnRow = lv_obj_create(header);
    lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    DefaultTheme::disableScroll(btnRow);

    m_btnScan = lv_button_create(btnRow);
    lv_obj_set_size(m_btnScan, LV_PCT(48), 40);
    lv_obj_set_style_bg_color(m_btnScan, lv_color_hex(kAccent), 0);
    lv_obj_set_style_radius(m_btnScan, 10, 0);
    lv_obj_add_event_cb(m_btnScan, scanBtnCb, LV_EVENT_CLICKED, this);
    m_lblScan = lv_label_create(m_btnScan);
    lv_label_set_text(m_lblScan, "Iniciar Escaneo");
    lv_obj_set_style_text_color(m_lblScan, lv_color_black(), 0);
    lv_obj_center(m_lblScan);

    m_btnExport = lv_button_create(btnRow);
    lv_obj_set_size(m_btnExport, LV_PCT(48), 40);
    lv_obj_set_style_bg_color(m_btnExport, lv_color_hex(0x1F293D), 0);
    lv_obj_set_style_border_color(m_btnExport, lv_color_hex(kAccent), 0);
    lv_obj_set_style_border_width(m_btnExport, 1, 0);
    lv_obj_set_style_radius(m_btnExport, 10, 0);
    lv_obj_add_event_cb(m_btnExport, exportBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblExp = lv_label_create(m_btnExport);
    lv_label_set_text(lblExp, "Exportar CSV");
    lv_obj_set_style_text_color(lblExp, lv_color_hex(kAccent), 0);
    lv_obj_center(lblExp);

    // ── v2: fila CIDR (vacio = subred local) ────────────────────────
    m_taCidr = lv_textarea_create(header);
    lv_obj_set_width(m_taCidr, LV_PCT(100));
    lv_textarea_set_one_line(m_taCidr, true);
    lv_textarea_set_placeholder_text(m_taCidr, "CIDR (vacio=local) ej. 192.168.10.0/24");
    lv_obj_set_style_text_font(m_taCidr, &lv_font_montserrat_12, 0);
    // Teclado en pantalla global: sin esto el tactil no puede escribir.
    UIManager::attachKeyboard(m_taCidr);

    // ── v2: fila ping individual ────────────────────────────────────
    lv_obj_t* pingRow = lv_obj_create(header);
    lv_obj_set_size(pingRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(pingRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pingRow, 0, 0);
    lv_obj_set_style_pad_all(pingRow, 0, 0);
    lv_obj_set_flex_flow(pingRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pingRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(pingRow, 6, 0);
    DefaultTheme::disableScroll(pingRow);

    m_taPing = lv_textarea_create(pingRow);
    lv_obj_set_flex_grow(m_taPing, 1);
    lv_textarea_set_one_line(m_taPing, true);
    lv_textarea_set_placeholder_text(m_taPing, "Ping IP ej. 192.168.1.1");
    lv_obj_set_style_text_font(m_taPing, &lv_font_montserrat_12, 0);
    UIManager::attachKeyboard(m_taPing);
    // Pre-rellenar con el gateway: un toque a [Ping] funciona sin escribir.
    {
        auto* backend = cbdos::network::getLanScannerBackend();
        if (backend) {
            std::string gw = backend->getGatewayIp();
            if (!gw.empty()) {
                lv_textarea_set_text(m_taPing, gw.c_str());
            }
        }
    }

    lv_obj_t* btnPing = lv_button_create(pingRow);
    lv_obj_set_size(btnPing, 86, 40);
    lv_obj_set_style_bg_color(btnPing, lv_color_hex(0x1F293D), 0);
    lv_obj_set_style_border_color(btnPing, lv_color_hex(kAccent), 0);
    lv_obj_set_style_border_width(btnPing, 1, 0);
    lv_obj_set_style_radius(btnPing, 10, 0);
    lv_obj_add_event_cb(btnPing, pingBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblPing = lv_label_create(btnPing);
    lv_label_set_text(lblPing, "Ping");
    lv_obj_set_style_text_color(lblPing, lv_color_hex(kAccent), 0);
    lv_obj_center(lblPing);

    m_lblPingResult = lv_label_create(header);
    lv_label_set_text(m_lblPingResult, "");
    lv_obj_set_style_text_color(m_lblPingResult, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(m_lblPingResult, &lv_font_montserrat_12, 0);

    // ── Progreso + fase ───────────────────────────────────────────
    lv_obj_t* progCard = lv_obj_create(m_container);
    lv_obj_set_width(progCard, LV_PCT(100));
    lv_obj_set_height(progCard, LV_SIZE_CONTENT);
    DefaultTheme::applySunkenCard(progCard, 10);
    lv_obj_set_flex_flow(progCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(progCard, 10, 0);
    lv_obj_set_style_pad_row(progCard, 6, 0);
    DefaultTheme::disableScroll(progCard);

    m_bar = lv_bar_create(progCard);
    lv_obj_set_size(m_bar, LV_PCT(100), 12);
    lv_bar_set_range(m_bar, 0, 100);
    lv_bar_set_value(m_bar, 0, LV_ANIM_OFF);

    m_lblPhase = lv_label_create(progCard);
    lv_label_set_text(m_lblPhase, "Listo para escanear");
    lv_obj_set_style_text_color(m_lblPhase, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(m_lblPhase, &lv_font_montserrat_12, 0);

    // ── Lista scrollable de hosts (flex column) ───────────────────
    m_list = lv_obj_create(m_container);
    lv_obj_set_size(m_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_list, 1);
    lv_obj_set_style_bg_opa(m_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_list, 0, 0);
    lv_obj_set_style_pad_all(m_list, 0, 0);
    lv_obj_set_style_pad_row(m_list, 8, 0);
    lv_obj_set_flex_flow(m_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(m_list, LV_SCROLLBAR_MODE_AUTO);

    // Timer de refresco reactivo en Core 1 (baja frecuencia).
    m_pollTimer = lv_timer_create(pollTimerCb, 400, this);

    refreshFromService(true);
    return true;
}

void LanReconView::onDestroy() {
    if (m_pollTimer) {
        lv_timer_delete(m_pollTimer);
        m_pollTimer = nullptr;
    }
    m_lblNetInfo = nullptr;
    m_lblCount = nullptr;
    m_btnScan = nullptr;
    m_lblScan = nullptr;
    m_btnExport = nullptr;
    m_bar = nullptr;
    m_lblPhase = nullptr;
    m_list = nullptr;
    m_taCidr = nullptr;
    m_taPing = nullptr;
    m_lblPingResult = nullptr;
    BaseView::onDestroy();
}

void LanReconView::onShow() {
    BaseView::onShow();
    if (m_pollTimer) lv_timer_resume(m_pollTimer);
}

void LanReconView::onHide() {
    if (m_pollTimer) lv_timer_pause(m_pollTimer);
    BaseView::onHide();
}

// ── Callbacks ─────────────────────────────────────────────────────

void LanReconView::scanBtnCb(lv_event_t* e) {
    auto* view = static_cast<LanReconView*>(lv_event_get_user_data(e));
    if (!view) return;
    auto& svc = cbdos::network::LanScannerService::getInstance();
    if (svc.isScanning()) {
        svc.stopScan();
    } else {
        // v2: si hay CIDR valido, barrido a red arbitraria (ruteada).
        std::string cidr;
        if (view->m_taCidr && lv_obj_is_valid(view->m_taCidr)) {
            const char* txt = lv_textarea_get_text(view->m_taCidr);
            if (txt) {
                cidr = txt;
                // trim simple
                while (!cidr.empty() && (cidr.front() == ' ' || cidr.front() == '\t')) {
                    cidr.erase(cidr.begin());
                }
                while (!cidr.empty() && (cidr.back() == ' ' || cidr.back() == '\t' ||
                                         cidr.back() == '\r' || cidr.back() == '\n')) {
                    cidr.pop_back();
                }
            }
        }
        bool ok = false;
        if (cidr.empty()) {
            ok = svc.startScan();
        } else {
            ok = svc.startScanCidr(cidr);
            if (!ok) {
                UIManager::showToast("CIDR invalido (ej. 192.168.10.0/24)");
                return;
            }
        }
        if (!ok) {
            UIManager::showToast("Sin red o backend no listo");
        }
    }
    view->refreshFromService(true);
}

void LanReconView::pingBtnCb(lv_event_t* e) {
    auto* view = static_cast<LanReconView*>(lv_event_get_user_data(e));
    if (!view) return;
    std::string ip;
    if (view->m_taPing && lv_obj_is_valid(view->m_taPing)) {
        const char* txt = lv_textarea_get_text(view->m_taPing);
        if (txt) {
            ip = txt;
        }
    }
    while (!ip.empty() && (ip.front() == ' ' || ip.front() == '\t')) {
        ip.erase(ip.begin());
    }
    while (!ip.empty() && (ip.back() == ' ' || ip.back() == '\t' ||
                           ip.back() == '\r' || ip.back() == '\n')) {
        ip.pop_back();
    }
    if (ip.empty()) {
        UIManager::showToast("Escribe una IP para ping");
        return;
    }
    auto& svc = cbdos::network::LanScannerService::getInstance();
    uint32_t rtt = 0;
    std::string method;
    // Bloqueante breve (~1s max): congela la UI un instante pero evita
    // hilos y carreras en LVGL.
    bool ok = svc.pingSingle(ip, 1200, &rtt, &method);
    char buf[96];
    if (ok) {
        if (method == "icmp" && rtt > 0) {
            snprintf(buf, sizeof(buf), "%s responde %s %ums", ip.c_str(),
                     method.c_str(), static_cast<unsigned>(rtt));
        } else {
            snprintf(buf, sizeof(buf), "%s responde (%s)", ip.c_str(), method.c_str());
        }
    } else {
        snprintf(buf, sizeof(buf), "%s sin respuesta", ip.c_str());
    }
    if (view->m_lblPingResult && lv_obj_is_valid(view->m_lblPingResult)) {
        lv_label_set_text(view->m_lblPingResult, buf);
    }
    UIManager::showToast(buf);
}

void LanReconView::exportBtnCb(lv_event_t* e) {
    auto* view = static_cast<LanReconView*>(lv_event_get_user_data(e));
    if (!view) return;
    auto& svc = cbdos::network::LanScannerService::getInstance();
    std::string csv = svc.exportCsv();
    if (csv.empty()) {
        UIManager::showToast("Sin datos para exportar");
        return;
    }

    mkdir("/sdcard", 0755);
    mkdir("/sdcard/recon", 0755);

    char fname[64];
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
    localtime_r(&now, &tmBuf);
    std::strftime(fname, sizeof(fname), "lan_%Y%m%d_%H%M%S.csv", &tmBuf);
    std::string path = std::string("/sdcard/recon/") + fname;

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        UIManager::showToast("No se pudo escribir en /sdcard/recon/");
        return;
    }
    fwrite(csv.data(), 1, csv.size(), f);
    fclose(f);

    char msg[96];
    snprintf(msg, sizeof(msg), "CSV guardado: %s", fname);
    UIManager::showToast(msg);
}

void LanReconView::pollTimerCb(lv_timer_t* timer) {
    auto* view = static_cast<LanReconView*>(lv_timer_get_user_data(timer));
    if (!view) return;
    view->refreshFromService(false);
}

void LanReconView::sshBtnCb(lv_event_t* e) {
    auto* ip = static_cast<std::string*>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        delete ip;
        return;
    }
    if (code != LV_EVENT_CLICKED || !ip) return;
    char msg[80];
    snprintf(msg, sizeof(msg), "SSH -> %s", ip->c_str());
    UIManager::showToast(msg);
    UIManager::getInstance().pushView(std::make_shared<TerminalView>());
}

void LanReconView::pingHostBtnCb(lv_event_t* e) {
    auto* ip = static_cast<std::string*>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        delete ip;
        return;
    }
    if (code != LV_EVENT_CLICKED || !ip || ip->empty()) return;
    uint32_t rtt = 0;
    std::string method;
    // Bloqueante breve (~1.2s max): evita hilos y carreras en LVGL.
    bool ok = cbdos::network::LanScannerService::getInstance().pingSingle(
        *ip, 1200, &rtt, &method);
    char msg[96];
    if (ok) {
        if (method == "icmp" && rtt > 0) {
            snprintf(msg, sizeof(msg), "%s OK %s %ums", ip->c_str(),
                     method.c_str(), static_cast<unsigned>(rtt));
        } else {
            snprintf(msg, sizeof(msg), "%s OK (%s)", ip->c_str(), method.c_str());
        }
    } else {
        snprintf(msg, sizeof(msg), "%s sin respuesta", ip->c_str());
    }
    UIManager::showToast(msg);
}

// ── Refresco reactivo ─────────────────────────────────────────────

void LanReconView::refreshFromService(bool force) {
    if (!m_container || !lv_obj_is_valid(m_container)) return;
    auto& svc = cbdos::network::LanScannerService::getInstance();
    cbdos::network::LanScanProgress prog = svc.getProgress();
    bool scanning = svc.isScanning();

    int pct = static_cast<int>(prog.percentage);
    int phaseInt = static_cast<int>(prog.phase);

    if (m_bar && lv_obj_is_valid(m_bar)) {
        lv_bar_set_value(m_bar, pct, LV_ANIM_OFF);
    }
    if (m_lblPhase && lv_obj_is_valid(m_lblPhase)) {
        lv_label_set_text(m_lblPhase, phaseToText(prog.phase));
    }

    // IP local + mascara + gateway (backend o fallback).
    if (m_lblNetInfo && lv_obj_is_valid(m_lblNetInfo)) {
        auto* backend = cbdos::network::getLanScannerBackend();
        if (backend) {
            std::string info = "IP " + backend->getLocalIp() + "  ·  " + backend->getSubnetMask();
            std::string gw = backend->getGatewayIp();
            if (!gw.empty()) {
                info += "  ·  GW " + gw;
            }
            std::string cidr = svc.lastCidr();
            if (!cidr.empty()) {
                info += "\nObjetivo: " + cidr;
            }
            lv_label_set_text(m_lblNetInfo, info.c_str());
        } else {
            lv_label_set_text(m_lblNetInfo, "Sin red (backend no disponible)");
        }
    }

    // La lista solo se reconstruye si cambia el numero de hosts, la fase,
    // el porcentaje o el estado de escaneo (evita parpadeos a 60 FPS).
    std::vector<cbdos::network::LanHostInfo> hosts = svc.getResults();
    bool changed = force || m_firstRefresh || hosts.size() != m_lastCount ||
                   pct != m_lastPct || phaseInt != m_lastPhaseInt || scanning != m_lastScanning;
    if (changed) {
        m_lastCount = hosts.size();
        m_lastPct = pct;
        m_lastPhaseInt = phaseInt;
        m_lastScanning = scanning;
        m_firstRefresh = false;

        if (m_lblCount && lv_obj_is_valid(m_lblCount)) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%u host%s", static_cast<unsigned>(hosts.size()),
                     hosts.size() == 1 ? "" : "s");
            lv_label_set_text(m_lblCount, buf);
        }
        updateScanButton(scanning);
        rebuildHostList(hosts);
    }
}

void LanReconView::updateScanButton(bool scanning) {
    if (m_lblScan && lv_obj_is_valid(m_lblScan)) {
        lv_label_set_text(m_lblScan, scanning ? "Detener" : "Iniciar Escaneo");
    }
    if (m_btnScan && lv_obj_is_valid(m_btnScan)) {
        lv_obj_set_style_bg_color(m_btnScan,
                                  scanning ? lv_color_hex(0xEF4444) : lv_color_hex(kAccent), 0);
    }
}

void LanReconView::rebuildHostList(const std::vector<cbdos::network::LanHostInfo>& hosts) {
    if (!m_list || !lv_obj_is_valid(m_list)) return;
    lv_obj_clean(m_list);

    if (hosts.empty()) {
        lv_obj_t* empty = lv_label_create(m_list);
        bool scanning = cbdos::network::LanScannerService::getInstance().isScanning();
        lv_label_set_text(empty, scanning ? "Escaneando la red local..." : "Sin hosts. Pulsa Iniciar Escaneo.");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x64748B), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, 0);
        return;
    }

    for (const auto& host : hosts) {
        lv_obj_t* card = lv_obj_create(m_list);
        lv_obj_set_width(card, LV_PCT(100));
        lv_obj_set_height(card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(kCardBg), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(kCardBorder), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_style_pad_row(card, 6, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        DefaultTheme::disableScroll(card);

        // Fila superior: icono + IP + fabricante.
        lv_obj_t* topRow = lv_obj_create(card);
        lv_obj_set_size(topRow, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(topRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(topRow, 0, 0);
        lv_obj_set_style_pad_all(topRow, 0, 0);
        lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(topRow, 8, 0);
        DefaultTheme::disableScroll(topRow);

        lv_obj_t* icon = lv_label_create(topRow);
        lv_label_set_text(icon, hostIcon(host));
        lv_obj_set_style_text_color(icon, lv_color_hex(kAccent), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);

        lv_obj_t* ipLbl = lv_label_create(topRow);
        lv_label_set_text(ipLbl, host.ip.c_str());
        lv_obj_set_style_text_color(ipLbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(ipLbl, &lv_font_montserrat_14, 0);

        lv_obj_t* vendorLbl = lv_label_create(card);
        std::string sub = host.vendor.empty() ? std::string("Desconocido") : host.vendor;
        sub += "  ·  " + host.getMacString();
        lv_label_set_text(vendorLbl, sub.c_str());
        lv_obj_set_style_text_color(vendorLbl, lv_color_hex(0x94A3B8), 0);
        lv_obj_set_style_text_font(vendorLbl, &lv_font_montserrat_12, 0);

        // v2: linea de descubrimiento (metodo + RTT + SERVER SSDP).
        if (!host.discovery.empty() || host.rttMs > 0 || !host.ssdpServer.empty() ||
            host.isTv) {
            lv_obj_t* discLbl = lv_label_create(card);
            std::string disc;
            if (host.isTv) {
                disc += "[TV] ";
            }
            if (!host.discovery.empty()) {
                disc += host.discovery;
            }
            if (host.rttMs > 0) {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), " %ums", static_cast<unsigned>(host.rttMs));
                disc += tmp;
            }
            if (!host.ssdpServer.empty()) {
                std::string srv = host.ssdpServer;
                if (srv.size() > 48) {
                    srv.resize(48);
                }
                disc += " · " + srv;
            }
            lv_label_set_text(discLbl, disc.c_str());
            lv_obj_set_style_text_color(discLbl, lv_color_hex(0x64748B), 0);
            lv_obj_set_style_text_font(discLbl, &lv_font_montserrat_12, 0);
        }

        // Chips de puertos abiertos.
        lv_obj_t* chipsRow = lv_obj_create(card);
        lv_obj_set_size(chipsRow, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(chipsRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chipsRow, 0, 0);
        lv_obj_set_style_pad_all(chipsRow, 0, 0);
        lv_obj_set_style_pad_row(chipsRow, 4, 0);
        lv_obj_set_style_pad_column(chipsRow, 4, 0);
        lv_obj_set_flex_flow(chipsRow, LV_FLEX_FLOW_ROW_WRAP);
        DefaultTheme::disableScroll(chipsRow);

        for (size_t i = 0; i < cbdos::network::kLanReconPortCount; ++i) {
            uint16_t port = cbdos::network::kLanReconPorts[i];
            if (!host.hasPort(port)) continue;
            lv_obj_t* chip = lv_obj_create(chipsRow);
            lv_obj_set_size(chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(chip, lv_color_hex(kChipBg), 0);
            lv_obj_set_style_border_color(chip, lv_color_hex(kAccent), 0);
            lv_obj_set_style_border_width(chip, 1, 0);
            lv_obj_set_style_radius(chip, 8, 0);
            lv_obj_set_style_pad_all(chip, 4, 0);
            lv_obj_set_style_pad_hor(chip, 8, 0);
            DefaultTheme::disableScroll(chip);
            lv_obj_t* chipLbl = lv_label_create(chip);
            char buf[32];
            snprintf(buf, sizeof(buf), "%u %s", port, portShortName(port));
            lv_label_set_text(chipLbl, buf);
            lv_obj_set_style_text_color(chipLbl, lv_color_hex(kAccent), 0);
            lv_obj_set_style_text_font(chipLbl, &lv_font_montserrat_12, 0);
        }

        // Banner / titulo web si existe.
        if (!host.banner.empty()) {
            lv_obj_t* bannerLbl = lv_label_create(card);
            lv_label_set_text(bannerLbl, host.banner.c_str());
            lv_obj_set_style_text_color(bannerLbl, lv_color_hex(0x64748B), 0);
            lv_obj_set_style_text_font(bannerLbl, &lv_font_montserrat_12, 0);
        }

        // Acciones por host (sin escribir): Ping siempre, SSH si puerto 22.
        {
            bool wantSsh = host.hasPort(22);
            lv_obj_t* actRow = lv_obj_create(card);
            lv_obj_set_size(actRow, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(actRow, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(actRow, 0, 0);
            lv_obj_set_style_pad_all(actRow, 0, 0);
            lv_obj_set_style_pad_column(actRow, 6, 0);
            lv_obj_set_flex_flow(actRow, LV_FLEX_FLOW_ROW);
            DefaultTheme::disableScroll(actRow);

            lv_obj_t* pingBtn = lv_button_create(actRow);
            lv_obj_set_flex_grow(pingBtn, 1);
            lv_obj_set_height(pingBtn, 38);
            lv_obj_set_style_bg_color(pingBtn, lv_color_hex(0x1F293D), 0);
            lv_obj_set_style_border_color(pingBtn, lv_color_hex(kAccent), 0);
            lv_obj_set_style_border_width(pingBtn, 1, 0);
            lv_obj_set_style_radius(pingBtn, 8, 0);
            auto* pingIp = new std::string(host.ip);
            lv_obj_add_event_cb(pingBtn, pingHostBtnCb, LV_EVENT_CLICKED, pingIp);
            lv_obj_add_event_cb(pingBtn, pingHostBtnCb, LV_EVENT_DELETE, pingIp);
            lv_obj_t* pingLbl = lv_label_create(pingBtn);
            lv_label_set_text(pingLbl, "Ping");
            lv_obj_set_style_text_color(pingLbl, lv_color_hex(kAccent), 0);
            lv_obj_center(pingLbl);

            if (wantSsh) {
                lv_obj_t* sshBtn = lv_button_create(actRow);
                lv_obj_set_flex_grow(sshBtn, 1);
                lv_obj_set_height(sshBtn, 38);
                lv_obj_set_style_bg_color(sshBtn, lv_color_hex(0x1F293D), 0);
                lv_obj_set_style_border_color(sshBtn, lv_color_hex(kAccent), 0);
                lv_obj_set_style_border_width(sshBtn, 1, 0);
                lv_obj_set_style_radius(sshBtn, 8, 0);
                auto* ipCopy = new std::string(host.ip);
                lv_obj_add_event_cb(sshBtn, sshBtnCb, LV_EVENT_CLICKED, ipCopy);
                lv_obj_add_event_cb(sshBtn, sshBtnCb, LV_EVENT_DELETE, ipCopy);
                lv_obj_t* sshLbl = lv_label_create(sshBtn);
                lv_label_set_text(sshLbl, ">_ Conectar SSH");
                lv_obj_set_style_text_color(sshLbl, lv_color_hex(kAccent), 0);
                lv_obj_center(sshLbl);
            }
        }
    }
}

const char* LanReconView::phaseToText(cbdos::network::LanScanPhase phase) {
    using cbdos::network::LanScanPhase;
    switch (phase) {
        case LanScanPhase::Idle: return "Listo para escanear";
        case LanScanPhase::ArpSweep: return "Barrido ARP...";
        case LanScanPhase::PortScan: return "Escaneando puertos...";
        case LanScanPhase::BannerGrab: return "Capturando banners...";
        case LanScanPhase::Completed: return "Completado";
        case LanScanPhase::Aborted: return "Escaneo detenido";
        default: return "Listo para escanear";
    }
}

const char* LanReconView::hostIcon(const cbdos::network::LanHostInfo& host) {
    // Gateway probable (.1 / .254): icono router.
    size_t dot = host.ip.rfind('.');
    if (dot != std::string::npos) {
        std::string tail = host.ip.substr(dot + 1);
        if (tail == "1" || tail == "254") return LV_SYMBOL_WIFI;
    }
    // v2: TV detectada por SSDP/vendor/puertos cast.
    if (host.isTv) return LV_SYMBOL_IMAGE;
    if (host.hasPort(554)) return LV_SYMBOL_EYE_OPEN;
    if (host.hasPort(80) || host.hasPort(443) || host.hasPort(8080) || host.hasPort(8443)) {
        return LV_SYMBOL_EYE_OPEN;
    }
    if (host.hasPort(22)) return LV_SYMBOL_KEYBOARD;
    return LV_SYMBOL_FILE;
}

const char* LanReconView::portShortName(uint16_t port) {
    switch (port) {
        case 22: return "SSH";
        case 23: return "TELNET";
        case 80: return "HTTP";
        case 443: return "HTTPS";
        case 554: return "RTSP";
        case 3389: return "RDP";
        case 8080: return "HTTP-alt";
        case 8443: return "HTTPS-alt";
        case 1900: return "SSDP";
        case 21: return "FTP";
        case 135: return "RPC";
        case 139: return "NetBIOS";
        case 445: return "SMB";
        case 1400: return "Sonos";
        case 7000: return "AirPlay";
        case 8008: return "Cast";
        case 8009: return "Cast-TLS";
        case 9100: return "Print";
        case 1883: return "MQTT";
        case 5357: return "WSD";
        default: return "";
    }
}

} // namespace ui
} // namespace cbdos
