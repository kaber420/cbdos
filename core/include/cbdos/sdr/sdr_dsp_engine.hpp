#pragma once

#include "cbdos_build_profile.h"

#if CBDOS_FEATURE_SDR

#include "cbdos/sdr/esp_sdr_client.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>

namespace cbdos {
namespace sdr {

struct DspConfig {
    uint16_t fftBins        = 512;    // 256, 512, 1024, 2048
    float    smoothAlpha    = 0.35f;  // Factor EMA (0.01 - 1.0)
    bool     enableMaxHold  = true;
    float    maxHoldDecayDb = 0.05f;  // Decaimiento dB por trama (0 = sin decaimiento)
};

class SdrDspEngine {
public:
    SdrDspEngine();
    ~SdrDspEngine();

    // Configuración y pre-cálculo de tablas de ventaneado y FFT
    bool configure(const DspConfig& config);
    const DspConfig& getConfig() const { return m_config; }

    // Procesa un SdrFrame con muestras I/Q y rellena el espectro en dBFS,
    // detección de pico y frecuencias.
    bool process(const SdrFrame& inFrame, SdrFrame& outFrame);

    // Procesa directamente un buffer crudo de muestras I/Q normalizadas [-1.0f, +1.0f]
    bool processRaw(const float* iqData, size_t sampleCount,
                    uint32_t sampleRate, uint32_t freqMHz,
                    SdrFrame& outFrame);

    // Obtención de vectores de espectro calculados
    const std::vector<float>& getInstantSpectrum() const { return m_instantSpectrum; }
    const std::vector<float>& getSmoothedSpectrum() const { return m_smoothedSpectrum; }
    const std::vector<float>& getMaxHoldSpectrum() const { return m_maxHoldSpectrum; }

    // Reinicia acumuladores de suavizado y max-hold
    void reset();
    void resetMaxHold();

    // Telemetría de tiempo de cómputo en microsegundos
    uint32_t getLastDspTimeUs() const { return m_lastDspTimeUs; }

private:
    void ensureTables(uint16_t n);
    void computeFft(float* real, float* imag, uint16_t n);

    DspConfig m_config;
    uint16_t  m_allocatedN = 0;

    // Tablas precalculadas
    std::vector<float>    m_hannWindow;
    std::vector<uint16_t> m_bitRev;
    std::vector<float>    m_twiddleCos;
    std::vector<float>    m_twiddleSin;

    // Buffers de trabajo
    std::vector<float>    m_fftReal;
    std::vector<float>    m_fftImag;

    // Vectores de salida
    std::vector<float>    m_instantSpectrum;
    std::vector<float>    m_smoothedSpectrum;
    std::vector<float>    m_maxHoldSpectrum;

    bool     m_hasHistory = false;
    uint32_t m_lastDspTimeUs = 0;
};

// Instancia singleton accesible para el sistema
SdrDspEngine& getSdrDspEngine();

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
