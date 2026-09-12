// CBDos HAL: ILanScannerBackend para ESP32-P4 (ESP-IDF 5.5 nativo).
//
// Ver specs/drafts/BORRADOR_LAN_RECON_Y_ESCANEADOR_RED_LOCAL.md §2 (diagrama HAL)
// y contrato en core/include/cbdos/lan_recon.hpp.
// Transporte: sockets BSD de LwIP (no bloqueante + select) y cache ARP de LwIP.

#include "cbdos/lan_recon.hpp"
#include "cbdos/network.hpp"

#include <esp_log.h>
#include <esp_netif.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/netif.h>
#include <lwip/etharp.h>
#include <lwip/ip_addr.h>
#include <lwip/inet.h>

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdio>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG_LAN_P4 = "LAN_RECON_P4";

namespace cbdos {
namespace bsp {
namespace {

constexpr uint32_t kDefaultMaskFallback = 0xFFFFFF00u;  // 255.255.255.0

void applySocketTimeouts(int sock, uint32_t timeoutMs) {
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeoutMs / 1000u);
    tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000u) * 1000u);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// Conecta por TCP con timeout usando socket no bloqueante + select.
// En exito deja el socket en modo bloqueante con timeouts y lo devuelve
// en outSock (el llamador debe cerrarlo con ::close). Retorna false si falla.
bool connectWithTimeout(const char* ip, uint16_t port, uint32_t timeoutMs, int* outSock) {
    if (outSock == nullptr || ip == nullptr || ip[0] == '\0' || port == 0) {
        return false;
    }
    *outSock = -1;
    if (timeoutMs == 0) {
        timeoutMs = 250;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    int prevFlags = fcntl(sock, F_GETFL, 0);
    if (prevFlags < 0) {
        prevFlags = 0;
    }
    if (fcntl(sock, F_SETFL, prevFlags | O_NONBLOCK) < 0) {
        ::close(sock);
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        ::close(sock);
        return false;
    }

    int rc = ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (rc != 0 && errno != EINPROGRESS) {
        ::close(sock);
        return false;
    }

    if (rc != 0) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sock, &wfds);
        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(timeoutMs / 1000u);
        tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000u) * 1000u);
        int sel = select(sock + 1, nullptr, &wfds, nullptr, &tv);
        if (sel <= 0) {
            ::close(sock);
            return false;
        }
        int soErr = 0;
        socklen_t soErrLen = sizeof(soErr);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soErr, &soErrLen) != 0 || soErr != 0) {
            ::close(sock);
            return false;
        }
    }

    // Vuelve a modo bloqueante con timeouts acotados para send/recv.
    fcntl(sock, F_SETFL, prevFlags);
    applySocketTimeouts(sock, timeoutMs);
    *outSock = sock;
    return true;
}

bool parseIpv4(const std::string& ip, ip4_addr_t* out) {
    if (out == nullptr || ip.empty()) {
        return false;
    }
    // ipaddr_aton acepta solo dotted-decimal para IP_T; suficiente para el sweep.
    ip_addr_t tmp;
    std::memset(&tmp, 0, sizeof(tmp));
    if (ipaddr_aton(ip.c_str(), &tmp) == 0) {
        return false;
    }
    if (!IP_IS_V4(&tmp)) {
        return false;
    }
    *out = tmp.u_addr.ip4;
    return true;
}

bool lookupArpCache(const ip4_addr_t& ip4, uint8_t mac[6]) {
    // v2.1: SIN filtro de flags/hwaddr. Con WiFi hosted (C6) el netif
    // puede no marcar ETHARP y el filtro cegaba la lectura aunque la
    // entrada ARP si existiera. etharp_find_addr solo compara tabla.
    for (struct netif* n = netif_list; n != nullptr; n = n->next) {
        struct eth_addr* ethRet = nullptr;
        const ip4_addr_t* ipRet = nullptr;
        s8_t idx = etharp_find_addr(n, &ip4, &ethRet, &ipRet);
        if (idx >= 0 && ethRet != nullptr) {
            std::memcpy(mac, ethRet->addr, 6);
            return true;
        }
    }
    return false;
}

bool sendArpRequest(const ip4_addr_t& ip4) {
    bool sent = false;
    for (struct netif* n = netif_list; n != nullptr; n = n->next) {
        if (netif_is_up(n) && (n->flags & NETIF_FLAG_ETHARP) && n->hwaddr_len == 6) {
            etharp_request(n, &ip4);
            sent = true;
        }
    }
    if (!sent) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            ESP_LOGW(TAG_LAN_P4, "arpProbe: ningun netif ETHARP-up (WiFi hosted?)");
        }
    }
    return sent;
}

std::string cleanBanner(std::string s) {
    for (char& c : s) {
        if (c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        } else if (static_cast<unsigned char>(c) < 0x20u ||
                   static_cast<unsigned char>(c) == 0x7Fu) {
            c = ' ';
        }
    }
    // Trim de espacios en ambos extremos.
    std::size_t begin = 0;
    while (begin < s.size() && s[begin] == ' ') {
        ++begin;
    }
    std::size_t end = s.size();
    while (end > begin && s[end - 1] == ' ') {
        --end;
    }
    return s.substr(begin, end - begin);
}

std::string toLowerCopy(const std::string& s) {
    std::string out(s.size(), '\0');
    for (std::size_t i = 0; i < s.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return out;
}

std::string trimCopy(const std::string& s) {
    std::size_t begin = 0;
    while (begin < s.size() &&
           (s[begin] == ' ' || s[begin] == '\t' || s[begin] == '\r' || s[begin] == '\n')) {
        ++begin;
    }
    std::size_t end = s.size();
    while (end > begin &&
            (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return s.substr(begin, end - begin);
}

// ── v2 herramienta real: ICMP Echo real via raw socket ──
uint16_t icmpChecksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    size_t i = 0;
    while (i + 1 < len) {
        sum += (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
        i += 2;
    }
    if (i < len) {
        sum += static_cast<uint32_t>(data[i]) << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

uint32_t nowMs() {
    return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

bool icmpEchoOnce(const char* ip, uint32_t timeoutMs, uint32_t* outRttMs) {
    if (ip == nullptr || ip[0] == '\0') {
        return false;
    }
    if (timeoutMs == 0) {
        timeoutMs = 400;
    }
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        static bool s_rawWarned = false;
        if (!s_rawWarned) {
            s_rawWarned = true;
            ESP_LOGW(TAG_LAN_P4, "socket RAW ICMP no disponible (errno=%d). Ping usa fallback TCP.", errno);
        }
        return false;
    }
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeoutMs / 1000u);
    tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000u) * 1000u);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dst;
    std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &dst.sin_addr) != 1) {
        ::close(sock);
        return false;
    }

    uint8_t pkt[32];
    std::memset(pkt, 0, sizeof(pkt));
    pkt[0] = 8;
    pkt[1] = 0;
    const uint16_t ident = static_cast<uint16_t>(nowMs() & 0xFFFF);
    pkt[4] = static_cast<uint8_t>(ident >> 8);
    pkt[5] = static_cast<uint8_t>(ident & 0xFF);
    pkt[6] = 0;
    pkt[7] = 1;
    for (size_t i = 8; i < sizeof(pkt); ++i) {
        pkt[i] = static_cast<uint8_t>(i);
    }
    uint16_t cksum = icmpChecksum(pkt, sizeof(pkt));
    pkt[2] = static_cast<uint8_t>(cksum >> 8);
    pkt[3] = static_cast<uint8_t>(cksum & 0xFF);

    const uint32_t t0 = nowMs();
    ssize_t sent = sendto(sock, pkt, sizeof(pkt), 0,
                          reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
    if (sent != static_cast<ssize_t>(sizeof(pkt))) {
        ::close(sock);
        return false;
    }
    uint8_t rx[128];
    for (;;) {
        if (nowMs() - t0 >= timeoutMs) {
            break;
        }
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(sock, rx, sizeof(rx), 0,
                             reinterpret_cast<struct sockaddr*>(&from), &fromLen);
        if (n < 28) {
            continue;
        }
        const size_t ihl = (rx[0] & 0x0F) * 4;
        if (ihl + 8 > static_cast<size_t>(n)) {
            continue;
        }
        const uint8_t* icmp = rx + ihl;
        if (icmp[0] != 0) {
            continue;
        }
        const uint16_t rIdent = (static_cast<uint16_t>(icmp[4]) << 8) | icmp[5];
        if (rIdent != ident) {
            continue;
        }
        if (outRttMs) {
            *outRttMs = nowMs() - t0;
        }
        ::close(sock);
        return true;
    }
    ::close(sock);
    return false;
}

bool udpTouch(const char* ip, uint16_t port, uint32_t timeoutMs = 250) {
    if (ip == nullptr || ip[0] == '\0' || port == 0) {
        return false;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(timeoutMs / 1000u);
    tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000u) * 1000u);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_in dst;
    std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &dst.sin_addr) != 1) {
        ::close(sock);
        return false;
    }
    const uint8_t probe = 0xAA;
    sendto(sock, &probe, 1, 0, reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));
    uint8_t rx[64];
    recv(sock, rx, sizeof(rx), 0);
    ::close(sock);
    return true;
}

class LanScannerBackendP4 : public network::ILanScannerBackend {
public:
    bool isNetworkConnected() const override {
        return network::isConnected();
    }

    std::string getLocalIp() const override {
        return network::getIpAddress();
    }

    std::string getSubnetMask() const override {
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == nullptr) {
            netif = esp_netif_next(nullptr);
        }
        if (netif != nullptr) {
            esp_netif_ip_info_t info;
            std::memset(&info, 0, sizeof(info));
            if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
                char buf[IP4ADDR_STRLEN_MAX];
                std::memset(buf, 0, sizeof(buf));
                esp_ip4addr_ntoa(&info.netmask, buf, static_cast<int>(sizeof(buf)));
                if (buf[0] != '\0') {
                    return std::string(buf);
                }
            }
        }
        // v2 fix: antes imprimia "0.255.255.255" por orden de bytes invertido.
        return std::string("255.255.255.0");
    }

    bool pingHost(const std::string& ip, uint32_t timeoutMs = 150) override {
        if (ip.empty()) {
            return false;
        }
        // Sondeo rapido TCP en puertos habituales; reparte el timeout.
        static const uint16_t kProbePorts[] = {80, 443, 22};
        uint32_t perPort = timeoutMs / 3u;
        if (perPort < 50u) {
            perPort = 50u;
        }
        for (uint16_t port : kProbePorts) {
            int sock = -1;
            if (connectWithTimeout(ip.c_str(), port, perPort, &sock)) {
                ::close(sock);
                return true;
            }
        }
        return false;
    }

    bool resolveMacArp(const std::string& ip, uint8_t mac[6]) override {
        if (ip.empty() || mac == nullptr) {
            return false;
        }
        ip4_addr_t ip4;
        std::memset(&ip4, 0, sizeof(ip4));
        if (!parseIpv4(ip, &ip4)) {
            return false;
        }
        if (lookupArpCache(ip4, mac)) {
            return true;
        }
        // v2: estimulo UDP via sockets (netif-agnostico: la propia lwIP
        // resuelve ARP al enviar) + requests manuales best-effort.
        udpTouch(ip.c_str(), 5353, 120);
        udpTouch(ip.c_str(), 1900, 120);
        for (int attempt = 0; attempt < 3; ++attempt) {
            sendArpRequest(ip4);
            vTaskDelay(pdMS_TO_TICKS(180));
            if (lookupArpCache(ip4, mac)) {
                return true;
            }
        }
        return lookupArpCache(ip4, mac);
    }

    // ── v2.1 blast+collect ──
    bool arpProbe(const std::string& ip) override {
        if (ip.empty()) {
            return false;
        }
        ip4_addr_t ip4;
        std::memset(&ip4, 0, sizeof(ip4));
        if (!parseIpv4(ip, &ip4)) {
            return false;
        }
        sendArpRequest(ip4);
        return true;
    }

    bool lookupArpCacheOnly(const std::string& ip, uint8_t mac[6]) override {
        if (ip.empty() || mac == nullptr) {
            return false;
        }
        ip4_addr_t ip4;
        std::memset(&ip4, 0, sizeof(ip4));
        if (!parseIpv4(ip, &ip4)) {
            return false;
        }
        return lookupArpCache(ip4, mac);
    }

    bool probeTcpPort(const std::string& ip, uint16_t port, uint32_t timeoutMs = 250) override {
        if (ip.empty() || port == 0) {
            return false;
        }
        int sock = -1;
        if (!connectWithTimeout(ip.c_str(), port, timeoutMs, &sock)) {
            return false;
        }
        ::close(sock);
        return true;
    }

    // ── v2 herramienta real ──
    bool pingIcmp(const std::string& ip, uint32_t timeoutMs,
                  uint32_t* outRttMs = nullptr) override {
        if (ip.empty()) {
            return false;
        }
        if (outRttMs) {
            *outRttMs = 0;
        }
        uint32_t rtt = 0;
        if (icmpEchoOnce(ip.c_str(), timeoutMs, &rtt)) {
            if (outRttMs) {
                *outRttMs = rtt;
            }
            return true;
        }
        return false;
    }

    bool udpStimulate(const std::string& ip, uint16_t port = 5353) override {
        if (ip.empty()) {
            return false;
        }
        return udpTouch(ip.c_str(), port, 150);
    }

    std::string getGatewayIp() const override {
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif == nullptr) {
            netif = esp_netif_next(nullptr);
        }
        if (netif != nullptr) {
            esp_netif_ip_info_t info;
            std::memset(&info, 0, sizeof(info));
            if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
                char buf[IP4ADDR_STRLEN_MAX];
                std::memset(buf, 0, sizeof(buf));
                esp_ip4addr_ntoa(&info.gw, buf, static_cast<int>(sizeof(buf)));
                if (buf[0] != '\0' && std::string(buf) != "0.0.0.0") {
                    return std::string(buf);
                }
            }
        }
        return std::string();
    }

    bool ssdpScan(uint32_t timeoutMs,
                  network::OnSsdpDeviceCallback onDevice) override {
        if (!onDevice || !network::isConnected()) {
            return false;
        }
        if (timeoutMs < 1000) {
            timeoutMs = 2500;
        }
        if (timeoutMs > 8000) {
            timeoutMs = 8000;
        }
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            return false;
        }
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 250 * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        const uint8_t ttl = 2;
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

        struct sockaddr_in dst;
        std::memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(1900);
        inet_pton(AF_INET, "239.255.255.250", &dst.sin_addr);

        const char* msearch =
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "MAN: \"ns=01; ns=01\"\r\n"
            "MX: 2\r\n"
            "ST: ssdp:all\r\n"
            "USER-AGENT: CBDos-Recon/2.0\r\n\r\n";
        sendto(sock, msearch, strlen(msearch), 0,
               reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));

        const uint32_t t0 = nowMs();
        char rx[1500];
        uint32_t found = 0;
        while (nowMs() - t0 < timeoutMs) {
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(sock, rx, sizeof(rx) - 1, 0,
                                 reinterpret_cast<struct sockaddr*>(&from), &fromLen);
            if (n <= 0) {
                continue;
            }
            rx[n] = '\0';
            std::string resp(rx, static_cast<size_t>(n));
            std::string lower = toLowerCopy(resp);
            if (lower.find("location:") == std::string::npos &&
                lower.find("server:") == std::string::npos) {
                continue;
            }
            char ipStr[32] = {0};
            {
                struct in_addr ina = from.sin_addr;
                const char* asc = inet_ntoa(ina);
                if (asc) {
                    std::strncpy(ipStr, asc, sizeof(ipStr) - 1);
                }
            }
            if (ipStr[0] == '\0') {
                continue;
            }
            network::SsdpDeviceInfo dev;
            dev.ip = ipStr;
            size_t pos = 0;
            while (pos < resp.size()) {
                size_t eol = resp.find("\r\n", pos);
                if (eol == std::string::npos) {
                    eol = resp.size();
                }
                std::string line = resp.substr(pos, eol - pos);
                std::string ll = toLowerCopy(line);
                if (ll.rfind("location:", 0) == 0) {
                    dev.location = trimCopy(line.substr(9));
                } else if (ll.rfind("server:", 0) == 0) {
                    dev.server = trimCopy(line.substr(7));
                } else if (ll.rfind("usn:", 0) == 0) {
                    dev.usn = trimCopy(line.substr(4));
                } else if (ll.rfind("st:", 0) == 0) {
                    dev.st = trimCopy(line.substr(3));
                }
                if (eol == resp.size()) {
                    break;
                }
                pos = eol + 2;
            }
            onDevice(dev);
            ++found;
        }
        ::close(sock);
        ESP_LOGI(TAG_LAN_P4, "SSDP: %u dispositivos anunciados", static_cast<unsigned>(found));
        return found > 0;
    }

    std::string grabTcpBanner(const std::string& ip, uint16_t port, uint32_t timeoutMs = 500) override {
        if (ip.empty() || port == 0) {
            return std::string();
        }
        int sock = -1;
        if (!connectWithTimeout(ip.c_str(), port, timeoutMs, &sock)) {
            return std::string();
        }
        // Espera legible hasta timeoutMs antes de leer (banners SSH/Telnet).
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(timeoutMs / 1000u);
        tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000u) * 1000u);
        std::string result;
        if (select(sock + 1, &rfds, nullptr, nullptr, &tv) > 0) {
            char buf[257];
            std::memset(buf, 0, sizeof(buf));
            ssize_t n = recv(sock, buf, 256, 0);
            if (n > 0) {
                result.assign(buf, static_cast<std::size_t>(n));
            }
        }
        ::close(sock);
        return cleanBanner(result);
    }

    std::string fetchHttpTitle(const std::string& ip, uint16_t port, uint32_t timeoutMs = 800) override {
        if (ip.empty() || port == 0) {
            return std::string();
        }
        int sock = -1;
        if (!connectWithTimeout(ip.c_str(), port, timeoutMs, &sock)) {
            return std::string();
        }
        std::string req = "GET / HTTP/1.0\r\nHost: " + ip + "\r\nUser-Agent: CBDos-Recon\r\n\r\n";
        std::size_t sent = 0;
        while (sent < req.size()) {
            ssize_t w = send(sock, req.data() + sent, req.size() - sent, 0);
            if (w <= 0) {
                ::close(sock);
                return std::string();
            }
            sent += static_cast<std::size_t>(w);
        }

        std::string body;
        body.reserve(4096);
        char chunk[512];
        while (body.size() < 8192) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            struct timeval tv;
            tv.tv_sec = static_cast<time_t>(timeoutMs / 1000u);
            tv.tv_usec = static_cast<suseconds_t>((timeoutMs % 1000u) * 1000u);
            int ready = select(sock + 1, &rfds, nullptr, nullptr, &tv);
            if (ready <= 0) {
                break;
            }
            ssize_t n = recv(sock, chunk, sizeof(chunk), 0);
            if (n <= 0) {
                break;
            }
            body.append(chunk, static_cast<std::size_t>(n));
            std::string lowered = toLowerCopy(body);
            if (lowered.find("</title>") != std::string::npos) {
                break;
            }
        }
        ::close(sock);

        std::string lowered = toLowerCopy(body);
        std::size_t begin = lowered.find("<title>");
        if (begin == std::string::npos) {
            return std::string();
        }
        begin += 7u;  // strlen("<title>")
        std::size_t end = lowered.find("</title>", begin);
        if (end == std::string::npos || end <= begin) {
            return std::string();
        }
        std::string title = trimCopy(body.substr(begin, end - begin));
        if (title.size() > 256u) {
            title.resize(256u);
        }
        return title;
    }
};

static LanScannerBackendP4 s_lanBackendP4;

}  // namespace
}  // namespace bsp
}  // namespace cbdos

namespace cbdos {
namespace bsp {

void init_lan_recon_p4() {
    network::setLanScannerBackend(&s_lanBackendP4);
    ESP_LOGI(TAG_LAN_P4, "HAL LAN Recon P4 registrado (LwIP + sockets BSD).");
}

}  // namespace bsp
}  // namespace cbdos
