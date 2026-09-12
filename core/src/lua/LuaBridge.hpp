#pragma once
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

private:
    static void registerAudioAPI(lua_State* L);
    static void registerSystemAPI(lua_State* L);
    static void registerGpioAPI(lua_State* L);
    static void registerFsAPI(lua_State* L);
    static void registerGfxAPI(lua_State* L);
    static void registerUartAPI(lua_State* L);
    static void registerUIAPI(lua_State* L);
    static void registerCanvasAPI(lua_State* L);
    static void registerHidAPI(lua_State* L);
    static void registerDuckyAPI(lua_State* L);
    static void registerSshAPI(lua_State* L);
    static void registerNetAPI(lua_State* L);
};
