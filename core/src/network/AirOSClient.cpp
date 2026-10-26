#include "cbdos/airos_client.hpp"
#include "cbdos/http.hpp"
#include "cbdos/network.hpp"
#include "../lua/cJSON.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace cbdos {
namespace network {

AirOSClient::AirOSClient() = default;

void AirOSClient::setTarget(const std::string& host, const std::string& user, const std::string& pass, uint16_t port) {
    if (m_host != host || m_user != user || m_pass != pass || m_port != port) {
        m_host = host;
        m_user = user;
        m_pass = pass;
        m_port = port;
        m_cookie.clear();
        m_csrfToken.clear();
    }
}

bool AirOSClient::authenticate(uint32_t timeoutMs) {
    if (m_host.empty() || !cbdos::network::isConnected()) {
        return false;
    }

    cbdos::http::HttpRequest authReq;
    authReq.method = cbdos::http::HttpMethod::Post;
    std::string portStr = (m_port == 443 || m_port == 0) ? "" : (":" + std::to_string(m_port));
    authReq.url = "https://" + m_host + portStr + "/api/auth";
    authReq.headers.push_back({"Content-Type", "application/x-www-form-urlencoded"});
    authReq.body = "username=" + m_user + "&password=" + m_pass;
    authReq.timeoutMs = timeoutMs;
    authReq.insecure = true;

    auto authRes = cbdos::http::execute(authReq);

    m_cookie.clear();
    m_csrfToken.clear();

    for (const auto& h : authRes.headers) {
        if (h.key == "Set-Cookie" || h.key == "set-cookie") {
            m_cookie = h.value;
        } else if (h.key == "X-CSRF-ID" || h.key == "x-csrf-id" || h.key == "x-csrf-token") {
            m_csrfToken = h.value;
        }
    }

    return (authRes.statusCode == 200 || !m_cookie.empty());
}

AirOSTelemetry AirOSClient::fetchTelemetry(uint32_t timeoutMs) {
    AirOSTelemetry result;

    if (m_host.empty()) {
        result.errorMessage = "Host no configurado";
        return result;
    }

    if (!cbdos::network::isConnected()) {
        result.errorMessage = "Red desconectada";
        return result;
    }

    // 1. Autenticar si la sesión está vacía
    if (m_cookie.empty()) {
        if (!authenticate(timeoutMs)) {
            result.errorMessage = "Fallo de autenticacion";
            return result;
        }
    }

    // 2. Consulta de telemetría a /status.cgi
    std::string portStr = (m_port == 443 || m_port == 0) ? "" : (":" + std::to_string(m_port));
    cbdos::http::HttpRequest statusReq;
    statusReq.method = cbdos::http::HttpMethod::Get;
    statusReq.url = "https://" + m_host + portStr + "/status.cgi";
    if (!m_cookie.empty()) statusReq.headers.push_back({"Cookie", m_cookie});
    if (!m_csrfToken.empty()) statusReq.headers.push_back({"X-CSRF-ID", m_csrfToken});
    statusReq.timeoutMs = timeoutMs;
    statusReq.insecure = true;

    auto statusRes = cbdos::http::execute(statusReq);

    // Reintento en caso de 401/403
    if ((statusRes.statusCode == 401 || statusRes.statusCode == 403) && authenticate(timeoutMs)) {
        statusReq.headers.clear();
        if (!m_cookie.empty()) statusReq.headers.push_back({"Cookie", m_cookie});
        if (!m_csrfToken.empty()) statusReq.headers.push_back({"X-CSRF-ID", m_csrfToken});
        statusRes = cbdos::http::execute(statusReq);
    }

    if (statusRes.statusCode != 200 || statusRes.body.empty()) {
        result.errorMessage = "HTTP Error " + std::to_string(statusRes.statusCode);
        return result;
    }

    // 3. Deserialización profunda de /status.cgi
    cJSON* root = cJSON_Parse(statusRes.body.c_str());
    if (!root) {
        result.errorMessage = "JSON Invalido";
        return result;
    }

    // A. Información del Host / Dispositivo AP
    cJSON* hostObj = cJSON_GetObjectItem(root, "host");
    if (hostObj) {
        cJSON* hname = cJSON_GetObjectItem(hostObj, "hostname");
        if (hname && hname->valuestring) result.hostname = hname->valuestring;

        cJSON* devmodel = cJSON_GetObjectItem(hostObj, "devmodel");
        if (devmodel && devmodel->valuestring) result.model = devmodel->valuestring;

        cJSON* fw = cJSON_GetObjectItem(hostObj, "fwversion");
        if (fw && fw->valuestring) result.firmware = fw->valuestring;

        cJSON* upt = cJSON_GetObjectItem(hostObj, "uptime");
        if (upt) result.uptimeSec = upt->valueint;
    }

    // B. Parámetros Wireless y Saturación de Espectro
    cJSON* wireless = cJSON_GetObjectItem(root, "wireless");
    if (wireless) {
        cJSON* essid = cJSON_GetObjectItem(wireless, "essid");
        if (essid && essid->valuestring) result.ssid = essid->valuestring;

        cJSON* freq = cJSON_GetObjectItem(wireless, "frequency");
        if (freq) result.frequency = freq->valueint;

        cJSON* bw = cJSON_GetObjectItem(wireless, "chanbw");
        if (bw) result.channelWidth = bw->valueint;

        cJSON* noise = cJSON_GetObjectItem(wireless, "noisef");
        if (noise) result.noiseFloor = noise->valueint;

        // Airtime / Polling
        cJSON* polling = cJSON_GetObjectItem(wireless, "polling");
        if (polling) {
            cJSON* use = cJSON_GetObjectItem(polling, "use");
            if (use) result.airtimeTotal = use->valueint / 10; // escala 0-100%

            cJSON* tx_use = cJSON_GetObjectItem(polling, "tx_use");
            if (tx_use) result.airtimeTx = tx_use->valueint / 10;

            cJSON* rx_use = cJSON_GetObjectItem(polling, "rx_use");
            if (rx_use) result.airtimeRx = rx_use->valueint / 10;
        }

        // C. Parseo exhaustivo de Estaciones Clientes (CPEs)
        cJSON* staArr = cJSON_GetObjectItem(wireless, "sta");
        if (staArr && cJSON_IsArray(staArr)) {
            int count = cJSON_GetArraySize(staArr);
            for (int i = 0; i < count; ++i) {
                cJSON* sta = cJSON_GetArrayItem(staArr, i);
                if (!sta) continue;

                AirOSStation info;
                cJSON* mac = cJSON_GetObjectItem(sta, "mac");
                if (mac && mac->valuestring) info.mac = mac->valuestring;

                cJSON* ip = cJSON_GetObjectItem(sta, "lastip");
                if (ip && ip->valuestring) info.ip = ip->valuestring;

                cJSON* name = cJSON_GetObjectItem(sta, "name");
                if (name && name->valuestring) info.name = name->valuestring;

                cJSON* sig = cJSON_GetObjectItem(sta, "signal");
                if (sig) info.signalDbm = sig->valueint;

                // Cadenas MIMO (Alineación)
                cJSON* chainrssi = cJSON_GetObjectItem(sta, "chainrssi");
                if (chainrssi && cJSON_IsArray(chainrssi)) {
                    cJSON* c0 = cJSON_GetArrayItem(chainrssi, 0);
                    cJSON* c1 = cJSON_GetArrayItem(chainrssi, 1);
                    if (c0) info.signalChain0 = c0->valueint;
                    if (c1) info.signalChain1 = c1->valueint;
                    info.chainDelta = std::abs(info.signalChain0 - info.signalChain1);
                }

                cJSON* tx = cJSON_GetObjectItem(sta, "tx");
                if (tx) info.txRateKbps = static_cast<int>(tx->valuedouble);

                cJSON* rx = cJSON_GetObjectItem(sta, "rx");
                if (rx) info.rxRateKbps = static_cast<int>(rx->valuedouble);

                cJSON* dist = cJSON_GetObjectItem(sta, "distance");
                if (dist) info.distanceM = dist->valueint;

                cJSON* ccq = cJSON_GetObjectItem(sta, "ccq");
                if (ccq) info.ccq = ccq->valueint;

                // Datos remotos del CPE (Capacidad, Throughput y Cable)
                cJSON* remote = cJSON_GetObjectItem(sta, "remote");
                if (remote) {
                    cJSON* rHostname = cJSON_GetObjectItem(remote, "hostname");
                    if (rHostname && rHostname->valuestring && info.name.empty()) {
                        info.name = rHostname->valuestring;
                    }

                    cJSON* rUptime = cJSON_GetObjectItem(remote, "uptime");
                    if (rUptime) info.uptimeSec = rUptime->valueint;

                    cJSON* txTh = cJSON_GetObjectItem(remote, "tx_throughput");
                    if (txTh) info.txThroughputKbps = txTh->valueint;

                    cJSON* rxTh = cJSON_GetObjectItem(remote, "rx_throughput");
                    if (rxTh) info.rxThroughputKbps = rxTh->valueint;

                    // Diagnóstico de Cable Ethernet PoE local en el CPE
                    cJSON* ethlist = cJSON_GetObjectItem(remote, "ethlist");
                    if (ethlist && cJSON_IsArray(ethlist) && cJSON_GetArraySize(ethlist) > 0) {
                        cJSON* eth0 = cJSON_GetArrayItem(ethlist, 0);
                        if (eth0) {
                            cJSON* plugged = cJSON_GetObjectItem(eth0, "plugged");
                            if (plugged) info.ethPlugged = (plugged->valueint != 0);

                            cJSON* speed = cJSON_GetObjectItem(eth0, "speed");
                            if (speed) info.ethSpeedMbps = speed->valueint;

                            // Alerta si está desconectado o negocia a solo 10 Mbps (cable estrangulado/dañado)
                            if (!info.ethPlugged || info.ethSpeedMbps <= 10) {
                                info.cableFault = true;
                            }
                        }
                    }
                }

                // Capacidad AirMAX
                cJSON* airmax = cJSON_GetObjectItem(sta, "airmax");
                if (airmax) {
                    cJSON* dlCap = cJSON_GetObjectItem(airmax, "dl_capacity");
                    if (dlCap) info.dlCapacityMbps = dlCap->valueint / 1000;

                    cJSON* ulCap = cJSON_GetObjectItem(airmax, "ul_capacity");
                    if (ulCap) info.ulCapacityMbps = ulCap->valueint / 1000;
                }

                result.stations.push_back(info);
            }
        }
    }

    // D. GPS (si el equipo tiene antena GPS)
    cJSON* gps = cJSON_GetObjectItem(root, "gps");
    if (gps) {
        cJSON* sats = cJSON_GetObjectItem(gps, "sats");
        if (sats) result.gpsSats = sats->valueint;
    }

    cJSON_Delete(root);
    result.success = true;
    return result;
}

} // namespace network
} // namespace cbdos
