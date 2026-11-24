#include "KvmClientView.hpp"
#include "UiMetrics.hpp"
#include "UIManager.hpp"
#include "themes/DefaultTheme.h"
#include "ThemeEngine.hpp"
#include <cstdio>
#include <cstring>

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char* TAG = "KvmView";
#endif

namespace cbdos {
namespace ui {

KvmClientView::KvmClientView()
    : BaseView("KVM Client", ViewMode::Solid, ViewOrientation::Landscape) {
    m_canvasW = 800;
    m_canvasH = 432; // Deja espacio para el HeaderBar superior (48px)
}

KvmClientView::~KvmClientView() {
    onDestroy();
}

void KvmClientView::onDestroy() {
    UIManager::closeKeyboard();

    auto& svc = kvm::KvmClientService::getInstance();
    svc.setFrameCallback(nullptr);
    svc.setStateCallback(nullptr);
    svc.setTelemetryCallback(nullptr);
    svc.disconnect();

    m_decoder.deinit();

    if (m_canvasBuf) {
#if defined(ESP_PLATFORM)
        free(m_canvasBuf);
#else
        delete[] m_canvasBuf;
#endif
        m_canvasBuf = nullptr;
    }

    m_canvasObj    = nullptr;
    m_toolbar      = nullptr;
    m_connectModal = nullptr;
    m_taHost       = nullptr;
    m_taPort       = nullptr;
    m_lblStats     = nullptr;
    m_taKvmInput   = nullptr;

    BaseView::onDestroy();
}

bool KvmClientView::onCreate(lv_obj_t* parent) {
    if (!parent) return false;

    UIManager::getInstance().getHeaderBar().setTitle("KVM Client (Panorámico)");

    const auto& palette = ThemeEngine::getInstance().getPalette();

    // Contenedor principal a pantalla completa panorámica (800x480)
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(0x050505), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 0, 0);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Asignar búfer de canvas RGB565 en PSRAM (800 x 432 px = 691,200 bytes)
    size_t bufSize = m_canvasW * m_canvasH * sizeof(uint16_t);
#if defined(ESP_PLATFORM)
    m_canvasBuf = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(128, bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
#else
    m_canvasBuf = new uint8_t[bufSize];
#endif

    if (m_canvasBuf) {
        std::memset(m_canvasBuf, 0x11, bufSize); // Fondo oscuro inicial
    }

    // 2. Descriptor de imagen para LVGL 9.5
    m_imgDsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    m_imgDsc.header.cf     = LV_COLOR_FORMAT_RGB565;
    m_imgDsc.header.w      = m_canvasW;
    m_imgDsc.header.h      = m_canvasH;
    m_imgDsc.header.stride = m_canvasW * sizeof(uint16_t);
    m_imgDsc.data          = m_canvasBuf;
    m_imgDsc.data_size     = bufSize;

    // 3. Objeto de video interactivo
    m_canvasObj = lv_image_create(m_container);
    lv_obj_set_size(m_canvasObj, m_canvasW, m_canvasH);
    lv_obj_set_pos(m_canvasObj, 0, 0);
    lv_image_set_src(m_canvasObj, &m_imgDsc);
    lv_obj_add_flag(m_canvasObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(m_canvasObj, touchCanvasCb, LV_EVENT_ALL, this);

    // 4. Inicializar decodificador JPEG por hardware P4
    m_decoder.init(1920, 1088);

    // 5. Crear Toolbar Flotante inferior
    createToolbar(m_container);

    // 6. Conectar callbacks con KvmClientService
    auto& svc = kvm::KvmClientService::getInstance();

    svc.setFrameCallback([this](const kvm::KvmVideoHdr& hdr, const uint8_t* jpegData, size_t len) {
        if (!m_canvasBuf || !m_canvasObj) return;

        bool ok = m_decoder.decodeAndScale(jpegData, len, m_canvasBuf, m_canvasW, m_canvasH);
        if (ok) {
#if defined(ESP_PLATFORM)
            if (lvgl_port_lock(pdMS_TO_TICKS(10))) {
                lv_obj_invalidate(m_canvasObj);
                lvgl_port_unlock();
            }
#else
            lv_obj_invalidate(m_canvasObj);
#endif
        }
    });

    svc.setTelemetryCallback([this](float fps, int rttMs, bool inControl) {
        updateTelemetryUi(fps, rttMs, inControl);
    });

    svc.setStateCallback([this](kvm::KvmState state, const std::string& info) {
#if defined(ESP_PLATFORM)
        if (lvgl_port_lock(pdMS_TO_TICKS(10))) {
#endif
            if (state == kvm::KvmState::Connected) {
                if (m_connectModal && lv_obj_is_valid(m_connectModal)) {
                    lv_obj_set_hidden(m_connectModal, true);
                }
                UIManager::showToast("KVM: Conectado");
            } else if (state == kvm::KvmState::Failed || state == kvm::KvmState::Disconnected) {
                if (m_connectModal && lv_obj_is_valid(m_connectModal)) {
                    lv_obj_set_hidden(m_connectModal, false);
                }
            }
#if defined(ESP_PLATFORM)
            lvgl_port_unlock();
        }
#endif
    });

    // 7. Si no está conectado, mostrar modal de conexión inicial
    if (!svc.isConnected()) {
        createConnectDialog(m_container);
    }

    return true;
}

void KvmClientView::createToolbar(lv_obj_t* parent) {
    // 1. Textarea flotante para enviar texto al KVM (oculto hasta presionar [KBD])
    m_taKvmInput = lv_textarea_create(parent);
    lv_obj_set_size(m_taKvmInput, lv_pct(90), 36);
    lv_obj_set_style_max_width(m_taKvmInput, 480, 0);
    lv_obj_align(m_taKvmInput, LV_ALIGN_BOTTOM_LEFT, 10, -50);
    DefaultTheme::applyTextArea(m_taKvmInput, 6);
    lv_textarea_set_one_line(m_taKvmInput, true);
    lv_textarea_set_placeholder_text(m_taKvmInput, "Escribe aquí para enviar al KVM...");
    lv_obj_set_style_text_font(m_taKvmInput, &lv_font_montserrat_12, 0);
    lv_obj_set_hidden(m_taKvmInput, true);

    UIManager::attachKeyboard(m_taKvmInput, {
        .autoCloseOnSubmit = true,
        .onSubmit = [this](const char*) {
            // Al presionar Enter en el teclado CBDos, enviar Enter al KVM
            kvm::KvmClientService::getInstance().sendKeyCombo(0x00, 0x28);
            if (m_taKvmInput) {
                lv_obj_set_hidden(m_taKvmInput, true);
                m_virtualKbdVisible = false;
            }
        }
    });

    lv_obj_add_event_cb(m_taKvmInput, kbdInputCb, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(m_taKvmInput, [](lv_event_t* e) {
        auto* self = static_cast<KvmClientView*>(lv_event_get_user_data(e));
        if (self && self->m_virtualKbdVisible) {
            self->m_virtualKbdVisible = false;
            if (self->m_taKvmInput) {
                lv_obj_set_hidden(self->m_taKvmInput, true);
            }
        }
    }, LV_EVENT_DEFOCUSED, this);

    // 2. Barra de herramientas flotante
    m_toolbar = lv_obj_create(parent);
    lv_obj_set_size(m_toolbar, lv_pct(96), 42);
    lv_obj_set_style_max_width(m_toolbar, 960, 0);
    lv_obj_align(m_toolbar, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(m_toolbar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(m_toolbar, LV_OPA_80, 0);
    lv_obj_set_style_border_color(m_toolbar, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(m_toolbar, 1, 0);
    lv_obj_set_style_radius(m_toolbar, 8, 0);
    lv_obj_set_style_pad_all(m_toolbar, 4, 0);
    lv_obj_set_flex_flow(m_toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto makeBtn = [this](const char* text, lv_event_cb_t cb, uint32_t color = 0x334155) -> lv_obj_t* {
        lv_obj_t* btn = lv_button_create(m_toolbar);
        lv_obj_set_size(btn, LV_SIZE_CONTENT, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_set_style_pad_ver(btn, 4, 0);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        return btn;
    };

    makeBtn("CAD", btnCadCb, 0xDC2626); // Rojo alerta para Ctrl+Alt+Del
    makeBtn("WIN", btnSuperCb);
    makeBtn("ESC", btnEscCb);
    makeBtn("TAB", btnTabCb);
    makeBtn("KBD", btnKbdCb, 0x2563EB); // Azul para teclado virtual CBDos

    // Etiqueta de telemetría (FPS y RTT)
    m_lblStats = lv_label_create(m_toolbar);
    lv_label_set_text(m_lblStats, "FPS: 0.0 | RTT: -- ms");
    lv_obj_set_style_text_font(m_lblStats, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblStats, lv_color_hex(0x94A3B8), 0);

    // Botón desconectar / salir
    makeBtn("SALIR", [](lv_event_t*) {
        UIManager::closeKeyboard();
        UIManager::getInstance().popView();
    }, 0x475569);
}

void KvmClientView::createConnectDialog(lv_obj_t* parent) {
    const auto metrics = UiMetrics::current();
    m_connectModal = lv_obj_create(parent);
    applyAdaptiveModalSize(m_connectModal, metrics, 480, 420);
    lv_obj_center(m_connectModal);
    lv_obj_set_style_bg_color(m_connectModal, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(m_connectModal, lv_color_hex(0x0284C7), 0);
    lv_obj_set_style_border_width(m_connectModal, 2, 0);
    lv_obj_set_style_radius(m_connectModal, 12, 0);
    lv_obj_set_style_pad_all(m_connectModal, 16, 0);
    lv_obj_set_flex_flow(m_connectModal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m_connectModal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m_connectModal, 10, 0);

    lv_obj_t* title = lv_label_create(m_connectModal);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Conectar a Servidor KVM");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);

    // Campo IP Host con tema CBDos oscuro
    m_taHost = lv_textarea_create(m_connectModal);
    lv_obj_set_size(m_taHost, lv_pct(100), 42);
    DefaultTheme::applyTextArea(m_taHost, 8);
    lv_textarea_set_one_line(m_taHost, true);
    lv_textarea_set_placeholder_text(m_taHost, "IP Host (ej: 192.168.1.150)");
    lv_textarea_set_text(m_taHost, "192.168.1.150");
    lv_obj_set_style_text_font(m_taHost, &lv_font_montserrat_14, 0);

    // Campo Puerto con tema CBDos oscuro
    m_taPort = lv_textarea_create(m_connectModal);
    lv_obj_set_size(m_taPort, lv_pct(100), 42);
    DefaultTheme::applyTextArea(m_taPort, 8);
    lv_textarea_set_one_line(m_taPort, true);
    lv_textarea_set_placeholder_text(m_taPort, "Puerto (ej: 80 o 443)");
    lv_textarea_set_text(m_taPort, "80");
    lv_obj_set_style_text_font(m_taPort, &lv_font_montserrat_14, 0);

    UIManager::attachKeyboard(m_taHost, {
        .autoCloseOnSubmit = false,
        .nextFocusTarget = m_taPort
    });
    UIManager::attachKeyboard(m_taPort, {
        .autoCloseOnSubmit = true,
        .onSubmit = [this](const char*) {
            btnConnectCb(nullptr);
        }
    });

    // Fila de Botones: Cancelar / Conectar
    lv_obj_t* btnRow = lv_obj_create(m_connectModal);
    lv_obj_set_size(btnRow, lv_pct(100), 44);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* btnCancel = lv_button_create(btnRow);
    lv_obj_set_size(btnCancel, 140, 40);
    DefaultTheme::applyButton(btnCancel, 8);
    lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0x475569), 0);
    lv_obj_add_event_cb(btnCancel, [](lv_event_t*) {
        UIManager::closeKeyboard();
        UIManager::getInstance().popView();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lblCancel = lv_label_create(btnCancel);
    lv_label_set_text(lblCancel, LV_SYMBOL_LEFT " Cancelar");
    lv_obj_center(lblCancel);

    lv_obj_t* btnConnect = lv_button_create(btnRow);
    lv_obj_set_size(btnConnect, 210, 40);
    DefaultTheme::applyButton(btnConnect, 8);
    lv_obj_add_event_cb(btnConnect, btnConnectCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblBtn = lv_label_create(btnConnect);
    lv_label_set_text(lblBtn, LV_SYMBOL_PLAY " Iniciar Sesión");
    lv_obj_center(lblBtn);
}

void KvmClientView::updateTelemetryUi(float fps, int rttMs, bool inControl) {
    if (!m_lblStats || !lv_obj_is_valid(m_lblStats)) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "FPS: %.1f | RTT: %d ms %s",
             fps, rttMs, inControl ? "[CTRL]" : "[VIEW]");

#if defined(ESP_PLATFORM)
    if (lvgl_port_lock(pdMS_TO_TICKS(10))) {
        lv_label_set_text(m_lblStats, buf);
        lvgl_port_unlock();
    }
#else
    lv_label_set_text(m_lblStats, buf);
#endif
}

void KvmClientView::touchCanvasCb(lv_event_t* e) {
    auto* self = static_cast<KvmClientView*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    // Normalizar coordenadas a rango 0 .. 32767
    int32_t clampedX = pt.x < 0 ? 0 : (pt.x >= (int32_t)self->m_canvasW ? self->m_canvasW - 1 : pt.x);
    int32_t clampedY = pt.y < 0 ? 0 : (pt.y >= (int32_t)self->m_canvasH ? self->m_canvasH - 1 : pt.y);

    uint16_t normX = static_cast<uint16_t>((clampedX * kvm::KVM_ABS_MAX) / self->m_canvasW);
    uint16_t normY = static_cast<uint16_t>((clampedY * kvm::KVM_ABS_MAX) / self->m_canvasH);

    auto& svc = kvm::KvmClientService::getInstance();

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        svc.sendMouseAbs(0x01, normX, normY); // Clic izquierdo presionado
    } else if (code == LV_EVENT_RELEASED) {
        svc.sendMouseAbs(0x00, normX, normY); // Liberado
    }
}

void KvmClientView::btnCadCb(lv_event_t* e) {
    kvm::KvmClientService::getInstance().sendCtrlAltDel();
    UIManager::showToast("KVM: Macro Ctrl+Alt+Del enviada");
}

void KvmClientView::btnSuperCb(lv_event_t* e) {
    // Tecla Windows / GUI (Usage 0xE3)
    kvm::KvmClientService::getInstance().sendKeyCombo(0x08, 0xE3);
}

void KvmClientView::btnEscCb(lv_event_t* e) {
    // Escape (Usage 0x29)
    kvm::KvmClientService::getInstance().sendKeyCombo(0x00, 0x29);
}

void KvmClientView::btnTabCb(lv_event_t* e) {
    // Tab (Usage 0x2B)
    kvm::KvmClientService::getInstance().sendKeyCombo(0x00, 0x2B);
}

void KvmClientView::btnKbdCb(lv_event_t* e) {
    auto* self = static_cast<KvmClientView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taKvmInput) return;

    if (!self->m_virtualKbdVisible) {
        lv_obj_set_hidden(self->m_taKvmInput, false);
        UIManager::openKeyboard(self->m_taKvmInput);
        self->m_virtualKbdVisible = true;
    } else {
        UIManager::closeKeyboard();
        lv_obj_set_hidden(self->m_taKvmInput, true);
        self->m_virtualKbdVisible = false;
    }
}

void KvmClientView::kbdInputCb(lv_event_t* e) {
    auto* self = static_cast<KvmClientView*>(lv_event_get_user_data(e));
    if (!self || !self->m_taKvmInput) return;

    const char* txt = lv_textarea_get_text(self->m_taKvmInput);
    if (!txt || strlen(txt) == 0) return;

    size_t len = strlen(txt);
    for (size_t i = 0; i < len; i++) {
        char c = txt[i];
        uint8_t usage = 0;
        uint8_t mod = 0;

        if (c >= 'a' && c <= 'z') {
            usage = 0x04 + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            usage = 0x04 + (c - 'A');
            mod = 0x02; // Left Shift
        } else if (c >= '1' && c <= '9') {
            usage = 0x1E + (c - '1');
        } else if (c == '0') {
            usage = 0x27;
        } else if (c == '\n' || c == '\r') {
            usage = 0x28; // Enter
        } else if (c == ' ') {
            usage = 0x2C; // Space
        } else if (c == '-') {
            usage = 0x2D;
        } else if (c == '=') {
            usage = 0x2E;
        } else if (c == '.') {
            usage = 0x37;
        } else if (c == '/') {
            usage = 0x38;
        } else if (c == ':') {
            usage = 0x33;
            mod = 0x02;
        }

        if (usage > 0) {
            kvm::KvmClientService::getInstance().sendKeyCombo(mod, usage);
        }
    }

    // Vaciar el campo para que continúe capturando teclas
    lv_textarea_set_text(self->m_taKvmInput, "");
}

void KvmClientView::btnConnectCb(lv_event_t* e) {
    auto* self = (e != nullptr) ? static_cast<KvmClientView*>(lv_event_get_user_data(e)) : nullptr;
    if (!self) {
        auto currentView = UIManager::getInstance().getCurrentView();
        self = static_cast<KvmClientView*>(currentView.get());
    }
    if (!self || !self->m_taHost || !self->m_taPort) return;

    UIManager::closeKeyboard();

    const char* host = lv_textarea_get_text(self->m_taHost);
    const char* portStr = lv_textarea_get_text(self->m_taPort);
    uint16_t port = static_cast<uint16_t>(atoi(portStr));
    if (port == 0) port = 80;

    bool useSsl = (port == 443);
    UIManager::showToast("Conectando...");
    kvm::KvmClientService::getInstance().connect(host, port, useSsl);
}

} // namespace ui
} // namespace cbdos
