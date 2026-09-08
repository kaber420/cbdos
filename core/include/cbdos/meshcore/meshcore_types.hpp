#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace cbdos {
namespace meshcore {

// ────────────────────────────────────────────────────────────────
// Framing USB-serie del Companion Protocol oficial.
// App → Radio: '<' (0x3C) + longitud u16 LE (2 bytes) + frame.
// Radio → App: '>' (0x3E) + longitud u16 LE (2 bytes) + frame.
// (En BLE cada frame va sin framing; en TCP el proxy usa 0x3C
// en ambas direcciones.)
// Ref: https://docs.meshcore.io/companion_protocol/
// ────────────────────────────────────────────────────────────────
static constexpr uint8_t FRAME_CHAR_APP_TO_RADIO = 0x3C;  // '<'
static constexpr uint8_t FRAME_CHAR_RADIO_TO_APP = 0x3E;  // '>'
static constexpr size_t FRAME_MAX_PAYLOAD = 1024;

static constexpr uint8_t CHANNEL_COUNT = 8;
static constexpr size_t CHANNEL_NAME_LEN = 32;
static constexpr size_t CHANNEL_SECRET_LEN = 16;
static constexpr size_t PUBKEY_LEN = 32;
static constexpr size_t PUBKEY_PREFIX_LEN = 6;

// Secreto público bien conocido del canal público por defecto.
static constexpr uint8_t PUBLIC_CHANNEL_SECRET[CHANNEL_SECRET_LEN] = {
    0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
    0xac, 0x9e, 0x5e, 0xdb, 0xaa, 0x11, 0x5c, 0xd7
};

// Comandos (App → Radio)
enum class Cmd : uint8_t {
    APP_START            = 0x01,
    SEND_DM              = 0x02,
    SEND_CHANNEL_MESSAGE = 0x03,
    GET_CONTACTS         = 0x04,
    SET_TIME             = 0x06,
    SEND_ADVERT          = 0x07,
    SET_ADVERT_NAME      = 0x08,
    ADD_UPDATE_CONTACT   = 0x09,
    GET_MESSAGE          = 0x0A,  // aka CMD_SYNC_NEXT_MESSAGE
    RESET_PATH           = 0x0D,
    SET_ADVERT_LATLON    = 0x0E,
    REMOVE_CONTACT       = 0x0F,
    SHARE_CONTACT        = 0x10,
    EXPORT_CONTACT       = 0x11,
    IMPORT_CONTACT       = 0x12,
    GET_BATTERY          = 0x14,
    DEVICE_QUERY         = 0x16,
    GET_CHANNEL          = 0x1F,
    SET_CHANNEL          = 0x20,
    SEND_CHANNEL_DATAGRAM = 0x3E,
};

// Respuestas / notificaciones (Radio → App)
enum class PacketType : uint8_t {
    OK                   = 0x00,
    ERROR                = 0x01,
    CONTACT_START        = 0x02,
    CONTACT              = 0x03,
    CONTACT_END          = 0x04,
    SELF_INFO            = 0x05,
    MSG_SENT             = 0x06,
    CONTACT_MSG_RECV     = 0x07,
    CHANNEL_MSG_RECV     = 0x08,
    CURRENT_TIME         = 0x09,
    NO_MORE_MSGS         = 0x0A,
    EXPORT_CONTACT       = 0x0B,
    BATTERY              = 0x0C,
    DEVICE_INFO          = 0x0D,
    CONTACT_MSG_RECV_V3  = 0x10,
    CHANNEL_MSG_RECV_V3  = 0x11,
    CHANNEL_INFO         = 0x12,
    CHANNEL_DATA_RECV    = 0x1B,
    ADVERTISEMENT        = 0x80,
    ACK                  = 0x82,
    MESSAGES_WAITING     = 0x83,
    LOG_DATA             = 0x88,
    NEW_ADVERTISEMENT    = 0x8A,
    TELEMETRY            = 0x8B,
};

// Códigos de error (PACKET_ERROR, byte 1)
enum class ErrCode : uint8_t {
    UNSUPPORTED_CMD = 1,
    NOT_FOUND       = 2,
    TABLE_FULL      = 3,
    BAD_STATE       = 4,
    FILE_IO_ERROR   = 5,
    ILLEGAL_ARG     = 6,
};

// Tipo de texto en mensajes recibidos
enum class TxtType : uint8_t {
    PLAIN        = 0,
    CLI_DATA     = 1,
    SIGNED_PLAIN = 2,
};

// Clasificación de canal según secreto
enum class ChannelKind : uint8_t {
    Unknown,
    Empty,    // nombre vacío + secreto cero
    Public,   // secreto == PUBLIC_CHANNEL_SECRET
    Hashtag,  // secreto == sha256(nombre)[0..16)
    Private,  // secreto aleatorio
};

// Info de un slot de canal (0-7)
struct MeshChannel {
    uint8_t index = 0;
    std::string name;
    uint8_t secret[CHANNEL_SECRET_LEN] = {0};
    ChannelKind kind = ChannelKind::Unknown;
    bool known = false;  // true si se recibió CHANNEL_INFO para este slot
};

// PACKET_SELF_INFO (0x05): identidad y radio del dongle
struct SelfInfo {
    uint8_t advType = 0;
    int8_t txPowerDbm = 0;
    int8_t maxTxPowerDbm = 0;
    uint8_t publicKey[PUBKEY_LEN] = {0};
    double advLatitude = 0.0;
    double advLongitude = 0.0;
    uint8_t multiAcks = 0;
    uint8_t advLocPolicy = 0;
    uint8_t telemetryMode = 0;
    bool manualAddContacts = false;
    uint32_t frequencyHz = 0;
    uint32_t bandwidthHz = 0;
    uint8_t spreadingFactor = 0;
    uint8_t codingRate = 0;
    std::string name;
    bool valid = false;
};

// PACKET_DEVICE_INFO (0x0D): firmware y modelo
struct DeviceInfo {
    uint8_t firmwareVersion = 0;
    uint16_t maxContacts = 0;
    uint8_t maxChannels = 0;
    uint32_t blePin = 0;
    std::string firmwareBuild;
    std::string model;
    std::string version;
    bool clientRepeat = false;
    uint8_t pathHashMode = 0;
    bool valid = false;
};

// PACKET_BATTERY (0x0C)
struct BatteryInfo {
    uint16_t voltageMv = 0;
    uint32_t usedStorageKb = 0;
    uint32_t totalStorageKb = 0;
    bool hasStorage = false;
    bool valid = false;
};

// Mensaje de canal recibido (0x08 / 0x11) o eco local de envío
struct ChannelMessage {
    uint8_t channelIndex = 0;
    uint8_t pathLength = 0;
    TxtType txtType = TxtType::PLAIN;
    uint32_t timestamp = 0;  // segundos UNIX
    std::string text;
    float snrDb = 0.0f;      // solo V3
    bool hasSnr = false;
    bool outgoing = false;   // true si es eco local de un envío propio
};

// Mensaje de contacto/DM recibido (0x07 / 0x10)
struct ContactMessage {
    std::string pubkeyPrefix;  // 6 bytes en hex (12 caracteres)
    uint8_t pathLength = 0;
    TxtType txtType = TxtType::PLAIN;
    uint32_t timestamp = 0;  // segundos UNIX
    std::string text;
    float snrDb = 0.0f;  // solo V3
    bool hasSnr = false;
    bool outgoing = false;  // true si es eco local de un DM propio
};

// Confirmación PACKET_MSG_SENT (0x06)
struct MsgSentInfo {
    bool flood = false;      // route flag: 0=direct, 1=flood
    uint32_t tag = 0;        // expected ACK tag
    uint32_t timeoutMs = 0;  // timeout sugerido
};

// ──────────────────────────────────────────────────────────────
// Fase 1 (RFC-CBDOS-MESHCORE-FULL): agenda, DM fiable y adverts.
// Layout CONTACT/ADVERT (Radio→App, tras byte de tipo):
//  [pubkey32][type u8][flags u8][out_path_len i8][out_path 64B]
//  [name 32B null-pad][last_advert u32 LE][lat i32 LE][lon i32 LE]
//  [lastmod u32 LE] = 147 bytes (+1 de tipo = 148).
// ──────────────────────────────────────────────────────────────
static constexpr size_t CONTACT_NAME_LEN = 32;
static constexpr size_t CONTACT_OUTPATH_LEN = 64;
static constexpr size_t CONTACT_BODY_LEN = 147;

enum class ContactType : uint8_t {
    Unknown  = 0,
    Chat     = 1,
    Repeater = 2,
    Room     = 3,
    Sensor   = 4,
};

enum class AdvertType : uint8_t {
    ZeroHop = 0,
    Flood   = 1,
};

struct MeshContact {
    uint8_t pubkey[PUBKEY_LEN] = {0};
    bool hasPubkey = false;
    std::string prefixHex12;  // 6 primeros bytes en hex (para DM por prefijo)
    std::string name;
    uint8_t type = 0;   // ContactType
    uint8_t flags = 0;
    int8_t outPathLen = -1;  // -1 = flood
    uint8_t outPath[CONTACT_OUTPATH_LEN] = {0};
    uint32_t lastAdvert = 0;  // epoch u32
    double lat = 0.0;
    double lon = 0.0;
    uint32_t lastmod = 0;
    float lastSnr = 0.0f;
    bool hasSnr = false;
    uint8_t hops = 0;
    bool favourite = false;
    uint32_t unread = 0;
    bool valid = false;
};

// Hilo DM 1-1 indexado por prefixHex12 (12 hex del pubkey).
struct DMThread {
    std::string prefixHex12;
    std::vector<ContactMessage> msgs;
};

enum class DmStatus : uint8_t {
    Sending,
    Delivered,
    Failed,
};

// DM pendiente de ACK (retry 5x direct / 3x flood, último fuerza flood).
struct PendingDM {
    uint32_t tag = 0;
    bool hasTag = false;  // false = esperando MSG_SENT que asigne tag
    uint8_t destPubkey[PUBKEY_LEN] = {0};
    std::string destPrefix;
    std::string text;
    uint32_t timestamp = 0;
    uint8_t attempts = 0;
    uint8_t maxAttempts = 5;
    bool flood = false;
    uint32_t deadlineMs = 0;
    DmStatus status = DmStatus::Sending;
};

} // namespace meshcore
} // namespace cbdos
