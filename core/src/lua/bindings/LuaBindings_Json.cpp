#include "LuaBridge_Internal.hpp"
#include "../cJSON.h"
#include "cbdos/memory.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// cbdos.json.* Bindings
// ─────────────────────────────────────────────────────────────────────────────

// Inicializador de hooks de cJSON con el allocator unificado del sistema
static void ensure_cjson_hooks() {
    static bool hooks_set = false;
    if (!hooks_set) {
        cJSON_Hooks hooks;
        hooks.malloc_fn = [](size_t sz) -> void* {
            void* p = nullptr;
            if (sz < 256) {
                p = cbdos::mem::alloc_internal(sz);
            }
            if (!p) {
                p = cbdos::mem::alloc_psram(sz);
            }
            if (!p) {
                p = malloc(sz);
            }
            return p;
        };
        hooks.free_fn = [](void* ptr) {
            if (ptr) {
                cbdos::mem::free_mem(ptr);
            }
        };
        cJSON_InitHooks(&hooks);
        hooks_set = true;
    }
}

// Convierte un nodo cJSON recursivamente en objetos de la pila de Lua
static void push_cjson_to_lua(lua_State* L, const cJSON* item) {
    if (!item) {
        lua_pushnil(L);
        return;
    }

    switch (item->type & 0xFF) {
        case cJSON_False:
            lua_pushboolean(L, 0);
            break;
        case cJSON_True:
            lua_pushboolean(L, 1);
            break;
        case cJSON_NULL:
            lua_pushnil(L);
            break;
        case cJSON_Number: {
            double d = item->valuedouble;
            if (std::floor(d) == d && d >= -2147483648.0 && d <= 2147483647.0) {
                lua_pushinteger(L, (lua_Integer)item->valueint);
            } else {
                lua_pushnumber(L, (lua_Number)d);
            }
            break;
        }
        case cJSON_String:
        case cJSON_Raw:
            lua_pushstring(L, item->valuestring ? item->valuestring : "");
            break;
        case cJSON_Array: {
            lua_newtable(L);
            int idx = 1;
            cJSON* child = item->child;
            while (child) {
                push_cjson_to_lua(L, child);
                lua_rawseti(L, -2, idx++);
                child = child->next;
            }
            break;
        }
        case cJSON_Object: {
            lua_newtable(L);
            cJSON* child = item->child;
            while (child) {
                if (child->string) {
                    push_cjson_to_lua(L, child);
                    lua_setfield(L, -2, child->string);
                }
                child = child->next;
            }
            break;
        }
        default:
            lua_pushnil(L);
            break;
    }
}

// Convierte un valor de la pila de Lua recursivamente a cJSON*
static cJSON* lua_value_to_cjson(lua_State* L, int index) {
    int type = lua_type(L, index);
    switch (type) {
        case LUA_TNIL:
            return cJSON_CreateNull();
        case LUA_TBOOLEAN:
            return cJSON_CreateBool(lua_toboolean(L, index));
        case LUA_TNUMBER: {
            lua_Number num = lua_tonumber(L, index);
            return cJSON_CreateNumber((double)num);
        }
        case LUA_TSTRING:
            return cJSON_CreateString(lua_tostring(L, index));
        case LUA_TTABLE: {
            // Determinar si es un array secuencial
            bool is_array = true;
            size_t len = lua_rawlen(L, index);
            if (len == 0) {
                // Puede ser una tabla vacía o un objeto vacío
                lua_pushnil(L);
                if (lua_next(L, index < 0 ? index - 1 : index) != 0) {
                    // Tiene al menos un elemento, no es array si no comienza en 1
                    is_array = false;
                    lua_pop(L, 2);
                }
            }

            if (is_array && len > 0) {
                cJSON* arr = cJSON_CreateArray();
                for (size_t i = 1; i <= len; ++i) {
                    lua_rawgeti(L, index, i);
                    cJSON* child = lua_value_to_cjson(L, -1);
                    lua_pop(L, 1);
                    if (child) {
                        cJSON_AddItemToArray(arr, child);
                    }
                }
                return arr;
            } else {
                cJSON* obj = cJSON_CreateObject();
                int abs_index = (index < 0) ? lua_gettop(L) + index + 1 : index;
                lua_pushnil(L);
                while (lua_next(L, abs_index) != 0) {
                    // key en -2, value en -1
                    if (lua_type(L, -2) == LUA_TSTRING) {
                        const char* key = lua_tostring(L, -2);
                        cJSON* child = lua_value_to_cjson(L, -1);
                        if (child) {
                            cJSON_AddItemToObject(obj, key, child);
                        }
                    } else if (lua_type(L, -2) == LUA_TNUMBER) {
                        char key_buf[32];
                        snprintf(key_buf, sizeof(key_buf), "%lld", (long long)lua_tointeger(L, -2));
                        cJSON* child = lua_value_to_cjson(L, -1);
                        if (child) {
                            cJSON_AddItemToObject(obj, key_buf, child);
                        }
                    }
                    lua_pop(L, 1);
                }
                return obj;
            }
        }
        default:
            return cJSON_CreateNull();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// cbdos.json.decode(json_string) -> table / nil, error_str
// ─────────────────────────────────────────────────────────────────────────────
static int lua_json_decode(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "Se requiere un string JSON como argumento");
        return 2;
    }

    ensure_cjson_hooks();

    size_t len = 0;
    const char* json_str = lua_tolstring(L, 1, &len);
    if (!json_str || len == 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Cadena JSON vacia");
        return 2;
    }

    const char* parse_end = nullptr;
    cJSON* root = cJSON_ParseWithOpts(json_str, &parse_end, 0);
    if (!root) {
        const char* err = cJSON_GetErrorPtr();
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Error de parseo JSON cerca de: %.32s", err ? err : "desconocido");
        lua_pushnil(L);
        lua_pushstring(L, err_msg);
        return 2;
    }

    push_cjson_to_lua(L, root);
    cJSON_Delete(root);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// cbdos.json.encode(value) -> json_string / nil, error_str
// ─────────────────────────────────────────────────────────────────────────────
static int lua_json_encode(lua_State* L) {
    if (lua_gettop(L) < 1) {
        lua_pushnil(L);
        lua_pushstring(L, "Se requiere al menos un argumento");
        return 2;
    }

    ensure_cjson_hooks();

    cJSON* root = lua_value_to_cjson(L, 1);
    if (!root) {
        lua_pushnil(L);
        lua_pushstring(L, "Error al serializar valor a JSON");
        return 2;
    }

    char* rendered = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!rendered) {
        lua_pushnil(L);
        lua_pushstring(L, "Error al generar string JSON (sin memoria)");
        return 2;
    }

    lua_pushstring(L, rendered);
    cbdos::mem::free_mem(rendered);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registro de la API
// ─────────────────────────────────────────────────────────────────────────────
void registerJsonAPI(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_json_decode);
    lua_setfield(L, -2, "decode");

    lua_pushcfunction(L, lua_json_encode);
    lua_setfield(L, -2, "encode");

    lua_setfield(L, -2, "json");
}
