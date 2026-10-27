#include "cbdos/neighbor_discovery.hpp"
#include <Arduino.h>
#include <WiFi.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

#include <unistd.h>
#include <cstring>
#include <memory>

namespace cbdos {
namespace bsp {
namespace {

class NeighborDiscoveryBackendS3 : public network::INeighborDiscoveryBackend {
public:
    bool isNetworkConnected() const override {
        return WiFi.status() == WL_CONNECTED;
    }

    bool scanUbiquiti(uint32_t timeoutMs, network::OnNeighborDiscoveredCb onDevice) override {
        if (!onDevice || !isNetworkConnected()) {
            return false;
        }
        if (timeoutMs < 500) timeoutMs = 1500;
        if (timeoutMs > 5000) timeoutMs = 5000;

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) return false;

        int broadcastEnable = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in dst;
        std::memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(10001);
        dst.sin_addr.s_addr = INADDR_BROADCAST;

        // Paquete de sondeo Ubiquiti v1
        const uint8_t probe[] = {0x01, 0x00, 0x00, 0x00};
        sendto(sock, probe, sizeof(probe), 0,
               reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));

        const uint32_t t0 = millis();
        auto rxBuf = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[1500]);
        if (!rxBuf) {
            ::close(sock);
            return false;
        }

        uint8_t* rx = rxBuf.get();
        uint32_t found = 0;

        while (millis() - t0 < timeoutMs) {
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(sock, rx, 1500, 0,
                                 reinterpret_cast<struct sockaddr*>(&from), &fromLen);
            if (n < 4) continue;

            // Validación de cabecera UBNT (0x01 o 0x02, versión 0x00, 0x00)
            if (rx[0] != 0x01 && rx[0] != 0x02) continue;

            network::NeighborDevice dev;
            dev.brand = network::NeighborBrand::Ubiquiti;
            dev.protocol = "ubnt";
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(from.sin_addr), ipStr, sizeof(ipStr));
            dev.ip = ipStr;
            dev.lastSeenMs = millis();

            // Decodificación de TLVs UBNT
            // Formato TLV: [Type: 1B] [Length: 2B (big-endian)] [Value: Length bytes]
            size_t offset = 4;
            while (offset + 3 <= static_cast<size_t>(n)) {
                uint8_t type = rx[offset];
                uint16_t len = (static_cast<uint16_t>(rx[offset + 1]) << 8) | rx[offset + 2];
                offset += 3;
                if (offset + len > static_cast<size_t>(n)) break;

                const uint8_t* val = &rx[offset];
                if (type == 0x01 && len == 6) {
                    std::memcpy(dev.mac, val, 6);
                } else if (type == 0x02 && len >= 10) {
                    // MAC + IP
                    std::memcpy(dev.mac, val, 6);
                    char ipBuf[16];
                    std::snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", val[6], val[7], val[8], val[9]);
                    dev.ip = ipBuf;
                } else if (type == 0x03) {
                    dev.firmware.assign(reinterpret_cast<const char*>(val), len);
                } else if (type == 0x0B) {
                    dev.identity.assign(reinterpret_cast<const char*>(val), len);
                } else if (type == 0x0C) {
                    dev.model.assign(reinterpret_cast<const char*>(val), len);
                } else if (type == 0x0D) {
                    // ESSID (para antenas airMAX)
                    std::string essid(reinterpret_cast<const char*>(val), len);
                    if (!essid.empty() && dev.identity.empty()) {
                        dev.identity = essid;
                    }
                }
                offset += len;
            }

            if (!dev.ip.empty()) {
                onDevice(dev);
                ++found;
            }
        }

        ::close(sock);
        return found > 0;
    }

    bool scanMikrotik(uint32_t timeoutMs, network::OnNeighborDiscoveredCb onDevice) override {
        if (!onDevice || !isNetworkConnected()) {
            return false;
        }
        if (timeoutMs < 500) timeoutMs = 1500;
        if (timeoutMs > 5000) timeoutMs = 5000;

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) return false;

        int broadcastEnable = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

        struct sockaddr_in bindAddr;
        std::memset(&bindAddr, 0, sizeof(bindAddr));
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons(5678);
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bind(sock, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr));

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Enviar consulta broadcast MNDP a 255.255.255.255:5678
        struct sockaddr_in dst;
        std::memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(5678);
        dst.sin_addr.s_addr = INADDR_BROADCAST;

        const uint8_t mndpQuery[] = {0x00, 0x00, 0x00, 0x00};
        sendto(sock, mndpQuery, sizeof(mndpQuery), 0,
               reinterpret_cast<struct sockaddr*>(&dst), sizeof(dst));

        const uint32_t t0 = millis();
        auto rxBuf = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[1500]);
        if (!rxBuf) {
            ::close(sock);
            return false;
        }

        uint8_t* rx = rxBuf.get();
        uint32_t found = 0;

        while (millis() - t0 < timeoutMs) {
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(sock, rx, 1500, 0,
                                 reinterpret_cast<struct sockaddr*>(&from), &fromLen);
            if (n < 4) continue;

            network::NeighborDevice dev;
            dev.brand = network::NeighborBrand::MikroTik;
            dev.protocol = "mndp";
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(from.sin_addr), ipStr, sizeof(ipStr));
            dev.ip = ipStr;
            dev.lastSeenMs = millis();

            // Decodificación TLV MikroTik MNDP:
            // Cabecera MNDP: 4 bytes (2B flags/unknown + 2B SeqNo)
            // Formato TLV: [Type: 2B (big-endian)] [Length: 2B (big-endian)] [Value: Length bytes]
            size_t offset = 4;
            while (offset + 4 <= static_cast<size_t>(n)) {
                uint16_t type = (static_cast<uint16_t>(rx[offset]) << 8) | rx[offset + 1];
                uint16_t len = (static_cast<uint16_t>(rx[offset + 2]) << 8) | rx[offset + 3];
                offset += 4;
                if (offset + len > static_cast<size_t>(n)) break;

                const uint8_t* val = &rx[offset];
                if (type == 0x0001 && len == 6) {
                    std::memcpy(dev.mac, val, 6);
                } else if (type == 0x0005) {
                    dev.identity.assign(reinterpret_cast<const char*>(val), len);
                } else if (type == 0x0007) {
                    dev.firmware.assign(reinterpret_cast<const char*>(val), len);
                } else if (type == 0x000C) {
                    // Board model exacto (ej: "RB750Gr3", "CCR2004")
                    dev.model.assign(reinterpret_cast<const char*>(val), len);
                } else if (type == 0x0008) {
                    // Platform fallback si aún no tenemos Board (ej: "RouterBOARD")
                    if (dev.model.empty()) {
                        dev.model.assign(reinterpret_cast<const char*>(val), len);
                    }
                } else if (type == 0x000A && len == 4) {
                    dev.uptimeSec = (static_cast<uint32_t>(val[0]) << 24) |
                                   (static_cast<uint32_t>(val[1]) << 16) |
                                   (static_cast<uint32_t>(val[2]) << 8)  |
                                   static_cast<uint32_t>(val[3]);
                } else if (type == 0x0010) {
                    dev.interface.assign(reinterpret_cast<const char*>(val), len);
                }
                offset += len;
            }

            if (!dev.ip.empty()) {
                onDevice(dev);
                ++found;
            }
        }

        ::close(sock);
        return found > 0;
    }
};

static NeighborDiscoveryBackendS3 s_neighborBackendS3;

} // namespace
} // namespace bsp
} // namespace cbdos

namespace cbdos {
namespace bsp {

void initNeighborDiscoveryS3() {
    network::setNeighborDiscoveryBackend(&s_neighborBackendS3);
    Serial.println("[NEIGHBOR_S3] Backend Neighbor Discovery (UBNT/MNDP) registrado.");
}

} // namespace bsp
} // namespace cbdos
