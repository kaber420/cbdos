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


void registerGpioAPI(lua_State* L) {
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

void registerUartAPI(lua_State* L) {
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
