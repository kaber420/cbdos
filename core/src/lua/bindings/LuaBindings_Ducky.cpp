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

void registerDuckyAPI(lua_State* L) {
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

