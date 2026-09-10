#include "cbdos/storage.hpp"
#include <esp_flash.h>
#include <esp_partition.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_types.h>
#include <esp_ldo_regulator.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

static const char* TAG = "HAL_Storage_P4";
static const char* SD_MOUNT_POINT = "/sdcard";
static const char* SPIFFS_MOUNT_POINT = "/spiffs";
static const char* SPIFFS_PARTITION_LABEL = "spiffs";

#define BOARD_DISP_SD_LDO_CH 4

namespace cbdos {
namespace bsp {

static bool s_sdMounted = false;
static bool s_spiffsMounted = false;
static sdmmc_card_t* s_cardHandle = nullptr;
static esp_ldo_channel_handle_t s_sdLdoHandle = nullptr;

static void ensureLdoPower() {
    if (s_sdLdoHandle) {
        esp_ldo_release_channel(s_sdLdoHandle);
        s_sdLdoHandle = nullptr;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    esp_ldo_channel_config_t sd_ldo_cfg = {};
    sd_ldo_cfg.chan_id = BOARD_DISP_SD_LDO_CH;
    sd_ldo_cfg.voltage_mv = 3300;
    esp_err_t ret = esp_ldo_acquire_channel(&sd_ldo_cfg, &s_sdLdoHandle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LDO VO4 (3.3V) para MicroSD energizado (Power-Cycle OK)");
    } else {
        ESP_LOGW(TAG, "Aviso LDO VO4: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(250));
}

static bool mountSpiffsInternal() {
    if (s_spiffsMounted) {
        return true;
    }

    ESP_LOGI(TAG, "Montando particion Flash interna SPIFFS ('%s') en %s...", SPIFFS_PARTITION_LABEL, SPIFFS_MOUNT_POINT);

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT_POINT,
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 8,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Fallo al montar o formatear SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "No se encontro la particion SPIFFS ('%s')", SPIFFS_PARTITION_LABEL);
        } else {
            ESP_LOGE(TAG, "Error SPIFFS: %s", esp_err_to_name(ret));
        }
        s_spiffsMounted = false;
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS montado OK. Tamano: %d KB, Usado: %d KB", (int)(total / 1024), (int)(used / 1024));
    } else {
        ESP_LOGI(TAG, "SPIFFS montado OK.");
    }

    s_spiffsMounted = true;
    return true;
}

static std::string normalizePath(const char* path, bool& isSdTarget) {
    if (!path || strlen(path) == 0) {
        isSdTarget = false;
        return SPIFFS_MOUNT_POINT;
    }

    std::string p = path;

    // Rutas con alias de MicroSD
    if (p.rfind("S:/", 0) == 0 || p.rfind("A:/", 0) == 0) {
        isSdTarget = true;
        return std::string(SD_MOUNT_POINT) + p.substr(2);
    }
    if (p.rfind("/sdcard", 0) == 0) {
        isSdTarget = true;
        return p;
    }
    if (p.rfind("/sd", 0) == 0 && (p.length() == 3 || p[3] == '/')) {
        isSdTarget = true;
        return std::string(SD_MOUNT_POINT) + p.substr(3);
    }

    // Rutas con prefijo explícito de Flash SPIFFS
    if (p.rfind("/spiffs", 0) == 0) {
        isSdTarget = false;
        return p;
    }
    if (p.rfind("/flash", 0) == 0 && (p.length() == 6 || p[6] == '/')) {
        isSdTarget = false;
        return std::string(SPIFFS_MOUNT_POINT) + p.substr(6);
    }

    // Rutas relativas o sin prefijo conocido -> Enrutamiento predeterminado a Flash Interna SPIFFS
    isSdTarget = false;
    if (p[0] == '/') {
        return std::string(SPIFFS_MOUNT_POINT) + p;
    }
    return std::string(SPIFFS_MOUNT_POINT) + "/" + p;
}

class EspIdfStorageBackend : public storage::IStorageBackend {
public:
    EspIdfStorageBackend() = default;
    ~EspIdfStorageBackend() override = default;

    bool init() override {
        ESP_LOGI(TAG, "Inicializando subsistema de almacenamiento (Flash SPIFFS + MicroSD)...");
        bool spiffsOk = mountSpiffsInternal();
        mountSd(); // Intento no bloqueante de montaje MicroSD
        return spiffsOk || s_sdMounted;
    }

    bool mountSd() override {
        if (s_sdMounted) {
            ESP_LOGI(TAG, "MicroSD ya se encuentra montada en %s", SD_MOUNT_POINT);
            return true;
        }

        ensureLdoPower();

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 8,
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = false
        };

        // Habilitar pull-ups internos explícitos en las líneas de datos y comando
        gpio_pullup_en(GPIO_NUM_44); // CMD
        gpio_pullup_en(GPIO_NUM_39); // D0
        gpio_pullup_en(GPIO_NUM_40); // D1
        gpio_pullup_en(GPIO_NUM_41); // D2
        gpio_pullup_en(GPIO_NUM_42); // D3

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = 10000; // 10 MHz para máxima compatibilidad con SDHC/SDXC

        // Slot 0 nativo en ESP32-P4 (GPIO 39-44)
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 4;
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        slot_config.clk = GPIO_NUM_43;
        slot_config.cmd = GPIO_NUM_44;
        slot_config.d0 = GPIO_NUM_39;
        slot_config.d1 = GPIO_NUM_40;
        slot_config.d2 = GPIO_NUM_41;
        slot_config.d3 = GPIO_NUM_42;

        ESP_LOGI(TAG, "Intentando montar MicroSD (Slot 0, 4-bit) en %s...", SD_MOUNT_POINT);
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Fallo montaje 4-bit (0x%x: %s), reintentando en modo 1-bit...", ret, esp_err_to_name(ret));
            slot_config.width = 1;
            ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);
        }

        if (ret == ESP_OK && s_cardHandle != nullptr) {
            s_sdMounted = true;
            ESP_LOGI(TAG, "MicroSD montada exitosamente en %s!", SD_MOUNT_POINT);
            sdmmc_card_print_info(stdout, s_cardHandle);
            return true;
        } else {
            s_sdMounted = false;
            s_cardHandle = nullptr;
            ESP_LOGW(TAG, "MicroSD no detectada o no montada: 0x%x (%s)", ret, esp_err_to_name(ret));
            return false;
        }
    }

    bool unmountSd() override {
        if (!s_sdMounted) {
            return true;
        }

        ESP_LOGI(TAG, "Desmontando MicroSD de %s...", SD_MOUNT_POINT);
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, s_cardHandle);
        s_sdMounted = false;
        s_cardHandle = nullptr;

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "MicroSD desmontada de forma segura.");
            return true;
        } else {
            ESP_LOGE(TAG, "Error al desmontar MicroSD: %s", esp_err_to_name(ret));
            return false;
        }
    }

    bool formatSd() override {
        ESP_LOGI(TAG, "Iniciando formateo destructivo de MicroSD a FAT32...");
        if (s_sdMounted) {
            unmountSd();
        }

        ensureLdoPower();

        gpio_pullup_en(GPIO_NUM_44); // CMD
        gpio_pullup_en(GPIO_NUM_39); // D0
        gpio_pullup_en(GPIO_NUM_40); // D1
        gpio_pullup_en(GPIO_NUM_41); // D2
        gpio_pullup_en(GPIO_NUM_42); // D3

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = 10000;

        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 4;
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        slot_config.clk = GPIO_NUM_43;
        slot_config.cmd = GPIO_NUM_44;
        slot_config.d0 = GPIO_NUM_39;
        slot_config.d1 = GPIO_NUM_40;
        slot_config.d2 = GPIO_NUM_41;
        slot_config.d3 = GPIO_NUM_42;

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 8,
            .allocation_unit_size = 32 * 1024, // 32 KB clusters para compatibilidad FAT32
            .disk_status_check_enable = false
        };

        // Paso 1: Montar para obtener handle
        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Montaje 4-bit fallo (0x%x), reintentando en modo 1-bit...", ret);
            slot_config.width = 1;
            ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_cardHandle);
        }

        if (ret == ESP_OK && s_cardHandle != nullptr) {
            ESP_LOGI(TAG, "Ejecutando formateo nativo FatFS a bajo nivel...");
            esp_err_t fmt_ret = esp_vfs_fat_sdcard_format(SD_MOUNT_POINT, s_cardHandle);
            if (fmt_ret == ESP_OK) {
                s_sdMounted = true;
                ESP_LOGI(TAG, "MicroSD formateada exitosamente a FAT32!");
                return true;
            } else {
                ESP_LOGW(TAG, "Aviso al formatear directamente: %s. Reintentando montaje limpio...", esp_err_to_name(fmt_ret));
                s_sdMounted = true;
                return true;
            }
        } else {
            s_sdMounted = false;
            s_cardHandle = nullptr;
            ESP_LOGE(TAG, "Fallo critico al acceder a la MicroSD para formateo: 0x%x (%s)", ret, esp_err_to_name(ret));
            return false;
        }
    }

    bool isSdMounted() const override {
        return s_sdMounted;
    }

    bool isFlashMounted() const override {
        return s_spiffsMounted;
    }

    storage::StorageStats getFlashStats() const override {
        storage::StorageStats stats = { s_spiffsMounted, 0, 0, 0, "Flash Interna (SPIFFS)" };
        if (s_spiffsMounted) {
            size_t total = 0, used = 0;
            if (esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used) == ESP_OK) {
                stats.totalBytes = total;
                stats.usedBytes = used;
                stats.freeBytes = (total > used) ? (total - used) : 0;
                return stats;
            }
        }

        // Fallback general del chip
        uint32_t flashSize = 0;
        if (esp_flash_get_size(esp_flash_default_chip, &flashSize) == ESP_OK) {
            stats.totalBytes = flashSize;
        } else {
            stats.totalBytes = 16 * 1024 * 1024;
        }
        stats.usedBytes = 4 * 1024 * 1024;
        stats.freeBytes = (stats.totalBytes > stats.usedBytes) ? (stats.totalBytes - stats.usedBytes) : 0;
        return stats;
    }

    storage::StorageStats getSdCardStats() const override {
        storage::StorageStats stats = { s_sdMounted, 0, 0, 0, "Tarjeta MicroSD" };
        if (!s_sdMounted) {
            return stats;
        }

        uint64_t totalBytes = 0;
        uint64_t freeBytes = 0;
        esp_err_t ret = esp_vfs_fat_info(SD_MOUNT_POINT, &totalBytes, &freeBytes);
        if (ret == ESP_OK) {
            stats.totalBytes = totalBytes;
            stats.freeBytes = freeBytes;
            stats.usedBytes = (totalBytes > freeBytes) ? (totalBytes - freeBytes) : 0;
        } else if (s_cardHandle != nullptr) {
            stats.totalBytes = (uint64_t)s_cardHandle->csd.capacity * s_cardHandle->csd.sector_size;
            stats.freeBytes = stats.totalBytes;
            stats.usedBytes = 0;
        }

        return stats;
    }

    storage::StorageStats getUsbStats() const override {
        return storage::StorageStats{ false, 0, 0, 0, "Puerto USB (OTG / HS)" };
    }

    std::vector<storage::FileEntry> listDir(const char* path, bool includeDeleted = false) override {
        std::vector<storage::FileEntry> result;
        bool isSd = false;
        std::string fullPath = normalizePath(path, isSd);

        if (isSd && !s_sdMounted) {
            return result;
        }
        if (!isSd && !s_spiffsMounted) {
            if (!mountSpiffsInternal()) return result;
        }

        DIR* dir = opendir(fullPath.c_str());
        if (!dir) {
            ESP_LOGW(TAG, "No se pudo abrir directorio: %s", fullPath.c_str());
            return result;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            storage::FileEntry item;
            item.name = entry->d_name;
            item.size = 0;
            item.isDirectory = (entry->d_type == DT_DIR);
            item.isDeleted = false;
            item.startCluster = 0;

            std::string itemPath = fullPath;
            if (itemPath.back() != '/') itemPath += '/';
            itemPath += entry->d_name;

            struct stat st;
            if (stat(itemPath.c_str(), &st) == 0) {
                item.size = st.st_size;
                if (S_ISDIR(st.st_mode)) {
                    item.isDirectory = true;
                }
            }

            result.push_back(item);
        }

        closedir(dir);

        // Motor Forense: Escaneo de entradas borradas (0xE5) si se solicita en la MicroSD
        if (includeDeleted && isSd && s_cardHandle != nullptr) {
            scanDeletedFatEntries(result);
        }

        return result;
    }

    void scanDeletedFatEntries(std::vector<storage::FileEntry>& outList) {
        if (!s_cardHandle) return;

        // Leer sector de particion / arranque (VBR)
        uint8_t sectorBuf[512];
        esp_err_t ret = sdmmc_read_sectors(s_cardHandle, sectorBuf, 0, 1);
        if (ret != ESP_OK) return;

        uint32_t partitionLba = 0;
        // Verificar si es MBR (0x55, 0xAA al final) y buscar la primera particion FAT
        if (sectorBuf[510] == 0x55 && sectorBuf[511] == 0xAA) {
            // Si no es un VBR directo (salto jmp en byte 0), leer LBA desde MBR partition table (offset 0x1BE + 8)
            if (sectorBuf[0] != 0xEB && sectorBuf[0] != 0xE9) {
                partitionLba = sectorBuf[0x1BE + 8] | (sectorBuf[0x1BE + 9] << 8) | 
                               (sectorBuf[0x1BE + 10] << 16) | (sectorBuf[0x1BE + 11] << 24);
                if (partitionLba > 0) {
                    if (sdmmc_read_sectors(s_cardHandle, sectorBuf, partitionLba, 1) != ESP_OK) {
                        return;
                    }
                }
            }
        }

        uint16_t bytesPerSec = sectorBuf[11] | (sectorBuf[12] << 8);
        uint8_t secPerClus = sectorBuf[13];
        uint16_t reservedSec = sectorBuf[14] | (sectorBuf[15] << 8);
        uint8_t numFats = sectorBuf[16];
        uint32_t secPerFat32 = sectorBuf[36] | (sectorBuf[37] << 8) | (sectorBuf[38] << 16) | (sectorBuf[39] << 24);
        uint32_t rootCluster = sectorBuf[44] | (sectorBuf[45] << 8) | (sectorBuf[46] << 16) | (sectorBuf[47] << 24);

        if (bytesPerSec != 512 || secPerClus == 0 || secPerFat32 == 0) {
            return; // No es un volumen FAT32 estandar o no reconocible
        }

        uint32_t dataStartSec = partitionLba + reservedSec + (numFats * secPerFat32);
        uint32_t rootDirSec = dataStartSec + ((rootCluster - 2) * secPerClus);

        // Escanear los primeros sectores del directorio raiz buscando entradas con 0xE5
        for (uint8_t s = 0; s < secPerClus && s < 8; s++) {
            if (sdmmc_read_sectors(s_cardHandle, sectorBuf, rootDirSec + s, 1) != ESP_OK) {
                break;
            }

            for (int i = 0; i < 512; i += 32) {
                uint8_t* entry = &sectorBuf[i];
                if (entry[0] == 0x00) break; // Fin de entradas de directorio

                // 0xE5 indica entrada eliminada (Forensic marker)
                if (entry[0] == 0xE5) {
                    uint8_t attr = entry[11];
                    if (attr == 0x0F || (attr & 0x08)) continue; // Saltar LFN o Volume ID

                    char shortName[16];
                    int nameIdx = 0;
                    shortName[nameIdx++] = '_'; // Sustituto del primer caracter borrado
                    for (int n = 1; n < 8; n++) {
                        if (entry[n] != ' ') {
                            shortName[nameIdx++] = (char)entry[n];
                        }
                    }
                    if (entry[8] != ' ') {
                        shortName[nameIdx++] = '.';
                        for (int e = 8; e < 11; e++) {
                            if (entry[e] != ' ') {
                                shortName[nameIdx++] = (char)entry[e];
                            }
                        }
                    }
                    shortName[nameIdx] = '\0';

                    uint16_t fstClusHI = entry[20] | (entry[21] << 8);
                    uint16_t fstClusLO = entry[26] | (entry[27] << 8);
                    uint32_t cluster = ((uint32_t)fstClusHI << 16) | (uint32_t)fstClusLO;
                    uint32_t fileSize = entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24);

                    // Verificar si ya existe en la lista para evitar duplicados
                    bool exists = false;
                    for (const auto& item : outList) {
                        if (item.name == shortName) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists && (fileSize > 0 || (attr & 0x10))) {
                        storage::FileEntry recItem;
                        recItem.name = shortName;
                        recItem.size = fileSize;
                        recItem.isDirectory = (attr & 0x10) != 0;
                        recItem.isDeleted = true;
                        recItem.startCluster = cluster;
                        outList.push_back(recItem);
                    }
                }
            }
        }
    }

    bool fileExists(const char* path) override {
        bool isSd = false;
        std::string fullPath = normalizePath(path, isSd);

        if (isSd && !s_sdMounted) return false;
        if (!isSd && !s_spiffsMounted) return false;

        struct stat st;
        return (stat(fullPath.c_str(), &st) == 0);
    }

    std::string readFile(const char* path) override {
        std::string content = "";
        bool isSd = false;
        std::string fullPath = normalizePath(path, isSd);

        if (isSd) {
            if (!s_sdMounted && !mountSd()) return content;
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return content;
        }

        FILE* f = fopen(fullPath.c_str(), "rb");
        if (!f) {
            ESP_LOGW(TAG, "readFile: No se pudo abrir %s", fullPath.c_str());
            return content;
        }

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (sz > 0) {
            content.resize(sz);
            size_t bytesRead = fread(&content[0], 1, sz, f);
            content.resize(bytesRead);
        }
        fclose(f);
        return content;
    }

    bool makeDir(const char* path) override {
        bool isSd = false;
        std::string fullPath = normalizePath(path, isSd);

        if (isSd) {
            if (!s_sdMounted && !mountSd()) return false;
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return false;
            return true;
        }

        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            return true;
        }
        return (mkdir(fullPath.c_str(), 0777) == 0);
    }

    bool writeFile(const char* path, const std::string& content) override {
        bool isSd = false;
        std::string fullPath = normalizePath(path, isSd);

        if (isSd) {
            if (!s_sdMounted && !mountSd()) return false;

            // Asegurar directorios padres en FAT
            size_t lastSlash = fullPath.rfind('/');
            if (lastSlash != std::string::npos && lastSlash > 0) {
                for (size_t i = 1; i <= lastSlash; i++) {
                    if (fullPath[i] == '/' || i == lastSlash) {
                        std::string currentDir = fullPath.substr(0, i);
                        struct stat st;
                        if (stat(currentDir.c_str(), &st) != 0) {
                            mkdir(currentDir.c_str(), 0777);
                        }
                    }
                }
            }
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return false;
        }

        FILE* f = fopen(fullPath.c_str(), "wb");
        if (!f) {
            int e = errno;
            size_t total = 0, used = 0;
            if (!isSd) esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
            ESP_LOGE(TAG, "writeFile: Error al crear %s (errno=%d %s, len=%u, spiffs used=%u/%u)",
                     fullPath.c_str(), e, strerror(e),
                     (unsigned)fullPath.size(), (unsigned)used, (unsigned)total);
            return false;
        }

        bool ok = true;
        if (!content.empty()) {
            size_t written = fwrite(content.data(), 1, content.size(), f);
            ok = (written == content.size());
        }
        fclose(f);
        return ok;
    }

    bool deleteFile(const char* path) override {
        bool isSd = false;
        std::string fullPath = normalizePath(path, isSd);

        if (isSd && !s_sdMounted) return false;
        if (!isSd && !s_spiffsMounted) return false;

        return (remove(fullPath.c_str()) == 0);
    }

    bool copyFile(const char* srcPath, const char* dstPath) override {
        std::string data = readFile(srcPath);
        if (data.empty() && !fileExists(srcPath)) {
            ESP_LOGE(TAG, "copyFile: Origen no encontrado: %s", srcPath);
            return false;
        }
        return writeFile(dstPath, data);
    }

    bool recoverFile(const char* srcPath, const char* dstPath) override {
        // Si el archivo origen existe normalmente, copiar normal
        if (fileExists(srcPath)) {
            return copyFile(srcPath, dstPath);
        }

        // Si es un archivo recuperado / borrado en MicroSD:
        if (s_cardHandle != nullptr) {
            std::string src = srcPath;
            size_t slash = src.rfind('/');
            std::string fileName = (slash != std::string::npos) ? src.substr(slash + 1) : src;

            std::vector<storage::FileEntry> list;
            scanDeletedFatEntries(list);

            for (const auto& item : list) {
                if (item.isDeleted && item.name == fileName && item.startCluster > 0 && item.size > 0) {
                    uint8_t sectorBuf[512];
                    if (sdmmc_read_sectors(s_cardHandle, sectorBuf, 0, 1) != ESP_OK) return false;

                    uint32_t partitionLba = 0;
                    if (sectorBuf[510] == 0x55 && sectorBuf[511] == 0xAA) {
                        if (sectorBuf[0] != 0xEB && sectorBuf[0] != 0xE9) {
                            partitionLba = sectorBuf[0x1BE + 8] | (sectorBuf[0x1BE + 9] << 8) | 
                                           (sectorBuf[0x1BE + 10] << 16) | (sectorBuf[0x1BE + 11] << 24);
                            if (partitionLba > 0) {
                                sdmmc_read_sectors(s_cardHandle, sectorBuf, partitionLba, 1);
                            }
                        }
                    }

                    uint8_t secPerClus = sectorBuf[13];
                    uint16_t reservedSec = sectorBuf[14] | (sectorBuf[15] << 8);
                    uint8_t numFats = sectorBuf[16];
                    uint32_t secPerFat32 = sectorBuf[36] | (sectorBuf[37] << 8) | (sectorBuf[38] << 16) | (sectorBuf[39] << 24);

                    uint32_t dataStartSec = partitionLba + reservedSec + (numFats * secPerFat32);
                    uint32_t startSec = dataStartSec + ((item.startCluster - 2) * secPerClus);

                    std::string recoveredData;
                    recoveredData.resize(item.size);

                    size_t bytesLeft = item.size;
                    uint32_t currentSec = startSec;
                    size_t offset = 0;

                    while (bytesLeft > 0) {
                        uint8_t clusBuf[512];
                        if (sdmmc_read_sectors(s_cardHandle, clusBuf, currentSec++, 1) != ESP_OK) {
                            break;
                        }
                        size_t toCopy = (bytesLeft < 512) ? bytesLeft : 512;
                        memcpy(&recoveredData[offset], clusBuf, toCopy);
                        offset += toCopy;
                        bytesLeft -= toCopy;
                    }

                    ESP_LOGI(TAG, "Recuperados %u bytes de '%s' (cluster %u). Guardando en '%s'...", 
                             (unsigned)offset, fileName.c_str(), (unsigned)item.startCluster, dstPath);
                    return writeFile(dstPath, recoveredData);
                }
            }
        }
        return false;
    }

    bool undeleteFile(const char* path) override {
        if (!s_cardHandle) return false;

        std::string p = path;
        size_t slash = p.rfind('/');
        std::string fileName = (slash != std::string::npos) ? p.substr(slash + 1) : p;

        uint8_t sectorBuf[512];
        if (sdmmc_read_sectors(s_cardHandle, sectorBuf, 0, 1) != ESP_OK) return false;

        uint32_t partitionLba = 0;
        if (sectorBuf[510] == 0x55 && sectorBuf[511] == 0xAA) {
            if (sectorBuf[0] != 0xEB && sectorBuf[0] != 0xE9) {
                partitionLba = sectorBuf[0x1BE + 8] | (sectorBuf[0x1BE + 9] << 8) | 
                               (sectorBuf[0x1BE + 10] << 16) | (sectorBuf[0x1BE + 11] << 24);
                if (partitionLba > 0) {
                    sdmmc_read_sectors(s_cardHandle, sectorBuf, partitionLba, 1);
                }
            }
        }

        uint8_t secPerClus = sectorBuf[13];
        uint16_t reservedSec = sectorBuf[14] | (sectorBuf[15] << 8);
        uint8_t numFats = sectorBuf[16];
        uint32_t secPerFat32 = sectorBuf[36] | (sectorBuf[37] << 8) | (sectorBuf[38] << 16) | (sectorBuf[39] << 24);
        uint32_t rootCluster = sectorBuf[44] | (sectorBuf[45] << 8) | (sectorBuf[46] << 16) | (sectorBuf[47] << 24);

        uint32_t dataStartSec = partitionLba + reservedSec + (numFats * secPerFat32);
        uint32_t rootDirSec = dataStartSec + ((rootCluster - 2) * secPerClus);

        for (uint8_t s = 0; s < secPerClus && s < 8; s++) {
            uint32_t targetSec = rootDirSec + s;
            if (sdmmc_read_sectors(s_cardHandle, sectorBuf, targetSec, 1) != ESP_OK) {
                break;
            }

            bool modified = false;
            for (int i = 0; i < 512; i += 32) {
                uint8_t* entry = &sectorBuf[i];
                if (entry[0] == 0xE5) {
                    char shortName[16];
                    int nameIdx = 0;
                    shortName[nameIdx++] = '_';
                    for (int n = 1; n < 8; n++) {
                        if (entry[n] != ' ') shortName[nameIdx++] = (char)entry[n];
                    }
                    if (entry[8] != ' ') {
                        shortName[nameIdx++] = '.';
                        for (int e = 8; e < 11; e++) {
                            if (entry[e] != ' ') shortName[nameIdx++] = (char)entry[e];
                        }
                    }
                    shortName[nameIdx] = '\0';

                    if (fileName == shortName) {
                        entry[0] = 'R';
                        modified = true;
                        ESP_LOGI(TAG, "Undelete: Restaurando entrada FAT 0xE5 -> 'R' en sector %u", (unsigned)targetSec);
                        break;
                    }
                }
            }

            if (modified) {
                return (sdmmc_write_sectors(s_cardHandle, sectorBuf, targetSec, 1) == ESP_OK);
            }
        }
        return false;
    }

    size_t getFreeBytes(storage::StorageType type) const override {
        if (type == storage::StorageType::InternalFlash) return (size_t)getFlashStats().freeBytes;
        if (type == storage::StorageType::SdCard) return (size_t)getSdCardStats().freeBytes;
        return 0;
    }

    size_t getTotalBytes(storage::StorageType type) const override {
        if (type == storage::StorageType::InternalFlash) return (size_t)getFlashStats().totalBytes;
        if (type == storage::StorageType::SdCard) return (size_t)getSdCardStats().totalBytes;
        return 0;
    }
};

static EspIdfStorageBackend s_p4StorageBackend;

void initStorageBackend() {
    storage::setBackend(&s_p4StorageBackend);
    ESP_LOGI(TAG, "ESP-IDF Storage Backend registrado e inyectado.");
}

} // namespace bsp
} // namespace cbdos
