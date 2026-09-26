#include "LuaBridge.hpp"
#include "bindings/LuaBridge_Internal.hpp"
#include <cstdio>

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
    registerCanvasAPI(L);
    registerUIAPI(L);
    registerHidAPI(L);
    registerDuckyAPI(L);
    registerSshAPI(L);
    registerNetAPI(L);
    registerHttpAPI(L);

    // Guardar tabla como global "cbdos"
    lua_setglobal(L, "cbdos");

    // Aliases globales
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
    lua_getglobal(L, "cbdos");
    lua_getfield(L, -1, "http");
    lua_setglobal(L, "http");

    printf("[LuaBridge] Bindings registrados.\\n");
}
