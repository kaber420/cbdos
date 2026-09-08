#pragma once

#include "meshcore_types.hpp"
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace meshcore {

// Cliente del Companion Protocol oficial de MeshCore (faming USB-serie
// '<'/'#' + longitud de 1 byte). Sin dependencias de plataforma salvo
// cbdos::serial; apto para ESP32 y para pruebas en host.
class MeshCoreClient {
public:
    static MeshCoreClient& getInstance();

    // ── Conexión ────────────────────────────────────────────────
    bool connect(const std::string& portId = "jp1", uint32_t baudrate = 115200);
    void disconnect();
    bool isConnected() const;
    // true si el dongle respondió al handshake (SELF_INFO recibido).
    // isConnected() solo garantiza puerto serie abierto; esto confirma
    // que del otro lado hay un firmware MeshCore real respondiendo.
    bool hasHandshake() const;
    std::string getActivePort() const;

    // ── Bomba de RX: drena el serial y procesa bytes ────────────
    void process();
    // Procesa un byte (expuesto para pruebas e integración).
    void processByte(uint8_t byte);

    // ── Handshake / consultas ───────────────────────────────────
    bool sendAppStart(const std::string& appName = "CBDos");
    bool queryDeviceInfo();
    bool queryBattery();

    // ── Mensajes ────────────────────────────────────────────────
    // Envía texto a un canal (trunca a 133 caracteres según spec).
    bool sendChannelMessage(uint8_t channelIdx, const std::string& text);
    // Pide el siguiente mensaje encolado (CMD_GET_MESSAGE).
    bool getNextMessage();
    // Inicia/continúa el drenado de la cola del dongle.
    bool pollMessages();
    // Drenado automático al recibir MESSAGES_WAITING o mensajes.
    void setAutoDrain(bool enabled);

    // ── Canales ─────────────────────────────────────────────────
    bool getChannel(uint8_t idx);
    bool queryAllChannels();
    bool setChannel(uint8_t idx, const std::string& name, const uint8_t secret[CHANNEL_SECRET_LEN]);
    bool clearChannel(uint8_t idx);
    // secret_hex: 32 caracteres hexadecimales.
    bool setChannelHex(uint8_t idx, const std::string& name, const std::string& secretHex);
    // Crea canal hashtag: secreto = sha256(nombre)[0..16).
    bool setHashtagChannel(uint8_t idx, const std::string& name);

    // ── Configuración por CLI de texto (misma vía serial) ───────
    bool sendCliCommand(const std::string& cmd);
    bool setName(const std::string& name);
    bool setRadio(uint32_t freqMhz, uint32_t bwKhz, uint8_t sf, uint8_t cr);
    bool setTxPower(int txPowerDbm);
    bool setFrequency(uint32_t freqMhz);
    bool reboot();

    // ── Getters de estado ───────────────────────────────────────
    const SelfInfo& getSelfInfo() const;
    const DeviceInfo& getDeviceInfo() const;
    const BatteryInfo& getBattery() const;
    const MeshChannel& getChannelInfo(uint8_t idx) const;
    const std::vector<ChannelMessage>& getChannelHistory() const;
    const std::vector<ContactMessage>& getContactHistory() const;
    const MsgSentInfo& getLastMsgSent() const;
    bool hasLastMsgSent() const;
    ErrCode getLastError() const;
    bool hasLastError() const;
    void clearLastError();
    void clearHistory();

    // Utilidades estáticas (también usadas por la UI y pruebas)
    static bool parseSecretHex(const std::string& hex, uint8_t out[CHANNEL_SECRET_LEN]);
    static std::string secretToHex(const uint8_t secret[CHANNEL_SECRET_LEN]);
    static const char* channelKindName(ChannelKind kind);

    // ── Callbacks reactivos (cero polling) ──────────────────────
    void setOnChannelMessage(std::function<void(const ChannelMessage&)> cb);
    void setOnContactMessage(std::function<void(const ContactMessage&)> cb);
    void setOnSelfInfo(std::function<void(const SelfInfo&)> cb);
    void setOnDeviceInfo(std::function<void(const DeviceInfo&)> cb);
    void setOnChannelInfo(std::function<void(const MeshChannel&)> cb);
    void setOnBattery(std::function<void(const BatteryInfo&)> cb);
    void setOnMsgSent(std::function<void(const MsgSentInfo&)> cb);
    void setOnError(std::function<void(ErrCode)> cb);
    void setOnMessagesWaiting(std::function<void()> cb);
    void setOnConnectionState(std::function<void(bool connected)> cb);

private:
    MeshCoreClient();
    ~MeshCoreClient();
    MeshCoreClient(const MeshCoreClient&) = delete;
    MeshCoreClient& operator=(const MeshCoreClient&) = delete;

    enum class ParseState {
        WaitFrameStart,  // esperar '>' (0x3E); resto = basura CLI, se ignora
        WaitLenLow,      // byte bajo de longitud u16 LE
        WaitLenHigh,     // byte alto de longitud u16 LE
        WaitPayload,     // payload completo
    };

    void handleFrame(const std::vector<uint8_t>& payload);
    void handleSelfInfo(const std::vector<uint8_t>& data);
    void handleDeviceInfo(const std::vector<uint8_t>& data);
    void handleChannelMsg(const std::vector<uint8_t>& data, bool v3);
    void handleContactMsg(const std::vector<uint8_t>& data, bool v3);
    void handleChannelInfo(const std::vector<uint8_t>& data);
    void handleBattery(const std::vector<uint8_t>& data);
    void handleMsgSent(const std::vector<uint8_t>& data);
    void handleOk(const std::vector<uint8_t>& data);
    void handleError(const std::vector<uint8_t>& data);
    void pushHistory(const ChannelMessage& msg);

    static ChannelKind classifyChannel(const std::string& name,
                                       const uint8_t secret[CHANNEL_SECRET_LEN]);

    // Envío de frames '<' + len + payload
    bool sendFrame(Cmd cmd, const uint8_t* extra = nullptr, size_t extraLen = 0);
    bool sendText(const std::string& text);

    // Estado del cliente
    bool m_connected = false;
    std::string m_activePort = "jp1";
    uint32_t m_baudrate = 115200;
    ParseState m_parseState = ParseState::WaitFrameStart;
    uint16_t m_rxPayloadLen = 0;
    std::vector<uint8_t> m_rxPayload;
    bool m_autoDrain = true;
    bool m_drainPending = false;
    // Reintento de handshake: el dongle suele reiniciarse al abrir el CDC,
    // así que el primer APP_START puede perderse durante su arranque.
    // process() se llama cada ~40 ms desde la UI.
    uint32_t m_ticksSinceHandshake = 0;
    bool m_hadHandshake = false;
    static constexpr uint32_t HANDSHAKE_RETRY_TICKS = 50;  // ~2 s

    // Datos
    SelfInfo m_selfInfo;
    DeviceInfo m_deviceInfo;
    BatteryInfo m_battery;
    MeshChannel m_channels[CHANNEL_COUNT];
    std::vector<ChannelMessage> m_channelHistory;
    std::vector<ContactMessage> m_contactHistory;
    MsgSentInfo m_lastMsgSent;
    bool m_hasMsgSent = false;
    ErrCode m_lastError = ErrCode::UNSUPPORTED_CMD;
    bool m_hasError = false;

    // Callbacks
    std::function<void(const ChannelMessage&)> m_onChannelMsg;
    std::function<void(const ContactMessage&)> m_onContactMsg;
    std::function<void(const SelfInfo&)> m_onSelfInfo;
    std::function<void(const DeviceInfo&)> m_onDeviceInfo;
    std::function<void(const MeshChannel&)> m_onChannelInfo;
    std::function<void(const BatteryInfo&)> m_onBattery;
    std::function<void(const MsgSentInfo&)> m_onMsgSent;
    std::function<void(ErrCode)> m_onError;
    std::function<void()> m_onMessagesWaiting;
    std::function<void(bool connected)> m_onConnState;
};

} // namespace meshcore
} // namespace cbdos
