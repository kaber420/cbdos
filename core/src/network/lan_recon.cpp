#include "cbdos/lan_recon.hpp"

#include <cstdio>
#include <cctype>

namespace cbdos {
namespace network {

static ILanScannerBackend* s_lanScannerBackend = nullptr;

void setLanScannerBackend(ILanScannerBackend* backend) {
    s_lanScannerBackend = backend;
}

ILanScannerBackend* getLanScannerBackend() {
    return s_lanScannerBackend;
}

bool LanHostInfo::hasPort(uint16_t port) const {
    const uint32_t bit = lanPortToBit(port);
    if (bit == 0u) {
        return false;
    }
    return (openPortsBitmask & bit) != 0u;
}

bool LanHostInfo::addPort(uint16_t port) {
    const uint32_t bit = lanPortToBit(port);
    if (bit == 0u) {
        return false;
    }
    openPortsBitmask |= bit;
    return true;
}

std::string LanHostInfo::getMacString() const {
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

bool parseIpv4Uint32(const std::string& ip, uint32_t& out) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    char tail = '\0';
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }
    out = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
          (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
    return true;
}

std::string formatIpv4Uint32(uint32_t addr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                  static_cast<unsigned int>((addr >> 24) & 0xFFu),
                  static_cast<unsigned int>((addr >> 16) & 0xFFu),
                  static_cast<unsigned int>((addr >> 8) & 0xFFu),
                  static_cast<unsigned int>(addr & 0xFFu));
    return std::string(buf);
}

namespace {
uint32_t prefixToMask(uint8_t prefix) {
    if (prefix == 0) return 0u;
    if (prefix >= 32) return 0xFFFFFFFFu;
    return 0xFFFFFFFFu << (32 - prefix);
}

std::string trimAscii(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
    size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

std::string toLowerAscii(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char ch : s) o.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    return o;
}
}  // namespace

bool parseCidr(const std::string& cidr, uint32_t& outNet, uint32_t& outMask) {
    const std::string t = trimAscii(cidr);
    const size_t slash = t.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= t.size()) {
        return false;
    }
    const std::string ipPart = trimAscii(t.substr(0, slash));
    const std::string maskPart = trimAscii(t.substr(slash + 1));
    uint32_t ip = 0;
    if (!parseIpv4Uint32(ipPart, ip)) {
        return false;
    }
    uint32_t mask = 0;
    if (maskPart.find('.') != std::string::npos) {
        if (!parseIpv4Uint32(maskPart, mask) || mask == 0) {
            return false;
        }
    } else {
        unsigned prefix = 0;
        char tail = '\0';
        if (std::sscanf(maskPart.c_str(), "%u%c", &prefix, &tail) != 1 || prefix == 0 || prefix > 32) {
            return false;
        }
        // /31 y /32 no tienen hosts utiles para sweep (solo 1-2 IPs).
        // Los aceptamos pero el builder los capara.
        mask = prefixToMask(static_cast<uint8_t>(prefix));
    }
    outMask = mask;
    outNet = ip & mask;
    return true;
}

bool buildTargetsFromCidr(const std::string& cidr,
                          const std::string& excludeIp,
                          std::vector<std::string>& out,
                          size_t maxHosts) {
    out.clear();
    uint32_t net = 0, mask = 0;
    if (!parseCidr(cidr, net, mask)) {
        return false;
    }
    const uint32_t bcast = net | ~mask;
    if (bcast <= net + 1) {
        return false;  // /31//32 sin rango util
    }
    uint32_t exclude = 0;
    const bool hasExclude = !excludeIp.empty() && parseIpv4Uint32(excludeIp, exclude);
    for (uint32_t addr = net + 1; addr < bcast && out.size() < maxHosts; ++addr) {
        if (hasExclude && addr == exclude) {
            continue;
        }
        out.push_back(formatIpv4Uint32(addr));
    }
    return !out.empty();
}

bool isTvVendorName(const std::string& vendor) {
    const std::string v = toLowerAscii(vendor);
    if (v.empty() || v == "unknown") return false;
    return v.find("samsung") != std::string::npos ||
           v.find("lg") != std::string::npos ||
           v.find("sony") != std::string::npos ||
           v.find("roku") != std::string::npos ||
           v.find("tcl") != std::string::npos ||
           v.find("hisense") != std::string::npos ||
           v.find("vizio") != std::string::npos ||
           v.find("philips") != std::string::npos ||
           v.find("panasonic") != std::string::npos;
}

bool isTvSsdpServer(const std::string& server) {
    const std::string s = toLowerAscii(server);
    if (s.empty()) return false;
    return s.find("dlna") != std::string::npos ||
           s.find("dms") != std::string::npos ||
           s.find("dmp") != std::string::npos ||
           s.find("chromecast") != std::string::npos ||
           s.find("google cast") != std::string::npos ||
           s.find("roku") != std::string::npos ||
           s.find("samsung") != std::string::npos ||
           s.find("lg") != std::string::npos ||
           s.find("sony") != std::string::npos ||
           s.find("upnp") != std::string::npos ||
           s.find("mediaserver") != std::string::npos ||
           s.find("tizen") != std::string::npos ||
           s.find("webos") != std::string::npos;
}

} // namespace network
} // namespace cbdos
