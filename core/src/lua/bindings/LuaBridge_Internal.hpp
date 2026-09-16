#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "../LuaBridge.hpp"

void registerSystemAPI(lua_State* L);
void registerGpioAPI(lua_State* L);
void registerUartAPI(lua_State* L);
void registerFsAPI(lua_State* L);
void registerAudioAPI(lua_State* L);
void registerUIAPI(lua_State* L);
void registerGfxAPI(lua_State* L);
void registerCanvasAPI(lua_State* L);
void registerHidAPI(lua_State* L);
void registerDuckyAPI(lua_State* L);
void registerSshAPI(lua_State* L);
void registerNetAPI(lua_State* L);

inline uint16_t colorToRGB565(uint32_t c) {
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = c & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

extern const uint8_t font5x7[96][5];

int lua_ui_create_canvas(lua_State* L);
struct _lv_obj_t;
struct _lv_obj_t* get_target_parent(lua_State* L, int argIdx);