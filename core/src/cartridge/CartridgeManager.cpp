#include "cbdos/cartridge.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/system.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#ifdef ESP_PLATFORM
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#endif

static const char* TAG = "CartridgeMgr";

namespace cbdos {
namespace cartridge {

CartridgeSlotInfo CartridgeManager::getSlotInfo(esp_partition_subtype_t subtype) {
    CartridgeSlotInfo info;
    info.subtype = subtype;
    info.isInstalled = false;

#ifdef ESP_PLATFORM
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
    if (!part) {
        info.projectName = "No existe";
        return info;
    }
    info.partitionSize = part->size;

    esp_app_desc_t app_desc;
    esp_err_t err = esp_ota_get_partition_description(part, &app_desc);
    if (err == ESP_OK && app_desc.magic_word == ESP_APP_DESC_MAGIC_WORD) {
        info.isInstalled = true;
        info.projectName = app_desc.project_name;
        info.version = app_desc.version;
        info.compileDate = app_desc.date;
        info.compileTime = app_desc.time;
    } else {
        info.projectName = "Ranura Vacía";
    }
#else
    info.isInstalled = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1);
    info.projectName = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) ? "DOOM Classic" : "Ranura Vacía";
    info.version = "1.0.0";
    info.partitionSize = (subtype == ESP_PARTITION_SUBTYPE_APP_OTA_2) ? (2 * 1024 * 1024) : (4 * 1024 * 1024);
#endif

    return info;
}

bool CartridgeManager::isSlotInstalled(esp_partition_subtype_t subtype) {
    return getSlotInfo(subtype).isInstalled;
}

bool CartridgeManager::bootSlot(esp_partition_subtype_t subtype) {
#ifdef ESP_PLATFORM
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
    if (!part) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Particion 0x%x no encontrada", subtype);
        return false;
    }

    esp_app_desc_t app_desc;
    if (esp_ota_get_partition_description(part, &app_desc) != ESP_OK || app_desc.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Particion vacia o invalida en 0x%x", subtype);
        return false;
    }

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Arrancando %s v%s...", app_desc.project_name, app_desc.version);
    esp_err_t err = esp_ota_set_boot_partition(part);
    if (err == ESP_OK) {
        cbdos::system::sleepMs(300);
        cbdos::system::restart();
        return true;
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error al configurar arranque: %s", esp_err_to_name(err));
        return false;
    }
#else
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Simulación: Boot slot 0x%x", subtype);
    return true;
#endif
}

static std::string s_lastError = "";

std::string CartridgeManager::getLastError() {
    return s_lastError;
}

static std::string resolveSdPath(const std::string& path) {
    if (path.empty()) return "";
    
    // Ya tiene /sdcard/ o es exactamente /sdcard
    if (path.rfind("/sdcard/", 0) == 0 || path == "/sdcard") {
        return path;
    }
    // Prefijos de alias /sd/ o /sd
    if (path.rfind("/sd/", 0) == 0) {
        return "/sdcard/" + path.substr(4);
    }
    if (path == "/sd") {
        return "/sdcard";
    }
    // Prefijos de letra de unidad S:/ o A:/
    if (path.rfind("S:/", 0) == 0 || path.rfind("A:/", 0) == 0) {
        return "/sdcard/" + path.substr(3);
    }
    // Prefijo absoluto sin especificar /sdcard (ej. /cartridges/... o /doom.bin)
    if (path.front() == '/') {
        return "/sdcard" + path;
    }
    // Relativo (ej. cartridges/doom.bin)
    return "/sdcard/" + path;
}

std::vector<std::string> CartridgeManager::listBinFilesOnSD(const std::string& directory) {
    std::vector<std::string> list;

    std::vector<std::string> searchDirs;
    if (!directory.empty()) {
        searchDirs.push_back(resolveSdPath(directory));
    }
    searchDirs.push_back("/sdcard/cartridges");
    searchDirs.push_back("/sdcard/cartuchos");
    searchDirs.push_back("/sdcard");

    for (const auto& dir : searchDirs) {
        auto entries = cbdos::storage::listDir(dir.c_str());
        for (const auto& entry : entries) {
            if (entry.isDirectory) continue;
            std::string name = entry.name;
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.length() > 4 && lowerName.substr(lowerName.length() - 4) == ".bin") {
                std::string fullPath = dir;
                if (fullPath.empty() || fullPath.back() != '/') fullPath += "/";
                fullPath += name;
                
                // Evitar duplicados
                if (std::find(list.begin(), list.end(), fullPath) == list.end()) {
                    list.push_back(fullPath);
                }
            }
        }
        // Si encontramos archivos en la carpeta de cartuchos específica, priorizamos esa
        if (!list.empty() && (dir.find("cartridges") != std::string::npos || dir.find("cartuchos") != std::string::npos)) {
            break;
        }
    }

    return list;
}

bool CartridgeManager::flashFromSD(const std::string& sdPath, 
                                   esp_partition_subtype_t targetSlot, 
                                   std::function<void(size_t written, size_t total)> progressCb) {
    s_lastError = "";
#ifdef ESP_PLATFORM
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, targetSlot, NULL);
    if (!part) {
        s_lastError = "Particion destino no encontrada";
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
        return false;
    }

    std::string resolvedPath = resolveSdPath(sdPath);
    FILE* f = fopen(resolvedPath.c_str(), "rb");
    if (!f) {
        s_lastError = "No se pudo abrir: " + resolvedPath;
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
        return false;
    }

    size_t totalBytes = 0;
    struct stat st;
    if (fstat(fileno(f), &st) == 0 && st.st_size > 0) {
        totalBytes = (size_t)st.st_size;
    } else {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            totalBytes = (size_t)sz;
        }
    }

    if (totalBytes == 0) {
        fclose(f);
        s_lastError = "Archivo vacio o ilegible: " + resolvedPath;
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
        return false;
    }

    if (totalBytes > part->size) {
        fclose(f);
        char errBuf[64];
        snprintf(errBuf, sizeof(errBuf), "Tamano (%u KB) excede particion (%u KB)", 
                 (unsigned)(totalBytes / 1024), (unsigned)(part->size / 1024));
        s_lastError = errBuf;
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
        return false;
    }

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Iniciando OTA en 0x%06lx (%u bytes)...", 
                       (unsigned long)part->address, (unsigned)totalBytes);

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(part, totalBytes, &ota_handle);
    if (err != ESP_OK) {
        fclose(f);
        s_lastError = std::string("esp_ota_begin: ") + esp_err_to_name(err);
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
        return false;
    }

    const size_t CHUNK_SIZE = 4096;
    uint8_t* buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!buffer) {
        esp_ota_abort(ota_handle);
        fclose(f);
        s_lastError = "Sin RAM para buffer de flasheo";
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
        return false;
    }

    size_t bytesWritten = 0;
    bool success = true;

    while (bytesWritten < totalBytes) {
        size_t toRead = std::min((size_t)(totalBytes - bytesWritten), CHUNK_SIZE);
        size_t bytesRead = fread(buffer, 1, toRead, f);
        if (bytesRead == 0) {
            s_lastError = "Fallo de lectura en MicroSD";
            success = false;
            break;
        }

        err = esp_ota_write(ota_handle, (const void*)buffer, bytesRead);
        if (err != ESP_OK) {
            s_lastError = std::string("esp_ota_write: ") + esp_err_to_name(err);
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
            success = false;
            break;
        }

        bytesWritten += bytesRead;
        if (progressCb) {
            progressCb(bytesWritten, totalBytes);
        }

        // Ceder tiempo al planificador y al bus DMA de pantalla (MIPI-DPI a 60 FPS)
        cbdos::system::sleepMs(2);
    }

    free(buffer);
    fclose(f);

    if (success && bytesWritten == totalBytes) {
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            s_lastError = std::string("esp_ota_end (validacion): ") + esp_err_to_name(err);
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "%s", s_lastError.c_str());
            return false;
        }
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Flasheo completado y validado (%u bytes)", (unsigned)bytesWritten);
        return true;
    } else {
        esp_ota_abort(ota_handle);
        if (s_lastError.empty()) s_lastError = "Escritura incompleta en Flash";
        return false;
    }
#else
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Simulación: Flasheo desde SD %s a slot 0x%x", sdPath.c_str(), targetSlot);
    return true;
#endif
}

} // namespace cartridge
} // namespace cbdos
