#include "BleCompanyDatabase.hpp"

#include <algorithm>

namespace cbdos {
namespace network {

// Tabla estática en Flash (.rodata): 0 bytes en RAM dinámica (Heap).
// ORDENADA estrictamente por companyId ascendente para std::lower_bound O(log N).
static const BleCompanyEntry kBleCompanyTable[] = {
    {0x0001, "Nokia"},
    {0x0002, "Intel"},
    {0x0003, "IBM"},
    {0x0004, "Toshiba"},
    {0x0006, "Microsoft"},
    {0x000A, "Qualcomm"},
    {0x000D, "Texas Instruments"},
    {0x001D, "Qualcomm Atheros"},
    {0x002D, "Sony Mobile"},
    {0x004C, "Apple, Inc."},
    {0x0056, "Sony Ericsson"},
    {0x0057, "Harman International"},
    {0x0059, "Nordic Semiconductor"},
    {0x006B, "Polar Electro"},
    {0x0075, "Samsung Electronics"},
    {0x0078, "Nike, Inc."},
    {0x0087, "Garmin International"},
    {0x008A, "Jawbone"},
    {0x009E, "Bose Corporation"},
    {0x00A4, "Telit Wireless"},
    {0x00BF, "Logitech International"},
    {0x00C7, "Seiko Epson"},
    {0x00C9, "Quuppa"},
    {0x00D2, "Dialog Semiconductor"},
    {0x00D6, "Tile, Inc."},
    {0x00E0, "Google LLC"},
    {0x00F0, "Sony Mobile"},
    {0x0113, "Lenovo"},
    {0x011A, "Renesas Electronics"},
    {0x012D, "Sony Group"},
    {0x0131, "Cypress Semiconductor"},
    {0x013A, "Casio Computer"},
    {0x0157, "Anki, Inc."},
    {0x015D, "Bose Corporation"},
    {0x0171, "Amazon.com Services"},
    {0x0181, "Snap Inc."},
    {0x019A, "Razer Inc."},
    {0x01A7, "Anker Innovations"},
    {0x01AB, "Huawei Technologies"},
    {0x01D6, "Beats Electronics"},
    {0x01E8, "Siemens AG"},
    {0x01F0, "LG Electronics"},
    {0x0211, "ASUSTek Computer"},
    {0x0225, "OnePlus"},
    {0x027D, "Huawei Device"},
    {0x02DE, "DJI Technology"},
    {0x02E5, "Espressif Systems"},
    {0x02FE, "Poco / Xiaomi"},
    {0x038F, "Xiaomi Inc."},
    {0x0392, "Telink Semiconductor"},
    {0x0399, "Nura Operations"},
    {0x03C2, "Baidu Online"},
    {0x0440, "Skullcandy"},
    {0x0499, "Flipper Devices Inc."},
    {0x04A6, "Sonos, Inc."},
    {0x04FA, "Tuya Global"},
    {0x0529, "Sennheiser Electronic"},
    {0x0553, "Nintendo Co., Ltd."},
    {0x058A, "Meta Platforms, Inc."},
    {0x0602, "Zepp Health / Amazfit"},
    {0x064B, "Nothing Technology"},
    {0x066C, "Realtek Semiconductor"},
    {0x08F5, "Luxottica Group (Ray-Ban)"},
    {0x0926, "JBL / Harman"},
    {0x09FE, "Pico Immersive"},
    {0x0A2B, "Whoop, Inc."},
    {0x0AB1, "Valve Corporation"},
    {0x0B5B, "Bang & Olufsen"}
};

static constexpr std::size_t kBleCompanyTableSize =
    sizeof(kBleCompanyTable) / sizeof(kBleCompanyTable[0]);

const BleCompanyEntry* getBleCompanyTable() {
    return kBleCompanyTable;
}

std::size_t getBleCompanyTableSize() {
    return kBleCompanyTableSize;
}

std::string lookupVendorByCompanyId(uint16_t companyId) {
    const BleCompanyEntry* begin = kBleCompanyTable;
    const BleCompanyEntry* end = kBleCompanyTable + kBleCompanyTableSize;
    const BleCompanyEntry* it = std::lower_bound(
        begin, end, companyId,
        [](const BleCompanyEntry& entry, uint16_t value) {
            return entry.companyId < value;
        });

    if (it != end && it->companyId == companyId) {
        return std::string(it->vendor);
    }
    return std::string("Unknown");
}

} // namespace network
} // namespace cbdos
