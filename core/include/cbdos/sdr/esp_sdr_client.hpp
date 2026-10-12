#pragma once

#include "cbdos_build_profile.h"

#if CBDOS_FEATURE_SDR

#include "cbdos/usb_host.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace sdr {

enum class GainMode : uint8_t {
    HardwareAgc = 0,
    Manual      = 1
};

enum class FrameType : uint8_t {
    BurstIQ     = 0,
    Spectrum    = 1
};

struct SdrConfig {
    uint32_t frequencyMHz = 2412;   // 2412 - 2484 MHz (o extendido)
    uint32_t sampleRate   = 40000000; // 16M, 40M, 80M
    uint8_t  sampleBits   = 8;      // 8 o 10 bits por componente
    uint16_t fftBins      = 512;    // 256, 512, 1024, 2048
    GainMode gainMode     = GainMode::HardwareAgc;
    int      gainIndex    = 40;     // 0 - 60 si es Manual
    bool     maxHold      = false;
};

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

    // Control de radio
    bool tune(uint32_t freqMHz);
    bool setGain(GainMode mode, int gainIndex = 40);
    uint32_t getFrequency() const { return m_currentFreqMHz; }
    GainMode getGainMode() const { return m_gainMode; }
    int getGainIndex() const { return m_gainIndex; }

    // Adquisición por ráfagas I/Q síncrona
    bool captureBurst(const SdrConfig& cfg, SdrFrame& outFrame, uint32_t timeoutMs = 2500);

    // Streaming asíncrono
    bool startStreaming(const SdrConfig& cfg, SdrFrameCallback callback);
    void stopStreaming();
    bool isStreaming() const { return m_streaming; }

    // Telemetría y estadísticas
    const SdrStats& getStats() const { return m_stats; }
    void resetStats();

    // Cálculo auxiliar de CRC32 (polinomio IEEE 802.3 0xEDB88320)
    static uint32_t calculateCrc32(const uint8_t* data, size_t length);

private:
    bool performSync();
    bool queryCaps();
    int getRateDivider(uint32_t rate) const;

    bool m_connected       = false;
    bool m_streaming       = false;
    uint32_t m_currentFreqMHz = 2412;
    GainMode m_gainMode    = GainMode::HardwareAgc;
    int m_gainIndex        = 40;
    uint32_t m_sequence    = 0;
    SdrStats m_stats;

    SdrConfig m_streamConfig;
    SdrFrameCallback m_streamCallback = nullptr;
    void* m_streamTaskHandle = nullptr;

    static void streamTaskTrampoline(void* arg);
    void streamTaskLoop();
};

EspSdrClient& getEspSdrClient();

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
