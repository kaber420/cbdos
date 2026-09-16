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
// Graphics API (Direct Framebuffer 2D / cbdos.gfx.*)
// ─────────────────────────────────────────────────────────────────────────────
static inline void setPixel(int x, int y, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (x >= 0 && x < w && y >= 0 && y < h) {
        int idx = y * w + x;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawHLine(int x, int y, int len, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (y < 0 || y >= h) return;
    int xStart = std::max(0, x);
    int xEnd = std::min(w, x + len);
    for (int i = xStart; i < xEnd; i++) {
        int idx = y * w + i;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawVLine(int x, int y, int len, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (x < 0 || x >= w) return;
    int yStart = std::max(0, y);
    int yEnd = std::min(h, y + len);
    for (int j = yStart; j < yEnd; j++) {
        int idx = j * w + x;
        if (fb0) fb0[idx] = col565;
        if (fb1) fb1[idx] = col565;
    }
}

static void drawRect(int x, int y, int rw, int rh, uint16_t col565, bool filled, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (filled) {
        int yStart = std::max(0, y);
        int yEnd = std::min(h, y + rh);
        for (int j = yStart; j < yEnd; j++) {
            drawHLine(x, j, rw, col565, w, h, fb0, fb1);
        }
    } else {
        drawHLine(x, y, rw, col565, w, h, fb0, fb1);
        drawHLine(x, y + rh - 1, rw, col565, w, h, fb0, fb1);
        drawVLine(x, y, rh, col565, w, h, fb0, fb1);
        drawVLine(x + rw - 1, y, rh, col565, w, h, fb0, fb1);
    }
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t col565, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixel(x0, y0, col565, w, h, fb0, fb1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void drawCircle(int cx, int cy, int r, uint16_t col565, bool filled, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (r <= 0) return;
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    if (filled) {
        drawHLine(cx - r, cy, 2 * r + 1, col565, w, h, fb0, fb1);
    } else {
        setPixel(cx, cy + r, col565, w, h, fb0, fb1);
        setPixel(cx, cy - r, col565, w, h, fb0, fb1);
        setPixel(cx + r, cy, col565, w, h, fb0, fb1);
        setPixel(cx - r, cy, col565, w, h, fb0, fb1);
    }

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (filled) {
            drawHLine(cx - x, cy + y, 2 * x + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - x, cy - y, 2 * x + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - y, cy + x, 2 * y + 1, col565, w, h, fb0, fb1);
            drawHLine(cx - y, cy - x, 2 * y + 1, col565, w, h, fb0, fb1);
        } else {
            setPixel(cx + x, cy + y, col565, w, h, fb0, fb1);
            setPixel(cx - x, cy + y, col565, w, h, fb0, fb1);
            setPixel(cx + x, cy - y, col565, w, h, fb0, fb1);
            setPixel(cx - x, cy - y, col565, w, h, fb0, fb1);
            setPixel(cx + y, cy + x, col565, w, h, fb0, fb1);
            setPixel(cx - y, cy + x, col565, w, h, fb0, fb1);
            setPixel(cx + y, cy - x, col565, w, h, fb0, fb1);
            setPixel(cx - y, cy - x, col565, w, h, fb0, fb1);
        }
    }
}

static void drawChar(int x, int y, char c, uint16_t col565, int size, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (c < 32 || c > 126) c = '?';
    int fontIdx = c - 32;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[fontIdx][col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                if (size == 1) {
                    setPixel(x + col, y + row, col565, w, h, fb0, fb1);
                } else {
                    drawRect(x + (col * size), y + (row * size), size, size, col565, true, w, h, fb0, fb1);
                }
            }
        }
    }
}

static void drawText(int x, int y, const char* str, uint16_t col565, int size, int w, int h, uint16_t* fb0, uint16_t* fb1) {
    if (!str) return;
    int cursorX = x;
    int cursorY = y;
    int charW = 6 * size;
    int charH = 8 * size;

    while (*str) {
        if (*str == '\n') {
            cursorX = x;
            cursorY += charH;
        } else if (*str == '\r') {
            // ignorar
        } else {
            drawChar(cursorX, cursorY, *str, col565, size, w, h, fb0, fb1);
            cursorX += charW;
        }
        str++;
    }
}

static int lua_gfx_clear(lua_State* L) {
    uint32_t col = (uint32_t)luaL_optinteger(L, 1, 0x000000);
    uint16_t col565 = colorToRGB565(col);
    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    size_t totalPixels = caps.width * caps.height;
    if (fb0) {
        for (size_t i = 0; i < totalPixels; i++) fb0[i] = col565;
    }
    if (fb1) {
        for (size_t i = 0; i < totalPixels; i++) fb1[i] = col565;
    }
    cbdos::display::flush();
    cbdos::display::flush();
    return 0;
}

static int lua_gfx_draw_rect(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int rw = (int)luaL_checkinteger(L, 3);
    int rh = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);
    bool filled = lua_toboolean(L, 6);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawRect(x, y, rw, rh, colorToRGB565(col), filled, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_circle(lua_State* L) {
    int cx = (int)luaL_checkinteger(L, 1);
    int cy = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 4);
    bool filled = lua_toboolean(L, 5);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawCircle(cx, cy, r, colorToRGB565(col), filled, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_line(lua_State* L) {
    int x0 = (int)luaL_checkinteger(L, 1);
    int y0 = (int)luaL_checkinteger(L, 2);
    int x1 = (int)luaL_checkinteger(L, 3);
    int y1 = (int)luaL_checkinteger(L, 4);
    uint32_t col = (uint32_t)luaL_checkinteger(L, 5);

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawLine(x0, y0, x1, y1, colorToRGB565(col), caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_draw_text(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    uint32_t col = (uint32_t)luaL_optinteger(L, 4, 0xFFFFFF);
    int size = (int)luaL_optinteger(L, 5, 1);
    if (size < 1) size = 1;

    auto caps = cbdos::display::getCapabilities();
    uint16_t* fb0 = (uint16_t*)cbdos::display::getFramebuffer(0);
    uint16_t* fb1 = (uint16_t*)cbdos::display::getFramebuffer(1);
    drawText(x, y, text, colorToRGB565(col), size, caps.width, caps.height, fb0, fb1);
    return 0;
}

static int lua_gfx_touch(lua_State* L) {
    cbdos::input::TouchPoint tp;
    bool touched = cbdos::input::getTouch(tp);
    lua_newtable(L);
    lua_pushboolean(L, touched && tp.isPressed);
    lua_setfield(L, -2, "touched");
    lua_pushinteger(L, tp.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, tp.y);
    lua_setfield(L, -2, "y");
    return 1;
}

static int lua_gfx_flush(lua_State* L) {
    (void)L;
    cbdos::display::flush();
    return 0;
}

static int lua_gfx_width(lua_State* L) {
    auto caps = cbdos::display::getCapabilities();
    lua_pushinteger(L, caps.width);
    return 1;
}

static int lua_gfx_height(lua_State* L) {
    auto caps = cbdos::display::getCapabilities();
    lua_pushinteger(L, caps.height);
    return 1;
}

static int lua_gfx_rgb(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    uint32_t rgb = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    lua_pushinteger(L, rgb);
    return 1;
}

static int lua_gfx_pause_ui(lua_State* L) {
    uint32_t seconds = (uint32_t)luaL_optinteger(L, 1, 0);
    LuaBridge::pauseUI(seconds);
    return 0;
}

static int lua_gfx_resume_ui(lua_State* L) {
    LuaBridge::resumeUI();
    return 0;
}

static int lua_gfx_is_ui_paused(lua_State* L) {
    lua_pushboolean(L, LuaBridge::isUIPaused());
    return 1;
}


void registerGfxAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_gfx_clear);
    lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, lua_gfx_draw_rect);
    lua_setfield(L, -2, "draw_rect");
    lua_pushcfunction(L, lua_gfx_draw_circle);
    lua_setfield(L, -2, "draw_circle");
    lua_pushcfunction(L, lua_gfx_draw_line);
    lua_setfield(L, -2, "draw_line");
    lua_pushcfunction(L, lua_gfx_draw_text);
    lua_setfield(L, -2, "draw_text");
    lua_pushcfunction(L, lua_gfx_flush);
    lua_setfield(L, -2, "flush");
    lua_pushcfunction(L, lua_gfx_touch);
    lua_setfield(L, -2, "touch");
    lua_pushcfunction(L, lua_gfx_rgb);
    lua_setfield(L, -2, "rgb");
    lua_pushcfunction(L, lua_gfx_width);
    lua_setfield(L, -2, "width");
    lua_pushcfunction(L, lua_gfx_height);
    lua_setfield(L, -2, "height");
    lua_pushcfunction(L, lua_gfx_pause_ui);
    lua_setfield(L, -2, "pause_ui");
    lua_pushcfunction(L, lua_gfx_resume_ui);
    lua_setfield(L, -2, "resume_ui");
    lua_pushcfunction(L, lua_gfx_is_ui_paused);
    lua_setfield(L, -2, "is_ui_paused");
    lua_setfield(L, -2, "gfx");
}
