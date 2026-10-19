#include "BeaconEmitter.hpp"

#include <cstring>

namespace cbdos {
namespace security {

namespace {

// --- Cabecera y parametros fijos de la trama (offsets de la tabla del .hpp) --
constexpr size_t kMacHeaderLen = 24;      // FC+dur+Addr1+Addr2+Addr3+seq
constexpr size_t kFixedParamsLen = 12;   // timestamp+interval+capability
constexpr size_t kBeaconBodyOffset = kMacHeaderLen + kFixedParamsLen; // 36

constexpr size_t kSsidIeHeaderLen = 2;          // id + longitud
constexpr size_t kSsidIeOffset = kBeaconBodyOffset + kSsidIeHeaderLen; // 38

// Supported Rates: id, longitud y 8 Rates en unidades de 0.5 Mbps
constexpr size_t kSupportedRatesIeLen = 10;
constexpr const uint8_t kSupportedRates[kSupportedRatesIeLen] = {
    0x01, 0x08,                          // tag, longitud
    0x82, 0x84, 0x8B, 0x96,              // 1, 2, 5.5, 11 Mbps
    0x24, 0x30, 0x48, 0x6C,              // 18, 24, 36, 54 Mbps
};

// DS Parameter Set: id, longitud y canal primario
constexpr size_t kDsParamIeLen = 3;
constexpr uint8_t kDsParamIe[kDsParamIeLen] = {0x03, 0x01, 0x00};

// Vendor Specific IE con HE Capabilities de Wi-Fi Alliance (OUI 00:50:6f).
// El cuerpo son 12 bytes reales (OUI[3] + tipo[1] + MAC caps[2] + PHY caps[3]
// + params[3]) y la longitud anunciada es 0x0C.
//
// La referencia GhostESP escribe esos mismos 12 bytes pero anuncia 0x0D, y
// despues anade 2 bytes a packet_size que nunca inicializa: el bloque acaba
// volanteando 2 bytes de pila en el aire. Aqui la longitud y el cuerpo
// coinciden, que es lo que un cliente real exige para parsear la IE.
constexpr size_t kHeCapsIeLen = 14;   // 2 (id+long) + 12 (cuerpo)
constexpr uint8_t kHeCapsIe[kHeCapsIeLen] = {
    0xFF, 0x0C,                          // tag vendor specific, longitud 12
    0x50, 0x6F, 0x9A,                    // OUI Wi-Fi Alliance + tipo HE (0x9A)
    0x00, 0x08,                          // HE MAC Capabilities Information
    0x00, 0x00, 0x40,                    // HE PHY Capabilities (80 MHz)
    0x00, 0x00, 0x01,                    // parametros de operacion HE
};

// Alfabeto del SSID aleatorio (como el charset de GhostESP).
constexpr char kSsidCharset[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
constexpr size_t kSsidCharsetLen = sizeof(kSsidCharset) - 1; // sin el '\0'

/**
 * @brief Cursor de escritura con saturacion
 *
 * Si la trama no cabe, deja de copiar y marca `overflow`; el constructor lo
 * comprueba y devuelve una trama invalida en lugar de emitir bytes recortados.
 */
struct Writer {
    uint8_t* buf;
    size_t cap;
    size_t pos = 0;
    bool overflow = false;

    void u8(uint8_t value) {
        if (pos >= cap) { overflow = true; return; }
        buf[pos++] = value;
    }

    void u16(uint16_t value) {   // 802.11 es little-endian en el cable
        u8(static_cast<uint8_t>(value & 0xFF));
        u8(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void u32(uint32_t value) {
        u8(static_cast<uint8_t>(value & 0xFF));
        u8(static_cast<uint8_t>((value >> 8) & 0xFF));
        u8(static_cast<uint8_t>((value >> 16) & 0xFF));
        u8(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void bytes(const uint8_t* data, size_t len) {
        if (len > cap - (pos < cap ? pos : cap)) { overflow = true; return; }
        std::memcpy(buf + pos, data, len);
        pos += len;
    }

    void fill(uint8_t value, size_t len) {
        if (len > cap - (pos < cap ? pos : cap)) { overflow = true; return; }
        std::memset(buf + pos, value, len);
        pos += len;
    }
};

// Coro de Rick Astley. Los versos mas largos ("Never gonna run around and
// desert you", 40 bytes) exceden el limite de 32 bytes del campo SSID: build()
// los trunca, que es lo unico que un beacon 802.11 valido permite.
constexpr const char* kRickrollLines[] = {
    "Never gonna give you up",
    "Never gonna let you down",
    "Never gonna run around and desert you",
    "Never gonna make you cry",
    "Never gonna say goodbye",
    "Never gonna tell a lie and hurt you",
};
constexpr size_t kRickrollLineCount =
    sizeof(kRickrollLines) / sizeof(kRickrollLines[0]);

} // namespace

// ---------------------------------------------------------------------------
// Rickroll
// ---------------------------------------------------------------------------

size_t BeaconEmitter::rickrollLineCount() {
    return kRickrollLineCount;
}

const char* BeaconEmitter::rickrollLine(uint8_t index) {
    return kRickrollLines[index % kRickrollLineCount];
}

uint8_t BeaconEmitter::nextRickrollIndex(uint8_t current) {
    return static_cast<uint8_t>((current + 1) % kRickrollLineCount);
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

bool BeaconEmitter::isSsidLengthValid(size_t ssidLength) {
    return ssidLength <= kMaxSsidLength;
}

size_t BeaconEmitter::frameSizeFor(size_t ssidLength, bool includeHeCapabilities) {
    if (ssidLength > kMaxSsidLength) ssidLength = kMaxSsidLength;
    size_t size = kSsidIeOffset + ssidLength + kSupportedRatesIeLen + kDsParamIeLen;
    if (includeHeCapabilities) size += kHeCapsIeLen;
    return size;
}

uint16_t BeaconEmitter::capabilityForAuth(radio::WifiAuthMode auth) {
    switch (auth) {
        case radio::WifiAuthMode::Open:
            // Red abierta: sin bit Privacy. Se conserva el bit reservado de la
            // referencia para no divergir de lo que emite GhostESP.
            return static_cast<uint16_t>(kCapEss | kCapReferenceReserved);
        case radio::WifiAuthMode::WEP:
        case radio::WifiAuthMode::WPA:
        case radio::WifiAuthMode::WPA2:
        case radio::WifiAuthMode::WPA3:
        case radio::WifiAuthMode::WPA2Enterprise:
        case radio::WifiAuthMode::WPA3Enterprise:
            return kCapDefault;
        default:
            // Auth desconocida: se anuncia abierta. Es el error menos dañino,
            // un cliente que no negocia cifrado simplemente no se conecta.
            return static_cast<uint16_t>(kCapEss | kCapReferenceReserved);
    }
}

void BeaconEmitter::fillRandomSsid(char* out, size_t capacity, size_t length,
                                   Rng& rng) {
    if (out == nullptr || capacity == 0) return;
    if (length > kMaxSsidLength) length = kMaxSsidLength;

    size_t written = 0;
    for (; written < length; ++written) {
        if (written + 1 >= capacity) break;   // reserva el terminador
        out[written] = kSsidCharset[rng.below(static_cast<uint32_t>(kSsidCharsetLen))];
    }
    out[written] = '\0';
}

void BeaconEmitter::fillRandomMac(uint8_t mac[wifi::WIFI_MAC_LEN], Rng& rng) {
    if (mac == nullptr) return;
    for (size_t i = 0; i < wifi::WIFI_MAC_LEN; ++i) {
        mac[i] = static_cast<uint8_t>(rng.next() & 0xFF);
    }
    mac[0] &= 0xFE;   // unicast: bit multicast del primer octeto a 0
    mac[0] |= 0x02;   // locally administered: bit de administracion local a 1
}

// ---------------------------------------------------------------------------
// Construccion de la trama
// ---------------------------------------------------------------------------

BeaconFrame BeaconEmitter::build(const Params& params, const char* ssid,
                                 size_t ssidLength) {
    BeaconFrame frame;

    if (ssidLength > kMaxSsidLength) ssidLength = kMaxSsidLength;
    if (ssidLength > 0 && ssid == nullptr) return frame;

    const size_t total = frameSizeFor(ssidLength, params.includeHeCapabilities);
    if (total > kMaxFrameSize) return frame;

    Writer w{frame.data, kMaxFrameSize};

    // --- MAC header ---
    w.u8(0x80);              // Frame Control: Management, subtype Beacon (0x08 << 2)
    w.u8(0x00);              // Version 0
    w.u16(0x0000);           // Duration / ID
    w.fill(0xFF, 6);         // Addr1: ff:ff:ff:ff:ff:ff (broadcast)
    w.bytes(params.bssid, wifi::WIFI_MAC_LEN);   // Addr2: origen = BSSID
    w.bytes(params.bssid, wifi::WIFI_MAC_LEN);   // Addr3: BSSID
    w.u16(params.seqControl);

    // --- Fixed parameters ---
    w.u32(params.timestamp);
    w.u32(0);                // parte alta del TSF; 0 = sin estampar
    w.u16(params.beaconInterval);
    w.u16(params.capabilityInfo);

    // --- SSID element ---
    w.u8(0x00);                                   // tag
    w.u8(static_cast<uint8_t>(ssidLength));       // longitud
    if (ssidLength > 0) w.bytes(reinterpret_cast<const uint8_t*>(ssid), ssidLength);

    // --- Supported Rates ---
    w.bytes(kSupportedRates, kSupportedRatesIeLen);

    // --- DS Parameter Set: anuncia el canal en el que se esta emitiendo ---
    w.u8(kDsParamIe[0]);
    w.u8(kDsParamIe[1]);
    w.u8(params.channel);

    // --- HE Capabilities (opcional) ---
    if (params.includeHeCapabilities) {
        w.bytes(kHeCapsIe, kHeCapsIeLen);
    }

    if (w.overflow || w.pos != total) return frame;

    frame.len = w.pos;
    frame.valid = true;
    return frame;
}

BeaconFrame BeaconEmitter::build(const Params& params, const std::string& ssid) {
    return build(params, ssid.c_str(), ssid.size());
}

BeaconFrame BeaconEmitter::buildRandomSsid(const Params& params, uint8_t length,
                                           Rng& rng) {
    if (length == 0) length = wifi::STRIKE_RANDOM_SSID_LEN;
    if (length > kMaxSsidLength) length = kMaxSsidLength;

    char ssid[kMaxSsidLength + 1] = {0};
    fillRandomSsid(ssid, sizeof(ssid), length, rng);
    return build(params, ssid, length);
}

BeaconFrame BeaconEmitter::buildCloneAp(const radio::WifiApRecord& ap,
                                        bool includeHeCapabilities) {
    Params params;
    params.channel = ap.channel;
    std::memcpy(params.bssid, ap.bssid, wifi::WIFI_MAC_LEN);
    params.capabilityInfo = capabilityForAuth(ap.auth);
    params.includeHeCapabilities = includeHeCapabilities;
    // Un clon hereda la identidad de la AP original: mismo reloj TSF base y
    // mismo intervalo de beacon, para que el cliente no lo distinga por el
    // timing. El timestamp concreto lo estampa el servicio si lo necesita.
    return build(params, ap.ssid);
}

} // namespace security
} // namespace cbdos
