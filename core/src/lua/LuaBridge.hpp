#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdint>

struct lua_State;

class LuaBridge {
public:
    // Registra todas las APIs nativas del hardware, UI y Canvas en el estado de Lua (namespace cbdos.*)
    static void registerAll(lua_State* L);

    static void pauseUI(uint32_t seconds = 0);
    static void resumeUI();
    static bool isUIPaused();
    static bool checkAndClearNeedsRefresh();

};
