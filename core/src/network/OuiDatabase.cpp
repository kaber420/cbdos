#include "OuiDatabase.hpp"

#include <algorithm>

namespace cbdos {
namespace network {

// Tabla estatica en Flash (.rodata): sin RAM dinamica.
// ORDENADA ascendentemente por OUI para std::lower_bound O(log N).
// Para regenerar/verificar el orden, ordenar por el valor de 24 bits
// de "AA:BB:CC" como entero big-endian.
static const OuiEntry kOuiTable[] = {
    {0x00000C, "Cisco"}, // 00:00:0C
    {0x00015C, "Arris"}, // 00:01:5C
    {0x000163, "Cisco"}, // 00:01:63
    {0x0001FE, "Sony"}, // 00:01:FE
    {0x0002B3, "Intel"}, // 00:02:B3
    {0x0002C7, "Sony"}, // 00:02:C7
    {0x0002CF, "ZyXEL"}, // 00:02:CF
    {0x000347, "Intel"}, // 00:03:47
    {0x000393, "Apple"}, // 00:03:93
    {0x0003FF, "Microsoft"}, // 00:03:FF
    {0x000532, "Cisco"}, // 00:05:32
    {0x00055D, "D-Link"}, // 00:05:5D
    {0x000625, "Linksys"}, // 00:06:25
    {0x0007AB, "Samsung"}, // 00:07:AB
    {0x0007E9, "Intel"}, // 00:07:E9
    {0x00095B, "Netgear"}, // 00:09:5B
    {0x000B57, "Silicon Labs"}, // 00:0B:57
    {0x000B60, "Cisco"}, // 00:0B:60
    {0x000C41, "Linksys"}, // 00:0C:41
    {0x000C6E, "Asus"}, // 00:0C:6E
    {0x000D3A, "Microsoft"}, // 00:0D:3A
    {0x000D4B, "Roku"}, // 00:0D:4B
    {0x000D88, "D-Link"}, // 00:0D:88
    {0x000E0C, "Intel"}, // 00:0E:0C
    {0x000E58, "Sonos"}, // 00:0E:58
    {0x001095, "Technicolor"}, // 00:10:95
    {0x00110A, "HP"}, // 00:11:0A
    {0x001150, "Belkin"}, // 00:11:50
    {0x001237, "Texas Instruments"}, // 00:12:37
    {0x00125A, "Microsoft"}, // 00:12:5A
    {0x0012FB, "Samsung"}, // 00:12:FB
    {0x001320, "Intel"}, // 00:13:20
    {0x001349, "ZyXEL"}, // 00:13:49
    {0x0013E0, "Murata"}, // 00:13:E0
    {0x001422, "Dell"}, // 00:14:22
    {0x00155D, "Microsoft"}, // 00:15:5D
    {0x001596, "Arris"}, // 00:15:96
    {0x001599, "Samsung"}, // 00:15:99
    {0x0017E9, "Texas Instruments"}, // 00:17:E9
    {0x001882, "Huawei"}, // 00:18:82
    {0x001A11, "Google"}, // 00:1A:11
    {0x001AA1, "Cisco"}, // 00:1A:A1
    {0x001B2F, "Netgear"}, // 00:1B:2F
    {0x001C62, "LG"}, // 00:1C:62
    {0x001E10, "Huawei"}, // 00:1E:10
    {0x0022A1, "Huawei"}, // 00:22:A1
    {0x002536, "Hon Hai"}, // 00:25:36
    {0x002644, "Technicolor"}, // 00:26:44
    {0x002719, "TP-Link"}, // 00:27:19
    {0x00408C, "Axis"}, // 00:40:8C
    {0x00E04C, "Realtek"}, // 00:E0:4C
    {0x0418D6, "Ubiquiti"}, // 04:18:D6
    {0x085531, "MikroTik"}, // 08:55:31
    {0x0C37DC, "Huawei"}, // 0C:37:DC
    {0x14CC20, "TP-Link"}, // 14:CC:20
    {0x1C5F2B, "D-Link"}, // 1C:5F:2B
    {0x1C872C, "Asus"}, // 1C:87:2C
    {0x2047DA, "LG"}, // 20:47:DA
    {0x240AC4, "Espressif"}, // 24:0A:C4
    {0x245A4C, "Ubiquiti"}, // 24:5A:4C
    {0x246F28, "Espressif"}, // 24:6F:28
    {0x28107B, "D-Link"}, // 28:10:7B
    {0x281878, "Microsoft"}, // 28:18:78
    {0x286B7C, "Cisco"}, // 28:6B:7C
    {0x286C07, "Xiaomi"}, // 28:6C:07
    {0x2C56DC, "Asus"}, // 2C:56:DC
    {0x2CC81B, "MikroTik"}, // 2C:C8:1B
    {0x30AEA4, "Espressif"}, // 30:AE:A4
    {0x3480B3, "Xiaomi"}, // 34:80:B3
    {0x34B1F7, "Texas Instruments"}, // 34:B1:F7
    {0x34D270, "Amazon"}, // 34:D2:70
    {0x3C0630, "Apple"}, // 3C:06:30
    {0x3C1710, "Sagemcom"}, // 3C:17:10
    {0x3C5A37, "Google"}, // 3C:5A:37
    {0x3C71BF, "Espressif"}, // 3C:71:BF
    {0x3CA9F4, "Intel"}, // 3C:A9:F4
    {0x3CD92B, "HP"}, // 3C:D9:2B
    {0x3CE524, "Dahua"}, // 3C:E5:24
    {0x40CBC0, "Apple"}, // 40:CB:C0
    {0x4419B6, "Hikvision"}, // 44:19:B6
    {0x4846FB, "Huawei"}, // 48:46:FB
    {0x4C11BF, "Dahua"}, // 4C:11:BF
    {0x50C7BF, "TP-Link"}, // 50:C7:BF
    {0x543204, "Espressif"}, // 54:32:04
    {0x546009, "Google"}, // 54:60:09
    {0x58238C, "Linksys"}, // 58:23:8C
    {0x5855CA, "Apple"}, // 58:55:CA
    {0x588D09, "Cisco"}, // 58:8D:09
    {0x5C6A80, "ZyXEL"}, // 5C:6A:80
    {0x5C8D4E, "Samsung"}, // 5C:8D:4E
    {0x605718, "Murata"}, // 60:57:18
    {0x640980, "Xiaomi"}, // 64:09:80
    {0x6837E9, "Amazon"}, // 68:37:E9
    {0x687251, "Ubiquiti"}, // 68:72:51
    {0x6C198F, "TP-Link"}, // 6C:19:8F
    {0x6C3B6B, "MikroTik"}, // 6C:3B:6B
    {0x70105C, "Cisco"}, // 70:10:5C
    {0x7014A6, "Hon Hai"}, // 70:14:A6
    {0x707BE8, "Huawei"}, // 70:7B:E8
    {0x74C246, "Amazon"}, // 74:C2:46
    {0x7811DC, "Xiaomi"}, // 78:11:DC
    {0x781FDB, "Samsung"}, // 78:1F:DB
    {0x782184, "Espressif"}, // 78:21:84
    {0x7828CA, "Sonos"}, // 78:28:CA
    {0x788A20, "Ubiquiti"}, // 78:8A:20
    {0x7C034E, "Sagemcom"}, // 7C:03:4E
    {0x7C70DB, "Intel"}, // 7C:70:DB
    {0x84CCA8, "Espressif"}, // 84:CC:A8
    {0x84D81B, "TP-Link"}, // 84:D8:1B
    {0x84FD27, "Silicon Labs"}, // 84:FD:27
    {0x8C8590, "Apple"}, // 8C:85:90
    {0x8CAAB5, "Espressif"}, // 8C:AA:B5
    {0x8CF5A3, "Samsung"}, // 8C:F5:A3
    {0x9002A9, "Dahua"}, // 90:02:A9
    {0x90A2DA, "Arduino"}, // 90:A2:DA
    {0xA0510B, "Intel"}, // A0:51:0B
    {0xA42B8C, "TP-Link"}, // A4:2B:8C
    {0xA4B805, "Apple"}, // A4:B8:05
    {0xA4CF12, "Espressif"}, // A4:CF:12
    {0xA8610A, "Arduino"}, // A8:61:0A
    {0xAC220B, "Asus"}, // AC:22:0B
    {0xACBC32, "Apple"}, // AC:BC:32
    {0xACCC8E, "Axis"}, // AC:CC:8E
    {0xB0AA77, "Cisco"}, // B0:AA:77
    {0xB4FBE4, "Ubiquiti"}, // B4:FB:E4
    {0xB81332, "Roku"}, // B8:13:32
    {0xB827EB, "Raspberry Pi"}, // B8:27:EB
    {0xB869F4, "MikroTik"}, // B8:69:F4
    {0xB8A44F, "Axis"}, // B8:A4:4F
    {0xB8CA3A, "Dell"}, // B8:CA:3A
    {0xBCDDC2, "Espressif"}, // BC:DD:C2
    {0xC03F0E, "Netgear"}, // C0:3F:0E
    {0xC056E3, "Hikvision"}, // C0:56:E3
    {0xC0A0BB, "D-Link"}, // C0:A0:BB
    {0xC43DC7, "Netgear"}, // C4:3D:C7
    {0xC44F33, "Espressif"}, // C4:4F:33
    {0xC83A35, "Tenda"}, // C8:3A:35
    {0xCC2DE0, "MikroTik"}, // CC:2D:E0
    {0xD0034B, "Apple"}, // D0:03:4B
    {0xD0DF9C, "Samsung"}, // D0:DF:9C
    {0xD4CA6D, "MikroTik"}, // D4:CA:6D
    {0xD8132A, "Espressif"}, // D8:13:2A
    {0xD83ADD, "Raspberry Pi"}, // D8:3A:DD
    {0xDCA632, "Raspberry Pi"}, // DC:A6:32
    {0xE05A1B, "Espressif"}, // E0:5A:1B
    {0xE063DA, "Ubiquiti"}, // E0:63:DA
    {0xE45F01, "Raspberry Pi"}, // E4:5F:01
    {0xE48D8C, "MikroTik"}, // E4:8D:8C
    {0xE4C32A, "TP-Link"}, // E4:C3:2A
    {0xEC1A59, "Belkin"}, // EC:1A:59
    {0xEC64C9, "Espressif"}, // EC:64:C9
    {0xF01898, "Apple"}, // F0:18:98
    {0xF02528, "Samsung"}, // F0:25:28
    {0xF09FC2, "Ubiquiti"}, // F0:9F:C2
    {0xF4EC38, "TP-Link"}, // F4:EC:38
    {0xF4F15A, "Apple"}, // F4:F1:5A
    {0xF4F5DB, "Google"}, // F4:F5:DB
    {0xFCECDA, "Ubiquiti"}, // FC:EC:DA
};

static constexpr std::size_t kOuiTableSize =
    sizeof(kOuiTable) / sizeof(kOuiTable[0]);

const OuiEntry* getOuiTable() {
    return kOuiTable;
}

std::size_t getOuiTableSize() {
    return kOuiTableSize;
}

uint32_t macToOui(const uint8_t mac[6]) {
    if (mac == nullptr) {
        return 0xFFFFFFu;
    }
    return (static_cast<uint32_t>(mac[0]) << 16) |
           (static_cast<uint32_t>(mac[1]) << 8) |
           static_cast<uint32_t>(mac[2]);
}

std::string lookupVendorByOui(uint32_t oui) {
    const OuiEntry* begin = kOuiTable;
    const OuiEntry* end = kOuiTable + kOuiTableSize;
    const OuiEntry* it = std::lower_bound(
        begin, end, oui,
        [](const OuiEntry& entry, uint32_t value) { return entry.oui < value; });
    if (it != end && it->oui == oui) {
        return std::string(it->vendor);
    }
    return std::string("Unknown");
}

std::string lookupVendorByMac(const uint8_t mac[6]) {
    if (mac == nullptr) {
        return std::string("Unknown");
    }
    return lookupVendorByOui(macToOui(mac));
}

} // namespace network
} // namespace cbdos
