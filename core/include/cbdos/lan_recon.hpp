#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace network {

// ────────────────────────────────────────────────────────────────
// LAN Recon: fases del escaneo (Idle -> ArpSweep -> PortScan ->
// BannerGrab -> Completed / Aborted). Ver specs/drafts/
// BORRADOR_LAN_RECON_Y_ESCANEADOR_RED_LOCAL.md §4.
// Contrato C++17 puro: sin headers de Arduino, ESP-IDF ni FreeRTOS.
// ────────────────────────────────────────────────────────────────

enum class LanScanPhase {
    Idle,
    ArpSweep,
    PortScan,
    BannerGrab,
    Completed,
    Aborted
};

// Puertos criticos sondeados por el escaner (ver borrador §1).
// El bitmask de LanHostInfo usa la posicion en esta tabla como bit.
// v2 herramienta real: se agregan puertos PC (SMB/NetBIOS/RPC) y TV
// (Chromecast/AirPlay/Sonos) para no perder PCs y SmartTV con 80/443
// cerrados. Max 32 entradas (bitmask uint32_t).
static constexpr uint16_t kLanReconPorts[] = {
    22,    // SSH
    23,    // Telnet
    80,    // HTTP
    443,   // HTTPS
    554,   // RTSP (CCTV)
    3389,  // RDP
    8080,  // HTTP alt
    8443,  // HTTPS alt
    1900,  // SSDP/UPnP (TCP, algunas TVs lo abren)
    21,    // FTP
    135,   // RPC (Windows)
    139,   // NetBIOS (Windows/Samba)
    445,   // SMB (Windows/Samba/NAS) - clave para detectar PCs
    1400,  // Sonos / UPnP AV
    7000,  // AirPlay
    8008,  // Chromecast / Android TV
    8009,  // Chromecast TLS
    9100,  // Impresoras RAW
    1883,  // MQTT (IoT)
    5357,  // WSDAPI (Windows discovery)
};
static constexpr std::size_t kLanReconPortCount =
    sizeof(kLanReconPorts) / sizeof(kLanReconPorts[0]);

// Indice del puerto en kLanReconPorts, o -1 si no esta cubierto.
inline int lanPortToBitIndex(uint16_t port) {
    for (std::size_t i = 0; i < kLanReconPortCount; ++i) {
        if (kLanReconPorts[i] == port) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Bit del bitmask para un puerto conocido, o 0 si no esta cubierto.
inline uint32_t lanPortToBit(uint16_t port) {
    const int idx = lanPortToBitIndex(port);
    return (idx >= 0 && idx < 32) ? (1u << static_cast<uint32_t>(idx)) : 0u;
}

struct LanHostInfo {
    std::string ip;
    uint8_t mac[6]{0};
    std::string vendor;
    std::string banner;
    uint32_t openPortsBitmask{0};
    bool alive{false};

    // ── v2 herramienta real (defaults conservan compat) ──
    uint32_t rttMs{0};            // RTT del ping ICMP/TCP que lo descubrio
    std::string discovery;        // "arp","icmp","tcp","ssdp","cache","manual"
    std::string ssdpServer;       // Cabecera SERVER SSDP (TVs DLNA/Chromecast)
    std::string ssdpLocation;     // URL LOCATION SSDP (descriptor XML)
    bool isTv{false};             // Heuristica TV (vendor/SERVER/puertos 8008/8009/1400/7000)

    bool hasPort(uint16_t port) const;
    // Marca el puerto como abierto. Retorna false si el puerto no
    // esta en kLanReconPorts (no representable en el bitmask).
    bool addPort(uint16_t port);
    // Formato "AA:BB:CC:DD:EE:FF" (hex mayusculas).
    std::string getMacString() const;
};

struct LanScanProgress {
    LanScanPhase phase{LanScanPhase::Idle};
    uint8_t percentage{0};
    uint16_t hostsDiscovered{0};
    uint16_t currentHostIndex{0};
};

using OnHostFoundCallback = std::function<void(const LanHostInfo& host)>;
using OnProgressCallback = std::function<void(const LanScanProgress& progress)>;
using OnScanFinishedCallback = std::function<void(const std::vector<LanHostInfo>& results)>;

// Dispositivo anunciado via SSDP M-SEARCH (UDP 239.255.255.250:1900).
// Las SmartTV (Samsung/LG/Roku/Chromecast) y muchas impresoras/IoT
// responden aqui aunque tengan el TCP filtrado.
struct SsdpDeviceInfo {
    std::string ip;
    std::string location;
    std::string server;
    std::string usn;
    std::string st;
};

using OnSsdpDeviceCallback = std::function<void(const SsdpDeviceInfo& dev)>;

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ puro. Implementaciones en bsp/*/hal/
// (LwIP + sockets BSD, worker FreeRTOS en Core 0). core/ solo
// orquesta a traves de este backend inyectado.
// ────────────────────────────────────────────────────────────────

class ILanScannerBackend {
public:
    virtual ~ILanScannerBackend() = default;

    virtual bool isNetworkConnected() const = 0;
    virtual std::string getLocalIp() const = 0;
    virtual std::string getSubnetMask() const = 0;

    virtual bool pingHost(const std::string& ip, uint32_t timeoutMs = 150) = 0;
    virtual bool resolveMacArp(const std::string& ip, uint8_t mac[6]) = 0;
    virtual bool probeTcpPort(const std::string& ip, uint16_t port, uint32_t timeoutMs = 250) = 0;
    virtual std::string grabTcpBanner(const std::string& ip, uint16_t port, uint32_t timeoutMs = 500) = 0;
    virtual std::string fetchHttpTitle(const std::string& ip, uint16_t port, uint32_t timeoutMs = 800) = 0;

    // ── v2 herramienta real (defaults = compat con HALs viejos) ──
    // Ping ICMP real (raw socket / esp_ping). outRttMs opcional.
    // Default: delega al pingHost legacy (TCP) para no romper.
    virtual bool pingIcmp(const std::string& ip, uint32_t timeoutMs,
                          uint32_t* outRttMs = nullptr) {
        return pingHost(ip, timeoutMs);
    }
    // Envia un datagrama UDP a puerto probablemente cerrado para forzar
    // al host a responder (ICMP port-unreachable) y poblar la caché ARP.
    // No requiere respuesta valida, solo "tocar" al host.
    virtual bool udpStimulate(const std::string& ip, uint16_t port = 5353) {
        (void)ip; (void)port; return false;
    }
    // Disparo ARP fire-and-forget (sin espera): para barridos blast,
    // donde la espera se hace una sola vez para toda la subred.
    // Default: aproximacion via UDP (la pila lwIP resuelve ARP sola).
    virtual bool arpProbe(const std::string& ip) {
        return udpStimulate(ip, 5353);
    }
    // Lectura NO bloqueante de la cache ARP (sin generar trafico ni
    // esperas). Ideal tras una fase blast. Default: resolve bloqueante.
    virtual bool lookupArpCacheOnly(const std::string& ip, uint8_t mac[6]) {
        return resolveMacArp(ip, mac);
    }
    // IP del gateway por defecto (para mostrar y para excluir/resaltar).
    virtual std::string getGatewayIp() const { return std::string(); }
    // Barrido SSDP M-SEARCH. Invoca onDevice por cada respuesta HTTPU.
    virtual bool ssdpScan(uint32_t timeoutMs,
                          OnSsdpDeviceCallback onDevice) {
        (void)timeoutMs; (void)onDevice; return false;
    }
};

// Helpers IPv4/CIDR puros (sin sockets). Implementados en lan_recon.cpp.
bool parseIpv4Uint32(const std::string& ip, uint32_t& out);
std::string formatIpv4Uint32(uint32_t addr);
// "192.168.1.0/24" -> base/mascara. Acepta tambien "192.168.1.42/24"
// (normaliza a red) y "192.168.1.0/255.255.255.0".
bool parseCidr(const std::string& cidr, uint32_t& outNet, uint32_t& outMask);
// Construye lista de objetivos desde CIDR (excluye red/broadcast y
// opcionalmente la IP local). Capada a maxHosts (def 1024).
bool buildTargetsFromCidr(const std::string& cidr,
                          const std::string& excludeIp,
                          std::vector<std::string>& out,
                          size_t maxHosts = 1024);
// Heuristicas TV: vendor OUI o cabecera SERVER SSDP tipica.
bool isTvVendorName(const std::string& vendor);
bool isTvSsdpServer(const std::string& server);

void setLanScannerBackend(ILanScannerBackend* backend);
ILanScannerBackend* getLanScannerBackend();

} // namespace network
} // namespace cbdos
