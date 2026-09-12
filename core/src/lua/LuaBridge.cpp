#include "LuaBridge.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/system.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/network.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"
#include "cbdos/uart.hpp"
#include "cbdos/gpio.hpp"
#include "cbdos/memory.hpp"
#include "cbdos/hid.hpp"
#include "cbdos/ducky.hpp"
#include "cbdos/ssh.hpp"
#include "cbdos/lan_recon.hpp"
#include "../network/LanScannerService.hpp"
#include "../UIManager.hpp"
#include "../themes/DefaultTheme.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <lvgl.h>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// ─────────────────────────────────────────────────────────────────────────────
// UI Control & State
// ─────────────────────────────────────────────────────────────────────────────
static volatile uint32_t s_uiPausedUntil = 0;
static volatile bool s_uiPausedIndefinite = false;
static volatile bool s_needsScreenRefresh = false;

void LuaBridge::pauseUI(uint32_t seconds) {
    if (seconds == 0) {
        s_uiPausedIndefinite = true;
        s_uiPausedUntil = 0;
    } else {
        s_uiPausedIndefinite = false;
        s_uiPausedUntil = cbdos::system::getTimeMs() + (seconds * 1000);
    }
}

void LuaBridge::resumeUI() {
    bool wasPaused = s_uiPausedIndefinite || (s_uiPausedUntil > 0);
    s_uiPausedIndefinite = false;
    s_uiPausedUntil = 0;
    if (wasPaused) {
        s_needsScreenRefresh = true;
    }
}

bool LuaBridge::checkAndClearNeedsRefresh() {
    if (s_needsScreenRefresh) {
        s_needsScreenRefresh = false;
        return true;
    }
    return false;
}

bool LuaBridge::isUIPaused() {
    if (s_uiPausedIndefinite) return true;
    if (s_uiPausedUntil > 0) {
        if (cbdos::system::getTimeMs() < s_uiPausedUntil) {
            return true;
        } else {
            resumeUI();
            return false;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_beep(lua_State* L) {
    lua_Number freq = luaL_checknumber(L, 1);
    lua_Integer ms = luaL_checkinteger(L, 2);
    cbdos::audio::playTone((uint32_t)freq, (uint32_t)ms);
    return 0;
}

static int lua_play_mp3(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool ok = cbdos::audio::playFile(path);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_stop_audio(lua_State* L) {
    cbdos::audio::stop();
    return 0;
}

static int lua_set_volume(lua_State* L) {
    lua_Integer vol = luaL_checkinteger(L, 1);
    cbdos::audio::setVolume((uint8_t)vol);
    return 0;
}

static int lua_get_volume(lua_State* L) {
    lua_pushinteger(L, cbdos::audio::getVolume());
    return 1;
}

static int lua_record_start(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t srate = (uint32_t)luaL_optinteger(L, 2, 16000);
    cbdos::audio::RecordConfig cfg;
    cfg.sampleRate = srate;
    cfg.channels = 1;
    cfg.bitsPerSample = 16;
    cfg.micGainDb = 24;
    bool ok = cbdos::audio::recordStart(path, cfg);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_record_stop(lua_State* L) {
    cbdos::audio::recordStop();
    return 0;
}

static int lua_is_recording(lua_State* L) {
    lua_pushboolean(L, cbdos::audio::isRecording());
    return 1;
}

static int lua_get_mic_level(lua_State* L) {
    lua_pushnumber(L, cbdos::audio::getMicPeakLevel());
    return 1;
}


// ─────────────────────────────────────────────────────────────────────────────
// System API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_delay(lua_State* L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms > 0) {
        cbdos::system::sleepMs((uint32_t)ms);
    }
    return 0;
}

static int lua_millis(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)cbdos::system::getTimeMs());
    return 1;
}

static int lua_free_psram(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)cbdos::system::getFreePsram());
    return 1;
}

static int lua_free_heap(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)cbdos::system::getFreeHeap());
    return 1;
}

static int lua_get_battery(lua_State* L) {
    lua_pushinteger(L, 100);
    return 1;
}

static int lua_wifi_status(lua_State* L) {
    lua_pushboolean(L, cbdos::network::isConnected());
    return 1;
}

static int lua_get_ip(lua_State* L) {
    std::string ip = cbdos::network::getIpAddress();
    if (ip.empty()) {
        ip = "0.0.0.0";
    }
    lua_pushstring(L, ip.c_str());
    return 1;
}

static int lua_cpu_temp(lua_State* L) {
    lua_pushnumber(L, (lua_Number)cbdos::system::getCpuTemperature());
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// GPIO API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_pin_mode(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    int modeInt = (int)luaL_checkinteger(L, 2);
    cbdos::gpio::PinMode mode = cbdos::gpio::PinMode::Input;
    if (modeInt == 1) mode = cbdos::gpio::PinMode::Output;
    else if (modeInt == 2) mode = cbdos::gpio::PinMode::InputPullUp;
    else if (modeInt == 3) mode = cbdos::gpio::PinMode::InputPullDown;

    bool ok = cbdos::gpio::setPinMode(pin, mode);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_digital_write(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    int val = (int)luaL_checkinteger(L, 2);
    bool ok = cbdos::gpio::digitalWrite(pin, val ? cbdos::gpio::PinLevel::High : cbdos::gpio::PinLevel::Low);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_digital_read(lua_State* L) {
    int pin = (int)luaL_checkinteger(L, 1);
    cbdos::gpio::PinLevel level = cbdos::gpio::digitalRead(pin);
    lua_pushinteger(L, (level == cbdos::gpio::PinLevel::High) ? 1 : 0);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem / SD API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_read_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::string content = cbdos::storage::readFile(path);
    if (content.empty() && !cbdos::storage::fileExists(path)) {
        if (std::string(path).rfind("/sdcard/", 0) != 0) {
            std::string alt = std::string("/sdcard/") + (path[0] == '/' ? path + 1 : path);
            content = cbdos::storage::readFile(alt.c_str());
        }
    }

    if (content.empty() && !cbdos::storage::fileExists(path)) {
        lua_pushnil(L);
        lua_pushstring(L, "No se pudo leer el archivo");
        return 2;
    }

    lua_pushlstring(L, content.data(), content.size());
    return 1;
}

static int lua_write_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t dataLen = 0;
    const char* data = luaL_checklstring(L, 2, &dataLen);
    std::string content(data, dataLen);

    bool ok = cbdos::storage::writeFile(path, content);
    if (!ok && std::string(path).rfind("/sdcard/", 0) != 0) {
        std::string alt = std::string("/sdcard/") + (path[0] == '/' ? path + 1 : path);
        ok = cbdos::storage::writeFile(alt.c_str(), content);
    }

    lua_pushboolean(L, ok);
    return 1;
}

static int lua_file_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool exists = cbdos::storage::fileExists(path);
    if (!exists && std::string(path).rfind("/sdcard/", 0) != 0) {
        std::string p = std::string("/sdcard/") + (path[0] == '/' ? path + 1 : path);
        exists = cbdos::storage::fileExists(p.c_str());
    }
    lua_pushboolean(L, exists);
    return 1;
}

static int lua_mount_sd(lua_State* L) {
    bool ok = cbdos::storage::mountSd();
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_format_sd(lua_State* L) {
    bool ok = cbdos::storage::formatSd();
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_list_dir(lua_State* L) {
    const char* path = luaL_optstring(L, 1, "/sdcard");
    auto entries = cbdos::storage::listDir(path);
    if (entries.empty() && strcmp(path, "/sdcard") == 0) {
        entries = cbdos::storage::listDir("/");
    }

    lua_newtable(L);
    for (size_t i = 0; i < entries.size(); i++) {
        lua_newtable(L);
        lua_pushstring(L, entries[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, entries[i].size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, entries[i].isDirectory);
        lua_setfield(L, -2, "isDirectory");
        lua_pushboolean(L, entries[i].isDirectory);
        lua_setfield(L, -2, "is_directory");
        lua_pushboolean(L, entries[i].isDirectory);
        lua_setfield(L, -2, "is_dir");
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// UART API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_uart_init(lua_State* L) {
    uint32_t baud = (uint32_t)luaL_optinteger(L, 1, 115200);
    int tx = (int)luaL_optinteger(L, 2, cbdos::uart::getDefaultTxPin());
    int rx = (int)luaL_optinteger(L, 3, cbdos::uart::getDefaultRxPin());

    bool ok = cbdos::uart::init(tx, rx, baud);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_uart_write(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    size_t written = cbdos::uart::writeString(str);
    lua_pushinteger(L, written);
    return 1;
}

static int lua_uart_read(lua_State* L) {
    size_t maxLen = (size_t)luaL_optinteger(L, 1, 512);
    std::string data = cbdos::uart::readString(maxLen);
    lua_pushstring(L, data.c_str());
    return 1;
}

static int lua_uart_available(lua_State* L) {
    lua_pushinteger(L, cbdos::uart::available());
    return 1;
}

static int lua_uart_flush(lua_State* L) {
    cbdos::uart::flush();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// LVGL 9.5 UI API (cbdos.ui.*)
// ─────────────────────────────────────────────────────────────────────────────

struct LuaUIEventData {
    lua_State* L;
    int fnRef;
};

static void lua_ui_generic_event_cb(lv_event_t* e) {
    auto* data = static_cast<LuaUIEventData*>(lv_event_get_user_data(e));
    if (!data || !data->L) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_DELETE) {
        if (data->fnRef != LUA_NOREF) {
            luaL_unref(data->L, LUA_REGISTRYINDEX, data->fnRef);
        }
        delete data;
        return;
    }

    if (data->fnRef == LUA_NOREF) return;

    lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->fnRef);
    if (!lua_isfunction(data->L, -1)) {
        lua_pop(data->L, 1);
        return;
    }

    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    int nargs = 0;

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_check_type(target, &lv_slider_class)) {
            lua_pushinteger(data->L, lv_slider_get_value(target));
            nargs = 1;
        } else if (lv_obj_check_type(target, &lv_switch_class)) {
            lua_pushboolean(data->L, lv_obj_has_state(target, LV_STATE_CHECKED));
            nargs = 1;
        }
    }

    if (lua_pcall(data->L, nargs, 0, 0) != 0) {
        const char* err = lua_tostring(data->L, -1);
        printf("[LuaUI Error] %s\n", err ? err : "Error desconocido");
        lua_pop(data->L, 1);
    }
}

static inline int32_t parse_coord(lua_State* L, int argIdx, int32_t defaultVal) {
    if (lua_isnoneornil(L, argIdx)) return defaultVal;
    int32_t val = (int32_t)luaL_checkinteger(L, argIdx);
    if (val <= 0 || val == -1) return defaultVal;
    return val;
}

static lv_obj_t* get_target_parent(lua_State* L, int argIdx) {
    if (lua_islightuserdata(L, argIdx)) {
        lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, argIdx);
        if (obj && lv_obj_is_valid(obj)) return obj;
    }
    return lv_screen_active();
}

static int lua_ui_create_card(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    int32_t w = parse_coord(L, 2, LV_PCT(100));
    int32_t h = parse_coord(L, 3, LV_SIZE_CONTENT);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lua_pushlightuserdata(L, card);
    return 1;
}

static int lua_ui_create_sunken_card(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    int32_t w = parse_coord(L, 2, LV_PCT(100));
    int32_t h = parse_coord(L, 3, LV_SIZE_CONTENT);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    DefaultTheme::applySunkenCard(card, 16);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lua_pushlightuserdata(L, card);
    return 1;
}

static int lua_ui_create_label(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    const char* text = luaL_optstring(L, 2, "");

    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    lua_pushlightuserdata(L, lbl);
    return 1;
}

static int lua_ui_set_text(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        lv_label_set_text(obj, text);
    }
    return 0;
}

static int lua_ui_set_color(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        lv_obj_set_style_text_color(obj, lv_color_hex(hexColor), 0);
    }
    return 0;
}

static int lua_ui_set_font_size(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int size = (int)luaL_checkinteger(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        if (size >= 24) {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, 0);
        } else if (size >= 16) {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, 0);
        } else if (size >= 14) {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
        } else {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, 0);
        }
    }
    return 0;
}

static int lua_ui_create_button(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    const char* text = luaL_optstring(L, 2, "Boton");

    lv_obj_t* btn = lv_button_create(parent);
    DefaultTheme::applyButton(btn, 16);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_pad_hor(btn, 16, 0);
    lv_obj_set_style_pad_ver(btn, 8, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(btn, lua_ui_generic_event_cb, LV_EVENT_CLICKED, data);
        lv_obj_add_event_cb(btn, lua_ui_generic_event_cb, LV_EVENT_DELETE, data);
    }

    lua_pushlightuserdata(L, btn);
    return 1;
}

static int lua_ui_create_slider(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    int32_t minVal = (int32_t)luaL_optinteger(L, 2, 0);
    int32_t maxVal = (int32_t)luaL_optinteger(L, 3, 100);
    int32_t val = (int32_t)luaL_optinteger(L, 4, 50);

    lv_obj_t* slider = lv_slider_create(parent);
    lv_slider_set_range(slider, minVal, maxVal);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    lv_obj_set_size(slider, LV_PCT(100), 16);

    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(slider, lua_ui_generic_event_cb, LV_EVENT_VALUE_CHANGED, data);
        lv_obj_add_event_cb(slider, lua_ui_generic_event_cb, LV_EVENT_DELETE, data);
    }

    lua_pushlightuserdata(L, slider);
    return 1;
}

static int lua_ui_create_switch(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    bool state = lua_toboolean(L, 2);

    lv_obj_t* sw = lv_switch_create(parent);
    if (state) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(sw, lua_ui_generic_event_cb, LV_EVENT_VALUE_CHANGED, data);
        lv_obj_add_event_cb(sw, lua_ui_generic_event_cb, LV_EVENT_DELETE, data);
    }

    lua_pushlightuserdata(L, sw);
    return 1;
}

static int lua_ui_create_row(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lua_pushlightuserdata(L, row);
    return 1;
}

static int lua_ui_create_column(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_set_size(col, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 4, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 8, 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lua_pushlightuserdata(L, col);
    return 1;
}

static int lua_ui_create_dropdown(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    const char* options = luaL_checkstring(L, 2);

    lv_obj_t* dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(dd, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(dd, 1, 0);
    lv_obj_set_style_radius(dd, 6, 0);
    lv_obj_set_style_pad_hor(dd, 10, 0);
    lv_obj_set_style_pad_ver(dd, 6, 0);
    lv_obj_set_style_text_color(dd, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, 0);

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(dd, [](lv_event_t* e) {
            auto* d = static_cast<LuaUIEventData*>(lv_event_get_user_data(e));
            if (!d) return;
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_DELETE) {
                if (d->L && d->fnRef != LUA_NOREF) {
                    luaL_unref(d->L, LUA_REGISTRYINDEX, d->fnRef);
                }
                delete d;
                return;
            }
            if (code == LV_EVENT_VALUE_CHANGED && d->L && d->fnRef != LUA_NOREF) {
                lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
                uint32_t sel = lv_dropdown_get_selected(target);
                char buf[64];
                lv_dropdown_get_selected_str(target, buf, sizeof(buf));

                lua_rawgeti(d->L, LUA_REGISTRYINDEX, d->fnRef);
                if (lua_isfunction(d->L, -1)) {
                    lua_pushinteger(d->L, sel);
                    lua_pushstring(d->L, buf);
                    if (lua_pcall(d->L, 2, 0, 0) != 0) {
                        const char* err = lua_tostring(d->L, -1);
                        printf("[Dropdown Event Error] %s\n", err ? err : "Unknown");
                        lua_pop(d->L, 1);
                    }
                } else {
                    lua_pop(d->L, 1);
                }
            }
        }, LV_EVENT_ALL, data);
    }

    lua_pushlightuserdata(L, dd);
    return 1;
}

static int lua_ui_get_selected(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    if (obj && lv_obj_is_valid(obj)) {
        lua_pushinteger(L, lv_dropdown_get_selected(obj));
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}

static int lua_ui_set_selected(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    uint32_t idx = (uint32_t)luaL_checkinteger(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        lv_dropdown_set_selected(obj, idx);
    }
    return 0;
}

static int lua_ui_set_size(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int32_t w = (int32_t)luaL_checkinteger(L, 2);
    int32_t h = (int32_t)luaL_checkinteger(L, 3);
    if (obj && lv_obj_is_valid(obj)) {
        lv_obj_set_size(obj, w, h);
    }
    return 0;
}

static int lua_ui_show_toast(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    cbdos::ui::UIManager::showToast(msg);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// LVGL 9.5 Custom Pixel Canvas API (cbdos.canvas.*)
// ─────────────────────────────────────────────────────────────────────────────

struct LuaCanvasCtx {
    uint16_t* buffer;
    uint32_t width;
    uint32_t height;
    lua_State* L;
    int clickRef;
    int touchRef;
};

static inline uint16_t colorToRGB565(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline uint32_t rgb565ToColor(uint16_t c565) {
    uint8_t r = ((c565 >> 11) & 0x1F) << 3;
    uint8_t g = ((c565 >> 5) & 0x3F) << 2;
    uint8_t b = (c565 & 0x1F) << 3;
    return (r << 16) | (g << 8) | b;
}

// 5x7 Font Bitmap table (ASCII 32..126)
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08}
};

static void canvas_event_cb(lv_event_t* e) {
    auto* ctx = static_cast<LuaCanvasCtx*>(lv_event_get_user_data(e));
    if (!ctx) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        if (ctx->L) {
            if (ctx->clickRef != LUA_NOREF) {
                luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->clickRef);
            }
            if (ctx->touchRef != LUA_NOREF) {
                luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->touchRef);
            }
        }
        if (ctx->buffer) {
            cbdos::mem::free_mem(ctx->buffer);
            ctx->buffer = nullptr;
        }
        delete ctx;
        return;
    }

    if (code == LV_EVENT_CLICKED && ctx->L && ctx->clickRef != LUA_NOREF) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        lv_area_t coords;
        lv_obj_get_coords(target, &coords);
        int32_t relX = point.x - coords.x1;
        int32_t relY = point.y - coords.y1;

        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->clickRef);
        if (lua_isfunction(ctx->L, -1)) {
            lua_pushinteger(ctx->L, relX);
            lua_pushinteger(ctx->L, relY);
            if (lua_pcall(ctx->L, 2, 0, 0) != 0) {
                const char* err = lua_tostring(ctx->L, -1);
                printf("[Canvas Click Error] %s\n", err ? err : "Unknown");
                lua_pop(ctx->L, 1);
            }
        } else {
            lua_pop(ctx->L, 1);
        }
    }

    if ((code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) && ctx->L && ctx->touchRef != LUA_NOREF) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        lv_area_t coords;
        lv_obj_get_coords(target, &coords);
        int32_t relX = point.x - coords.x1;
        int32_t relY = point.y - coords.y1;

        const char* eventStr = "pressed";
        if (code == LV_EVENT_PRESSING) eventStr = "drag";
        else if (code == LV_EVENT_RELEASED) eventStr = "released";

        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->touchRef);
        if (lua_isfunction(ctx->L, -1)) {
            lua_pushstring(ctx->L, eventStr);
            lua_pushinteger(ctx->L, relX);
            lua_pushinteger(ctx->L, relY);
            if (lua_pcall(ctx->L, 3, 0, 0) != 0) {
                const char* err = lua_tostring(ctx->L, -1);
                printf("[Canvas Touch Error] %s\n", err ? err : "Unknown");
                lua_pop(ctx->L, 1);
            }
        } else {
            lua_pop(ctx->L, 1);
        }
    }
}

static LuaCanvasCtx* get_canvas_ctx(lv_obj_t* canvas) {
    if (!canvas || !lv_obj_is_valid(canvas)) return nullptr;
    return (LuaCanvasCtx*)lv_obj_get_user_data(canvas);
}

static int lua_ui_create_canvas(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    uint32_t w = (uint32_t)luaL_optinteger(L, 2, 300);
    uint32_t h = (uint32_t)luaL_optinteger(L, 3, 200);

    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, w, h);

    size_t bufSize = w * h * sizeof(uint16_t);
    uint16_t* buf = (uint16_t*)cbdos::mem::alloc_psram(bufSize);
    if (!buf) {
        buf = (uint16_t*)malloc(bufSize);
    }
    if (!buf) {
        lua_pushnil(L);
        return 1;
    }
    memset(buf, 0, bufSize);

    lv_canvas_set_buffer(canvas, buf, w, h, LV_COLOR_FORMAT_RGB565);

    auto* ctx = new LuaCanvasCtx{ buf, w, h, L, LUA_NOREF, LUA_NOREF };
    lv_obj_set_user_data(canvas, ctx);
    lv_obj_add_event_cb(canvas, canvas_event_cb, LV_EVENT_ALL, ctx);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);

    lua_pushlightuserdata(L, canvas);
    return 1;
}

static int lua_canvas_fill(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    uint32_t hexColor = (uint32_t)luaL_optinteger(L, 2, 0x000000);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    size_t total = ctx->width * ctx->height;
    for (size_t i = 0; i < total; i++) {
        ctx->buffer[i] = c565;
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_set_px(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 4);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    if (x >= 0 && x < (int)ctx->width && y >= 0 && y < (int)ctx->height) {
        ctx->buffer[y * ctx->width + x] = colorToRGB565(hexColor);
        lv_obj_invalidate(canvas);
    }
    return 0;
}

static int lua_canvas_draw_rect(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int rx = (int)luaL_checkinteger(L, 2);
    int ry = (int)luaL_checkinteger(L, 3);
    int rw = (int)luaL_checkinteger(L, 4);
    int rh = (int)luaL_checkinteger(L, 5);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 6);
    bool filled = lua_toboolean(L, 7);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    int x1 = std::max(0, rx);
    int y1 = std::max(0, ry);
    int x2 = std::min((int)ctx->width - 1, rx + rw - 1);
    int y2 = std::min((int)ctx->height - 1, ry + rh - 1);

    if (x1 > x2 || y1 > y2) return 0;

    if (filled) {
        for (int y = y1; y <= y2; y++) {
            uint16_t* row = &ctx->buffer[y * ctx->width];
            for (int x = x1; x <= x2; x++) {
                row[x] = c565;
            }
        }
    } else {
        for (int x = x1; x <= x2; x++) {
            if (ry >= 0 && ry < (int)ctx->height) ctx->buffer[ry * ctx->width + x] = c565;
            if (ry + rh - 1 >= 0 && ry + rh - 1 < (int)ctx->height) ctx->buffer[(ry + rh - 1) * ctx->width + x] = c565;
        }
        for (int y = y1; y <= y2; y++) {
            if (rx >= 0 && rx < (int)ctx->width) ctx->buffer[y * ctx->width + rx] = c565;
            if (rx + rw - 1 >= 0 && rx + rw - 1 < (int)ctx->width) ctx->buffer[y * ctx->width + (rx + rw - 1)] = c565;
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_draw_line(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int x0 = (int)luaL_checkinteger(L, 2);
    int y0 = (int)luaL_checkinteger(L, 3);
    int x1 = (int)luaL_checkinteger(L, 4);
    int y1 = (int)luaL_checkinteger(L, 5);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 6);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        if (x0 >= 0 && x0 < (int)ctx->width && y0 >= 0 && y0 < (int)ctx->height) {
            ctx->buffer[y0 * ctx->width + x0] = c565;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_draw_circle(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int cx = (int)luaL_checkinteger(L, 2);
    int cy = (int)luaL_checkinteger(L, 3);
    int r = (int)luaL_checkinteger(L, 4);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 5);
    bool filled = lua_toboolean(L, 6);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer || r <= 0) return 0;

    uint16_t c565 = colorToRGB565(hexColor);

    auto setPx = [&](int x, int y) {
        if (x >= 0 && x < (int)ctx->width && y >= 0 && y < (int)ctx->height) {
            ctx->buffer[y * ctx->width + x] = c565;
        }
    };

    if (filled) {
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    setPx(cx + x, cy + y);
                }
            }
        }
    } else {
        int x = r, y = 0;
        int err = 0;
        while (x >= y) {
            setPx(cx + x, cy + y); setPx(cx + y, cy + x);
            setPx(cx - y, cy + x); setPx(cx - x, cy + y);
            setPx(cx - x, cy - y); setPx(cx - y, cy - x);
            setPx(cx + y, cy - x); setPx(cx + x, cy - y);
            if (err <= 0) { y += 1; err += 2 * y + 1; }
            if (err > 0) { x -= 1; err -= 2 * x + 1; }
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_draw_text(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int startX = (int)luaL_checkinteger(L, 2);
    int startY = (int)luaL_checkinteger(L, 3);
    const char* text = luaL_checkstring(L, 4);
    uint32_t hexColor = (uint32_t)luaL_optinteger(L, 5, 0xFFFFFF);
    int scale = (int)luaL_optinteger(L, 6, 1);
    if (scale < 1) scale = 1;

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer || !text) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    int curX = startX;

    while (*text) {
        char ch = *text++;
        if (ch >= 32 && ch <= 126) {
            const uint8_t* charData = font5x7[ch - 32];
            for (int col = 0; col < 5; col++) {
                uint8_t line = charData[col];
                for (int row = 0; row < 7; row++) {
                    if (line & (1 << row)) {
                        for (int sx = 0; sx < scale; sx++) {
                            for (int sy = 0; sy < scale; sy++) {
                                int px = curX + col * scale + sx;
                                int py = startY + row * scale + sy;
                                if (px >= 0 && px < (int)ctx->width && py >= 0 && py < (int)ctx->height) {
                                    ctx->buffer[py * ctx->width + px] = c565;
                                }
                            }
                        }
                    }
                }
            }
            curX += 6 * scale;
        } else if (ch == '\n') {
            startY += 8 * scale;
            curX = startX;
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_on_click(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx) return 0;

    if (ctx->clickRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ctx->clickRef);
        ctx->clickRef = LUA_NOREF;
    }

    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        ctx->clickRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

static int lua_canvas_on_touch(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx) return 0;

    if (ctx->touchRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ctx->touchRef);
        ctx->touchRef = LUA_NOREF;
    }

    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        ctx->touchRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

static int lua_canvas_get_px(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (x >= 0 && x < (int)ctx->width && y >= 0 && y < (int)ctx->height) {
        uint16_t c565 = ctx->buffer[y * ctx->width + x];
        lua_pushinteger(L, (lua_Integer)rgb565ToColor(c565));
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int lua_canvas_flood_fill(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int startX = (int)luaL_checkinteger(L, 2);
    int startY = (int)luaL_checkinteger(L, 3);
    uint32_t fillHex = (uint32_t)luaL_checkinteger(L, 4);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    int w = (int)ctx->width;
    int h = (int)ctx->height;
    if (startX < 0 || startX >= w || startY < 0 || startY >= h) return 0;

    uint16_t target565 = ctx->buffer[startY * w + startX];
    uint16_t fill565 = colorToRGB565(fillHex);
    if (target565 == fill565) return 0;

    std::vector<std::pair<int, int>> queue;
    queue.reserve(1024);
    queue.push_back({startX, startY});
    ctx->buffer[startY * w + startX] = fill565;

    size_t head = 0;
    while (head < queue.size()) {
        auto pt = queue[head++];
        int cx = pt.first;
        int cy = pt.second;

        const int dx[4] = {0, 0, -1, 1};
        const int dy[4] = {-1, 1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                if (ctx->buffer[ny * w + nx] == target565) {
                    ctx->buffer[ny * w + nx] = fill565;
                    queue.push_back({nx, ny});
                }
            }
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t bfType{0x4D42}; // "BM"
    uint32_t bfSize{0};
    uint16_t bfReserved1{0};
    uint16_t bfReserved2{0};
    uint32_t bfOffBits{54};
    uint32_t biSize{40};
    int32_t biWidth{0};
    int32_t biHeight{0};
    uint16_t biPlanes{1};
    uint16_t biBitCount{24};
    uint32_t biCompression{0};
    uint32_t biSizeImage{0};
    int32_t biXPelsPerMeter{2835};
    int32_t biYPelsPerMeter{2835};
    uint32_t biClrUsed{0};
    uint32_t biClrImportant{0};
};
#pragma pack(pop)

static int lua_canvas_save_bmp(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    const char* path = luaL_checkstring(L, 2);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) {
        lua_pushboolean(L, false);
        return 1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        lua_pushboolean(L, false);
        return 1;
    }

    int32_t w = (int32_t)ctx->width;
    int32_t h = (int32_t)ctx->height;
    int rowSize = ((w * 3 + 3) / 4) * 4;
    uint32_t imageSize = rowSize * h;

    BMPHeader hdr;
    hdr.bfSize = 54 + imageSize;
    hdr.biWidth = w;
    hdr.biHeight = h;
    hdr.biSizeImage = imageSize;

    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        lua_pushboolean(L, false);
        return 1;
    }

    std::vector<uint8_t> rowBuf(rowSize, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint16_t c565 = ctx->buffer[y * w + x];
            uint8_t r = ((c565 >> 11) & 0x1F) << 3;
            uint8_t g = ((c565 >> 5) & 0x3F) << 2;
            uint8_t b = (c565 & 0x1F) << 3;
            rowBuf[x * 3 + 0] = b;
            rowBuf[x * 3 + 1] = g;
            rowBuf[x * 3 + 2] = r;
        }
        fwrite(rowBuf.data(), 1, rowSize, f);
    }

    fclose(f);
    lua_pushboolean(L, true);
    return 1;
}

static int lua_canvas_load_bmp(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    const char* path = luaL_checkstring(L, 2);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) {
        lua_pushboolean(L, false);
        return 1;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        lua_pushboolean(L, false);
        return 1;
    }

    BMPHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.bfType != 0x4D42) {
        fclose(f);
        lua_pushboolean(L, false);
        return 1;
    }

    int32_t bmpW = hdr.biWidth;
    int32_t bmpH = abs(hdr.biHeight);
    bool flipY = (hdr.biHeight > 0);

    if (hdr.biBitCount == 24) {
        int rowSize = ((bmpW * 3 + 3) / 4) * 4;
        std::vector<uint8_t> rowBuf(rowSize);
        fseek(f, hdr.bfOffBits, SEEK_SET);

        for (int row = 0; row < bmpH; row++) {
            if (fread(rowBuf.data(), 1, rowSize, f) != (size_t)rowSize) break;
            int targetY = flipY ? (bmpH - 1 - row) : row;
            if (targetY >= (int)ctx->height) continue;

            for (int col = 0; col < bmpW && col < (int)ctx->width; col++) {
                uint8_t b = rowBuf[col * 3 + 0];
                uint8_t g = rowBuf[col * 3 + 1];
                uint8_t r = rowBuf[col * 3 + 2];
                uint16_t c565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                ctx->buffer[targetY * ctx->width + col] = c565;
            }
        }
        fclose(f);
        lv_obj_invalidate(canvas);
        lua_pushboolean(L, true);
        return 1;
    }

    fclose(f);
    lua_pushboolean(L, false);
    return 1;
}

static int lua_canvas_refresh(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (canvas && lv_obj_is_valid(canvas)) {
        lv_obj_invalidate(canvas);
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Graphics API (Direct Framebuffer 2D / cbdos.gfx.*)
// ─────────────────────────────────────────────────────────────────────────────
static inline void setPixel(int x, int y, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (x >= 0 && x < w && y >= 0 && y < h) {
        int idx = y * w + x;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawHLine(int x, int y, int len, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (y < 0 || y >= h) return;
    int xStart = std::max(0, x);
    int xEnd = std::min(w, x + len);
    for (int i = xStart; i < xEnd; i++) {
        int idx = y * w + i;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawVLine(int x, int y, int len, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (x < 0 || x >= w) return;
    int yStart = std::max(0, y);
    int yEnd = std::min(h, y + len);
    for (int j = yStart; j < yEnd; j++) {
        int idx = j * w + x;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawRect(int x, int y, int rw, int rh, uint16_t col565, bool filled, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (filled) {
        int yStart = std::max(0, y);
        int yEnd = std::min(h, y + rh);
        for (int j = yStart; j < yEnd; j++) {
            drawHLine(x, j, rw, col565, w, h, fb0, fb1);
        }
    } else {
        drawHLine(x, y, rw, col565, w, h, fb0, fb1);
        drawHLine(x, y + rh - 1, rw, col565, w, h, fb0, fb1);
        drawVLine(x, y, rh, col565, w, h, fb0, fb1);
        drawVLine(x + rw - 1, y, rh, col565, w, h, fb0, fb1);
    }
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixel(x0, y0, col565, w, h, fb0, fb1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void drawCircle(int cx, int cy, int r, uint16_t col565, bool filled, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (r <= 0) return;
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    if (filled) {
        drawHLine(cx - r, cy, 2 * r + 1, col565, w, h, fb0, fb1);
    } else {
        setPixel(cx, cy + r, col565, w, h, fb0, fb1);
        setPixel(cx, cy - r, col565, w, h, fb0, fb1);
        setPixel(cx + r, cy, col565, w, h, fb0, fb1);
        setPixel(cx - r, cy, col565, w, h, fb0, fb1);
    }

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (filled) {
            drawHLine(cx - x, cy + y, 2 * x + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - x, cy - y, 2 * x + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - y, cy + x, 2 * y + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - y, cy - x, 2 * y + 1, col565, w, h, fb0, fb1);
        } else {
            setPixel(cx + x, cy + y, col565, w, h, fb0, fb1);
            setPixel(cx - x, cy + y, col565, w, h, fb0, fb1);
            setPixel(cx + x, cy - y, col565, w, h, fb0, fb1);
            setPixel(cx - x, cy - y, col565, w, h, fb0, fb1);
            setPixel(cx + y, cy + x, col565, w, h, fb0, fb1);
            setPixel(cx - y, cy + x, col565, w, h, fb0, fb1);
            setPixel(cx + y, cy - x, col565, w, h, fb0, fb1);
            setPixel(cx - y, cy - x, col565, w, h, fb0, fb1);
        }
    }
}

static void drawChar(int x, int y, char c, uint16_t col565, int size, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (c < 32 || c > 126) c = '?';
    int fontIdx = c - 32;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[fontIdx][col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                if (size == 1) {
                    setPixel(x + col, y + row, col565, w, h, fb0, fb1);
                } else {
                    drawRect(x + (col * size), y + (row * size), size, size, col565, true, w, h, fb0, fb1);
                }
            }
        }
    }
}

static void drawText(int x, int y, const char* str, uint16_t col565, int size, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (!str) return;
    int cursorX = x;
    int cursorY = y;
    int charW = 6 * size;
    int charH = 8 * size;

    while (*str) {
        if (*str == '\n') {
            cursorX = x;
            cursorY += charH;
        } else if (*str == '\r') {
            // ignorar
        } else {
            drawChar(cursorX, cursorY, *str, col565, size, w, h, fb0, fb1);
            cursorX += charW;
        }
        str++;
    }
}

static int lua_gfx_clear(lua_State* L) {
    uint32_t col = (uint32_t)luaL_optinteger(L, 1, 0x000000);
    uint16_t col565 = colorToRGB565(col);
    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    size_t totalPixels = caps.width * caps.height;
    if (fb0) {
        for (size_t i = 0; i < totalPixels; i++) fb0[i] = col565;
    }
    if (fb1) {
        for (size_t i = 0; i < totalPixels; i++) fb1[i] = col565;
    }
    cbdos::display::flush();
    cbdos::display::flush();
    return 0;
}

static int lua_gfx_draw_rect(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int rw = (int)luaL_checkinteger(L, 3);
    int rh = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);
    bool filled = lua_toboolean(L, 6);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawRect(x, y, rw, rh, colorToRGB565(col), filled, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_circle(lua_State* L) {
    int cx = (int)luaL_checkinteger(L, 1);
    int cy = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 4);
    bool filled = lua_toboolean(L, 5);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawCircle(cx, cy, r, colorToRGB565(col), filled, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_line(lua_State* L) {
    int x0 = (int)luaL_checkinteger(L, 1);
    int y0 = (int)luaL_checkinteger(L, 2);
    int x1 = (int)luaL_checkinteger(L, 3);
    int y1 = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawLine(x0, y0, x1, y1, colorToRGB565(col), caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_text(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    uint32_t col = (uint32_t)luaL_optinteger(L, 4, 0xFFFFFF);
    int size = (int)luaL_optinteger(L, 5, 1);
    if (size < 1) size = 1;

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawText(x, y, text, colorToRGB565(col), size, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_touch(lua_State* L) {
    cbdos::input::TouchPoint tp;
    bool touched = cbdos::input::getTouch(tp);
    lua_newtable(L);
    lua_pushboolean(L, touched && tp.isPressed);
    lua_setfield(L, -2, "touched");
    lua_pushinteger(L, tp.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, tp.y);
    lua_setfield(L, -2, "y");
    return 1;
}

static int lua_gfx_flush(lua_State* L) {
    (void)L;
    cbdos::display::flush();
    return 0;
}

static int lua_gfx_width(lua_State* L) {
    auto caps = cbdos::display::getCapabilities();
    lua_pushinteger(L, caps.width);
    return 1;
}

static int lua_gfx_height(lua_State* L) {
    auto caps = cbdos::display::getCapabilities();
    lua_pushinteger(L, caps.height);
    return 1;
}

static int lua_gfx_rgb(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    uint32_t rgb = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    lua_pushinteger(L, rgb);
    return 1;
}

static int lua_gfx_pause_ui(lua_State* L) {
    uint32_t seconds = (uint32_t)luaL_optinteger(L, 1, 0);
    LuaBridge::pauseUI(seconds);
    return 0;
}

static int lua_gfx_resume_ui(lua_State* L) {
    LuaBridge::resumeUI();
    return 0;
}

static int lua_gfx_is_ui_paused(lua_State* L) {
    lua_pushboolean(L, LuaBridge::isUIPaused());
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registro de Módulos
// ─────────────────────────────────────────────────────────────────────────────

void LuaBridge::registerGfxAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_gfx_clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, lua_gfx_draw_rect);
    lua_setfield(L, -2, "draw_rect");
    lua_pushcfunction(L, lua_gfx_draw_circle);
    lua_setfield(L, -2, "draw_circle");
    lua_pushcfunction(L, lua_gfx_draw_line);
    lua_setfield(L, -2, "draw_line");
    lua_pushcfunction(L, lua_gfx_draw_text);
    lua_setfield(L, -2, "draw_text");
    lua_pushcfunction(L, lua_gfx_flush);
    lua_setfield(L, -2, "flush");
    lua_pushcfunction(L, lua_gfx_touch);
    lua_setfield(L, -2, "touch");
    lua_pushcfunction(L, lua_gfx_rgb);
    lua_setfield(L, -2, "rgb");
    lua_pushcfunction(L, lua_gfx_width);
    lua_setfield(L, -2, "width");
    lua_pushcfunction(L, lua_gfx_height);
    lua_setfield(L, -2, "height");
    lua_pushcfunction(L, lua_gfx_pause_ui);
    lua_setfield(L, -2, "pause_ui");
    lua_pushcfunction(L, lua_gfx_resume_ui);
    lua_setfield(L, -2, "resume_ui");
    lua_pushcfunction(L, lua_gfx_is_ui_paused);
    lua_setfield(L, -2, "is_ui_paused");
    lua_setfield(L, -2, "gfx");
}

void LuaBridge::registerAudioAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_beep);
    lua_setfield(L, -2, "beep");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_mp3");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_file");
    lua_pushcfunction(L, lua_stop_audio);
    lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, lua_set_volume);
    lua_setfield(L, -2, "set_volume");
    lua_pushcfunction(L, lua_record_start);
    lua_setfield(L, -2, "record_start");
    lua_pushcfunction(L, lua_record_stop);
    lua_setfield(L, -2, "record_stop");
    lua_pushcfunction(L, lua_is_recording);
    lua_setfield(L, -2, "is_recording");
    lua_pushcfunction(L, lua_get_mic_level);
    lua_setfield(L, -2, "get_mic_level");
    lua_setfield(L, -2, "audio");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_beep);
    lua_setfield(L, -2, "beep");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_mp3");
    lua_pushcfunction(L, lua_play_mp3);
    lua_setfield(L, -2, "play_file");
    lua_pushcfunction(L, lua_stop_audio);
    lua_setfield(L, -2, "stop_audio");
    lua_pushcfunction(L, lua_set_volume);
    lua_setfield(L, -2, "set_volume");
    lua_pushcfunction(L, lua_get_volume);
    lua_setfield(L, -2, "get_volume");
}

void LuaBridge::registerSystemAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "sleep");
    lua_pushcfunction(L, lua_millis);
    lua_setfield(L, -2, "millis");
    lua_pushcfunction(L, lua_millis);
    lua_setfield(L, -2, "get_time");
    lua_pushcfunction(L, lua_free_psram);
    lua_setfield(L, -2, "free_psram");
    lua_pushcfunction(L, lua_free_heap);
    lua_setfield(L, -2, "free_heap");
    lua_pushcfunction(L, lua_get_battery);
    lua_setfield(L, -2, "get_battery");
    lua_pushcfunction(L, lua_wifi_status);
    lua_setfield(L, -2, "wifi_status");
    lua_pushcfunction(L, lua_get_ip);
    lua_setfield(L, -2, "get_ip");
    lua_pushcfunction(L, lua_cpu_temp);
    lua_setfield(L, -2, "cpu_temp");
    lua_setfield(L, -2, "system");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "delay");
    lua_pushcfunction(L, lua_delay);
    lua_setfield(L, -2, "sleep");
    lua_pushcfunction(L, lua_millis);
    lua_setfield(L, -2, "millis");
    lua_pushcfunction(L, lua_free_psram);
    lua_setfield(L, -2, "free_psram");
    lua_pushcfunction(L, lua_free_heap);
    lua_setfield(L, -2, "free_heap");
    lua_pushcfunction(L, lua_get_battery);
    lua_setfield(L, -2, "get_battery");
    lua_pushcfunction(L, lua_wifi_status);
    lua_setfield(L, -2, "wifi_status");
    lua_pushcfunction(L, lua_get_ip);
    lua_setfield(L, -2, "get_ip");
}

void LuaBridge::registerGpioAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_pin_mode);
    lua_setfield(L, -2, "pin_mode");
    lua_pushcfunction(L, lua_digital_write);
    lua_setfield(L, -2, "digital_write");
    lua_pushcfunction(L, lua_digital_read);
    lua_setfield(L, -2, "digital_read");
    lua_setfield(L, -2, "gpio");

    // Accesos directos en cbdos.*
    lua_pushcfunction(L, lua_pin_mode);
    lua_setfield(L, -2, "pin_mode");
    lua_pushcfunction(L, lua_digital_write);
    lua_setfield(L, -2, "digital_write");
    lua_pushcfunction(L, lua_digital_read);
    lua_setfield(L, -2, "digital_read");
}

void LuaBridge::registerFsAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_read_file);
    lua_setfield(L, -2, "read_file");
    lua_pushcfunction(L, lua_write_file);
    lua_setfield(L, -2, "write_file");
    lua_pushcfunction(L, lua_file_exists);
    lua_setfield(L, -2, "file_exists");
    lua_pushcfunction(L, lua_list_dir);
    lua_setfield(L, -2, "list_dir");
    lua_pushcfunction(L, lua_mount_sd);
    lua_setfield(L, -2, "mount_sd");
    lua_pushcfunction(L, lua_format_sd);
    lua_setfield(L, -2, "format_sd");
    lua_setfield(L, -2, "fs");

    // Alias cbdos.storage
    lua_newtable(L);
    lua_pushcfunction(L, lua_read_file);
    lua_setfield(L, -2, "read_file");
    lua_pushcfunction(L, lua_write_file);
    lua_setfield(L, -2, "write_file");
    lua_pushcfunction(L, lua_file_exists);
    lua_setfield(L, -2, "file_exists");
    lua_pushcfunction(L, lua_list_dir);
    lua_setfield(L, -2, "list_dir");
    lua_pushcfunction(L, lua_mount_sd);
    lua_setfield(L, -2, "mount_sd");
    lua_pushcfunction(L, lua_format_sd);
    lua_setfield(L, -2, "format_sd");
    lua_setfield(L, -2, "storage");
}

void LuaBridge::registerUartAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_uart_init);
    lua_setfield(L, -2, "init");
    lua_pushcfunction(L, lua_uart_write);
    lua_setfield(L, -2, "write");
    lua_pushcfunction(L, lua_uart_read);
    lua_setfield(L, -2, "read");
    lua_pushcfunction(L, lua_uart_available);
    lua_setfield(L, -2, "available");
    lua_pushcfunction(L, lua_uart_flush);
    lua_setfield(L, -2, "flush");
    lua_setfield(L, -2, "uart");
}

void LuaBridge::registerCanvasAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_canvas_fill);
    lua_setfield(L, -2, "fill");
    lua_pushcfunction(L, lua_canvas_set_px);
    lua_setfield(L, -2, "set_px");
    lua_pushcfunction(L, lua_canvas_get_px);
    lua_setfield(L, -2, "get_px");
    lua_pushcfunction(L, lua_canvas_draw_rect);
    lua_setfield(L, -2, "draw_rect");
    lua_pushcfunction(L, lua_canvas_draw_line);
    lua_setfield(L, -2, "draw_line");
    lua_pushcfunction(L, lua_canvas_draw_circle);
    lua_setfield(L, -2, "draw_circle");
    lua_pushcfunction(L, lua_canvas_draw_text);
    lua_setfield(L, -2, "draw_text");
    lua_pushcfunction(L, lua_canvas_flood_fill);
    lua_setfield(L, -2, "flood_fill");
    lua_pushcfunction(L, lua_canvas_on_click);
    lua_setfield(L, -2, "on_click");
    lua_pushcfunction(L, lua_canvas_on_touch);
    lua_setfield(L, -2, "on_touch");
    lua_pushcfunction(L, lua_canvas_refresh);
    lua_setfield(L, -2, "refresh");
    lua_pushcfunction(L, lua_canvas_save_bmp);
    lua_setfield(L, -2, "save_bmp");
    lua_pushcfunction(L, lua_canvas_load_bmp);
    lua_setfield(L, -2, "load_bmp");
    lua_setfield(L, -2, "canvas");
}

void LuaBridge::registerUIAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_ui_create_card);
    lua_setfield(L, -2, "create_card");
    lua_pushcfunction(L, lua_ui_create_sunken_card);
    lua_setfield(L, -2, "create_sunken_card");
    lua_pushcfunction(L, lua_ui_create_label);
    lua_setfield(L, -2, "create_label");
    lua_pushcfunction(L, lua_ui_set_text);
    lua_setfield(L, -2, "set_text");
    lua_pushcfunction(L, lua_ui_set_color);
    lua_setfield(L, -2, "set_color");
    lua_pushcfunction(L, lua_ui_set_font_size);
    lua_setfield(L, -2, "set_font_size");
    lua_pushcfunction(L, lua_ui_create_button);
    lua_setfield(L, -2, "create_button");
    lua_pushcfunction(L, lua_ui_create_dropdown);
    lua_setfield(L, -2, "create_dropdown");
    lua_pushcfunction(L, lua_ui_get_selected);
    lua_setfield(L, -2, "get_selected");
    lua_pushcfunction(L, lua_ui_set_selected);
    lua_setfield(L, -2, "set_selected");
    lua_pushcfunction(L, lua_ui_create_slider);
    lua_setfield(L, -2, "create_slider");
    lua_pushcfunction(L, lua_ui_create_switch);
    lua_setfield(L, -2, "create_switch");
    lua_pushcfunction(L, lua_ui_create_row);
    lua_setfield(L, -2, "create_row");
    lua_pushcfunction(L, lua_ui_create_column);
    lua_setfield(L, -2, "create_column");
    lua_pushcfunction(L, lua_ui_create_canvas);
    lua_setfield(L, -2, "create_canvas");
    lua_pushcfunction(L, lua_ui_set_size);
    lua_setfield(L, -2, "set_size");
    lua_pushcfunction(L, lua_ui_show_toast);
    lua_setfield(L, -2, "show_toast");
    lua_setfield(L, -2, "ui");
}

// ─────────────────────────────────────────────────────────────────────────────
// USB HID & MacroPad API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_hid_is_connected(lua_State* L) {
    lua_pushboolean(L, cbdos::hid::isConnected());
    return 1;
}

static int lua_hid_is_ready(lua_State* L) {
    lua_pushboolean(L, cbdos::hid::isReady());
    return 1;
}

static int lua_hid_type(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    uint32_t delay_ms = (uint32_t)luaL_optinteger(L, 2, 10);
    if (text) {
        cbdos::hid::sendString(text, delay_ms);
    }
    return 0;
}

static int lua_hid_press_key(lua_State* L) {
    const char* keyName = luaL_checkstring(L, 1);
    const char* modName = luaL_optstring(L, 2, nullptr);
    uint8_t mod = modName ? cbdos::hid::nameToModifier(modName) : static_cast<uint8_t>(cbdos::hid::MOD_NONE);
    uint8_t keycode = cbdos::hid::nameToKeycode(keyName);

    if (keycode != cbdos::hid::keycode::KEY_NONE) {
        cbdos::hid::sendKeyPress(keycode, mod);
        cbdos::system::sleepMs(20);
        cbdos::hid::sendKeyRelease();
    }
    return 0;
}

static int lua_hid_press_gui(lua_State* L) {
    const char* keyName = luaL_checkstring(L, 1);
    uint8_t keycode = cbdos::hid::nameToKeycode(keyName);
    if (keycode != cbdos::hid::keycode::KEY_NONE) {
        cbdos::hid::sendKeyPress(keycode, cbdos::hid::MOD_LGUI);
        cbdos::system::sleepMs(20);
        cbdos::hid::sendKeyRelease();
    }
    return 0;
}

static int lua_hid_press_combo(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "press_combo espera una tabla de teclas ej: {'CTRL', 'ALT', 't'}");
    }

    uint8_t mod = 0;
    std::vector<uint8_t> keys;

    size_t len = lua_rawlen(L, 1);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_isstring(L, -1)) {
            const char* token = lua_tostring(L, -1);
            uint8_t m = cbdos::hid::nameToModifier(token);
            if (m != cbdos::hid::MOD_NONE) {
                mod |= m;
            } else {
                uint8_t k = cbdos::hid::nameToKeycode(token);
                if (k != cbdos::hid::keycode::KEY_NONE) {
                    keys.push_back(k);
                }
            }
        }
        lua_pop(L, 1);
    }

    cbdos::hid::sendCombo(keys, mod);
    return 0;
}

static int lua_hid_mouse_move(lua_State* L) {
    int dx = luaL_checkinteger(L, 1);
    int dy = luaL_checkinteger(L, 2);
    int wheel = luaL_optinteger(L, 3, 0);
    cbdos::hid::mouseMove(static_cast<int8_t>(dx), static_cast<int8_t>(dy), static_cast<int8_t>(wheel));
    return 0;
}

static int lua_hid_mouse_click(lua_State* L) {
    const char* btn = luaL_optstring(L, 1, "LEFT");
    std::string b = btn;
    std::transform(b.begin(), b.end(), b.begin(), ::toupper);

    if (b == "RIGHT") {
        cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_RIGHT);
    } else if (b == "MIDDLE") {
        cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_MIDDLE);
    } else {
        cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_LEFT);
    }
    return 0;
}

static int lua_hid_get_leds(lua_State* L) {
    uint8_t state = cbdos::hid::getLedState();
    lua_newtable(L);

    lua_pushboolean(L, (state & cbdos::hid::LED_NUMLOCK) != 0);
    lua_setfield(L, -2, "numlock");

    lua_pushboolean(L, (state & cbdos::hid::LED_CAPSLOCK) != 0);
    lua_setfield(L, -2, "capslock");

    lua_pushboolean(L, (state & cbdos::hid::LED_SCROLLLOCK) != 0);
    lua_setfield(L, -2, "scrolllock");

    lua_pushinteger(L, state);
    lua_setfield(L, -2, "raw");

    return 1;
}

static int lua_hid_wait_led_event(lua_State* L) {
    uint32_t timeout_ms = (uint32_t)luaL_optinteger(L, 1, 3000);
    uint8_t mask = (uint8_t)luaL_optinteger(L, 2, (cbdos::hid::LED_CAPSLOCK | cbdos::hid::LED_NUMLOCK | cbdos::hid::LED_SCROLLLOCK));

    bool changed = cbdos::hid::waitForLedEvent(mask, timeout_ms);
    if (changed) {
        uint8_t cur = cbdos::hid::getLedState();
        lua_pushinteger(L, cur);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int lua_hid_enable(lua_State* L) {
    bool ok = cbdos::hid::enable();
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_hid_disable(lua_State* L) {
    bool ok = cbdos::hid::disable();
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_hid_is_enabled(lua_State* L) {
    lua_pushboolean(L, cbdos::hid::isEnabled());
    return 1;
}

static int lua_hid_delay(lua_State* L) {
    uint32_t ms = (uint32_t)luaL_checkinteger(L, 1);
    cbdos::system::sleepMs(ms);
    return 0;
}

void LuaBridge::registerHidAPI(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_hid_enable);
    lua_setfield(L, -2, "enable");
    lua_pushcfunction(L, lua_hid_enable);
    lua_setfield(L, -2, "start");
    lua_pushcfunction(L, lua_hid_disable);
    lua_setfield(L, -2, "disable");
    lua_pushcfunction(L, lua_hid_disable);
    lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, lua_hid_is_enabled);
    lua_setfield(L, -2, "is_enabled");

    lua_pushcfunction(L, lua_hid_is_connected);
    lua_setfield(L, -2, "is_connected");
    lua_pushcfunction(L, lua_hid_is_ready);
    lua_setfield(L, -2, "is_ready");
    lua_pushcfunction(L, lua_hid_type);
    lua_setfield(L, -2, "type");
    lua_pushcfunction(L, lua_hid_press_key);
    lua_setfield(L, -2, "press_key");
    lua_pushcfunction(L, lua_hid_press_gui);
    lua_setfield(L, -2, "press_gui");
    lua_pushcfunction(L, lua_hid_press_combo);
    lua_setfield(L, -2, "press_combo");
    lua_pushcfunction(L, lua_hid_mouse_move);
    lua_setfield(L, -2, "mouse_move");
    lua_pushcfunction(L, lua_hid_mouse_click);
    lua_setfield(L, -2, "mouse_click");
    lua_pushcfunction(L, lua_hid_get_leds);
    lua_setfield(L, -2, "get_leds");
    lua_pushcfunction(L, lua_hid_wait_led_event);
    lua_setfield(L, -2, "wait_led_event");
    lua_pushcfunction(L, lua_hid_delay);
    lua_setfield(L, -2, "delay");

    // Constantes de LEDs
    lua_pushinteger(L, cbdos::hid::LED_NUMLOCK);
    lua_setfield(L, -2, "LED_NUMLOCK");
    lua_pushinteger(L, cbdos::hid::LED_CAPSLOCK);
    lua_setfield(L, -2, "LED_CAPSLOCK");
    lua_pushinteger(L, cbdos::hid::LED_SCROLLLOCK);
    lua_setfield(L, -2, "LED_SCROLLLOCK");

    lua_setfield(L, -2, "hid");
}

// ─────────────────────────────────────────────────────────────────────────────
// BadUSB DuckyScript API
// ─────────────────────────────────────────────────────────────────────────────
static int lua_ducky_load_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool ok = cbdos::ducky::loadFile(path ? path : "");
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_ducky_run(lua_State* L) {
    cbdos::ducky::run();
    return 0;
}

static int lua_ducky_stop(lua_State* L) {
    cbdos::ducky::stop();
    return 0;
}

static int lua_ducky_is_running(lua_State* L) {
    lua_pushboolean(L, cbdos::ducky::isRunning());
    return 1;
}

static int lua_ducky_set_default_delay(lua_State* L) {
    uint32_t ms = (uint32_t)luaL_checkinteger(L, 1);
    cbdos::ducky::DuckyInterpreter::getInstance().setDefaultDelay(ms);
    return 0;
}

void LuaBridge::registerDuckyAPI(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_ducky_load_file);
    lua_setfield(L, -2, "load_file");
    lua_pushcfunction(L, lua_ducky_run);
    lua_setfield(L, -2, "run");
    lua_pushcfunction(L, lua_ducky_stop);
    lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, lua_ducky_is_running);
    lua_setfield(L, -2, "is_running");
    lua_pushcfunction(L, lua_ducky_set_default_delay);
    lua_setfield(L, -2, "set_default_delay");

    lua_setfield(L, -2, "ducky");
}

// ─────────────────────────────────────────────────────────────────────────────
// SSH Client API (cbdos.ssh.* / ssh.*)
// ─────────────────────────────────────────────────────────────────────────────
static int lua_ssh_connect(lua_State* L) {
    cbdos::ssh::SshConfig cfg;

    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "host");
        if (lua_isstring(L, -1)) cfg.host = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "port");
        cfg.port = (uint16_t)luaL_optinteger(L, -1, 22);
        lua_pop(L, 1);

        lua_getfield(L, 1, "username");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "user");
        }
        if (lua_isstring(L, -1)) cfg.username = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "password");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "pass");
        }
        if (lua_isstring(L, -1)) cfg.password = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "key_path");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "key");
        }
        if (lua_isstring(L, -1)) {
            cfg.privateKeyPath = lua_tostring(L, -1);
            cfg.authType = cbdos::ssh::SshAuthType::PublicKey;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "passphrase");
        if (lua_isstring(L, -1)) cfg.passphrase = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "timeout");
        cfg.timeoutMs = (uint32_t)luaL_optinteger(L, -1, 8000);
        lua_pop(L, 1);
    } else {
        cfg.host = luaL_checkstring(L, 1);
        cfg.username = luaL_optstring(L, 2, "root");
        cfg.password = luaL_optstring(L, 3, "");
        cfg.port = (uint16_t)luaL_optinteger(L, 4, 22);
        cfg.timeoutMs = (uint32_t)luaL_optinteger(L, 5, 8000);
    }

    std::string errMsg;
    bool ok = cbdos::ssh::connect(cfg, [&errMsg](cbdos::ssh::SshSessionState state, const std::string& msg) {
        if (state == cbdos::ssh::SshSessionState::ErrorAuthFailed ||
            state == cbdos::ssh::SshSessionState::ErrorSocket ||
            state == cbdos::ssh::SshSessionState::ErrorTimeout) {
            errMsg = msg;
        }
    });

    lua_pushboolean(L, ok);
    if (!ok) {
        lua_pushstring(L, errMsg.empty() ? "Fallo al conectar SSH" : errMsg.c_str());
        return 2;
    }
    return 1;
}

static int lua_ssh_disconnect(lua_State* L) {
    cbdos::ssh::disconnect();
    return 0;
}

static int lua_ssh_is_connected(lua_State* L) {
    lua_pushboolean(L, cbdos::ssh::isConnected());
    return 1;
}

static int lua_ssh_exec(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    uint32_t timeoutMs = (uint32_t)luaL_optinteger(L, 2, 10000);

    cbdos::ssh::SshExecResult res = cbdos::ssh::execute(cmd, timeoutMs);

    if (!res.success && res.stdOut.empty() && !res.errorMessage.empty()) {
        lua_pushnil(L);
        lua_pushstring(L, res.errorMessage.c_str());
        lua_pushinteger(L, res.exitCode);
        return 3;
    }

    lua_pushstring(L, res.stdOut.c_str());
    lua_pushinteger(L, res.exitCode);
    lua_pushstring(L, res.stdErr.c_str());
    return 3;
}

static int lua_ssh_write(lua_State* L) {
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    bool ok = cbdos::ssh::sendInput(reinterpret_cast<const uint8_t*>(data), len);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_ssh_close_shell(lua_State* L) {
    cbdos::ssh::closeShell();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Net API (LAN Recon v2): ping ICMP real + sonda TCP + sweep async.
// ─────────────────────────────────────────────────────────────────────────────
static int lua_net_ping(lua_State* L) {
    const char* ip = luaL_checkstring(L, 1);
    uint32_t timeoutMs = (uint32_t)luaL_optinteger(L, 2, 1000);
    uint32_t rtt = 0;
    std::string method;
    bool ok = cbdos::network::LanScannerService::getInstance().pingSingle(
        ip ? ip : "", timeoutMs, &rtt, &method);
    lua_pushboolean(L, ok);
    lua_pushinteger(L, (lua_Integer)rtt);
    lua_pushstring(L, method.c_str());
    return 3;
}

static int lua_net_probe_port(lua_State* L) {
    const char* ip = luaL_checkstring(L, 1);
    int port = (int)luaL_checkinteger(L, 2);
    uint32_t timeoutMs = (uint32_t)luaL_optinteger(L, 3, 300);
    bool open = false;
    auto* backend = cbdos::network::getLanScannerBackend();
    if (backend && ip && port > 0 && port < 65536) {
        open = backend->probeTcpPort(ip, (uint16_t)port, timeoutMs);
    }
    lua_pushboolean(L, open);
    return 1;
}

static int lua_net_scan_start(lua_State* L) {
    const char* cidr = luaL_optstring(L, 1, "");
    bool ok = false;
    if (cidr && cidr[0] != '\0') {
        ok = cbdos::network::LanScannerService::getInstance().startScanCidr(cidr);
    } else {
        ok = cbdos::network::LanScannerService::getInstance().startScan();
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_net_scan_stop(lua_State* L) {
    (void)L;
    cbdos::network::LanScannerService::getInstance().stopScan();
    return 0;
}

static int lua_net_scanning(lua_State* L) {
    lua_pushboolean(L, cbdos::network::LanScannerService::getInstance().isScanning());
    return 1;
}

static int lua_net_results(lua_State* L) {
    std::vector<cbdos::network::LanHostInfo> hosts =
        cbdos::network::LanScannerService::getInstance().getResults();
    lua_newtable(L);
    int idx = 1;
    for (const auto& h : hosts) {
        lua_newtable(L);
        lua_pushstring(L, h.ip.c_str());
        lua_setfield(L, -2, "ip");
        lua_pushstring(L, h.getMacString().c_str());
        lua_setfield(L, -2, "mac");
        lua_pushstring(L, h.vendor.c_str());
        lua_setfield(L, -2, "vendor");
        lua_pushstring(L, h.banner.c_str());
        lua_setfield(L, -2, "banner");
        lua_pushstring(L, h.discovery.c_str());
        lua_setfield(L, -2, "discovery");
        lua_pushstring(L, h.ssdpServer.c_str());
        lua_setfield(L, -2, "server");
        lua_pushinteger(L, (lua_Integer)h.rttMs);
        lua_setfield(L, -2, "rtt");
        lua_pushboolean(L, h.isTv);
        lua_setfield(L, -2, "is_tv");
        lua_newtable(L);
        int pidx = 1;
        for (size_t i = 0; i < cbdos::network::kLanReconPortCount; ++i) {
            uint16_t port = cbdos::network::kLanReconPorts[i];
            if (h.hasPort(port)) {
                lua_pushinteger(L, (lua_Integer)port);
                lua_rawseti(L, -2, pidx++);
            }
        }
        lua_setfield(L, -2, "ports");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

void LuaBridge::registerSshAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_ssh_connect);
    lua_setfield(L, -2, "connect");
    lua_pushcfunction(L, lua_ssh_disconnect);
    lua_setfield(L, -2, "disconnect");
    lua_pushcfunction(L, lua_ssh_disconnect);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, lua_ssh_is_connected);
    lua_setfield(L, -2, "is_connected");
    lua_pushcfunction(L, lua_ssh_exec);
    lua_setfield(L, -2, "exec");
    lua_pushcfunction(L, lua_ssh_exec);
    lua_setfield(L, -2, "execute");
    lua_pushcfunction(L, lua_ssh_write);
    lua_setfield(L, -2, "write");
    lua_pushcfunction(L, lua_ssh_write);
    lua_setfield(L, -2, "send");
    lua_pushcfunction(L, lua_ssh_close_shell);
    lua_setfield(L, -2, "close_shell");
    lua_setfield(L, -2, "ssh");
}

void LuaBridge::registerNetAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_net_ping);
    lua_setfield(L, -2, "ping");
    lua_pushcfunction(L, lua_net_probe_port);
    lua_setfield(L, -2, "probe_port");
    lua_pushcfunction(L, lua_net_scan_start);
    lua_setfield(L, -2, "scan_start");
    lua_pushcfunction(L, lua_net_scan_stop);
    lua_setfield(L, -2, "scan_stop");
    lua_pushcfunction(L, lua_net_scanning);
    lua_setfield(L, -2, "scanning");
    lua_pushcfunction(L, lua_net_results);
    lua_setfield(L, -2, "results");
    lua_setfield(L, -2, "net");
}

void LuaBridge::registerAll(lua_State* L) {
    if (!L) return;

    // Crear la tabla global cbdos
    lua_newtable(L);

    registerAudioAPI(L);
    registerSystemAPI(L);
    registerGpioAPI(L);
    registerFsAPI(L);
    registerGfxAPI(L);
    registerUartAPI(L);
    registerUIAPI(L);
    registerCanvasAPI(L);
    registerHidAPI(L);
    registerDuckyAPI(L);
    registerSshAPI(L);
    registerNetAPI(L);

    // Guardar tabla como global "cbdos"
    lua_setglobal(L, "cbdos");

    // Aliases globales directos para scripts estilo ducky/badusb y scripts estándar
    lua_getglobal(L, "cbdos");
    lua_getfield(L, -1, "hid");
    lua_setglobal(L, "hid");
    lua_getglobal(L, "cbdos");
    lua_getfield(L, -1, "ducky");
    lua_setglobal(L, "ducky");
    lua_getglobal(L, "cbdos");
    lua_getfield(L, -1, "system");
    lua_setglobal(L, "sys");
    lua_getglobal(L, "cbdos");
    lua_getfield(L, -1, "ssh");
    lua_setglobal(L, "ssh");
    lua_getglobal(L, "cbdos");
    lua_getfield(L, -1, "net");
    lua_setglobal(L, "net");

    printf("[LuaBridge] Bindings 'cbdos.*', 'sys.*', 'hid.*', 'ducky.*', 'ssh.*', 'net.*' registrados.\n");
}
