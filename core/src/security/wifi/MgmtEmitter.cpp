#include "MgmtEmitter.hpp"

#include <cstring>

namespace cbdos {
namespace security {

namespace {

// Longitudes fijas de la trama (offsets de la tabla del .hpp).
constexpr size_t kMacHeaderLen = 24;   // FC(2) + dur(2) + Addr1/2/3(18) + seq(2)
constexpr size_t kReasonLen = 2;       // Reason Code
constexpr size_t kBodyOffset = kMacHeaderLen + kReasonLen;   // 26

} // namespace

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

size_t MgmtEmitter::frameSize() {
    return kBodyOffset;
}

bool MgmtEmitter::isSubtypeValid(uint8_t subtype) {
    return subtype == kSubtypeDeauth || subtype == kSubtypeDisassoc;
}

uint8_t MgmtEmitter::frameControlByte1(bool protectedFrame) {
    // Solo se manipula el bit Protected Frame: To/From DS, More Fragments,
    // Retry y Power Management quedan a 0, que es lo que exige una trama de
    // gestion emitida por una AP o por una STA sin enlace asociado.
    return protectedFrame ? kFcProtectedFrame : 0x00;
}

void MgmtEmitter::fillBroadcast(uint8_t mac[wifi::WIFI_MAC_LEN]) {
    if (mac == nullptr) return;
    std::memset(mac, 0xFF, wifi::WIFI_MAC_LEN);
}

void MgmtEmitter::fillRandomStaMac(uint8_t mac[wifi::WIFI_MAC_LEN], Rng& rng) {
    if (mac == nullptr) return;
    for (size_t i = 0; i < wifi::WIFI_MAC_LEN; ++i) {
        mac[i] = static_cast<uint8_t>(rng.next() & 0xFF);
    }
    mac[0] &= 0xFE;   // unicast: bit multicast del primer octeto a 0
    mac[0] |= 0x02;   // locally administered: bit de administracion local a 1
}

uint16_t MgmtEmitter::randomSequenceControl(Rng& rng) {
    // 12 bits de secuencia << 4 y fragmento a 0 (estas tramas no se fragmentan).
    return static_cast<uint16_t>((rng.next() & 0x0FFFu) << 4);
}

// ---------------------------------------------------------------------------
// Construccion de la trama
// ---------------------------------------------------------------------------

MgmtFrame MgmtEmitter::build(uint8_t subtype, const Params& params) {
    MgmtFrame frame;

    if (!isSubtypeValid(subtype)) return frame;
    if (frameSize() > kMaxFrameSize) return frame;

    // --- MAC header ---
    // Byte 0: Protocol Version (0) | Type (0 = Management) | Subtype.
    frame.data[0] = static_cast<uint8_t>((subtype << 4) | (kTypeManagement << 2));
    frame.data[1] = frameControlByte1(params.protectedFrame);
    frame.data[2] = 0x00;   // Duration / ID: las tramas de gestion no llevan duracion
    frame.data[3] = 0x00;

    // --- Direcciones ---
    std::memcpy(frame.data + 4, params.da, wifi::WIFI_MAC_LEN);      // Addr1
    std::memcpy(frame.data + 10, params.sa, wifi::WIFI_MAC_LEN);     // Addr2
    std::memcpy(frame.data + 16, params.bssid, wifi::WIFI_MAC_LEN);  // Addr3

    // --- Sequence Control: 12 bits de secuencia + 4 de fragmento ---
    frame.data[22] = static_cast<uint8_t>(params.seqControl & 0xFF);
    frame.data[23] = static_cast<uint8_t>((params.seqControl >> 8) & 0xFF);

    // --- Reason Code (little-endian en el cable) ---
    frame.data[24] = static_cast<uint8_t>(params.reasonCode & 0xFF);
    frame.data[25] = static_cast<uint8_t>((params.reasonCode >> 8) & 0xFF);

    frame.len = frameSize();
    frame.valid = true;
    return frame;
}

MgmtFrame MgmtEmitter::buildDeauth(const Params& params) {
    return build(kSubtypeDeauth, params);
}

MgmtFrame MgmtEmitter::buildDisassoc(const Params& params) {
    return build(kSubtypeDisassoc, params);
}

} // namespace security
} // namespace cbdos