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
// LVGL 9.5 Custom Pixel Canvas API (cbdos.canvas.*)
// ─────────────────────────────────────────────────────────────────────────────

struct LuaCanvasCtx {
    uint16_t* buffer;
    uint32_t width;
    uint32_t height;
    lua_State* L;
    int clickRef;
    int touchRef;
};


static inline uint32_t rgb565ToColor(uint16_t c565) {
    uint8_t r = ((c565 >> 11) & 0x1F) << 3;
    uint8_t g = ((c565 >> 5) & 0x3F) << 2;
    uint8_t b = (c565 & 0x1F) << 3;
    return (r << 16) | (g << 8) | b;
}

// 5x7 Font Bitmap table (ASCII 32..126)
extern const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x10,0x08,0x08,0x10,0x08}
};

static void canvas_event_cb(lv_event_t* e) {
    auto* ctx = static_cast<LuaCanvasCtx*>(lv_event_get_user_data(e));
    if (!ctx) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        if (ctx->L) {
            if (ctx->clickRef != LUA_NOREF) {
                luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->clickRef);
            }
            if (ctx->touchRef != LUA_NOREF) {
                luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->touchRef);
            }
        }
        if (ctx->buffer) {
            cbdos::mem::free_mem(ctx->buffer);
            ctx->buffer = nullptr;
        }
        delete ctx;
        return;
    }

    if (code == LV_EVENT_CLICKED && ctx->L && ctx->clickRef != LUA_NOREF) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        lv_area_t coords;
        lv_obj_get_coords(target, &coords);
        int32_t relX = point.x - coords.x1;
        int32_t relY = point.y - coords.y1;

        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->clickRef);
        if (lua_isfunction(ctx->L, -1)) {
            lua_pushinteger(ctx->L, relX);
            lua_pushinteger(ctx->L, relY);
            if (lua_pcall(ctx->L, 2, 0, 0) != 0) {
                const char* err = lua_tostring(ctx->L, -1);
                printf("[Canvas Click Error] %s\n", err ? err : "Unknown");
                lua_pop(ctx->L, 1);
            }
        } else {
            lua_pop(ctx->L, 1);
        }
    }

    if ((code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) && ctx->L && ctx->touchRef != LUA_NOREF) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        lv_area_t coords;
        lv_obj_get_coords(target, &coords);
        int32_t relX = point.x - coords.x1;
        int32_t relY = point.y - coords.y1;

        const char* eventStr = "pressed";
        if (code == LV_EVENT_PRESSING) eventStr = "drag";
        else if (code == LV_EVENT_RELEASED) eventStr = "released";

        lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, ctx->touchRef);
        if (lua_isfunction(ctx->L, -1)) {
            lua_pushstring(ctx->L, eventStr);
            lua_pushinteger(ctx->L, relX);
            lua_pushinteger(ctx->L, relY);
            if (lua_pcall(ctx->L, 3, 0, 0) != 0) {
                const char* err = lua_tostring(ctx->L, -1);
                printf("[Canvas Touch Error] %s\n", err ? err : "Unknown");
                lua_pop(ctx->L, 1);
            }
        } else {
            lua_pop(ctx->L, 1);
        }
    }
}

static LuaCanvasCtx* get_canvas_ctx(lv_obj_t* canvas) {
    if (!canvas || !lv_obj_is_valid(canvas)) return nullptr;
    return (LuaCanvasCtx*)lv_obj_get_user_data(canvas);
}

int lua_ui_create_canvas(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    uint32_t w = (uint32_t)luaL_optinteger(L, 2, 300);
    uint32_t h = (uint32_t)luaL_optinteger(L, 3, 200);

    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, w, h);

    size_t bufSize = w * h * sizeof(uint16_t);
    uint16_t* buf = (uint16_t*)cbdos::mem::alloc_psram(bufSize);
    if (!buf) {
        buf = (uint16_t*)malloc(bufSize);
    }
    if (!buf) {
        lua_pushnil(L);
        return 1;
    }
    memset(buf, 0, bufSize);

    lv_canvas_set_buffer(canvas, buf, w, h, LV_COLOR_FORMAT_RGB565);

    auto* ctx = new LuaCanvasCtx{ buf, w, h, L, LUA_NOREF, LUA_NOREF };
    lv_obj_set_user_data(canvas, ctx);
    lv_obj_add_event_cb(canvas, canvas_event_cb, LV_EVENT_ALL, ctx);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);

    lua_pushlightuserdata(L, canvas);
    return 1;
}

static int lua_canvas_fill(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    uint32_t hexColor = (uint32_t)luaL_optinteger(L, 2, 0x000000);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    size_t total = ctx->width * ctx->height;
    for (size_t i = 0; i < total; i++) {
        ctx->buffer[i] = c565;
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_set_px(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 4);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    if (x >= 0 && x < (int)ctx->width && y >= 0 && y < (int)ctx->height) {
        ctx->buffer[y * ctx->width + x] = colorToRGB565(hexColor);
        lv_obj_invalidate(canvas);
    }
    return 0;
}

static int lua_canvas_draw_rect(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int rx = (int)luaL_checkinteger(L, 2);
    int ry = (int)luaL_checkinteger(L, 3);
    int rw = (int)luaL_checkinteger(L, 4);
    int rh = (int)luaL_checkinteger(L, 5);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 6);
    bool filled = lua_toboolean(L, 7);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    int x1 = std::max(0, rx);
    int y1 = std::max(0, ry);
    int x2 = std::min((int)ctx->width - 1, rx + rw - 1);
    int y2 = std::min((int)ctx->height - 1, ry + rh - 1);

    if (x1 > x2 || y1 > y2) return 0;

    if (filled) {
        for (int y = y1; y <= y2; y++) {
            uint16_t* row = &ctx->buffer[y * ctx->width];
            for (int x = x1; x <= x2; x++) {
                row[x] = c565;
            }
        }
    } else {
        for (int x = x1; x <= x2; x++) {
            if (ry >= 0 && ry < (int)ctx->height) ctx->buffer[ry * ctx->width + x] = c565;
            if (ry + rh - 1 >= 0 && ry + rh - 1 < (int)ctx->height) ctx->buffer[(ry + rh - 1) * ctx->width + x] = c565;
        }
        for (int y = y1; y <= y2; y++) {
            if (rx >= 0 && rx < (int)ctx->width) ctx->buffer[y * ctx->width + rx] = c565;
            if (rx + rw - 1 >= 0 && rx + rw - 1 < (int)ctx->width) ctx->buffer[y * ctx->width + (rx + rw - 1)] = c565;
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_draw_line(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int x0 = (int)luaL_checkinteger(L, 2);
    int y0 = (int)luaL_checkinteger(L, 3);
    int x1 = (int)luaL_checkinteger(L, 4);
    int y1 = (int)luaL_checkinteger(L, 5);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 6);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        if (x0 >= 0 && x0 < (int)ctx->width && y0 >= 0 && y0 < (int)ctx->height) {
            ctx->buffer[y0 * ctx->width + x0] = c565;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_draw_circle(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int cx = (int)luaL_checkinteger(L, 2);
    int cy = (int)luaL_checkinteger(L, 3);
    int r = (int)luaL_checkinteger(L, 4);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 5);
    bool filled = lua_toboolean(L, 6);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer || r <= 0) return 0;

    uint16_t c565 = colorToRGB565(hexColor);

    auto setPx = [&](int x, int y) {
        if (x >= 0 && x < (int)ctx->width && y >= 0 && y < (int)ctx->height) {
            ctx->buffer[y * ctx->width + x] = c565;
        }
    };

    if (filled) {
        for (int y = -r; y <= r; y++) {
            for (int x = -r; x <= r; x++) {
                if (x * x + y * y <= r * r) {
                    setPx(cx + x, cy + y);
                }
            }
        }
    } else {
        int x = r, y = 0;
        int err = 0;
        while (x >= y) {
            setPx(cx + x, cy + y); setPx(cx + y, cy + x);
            setPx(cx - y, cy + x); setPx(cx - x, cy + y);
            setPx(cx - x, cy - y); setPx(cx - y, cy - x);
            setPx(cx + y, cy - x); setPx(cx + x, cy - y);
            if (err <= 0) { y += 1; err += 2 * y + 1; }
            if (err > 0) { x -= 1; err -= 2 * x + 1; }
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_draw_text(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int startX = (int)luaL_checkinteger(L, 2);
    int startY = (int)luaL_checkinteger(L, 3);
    const char* text = luaL_checkstring(L, 4);
    uint32_t hexColor = (uint32_t)luaL_optinteger(L, 5, 0xFFFFFF);
    int scale = (int)luaL_optinteger(L, 6, 1);
    if (scale < 1) scale = 1;

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer || !text) return 0;

    uint16_t c565 = colorToRGB565(hexColor);
    int curX = startX;

    while (*text) {
        char ch = *text++;
        if (ch >= 32 && ch <= 126) {
            const uint8_t* charData = font5x7[ch - 32];
            for (int col = 0; col < 5; col++) {
                uint8_t line = charData[col];
                for (int row = 0; row < 7; row++) {
                    if (line & (1 << row)) {
                        for (int sx = 0; sx < scale; sx++) {
                            for (int sy = 0; sy < scale; sy++) {
                                int px = curX + col * scale + sx;
                                int py = startY + row * scale + sy;
                                if (px >= 0 && px < (int)ctx->width && py >= 0 && py < (int)ctx->height) {
                                    ctx->buffer[py * ctx->width + px] = c565;
                                }
                            }
                        }
                    }
                }
            }
            curX += 6 * scale;
        } else if (ch == '\n') {
            startY += 8 * scale;
            curX = startX;
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

static int lua_canvas_on_click(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx) return 0;

    if (ctx->clickRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ctx->clickRef);
        ctx->clickRef = LUA_NOREF;
    }

    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        ctx->clickRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

static int lua_canvas_on_touch(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx) return 0;

    if (ctx->touchRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, ctx->touchRef);
        ctx->touchRef = LUA_NOREF;
    }

    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        ctx->touchRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}

static int lua_canvas_get_px(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (x >= 0 && x < (int)ctx->width && y >= 0 && y < (int)ctx->height) {
        uint16_t c565 = ctx->buffer[y * ctx->width + x];
        lua_pushinteger(L, (lua_Integer)rgb565ToColor(c565));
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

static int lua_canvas_flood_fill(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    int startX = (int)luaL_checkinteger(L, 2);
    int startY = (int)luaL_checkinteger(L, 3);
    uint32_t fillHex = (uint32_t)luaL_checkinteger(L, 4);

    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) return 0;

    int w = (int)ctx->width;
    int h = (int)ctx->height;
    if (startX < 0 || startX >= w || startY < 0 || startY >= h) return 0;

    uint16_t target565 = ctx->buffer[startY * w + startX];
    uint16_t fill565 = colorToRGB565(fillHex);
    if (target565 == fill565) return 0;

    std::vector<std::pair<int, int>> queue;
    queue.reserve(1024);
    queue.push_back({startX, startY});
    ctx->buffer[startY * w + startX] = fill565;

    size_t head = 0;
    while (head < queue.size()) {
        auto pt = queue[head++];
        int cx = pt.first;
        int cy = pt.second;

        const int dx[4] = {0, 0, -1, 1};
        const int dy[4] = {-1, 1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                if (ctx->buffer[ny * w + nx] == target565) {
                    ctx->buffer[ny * w + nx] = fill565;
                    queue.push_back({nx, ny});
                }
            }
        }
    }
    lv_obj_invalidate(canvas);
    return 0;
}

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t bfType{0x4D42}; // "BM"
    uint32_t bfSize{0};
    uint16_t bfReserved1{0};
    uint16_t bfReserved2{0};
    uint32_t bfOffBits{54};
    uint32_t biSize{40};
    int32_t biWidth{0};
    int32_t biHeight{0};
    uint16_t biPlanes{1};
    uint16_t biBitCount{24};
    uint32_t biCompression{0};
    uint32_t biSizeImage{0};
    int32_t biXPelsPerMeter{2835};
    int32_t biYPelsPerMeter{2835};
    uint32_t biClrUsed{0};
    uint32_t biClrImportant{0};
};
#pragma pack(pop)

static int lua_canvas_save_bmp(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    const char* path = luaL_checkstring(L, 2);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) {
        lua_pushboolean(L, false);
        return 1;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        lua_pushboolean(L, false);
        return 1;
    }

    int32_t w = (int32_t)ctx->width;
    int32_t h = (int32_t)ctx->height;
    int rowSize = ((w * 3 + 3) / 4) * 4;
    uint32_t imageSize = rowSize * h;

    BMPHeader hdr;
    hdr.bfSize = 54 + imageSize;
    hdr.biWidth = w;
    hdr.biHeight = h;
    hdr.biSizeImage = imageSize;

    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        lua_pushboolean(L, false);
        return 1;
    }

    std::vector<uint8_t> rowBuf(rowSize, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint16_t c565 = ctx->buffer[y * w + x];
            uint8_t r = ((c565 >> 11) & 0x1F) << 3;
            uint8_t g = ((c565 >> 5) & 0x3F) << 2;
            uint8_t b = (c565 & 0x1F) << 3;
            rowBuf[x * 3 + 0] = b;
            rowBuf[x * 3 + 1] = g;
            rowBuf[x * 3 + 2] = r;
        }
        fwrite(rowBuf.data(), 1, rowSize, f);
    }

    fclose(f);
    lua_pushboolean(L, true);
    return 1;
}

static int lua_canvas_load_bmp(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    const char* path = luaL_checkstring(L, 2);
    auto* ctx = get_canvas_ctx(canvas);
    if (!ctx || !ctx->buffer) {
        lua_pushboolean(L, false);
        return 1;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        lua_pushboolean(L, false);
        return 1;
    }

    BMPHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.bfType != 0x4D42) {
        fclose(f);
        lua_pushboolean(L, false);
        return 1;
    }

    int32_t bmpW = hdr.biWidth;
    int32_t bmpH = abs(hdr.biHeight);
    bool flipY = (hdr.biHeight > 0);

    if (hdr.biBitCount == 24) {
        int rowSize = ((bmpW * 3 + 3) / 4) * 4;
        std::vector<uint8_t> rowBuf(rowSize);
        fseek(f, hdr.bfOffBits, SEEK_SET);

        for (int row = 0; row < bmpH; row++) {
            if (fread(rowBuf.data(), 1, rowSize, f) != (size_t)rowSize) break;
            int targetY = flipY ? (bmpH - 1 - row) : row;
            if (targetY >= (int)ctx->height) continue;

            for (int col = 0; col < bmpW && col < (int)ctx->width; col++) {
                uint8_t b = rowBuf[col * 3 + 0];
                uint8_t g = rowBuf[col * 3 + 1];
                uint8_t r = rowBuf[col * 3 + 2];
                uint16_t c565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                ctx->buffer[targetY * ctx->width + col] = c565;
            }
        }
        fclose(f);
        lv_obj_invalidate(canvas);
        lua_pushboolean(L, true);
        return 1;
    }

    fclose(f);
    lua_pushboolean(L, false);
    return 1;
}

static int lua_canvas_refresh(lua_State* L) {
    lv_obj_t* canvas = (lv_obj_t*)lua_touserdata(L, 1);
    if (canvas && lv_obj_is_valid(canvas)) {
        lv_obj_invalidate(canvas);
    }
    return 0;
}


void registerCanvasAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_canvas_fill);
    lua_setfield(L, -2, "fill");
    lua_pushcfunction(L, lua_canvas_set_px);
    lua_setfield(L, -2, "set_px");
    lua_pushcfunction(L, lua_canvas_get_px);
    lua_setfield(L, -2, "get_px");
    lua_pushcfunction(L, lua_canvas_draw_rect);
    lua_setfield(L, -2, "draw_rect");
    lua_pushcfunction(L, lua_canvas_draw_line);
    lua_setfield(L, -2, "draw_line");
    lua_pushcfunction(L, lua_canvas_draw_circle);
    lua_setfield(L, -2, "draw_circle");
    lua_pushcfunction(L, lua_canvas_draw_text);
    lua_setfield(L, -2, "draw_text");
    lua_pushcfunction(L, lua_canvas_flood_fill);
    lua_setfield(L, -2, "flood_fill");
    lua_pushcfunction(L, lua_canvas_on_click);
    lua_setfield(L, -2, "on_click");
    lua_pushcfunction(L, lua_canvas_on_touch);
    lua_setfield(L, -2, "on_touch");
    lua_pushcfunction(L, lua_canvas_refresh);
    lua_setfield(L, -2, "refresh");
    lua_pushcfunction(L, lua_canvas_save_bmp);
    lua_setfield(L, -2, "save_bmp");
    lua_pushcfunction(L, lua_canvas_load_bmp);
    lua_setfield(L, -2, "load_bmp");
    lua_setfield(L, -2, "canvas");
}
