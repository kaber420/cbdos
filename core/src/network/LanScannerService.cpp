#include "LanScannerService.hpp"

#include "OuiDatabase.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace cbdos {
namespace network {

namespace {

// ── Helpers IPv4 puros (sin sockets ni LwIP) ──

bool parseIpv4(const std::string& ip, uint32_t& out) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    char tail = '\0';
    // %c final detecta basura trailing ("1.2.3.4x").
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

std::string formatIpv4(uint32_t addr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                  static_cast<unsigned int>((addr >> 24) & 0xFFu),
                  static_cast<unsigned int>((addr >> 16) & 0xFFu),
                  static_cast<unsigned int>((addr >> 8) & 0xFFu),
                  static_cast<unsigned int>(addr & 0xFFu));
    return std::string(buf);
}

// Escapa un campo CSV segun RFC 4180.
std::string csvEscape(const std::string& field) {
    const bool needsQuotes = field.find_first_of(",\"\n\r") != std::string::npos;
    if (!needsQuotes) {
        return field;
    }
    std::string out;
    out.reserve(field.size() + 2);
    out.push_back('"');
    for (char ch : field) {
        if (ch == '"') {
            out.push_back('"');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

bool isHttpPort(uint16_t port) {
    return port == 80 || port == 8080 || port == 443 || port == 8443;
}

} // namespace

LanScannerService& LanScannerService::getInstance() {
    static LanScannerService instance;
    return instance;
}

LanScannerService::LanScannerService() {
    m_mutex = cbdos::rtos::createMutex();
    m_progress.phase = LanScanPhase::Idle;
    m_progress.percentage = 0;
    m_progress.hostsDiscovered = 0;
    m_progress.currentHostIndex = 0;
}

LanScannerService::~LanScannerService() {
    m_abortRequested.store(true, std::memory_order_relaxed);
    if (m_mutex) {
        cbdos::rtos::deleteMutex(m_mutex);
        m_mutex = nullptr;
    }
}

bool LanScannerService::startScan() {
    return beginScan(std::string());
}

bool LanScannerService::startScanCidr(const std::string& cidr) {
    uint32_t net = 0, mask = 0;
    if (!parseCidr(cidr, net, mask)) {
        return false;
    }
    return beginScan(cidr);
}

bool LanScannerService::beginScan(const std::string& cidr) {
    bool expected = false;
    if (!m_scanning.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
        return false; // Ya hay un escaneo en curso.
    }

    ILanScannerBackend* backend = getLanScannerBackend();
    if (backend == nullptr || !backend->isNetworkConnected()) {
        m_scanning.store(false, std::memory_order_release);
        return false;
    }

    m_abortRequested.store(false, std::memory_order_relaxed);

    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_results.clear();
        m_progress.phase = LanScanPhase::Idle;
        m_progress.percentage = 0;
        m_progress.hostsDiscovered = 0;
        m_progress.currentHostIndex = 0;
        m_cidr = cidr;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_results.clear();
        m_progress = LanScanProgress{};
        m_cidr = cidr;
    }

    m_taskHandle = cbdos::rtos::createTask(taskFn, "lan_recon_task", 8192,
                                           this, 3, 0);
    // Nota: el stub debil de cbdos_core ejecuta fn() en linea y retorna
    // nullptr; en ese caso el escaneo ya termino de forma sincrona
    // (m_scanning == false). Solo es fallo real si el worker no corrio.
    if (m_taskHandle == nullptr &&
        m_scanning.load(std::memory_order_acquire)) {
        m_scanning.store(false, std::memory_order_release);
        return false;
    }
    return true;
}

std::string LanScannerService::lastCidr() const {
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        const std::string copy = m_cidr;
        cbdos::rtos::unlockMutex(m_mutex);
        return copy;
    }
    return m_cidr;
}

bool LanScannerService::pingSingle(const std::string& ip, uint32_t timeoutMs,
                                   uint32_t* outRttMs,
                                   std::string* outMethod) {
    if (outRttMs) {
        *outRttMs = 0;
    }
    if (outMethod) {
        outMethod->assign("none");
    }
    if (ip.empty()) {
        return false;
    }
    uint32_t dummy = 0;
    if (!parseIpv4(ip, dummy)) {
        return false;
    }
    ILanScannerBackend* backend = getLanScannerBackend();
    if (backend == nullptr || !backend->isNetworkConnected()) {
        return false;
    }
    if (timeoutMs == 0) {
        timeoutMs = 800;
    }
    if (timeoutMs > 5000) {
        timeoutMs = 5000;
    }
    // 1) ICMP real primero (rapido y fiable en LAN).
    uint32_t rtt = 0;
    const uint32_t icmpBudget = timeoutMs >= 400 ? timeoutMs / 2 : timeoutMs;
    if (backend->pingIcmp(ip, icmpBudget, &rtt)) {
        if (outRttMs) {
            *outRttMs = rtt;
        }
        if (outMethod) {
            outMethod->assign("icmp");
        }
        return true;
    }
    // 2) Fallback TCP a puertos tipicos (PCs/TVs con ICMP filtrado).
    static const uint16_t kFallback[] = {445, 80, 8008, 22, 443};
    const uint32_t perPort = (timeoutMs - icmpBudget) / 5 + 60;
    for (uint16_t port : kFallback) {
        uint32_t t0 = 0;  // sin reloj portable en core: RTT aproximado no critico
        (void)t0;
        if (backend->probeTcpPort(ip, port, perPort > 300 ? 300 : perPort)) {
            if (outRttMs) {
                *outRttMs = 0;
            }
            if (outMethod) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "tcp:%u", static_cast<unsigned>(port));
                outMethod->assign(buf);
            }
            return true;
        }
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            break;
        }
    }
    // 3) Ultimo recurso: ARP (solo red local).
    uint8_t mac[6] = {0};
    if (backend->resolveMacArp(ip, mac)) {
        if (outMethod) {
            outMethod->assign("arp");
        }
        return true;
    }
    return false;
}

void LanScannerService::stopScan() {
    m_abortRequested.store(true, std::memory_order_relaxed);
}

bool LanScannerService::isScanning() const {
    return m_scanning.load(std::memory_order_acquire);
}

LanScanProgress LanScannerService::getProgress() const {
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        const LanScanProgress copy = m_progress;
        cbdos::rtos::unlockMutex(m_mutex);
        return copy;
    }
    return m_progress;
}

std::vector<LanHostInfo> LanScannerService::getResults() const {
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        const std::vector<LanHostInfo> copy = m_results;
        cbdos::rtos::unlockMutex(m_mutex);
        return copy;
    }
    return m_results;
}

void LanScannerService::setOnHostFound(OnHostFoundCallback cb) {
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_onHostFound = std::move(cb);
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_onHostFound = std::move(cb);
    }
}

void LanScannerService::setOnProgress(OnProgressCallback cb) {
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_onProgress = std::move(cb);
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_onProgress = std::move(cb);
    }
}

void LanScannerService::setOnScanFinished(OnScanFinishedCallback cb) {
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_onScanFinished = std::move(cb);
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_onScanFinished = std::move(cb);
    }
}

std::string LanScannerService::exportCsv() const {
    const std::vector<LanHostInfo> snapshot = getResults();
    std::ostringstream out;
    out << "ip,mac,vendor,ports,rtt_ms,discovery,is_tv,banner\n";
    for (const auto& host : snapshot) {
        std::string ports;
        for (std::size_t i = 0; i < kLanReconPortCount; ++i) {
            if (host.hasPort(kLanReconPorts[i])) {
                if (!ports.empty()) {
                    ports.push_back(';');
                }
                ports += std::to_string(kLanReconPorts[i]);
            }
        }
        out << csvEscape(host.ip) << ',' << csvEscape(host.getMacString())
            << ',' << csvEscape(host.vendor) << ',' << csvEscape(ports) << ','
            << host.rttMs << ',' << csvEscape(host.discovery) << ','
            << (host.isTv ? "1" : "0") << ',' << csvEscape(host.banner) << '\n';
    }
    return out.str();
}

void LanScannerService::taskFn(void* param) {
    auto* self = static_cast<LanScannerService*>(param);
    if (self) {
        self->runScan();
    }
    // FreeRTOS aborta ("should not return") si la funcion de tarea
    // retorna: auto-eliminar siguiendo la convencion del resto del
    // proyecto (WavRecorder, AudioPlayer, TlvBrowserView...).
    // En el stub debil de host (tests) es no-op.
    cbdos::rtos::deleteTask(nullptr);
}

void LanScannerService::setPhase(LanScanPhase phase, uint8_t percentage) {
    LanScanProgress snapshot{};
    OnProgressCallback cb;
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_progress.phase = phase;
        m_progress.percentage = percentage;
        snapshot = m_progress;
        cb = m_onProgress;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_progress.phase = phase;
        m_progress.percentage = percentage;
        snapshot = m_progress;
        cb = m_onProgress;
    }
    if (cb) {
        cb(snapshot);
    }
}

void LanScannerService::publishProgress(const LanScanProgress& progress) {
    OnProgressCallback cb;
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_progress = progress;
        cb = m_onProgress;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_progress = progress;
        cb = m_onProgress;
    }
    if (cb) {
        cb(progress);
    }
}

void LanScannerService::finishScan(LanScanPhase finalPhase) {
    std::vector<LanHostInfo> snapshot;
    OnScanFinishedCallback finishedCb;
    LanScanProgress snapshotProgress{};
    OnProgressCallback progressCb;
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_progress.phase = finalPhase;
        if (finalPhase == LanScanPhase::Completed) {
            m_progress.percentage = 100;
        }
        snapshot = m_results;
        snapshotProgress = m_progress;
        finishedCb = m_onScanFinished;
        progressCb = m_onProgress;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_progress.phase = finalPhase;
        if (finalPhase == LanScanPhase::Completed) {
            m_progress.percentage = 100;
        }
        snapshot = m_results;
        snapshotProgress = m_progress;
        finishedCb = m_onScanFinished;
        progressCb = m_onProgress;
    }
    if (progressCb) {
        progressCb(snapshotProgress);
    }
    m_scanning.store(false, std::memory_order_release);
    m_taskHandle = nullptr;
    if (finishedCb) {
        finishedCb(snapshot);
    }
}

void LanScannerService::addFoundHost(const LanHostInfo& host, uint8_t percentage) {
    OnHostFoundCallback hostCb;
    LanScanProgress snapshot{};
    OnProgressCallback progressCb;
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_results.push_back(host);
        m_progress.hostsDiscovered =
            static_cast<uint16_t>(m_results.size());
        m_progress.currentHostIndex =
            static_cast<uint16_t>(m_results.size());
        m_progress.percentage = percentage;
        snapshot = m_progress;
        hostCb = m_onHostFound;
        progressCb = m_onProgress;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_results.push_back(host);
        m_progress.hostsDiscovered =
            static_cast<uint16_t>(m_results.size());
        m_progress.currentHostIndex =
            static_cast<uint16_t>(m_results.size());
        m_progress.percentage = percentage;
        snapshot = m_progress;
        hostCb = m_onHostFound;
        progressCb = m_onProgress;
    }
    if (hostCb) {
        hostCb(host);
    }
    if (progressCb) {
        progressCb(snapshot);
    }
}

void LanScannerService::runScan() {
    ILanScannerBackend* backend = getLanScannerBackend();
    if (backend == nullptr) {
        finishScan(LanScanPhase::Aborted);
        return;
    }

    // ── Objetivos: CIDR explicito (red ruteada) o subred local ──
    std::string cidrCopy;
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        cidrCopy = m_cidr;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        cidrCopy = m_cidr;
    }
    const std::string localIp = backend->getLocalIp();

    std::vector<std::string> targets;
    bool isRouted = false;
    if (!cidrCopy.empty()) {
        // Red arbitraria: puede ser ruteada via gateway (solo L3).
        if (!buildTargetsFromCidr(cidrCopy, localIp, targets, 1024)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
        uint32_t reqNet = 0, reqMask = 0;
        uint32_t localAddr = 0, localMask = 0;
        std::string maskStr = backend->getSubnetMask();
        if (parseCidr(cidrCopy, reqNet, reqMask) && parseIpv4(localIp, localAddr) &&
            parseIpv4(maskStr.empty() ? "255.255.255.0" : maskStr, localMask)) {
            const uint32_t localNet = localAddr & localMask;
            isRouted = (reqNet != localNet);
        } else {
            isRouted = true;
        }
    } else {
        // Subred local derivada de IP+mascara.
        std::string maskStr = backend->getSubnetMask();
        if (maskStr.empty()) {
            maskStr = "255.255.255.0";
        }
        uint32_t localAddr = 0;
        uint32_t maskAddr = 0;
        if (!parseIpv4(localIp, localAddr) || localAddr == 0) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
        if (!parseIpv4(maskStr, maskAddr) || maskAddr == 0) {
            maskAddr = 0xFFFFFF00u; // fallback /24
        }
        const uint32_t netAddr = localAddr & maskAddr;
        const uint32_t bcastAddr = netAddr | ~maskAddr;
        if (bcastAddr > netAddr + 1) {
            const uint32_t hostCount = bcastAddr - netAddr - 1;
            targets.reserve(hostCount > 1024 ? 1024 : hostCount);
            for (uint32_t addr = netAddr + 1;
                 addr < bcastAddr && targets.size() < 1024; ++addr) {
                if (addr == localAddr) {
                    continue; // Omitir la IP propia.
                }
                targets.push_back(formatIpv4(addr));
            }
        }
    }
    if (targets.empty()) {
        finishScan(LanScanPhase::Aborted);
        return;
    }

    setPhase(LanScanPhase::ArpSweep, 0);

    // ── v2.2 Fase 1: blast ──
    // Disparo ARP fire-and-forget a toda la subred (microsegundos por
    // host): las respuestas llegan en paralelo a la pila lwIP y la
    // recoleccion posterior las lee de cache. Sin UDP bloqueante aqui
    // (su recv de 150ms por host sumaba ~38s muertos por /24).
    if (!isRouted) {
        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (m_abortRequested.load(std::memory_order_relaxed)) {
                finishScan(LanScanPhase::Aborted);
                return;
            }
            backend->arpProbe(targets[i]);
            if ((i & 31) == 31) {
                setPhase(LanScanPhase::ArpSweep,
                         static_cast<uint8_t>((i + 1) * 6 / targets.size()));
            }
        }
    }
    // Espera global a respuestas ARP (en rebanadas p/cancelar).
    for (int w = 0; w < 10; ++w) {
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
        cbdos::rtos::sleepMs(50);
    }

    // ── v2.2 Pasada A: solo cache ARP (casi instantanea) ──
    // Tras el blast, lo vivo en LAN ya esta en cache: TVs/PCs aparecen
    // aqui en ~1-2s sin pagar ningun timeout.
    std::vector<uint8_t> foundIdx(targets.size(), 0);
    if (!isRouted) {
        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (m_abortRequested.load(std::memory_order_relaxed)) {
                finishScan(LanScanPhase::Aborted);
                return;
            }
            uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
            if (!backend->lookupArpCacheOnly(targets[i], mac)) {
                continue;
            }
            LanHostInfo host;
            host.ip = targets[i];
            for (int b = 0; b < 6; ++b) {
                host.mac[b] = mac[b];
            }
            host.vendor = lookupVendorByMac(mac);
            host.alive = true;
            host.discovery = "arp";
            host.isTv = isTvVendorName(host.vendor);
            foundIdx[i] = 1;
            const uint8_t pctA = static_cast<uint8_t>(
                6 + ((i + 1) * 9) / (targets.size() > 0 ? targets.size() : 1));
            addFoundHost(host, pctA > 15 ? 15 : pctA);
        }
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
    }

    // ── v2.2 Pasada B: ICMP + TCP a los restantes ──
    // Solo las IPs no vistas en cache pagan timeouts. En red ruteada
    // es la unica via (sin ARP).
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
        if (foundIdx[i]) {
            continue;
        }
        const std::string& ip = targets[i];

        uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
        bool alive = false;
        bool arpOk = false;
        uint32_t rtt = 0;
        std::string how;
        if (backend->pingIcmp(ip, 120, &rtt)) {
            alive = true;
            how = "icmp";
            if (!isRouted) {
                arpOk = backend->lookupArpCacheOnly(ip, mac);
            }
        } else {
            static const uint16_t kLive[] = {445, 80, 8008};
            for (uint16_t lp : kLive) {
                if (m_abortRequested.load(std::memory_order_relaxed)) {
                    break;
                }
                if (backend->probeTcpPort(ip, lp, 110)) {
                    alive = true;
                    how = "tcp";
                    if (!isRouted) {
                        arpOk = backend->lookupArpCacheOnly(ip, mac);
                    }
                    break;
                }
            }
        }
        if (!alive) {
            continue;
        }

        LanHostInfo host;
        host.ip = ip;
        host.mac[0] = mac[0];
        host.mac[1] = mac[1];
        host.mac[2] = mac[2];
        host.mac[3] = mac[3];
        host.mac[4] = mac[4];
        host.mac[5] = mac[5];
        if (isRouted && !arpOk) {
            host.vendor = "Routed";
        } else {
            host.vendor = lookupVendorByMac(mac);
        }
        host.banner.clear();
        host.openPortsBitmask = 0;
        host.alive = true;
        host.rttMs = rtt;
        host.discovery = how.empty() ? (arpOk ? "arp" : "tcp") : how;
        host.isTv = isTvVendorName(host.vendor);

        const uint8_t pctB = static_cast<uint8_t>(
            15 + ((i + 1) * 35) / (targets.size() > 0 ? targets.size() : 1));
        addFoundHost(host, pctB > 50 ? 50 : pctB);
    }

    if (m_abortRequested.load(std::memory_order_relaxed)) {
        finishScan(LanScanPhase::Aborted);
        return;
    }

    // ── v2 Fase 1b: SSDP (solo red local; el multicast no cruza router) ──
    // Caza SmartTVs, impresoras e IoT que no abren TCP ni responden
    // ICMP pero anuncian UPnP/DLNA. Fusiona por IP con lo ya hallado.
    if (!isRouted) {
        setPhase(LanScanPhase::ArpSweep, 55);
        backend->ssdpScan(2500, [&](const SsdpDeviceInfo& dev) {
            if (dev.ip.empty()) {
                return;
            }
            if (m_mutex) {
                cbdos::rtos::lockMutex(m_mutex);
                for (auto& entry : m_results) {
                    if (entry.ip == dev.ip) {
                        entry.ssdpServer = dev.server;
                        entry.ssdpLocation = dev.location;
                        if (isTvSsdpServer(dev.server)) {
                            entry.isTv = true;
                        }
                        cbdos::rtos::unlockMutex(m_mutex);
                        return;
                    }
                }
                // Nuevo host solo visto por SSDP.
                LanHostInfo host;
                host.ip = dev.ip;
                host.vendor = "SSDP";
                host.alive = true;
                host.discovery = "ssdp";
                host.ssdpServer = dev.server;
                host.ssdpLocation = dev.location;
                host.isTv = isTvSsdpServer(dev.server);
                uint8_t mac[6] = {0};
                if (backend->resolveMacArp(dev.ip, mac)) {
                    for (int b = 0; b < 6; ++b) {
                        host.mac[b] = mac[b];
                    }
                    host.vendor = lookupVendorByMac(mac);
                    if (isTvVendorName(host.vendor)) {
                        host.isTv = true;
                    }
                }
                m_results.push_back(host);
                m_progress.hostsDiscovered =
                    static_cast<uint16_t>(m_results.size());
                cbdos::rtos::unlockMutex(m_mutex);
            }
        });
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
    }

    // ── Fase 2: PortScan ──
    setPhase(LanScanPhase::PortScan, 60);

    std::size_t hostCount = 0;
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        hostCount = m_results.size();
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        hostCount = m_results.size();
    }

    const std::size_t totalProbes =
        hostCount * kLanReconPortCount > 0 ? hostCount * kLanReconPortCount : 1;
    std::size_t doneProbes = 0;

    for (std::size_t h = 0; h < hostCount; ++h) {
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
        std::string hostIp;
        if (m_mutex) {
            cbdos::rtos::lockMutex(m_mutex);
            if (h < m_results.size()) {
                hostIp = m_results[h].ip;
            }
            cbdos::rtos::unlockMutex(m_mutex);
        } else if (h < m_results.size()) {
            hostIp = m_results[h].ip;
        }
        if (hostIp.empty()) {
            doneProbes += kLanReconPortCount;
            continue;
        }

        for (std::size_t p = 0; p < kLanReconPortCount; ++p) {
            if (m_abortRequested.load(std::memory_order_relaxed)) {
                finishScan(LanScanPhase::Aborted);
                return;
            }
            const uint16_t port = kLanReconPorts[p];
            bool open = backend->probeTcpPort(hostIp, port, 180);
            if (open) {
                if (m_mutex) {
                    cbdos::rtos::lockMutex(m_mutex);
                    for (auto& entry : m_results) {
                        if (entry.ip == hostIp) {
                            entry.addPort(port);
                            break;
                        }
                    }
                    cbdos::rtos::unlockMutex(m_mutex);
                } else {
                    for (auto& entry : m_results) {
                        if (entry.ip == hostIp) {
                            entry.addPort(port);
                            break;
                        }
                    }
                }
            }
            ++doneProbes;
            const uint8_t pct =
                static_cast<uint8_t>(60 + (doneProbes * 25) / totalProbes);
            LanScanProgress snapshot{};
            OnProgressCallback progressCb;
            if (m_mutex) {
                cbdos::rtos::lockMutex(m_mutex);
                m_progress.percentage = pct > 85 ? 85 : pct;
                m_progress.currentHostIndex =
                    static_cast<uint16_t>(h + 1);
                snapshot = m_progress;
                progressCb = m_onProgress;
                cbdos::rtos::unlockMutex(m_mutex);
            } else {
                m_progress.percentage = pct;
                snapshot = m_progress;
                progressCb = m_onProgress;
            }
            if (progressCb) {
                progressCb(snapshot);
            }
        }
    }

    if (m_abortRequested.load(std::memory_order_relaxed)) {
        finishScan(LanScanPhase::Aborted);
        return;
    }

    // ── v2: marcar TVs por puertos de cast tras el PortScan ──
    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        for (auto& entry : m_results) {
            if (!entry.isTv &&
                (entry.hasPort(8008) || entry.hasPort(8009) ||
                 entry.hasPort(1400) || entry.hasPort(7000))) {
                entry.isTv = true;
            }
            if (!entry.isTv && isTvVendorName(entry.vendor)) {
                entry.isTv = true;
            }
        }
        cbdos::rtos::unlockMutex(m_mutex);
    }

    // ── Fase 3: BannerGrab ──
    setPhase(LanScanPhase::BannerGrab, 85);

    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        hostCount = m_results.size();
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        hostCount = m_results.size();
    }

    for (std::size_t h = 0; h < hostCount; ++h) {
        if (m_abortRequested.load(std::memory_order_relaxed)) {
            finishScan(LanScanPhase::Aborted);
            return;
        }
        std::string hostIp;
        bool wantSsh = false;
        bool wantTelnet = false;
        uint16_t httpPort = 0;
        if (m_mutex) {
            cbdos::rtos::lockMutex(m_mutex);
            if (h < m_results.size()) {
                const LanHostInfo& entry = m_results[h];
                hostIp = entry.ip;
                wantSsh = entry.hasPort(22);
                wantTelnet = !wantSsh && entry.hasPort(23);
                for (std::size_t i = 0; i < kLanReconPortCount; ++i) {
                    const uint16_t port = kLanReconPorts[i];
                    if (isHttpPort(port) && entry.hasPort(port)) {
                        httpPort = port;
                        break;
                    }
                }
            }
            cbdos::rtos::unlockMutex(m_mutex);
        } else if (h < m_results.size()) {
            const LanHostInfo& entry = m_results[h];
            hostIp = entry.ip;
            wantSsh = entry.hasPort(22);
            wantTelnet = !wantSsh && entry.hasPort(23);
            for (std::size_t i = 0; i < kLanReconPortCount; ++i) {
                const uint16_t port = kLanReconPorts[i];
                if (isHttpPort(port) && entry.hasPort(port)) {
                    httpPort = port;
                    break;
                }
            }
        }
        if (hostIp.empty() || (!wantSsh && !wantTelnet && httpPort == 0)) {
            continue;
        }

        std::string banner;
        if (wantSsh) {
            banner = backend->grabTcpBanner(hostIp, 22, 500);
        } else if (wantTelnet) {
            banner = backend->grabTcpBanner(hostIp, 23, 500);
        } else {
            banner = backend->fetchHttpTitle(hostIp, httpPort, 800);
        }
        if (!banner.empty()) {
            if (m_mutex) {
                cbdos::rtos::lockMutex(m_mutex);
                for (auto& entry : m_results) {
                    if (entry.ip == hostIp) {
                        entry.banner = banner;
                        break;
                    }
                }
                cbdos::rtos::unlockMutex(m_mutex);
            } else {
                for (auto& entry : m_results) {
                    if (entry.ip == hostIp) {
                        entry.banner = banner;
                        break;
                    }
                }
            }
        }

        const uint8_t pct = static_cast<uint8_t>(
            85 + ((h + 1) * 14) / (hostCount > 0 ? hostCount : 1));
        LanScanProgress snapshot{};
        OnProgressCallback progressCb;
        if (m_mutex) {
            cbdos::rtos::lockMutex(m_mutex);
            m_progress.percentage = pct > 99 ? 99 : pct;
            m_progress.currentHostIndex = static_cast<uint16_t>(h + 1);
            snapshot = m_progress;
            progressCb = m_onProgress;
            cbdos::rtos::unlockMutex(m_mutex);
        } else {
            m_progress.percentage = pct;
            snapshot = m_progress;
            progressCb = m_onProgress;
        }
        if (progressCb) {
            progressCb(snapshot);
        }
    }

    if (m_abortRequested.load(std::memory_order_relaxed)) {
        finishScan(LanScanPhase::Aborted);
        return;
    }

    // ── Fase 4: Completed ──
    finishScan(LanScanPhase::Completed);
}

} // namespace network
} // namespace cbdos
