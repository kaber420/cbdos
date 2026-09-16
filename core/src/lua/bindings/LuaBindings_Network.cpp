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
// SSH Client API (cbdos.ssh.* / ssh.*)
// ─────────────────────────────────────────────────────────────────────────────
static int lua_ssh_connect(lua_State* L) {
    cbdos::ssh::SshConfig cfg;

    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "host");
        if (lua_isstring(L, -1)) cfg.host = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "port");
        cfg.port = (uint16_t)luaL_optinteger(L, -1, 22);
        lua_pop(L, 1);

        lua_getfield(L, 1, "username");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "user");
        }
        if (lua_isstring(L, -1)) cfg.username = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "password");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "pass");
        }
        if (lua_isstring(L, -1)) cfg.password = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "key_path");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "key");
        }
        if (lua_isstring(L, -1)) {
            cfg.privateKeyPath = lua_tostring(L, -1);
            cfg.authType = cbdos::ssh::SshAuthType::PublicKey;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "passphrase");
        if (lua_isstring(L, -1)) cfg.passphrase = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "timeout");
        cfg.timeoutMs = (uint32_t)luaL_optinteger(L, -1, 8000);
        lua_pop(L, 1);
    } else {
        cfg.host = luaL_checkstring(L, 1);
        cfg.username = luaL_optstring(L, 2, "root");
        cfg.password = luaL_optstring(L, 3, "");
        cfg.port = (uint16_t)luaL_optinteger(L, 4, 22);
        cfg.timeoutMs = (uint32_t)luaL_optinteger(L, 5, 8000);
    }

    std::string errMsg;
    bool ok = cbdos::ssh::connect(cfg, [&errMsg](cbdos::ssh::SshSessionState state, const std::string& msg) {
        if (state == cbdos::ssh::SshSessionState::ErrorAuthFailed ||
            state == cbdos::ssh::SshSessionState::ErrorSocket ||
            state == cbdos::ssh::SshSessionState::ErrorTimeout) {
            errMsg = msg;
        }
    });

    lua_pushboolean(L, ok);
    if (!ok) {
        lua_pushstring(L, errMsg.empty() ? "Fallo al conectar SSH" : errMsg.c_str());
        return 2;
    }
    return 1;
}

static int lua_ssh_disconnect(lua_State* L) {
    cbdos::ssh::disconnect();
    return 0;
}

static int lua_ssh_is_connected(lua_State* L) {
    lua_pushboolean(L, cbdos::ssh::isConnected());
    return 1;
}

static int lua_ssh_exec(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    uint32_t timeoutMs = (uint32_t)luaL_optinteger(L, 2, 10000);

    cbdos::ssh::SshExecResult res = cbdos::ssh::execute(cmd, timeoutMs);

    if (!res.success && res.stdOut.empty() && !res.errorMessage.empty()) {
        lua_pushnil(L);
        lua_pushstring(L, res.errorMessage.c_str());
        lua_pushinteger(L, res.exitCode);
        return 3;
    }

    lua_pushstring(L, res.stdOut.c_str());
    lua_pushinteger(L, res.exitCode);
    lua_pushstring(L, res.stdErr.c_str());
    return 3;
}

static int lua_ssh_write(lua_State* L) {
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    bool ok = cbdos::ssh::sendInput(reinterpret_cast<const uint8_t*>(data), len);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_ssh_close_shell(lua_State* L) {
    cbdos::ssh::closeShell();
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Net API (LAN Recon v2): ping ICMP real + sonda TCP + sweep async.
// ─────────────────────────────────────────────────────────────────────────────
static int lua_net_ping(lua_State* L) {
    const char* ip = luaL_checkstring(L, 1);
    uint32_t timeoutMs = (uint32_t)luaL_optinteger(L, 2, 1000);
    uint32_t rtt = 0;
    std::string method;
    bool ok = cbdos::network::LanScannerService::getInstance().pingSingle(
        ip ? ip : "", timeoutMs, &rtt, &method);
    lua_pushboolean(L, ok);
    lua_pushinteger(L, (lua_Integer)rtt);
    lua_pushstring(L, method.c_str());
    return 3;
}

static int lua_net_probe_port(lua_State* L) {
    const char* ip = luaL_checkstring(L, 1);
    int port = (int)luaL_checkinteger(L, 2);
    uint32_t timeoutMs = (uint32_t)luaL_optinteger(L, 3, 300);
    bool open = false;
    auto* backend = cbdos::network::getLanScannerBackend();
    if (backend && ip && port > 0 && port < 65536) {
        open = backend->probeTcpPort(ip, (uint16_t)port, timeoutMs);
    }
    lua_pushboolean(L, open);
    return 1;
}

static int lua_net_scan_start(lua_State* L) {
    const char* cidr = luaL_optstring(L, 1, "");
    bool ok = false;
    if (cidr && cidr[0] != '\0') {
        ok = cbdos::network::LanScannerService::getInstance().startScanCidr(cidr);
    } else {
        ok = cbdos::network::LanScannerService::getInstance().startScan();
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_net_scan_stop(lua_State* L) {
    (void)L;
    cbdos::network::LanScannerService::getInstance().stopScan();
    return 0;
}

static int lua_net_scanning(lua_State* L) {
    lua_pushboolean(L, cbdos::network::LanScannerService::getInstance().isScanning());
    return 1;
}

static int lua_net_results(lua_State* L) {
    std::vector<cbdos::network::LanHostInfo> hosts =
        cbdos::network::LanScannerService::getInstance().getResults();
    lua_newtable(L);
    int idx = 1;
    for (const auto& h : hosts) {
        lua_newtable(L);
        lua_pushstring(L, h.ip.c_str());
        lua_setfield(L, -2, "ip");
        lua_pushstring(L, h.getMacString().c_str());
        lua_setfield(L, -2, "mac");
        lua_pushstring(L, h.vendor.c_str());
        lua_setfield(L, -2, "vendor");
        lua_pushstring(L, h.banner.c_str());
        lua_setfield(L, -2, "banner");
        lua_pushstring(L, h.discovery.c_str());
        lua_setfield(L, -2, "discovery");
        lua_pushstring(L, h.ssdpServer.c_str());
        lua_setfield(L, -2, "server");
        lua_pushinteger(L, (lua_Integer)h.rttMs);
        lua_setfield(L, -2, "rtt");
        lua_pushboolean(L, h.isTv);
        lua_setfield(L, -2, "is_tv");
        lua_newtable(L);
        int pidx = 1;
        for (size_t i = 0; i < cbdos::network::kLanReconPortCount; ++i) {
            uint16_t port = cbdos::network::kLanReconPorts[i];
            if (h.hasPort(port)) {
                lua_pushinteger(L, (lua_Integer)port);
                lua_rawseti(L, -2, pidx++);
            }
        }
        lua_setfield(L, -2, "ports");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

void registerSshAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_ssh_connect);
    lua_setfield(L, -2, "connect");
    lua_pushcfunction(L, lua_ssh_disconnect);
    lua_setfield(L, -2, "disconnect");
    lua_pushcfunction(L, lua_ssh_disconnect);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, lua_ssh_is_connected);
    lua_setfield(L, -2, "is_connected");
    lua_pushcfunction(L, lua_ssh_exec);
    lua_setfield(L, -2, "exec");
    lua_pushcfunction(L, lua_ssh_exec);
    lua_setfield(L, -2, "execute");
    lua_pushcfunction(L, lua_ssh_write);
    lua_setfield(L, -2, "write");
    lua_pushcfunction(L, lua_ssh_write);
    lua_setfield(L, -2, "send");
    lua_pushcfunction(L, lua_ssh_close_shell);
    lua_setfield(L, -2, "close_shell");
    lua_setfield(L, -2, "ssh");
}

void registerNetAPI(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, lua_net_ping);
    lua_setfield(L, -2, "ping");
    lua_pushcfunction(L, lua_net_probe_port);
    lua_setfield(L, -2, "probe_port");
    lua_pushcfunction(L, lua_net_scan_start);
    lua_setfield(L, -2, "scan_start");
    lua_pushcfunction(L, lua_net_scan_stop);
    lua_setfield(L, -2, "scan_stop");
    lua_pushcfunction(L, lua_net_scanning);
    lua_setfield(L, -2, "scanning");
    lua_pushcfunction(L, lua_net_results);
    lua_setfield(L, -2, "results");
    lua_setfield(L, -2, "net");
}
