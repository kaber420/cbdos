#pragma once

#include "KvmProto.h"
#include <string>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <atomic>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_websocket_client.h"
#endif

namespace cbdos {
namespace kvm {

enum class KvmState {
    Disconnected,
    Connecting,
    Connected,
    Failed
};

class KvmClientService {
public:
    using StateCallback     = std::function<void(KvmState state, const std::string& info)>;
    using FrameCallback     = std::function<void(const KvmVideoHdr& hdr, const uint8_t* jpegData, size_t len)>;
    using TelemetryCallback = std::function<void(float fps, int rttMs, bool inControl)>;

    static KvmClientService& getInstance() {
        static KvmClientService instance;
        return instance;
    }

    bool connect(const std::string& host, uint16_t port = 443, bool useSsl = false);
    void disconnect();

    bool isConnected() const { return m_state == KvmState::Connected; }
    KvmState getState() const { return m_state; }
    const std::string& getHost() const { return m_host; }

    void setStateCallback(StateCallback cb) { m_stateCb = cb; }
    void setFrameCallback(FrameCallback cb) { m_frameCb = cb; }
    void setTelemetryCallback(TelemetryCallback cb) { m_telemetryCb = cb; }

    // Métodos de control HID
    bool sendMouseAbs(uint8_t buttons, uint16_t x, uint16_t y, int8_t wheel = 0, int8_t pan = 0);
    bool sendMouseRel(uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel = 0, int8_t pan = 0);
    bool sendKeyboard(uint8_t modifier, const uint8_t keys[6]);
    bool sendKeyCombo(uint8_t modifier, uint8_t key);
    bool sendCtrlAltDel();
    bool sendTakeover();
    bool sendPing();
    bool sendVideoAck(uint32_t seq);

    // Telemetría
    float getFps() const { return m_currentFps; }
    int   getRttMs() const { return m_rttMs; }
    bool  isInControl() const { return m_inControl; }

private:
    KvmClientService();
    ~KvmClientService();

    KvmClientService(const KvmClientService&) = delete;
    KvmClientService& operator=(const KvmClientService&) = delete;

    void updateState(KvmState state, const std::string& info);

#if defined(ESP_PLATFORM)
    static void wsControlHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    static void wsVideoHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);

    void onControlData(const uint8_t* data, size_t len);
    void onVideoChunk(const esp_websocket_event_data_t* d);

    esp_websocket_client_handle_t m_wsControl{nullptr};
    esp_websocket_client_handle_t m_wsVideo{nullptr};
    SemaphoreHandle_t             m_mutex{nullptr};

    uint8_t* m_videoReassemblyBuf{nullptr};
    size_t   m_videoReassemblyLen{0};
    size_t   m_videoReassemblyCap{0};
#endif

    std::string m_host;
    uint16_t    m_port{443};
    bool        m_useSsl{false};

    std::atomic<KvmState> m_state{KvmState::Disconnected};
    std::atomic<bool>     m_inControl{false};
    std::atomic<float>    m_currentFps{0.0f};
    std::atomic<int>      m_rttMs{0};

    uint32_t m_frameCounter{0};
    int64_t  m_lastFpsCalcUs{0};
    int64_t  m_pingSentUs{0};

    StateCallback     m_stateCb{nullptr};
    FrameCallback     m_frameCb{nullptr};
    TelemetryCallback m_telemetryCb{nullptr};
};

} // namespace kvm
} // namespace cbdos

