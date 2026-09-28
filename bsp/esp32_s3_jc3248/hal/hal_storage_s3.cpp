#include "cbdos/storage.hpp"
#include "cbdos_device_tree.h"
#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <SPIFFS.h>
#include <lvgl.h>

namespace cbdos {
namespace bsp {

static SPIClass* s_sdSPI = nullptr;
static bool s_sdMounted = false;
static bool s_spiffsMounted = false;
static bool s_lvfsRegistered = false;

static bool mountSpiffsInternal() {
    if (s_spiffsMounted) return true;
    if (SPIFFS.begin(true, "/spiffs", 10, "spiffs")) {
        s_spiffsMounted = true;
        Serial.println("[StorageHAL-S3] SPIFFS montado exitosamente en /spiffs");
        return true;
    }
    s_spiffsMounted = false;
    Serial.println("[StorageHAL-S3] WARN: No se pudo montar particion Flash SPIFFS");
    return false;
}

// ────────────────────────────────────────────────────────────────
//  Driver VFS para LVGL v9 (Unidad 'A:')
// ────────────────────────────────────────────────────────────────
static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    (void)drv;
    String fullPath = String(path);
    if (fullPath.startsWith("A:") || fullPath.startsWith("a:")) {
        fullPath = fullPath.substring(2);
    }
    if (fullPath.startsWith("/sdcard")) {
        fullPath = fullPath.substring(7);
    }
    if (!fullPath.startsWith("/")) {
        fullPath = "/" + fullPath;
    }

    const char * arduinoMode = FILE_READ;
    if (mode == LV_FS_MODE_WR) {
        arduinoMode = FILE_WRITE;
    } else if (mode == LV_FS_MODE_RD) {
        arduinoMode = FILE_READ;
    } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        arduinoMode = FILE_WRITE;
    }

    if (s_sdMounted) {
        File f = SD.open(fullPath, arduinoMode);
        if (f) return (void *)(new File(f));
    }
    if (s_spiffsMounted) {
        File f = SPIFFS.open(fullPath, arduinoMode);
        if (f) return (void *)(new File(f));
    }

    return NULL;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p) {
    (void)drv;
    File * fPtr = (File *)file_p;
    if (fPtr) {
        fPtr->close();
        delete fPtr;
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
    (void)drv;
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    *br = fPtr->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw) {
    (void)drv;
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    *bw = fPtr->write((const uint8_t *)buf, btw);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
    (void)drv;
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    bool ok = false;
    if (whence == LV_FS_SEEK_SET) {
        ok = fPtr->seek(pos, SeekSet);
    } else if (whence == LV_FS_SEEK_CUR) {
        ok = fPtr->seek(fPtr->position() + (int32_t)pos, SeekSet);
    } else if (whence == LV_FS_SEEK_END) {
        ok = fPtr->seek(fPtr->size() + (int32_t)pos, SeekSet);
    }
    return ok ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
    (void)drv;
    File * fPtr = (File *)file_p;
    if (!fPtr) return LV_FS_RES_UNKNOWN;
    *pos_p = fPtr->position();
    return LV_FS_RES_OK;
}

static void initLvfsDriver() {
    if (s_lvfsRegistered) return;
    if (!lv_is_initialized()) return;
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = 'A';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    lv_fs_drv_register(&fs_drv);
    s_lvfsRegistered = true;
}

static String resolvePath(const char* path, bool& isSdTarget) {
    String p = String(path ? path : "/");
    isSdTarget = false;

    // Rutas con alias de MicroSD
    if (p.startsWith("S:/") || p.startsWith("s:/") || p.startsWith("A:/") || p.startsWith("a:/")) {
        isSdTarget = true;
        p = p.substring(2);
    } else if (p.startsWith("/sdcard")) {
        isSdTarget = true;
        p = p.substring(7);
    } else if (p.startsWith("/sd")) {
        isSdTarget = true;
        p = p.substring(3);
    } else if (p.startsWith("/spiffs")) {
        isSdTarget = false;
        p = p.substring(7);
    } else if (p.startsWith("/flash")) {
        isSdTarget = false;
        p = p.substring(6);
    }

    if (p.isEmpty() || !p.startsWith("/")) {
        p = "/" + p;
    }
    return p;
}

class ArduinoStorageBackend : public storage::IStorageBackend {
public:
    ArduinoStorageBackend() = default;
    ~ArduinoStorageBackend() override = default;

    bool init() override {
        mountSpiffsInternal();
        mountSd();
        initLvfsDriver();
        return s_spiffsMounted || s_sdMounted;
    }

    bool mountSd() override {
        if (s_sdMounted && SD.cardType() != CARD_NONE) {
            return true;
        }

        if (!s_sdSPI) {
            s_sdSPI = new SPIClass(HSPI);
            s_sdSPI->begin(cbdos::board::sdcard::PIN_SCK,
                           cbdos::board::sdcard::PIN_MISO,
                           cbdos::board::sdcard::PIN_MOSI,
                           cbdos::board::sdcard::PIN_CS);
        }

        bool mounted = SD.begin(cbdos::board::sdcard::PIN_CS, *s_sdSPI, 10000000, "/sdcard");
        if (!mounted) {
            Serial.println("[StorageHAL-S3] SD 10MHz fallo, reintentando a 4MHz...");
            mounted = SD.begin(cbdos::board::sdcard::PIN_CS, *s_sdSPI, 4000000, "/sdcard");
        }

        if (mounted) {
            s_sdMounted = true;
            initLvfsDriver();
            Serial.println("[StorageHAL-S3] MicroSD montada exitosamente");
            return true;
        }

        s_sdMounted = false;
        Serial.println("[StorageHAL-S3] WARN: No se detecto tarjeta MicroSD");
        return false;
    }

    bool unmountSd() override {
        SD.end();
        s_sdMounted = false;
        return true;
    }

    bool formatSd() override {
        unmountSd();
        return mountSd();
    }

    bool isSdMounted() const override {
        return s_sdMounted && (SD.cardType() != CARD_NONE);
    }

    bool isFlashMounted() const override {
        return s_spiffsMounted;
    }

    storage::StorageStats getFlashStats() const override {
        storage::StorageStats stats = { s_spiffsMounted, 0, 0, 0, "Flash Interna (SPIFFS)" };
        if (s_spiffsMounted) {
            stats.totalBytes = SPIFFS.totalBytes();
            stats.usedBytes = SPIFFS.usedBytes();
            stats.freeBytes = (stats.totalBytes > stats.usedBytes) ? (stats.totalBytes - stats.usedBytes) : 0;
        } else {
            stats.totalBytes = ESP.getFlashChipSize();
            if (stats.totalBytes == 0) stats.totalBytes = 16 * 1024 * 1024;
            stats.usedBytes = ESP.getSketchSize();
            stats.freeBytes = stats.totalBytes > stats.usedBytes ? (stats.totalBytes - stats.usedBytes) : 0;
        }
        return stats;
    }

    storage::StorageStats getSdCardStats() const override {
        bool mounted = isSdMounted();
        storage::StorageStats stats = { mounted, 0, 0, 0, "Tarjeta MicroSD" };
        if (mounted) {
            stats.totalBytes = SD.totalBytes();
            stats.usedBytes = SD.usedBytes();
            stats.freeBytes = stats.totalBytes > stats.usedBytes ? (stats.totalBytes - stats.usedBytes) : 0;
        }
        return stats;
    }

    storage::StorageStats getUsbStats() const override {
        return storage::StorageStats{ false, 0, 0, 0, "USB Drive" };
    }

    std::vector<storage::FileEntry> listDir(const char* path, bool includeDeleted = false) override {
        std::vector<storage::FileEntry> result;
        bool isSd = false;
        String cleanPath = resolvePath(path, isSd);

        if (isSd) {
            if (!isSdMounted()) return result;
            File dir = SD.open(cleanPath);
            if (!dir || !dir.isDirectory()) return result;

            File entry = dir.openNextFile();
            while (entry) {
                storage::FileEntry fe;
                fe.name = entry.name();
                size_t slash = fe.name.find_last_of('/');
                if (slash != std::string::npos) {
                    fe.name = fe.name.substr(slash + 1);
                }
                fe.size = entry.size();
                fe.isDirectory = entry.isDirectory();
                result.push_back(fe);
                entry.close();
                entry = dir.openNextFile();
            }
            dir.close();
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return result;
            File root = SPIFFS.open(cleanPath);
            if (!root) return result;

            if (root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    storage::FileEntry fe;
                    fe.name = file.name();
                    size_t slash = fe.name.find_last_of('/');
                    if (slash != std::string::npos) {
                        fe.name = fe.name.substr(slash + 1);
                    }
                    fe.size = file.size();
                    fe.isDirectory = file.isDirectory();
                    result.push_back(fe);
                    file.close();
                    file = root.openNextFile();
                }
            }
            root.close();
        }

        return result;
    }

    bool fileExists(const char* path) override {
        bool isSd = false;
        String cleanPath = resolvePath(path, isSd);

        if (isSd) {
            if (!isSdMounted()) return false;
            return SD.exists(cleanPath);
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return false;
            return SPIFFS.exists(cleanPath);
        }
    }

    std::string readFile(const char* path) override {
        std::string content = "";
        bool isSd = false;
        String cleanPath = resolvePath(path, isSd);

        if (isSd) {
            if (!isSdMounted() && !mountSd()) return content;
            File f = SD.open(cleanPath, FILE_READ);
            if (f) {
                size_t fileSize = f.size();
                if (fileSize > 0) {
                    content.resize(fileSize);
                    f.read((uint8_t*)&content[0], fileSize);
                }
                f.close();
            }
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return content;
            File f = SPIFFS.open(cleanPath, FILE_READ);
            if (f) {
                size_t fileSize = f.size();
                if (fileSize > 0) {
                    content.resize(fileSize);
                    f.read((uint8_t*)&content[0], fileSize);
                }
                f.close();
            }
        }
        return content;
    }

    bool makeDir(const char* path) override {
        bool isSd = false;
        String cleanPath = resolvePath(path, isSd);

        if (isSd) {
            if (!isSdMounted() && !mountSd()) return false;
            return SD.mkdir(cleanPath);
        } else {
            // SPIFFS maneja nombres con barra pero no directorios reales
            return true;
        }
    }

    bool writeFile(const char* path, const std::string& content) override {
        bool isSd = false;
        String cleanPath = resolvePath(path, isSd);

        if (isSd) {
            if (!isSdMounted() && !mountSd()) return false;

            int lastSlash = cleanPath.lastIndexOf('/');
            if (lastSlash > 0) {
                for (int i = 1; i <= lastSlash; i++) {
                    if (cleanPath[i] == '/' || i == lastSlash) {
                        String currentDir = cleanPath.substring(0, i);
                        if (!SD.exists(currentDir)) {
                            SD.mkdir(currentDir);
                        }
                    }
                }
            }

            File f = SD.open(cleanPath, FILE_WRITE);
            bool ok = false;
            if (f) {
                if (!content.empty()) {
                    size_t written = f.write((const uint8_t*)content.data(), content.size());
                    ok = (written == content.size());
                } else {
                    ok = true;
                }
                f.close();
            }
            return ok;
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return false;
            File f = SPIFFS.open(cleanPath, FILE_WRITE);
            bool ok = false;
            if (f) {
                if (!content.empty()) {
                    size_t written = f.write((const uint8_t*)content.data(), content.size());
                    ok = (written == content.size());
                } else {
                    ok = true;
                }
                f.close();
            }
            return ok;
        }
    }

    bool deleteFile(const char* path) override {
        bool isSd = false;
        String cleanPath = resolvePath(path, isSd);

        if (isSd) {
            if (!isSdMounted()) return false;
            return SD.remove(cleanPath);
        } else {
            if (!s_spiffsMounted && !mountSpiffsInternal()) return false;
            return SPIFFS.remove(cleanPath);
        }
    }

    bool copyFile(const char* srcPath, const char* dstPath) override {
        std::string data = readFile(srcPath);
        if (data.empty() && !fileExists(srcPath)) {
            return false;
        }
        return writeFile(dstPath, data);
    }

    size_t getFreeBytes(storage::StorageType type) const override {
        if (type == storage::StorageType::InternalFlash) return (size_t)getFlashStats().freeBytes;
        return (size_t)getSdCardStats().freeBytes;
    }

    size_t getTotalBytes(storage::StorageType type) const override {
        if (type == storage::StorageType::InternalFlash) return (size_t)getFlashStats().totalBytes;
        return (size_t)getSdCardStats().totalBytes;
    }
};

static ArduinoStorageBackend s_s3StorageBackend;

void initStorageBackend() {
    storage::setBackend(&s_s3StorageBackend);
    Serial.println("[StorageHAL-S3] Arduino Storage Backend registrado e inyectado.");
}

} // namespace bsp
} // namespace cbdos
