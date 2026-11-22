#include "KvmClientService.hpp"
#include <cstdio>
#include <cstring>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char* TAG = "KvmService";
#endif

namespace cbdos {
namespace kvm {

static constexpr size_t DEFAULT_FRAME_BUFFER_CAP = 512 * 1024; // 512 KB en PSRAM

KvmClientService::KvmClientService() {
#if defined(ESP_PLATFORM)
    m_mutex = xSemaphoreCreateMutex();
    m_videoReassemblyCap = DEFAULT_FRAME_BUFFER_CAP;
    m_videoReassemblyBuf = static_cast<uint8_t*>(
        heap_caps_malloc(m_videoReassemblyCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (!m_videoReassemblyBuf) {
        // Fallback a heap estándar si no hay PSRAM
        m_videoReassemblyBuf = static_cast<uint8_t*>(malloc(64 * 1024));
        m_videoReassemblyCap = m_videoReassemblyBuf ? 64 * 1024 : 0;
    }
#endif
}

KvmClientService::~KvmClientService() {
    disconnect();
#if defined(ESP_PLATFORM)
    if (m_videoReassemblyBuf) {
        free(m_videoReassemblyBuf);
        m_videoReassemblyBuf = nullptr;
    }
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
#endif
}

void KvmClientService::updateState(KvmState state, const std::string& info) {
    m_state = state;
    if (m_stateCb) {
        m_stateCb(state, info);
    }
}

bool KvmClientService::connect(const std::string& host, uint16_t port, bool useSsl) {
    if (m_state == KvmState::Connected || m_state == KvmState::Connecting) {
        disconnect();
    }

    m_host   = host;
    m_port   = port;
    m_useSsl = useSsl;
    updateState(KvmState::Connecting, "Iniciando conexión a " + host);

#if defined(ESP_PLATFORM)
    char uriWs[128];
    char uriVideo[128];
    const char* proto = useSsl ? "wss" : "ws";

    snprintf(uriWs, sizeof(uriWs), "%s://%s:%u/ws", proto, host.c_str(), port);
    snprintf(uriVideo, sizeof(uriVideo), "%s://%s:%u/video", proto, host.c_str(), port);

    ESP_LOGI(TAG, "Conectando a control: %s y video: %s", uriWs, uriVideo);

    // 1. Configurar WebSocket de Control (/ws)
    esp_websocket_client_config_t cfgWs = {};
    cfgWs.uri = uriWs;
    cfgWs.buffer_size = 2048;
    cfgWs.disable_auto_reconnect = false;
    cfgWs.reconnect_timeout_ms = 3000;
    cfgWs.network_timeout_ms = 5000;
    if (useSsl) {
        cfgWs.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
        cfgWs.skip_cert_common_name_check = true;
    }

    m_wsControl = esp_websocket_client_init(&cfgWs);
    if (!m_wsControl) {
        ESP_LOGE(TAG, "Fallo al inicializar websocket de control");
        updateState(KvmState::Failed, "Error al crear socket de control");
        return false;
    }

    esp_websocket_register_events(m_wsControl, WEBSOCKET_EVENT_ANY, wsControlHandler, this);

    // 2. Configurar WebSocket de Video (/video)
    esp_websocket_client_config_t cfgVid = {};
    cfgVid.uri = uriVideo;
    cfgVid.buffer_size = 16384; // Buffer TCP amplio para fragmentos de video
    cfgVid.disable_auto_reconnect = false;
    cfgVid.reconnect_timeout_ms = 3000;
    cfgVid.network_timeout_ms = 5000;
    if (useSsl) {
        cfgVid.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
        cfgVid.skip_cert_common_name_check = true;
    }

    m_wsVideo = esp_websocket_client_init(&cfgVid);
    if (!m_wsVideo) {
        ESP_LOGE(TAG, "Fallo al inicializar websocket de video");
        esp_websocket_client_destroy(m_wsControl);
        m_wsControl = nullptr;
        updateState(KvmState::Failed, "Error al crear socket de video");
        return false;
    }

    esp_websocket_register_events(m_wsVideo, WEBSOCKET_EVENT_ANY, wsVideoHandler, this);

    // 3. Iniciar clientes
    esp_err_t errWs  = esp_websocket_client_start(m_wsControl);
    esp_err_t errVid = esp_websocket_client_start(m_wsVideo);

    if (errWs != ESP_OK || errVid != ESP_OK) {
        ESP_LOGE(TAG, "Error iniciando websockets: ws=%d, vid=%d", errWs, errVid);
        disconnect();
        updateState(KvmState::Failed, "Error al conectar sockets");
        return false;
    }

    return true;
#else
    updateState(KvmState::Connected, "Modo simulado / Host");
    return true;
#endif
}

void KvmClientService::disconnect() {
#if defined(ESP_PLATFORM)
    if (m_wsControl) {
        esp_websocket_client_stop(m_wsControl);
        esp_websocket_client_destroy(m_wsControl);
        m_wsControl = nullptr;
    }
    if (m_wsVideo) {
        esp_websocket_client_stop(m_wsVideo);
        esp_websocket_client_destroy(m_wsVideo);
        m_wsVideo = nullptr;
    }
#endif
    m_inControl = false;
    m_currentFps = 0.0f;
    m_rttMs = 0;
    updateState(KvmState::Disconnected, "Desconectado");
}

bool KvmClientService::sendMouseAbs(uint8_t buttons, uint16_t x, uint16_t y, int8_t wheel, int8_t pan) {
#if defined(ESP_PLATFORM)
    if (!m_wsControl || !esp_websocket_client_is_connected(m_wsControl)) return false;
    uint8_t pkt[8];
    buildMouseAbs(pkt, buttons, x, y, wheel, pan);
    return esp_websocket_client_send_bin(m_wsControl, reinterpret_cast<const char*>(pkt), sizeof(pkt), pdMS_TO_TICKS(100)) > 0;
#else
    return true;
#endif
}

bool KvmClientService::sendMouseRel(uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel, int8_t pan) {
#if defined(ESP_PLATFORM)
    if (!m_wsControl || !esp_websocket_client_is_connected(m_wsControl)) return false;
    uint8_t pkt[8];
    buildMouseRel(pkt, buttons, dx, dy, wheel, pan);
    return esp_websocket_client_send_bin(m_wsControl, reinterpret_cast<const char*>(pkt), sizeof(pkt), pdMS_TO_TICKS(100)) > 0;
#else
    return true;
#endif
}

bool KvmClientService::sendKeyboard(uint8_t modifier, const uint8_t keys[6]) {
#if defined(ESP_PLATFORM)
    if (!m_wsControl || !esp_websocket_client_is_connected(m_wsControl)) return false;
    uint8_t pkt[8];
    buildKeyboard(pkt, modifier, keys);
    return esp_websocket_client_send_bin(m_wsControl, reinterpret_cast<const char*>(pkt), sizeof(pkt), pdMS_TO_TICKS(100)) > 0;
#else
    return true;
#endif
}

bool KvmClientService::sendKeyCombo(uint8_t modifier, uint8_t key) {
    uint8_t keys[6] = {key, 0, 0, 0, 0, 0};
    bool ok1 = sendKeyboard(modifier, keys);
    bool ok2 = sendKeyboard(0, nullptr); // Liberar
    return ok1 && ok2;
}

bool KvmClientService::sendCtrlAltDel() {
    // Left Ctrl = 0x01, Left Alt = 0x04. Delete usage = 0x4C
    uint8_t del[6] = {0x4C, 0, 0, 0, 0, 0};
    bool ok1 = sendKeyboard(0x01 | 0x04, del);
    bool ok2 = sendKeyboard(0, nullptr);
    return ok1 && ok2;
}

bool KvmClientService::sendTakeover() {
#if defined(ESP_PLATFORM)
    if (!m_wsControl || !esp_websocket_client_is_connected(m_wsControl)) return false;
    uint8_t pkt = WS_C2D_TAKEOVER;
    return esp_websocket_client_send_bin(m_wsControl, reinterpret_cast<const char*>(&pkt), 1, pdMS_TO_TICKS(100)) > 0;
#else
    return true;
#endif
}

bool KvmClientService::sendPing() {
#if defined(ESP_PLATFORM)
    if (!m_wsControl || !esp_websocket_client_is_connected(m_wsControl)) return false;
    m_pingSentUs = esp_timer_get_time();
    uint8_t pkt = WS_C2D_PING;
    return esp_websocket_client_send_bin(m_wsControl, reinterpret_cast<const char*>(&pkt), 1, pdMS_TO_TICKS(100)) > 0;
#else
    return true;
#endif
}

bool KvmClientService::sendVideoAck(uint32_t seq) {
#if defined(ESP_PLATFORM)
    if (!m_wsVideo || !esp_websocket_client_is_connected(m_wsVideo)) return false;
    uint8_t ack[5];
    buildVideoAck(ack, seq);
    return esp_websocket_client_send_bin(m_wsVideo, reinterpret_cast<const char*>(ack), sizeof(ack), pdMS_TO_TICKS(50)) > 0;
#else
    return true;
#endif
}

#if defined(ESP_PLATFORM)
void KvmClientService::wsControlHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    auto* self = static_cast<KvmClientService*>(handler_args);
    auto* data = static_cast<esp_websocket_event_data_t*>(event_data);

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket /ws conectado");
            // Tomar control al conectar
            self->sendTakeover();
            self->sendPing();
            if (self->m_wsVideo && esp_websocket_client_is_connected(self->m_wsVideo)) {
                self->updateState(KvmState::Connected, "Sesión KVM establecida");
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket /ws desconectado");
            self->updateState(KvmState::Disconnected, "Canal de control desconectado");
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->data_len > 0 && data->data_ptr) {
                self->onControlData(reinterpret_cast<const uint8_t*>(data->data_ptr), data->data_len);
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en WebSocket /ws");
            break;

        default:
            break;
    }
}

void KvmClientService::wsVideoHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    auto* self = static_cast<KvmClientService*>(handler_args);
    auto* data = static_cast<esp_websocket_event_data_t*>(event_data);

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "WebSocket /video conectado");
            // Suscribirse a frames de imagen
            uint8_t sub = WS_SUBSCRIBE_PIC;
            esp_websocket_client_send_bin(self->m_wsVideo, reinterpret_cast<const char*>(&sub), 1, pdMS_TO_TICKS(100));
            if (self->m_wsControl && esp_websocket_client_is_connected(self->m_wsControl)) {
                self->updateState(KvmState::Connected, "Sesión KVM establecida");
            }
            break;
        }

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket /video desconectado");
            break;

        case WEBSOCKET_EVENT_DATA:
            self->onVideoChunk(data);
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en WebSocket /video");
            break;

        default:
            break;
    }
}

void KvmClientService::onControlData(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    uint8_t opcode = data[0];
    switch (opcode) {
        case WS_D2C_PONG: {
            if (m_pingSentUs > 0) {
                int64_t now = esp_timer_get_time();
                m_rttMs = static_cast<int>((now - m_pingSentUs) / 1000);
            }
            break;
        }
        case WS_D2C_CONTROL: {
            if (len >= 2) {
                uint8_t ctrlState = data[1];
                m_inControl = (ctrlState == CTRL_YOU);
            }
            break;
        }
        case WS_D2C_STATUS: {
            if (len >= 2) {
                // flags = data[1]
            }
            break;
        }
        default:
            break;
    }

    if (m_telemetryCb) {
        m_telemetryCb(m_currentFps.load(), m_rttMs.load(), m_inControl.load());
    }
}

void KvmClientService::onVideoChunk(const esp_websocket_event_data_t* d) {
    if (!d || !m_videoReassemblyBuf || d->data_len <= 0) return;

    // Notificación de que el servidor acepta ACKs (primer frame o saludo)
    if (d->payload_len == 1 && d->data_len == 1 && static_cast<uint8_t>(d->data_ptr[0]) == WS_D2C_VIDEO_ACKS) {
        ESP_LOGI(TAG, "Servidor KVM habilitó ventana de ACKs");
        return;
    }

    if (d->payload_offset == 0) {
        m_videoReassemblyLen = 0;
    }

    // Verificar desbordamiento de búfer
    if (d->payload_offset + d->data_len > m_videoReassemblyCap) {
        ESP_LOGW(TAG, "Frame KVM descartado: excede capacidad (%d bytes)", d->payload_len);
        m_videoReassemblyLen = 0;
        return;
    }

    std::memcpy(m_videoReassemblyBuf + d->payload_offset, d->data_ptr, d->data_len);
    m_videoReassemblyLen = d->payload_offset + d->data_len;

    // Cuadro completo reensamblado
    if (m_videoReassemblyLen == static_cast<size_t>(d->payload_len)) {
        KvmVideoHdr hdr;
        if (KvmVideoHdr::read(m_videoReassemblyBuf, m_videoReassemblyLen, hdr)) {
            // Confirmar inmediatamente con ACK para mantener la ventana de flujo
            sendVideoAck(hdr.seq);

            // Calcular FPS cada segundo
            m_frameCounter++;
            int64_t now = esp_timer_get_time();
            if (now - m_lastFpsCalcUs >= 1000000) {
                m_currentFps = static_cast<float>(m_frameCounter * 1000000.0f / (now - m_lastFpsCalcUs));
                m_frameCounter = 0;
                m_lastFpsCalcUs = now;
                // Enviar un ping de telemetría por segundo
                sendPing();
            }

            // Notificar frame listo para decodificación
            if (m_frameCb) {
                const uint8_t* jpegPayload = m_videoReassemblyBuf + VIDEO_HDR_LEN;
                size_t jpegLen = m_videoReassemblyLen - VIDEO_HDR_LEN;
                m_frameCb(hdr, jpegPayload, jpegLen);
            }
        }
    }
}
#endif

} // namespace kvm
} // namespace cbdos

