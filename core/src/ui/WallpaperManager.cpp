#include "WallpaperManager.h"
#include "components/AnimatedWallpaper.hpp"
#include "cbdos/memory.hpp"
#include "cbdos/log.hpp"
#include "cbdos/persistence.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(ARDUINO)
#include <SD.h>
#include <LittleFS.h>
#ifndef lv_fs_spi_lock
#define lv_fs_spi_lock() do{}while(0)
#define lv_fs_spi_unlock() do{}while(0)
#endif
#endif

static const char* TAG = "Wallpaper";
static const size_t WALLPAPER_WIDTH = 320;
static const size_t WALLPAPER_HEIGHT = 480;
static const size_t WALLPAPER_BUFFER_SIZE = WALLPAPER_WIDTH * WALLPAPER_HEIGHT * 2; // 307200 bytes

WallpaperManager::WallpaperManager() 
    : currentPath("animated_constellation"), hasCustomWallpaper(false), customBuffer(nullptr) {
    memset(&customDsc, 0, sizeof(customDsc));
}

WallpaperManager::~WallpaperManager() {
    if (customBuffer) {
        cbdos::mem::free_mem(customBuffer);
        customBuffer = nullptr;
    }
}

bool WallpaperManager::ensureBufferAllocated() {
    if (customBuffer) return true;
    customBuffer = (uint8_t*)cbdos::mem::alloc_psram(WALLPAPER_BUFFER_SIZE);
    if (!customBuffer) {
        CBD_LOG_E(TAG, "No se pudo asignar memoria para el fondo en PSRAM");
        return false;
    }

    customDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    customDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    customDsc.header.flags = 0;
    customDsc.header.w = WALLPAPER_WIDTH;
    customDsc.header.h = WALLPAPER_HEIGHT;
    customDsc.header.stride = WALLPAPER_WIDTH * 2;
    customDsc.header.reserved_2 = 0;
    customDsc.data_size = WALLPAPER_BUFFER_SIZE;
    customDsc.data = customBuffer;
    customDsc.reserved = NULL;
    return true;
}

bool WallpaperManager::loadFromFlash() {
#if defined(ARDUINO)
    if (!LittleFS.exists("/wallpaper.bin")) {
        return false;
    }
    File f = LittleFS.open("/wallpaper.bin", "r");
    if (!f) {
        Serial.println("[Wallpaper] No se pudo abrir /wallpaper.bin desde LittleFS");
        return false;
    }
    if (f.size() < WALLPAPER_BUFFER_SIZE) {
        Serial.printf("[Wallpaper] /wallpaper.bin tamaño invalido: %u\n", (unsigned int)f.size());
        f.close();
        return false;
    }
    if (!ensureBufferAllocated()) {
        f.close();
        return false;
    }
    size_t bytesRead = f.read(customBuffer, WALLPAPER_BUFFER_SIZE);
    f.close();
    if (bytesRead == WALLPAPER_BUFFER_SIZE) {
        hasCustomWallpaper = true;
        Serial.println("[Wallpaper] Fondo cargado exitosamente desde LittleFS a PSRAM");
        return true;
    }
#endif
    return false;
}

void WallpaperManager::init() {
    auto* backend = cbdos::persistence::getBackend();
    bool isCustom = false;
    // Sin RGB embebido en firmware: el fondo por defecto es animado.
    // El RGB solo vive en /wallpaper.bin (SPIFFS) copiado desde MicroSD.
    currentPath = "animated_constellation";
    hasCustomWallpaper = false;

    if (backend && backend->begin("wallpaper", true)) {
        isCustom = backend->getBool("custom", false);
        currentPath = backend->getString("path", "animated_constellation");
        backend->end();
    }

    // Migracion: instalaciones antiguas guardaban "default" (RGB embebido ya eliminado)
    if (currentPath == "default" || currentPath.empty()) {
        currentPath = "animated_constellation";
    }

    if (isCustom) {
        if (!loadFromFlash()) {
            hasCustomWallpaper = false;
            currentPath = "animated_constellation";
        }
    }
}

void WallpaperManager::applyWallpaper(lv_obj_t* parent) {
    if (!parent) return;

    if (currentPath == "animated" || currentPath == "animated_constellation") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::Constellation);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (currentPath == "animated_waves") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::Waves);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (currentPath == "animated_comic") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::ComicDrive);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (currentPath == "animated_fireflies") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::Fireflies);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (currentPath == "animated_touch") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::TouchSwarm);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (currentPath == "animated_synthwave") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::Synthwave80s);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (currentPath == "animated_matrix") {
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::MatrixRain);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    } else if (hasCustomWallpaper && customBuffer) {
        cbdos::ui::AnimatedWallpaper::getInstance().destroy();
        lv_obj_set_style_bg_image_src(parent, &customDsc, 0);
        lv_obj_set_style_bg_image_opa(parent, LV_OPA_COVER, 0);
    } else {
        // Fallback sin RGB embebido: animado Constellation (cero bytes en particion app)
        cbdos::ui::AnimatedWallpaper::getInstance().setStyle(cbdos::ui::AnimatedWallpaper::Style::Constellation);
        cbdos::ui::AnimatedWallpaper::getInstance().init(parent);
    }
}

void WallpaperManager::setAnimatedMode(lv_obj_t* parent, const std::string& mode) {
    currentPath = mode.empty() ? "animated_constellation" : mode;
    hasCustomWallpaper = false;
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("wallpaper", false)) {
        backend->setBool("custom", false);
        backend->setString("path", currentPath);
        backend->end();
    }
    if (parent) {
        applyWallpaper(parent);
    }
}

bool WallpaperManager::setWallpaper(const std::string& path) {
    if (path.rfind("animated", 0) == 0) {
        setAnimatedMode(nullptr, path);
        return true;
    }
    if (path == "default" || path.empty()) {
        // Compat: "default" ahora equivale al animado por defecto (sin RGB embebido)
        restoreDefault();
        return true;
    }
#if defined(ARDUINO)

    std::string sdPath = path;
    if (sdPath.rfind("A:/", 0) == 0) {
        sdPath = sdPath.substr(2); // Quitar "A:"
    }
    if (sdPath.rfind("S:/", 0) == 0) {
        sdPath = sdPath.substr(2); // Quitar "S:"
    }

    if (!ensureBufferAllocated()) {
        return false;
    }

    lv_fs_spi_lock();
    File f = SD.open(sdPath.c_str(), "r");
    if (!f) {
        lv_fs_spi_unlock();
        Serial.printf("[Wallpaper] ERROR: No se pudo abrir %s en SD\n", sdPath.c_str());
        return false;
    }

    size_t fSize = f.size();
    std::string lowerPath = sdPath;
    for (auto& c : lowerPath) c = tolower(c);

    bool loadSuccess = false;

    if (lowerPath.rfind(".bin") != std::string::npos) {
        if (fSize == WALLPAPER_BUFFER_SIZE + 12) {
            // Tiene cabecera LVGL 9 de 12 bytes
            f.seek(12);
            size_t rd = f.read(customBuffer, WALLPAPER_BUFFER_SIZE);
            loadSuccess = (rd == WALLPAPER_BUFFER_SIZE);
        } else if (fSize >= WALLPAPER_BUFFER_SIZE) {
            size_t rd = f.read(customBuffer, WALLPAPER_BUFFER_SIZE);
            loadSuccess = (rd == WALLPAPER_BUFFER_SIZE);
        }
    } else if (lowerPath.rfind(".bmp") != std::string::npos) {
        // Soporte de decodificación BMP 24 bits 320x480 directo a RGB565
        uint8_t header[54];
        if (f.read(header, 54) == 54 && header[0] == 'B' && header[1] == 'M') {
            uint32_t dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
            int32_t w = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
            int32_t h = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
            uint16_t bpp = header[28] | (header[29] << 8);

            if (w == (int32_t)WALLPAPER_WIDTH && abs(h) == (int32_t)WALLPAPER_HEIGHT && bpp == 24) {
                f.seek(dataOffset);
                bool topDown = (h < 0);
                uint8_t rowBuf[WALLPAPER_WIDTH * 3]; // 960 bytes por fila
                loadSuccess = true;

                for (int y = 0; y < (int)WALLPAPER_HEIGHT; y++) {
                    int targetY = topDown ? y : (WALLPAPER_HEIGHT - 1 - y);
                    if (f.read(rowBuf, sizeof(rowBuf)) != sizeof(rowBuf)) {
                        loadSuccess = false;
                        break;
                    }
                    uint16_t* destRow = (uint16_t*)(customBuffer + targetY * WALLPAPER_WIDTH * 2);
                    for (int x = 0; x < (int)WALLPAPER_WIDTH; x++) {
                        uint8_t b = rowBuf[x * 3 + 0];
                        uint8_t g = rowBuf[x * 3 + 1];
                        uint8_t r = rowBuf[x * 3 + 2];
                        uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                        destRow[x] = rgb565;
                    }
                }
            }
        }
    }

    f.close();
    lv_fs_spi_unlock();

    if (!loadSuccess) {
        Serial.println("[Wallpaper] ERROR: Formato o tamaño de imagen no soportado para copia rápida");
        return false;
    }

    // Guardar en LittleFS para persistencia permanente en Flash interna
    File flashFile = LittleFS.open("/wallpaper.bin", "w");
    if (flashFile) {
        flashFile.write(customBuffer, WALLPAPER_BUFFER_SIZE);
        flashFile.close();
        Serial.println("[Wallpaper] Fondo copiado y guardado exitosamente en LittleFS (/wallpaper.bin)");
    } else {
        Serial.println("[Wallpaper] WARN: No se pudo guardar en LittleFS Flash");
    }

    hasCustomWallpaper = true;
    currentPath = path;

    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("wallpaper", false)) {
        backend->setBool("custom", true);
        backend->setString("path", currentPath);
        backend->end();
    }

    return true;
#else
    return false;
#endif
}

void WallpaperManager::restoreDefault() {
    // "Default" = animado Constellation. Borra el RGB de SPIFFS (/wallpaper.bin)
    // para no ocupar flash con imagenes; el usuario puede recargarla desde SD.
    hasCustomWallpaper = false;
    currentPath = "animated_constellation";
#if defined(ARDUINO)
    if (LittleFS.exists("/wallpaper.bin")) {
        LittleFS.remove("/wallpaper.bin");
    }
#endif
    auto* backend = cbdos::persistence::getBackend();
    if (backend && backend->begin("wallpaper", false)) {
        backend->setBool("custom", false);
        backend->setString("path", "animated_constellation");
        backend->end();
    }
}

std::vector<std::string> WallpaperManager::getAvailableWallpapers() {
    std::vector<std::string> list;
#if defined(ARDUINO)
    lv_fs_spi_lock();
    File dir = SD.open("/wallpapers");
    if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String fname = file.name();
                String lower = fname;
                lower.toLowerCase();
                if (lower.endsWith(".bin") || lower.endsWith(".bmp") || lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png")) {
                    String fullPath = "A:/wallpapers/" + fname;
                    list.push_back(fullPath.c_str());
                }
            }
            file = dir.openNextFile();
        }
        dir.close();
    }
    lv_fs_spi_unlock();
#endif
    return list;
}
