#include "cbdos/vault.hpp"
#include "LuaBridge_Internal.hpp"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// cbdos.vault.* Bindings
// ─────────────────────────────────────────────────────────────────────────────

static int lua_vault_is_unlocked(lua_State* L) {
    lua_pushboolean(L, cbdos::security::VaultService::getInstance().isUnlocked());
    return 1;
}

static int lua_vault_unlock(lua_State* L) {
    const char* pin = luaL_checkstring(L, 1);
    bool ok = cbdos::security::VaultService::getInstance().unlock(pin ? pin : "");
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_vault_lock(lua_State* L) {
    cbdos::security::VaultService::getInstance().lock();
    return 0;
}

static int lua_vault_save(lua_State* L) {
    std::string ns = "default";
    int tableIdx = 1;

    if (lua_isstring(L, 1) && lua_istable(L, 2)) {
        ns = lua_tostring(L, 1);
        tableIdx = 2;
    } else if (!lua_istable(L, 1)) {
        return luaL_error(L, "vault.save espera (item) o (namespace, item)");
    }

    cbdos::security::VaultItem item;

    lua_getfield(L, tableIdx, "id");
    if (lua_isstring(L, -1)) item.id = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "label");
    if (lua_isstring(L, -1)) item.label = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "host");
    if (lua_isstring(L, -1)) item.host = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "port");
    if (lua_isnumber(L, -1)) item.port = (uint16_t)lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "user");
    if (lua_isstring(L, -1)) item.user = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "secret");
    if (lua_isstring(L, -1)) item.secret = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "type");
    if (lua_isstring(L, -1)) item.type = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, tableIdx, "notes");
    if (lua_isstring(L, -1)) item.notes = lua_tostring(L, -1);
    lua_pop(L, 1);

    if (item.id.empty()) {
        return luaL_error(L, "vault.save requiere al menos el campo 'id'");
    }

    bool ok = cbdos::security::VaultService::getInstance().saveItem(ns, item);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_vault_set(lua_State* L) {
    // Modo rapido: vault.set(id, secret) o vault.set(ns, id, secret)
    std::string ns = "default";
    std::string id;
    std::string secret;

    if (lua_gettop(L) >= 3) {
        ns = luaL_checkstring(L, 1);
        id = luaL_checkstring(L, 2);
        secret = luaL_checkstring(L, 3);
    } else {
        id = luaL_checkstring(L, 1);
        secret = luaL_checkstring(L, 2);
    }

    cbdos::security::VaultItem vi;
    vi.id = id;
    vi.label = id;
    vi.secret = secret;

    bool ok = cbdos::security::VaultService::getInstance().saveItem(ns, vi);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_vault_get(lua_State* L) {
    // Modo rapido: vault.get(id) o vault.get(ns, id) -> devuelve secret
    std::string ns = "default";
    std::string id;

    if (lua_gettop(L) >= 2) {
        ns = luaL_checkstring(L, 1);
        id = luaL_checkstring(L, 2);
    } else {
        id = luaL_checkstring(L, 1);
    }

    cbdos::security::VaultItem vi;
    bool ok = cbdos::security::VaultService::getInstance().getItem(ns, id, vi);
    if (ok) {
        lua_pushstring(L, vi.secret.c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int lua_vault_get_item(lua_State* L) {
    std::string ns = "default";
    std::string id;

    if (lua_gettop(L) >= 2) {
        ns = luaL_checkstring(L, 1);
        id = luaL_checkstring(L, 2);
    } else {
        id = luaL_checkstring(L, 1);
    }

    cbdos::security::VaultItem vi;
    bool ok = cbdos::security::VaultService::getInstance().getItem(ns, id, vi);
    if (!ok) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushstring(L, vi.id.c_str());    lua_setfield(L, -2, "id");
    lua_pushstring(L, vi.label.c_str()); lua_setfield(L, -2, "label");
    lua_pushstring(L, vi.host.c_str());  lua_setfield(L, -2, "host");
    lua_pushinteger(L, vi.port);         lua_setfield(L, -2, "port");
    lua_pushstring(L, vi.user.c_str());  lua_setfield(L, -2, "user");
    lua_pushstring(L, vi.secret.c_str());lua_setfield(L, -2, "secret");
    lua_pushstring(L, vi.type.c_str());  lua_setfield(L, -2, "type");
    lua_pushstring(L, vi.notes.c_str()); lua_setfield(L, -2, "notes");
    return 1;
}

static int lua_vault_find_by_host(lua_State* L) {
    std::string ns = "default";
    std::string host;

    if (lua_gettop(L) >= 2) {
        ns = luaL_checkstring(L, 1);
        host = luaL_checkstring(L, 2);
    } else {
        host = luaL_checkstring(L, 1);
    }

    cbdos::security::VaultItem vi;
    bool ok = cbdos::security::VaultService::getInstance().findByHost(ns, host, vi);
    if (!ok) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushstring(L, vi.id.c_str());    lua_setfield(L, -2, "id");
    lua_pushstring(L, vi.label.c_str()); lua_setfield(L, -2, "label");
    lua_pushstring(L, vi.host.c_str());  lua_setfield(L, -2, "host");
    lua_pushinteger(L, vi.port);         lua_setfield(L, -2, "port");
    lua_pushstring(L, vi.user.c_str());  lua_setfield(L, -2, "user");
    lua_pushstring(L, vi.secret.c_str());lua_setfield(L, -2, "secret");
    lua_pushstring(L, vi.type.c_str());  lua_setfield(L, -2, "type");
    lua_pushstring(L, vi.notes.c_str()); lua_setfield(L, -2, "notes");
    return 1;
}

static int lua_vault_delete(lua_State* L) {
    std::string ns = "default";
    std::string id;

    if (lua_gettop(L) >= 2) {
        ns = luaL_checkstring(L, 1);
        id = luaL_checkstring(L, 2);
    } else {
        id = luaL_checkstring(L, 1);
    }

    bool ok = cbdos::security::VaultService::getInstance().deleteItem(ns, id);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_vault_list(lua_State* L) {
    std::string ns = luaL_optstring(L, 1, "default");
    bool includeSecrets = lua_toboolean(L, 2) != 0;

    std::vector<cbdos::security::VaultItem> items;
    cbdos::security::VaultService::getInstance().listItems(ns, items, includeSecrets);

    lua_newtable(L);
    int idx = 1;
    for (const auto& vi : items) {
        lua_newtable(L);
        lua_pushstring(L, vi.id.c_str());    lua_setfield(L, -2, "id");
        lua_pushstring(L, vi.label.c_str()); lua_setfield(L, -2, "label");
        lua_pushstring(L, vi.host.c_str());  lua_setfield(L, -2, "host");
        lua_pushinteger(L, vi.port);         lua_setfield(L, -2, "port");
        lua_pushstring(L, vi.user.c_str());  lua_setfield(L, -2, "user");
        if (includeSecrets) {
            lua_pushstring(L, vi.secret.c_str());
            lua_setfield(L, -2, "secret");
        }
        lua_pushstring(L, vi.type.c_str());  lua_setfield(L, -2, "type");
        lua_pushstring(L, vi.notes.c_str()); lua_setfield(L, -2, "notes");

        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

static int lua_vault_encrypt(lua_State* L) {
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    if (!data || len == 0) {
        lua_pushnil(L);
        return 1;
    }

    std::vector<uint8_t> cipher;
    bool ok = cbdos::security::VaultService::getInstance().encryptData(
        reinterpret_cast<const uint8_t*>(data), len, cipher);
    if (ok) {
        lua_pushlstring(L, reinterpret_cast<const char*>(cipher.data()), cipher.size());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int lua_vault_decrypt(lua_State* L) {
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    if (!data || len < 28) {
        lua_pushnil(L);
        return 1;
    }

    std::vector<uint8_t> plain;
    bool ok = cbdos::security::VaultService::getInstance().decryptData(
        reinterpret_cast<const uint8_t*>(data), len, plain);
    if (ok) {
        lua_pushlstring(L, reinterpret_cast<const char*>(plain.data()), plain.size());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

void registerVaultAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_vault_is_unlocked); lua_setfield(L, -2, "is_unlocked");
    lua_pushcfunction(L, lua_vault_unlock);      lua_setfield(L, -2, "unlock");
    lua_pushcfunction(L, lua_vault_lock);        lua_setfield(L, -2, "lock");
    lua_pushcfunction(L, lua_vault_set);         lua_setfield(L, -2, "set");
    lua_pushcfunction(L, lua_vault_get);         lua_setfield(L, -2, "get");
    lua_pushcfunction(L, lua_vault_save);        lua_setfield(L, -2, "save");
    lua_pushcfunction(L, lua_vault_get_item);    lua_setfield(L, -2, "get_item");
    lua_pushcfunction(L, lua_vault_find_by_host);lua_setfield(L, -2, "find_by_host");
    lua_pushcfunction(L, lua_vault_delete);      lua_setfield(L, -2, "delete");
    lua_pushcfunction(L, lua_vault_list);        lua_setfield(L, -2, "list");
    lua_pushcfunction(L, lua_vault_encrypt);     lua_setfield(L, -2, "encrypt");
    lua_pushcfunction(L, lua_vault_decrypt);     lua_setfield(L, -2, "decrypt");
    lua_setfield(L, -2, "vault");
}
