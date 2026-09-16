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
#include "../../network/LanScannerService.hpp"
#include "../../UIManager.hpp"
#include "../../themes/DefaultTheme.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <lvgl.h>

#include "LuaBridge_Internal.hpp"

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



void registerAudioAPI(lua_State* L) {
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
