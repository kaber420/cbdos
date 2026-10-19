#include "LuaBridge_Internal.hpp"
#include "cbdos/ble.hpp"
#include "cbdos/ble_types.hpp"
#include "../../security/BleShieldService.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers de Parseo y Conversión
// ─────────────────────────────────────────────────────────────────────────────

inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseHexString(const char* hex, uint8_t* out, size_t maxLen, size_t* outLen) {
    if (!hex || !out || !outLen) return false;
    size_t count = 0;
    int highNibble = -1;
    for (size_t i = 0; hex[i] != '\0'; ++i) {
        char c = hex[i];
        if (c == ' ' || c == ':' || c == '-' || c == ',' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        int nib = hexNibble(c);
        if (nib < 0) return false;
        if (highNibble < 0) {
            highNibble = nib;
        } else {
            if (count >= maxLen) return false;
            out[count++] = static_cast<uint8_t>((highNibble << 4) | nib);
            highNibble = -1;
        }
    }
    if (highNibble != -1) return false;
    *outLen = count;
    return true;
}

bool parseMacAddress(const char* macStr, uint8_t outMac[6]) {
    size_t outLen = 0;
    return parseHexString(macStr, outMac, 6, &outLen) && (outLen == 6);
}

std::string bytesToHexString(const uint8_t* data, size_t len) {
    static const char hexChars[] = "0123456789ABCDEF";
    std::string res;
    res.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        res.push_back(hexChars[(data[i] >> 4) & 0x0F]);
        res.push_back(hexChars[data[i] & 0x0F]);
    }
    return res;
}

const char* stateToString(cbdos::ble::BleAdapterState st) {
    switch (st) {
        case cbdos::ble::BleAdapterState::Idle: return "idle";
        case cbdos::ble::BleAdapterState::Scanning: return "scanning";
        case cbdos::ble::BleAdapterState::Advertising: return "advertising";
        case cbdos::ble::BleAdapterState::Unavailable:
        default:
            return "unavailable";
    }
}

const char* spamTypeToString(cbdos::radio::BleSpamType type) {
    switch (type) {
        case cbdos::radio::BleSpamType::AppleJuice: return "apple_juice";
        case cbdos::radio::BleSpamType::AndroidFastPair: return "android_fast_pair";
        case cbdos::radio::BleSpamType::WindowsSwiftPair: return "windows_swift_pair";
        case cbdos::radio::BleSpamType::SamsungBuds: return "samsung_buds";
        case cbdos::radio::BleSpamType::FlipperSpam: return "flipper_spam";
        case cbdos::radio::BleSpamType::None:
        default:
            return "none";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 3.1. Estado y Control del Adaptador (cbdos.ble.* / ble.*)
// ─────────────────────────────────────────────────────────────────────────────

int lua_ble_available(lua_State* L) {
    lua_pushboolean(L, cbdos::ble::isAvailable());
    return 1;
}

int lua_ble_init(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "CBDos");
    auto* adapter = cbdos::ble::getAdapter();
    if (!adapter) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "No adapter registered");
        return 2;
    }
    if (!adapter->init(name)) {
        lua_pushboolean(L, false);
        const char* err = adapter->getLastError();
        lua_pushstring(L, (err && err[0]) ? err : "Init failed");
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

int lua_ble_shutdown(lua_State* L) {
    auto* adapter = cbdos::ble::getAdapter();
    if (adapter) {
        adapter->shutdown();
    }
    return 0;
}

int lua_ble_state(lua_State* L) {
    lua_pushstring(L, stateToString(cbdos::ble::getState()));
    return 1;
}

int lua_ble_set_tx_power(lua_State* L) {
    int dbm = static_cast<int>(luaL_checkinteger(L, 1));
    auto* adapter = cbdos::ble::getAdapter();
    if (!adapter) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool ok = adapter->setTxPower(static_cast<int8_t>(dbm));
    lua_pushboolean(L, ok);
    return 1;
}

int lua_ble_last_error(lua_State* L) {
    auto* adapter = cbdos::ble::getAdapter();
    lua_pushstring(L, adapter ? adapter->getLastError() : "No adapter");
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3.2. Emisión de Anuncios GAP (Advertising)
// ─────────────────────────────────────────────────────────────────────────────

int lua_ble_adv_start(lua_State* L) {
    cbdos::ble::BleAdvParams params;
    uint8_t advBuf[31] = {0};
    size_t advLen = 0;
    uint8_t scanRspBuf[31] = {0};
    size_t scanRspLen = 0;

    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "raw");
        bool isRaw = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 1, "data");
        if (lua_isstring(L, -1)) {
            size_t strLen = 0;
            const char* str = lua_tolstring(L, -1, &strLen);
            if (isRaw) {
                if (strLen > 31) {
                    lua_pushboolean(L, false);
                    lua_pushstring(L, "Payload exceeds 31 bytes");
                    return 2;
                }
                memcpy(advBuf, str, strLen);
                advLen = strLen;
            } else {
                if (!parseHexString(str, advBuf, sizeof(advBuf), &advLen)) {
                    lua_pushboolean(L, false);
                    lua_pushstring(L, "Invalid hex data or exceeds 31 bytes");
                    return 2;
                }
            }
        }
        lua_pop(L, 1);

        // Si no se definió 'data', construir AD con 'name'
        if (advLen == 0) {
            lua_getfield(L, 1, "name");
            if (lua_isstring(L, -1)) {
                size_t nameLen = 0;
                const char* nameStr = lua_tolstring(L, -1, &nameLen);
                if (nameLen > 26) nameLen = 26; // 31 - 3 (flags) - 2 (len + type) = 26
                advBuf[0] = 0x02; // Len
                advBuf[1] = 0x01; // Flags AD type
                advBuf[2] = 0x06; // LE General Discoverable | BR/EDR Not Supported
                advBuf[3] = static_cast<uint8_t>(nameLen + 1);
                advBuf[4] = 0x09; // Complete Local Name AD type
                memcpy(&advBuf[5], nameStr, nameLen);
                advLen = 5 + nameLen;
            }
            lua_pop(L, 1);
        }

        // scan_rsp
        lua_getfield(L, 1, "scan_rsp");
        if (lua_isstring(L, -1)) {
            const char* rspStr = lua_tostring(L, -1);
            if (!parseHexString(rspStr, scanRspBuf, sizeof(scanRspBuf), &scanRspLen)) {
                lua_pushboolean(L, false);
                lua_pushstring(L, "Invalid scan_rsp hex or exceeds 31 bytes");
                return 2;
            }
        }
        lua_pop(L, 1);

        // interval
        lua_getfield(L, 1, "interval");
        if (lua_isinteger(L, -1)) {
            int interval = static_cast<int>(lua_tointeger(L, -1));
            if (interval < 20) interval = 20;
            if (interval > 10240) interval = 10240;
            params.intervalMs = static_cast<uint16_t>(interval);
        } else {
            params.intervalMs = 100;
        }
        lua_pop(L, 1);

        // tx_power
        lua_getfield(L, 1, "tx_power");
        if (lua_isinteger(L, -1)) {
            params.txPowerDbm = static_cast<int8_t>(lua_tointeger(L, -1));
        } else {
            params.txPowerDbm = 127;
        }
        lua_pop(L, 1);

        // scannable
        lua_getfield(L, 1, "scannable");
        if (lua_isboolean(L, -1)) {
            params.scannable = lua_toboolean(L, -1);
        } else {
            params.scannable = (scanRspLen > 0);
        }
        lua_pop(L, 1);

        // random_mac
        lua_getfield(L, 1, "random_mac");
        bool randomMac = lua_toboolean(L, -1);
        lua_pop(L, 1);
        params.macRotation = randomMac ? cbdos::ble::BleMacRotationMode::EveryPacket : cbdos::ble::BleMacRotationMode::Off;

        // mac
        lua_getfield(L, 1, "mac");
        if (lua_isstring(L, -1)) {
            if (parseMacAddress(lua_tostring(L, -1), params.customMac)) {
                params.hasCustomMac = true;
                params.macRotation = cbdos::ble::BleMacRotationMode::Off;
            }
        }
        lua_pop(L, 1);

        // is_apple
        lua_getfield(L, 1, "is_apple");
        if (lua_isboolean(L, -1)) {
            params.isApple = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    } else if (lua_isstring(L, 1)) {
        // Modo rápido posicional: ble.adv_start(hex, [scan_rsp_hex], [interval_ms])
        const char* hexStr = lua_tostring(L, 1);
        if (!parseHexString(hexStr, advBuf, sizeof(advBuf), &advLen)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Invalid hex data or exceeds 31 bytes");
            return 2;
        }

        if (lua_isstring(L, 2)) {
            const char* rspStr = lua_tostring(L, 2);
            if (!parseHexString(rspStr, scanRspBuf, sizeof(scanRspBuf), &scanRspLen)) {
                lua_pushboolean(L, false);
                lua_pushstring(L, "Invalid scan_rsp hex or exceeds 31 bytes");
                return 2;
            }
        }

        int interval = static_cast<int>(luaL_optinteger(L, 3, 100));
        if (interval < 20) interval = 20;
        if (interval > 10240) interval = 10240;
        params.intervalMs = static_cast<uint16_t>(interval);
        params.txPowerDbm = 127;
        params.scannable = (scanRspLen > 0);
        params.macRotation = cbdos::ble::BleMacRotationMode::Off;
    } else {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Table or hex string expected");
        return 2;
    }

    if (advLen == 0) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "No advertising data specified");
        return 2;
    }

    params.data = advBuf;
    params.len = advLen;
    if (scanRspLen > 0) {
        params.scanRspData = scanRspBuf;
        params.scanRspLen = scanRspLen;
    }

    // Detener escaneo si estaba activo para arbitraje de radio única
    if (cbdos::security::BleShieldService::getInstance().isScanning()) {
        cbdos::security::BleShieldService::getInstance().stopScan();
    }

    auto* adapter = cbdos::ble::getAdapter();
    if (!adapter) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "No adapter registered");
        return 2;
    }

    if (!adapter->isAvailable()) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "BLE hardware not available");
        return 2;
    }

    if (!adapter->startAdvertising(params)) {
        lua_pushboolean(L, false);
        const char* err = adapter->getLastError();
        lua_pushstring(L, (err && err[0]) ? err : "Failed to start advertising");
        return 2;
    }

    lua_pushboolean(L, true);
    return 1;
}

int lua_ble_adv_stop(lua_State* L) {
    auto* adapter = cbdos::ble::getAdapter();
    bool ok = adapter ? adapter->stopAdvertising() : false;
    lua_pushboolean(L, ok);
    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3.3. Escaneo Pasivo/Activo y Telemetría (BleShieldService)
// ─────────────────────────────────────────────────────────────────────────────

int lua_ble_scan_start(lua_State* L) {
    auto& svc = cbdos::security::BleShieldService::getInstance();
    svc.init();
    svc.startScan();
    lua_pushboolean(L, svc.isScanning());
    return 1;
}

int lua_ble_scan_stop(lua_State* L) {
    cbdos::security::BleShieldService::getInstance().stopScan();
    return 0;
}

int lua_ble_scanning(lua_State* L) {
    lua_pushboolean(L, cbdos::security::BleShieldService::getInstance().isScanning());
    return 1;
}

int lua_ble_scan_clear(lua_State* L) {
    cbdos::security::BleShieldService::getInstance().clearDevices();
    return 0;
}

int lua_ble_scan_count(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(cbdos::security::BleShieldService::getInstance().getDeviceCount()));
    return 1;
}

int lua_ble_results(lua_State* L) {
    auto devices = cbdos::security::BleShieldService::getInstance().getDevices();
    lua_newtable(L);
    int idx = 1;

    for (const auto& dev : devices) {
        lua_newtable(L);

        lua_pushstring(L, dev.macStr.c_str());
        lua_setfield(L, -2, "mac");

        lua_pushstring(L, dev.name.c_str());
        lua_setfield(L, -2, "name");

        lua_pushstring(L, dev.vendor.c_str());
        lua_setfield(L, -2, "vendor");

        lua_pushstring(L, dev.details.c_str());
        lua_setfield(L, -2, "details");

        lua_pushstring(L, cbdos::radio::getCategoryName(dev.category));
        lua_setfield(L, -2, "category");

        lua_pushstring(L, cbdos::radio::getCategoryTag(dev.category));
        lua_setfield(L, -2, "tag");

        lua_pushinteger(L, dev.rssi);
        lua_setfield(L, -2, "rssi");

        lua_pushinteger(L, dev.txPower);
        lua_setfield(L, -2, "tx_power");

        lua_pushboolean(L, dev.isTracker);
        lua_setfield(L, -2, "is_tracker");

        lua_pushboolean(L, dev.isCameraDevice);
        lua_setfield(L, -2, "is_camera");

        lua_pushboolean(L, dev.isRandomMac);
        lua_setfield(L, -2, "is_random_mac");

        lua_pushinteger(L, static_cast<lua_Integer>(dev.hitCount));
        lua_setfield(L, -2, "hits");

        lua_pushinteger(L, static_cast<lua_Integer>(dev.firstSeenMs));
        lua_setfield(L, -2, "first_seen");

        lua_pushinteger(L, static_cast<lua_Integer>(dev.lastSeenMs));
        lua_setfield(L, -2, "last_seen");

        std::string rawHex = bytesToHexString(dev.rawPayload, dev.rawPayloadLen);
        lua_pushstring(L, rawHex.c_str());
        lua_setfield(L, -2, "raw_hex");

        lua_pushinteger(L, static_cast<lua_Integer>(dev.rawPayloadLen));
        lua_setfield(L, -2, "raw_len");

        if (dev.hasBattery) {
            if (dev.batteryL != 0xFF) {
                lua_pushinteger(L, dev.batteryL);
                lua_setfield(L, -2, "battery_l");
            }
            if (dev.batteryR != 0xFF) {
                lua_pushinteger(L, dev.batteryR);
                lua_setfield(L, -2, "battery_r");
            }
            if (dev.batteryCase != 0xFF) {
                lua_pushinteger(L, dev.batteryCase);
                lua_setfield(L, -2, "battery_case");
            }
        }

        lua_rawseti(L, -2, idx++);
    }

    return 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3.4. Localizador Geiger y Centinela IDS (BleShieldService)
// ─────────────────────────────────────────────────────────────────────────────

int lua_ble_geiger_start(lua_State* L) {
    const char* macStr = luaL_checkstring(L, 1);
    bool audio = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;

    uint8_t targetMac[6] = {0};
    if (!parseMacAddress(macStr, targetMac)) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Invalid MAC address");
        return 2;
    }

    auto& svc = cbdos::security::BleShieldService::getInstance();
    svc.setGeigerAudioEnabled(audio);
    svc.setGeigerTarget(targetMac);

    lua_pushboolean(L, true);
    return 1;
}

int lua_ble_geiger_stop(lua_State* L) {
    cbdos::security::BleShieldService::getInstance().clearGeigerTarget();
    return 0;
}

int lua_ble_geiger_status(lua_State* L) {
    auto& svc = cbdos::security::BleShieldService::getInstance();
    lua_newtable(L);

    lua_pushboolean(L, svc.hasGeigerTarget());
    lua_setfield(L, -2, "active");

    lua_pushinteger(L, svc.getGeigerRssi());
    lua_setfield(L, -2, "rssi");

    lua_pushinteger(L, svc.getGeigerProximity());
    lua_setfield(L, -2, "proximity");

    lua_pushnumber(L, svc.getGeigerDistanceMeters());
    lua_setfield(L, -2, "distance_m");

    lua_pushinteger(L, static_cast<lua_Integer>(svc.getGeigerTickIntervalMs()));
    lua_setfield(L, -2, "tick_ms");

    lua_pushboolean(L, svc.isGeigerAudioEnabled());
    lua_setfield(L, -2, "audio");

    return 1;
}

int lua_ble_ids_status(lua_State* L) {
    auto& svc = cbdos::security::BleShieldService::getInstance();
    auto alert = svc.getSpamAlert();

    lua_newtable(L);
    lua_pushboolean(L, svc.isIdsEnabled());
    lua_setfield(L, -2, "enabled");

    lua_pushboolean(L, alert.active);
    lua_setfield(L, -2, "alert_active");

    lua_pushstring(L, spamTypeToString(alert.type));
    lua_setfield(L, -2, "type");

    lua_pushnumber(L, alert.packetsPerSec);
    lua_setfield(L, -2, "pps");

    lua_pushinteger(L, static_cast<lua_Integer>(svc.getTotalSpamPackets()));
    lua_setfield(L, -2, "total_packets");

    lua_pushstring(L, alert.description.c_str());
    lua_setfield(L, -2, "description");

    return 1;
}

int lua_ble_ids_enable(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    cbdos::security::BleShieldService::getInstance().setIdsEnabled(enabled);
    return 0;
}

int lua_ble_ids_clear(lua_State* L) {
    cbdos::security::BleShieldService::getInstance().clearSpamAlert();
    return 0;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Registro en LuaBridge
// ─────────────────────────────────────────────────────────────────────────────

void registerBleAPI(lua_State* L) {
    lua_newtable(L);

    // Adaptador y estado
    lua_pushcfunction(L, lua_ble_available);
    lua_setfield(L, -2, "available");
    lua_pushcfunction(L, lua_ble_init);
    lua_setfield(L, -2, "init");
    lua_pushcfunction(L, lua_ble_shutdown);
    lua_setfield(L, -2, "shutdown");
    lua_pushcfunction(L, lua_ble_state);
    lua_setfield(L, -2, "state");
    lua_pushcfunction(L, lua_ble_set_tx_power);
    lua_setfield(L, -2, "set_tx_power");
    lua_pushcfunction(L, lua_ble_last_error);
    lua_setfield(L, -2, "last_error");

    // Emisión (Advertising)
    lua_pushcfunction(L, lua_ble_adv_start);
    lua_setfield(L, -2, "adv_start");
    lua_pushcfunction(L, lua_ble_adv_stop);
    lua_setfield(L, -2, "adv_stop");

    // Escaneo pasivo/activo y telemetría
    lua_pushcfunction(L, lua_ble_scan_start);
    lua_setfield(L, -2, "scan_start");
    lua_pushcfunction(L, lua_ble_scan_stop);
    lua_setfield(L, -2, "scan_stop");
    lua_pushcfunction(L, lua_ble_scanning);
    lua_setfield(L, -2, "scanning");
    lua_pushcfunction(L, lua_ble_scan_clear);
    lua_setfield(L, -2, "scan_clear");
    lua_pushcfunction(L, lua_ble_scan_count);
    lua_setfield(L, -2, "scan_count");
    lua_pushcfunction(L, lua_ble_results);
    lua_setfield(L, -2, "results");

    // Geiger Tracker
    lua_pushcfunction(L, lua_ble_geiger_start);
    lua_setfield(L, -2, "geiger_start");
    lua_pushcfunction(L, lua_ble_geiger_stop);
    lua_setfield(L, -2, "geiger_stop");
    lua_pushcfunction(L, lua_ble_geiger_status);
    lua_setfield(L, -2, "geiger_status");

    // Spam IDS
    lua_pushcfunction(L, lua_ble_ids_status);
    lua_setfield(L, -2, "ids_status");
    lua_pushcfunction(L, lua_ble_ids_enable);
    lua_setfield(L, -2, "ids_enable");
    lua_pushcfunction(L, lua_ble_ids_clear);
    lua_setfield(L, -2, "ids_clear");

    lua_setfield(L, -2, "ble");
}
