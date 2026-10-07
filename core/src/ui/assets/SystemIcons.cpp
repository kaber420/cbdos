#include "SystemIcons.hpp"
#include "cbdos/display.hpp"
#include <cstring>
#include <unordered_map>

// Iconos de 48x48 (ARGB8888) portables incluidos en Flash
extern const uint8_t app_recorder_bin[];
extern const uint8_t app_radio_bin[];
extern const uint8_t app_browser_bin[];
extern const uint8_t app_terminal_bin[];
extern const uint8_t app_cartridge_bin[];
extern const uint8_t app_lua_bin[];
extern const uint8_t app_editor_bin[];
extern const uint8_t app_utilities_bin[];
extern const uint8_t app_gallery_bin[];
extern const uint8_t app_files_bin[];
extern const uint8_t app_music_bin[];
extern const uint8_t app_flasher_bin[];
extern const uint8_t app_config_bin[];

namespace cbdos {
namespace ui {

void SystemIcons::init() {
}

const char* SystemIcons::getSvgData(const std::string& appId) {
    (void)appId;
    return nullptr;
}

static const uint8_t* getBinData(const std::string& appId) {
    if (appId == "recorder") return app_recorder_bin;
    if (appId == "radio") return app_radio_bin;
    if (appId == "browser") return app_browser_bin;
    if (appId == "terminal") return app_terminal_bin;
    if (appId == "cartridge") return app_cartridge_bin;
    if (appId == "lua") return app_lua_bin;
    if (appId == "editor") return app_editor_bin;
    if (appId == "utilities") return app_utilities_bin;
    if (appId == "gallery") return app_gallery_bin;
    if (appId == "files") return app_files_bin;
    if (appId == "music") return app_music_bin;
    if (appId == "flasher") return app_flasher_bin;
    if (appId == "config") return app_config_bin;
    return nullptr;
}

lv_obj_t* SystemIcons::createIcon(lv_obj_t* parent, const std::string& appId, int32_t size) {
    if (!parent) return nullptr;

    const uint8_t* binData = getBinData(appId);
    if (!binData) return nullptr;

    static std::unordered_map<std::string, lv_image_dsc_t> s_binDscMap;
    auto it = s_binDscMap.find(appId);
    if (it == s_binDscMap.end()) {
        lv_image_dsc_t dsc;
        lv_memzero(&dsc, sizeof(lv_image_dsc_t));
        dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
        dsc.header.w = 48;
        dsc.header.h = 48;
        dsc.data_size = 48 * 48 * 4;
        dsc.data = binData;
        s_binDscMap[appId] = dsc;
        it = s_binDscMap.find(appId);
    }

    lv_obj_t* bin_img = lv_image_create(parent);
    lv_image_set_src(bin_img, &(it->second));
    if (size > 0 && size != 48) {
        lv_image_set_scale(bin_img, (size * 256) / 48);
    }
    lv_obj_center(bin_img);
    lv_obj_remove_flag(bin_img, LV_OBJ_FLAG_CLICKABLE);
    return bin_img;
}

} // namespace ui
} // namespace cbdos
