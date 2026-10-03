#include "KerberosView.hpp"
#include "../../ui/themes/DefaultTheme.h"
#include "../../ui/UIManager.hpp"
#include "../../ui/components/HeaderBar.hpp"
#include "cbdos/hid.hpp"
#include "cbdos/display.hpp"
#include "cbdos/system.hpp"
#include "cbdos/usb_manager.hpp"
#include <cstdio>

namespace cbdos {
namespace ui {

KerberosView::KerberosView()
    : BaseView("Kerberos") {
}

bool KerberosView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    // Configurar cabecera del sistema
    UIManager::getInstance().getHeaderBar().setTitle("Kerberos FIDO2");
    UIManager::getInstance().getHeaderBar().showWifi(false);

    // Contenedor principal de la vista
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_hor(m_container, 12, 0);
    lv_obj_set_style_pad_ver(m_container, 10, 0);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    buildMainLayout(m_container);

    // Inicializar y habilitar token USB FIDO2 en hardware si el sistema arrancó en modo FIDO2
    security::KerberosManager::instance().init();
    if (cbdos::usb::UsbManager::getInstance().getBootMode() == cbdos::usb::UsbMode::Fido) {
        cbdos::hid::enable();
    } else {
        UIManager::showToast("Activa modo FIDO2 en Ajustes > Modo USB");
    }

    return true;
}

void KerberosView::onDestroy() {
    BaseView::onDestroy();
}

void KerberosView::onShow() {
    BaseView::onShow();
    UIManager::getInstance().getHeaderBar().setTitle("Kerberos FIDO2");
    UIManager::getInstance().getHeaderBar().showWifi(false);
    updateUIState();
}

void KerberosView::onHide() {
    BaseView::onHide();
}

void KerberosView::onUpdate() {
    updateUIState();
}

void KerberosView::onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {
    if (m_container && lv_obj_is_valid(m_container)) {
        lv_obj_set_style_bg_color(m_container, DefaultTheme::getBgColor(), 0);
    }
}

void KerberosView::buildMainLayout(lv_obj_t* parent) {
    // 1. Tarjeta de Estado del Token FIDO2
    buildStatusCard(parent);

    // 2. Tarjeta Interactiva de Presencia y Aprobación
    buildPromptCard(parent);
}

void KerberosView::buildStatusCard(lv_obj_t* parent) {
    m_statusCard = lv_obj_create(parent);
    lv_obj_set_size(m_statusCard, LV_PCT(100), 160);
    lv_obj_set_style_bg_color(m_statusCard, lv_color_hex(0x131722), 0);
    lv_obj_set_style_border_color(m_statusCard, lv_color_hex(0x242D40), 0);
    lv_obj_set_style_border_width(m_statusCard, 1, 0);
    lv_obj_set_style_radius(m_statusCard, 12, 0);
    lv_obj_set_style_pad_all(m_statusCard, 10, 0);
    lv_obj_set_flex_flow(m_statusCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_statusCard, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Fila superior: Badge y Título
    lv_obj_t* rowHeader = lv_obj_create(m_statusCard);
    lv_obj_set_size(rowHeader, LV_PCT(100), 28);
    lv_obj_set_style_bg_opa(rowHeader, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rowHeader, 0, 0);
    lv_obj_set_style_pad_all(rowHeader, 0, 0);
    lv_obj_set_flex_flow(rowHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rowHeader, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lblTitle = lv_label_create(rowHeader);
    lv_label_set_text(lblTitle, "🛡️ KERBEROS FIDO2 / CTAP2");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(0x00E5FF), 0);

    m_lblUsbBadge = lv_label_create(rowHeader);
    lv_label_set_text(m_lblUsbBadge, "● LISTO");
    lv_obj_set_style_text_font(m_lblUsbBadge, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblUsbBadge, lv_color_hex(0x10B981), 0);

    // Detalles Criptográficos
    m_lblDetails = lv_label_create(m_statusCard);
    lv_label_set_text(m_lblDetails, "AAGUID: KERBEROSPOSEIDON\nCurva: P-256 (secp256r1) | Cifrado: AES-256-GCM");
    lv_obj_set_style_text_font(m_lblDetails, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblDetails, lv_color_hex(0x94A3B8), 0);

    // Estado del Bus USB
    m_lblUsbStatus = lv_label_create(m_statusCard);
    lv_label_set_text(m_lblUsbStatus, "USB: Esperando solicitud del navegador...");
    lv_obj_set_style_text_font(m_lblUsbStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblUsbStatus, lv_color_hex(0xFBBF24), 0);

    // Contador de Firmas y Tráfico
    m_lblCounter = lv_label_create(m_statusCard);
    lv_label_set_text(m_lblCounter, "🔑 Firmas NVS: 0 | Paquetes USB: 0");
    lv_obj_set_style_text_font(m_lblCounter, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblCounter, lv_color_hex(0x64748B), 0);
}

void KerberosView::buildPromptCard(lv_obj_t* parent) {
    m_promptCard = lv_obj_create(parent);
    lv_obj_set_size(m_promptCard, LV_PCT(100), 210);
    lv_obj_set_style_bg_color(m_promptCard, lv_color_hex(0x161C2E), 0);
    lv_obj_set_style_border_color(m_promptCard, lv_color_hex(0x2A3752), 0);
    lv_obj_set_style_border_width(m_promptCard, 2, 0);
    lv_obj_set_style_radius(m_promptCard, 14, 0);
    lv_obj_set_style_pad_all(m_promptCard, 12, 0);
    lv_obj_set_flex_flow(m_promptCard, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_promptCard, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Título de la Solicitud
    m_lblPromptTitle = lv_label_create(m_promptCard);
    lv_label_set_text(m_lblPromptTitle, "ESPERANDO SOLICITUD WEBAUTHN");
    lv_obj_set_style_text_font(m_lblPromptTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(m_lblPromptTitle, lv_color_hex(0x64748B), 0);

    // Dominio / Sitio Web
    m_lblRpId = lv_label_create(m_promptCard);
    lv_label_set_text(m_lblRpId, "Conecta el USB y pulsa 'Registrar' o 'Entrar' en el navegador.");
    lv_obj_set_style_text_font(m_lblRpId, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblRpId, lv_color_hex(0x94A3B8), 0);
    lv_label_set_long_mode(m_lblRpId, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_lblRpId, LV_PCT(95));
    lv_obj_set_style_text_align(m_lblRpId, LV_TEXT_ALIGN_CENTER, 0);

    // Tipo de Operación
    m_lblOpType = lv_label_create(m_promptCard);
    lv_label_set_text(m_lblOpType, "Modo: FIDO2 / CTAP2 Passkeys");
    lv_obj_set_style_text_font(m_lblOpType, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblOpType, lv_color_hex(0x64748B), 0);

    // Fila de Botones [Rechazar] y [Aprobar]
    lv_obj_t* btnRow = lv_obj_create(m_promptCard);
    lv_obj_set_size(btnRow, LV_PCT(100), 50);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Botón Rechazar
    m_btnDeny = lv_button_create(btnRow);
    lv_obj_set_size(m_btnDeny, 120, 42);
    lv_obj_set_style_bg_color(m_btnDeny, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_radius(m_btnDeny, 10, 0);
    lv_obj_add_event_cb(m_btnDeny, denyButtonClickedCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblDeny = lv_label_create(m_btnDeny);
    lv_label_set_text(lblDeny, "✕ Rechazar");
    lv_obj_set_style_text_font(lblDeny, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDeny, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblDeny);

    // Botón Aprobar
    m_btnApprove = lv_button_create(btnRow);
    lv_obj_set_size(m_btnApprove, 130, 42);
    lv_obj_set_style_bg_color(m_btnApprove, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(m_btnApprove, 10, 0);
    lv_obj_add_event_cb(m_btnApprove, approveButtonClickedCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblApprove = lv_label_create(m_btnApprove);
    lv_label_set_text(lblApprove, "✓ Aprobar");
    lv_obj_set_style_text_font(lblApprove, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblApprove, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lblApprove);

    // Botones siempre listos y activos
}

void KerberosView::updateUIState() {
    auto &mgr = security::KerberosManager::instance();
    bool isPending = mgr.isPresencePending();
    uint32_t pkts = mgr.getPacketsReceivedCount();
    uint32_t counter = mgr.getSignatureCounter();

    // Actualizar badges e información USB
    if (m_lblUsbStatus && m_lblUsbBadge) {
        if (cbdos::usb::UsbManager::getInstance().getBootMode() != cbdos::usb::UsbMode::Fido) {
            lv_label_set_text(m_lblUsbStatus, "🔴 Requiere Modo USB FIDO2 (cambiar en Ajustes)");
            lv_obj_set_style_text_color(m_lblUsbStatus, lv_color_hex(0xEF4444), 0);
            lv_label_set_text(m_lblUsbBadge, "● INACTIVO");
            lv_obj_set_style_text_color(m_lblUsbBadge, lv_color_hex(0xEF4444), 0);
        } else if (isPending) {
            lv_label_set_text(m_lblUsbBadge, "⚠️ SOLICITUD");
            lv_obj_set_style_text_color(m_lblUsbBadge, lv_color_hex(0xFFB703), 0);
            lv_label_set_text(m_lblUsbStatus, "🟢 Petición WebAuthn recibida del Host");
            lv_obj_set_style_text_color(m_lblUsbStatus, lv_color_hex(0x00E5FF), 0);
        } else if (pkts > 0) {
            char sbuf[64];
            snprintf(sbuf, sizeof(sbuf), "🟢 Conectado: %lu pkts (Cmd: 0x%02X)", (unsigned long)pkts, mgr.getLastCommand());
            lv_label_set_text(m_lblUsbStatus, sbuf);
            lv_obj_set_style_text_color(m_lblUsbStatus, lv_color_hex(0x10B981), 0);
            lv_label_set_text(m_lblUsbBadge, "● EN LÍNEA");
            lv_obj_set_style_text_color(m_lblUsbBadge, lv_color_hex(0x10B981), 0);
        } else {
            lv_label_set_text(m_lblUsbStatus, "🟡 USB: Esperando tráfico FIDO2 del navegador...");
            lv_obj_set_style_text_color(m_lblUsbStatus, lv_color_hex(0xFBBF24), 0);
            lv_label_set_text(m_lblUsbBadge, "● LISTO");
            lv_obj_set_style_text_color(m_lblUsbBadge, lv_color_hex(0x94A3B8), 0);
        }
    }

    // Actualizar métricas del contador
    if (m_lblCounter && (pkts != m_lastPktCount)) {
        m_lastPktCount = pkts;
        char cbuf[64];
        snprintf(cbuf, sizeof(cbuf), "🔑 Firmas NVS: %lu | Paquetes USB: %lu", 
                 (unsigned long)counter, (unsigned long)pkts);
        lv_label_set_text(m_lblCounter, cbuf);
    }

    // Estado dinámico del cuadro de presencia
    if (isPending != m_lastPendingState) {
        m_lastPendingState = isPending;
        if (isPending) {
            const auto &req = mgr.getPresenceRequest();
            char rbuf[96];
            snprintf(rbuf, sizeof(rbuf), "Sitio Web: %s", req.rpId.empty() ? "Navegador Web" : req.rpId.c_str());
            lv_label_set_text(m_lblRpId, rbuf);

            if (req.isRegistration) {
                lv_label_set_text(m_lblPromptTitle, "🔑 NUEVO REGISTRO DE PASSKEY");
                lv_label_set_text(m_lblOpType, "Operación: Crear credencial pública");
            } else {
                lv_label_set_text(m_lblPromptTitle, "⚠️ SOLICITUD DE ACCESO WEBAUTHN");
                lv_label_set_text(m_lblOpType, "Operación: Firma de inicio de sesión");
            }

            lv_obj_set_style_border_color(m_promptCard, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_text_color(m_lblPromptTitle, lv_color_hex(0x00E5FF), 0);
        } else {
            lv_label_set_text(m_lblPromptTitle, "ESPERANDO SOLICITUD WEBAUTHN");
            lv_label_set_text(m_lblRpId, "Conecta el USB y pulsa 'Registrar' o 'Entrar' en el navegador.");
            lv_label_set_text(m_lblOpType, "Modo: FIDO2 / CTAP2 Passkeys");
            lv_obj_set_style_border_color(m_promptCard, lv_color_hex(0x2A3752), 0);
            lv_obj_set_style_text_color(m_lblPromptTitle, lv_color_hex(0x64748B), 0);
        }
    }
}

void KerberosView::approveButtonClickedCb(lv_event_t* e) {
    security::KerberosManager::instance().approvePresence();
}

void KerberosView::denyButtonClickedCb(lv_event_t* e) {
    security::KerberosManager::instance().denyPresence();
}

} // namespace ui
} // namespace cbdos
