#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>

#ifdef ESP_PLATFORM
#include "esp_partition.h"
#include "esp_ota_ops.h"
#else
typedef int esp_partition_subtype_t;
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
#define ESP_PARTITION_SUBTYPE_APP_OTA_2 0x12
#endif

namespace cbdos {
namespace cartridge {

struct CartridgeSlotInfo {
    bool isInstalled = false;
    std::string projectName = "Ranura Vacía";
    std::string version = "";
    std::string compileDate = "";
    std::string compileTime = "";
    size_t partitionSize = 0;
    esp_partition_subtype_t subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
};

class CartridgeManager {
public:
    // Obtener información de un slot (OTA_1 = Ranura 1, OTA_2 = Ranura 2)
    static CartridgeSlotInfo getSlotInfo(esp_partition_subtype_t subtype);

    // Verificar si un slot tiene un firmware válido instalado
    static bool isSlotInstalled(esp_partition_subtype_t subtype);

    // Reiniciar y arrancar el slot especificado
    static bool bootSlot(esp_partition_subtype_t subtype);

    // Listar todos los archivos .bin disponibles en la MicroSD
    static std::vector<std::string> listBinFilesOnSD(const std::string& directory = "/sdcard/cartridges");

    // Flashear un archivo binario desde la SD a la partición OTA especificada
    static bool flashFromSD(const std::string& sdPath, 
                            esp_partition_subtype_t targetSlot, 
                            std::function<void(size_t written, size_t total)> progressCb = nullptr);

    // Obtener el último mensaje de error detallado
    static std::string getLastError();
};

} // namespace cartridge
} // namespace cbdos
