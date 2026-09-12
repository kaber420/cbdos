#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "cbdos/lan_recon.hpp"
#include "cbdos/rtos.hpp"

namespace cbdos {
namespace network {

// ────────────────────────────────────────────────────────────────
// LanScannerService: orquestador de negocio del escaneo LAN.
//
// Fases: Idle -> ArpSweep -> PortScan -> BannerGrab -> Completed
//        (Aborted si se solicita stopScan() o falla el backend).
//
// Singleton thread-safe. El trabajo pesado corre en una tarea de
// fondo anclada a Core 0 ("lan_recon_task", 8192 stack, prio 3) a
// traves de la abstraccion cbdos::rtos. Los resultados y el progreso
// se protegen con cbdos::rtos::MutexHandle.
//
// Pureza core/: solo headers cbdos/* + STL C++17. Cero ESP-IDF,
// Arduino o FreeRTOS directos.
// Ver specs/drafts/BORRADOR_LAN_RECON_Y_ESCANEADOR_RED_LOCAL.md §2-3.
// ────────────────────────────────────────────────────────────────
class LanScannerService {
public:
    static LanScannerService& getInstance();

    LanScannerService(const LanScannerService&) = delete;
    LanScannerService& operator=(const LanScannerService&) = delete;

    // Inicia el escaneo en Core 0. Retorna false si ya hay un
    // escaneo en curso, no hay backend inyectado o no hay red.
    bool startScan();
    // v2: escaneo a CIDR arbitrario ("192.168.10.0/24"), util para
    // redes ruteadas via gateway (solo L3: ICMP+TCP, sin ARP).
    bool startScanCidr(const std::string& cidr);
    // v2: ping bloqueante a una IP (ICMP real + fallback TCP).
    // Retorna true si responde. outRttMs y outMethod opcionales
    // (method: "icmp", "tcp:445", ... o "none").
    bool pingSingle(const std::string& ip, uint32_t timeoutMs,
                    uint32_t* outRttMs = nullptr,
                    std::string* outMethod = nullptr);
    // Ultimo CIDR escaneado (vacio = subred local).
    std::string lastCidr() const;
    // Solicita cancelacion cooperativa (el worker la observa
    // entre cada IP/puerto y transiciona a Aborted).
    void stopScan();

    bool isScanning() const;
    LanScanProgress getProgress() const;
    std::vector<LanHostInfo> getResults() const;

    void setOnHostFound(OnHostFoundCallback cb);
    void setOnProgress(OnProgressCallback cb);
    void setOnScanFinished(OnScanFinishedCallback cb);

    // Inventario forense CSV: "ip,mac,vendor,ports,banner\n".
    std::string exportCsv() const;

private:
    LanScannerService();
    ~LanScannerService();

    static void taskFn(void* param);
    void runScan();

    // v2: nucleo parametrizado por CIDR ("" = subred local).
    bool beginScan(const std::string& cidr);

    // Helpers internos (asumen backend != nullptr).
    void setPhase(LanScanPhase phase, uint8_t percentage);
    void publishProgress(const LanScanProgress& progress);
    void finishScan(LanScanPhase finalPhase);
    // Publica un host hallado (mutex + callbacks + progreso).
    void addFoundHost(const LanHostInfo& host, uint8_t percentage);

    cbdos::rtos::TaskHandle m_taskHandle{nullptr};
    cbdos::rtos::MutexHandle m_mutex{nullptr};

    // Flags atomicos: se leen/escriben desde UI (Core 1) y worker (Core 0).
    std::atomic<bool> m_scanning{false};
    std::atomic<bool> m_abortRequested{false};

    // Estado compartido: SIEMPRE bajo m_mutex.
    std::vector<LanHostInfo> m_results;
    LanScanProgress m_progress{};
    std::string m_cidr;  // v2: objetivo del escaneo en curso

    // Callbacks (UI): se copian bajo mutex y se invocan fuera de el.
    OnHostFoundCallback m_onHostFound;
    OnProgressCallback m_onProgress;
    OnScanFinishedCallback m_onScanFinished;
};

} // namespace network
} // namespace cbdos
