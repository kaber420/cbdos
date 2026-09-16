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


void registerSystemAPI(lua_State* L) {
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
