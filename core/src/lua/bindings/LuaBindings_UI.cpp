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
// LVGL 9.5 UI API (cbdos.ui.*)
// ─────────────────────────────────────────────────────────────────────────────

struct LuaUIEventData {
    lua_State* L;
    int fnRef;
};

static void lua_ui_generic_event_cb(lv_event_t* e) {
    auto* data = static_cast<LuaUIEventData*>(lv_event_get_user_data(e));
    if (!data || !data->L) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_DELETE) {
        if (data->fnRef != LUA_NOREF) {
            luaL_unref(data->L, LUA_REGISTRYINDEX, data->fnRef);
        }
        delete data;
        return;
    }

    if (data->fnRef == LUA_NOREF) return;

    lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->fnRef);
    if (!lua_isfunction(data->L, -1)) {
        lua_pop(data->L, 1);
        return;
    }

    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    int nargs = 0;

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_check_type(target, &lv_slider_class)) {
            lua_pushinteger(data->L, lv_slider_get_value(target));
            nargs = 1;
        } else if (lv_obj_check_type(target, &lv_switch_class)) {
            lua_pushboolean(data->L, lv_obj_has_state(target, LV_STATE_CHECKED));
            nargs = 1;
        }
    }

    if (lua_pcall(data->L, nargs, 0, 0) != 0) {
        const char* err = lua_tostring(data->L, -1);
        printf("[LuaUI Error] %s\n", err ? err : "Error desconocido");
        lua_pop(data->L, 1);
    }
}

static inline int32_t parse_coord(lua_State* L, int argIdx, int32_t defaultVal) {
    if (lua_isnoneornil(L, argIdx)) return defaultVal;
    int32_t val = (int32_t)luaL_checkinteger(L, argIdx);
    if (val <= 0 || val == -1) return defaultVal;
    return val;
}

lv_obj_t* get_target_parent(lua_State* L, int argIdx) {
    if (lua_islightuserdata(L, argIdx)) {
        lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, argIdx);
        if (obj && lv_obj_is_valid(obj)) return obj;
    }
    return lv_screen_active();
}

static int lua_ui_create_card(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    int32_t w = parse_coord(L, 2, LV_PCT(100));
    int32_t h = parse_coord(L, 3, LV_SIZE_CONTENT);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    DefaultTheme::applyRaisedCard(card, 16);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lua_pushlightuserdata(L, card);
    return 1;
}

static int lua_ui_create_sunken_card(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    int32_t w = parse_coord(L, 2, LV_PCT(100));
    int32_t h = parse_coord(L, 3, LV_SIZE_CONTENT);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    DefaultTheme::applySunkenCard(card, 16);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lua_pushlightuserdata(L, card);
    return 1;
}

static int lua_ui_create_label(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    const char* text = luaL_optstring(L, 2, "");

    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    lua_pushlightuserdata(L, lbl);
    return 1;
}

static int lua_ui_set_text(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        lv_label_set_text(obj, text);
    }
    return 0;
}

static int lua_ui_set_color(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    uint32_t hexColor = (uint32_t)luaL_checkinteger(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        lv_obj_set_style_text_color(obj, lv_color_hex(hexColor), 0);
    }
    return 0;
}

static int lua_ui_set_font_size(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int size = (int)luaL_checkinteger(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        if (size >= 24) {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, 0);
        } else if (size >= 16) {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, 0);
        } else if (size >= 14) {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, 0);
        } else {
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, 0);
        }
    }
    return 0;
}

static int lua_ui_create_button(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    const char* text = luaL_optstring(L, 2, "Boton");

    lv_obj_t* btn = lv_button_create(parent);
    DefaultTheme::applyButton(btn, 16);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_pad_hor(btn, 16, 0);
    lv_obj_set_style_pad_ver(btn, 8, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, DefaultTheme::getTextColor(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(btn, lua_ui_generic_event_cb, LV_EVENT_CLICKED, data);
        lv_obj_add_event_cb(btn, lua_ui_generic_event_cb, LV_EVENT_DELETE, data);
    }

    lua_pushlightuserdata(L, btn);
    return 1;
}

static int lua_ui_create_slider(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    int32_t minVal = (int32_t)luaL_optinteger(L, 2, 0);
    int32_t maxVal = (int32_t)luaL_optinteger(L, 3, 100);
    int32_t val = (int32_t)luaL_optinteger(L, 4, 50);

    lv_obj_t* slider = lv_slider_create(parent);
    lv_slider_set_range(slider, minVal, maxVal);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    lv_obj_set_size(slider, LV_PCT(100), 16);

    if (lua_isfunction(L, 5)) {
        lua_pushvalue(L, 5);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(slider, lua_ui_generic_event_cb, LV_EVENT_VALUE_CHANGED, data);
        lv_obj_add_event_cb(slider, lua_ui_generic_event_cb, LV_EVENT_DELETE, data);
    }

    lua_pushlightuserdata(L, slider);
    return 1;
}

static int lua_ui_create_switch(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    bool state = lua_toboolean(L, 2);

    lv_obj_t* sw = lv_switch_create(parent);
    if (state) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(sw, lua_ui_generic_event_cb, LV_EVENT_VALUE_CHANGED, data);
        lv_obj_add_event_cb(sw, lua_ui_generic_event_cb, LV_EVENT_DELETE, data);
    }

    lua_pushlightuserdata(L, sw);
    return 1;
}

static int lua_ui_create_row(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lua_pushlightuserdata(L, row);
    return 1;
}

static int lua_ui_create_column(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_set_size(col, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 4, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 8, 0);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lua_pushlightuserdata(L, col);
    return 1;
}

static int lua_ui_create_dropdown(lua_State* L) {
    lv_obj_t* parent = get_target_parent(L, 1);
    const char* options = luaL_checkstring(L, 2);

    lv_obj_t* dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, LV_SIZE_CONTENT, 36);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(dd, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(dd, 1, 0);
    lv_obj_set_style_radius(dd, 6, 0);
    lv_obj_set_style_pad_hor(dd, 10, 0);
    lv_obj_set_style_pad_ver(dd, 6, 0);
    lv_obj_set_style_text_color(dd, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, 0);

    if (lua_isfunction(L, 3)) {
        lua_pushvalue(L, 3);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto* data = new LuaUIEventData{ L, fnRef };
        lv_obj_add_event_cb(dd, [](lv_event_t* e) {
            auto* d = static_cast<LuaUIEventData*>(lv_event_get_user_data(e));
            if (!d) return;
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_DELETE) {
                if (d->L && d->fnRef != LUA_NOREF) {
                    luaL_unref(d->L, LUA_REGISTRYINDEX, d->fnRef);
                }
                delete d;
                return;
            }
            if (code == LV_EVENT_VALUE_CHANGED && d->L && d->fnRef != LUA_NOREF) {
                lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
                uint32_t sel = lv_dropdown_get_selected(target);
                char buf[64];
                lv_dropdown_get_selected_str(target, buf, sizeof(buf));

                lua_rawgeti(d->L, LUA_REGISTRYINDEX, d->fnRef);
                if (lua_isfunction(d->L, -1)) {
                    lua_pushinteger(d->L, sel);
                    lua_pushstring(d->L, buf);
                    if (lua_pcall(d->L, 2, 0, 0) != 0) {
                        const char* err = lua_tostring(d->L, -1);
                        printf("[Dropdown Event Error] %s\n", err ? err : "Unknown");
                        lua_pop(d->L, 1);
                    }
                } else {
                    lua_pop(d->L, 1);
                }
            }
        }, LV_EVENT_ALL, data);
    }

    lua_pushlightuserdata(L, dd);
    return 1;
}

static int lua_ui_get_selected(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    if (obj && lv_obj_is_valid(obj)) {
        lua_pushinteger(L, lv_dropdown_get_selected(obj));
        return 1;
    }
    lua_pushinteger(L, 0);
    return 1;
}

static int lua_ui_set_selected(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    uint32_t idx = (uint32_t)luaL_checkinteger(L, 2);
    if (obj && lv_obj_is_valid(obj)) {
        lv_dropdown_set_selected(obj, idx);
    }
    return 0;
}

static int lua_ui_set_size(lua_State* L) {
    lv_obj_t* obj = (lv_obj_t*)lua_touserdata(L, 1);
    int32_t w = (int32_t)luaL_checkinteger(L, 2);
    int32_t h = (int32_t)luaL_checkinteger(L, 3);
    if (obj && lv_obj_is_valid(obj)) {
        lv_obj_set_size(obj, w, h);
    }
    return 0;
}

static int lua_ui_show_toast(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    cbdos::ui::UIManager::showToast(msg);
    return 0;
}


void registerUIAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_ui_create_card);
    lua_setfield(L, -2, "create_card");
    lua_pushcfunction(L, lua_ui_create_sunken_card);
    lua_setfield(L, -2, "create_sunken_card");
    lua_pushcfunction(L, lua_ui_create_label);
    lua_setfield(L, -2, "create_label");
    lua_pushcfunction(L, lua_ui_set_text);
    lua_setfield(L, -2, "set_text");
    lua_pushcfunction(L, lua_ui_set_color);
    lua_setfield(L, -2, "set_color");
    lua_pushcfunction(L, lua_ui_set_font_size);
    lua_setfield(L, -2, "set_font_size");
    lua_pushcfunction(L, lua_ui_create_button);
    lua_setfield(L, -2, "create_button");
    lua_pushcfunction(L, lua_ui_create_dropdown);
    lua_setfield(L, -2, "create_dropdown");
    lua_pushcfunction(L, lua_ui_get_selected);
    lua_setfield(L, -2, "get_selected");
    lua_pushcfunction(L, lua_ui_set_selected);
    lua_setfield(L, -2, "set_selected");
    lua_pushcfunction(L, lua_ui_create_slider);
    lua_setfield(L, -2, "create_slider");
    lua_pushcfunction(L, lua_ui_create_switch);
    lua_setfield(L, -2, "create_switch");
    lua_pushcfunction(L, lua_ui_create_row);
    lua_setfield(L, -2, "create_row");
    lua_pushcfunction(L, lua_ui_create_column);
    lua_setfield(L, -2, "create_column");
    lua_pushcfunction(L, lua_ui_create_canvas);
    lua_setfield(L, -2, "create_canvas");
    lua_pushcfunction(L, lua_ui_set_size);
    lua_setfield(L, -2, "set_size");
    lua_pushcfunction(L, lua_ui_show_toast);
    lua_setfield(L, -2, "show_toast");
    lua_setfield(L, -2, "ui");
}
