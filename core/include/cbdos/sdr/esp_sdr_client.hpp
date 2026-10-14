#pragma once

#include "cbdos_build_profile.h"

#if CBDOS_FEATURE_SDR

#include "cbdos/usb_host.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace cbdos {
namespace sdr {

enum class GainMode : uint8_t {
    HardwareAgc = 0,
    Manual      = 1
};

enum class CaptureMode : uint8_t {
    OnChipSpectrum = 0, // Streaming continuo On-Chip SPEC/SPC1 (200+ spectra/s)
    RawIQ          = 1  // Ráfagas crudas CAP16/CAP20
};

enum class FrameType : uint8_t {
    BurstIQ     = 0,
    Spectrum    = 1
};

struct SdrConfig {
    uint32_t    frequencyMHz = 2412;   // 100 - 6000 MHz (RANGE? del firmware S3)
    uint32_t    sampleRate   = 40000000; // S3: 16M, 40M, 80M
    uint8_t     sampleBits   = 8;      // 8 o 10 bits por componente
    uint16_t    fftBins      = 512;    // 256, 512, 1024, 2048
    GainMode    gainMode     = GainMode::HardwareAgc;
    int         gainIndex    = 40;     // 0 - 82 si es Manual (LIMITS? del firmware S3)
    uint32_t    filterBwMHz  = 0;      // 0 = filtro abierto (más ancho); 13 - 69 MHz
    bool        maxHold      = false;
    CaptureMode captureMode  = CaptureMode::OnChipSpectrum;
};

// Límites verificados contra el firmware ESP-SDR del S3 (RANGE? / LIMITS?)
static constexpr uint32_t kSdrMinFreqMHz    = 100;
static constexpr uint32_t kSdrMaxFreqMHz    = 6000;
static constexpr int      kSdrMaxGainIndex  = 82;
static constexpr uint32_t kSdrMinFilterBwMHz = 13;
static constexpr uint32_t kSdrMaxFilterBwMHz = 69;

struct SdrFrame {
    FrameType type          = FrameType::BurstIQ;
    uint32_t  sequence      = 0;
    uint32_t  frequencyMHz  = 2412;
    uint32_t  sampleRate    = 40000000;
    uint16_t  fftBins       = 512;
    uint32_t  captureUs     = 0;
    uint32_t  samples       = 0;
    bool      crcOk         = false;

    // Payload de muestras temporales I/Q normalizadas [-1.0f, +1.0f]
    // Tamaño = samples * 2 (I en índices pares, Q en impares)
    std::vector<float> iqData;

    // Payload de espectro en dBFS (-140 a 0 dBFS)
    // Tamaño = fftBins
    std::vector<float> spectrumDbfs;

    float peakDbfs    = -120.0f;
    float peakFreqMHz = 2412.0f;
};

struct SdrStats {
    uint32_t framesReceived    = 0;
    uint32_t crcErrors         = 0;
    uint32_t droppedCaptures   = 0;
    float    throughputKsps    = 0.0f;
    float    fps               = 0.0f;
    uint32_t lastElapsedMs     = 0;
};

using SdrFrameCallback = std::function<void(const SdrFrame& frame)>;

class EspSdrClient {
public:
    EspSdrClient();
    ~EspSdrClient();

    // Conexión y sincronización
    bool connect();
    void disconnect();
    bool isConnected() const { return m_connected; }

    // Control de radio.
    // NOTA: Estos comandos de texto se rechazan (return false) mientras la tarea de
    // streaming esté viva: sólo ella es propietaria del CDC. Usar la secuencia
    // stopStreaming() -> comando -> startStreaming().
    bool tune(uint32_t freqMHz);
    bool setGain(GainMode mode, int gainIndex = 40);
    bool setFilterBandwidth(uint32_t bwMHz); // 0 = filtro abierto; 13 - 69 MHz
    uint32_t getFrequency() const { return m_currentFreqMHz; }
    GainMode getGainMode() const { return m_gainMode; }
    int getGainIndex() const { return m_gainIndex; }
    uint32_t getFilterBandwidth() const { return m_filterBwMHz; }

    // Adquisición por ráfagas I/Q síncrona
    bool captureBurst(const SdrConfig& cfg, SdrFrame& outFrame, uint32_t timeoutMs = 2500);

    // Streaming asíncrono.
    // startStreaming(): aplica FREQ/GAIN/BANDWIDTH sólo si difieren del estado actual,
    // envía SPEC y valida el ACK de forma síncrona antes de lanzar la tarea.
    // stopStreaming(): bloquea hasta que la tarea drena SPECEND, purga el CDC y termina.
    bool startStreaming(const SdrConfig& cfg, SdrFrameCallback callback);
    void stopStreaming();
    bool isStreaming() const { return m_streaming.load(std::memory_order_acquire); }

    // Telemetría y estadísticas
    const SdrStats& getStats() const { return m_stats; }
    void resetStats();

    // Cálculo auxiliar de CRC32 (polinomio IEEE 802.3 0xEDB88320)
    static uint32_t calculateCrc32(const uint8_t* data, size_t length);

private:
    bool performSync();
    bool queryCaps();
    int getRateDivider(uint32_t rate) const;
    bool sendCommandExpectOk(const char* cmd, uint32_t timeoutMs = 500);
    bool isTaskAlive() const { return m_taskAlive.load(std::memory_order_acquire); }
    bool waitForTaskExit(uint32_t timeoutMs);
    void drainSpecEndAndPurge();

    bool m_connected       = false;
    std::atomic<bool> m_streaming{false}; // Orden de ejecución (lo baja stopStreaming)
    std::atomic<bool> m_taskAlive{false}; // La tarea existe y es propietaria del CDC
    bool m_needsResync     = false;       // Forzar purge + SYNC antes del próximo comando
    uint32_t m_currentFreqMHz = 2412;
    GainMode m_gainMode    = GainMode::HardwareAgc;
    int m_gainIndex        = 40;
    uint32_t m_filterBwMHz = 0;
    bool m_filterBwApplied = false;       // Si el firmware ya recibió un BANDWIDTH explícito
    uint32_t m_sequence    = 0;
    SdrStats m_stats;

    SdrConfig m_streamConfig;
    SdrFrameCallback m_streamCallback = nullptr;
    void* m_streamTaskHandle = nullptr;
    void* m_taskDoneSem      = nullptr;   // SemaphoreHandle_t binario: la tarea lo libera al terminar

    static void streamTaskTrampoline(void* arg);
    void streamTaskLoop();
};

EspSdrClient& getEspSdrClient();

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
