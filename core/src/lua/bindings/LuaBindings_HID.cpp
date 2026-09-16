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

void registerHidAPI(lua_State* L) {
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

