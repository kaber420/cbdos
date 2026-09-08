#include "cbdos/meshcore/meshcore_client.hpp"
#include "cbdos/serial.hpp"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <algorithm>

namespace cbdos {
namespace meshcore {

namespace {

// ── Lecturas little-endian con control de límites ──
uint16_t rdU16(const std::vector<uint8_t>& d, size_t off) {
    return static_cast<uint16_t>(d[off]) |
           (static_cast<uint16_t>(d[off + 1]) << 8);
}

uint32_t rdU32(const std::vector<uint8_t>& d, size_t off) {
    return static_cast<uint32_t>(d[off]) |
           (static_cast<uint32_t>(d[off + 1]) << 8) |
           (static_cast<uint32_t>(d[off + 2]) << 16) |
           (static_cast<uint32_t>(d[off + 3]) << 24);
}

bool allZero(const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (p[i] != 0) return false;
    }
    return true;
}

// strlen acotado para campos de longitud fija con padding nulo.
size_t boundedStrLen(const uint8_t* p, size_t maxLen) {
    size_t n = 0;
    while (n < maxLen && p[n] != 0) ++n;
    return n;
}

// Trunca UTF-8 sin partir secuencias multibyte.
std::string truncateUtf8(const std::string& s, size_t maxChars, size_t maxBytes) {
    size_t bytes = 0;
    size_t chars = 0;
    size_t i = 0;
    while (i < s.size() && chars < maxChars && bytes < maxBytes) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t seq = 1;
        if ((c & 0x80) == 0x00) seq = 1;
        else if ((c & 0xE0) == 0xC0) seq = 2;
        else if ((c & 0xF0) == 0xE0) seq = 3;
        else if ((c & 0xF8) == 0xF0) seq = 4;
        else seq = 1;  // byte de continuación huérfano: copiar tal cual
        if (i + seq > s.size()) break;
        if (bytes + seq > maxBytes) break;
        i += seq;
        bytes += seq;
        ++chars;
    }
    return s.substr(0, i);
}

// ── SHA-256 compacto (para derivar secretos de canal hashtag:
// secreto = sha256(nombre)[0..16)). ──
struct Sha256Ctx {
    uint32_t h[8];
    uint64_t totalLen;
    uint8_t buf[64];
    size_t bufLen;
};

uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

void sha256Init(Sha256Ctx& c) {
    c.h[0] = 0x6a09e667; c.h[1] = 0xbb67ae85;
    c.h[2] = 0x3c6ef372; c.h[3] = 0xa54ff53a;
    c.h[4] = 0x510e527f; c.h[5] = 0x9b05688c;
    c.h[6] = 0x1f83d9ab; c.h[7] = 0x5be0cd19;
    c.totalLen = 0;
    c.bufLen = 0;
}

void sha256Block(Sha256Ctx& c, const uint8_t* p) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(p[i * 4]) << 24) |
               (static_cast<uint32_t>(p[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(p[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(p[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
    uint32_t e = c.h[4], f = c.h[5], g = c.h[6], h = c.h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + K[i] + w[i];
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d;
    c.h[4] += e; c.h[5] += f; c.h[6] += g; c.h[7] += h;
}

void sha256Update(Sha256Ctx& c, const uint8_t* data, size_t len) {
    c.totalLen += len;
    while (len > 0) {
        size_t take = 64 - c.bufLen;
        if (take > len) take = len;
        memcpy(c.buf + c.bufLen, data, take);
        c.bufLen += take;
        data += take;
        len -= take;
        if (c.bufLen == 64) {
            sha256Block(c, c.buf);
            c.bufLen = 0;
        }
    }
}

void sha256Final(Sha256Ctx& c, uint8_t out[32]) {
    uint64_t bitLen = c.totalLen * 8;
    uint8_t pad = 0x80;
    sha256Update(c, &pad, 1);
    uint8_t zero = 0x00;
    while (c.bufLen != 56) sha256Update(c, &zero, 1);
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; ++i) {
        lenBytes[i] = static_cast<uint8_t>((bitLen >> (56 - i * 8)) & 0xFF);
    }
    sha256Update(c, lenBytes, 8);
    for (int i = 0; i < 8; ++i) {
        out[i * 4]     = static_cast<uint8_t>((c.h[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((c.h[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((c.h[i] >> 8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>(c.h[i] & 0xFF);
    }
}

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    Sha256Ctx c;
    sha256Init(c);
    sha256Update(c, data, len);
    sha256Final(c, out);
}

} // namespace

// ────────────────────────────────────────────────────────────────
// Singleton
// ────────────────────────────────────────────────────────────────

MeshCoreClient& MeshCoreClient::getInstance() {
    static MeshCoreClient s_instance;
    return s_instance;
}

MeshCoreClient::MeshCoreClient() {
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        m_channels[i].index = i;
    }
}

MeshCoreClient::~MeshCoreClient() {
    disconnect();
}

// ────────────────────────────────────────────────────────────────
// Conexión
// ────────────────────────────────────────────────────────────────

bool MeshCoreClient::connect(const std::string& portId, uint32_t baudrate) {
    if (m_connected && m_activePort == portId && m_baudrate == baudrate) {
        return true;
    }

    cbdos::serial::SerialConfig cfg;
    cfg.portId = portId;
    cfg.baudrate = baudrate;

    if (portId == "jp1") {
        cfg.txPin = 32;
        cfg.rxPin = 28;
        cfg.controlPin = 54;
    } else {
        cfg.txPin = -1;
        cfg.rxPin = -1;
        cfg.controlPin = -1;
    }

    if (!cbdos::serial::open(cfg)) {
        m_connected = false;
        if (m_onConnState) m_onConnState(false);
        return false;
    }

    m_connected = true;
    m_activePort = portId;
    m_baudrate = baudrate;
    m_parseState = ParseState::WaitFrameStart;
    m_rxPayload.clear();
    m_rxPayloadLen = 0;
    m_drainPending = false;
    m_hasError = false;
    m_ticksSinceHandshake = 0;
    m_hadHandshake = false;
    // Invalidar estado anterior: si hay datos visibles, son del dongle nuevo.
    m_selfInfo = SelfInfo();
    m_deviceInfo = DeviceInfo();
    m_battery = BatteryInfo();
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        m_channels[i] = MeshChannel();
        m_channels[i].index = i;
    }

    // Handshake oficial: APP_START primero, luego DEVICE_QUERY.
    sendAppStart();
    queryDeviceInfo();

    if (m_onConnState) m_onConnState(true);
    return true;
}

void MeshCoreClient::disconnect() {
    if (m_connected) {
        cbdos::serial::close();
        m_connected = false;
        if (m_onConnState) m_onConnState(false);
    }
}

bool MeshCoreClient::isConnected() const {
    return m_connected && cbdos::serial::isOpen();
}

bool MeshCoreClient::hasHandshake() const {
    return isConnected() && m_selfInfo.valid;
}

std::string MeshCoreClient::getActivePort() const {
    return m_activePort;
}

// ────────────────────────────────────────────────────────────────
// Bomba de RX y parser de 3 estados
// ────────────────────────────────────────────────────────────────

void MeshCoreClient::process() {
    if (!isConnected()) {
        m_hadHandshake = false;
        m_ticksSinceHandshake = 0;
        return;
    }

    size_t avail = cbdos::serial::available();
    if (avail > 0) {
        uint8_t buffer[256];
        while (avail > 0) {
            size_t toRead = std::min(avail, sizeof(buffer));
            size_t nRead = cbdos::serial::read(buffer, toRead);
            if (nRead == 0) break;
            for (size_t i = 0; i < nRead; ++i) processByte(buffer[i]);
            avail -= nRead;
        }
    }

    // Transición a handshake: descubrimiento inicial automático.
    if (m_selfInfo.valid && !m_hadHandshake) {
        m_hadHandshake = true;
        queryDeviceInfo();
        queryBattery();
        queryAllChannels();
        m_drainPending = true;
    }

    // Sin handshake: reintentar APP_START cada ~2 s (el dongle puede
    // estar aún arrancando tras el reset que provoca abrir el CDC).
    if (!m_selfInfo.valid) {
        if (++m_ticksSinceHandshake >= HANDSHAKE_RETRY_TICKS) {
            m_ticksSinceHandshake = 0;
            sendAppStart();
            queryDeviceInfo();
        }
    }

    // Drenado automático de la cola del dongle.
    if (m_autoDrain && m_drainPending && isConnected()) {
        m_drainPending = false;
        sendFrame(Cmd::GET_MESSAGE);
    }
}

void MeshCoreClient::processByte(uint8_t byte) {
    switch (m_parseState) {
        case ParseState::WaitFrameStart:
            if (byte == FRAME_CHAR_RADIO_TO_APP) {
                m_parseState = ParseState::WaitLenLow;
            }
            // Cualquier otro byte es basura CLI / texto: se ignora.
            break;

        case ParseState::WaitLenLow:
            m_rxPayloadLen = byte;
            m_parseState = ParseState::WaitLenHigh;
            break;

        case ParseState::WaitLenHigh:
            m_rxPayloadLen |= static_cast<uint16_t>(byte) << 8;
            m_rxPayload.clear();
            if (m_rxPayloadLen == 0 || m_rxPayloadLen > FRAME_MAX_PAYLOAD) {
                // Frame vacío o longitud anómala: descartar y resincronizar.
                m_parseState = ParseState::WaitFrameStart;
            } else {
                m_rxPayload.reserve(m_rxPayloadLen);
                m_parseState = ParseState::WaitPayload;
            }
            break;

        case ParseState::WaitPayload:
            m_rxPayload.push_back(byte);
            if (m_rxPayload.size() >= m_rxPayloadLen) {
                handleFrame(m_rxPayload);
                m_rxPayload.clear();
                m_parseState = ParseState::WaitFrameStart;
            }
            break;
    }
}

void MeshCoreClient::handleFrame(const std::vector<uint8_t>& payload) {
    if (payload.empty()) return;
    switch (static_cast<PacketType>(payload[0])) {
        case PacketType::OK:               handleOk(payload); break;
        case PacketType::ERROR:            handleError(payload); break;
        case PacketType::SELF_INFO:        handleSelfInfo(payload); break;
        case PacketType::MSG_SENT:         handleMsgSent(payload); break;
        case PacketType::CONTACT_MSG_RECV:    handleContactMsg(payload, false); break;
        case PacketType::CONTACT_MSG_RECV_V3: handleContactMsg(payload, true); break;
        case PacketType::CHANNEL_MSG_RECV:    handleChannelMsg(payload, false); break;
        case PacketType::CHANNEL_MSG_RECV_V3: handleChannelMsg(payload, true); break;
        case PacketType::NO_MORE_MSGS:
            m_drainPending = false;
            break;
        case PacketType::BATTERY:          handleBattery(payload); break;
        case PacketType::DEVICE_INFO:      handleDeviceInfo(payload); break;
        case PacketType::CHANNEL_INFO:     handleChannelInfo(payload); break;
        case PacketType::MESSAGES_WAITING:
            m_drainPending = true;
            if (m_onMessagesWaiting) m_onMessagesWaiting();
            break;
        case PacketType::CURRENT_TIME:
        case PacketType::CHANNEL_DATA_RECV:
        case PacketType::CONTACT_START:
        case PacketType::CONTACT:
        case PacketType::CONTACT_END:
        case PacketType::ADVERTISEMENT:
        case PacketType::ACK:
        case PacketType::LOG_DATA:
            m_drainPending = true;  // la cola puede contener más mensajes
            break;
    }
}

// ────────────────────────────────────────────────────────────────
// Parsers de respuestas
// ────────────────────────────────────────────────────────────────

void MeshCoreClient::handleSelfInfo(const std::vector<uint8_t>& d) {
    // Mínimo: tipo + adv/tx/max + pubkey(32) = 36 bytes.
    if (d.size() < 36) return;
    SelfInfo info;
    info.advType = d[1];
    info.txPowerDbm = static_cast<int8_t>(d[2]);
    info.maxTxPowerDbm = static_cast<int8_t>(d[3]);
    memcpy(info.publicKey, &d[4], PUBKEY_LEN);
    size_t off = 36;
    if (d.size() >= off + 8) {
        int32_t lat = static_cast<int32_t>(rdU32(d, off));
        int32_t lon = static_cast<int32_t>(rdU32(d, off + 4));
        info.advLatitude = static_cast<double>(lat) / 1e6;
        info.advLongitude = static_cast<double>(lon) / 1e6;
        off += 8;
    }
    if (d.size() >= off + 4) {
        info.multiAcks = d[off];
        info.advLocPolicy = d[off + 1];
        info.telemetryMode = d[off + 2];
        info.manualAddContacts = d[off + 3] != 0;
        off += 4;
    }
    if (d.size() >= off + 10) {
        info.frequencyHz = rdU32(d, off);
        info.bandwidthHz = rdU32(d, off + 4);
        info.spreadingFactor = d[off + 8];
        info.codingRate = d[off + 9];
        off += 10;
    }
    if (off < d.size()) {
        info.name.assign(reinterpret_cast<const char*>(&d[off]), d.size() - off);
        while (!info.name.empty() &&
               (info.name.back() == '\0' || info.name.back() == ' ')) {
            info.name.pop_back();
        }
    }
    info.valid = true;
    m_selfInfo = info;
    if (m_onSelfInfo) m_onSelfInfo(m_selfInfo);
}

void MeshCoreClient::handleDeviceInfo(const std::vector<uint8_t>& d) {
    if (d.size() < 2) return;
    DeviceInfo info;
    info.firmwareVersion = d[1];
    if (info.firmwareVersion >= 3 && d.size() >= 82) {
        info.maxContacts = static_cast<uint16_t>(d[2]) * 2;
        info.maxChannels = d[3];
        info.blePin = rdU32(d, 4);
        info.firmwareBuild.assign(reinterpret_cast<const char*>(&d[8]),
                                  boundedStrLen(&d[8], 12));
        info.model.assign(reinterpret_cast<const char*>(&d[20]),
                          boundedStrLen(&d[20], 40));
        info.version.assign(reinterpret_cast<const char*>(&d[60]),
                            boundedStrLen(&d[60], 20));
        if (d.size() >= 81) info.clientRepeat = d[80] != 0;
        if (d.size() >= 82) info.pathHashMode = d[81];
    }
    info.valid = true;
    m_deviceInfo = info;
    if (m_onDeviceInfo) m_onDeviceInfo(m_deviceInfo);
}

void MeshCoreClient::handleChannelMsg(const std::vector<uint8_t>& d, bool v3) {
    ChannelMessage msg;
    size_t off = 1;
    if (v3) {
        // [0x11][SNR][rsv][rsv][ch][path][txt][ts LE32][texto...]
        if (d.size() < 11) return;
        int8_t snrRaw = static_cast<int8_t>(d[1]);
        msg.snrDb = static_cast<float>(snrRaw) / 4.0f;
        msg.hasSnr = true;
        off = 4;
    } else {
        // [0x08][ch][path][txt][ts LE32][texto...]
        if (d.size() < 8) return;
    }
    if (d.size() < off + 7) return;
    msg.channelIndex = d[off];
    if (msg.channelIndex >= CHANNEL_COUNT) return;
    msg.pathLength = d[off + 1];
    msg.txtType = static_cast<TxtType>(d[off + 2]);
    msg.timestamp = rdU32(d, off + 3);
    msg.text.assign(reinterpret_cast<const char*>(&d[off + 7]),
                    d.size() - (off + 7));
    pushHistory(msg);
    m_drainPending = true;
    if (m_onChannelMsg) m_onChannelMsg(msg);
}

void MeshCoreClient::handleContactMsg(const std::vector<uint8_t>& d, bool v3) {
    ContactMessage msg;
    size_t off = 1;
    if (v3) {
        // [0x10][SNR][rsv][rsv][pubkey6][path][txt][ts LE32][(sig4)][texto...]
        if (d.size() < 16) return;
        int8_t snrRaw = static_cast<int8_t>(d[1]);
        msg.snrDb = static_cast<float>(snrRaw) / 4.0f;
        msg.hasSnr = true;
        off = 4;
    } else {
        // [0x07][pubkey6][path][txt][ts LE32][(sig4)][texto...]
        if (d.size() < 13) return;
    }
    if (d.size() < off + 12) return;
    char hex[13];
    snprintf(hex, sizeof(hex), "%02x%02x%02x%02x%02x%02x",
             d[off], d[off + 1], d[off + 2], d[off + 3], d[off + 4], d[off + 5]);
    msg.pubkeyPrefix = hex;
    msg.pathLength = d[off + 6];
    msg.txtType = static_cast<TxtType>(d[off + 7]);
    msg.timestamp = rdU32(d, off + 8);
    size_t textOff = off + 12;
    if (msg.txtType == TxtType::SIGNED_PLAIN) {
        if (d.size() < textOff + 4) return;
        textOff += 4;  // firma de 4 bytes
    }
    if (textOff <= d.size()) {
        msg.text.assign(reinterpret_cast<const char*>(&d[textOff]),
                        d.size() - textOff);
    }
    m_contactHistory.push_back(msg);
    if (m_contactHistory.size() > 200) m_contactHistory.erase(m_contactHistory.begin());
    m_drainPending = true;
    if (m_onContactMsg) m_onContactMsg(msg);
}

void MeshCoreClient::handleChannelInfo(const std::vector<uint8_t>& d) {
    // [0x12][idx][nombre 32B][secreto 16B] = 50 bytes
    if (d.size() < 50) return;
    uint8_t idx = d[1];
    if (idx >= CHANNEL_COUNT) return;
    MeshChannel ch;
    ch.index = idx;
    ch.name.assign(reinterpret_cast<const char*>(&d[2]), boundedStrLen(&d[2], CHANNEL_NAME_LEN));
    memcpy(ch.secret, &d[34], CHANNEL_SECRET_LEN);
    ch.kind = classifyChannel(ch.name, ch.secret);
    ch.known = true;
    m_channels[idx] = ch;
    if (m_onChannelInfo) m_onChannelInfo(ch);
}

void MeshCoreClient::handleBattery(const std::vector<uint8_t>& d) {
    if (d.size() < 3) return;
    BatteryInfo b;
    b.voltageMv = rdU16(d, 1);
    if (d.size() >= 11) {
        b.usedStorageKb = rdU32(d, 3);
        b.totalStorageKb = rdU32(d, 7);
        b.hasStorage = true;
    }
    b.valid = true;
    m_battery = b;
    if (m_onBattery) m_onBattery(m_battery);
}

void MeshCoreClient::handleMsgSent(const std::vector<uint8_t>& d) {
    if (d.size() < 10) return;
    m_lastMsgSent.flood = d[1] != 0;
    m_lastMsgSent.tag = rdU32(d, 2);
    m_lastMsgSent.timeoutMs = rdU32(d, 6);
    m_hasMsgSent = true;
    if (m_onMsgSent) m_onMsgSent(m_lastMsgSent);
}

void MeshCoreClient::handleOk(const std::vector<uint8_t>& /*data*/) {
    // PACKET_OK: comando aceptado. Sin estado adicional que rastrear.
}

void MeshCoreClient::handleError(const std::vector<uint8_t>& d) {
    ErrCode code = ErrCode::UNSUPPORTED_CMD;
    if (d.size() >= 2 && d[1] >= 1 && d[1] <= 6) {
        code = static_cast<ErrCode>(d[1]);
    }
    m_lastError = code;
    m_hasError = true;
    if (m_onError) m_onError(code);
}

void MeshCoreClient::pushHistory(const ChannelMessage& msg) {
    m_channelHistory.push_back(msg);
    if (m_channelHistory.size() > 200) {
        m_channelHistory.erase(m_channelHistory.begin());
    }
}

ChannelKind MeshCoreClient::classifyChannel(const std::string& name,
                                            const uint8_t secret[CHANNEL_SECRET_LEN]) {
    if (name.empty() && allZero(secret, CHANNEL_SECRET_LEN)) {
        return ChannelKind::Empty;
    }
    if (memcmp(secret, PUBLIC_CHANNEL_SECRET, CHANNEL_SECRET_LEN) == 0) {
        return ChannelKind::Public;
    }
    if (!name.empty()) {
        uint8_t digest[32];
        sha256(reinterpret_cast<const uint8_t*>(name.data()), name.size(), digest);
        if (memcmp(secret, digest, CHANNEL_SECRET_LEN) == 0) {
            return ChannelKind::Hashtag;
        }
    }
    return ChannelKind::Private;
}

// ────────────────────────────────────────────────────────────────
// Envío
// ────────────────────────────────────────────────────────────────

bool MeshCoreClient::sendFrame(Cmd cmd, const uint8_t* extra, size_t extraLen) {
    if (!isConnected()) return false;
    size_t total = 1 + extraLen;
    if (total > FRAME_MAX_PAYLOAD) {
        if (extraLen < 1) return false;
        extraLen = FRAME_MAX_PAYLOAD - 1;
        total = FRAME_MAX_PAYLOAD;
    }
    uint8_t header[3] = {
        FRAME_CHAR_APP_TO_RADIO,
        static_cast<uint8_t>(total & 0xFF),
        static_cast<uint8_t>((total >> 8) & 0xFF),
    };
    if (cbdos::serial::write(header, sizeof(header)) != sizeof(header)) return false;
    uint8_t cmdByte = static_cast<uint8_t>(cmd);
    if (cbdos::serial::write(&cmdByte, 1) != 1) return false;
    if (extraLen > 0 && cbdos::serial::write(extra, extraLen) != extraLen) return false;
    return true;
}

bool MeshCoreClient::sendText(const std::string& text) {
    if (!isConnected()) return false;
    std::string line = text;
    if (line.empty() || line.back() != '\n') line += '\n';
    return cbdos::serial::writeString(line) > 0;
}

// ────────────────────────────────────────────────────────────────
// Handshake / consultas
// ────────────────────────────────────────────────────────────────

bool MeshCoreClient::sendAppStart(const std::string& appName) {
    // [0x01][7B reservados][nombre app UTF-8]
    uint8_t reserved[7] = {0};
    std::string name = truncateUtf8(appName, 64, FRAME_MAX_PAYLOAD - 8);
    std::vector<uint8_t> payload;
    payload.reserve(7 + name.size());
    payload.insert(payload.end(), reserved, reserved + 7);
    payload.insert(payload.end(), name.begin(), name.end());
    return sendFrame(Cmd::APP_START, payload.data(), payload.size());
}

bool MeshCoreClient::queryDeviceInfo() {
    uint8_t arg = 0x03;
    return sendFrame(Cmd::DEVICE_QUERY, &arg, 1);
}

bool MeshCoreClient::queryBattery() {
    return sendFrame(Cmd::GET_BATTERY);
}

// ────────────────────────────────────────────────────────────────
// Mensajes
// ────────────────────────────────────────────────────────────────

bool MeshCoreClient::sendChannelMessage(uint8_t channelIdx, const std::string& text) {
    if (channelIdx >= CHANNEL_COUNT || text.empty() || !isConnected()) return false;
    // Spec: máximo 133 caracteres por mensaje.
    std::string clipped = truncateUtf8(text, 133, FRAME_MAX_PAYLOAD - 7);
    if (clipped.empty()) return false;

    uint32_t ts = static_cast<uint32_t>(time(nullptr));
    std::vector<uint8_t> payload;
    payload.reserve(6 + clipped.size());
    payload.push_back(0x00);
    payload.push_back(channelIdx);
    payload.push_back(static_cast<uint8_t>(ts & 0xFF));
    payload.push_back(static_cast<uint8_t>((ts >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>((ts >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((ts >> 24) & 0xFF));
    payload.insert(payload.end(), clipped.begin(), clipped.end());

    if (!sendFrame(Cmd::SEND_CHANNEL_MESSAGE, payload.data(), payload.size())) {
        return false;
    }

    ChannelMessage echo;
    echo.channelIndex = channelIdx;
    echo.timestamp = ts;
    echo.text = clipped;
    echo.outgoing = true;
    pushHistory(echo);
    return true;
}

bool MeshCoreClient::getNextMessage() {
    return sendFrame(Cmd::GET_MESSAGE);
}

bool MeshCoreClient::pollMessages() {
    m_drainPending = true;
    return sendFrame(Cmd::GET_MESSAGE);
}

void MeshCoreClient::setAutoDrain(bool enabled) {
    m_autoDrain = enabled;
}

// ────────────────────────────────────────────────────────────────
// Canales
// ────────────────────────────────────────────────────────────────

bool MeshCoreClient::getChannel(uint8_t idx) {
    if (idx >= CHANNEL_COUNT) return false;
    return sendFrame(Cmd::GET_CHANNEL, &idx, 1);
}

bool MeshCoreClient::queryAllChannels() {
    for (uint8_t i = 0; i < CHANNEL_COUNT; ++i) {
        if (!getChannel(i)) return false;
    }
    return true;
}

bool MeshCoreClient::setChannel(uint8_t idx, const std::string& name,
                                const uint8_t secret[CHANNEL_SECRET_LEN]) {
    if (idx >= CHANNEL_COUNT || secret == nullptr) return false;
    std::string clipped = truncateUtf8(name, 64, CHANNEL_NAME_LEN);
    uint8_t payload[1 + CHANNEL_NAME_LEN + CHANNEL_SECRET_LEN];
    payload[0] = idx;
    memset(payload + 1, 0, CHANNEL_NAME_LEN);
    memcpy(payload + 1, clipped.data(), clipped.size());
    memcpy(payload + 1 + CHANNEL_NAME_LEN, secret, CHANNEL_SECRET_LEN);
    return sendFrame(Cmd::SET_CHANNEL, payload, sizeof(payload));
}

bool MeshCoreClient::clearChannel(uint8_t idx) {
    uint8_t zero[CHANNEL_SECRET_LEN] = {0};
    return setChannel(idx, "", zero);
}

bool MeshCoreClient::setChannelHex(uint8_t idx, const std::string& name,
                                   const std::string& secretHex) {
    uint8_t secret[CHANNEL_SECRET_LEN];
    if (!parseSecretHex(secretHex, secret)) return false;
    return setChannel(idx, name, secret);
}

bool MeshCoreClient::setHashtagChannel(uint8_t idx, const std::string& name) {
    if (idx >= CHANNEL_COUNT || name.empty()) return false;
    uint8_t digest[32];
    sha256(reinterpret_cast<const uint8_t*>(name.data()), name.size(), digest);
    uint8_t secret[CHANNEL_SECRET_LEN];
    memcpy(secret, digest, CHANNEL_SECRET_LEN);
    return setChannel(idx, name, secret);
}

// ────────────────────────────────────────────────────────────────
// CLI de texto
// ────────────────────────────────────────────────────────────────

bool MeshCoreClient::sendCliCommand(const std::string& cmd) {
    if (cmd.empty()) return false;
    return sendText(cmd);
}

bool MeshCoreClient::setName(const std::string& name) {
    std::string clipped = truncateUtf8(name, 64, 31);
    if (clipped.empty()) return false;
    return sendText("set name " + clipped);
}

bool MeshCoreClient::setRadio(uint32_t freqMhz, uint32_t bwKhz, uint8_t sf, uint8_t cr) {
    char buf[64];
    snprintf(buf, sizeof(buf), "set radio %u,%u,%u,%u",
             (unsigned)freqMhz, (unsigned)bwKhz, (unsigned)sf, (unsigned)cr);
    return sendText(buf);
}

bool MeshCoreClient::setTxPower(int txPowerDbm) {
    char buf[32];
    snprintf(buf, sizeof(buf), "set tx %d", txPowerDbm);
    return sendText(buf);
}

bool MeshCoreClient::setFrequency(uint32_t freqMhz) {
    char buf[32];
    snprintf(buf, sizeof(buf), "set freq %u", (unsigned)freqMhz);
    return sendText(buf);
}

bool MeshCoreClient::reboot() {
    return sendText("reboot");
}

// ────────────────────────────────────────────────────────────────
// Getters y utilidades
// ────────────────────────────────────────────────────────────────

const SelfInfo& MeshCoreClient::getSelfInfo() const { return m_selfInfo; }
const DeviceInfo& MeshCoreClient::getDeviceInfo() const { return m_deviceInfo; }
const BatteryInfo& MeshCoreClient::getBattery() const { return m_battery; }

const MeshChannel& MeshCoreClient::getChannelInfo(uint8_t idx) const {
    static const MeshChannel s_empty;
    if (idx >= CHANNEL_COUNT) return s_empty;
    return m_channels[idx];
}

const std::vector<ChannelMessage>& MeshCoreClient::getChannelHistory() const {
    return m_channelHistory;
}

const std::vector<ContactMessage>& MeshCoreClient::getContactHistory() const {
    return m_contactHistory;
}

const MsgSentInfo& MeshCoreClient::getLastMsgSent() const { return m_lastMsgSent; }
bool MeshCoreClient::hasLastMsgSent() const { return m_hasMsgSent; }
ErrCode MeshCoreClient::getLastError() const { return m_lastError; }
bool MeshCoreClient::hasLastError() const { return m_hasError; }
void MeshCoreClient::clearLastError() { m_hasError = false; }

void MeshCoreClient::clearHistory() {
    m_channelHistory.clear();
    m_contactHistory.clear();
}

bool MeshCoreClient::parseSecretHex(const std::string& hex, uint8_t out[CHANNEL_SECRET_LEN]) {
    if (hex.size() != CHANNEL_SECRET_LEN * 2 || out == nullptr) return false;
    auto nibble = [](char c, uint8_t& v) -> bool {
        if (c >= '0' && c <= '9') v = static_cast<uint8_t>(c - '0');
        else if (c >= 'a' && c <= 'f') v = static_cast<uint8_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = static_cast<uint8_t>(c - 'A' + 10);
        else return false;
        return true;
    };
    for (size_t i = 0; i < CHANNEL_SECRET_LEN; ++i) {
        uint8_t hi, lo;
        if (!nibble(hex[i * 2], hi) || !nibble(hex[i * 2 + 1], lo)) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::string MeshCoreClient::secretToHex(const uint8_t secret[CHANNEL_SECRET_LEN]) {
    char buf[CHANNEL_SECRET_LEN * 2 + 1];
    for (size_t i = 0; i < CHANNEL_SECRET_LEN; ++i) {
        snprintf(buf + i * 2, 3, "%02x", secret[i]);
    }
    return std::string(buf, CHANNEL_SECRET_LEN * 2);
}

const char* MeshCoreClient::channelKindName(ChannelKind kind) {
    switch (kind) {
        case ChannelKind::Empty:   return "Vacio";
        case ChannelKind::Public:  return "Publico";
        case ChannelKind::Hashtag: return "Hashtag";
        case ChannelKind::Private: return "Privado";
        default:                   return "Desconocido";
    }
}

// ────────────────────────────────────────────────────────────────
// Setters de callbacks
// ────────────────────────────────────────────────────────────────

void MeshCoreClient::setOnChannelMessage(std::function<void(const ChannelMessage&)> cb) {
    m_onChannelMsg = cb;
}
void MeshCoreClient::setOnContactMessage(std::function<void(const ContactMessage&)> cb) {
    m_onContactMsg = cb;
}
void MeshCoreClient::setOnSelfInfo(std::function<void(const SelfInfo&)> cb) {
    m_onSelfInfo = cb;
}
void MeshCoreClient::setOnDeviceInfo(std::function<void(const DeviceInfo&)> cb) {
    m_onDeviceInfo = cb;
}
void MeshCoreClient::setOnChannelInfo(std::function<void(const MeshChannel&)> cb) {
    m_onChannelInfo = cb;
}
void MeshCoreClient::setOnBattery(std::function<void(const BatteryInfo&)> cb) {
    m_onBattery = cb;
}
void MeshCoreClient::setOnMsgSent(std::function<void(const MsgSentInfo&)> cb) {
    m_onMsgSent = cb;
}
void MeshCoreClient::setOnError(std::function<void(ErrCode)> cb) {
    m_onError = cb;
}
void MeshCoreClient::setOnMessagesWaiting(std::function<void()> cb) {
    m_onMessagesWaiting = cb;
}
void MeshCoreClient::setOnConnectionState(std::function<void(bool connected)> cb) {
    m_onConnState = cb;
}

} // namespace meshcore
} // namespace cbdos
