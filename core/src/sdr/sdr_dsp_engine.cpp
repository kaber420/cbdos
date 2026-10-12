#include "cbdos/sdr/sdr_dsp_engine.hpp"

#if CBDOS_FEATURE_SDR

#include "cbdos/system.hpp"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace cbdos {
namespace sdr {

namespace {

static constexpr float PI_F = 3.14159265358979323846f;

// Verifica si un número es potencia de 2
static inline bool isPowerOfTwo(uint16_t x) {
    return (x > 0) && ((x & (x - 1)) == 0);
}

// Redondea a la potencia de 2 más cercana válida (entre 256 y 2048)
static inline uint16_t clampFftSize(uint16_t n) {
    if (n <= 256) return 256;
    if (n <= 512) return 512;
    if (n <= 1024) return 1024;
    return 2048;
}

} // anonymous namespace

SdrDspEngine::SdrDspEngine() {
    configure(m_config);
}

SdrDspEngine::~SdrDspEngine() {
}

bool SdrDspEngine::configure(const DspConfig& config) {
    m_config = config;
    m_config.fftBins = clampFftSize(m_config.fftBins);
    if (m_config.smoothAlpha <= 0.0f) m_config.smoothAlpha = 0.01f;
    if (m_config.smoothAlpha > 1.0f)  m_config.smoothAlpha = 1.0f;
    if (m_config.maxHoldDecayDb < 0.0f) m_config.maxHoldDecayDb = 0.0f;

    ensureTables(m_config.fftBins);
    reset();
    return true;
}

void SdrDspEngine::ensureTables(uint16_t n) {
    if (m_allocatedN == n) return;

    m_allocatedN = n;
    m_hannWindow.resize(n);
    m_bitRev.resize(n);
    m_twiddleCos.resize(n / 2);
    m_twiddleSin.resize(n / 2);

    m_fftReal.resize(n);
    m_fftImag.resize(n);

    m_instantSpectrum.resize(n, -120.0f);
    m_smoothedSpectrum.resize(n, -120.0f);
    m_maxHoldSpectrum.resize(n, -120.0f);

    // 1. Precalcular ventana Hann periódica normalizada
    for (uint16_t i = 0; i < n; i++) {
        m_hannWindow[i] = 0.5f - 0.5f * cosf((2.0f * PI_F * (float)i) / (float)(n - 1));
    }

    // 2. Precalcular tabla de inversión de bits
    int bits = 0;
    while ((1U << bits) < n) bits++;

    for (uint16_t i = 0; i < n; i++) {
        uint16_t rev = 0;
        uint16_t val = i;
        for (int b = 0; b < bits; b++) {
            rev = (rev << 1) | (val & 1);
            val >>= 1;
        }
        m_bitRev[i] = rev;
    }

    // 3. Precalcular factores twiddle W_N^k = e^(-j 2pi k / N)
    for (uint16_t k = 0; k < n / 2; k++) {
        float angle = -2.0f * PI_F * (float)k / (float)n;
        m_twiddleCos[k] = cosf(angle);
        m_twiddleSin[k] = sinf(angle);
    }
}

void SdrDspEngine::computeFft(float* real, float* imag, uint16_t n) {
    // Permutación bit-reversal
    for (uint16_t i = 0; i < n; i++) {
        uint16_t j = m_bitRev[i];
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

    // Etapas Cooley-Tukey Radix-2 DIT
    for (uint16_t len = 2; len <= n; len <<= 1) {
        uint16_t half = len >> 1;
        uint16_t step = n / len;

        for (uint16_t i = 0; i < n; i += len) {
            for (uint16_t j = 0; j < half; j++) {
                uint16_t k = j * step;
                float c = m_twiddleCos[k];
                float s = m_twiddleSin[k];

                float uRe = real[i + j];
                float uIm = imag[i + j];

                float vRe = real[i + j + half] * c - imag[i + j + half] * s;
                float vIm = real[i + j + half] * s + imag[i + j + half] * c;

                real[i + j]        = uRe + vRe;
                imag[i + j]        = uIm + vIm;
                real[i + j + half] = uRe - vRe;
                imag[i + j + half] = uIm - vIm;
            }
        }
    }
}

bool SdrDspEngine::process(const SdrFrame& inFrame, SdrFrame& outFrame) {
    if (inFrame.iqData.empty() || inFrame.samples == 0) return false;

    return processRaw(inFrame.iqData.data(), inFrame.samples,
                      inFrame.sampleRate, inFrame.frequencyMHz,
                      outFrame);
}

bool SdrDspEngine::processRaw(const float* iqData, size_t sampleCount,
                              uint32_t sampleRate, uint32_t freqMHz,
                              SdrFrame& outFrame) {
    if (!iqData || sampleCount == 0) return false;

    uint64_t tStartUs = cbdos::system::getTimeUs();
    uint16_t n = m_config.fftBins;
    if (sampleCount < n) {
        n = clampFftSize((uint16_t)sampleCount);
    }
    ensureTables(n);

    // Ganancia coherente de ventana Hann = 0.5 * N -> factor de escala 1 / (N / 2)
    const float normScale = 1.0f / (float)(n / 2);

    // 1. Aplicar ventana Hann a las muestras I/Q de entrada
    for (uint16_t i = 0; i < n; i++) {
        float w = m_hannWindow[i];
        m_fftReal[i] = iqData[i * 2] * w;
        m_fftImag[i] = iqData[i * 2 + 1] * w;
    }

    // 2. Ejecutar FFT Radix-2 en punto flotante
    computeFft(m_fftReal.data(), m_fftImag.data(), n);

    // 3. FFT Shift y conversión a dBFS
    uint16_t half = n / 2;
    float peakDb = -140.0f;
    uint16_t peakIdx = 0;

    for (uint16_t i = 0; i < n; i++) {
        // Mapeo fftshift: mitad izquierda = frecuencias negativas [half..n-1]
        //                 mitad derecha   = frecuencias positivas [0..half-1]
        uint16_t srcBin = (i < half) ? (i + half) : (i - half);
        float re = m_fftReal[srcBin] * normScale;
        float im = m_fftImag[srcBin] * normScale;
        float pwr = re * re + im * im;

        float dbfs = 10.0f * log10f(pwr + 1e-12f);
        if (dbfs > 0.0f) dbfs = 0.0f;
        if (dbfs < -140.0f) dbfs = -140.0f;

        m_instantSpectrum[i] = dbfs;

        if (!m_hasHistory) {
            m_smoothedSpectrum[i] = dbfs;
            m_maxHoldSpectrum[i]  = dbfs;
        } else {
            // Suavizado EMA
            float alpha = m_config.smoothAlpha;
            m_smoothedSpectrum[i] = alpha * dbfs + (1.0f - alpha) * m_smoothedSpectrum[i];

            // Retención de pico (Max-Hold)
            if (m_config.enableMaxHold) {
                float decayed = m_maxHoldSpectrum[i] - m_config.maxHoldDecayDb;
                m_maxHoldSpectrum[i] = std::max(dbfs, decayed);
            } else {
                m_maxHoldSpectrum[i] = dbfs;
            }
        }

        if (dbfs > peakDb) {
            peakDb = dbfs;
            peakIdx = i;
        }
    }
    m_hasHistory = true;

    // 4. Calcular métricas de frecuencia
    float spanMHz = (float)sampleRate / 1000000.0f;
    float startFreqMHz = (float)freqMHz - (spanMHz / 2.0f);
    float stepMHz = spanMHz / (float)n;
    float peakFreq = startFreqMHz + (float)peakIdx * stepMHz;

    // 5. Poblar trama de salida
    outFrame.type         = FrameType::Spectrum;
    outFrame.frequencyMHz = freqMHz;
    outFrame.sampleRate   = sampleRate;
    outFrame.fftBins      = n;
    outFrame.samples      = (uint32_t)sampleCount;
    outFrame.crcOk        = true;
    outFrame.peakDbfs     = peakDb;
    outFrame.peakFreqMHz  = peakFreq;
    outFrame.spectrumDbfs = m_instantSpectrum;

    m_lastDspTimeUs = (uint32_t)(cbdos::system::getTimeUs() - tStartUs);
    return true;
}

void SdrDspEngine::reset() {
    m_hasHistory = false;
    std::fill(m_instantSpectrum.begin(), m_instantSpectrum.end(), -120.0f);
    std::fill(m_smoothedSpectrum.begin(), m_smoothedSpectrum.end(), -120.0f);
    std::fill(m_maxHoldSpectrum.begin(), m_maxHoldSpectrum.end(), -120.0f);
}

void SdrDspEngine::resetMaxHold() {
    m_maxHoldSpectrum = m_instantSpectrum;
}

SdrDspEngine& getSdrDspEngine() {
    static SdrDspEngine s_engine;
    return s_engine;
}

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
