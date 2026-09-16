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


void registerFsAPI(lua_State* L) {
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
