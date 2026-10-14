#include "cbdos/sdr/esp_sdr_client.hpp"

#if CBDOS_FEATURE_SDR

#include "cbdos/system.hpp"
#include "cbdos/sdr/sdr_dsp_engine.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace cbdos {
namespace sdr {

namespace {

// Tabla precalculada CRC32 (IEEE 802.3 polinomio 0xEDB88320)
static uint32_t s_crc32Table[256];
static bool s_crcTableInit = false;

static void initCrc32Table() {
    if (s_crcTableInit) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
        s_crc32Table[i] = c;
    }
    s_crcTableInit = true;
}

struct SpecProfile {
    uint32_t sampleRate;
    int divider;
    uint16_t fftBins;
    int param3;
    int upf;
};

// Perfiles SPEC del ESP32-S3 verificados en hardware con `SPECINFO?`
// (firmware ESP-SDR rev e74f2a4, 2026-10-08). Formato: {rate, divider, bins, param3, upf}.
// El S3 sólo soporta 16 / 40 / 80 MS/s en streaming continuo con FFT en chip.
static const SpecProfile kSpecProfiles[] = {
    // 16 MSa/s (divider 6)
    {16000000, 6, 256, 2, 1},
    {16000000, 6, 512, 2, 2},
    {16000000, 6, 1024, 2, 4},
    {16000000, 6, 2048, 3, 7},
    // 40 MSa/s (divider 1)
    {40000000, 1, 256, 5, 3},
    {40000000, 1, 512, 5, 5},
    {40000000, 1, 1024, 5, 9},
    {40000000, 1, 2048, 7, 17},
    // 80 MSa/s (divider 0)
    {80000000, 0, 256, 10, 6},
    {80000000, 0, 512, 12, 10},
    {80000000, 0, 1024, 14, 16},
    {80000000, 0, 2048, 14, 30}
};

static constexpr size_t kSpecProfileCount = sizeof(kSpecProfiles) / sizeof(kSpecProfiles[0]);

// Devuelve siempre un perfil válido del S3. El llamante debe usar prof->sampleRate y
// prof->fftBins como valores efectivos (pueden diferir de los solicitados).
static const SpecProfile* findSpecProfile(uint32_t rate, uint16_t bins) {
    for (size_t i = 0; i < kSpecProfileCount; i++) {
        if (kSpecProfiles[i].sampleRate == rate && kSpecProfiles[i].fftBins == bins) {
            return &kSpecProfiles[i];
        }
    }
    // Tasa no soportada por el S3: fallback a 40 MSa/s conservando los bins solicitados
    for (size_t i = 0; i < kSpecProfileCount; i++) {
        if (kSpecProfiles[i].sampleRate == 40000000 && kSpecProfiles[i].fftBins == bins) {
            return &kSpecProfiles[i];
        }
    }
    // Bins no soportados: 40 MSa/s, 512 bins
    return &kSpecProfiles[5];
}

// Helper para leer una línea terminada en '\n' desde IUsbCdcChannel
static std::string readLineFromCdc(cbdos::usb::IUsbCdcChannel* cdc, uint32_t timeoutMs, size_t maxBytes = 256) {
    if (!cdc) return "";
    std::string line;
    uint32_t startMs = cbdos::system::getTimeMs();

    while (cbdos::system::getTimeMs() - startMs < timeoutMs && line.size() < maxBytes) {
        std::string chunk = cdc->readString(1);
        if (!chunk.empty()) {
            char ch = chunk[0];
            if (ch == '\n') {
                break;
            }
            if (ch != '\r') {
                line.push_back(ch);
            }
        } else {
            cbdos::system::sleepMs(2);
        }
    }
    return line;
}

// Helper para leer N bytes exactos desde IUsbCdcChannel con timeout
static bool readExactBytes(cbdos::usb::IUsbCdcChannel* cdc, uint8_t* outBuf, size_t totalBytes, uint32_t timeoutMs) {
    if (!cdc || !outBuf || totalBytes == 0) return false;
    size_t received = 0;
    uint32_t startMs = cbdos::system::getTimeMs();

    while (received < totalBytes && (cbdos::system::getTimeMs() - startMs < timeoutMs)) {
        size_t chunkRead = 0;
        size_t toRead = totalBytes - received;
        cbdos::usb::UsbIoResult res = cdc->read(outBuf + received, toRead, chunkRead, 50);
        if (res == cbdos::usb::UsbIoResult::Ok && chunkRead > 0) {
            received += chunkRead;
            // Reiniciar deadline tras recepción activa para soportar streams
            startMs = cbdos::system::getTimeMs();
        } else if (res == cbdos::usb::UsbIoResult::Disconnected || res == cbdos::usb::UsbIoResult::HardwareError) {
            return false;
        } else {
            cbdos::system::sleepMs(2);
        }
    }
    return (received == totalBytes);
}

} // anonymous namespace

uint32_t EspSdrClient::calculateCrc32(const uint8_t* data, size_t length) {
    initCrc32Table();
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; i++) {
        crc = s_crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return (crc ^ 0xFFFFFFFFUL);
}

EspSdrClient::EspSdrClient() {
    initCrc32Table();
    m_taskDoneSem = (void*)xSemaphoreCreateBinary();
}

EspSdrClient::~EspSdrClient() {
    disconnect();
    if (m_taskDoneSem && !isTaskAlive()) {
        vSemaphoreDelete((SemaphoreHandle_t)m_taskDoneSem);
        m_taskDoneSem = nullptr;
    }
}

bool EspSdrClient::waitForTaskExit(uint32_t timeoutMs) {
    if (!isTaskAlive()) return true;
    if (!m_taskDoneSem) return false;
    if (xSemaphoreTake((SemaphoreHandle_t)m_taskDoneSem, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {
        m_streamTaskHandle = nullptr;
        return true;
    }
    return !isTaskAlive();
}

// Precondición: el llamante es el único propietario del CDC (la tarea de stream o,
// si no existe tarea, el hilo que la iba a lanzar).
void EspSdrClient::drainSpecEndAndPurge() {
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return;

    // Protocolo ESP-SDR: '\n' finaliza la trama activa y el firmware responde "SPECEND ...\n"
    // (medido en hardware: ~50 ms, 0 bytes residuales tras SPECEND).
    cdc->writeString("\n");

    static constexpr uint32_t kDrainTimeoutMs = 300;
    static constexpr size_t   kTailKeep = 16;
    std::string acc;
    acc.reserve(4096);
    uint8_t chunk[1024];
    bool gotSpecEnd = false;
    uint32_t startMs = cbdos::system::getTimeMs();

    while (cbdos::system::getTimeMs() - startMs < kDrainTimeoutMs) {
        size_t n = 0;
        cbdos::usb::UsbIoResult res = cdc->read(chunk, sizeof(chunk), n, 20);
        if (res == cbdos::usb::UsbIoResult::Disconnected || res == cbdos::usb::UsbIoResult::HardwareError) {
            break;
        }
        if (res == cbdos::usb::UsbIoResult::Ok && n > 0) {
            acc.append(reinterpret_cast<const char*>(chunk), n);
            size_t pos = acc.find("SPECEND");
            if (pos != std::string::npos && acc.find('\n', pos) != std::string::npos) {
                gotSpecEnd = true;
                break;
            }
            // Acotar memoria: sólo interesa la cola por si "SPECEND" queda partido entre chunks
            if (pos == std::string::npos && acc.size() > 4096) {
                acc.erase(0, acc.size() - kTailKeep);
            }
        }
    }

    cdc->purge();
    if (!gotSpecEnd) {
        // Estado del enlace incierto: forzar re-SYNC antes del próximo comando
        m_needsResync = true;
    }
}

bool EspSdrClient::sendCommandExpectOk(const char* cmd, uint32_t timeoutMs) {
    if (!m_connected || !cmd) return false;
    if (isTaskAlive()) return false; // La tarea de stream es la propietaria del CDC

    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return false;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    if (m_needsResync) {
        if (!performSync()) return false;
        cdc->purge();
        m_needsResync = false;
    }

    cdc->writeString(cmd);

    // Protocolo verificado: éxito = línea "OK"; fallo = línea "ERR <motivo>".
    // Cualquier otra línea (eco tardío, texto residual) se ignora hasta el deadline.
    uint32_t startMs = cbdos::system::getTimeMs();
    while (true) {
        uint32_t elapsed = cbdos::system::getTimeMs() - startMs;
        if (elapsed >= timeoutMs) break;
        std::string line = readLineFromCdc(cdc, timeoutMs - elapsed);
        if (line == "OK") return true;
        if (line.rfind("ERR", 0) == 0) return false;
    }
    return false;
}

bool EspSdrClient::performSync() {
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return false;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    cdc->purge();

    for (int attempt = 0; attempt < 3; attempt++) {
        uint32_t ts = cbdos::system::getTimeMs();
        char syncCmd[64];
        snprintf(syncCmd, sizeof(syncCmd), "\nSYNC %lu\n", (unsigned long)ts);
        cdc->writeString(syncCmd);

        std::string response;
        uint32_t startMs = cbdos::system::getTimeMs();
        while (cbdos::system::getTimeMs() - startMs < 1200) {
            std::string chunk = cdc->readString(128);
            if (!chunk.empty()) {
                response += chunk;
                if (response.find("SYNC") != std::string::npos) {
                    return true;
                }
            }
            cbdos::system::sleepMs(20);
        }
    }
    return false;
}

bool EspSdrClient::connect() {
    if (m_connected) return true;

    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host || !host->isDeviceConnected()) return false;

    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    if (!cdc->tryAcquire(cbdos::usb::CdcOwner::Terminal, 2000)) {
        return false;
    }

    if (!performSync()) {
        cdc->release(cbdos::usb::CdcOwner::Terminal);
        return false;
    }

    m_connected = true;
    m_needsResync = false;
    m_filterBwApplied = false; // El dispositivo puede haberse reiniciado

    // Configuración por defecto: sintonizar y aplicar ganancia guardadas
    tune(m_currentFreqMHz);
    setGain(m_gainMode, m_gainIndex);

    return true;
}

void EspSdrClient::disconnect() {
    if (!m_connected) return;

    stopStreaming();

    auto* host = cbdos::usb::getUsbHostBackend();
    if (host) {
        auto* cdc = host->getCdcChannel();
        if (cdc) {
            cdc->release(cbdos::usb::CdcOwner::Terminal);
        }
    }

    m_connected = false;
}

bool EspSdrClient::tune(uint32_t freqMHz) {
    if (freqMHz < kSdrMinFreqMHz || freqMHz > kSdrMaxFreqMHz) return false;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "FREQ %lu\n", (unsigned long)freqMHz);
    if (!sendCommandExpectOk(cmd)) return false;

    m_currentFreqMHz = freqMHz;
    return true;
}

bool EspSdrClient::setGain(GainMode mode, int gainIndex) {
    if (gainIndex < 0) gainIndex = 0;
    if (gainIndex > kSdrMaxGainIndex) gainIndex = kSdrMaxGainIndex;

    char cmd[48];
    if (mode == GainMode::HardwareAgc) {
        snprintf(cmd, sizeof(cmd), "GAIN HARDWARE\n");
    } else {
        snprintf(cmd, sizeof(cmd), "GAIN MANUAL %d\n", gainIndex);
    }
    if (!sendCommandExpectOk(cmd)) return false;

    m_gainMode = mode;
    m_gainIndex = gainIndex;
    return true;
}

bool EspSdrClient::setFilterBandwidth(uint32_t bwMHz) {
    // 0 = ajuste más ancho (filtro abierto). Rango analógico S3 verificado: 13 - 69 MHz.
    if (bwMHz != 0) {
        if (bwMHz < kSdrMinFilterBwMHz) bwMHz = kSdrMinFilterBwMHz;
        if (bwMHz > kSdrMaxFilterBwMHz) bwMHz = kSdrMaxFilterBwMHz;
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "BANDWIDTH %lu\n", (unsigned long)bwMHz);
    if (!sendCommandExpectOk(cmd)) return false;

    m_filterBwMHz = bwMHz;
    m_filterBwApplied = true;
    return true;
}

int EspSdrClient::getRateDivider(uint32_t rate) const {
    // Tasas oficiales ESPARGOS:
    // 80M: 0, 40M: 1, 20M: 2, 10M: 3, 8M: 4, 4M: 5, 16M: 6, 32M: 7
    switch (rate) {
        case 80000000: return 0;
        case 40000000: return 1;
        case 20000000: return 2;
        case 10000000: return 3;
        case 8000000:  return 4;
        case 4000000:  return 5;
        case 16000000: return 6;
        case 32000000: return 7;
        default:       return 1; // 40 MSa/s fallback
    }
}

bool EspSdrClient::captureBurst(const SdrConfig& cfg, SdrFrame& outFrame, uint32_t timeoutMs) {
    if (!m_connected) return false;
    // Sólo la propia tarea de stream (modo RawIQ) puede capturar mientras está viva
    if (isTaskAlive() && (TaskHandle_t)m_streamTaskHandle != xTaskGetCurrentTaskHandle()) return false;
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return false;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    uint32_t startMs = cbdos::system::getTimeMs();
    int divider = getRateDivider(cfg.sampleRate);
    uint32_t nSamples = std::max<uint32_t>(cfg.fftBins, 1024);

    // Formato comando: CAP<bits*2> <n> <divider>
    // 8 bits -> CAP16; 10 bits -> CAP20
    uint8_t bitArg = (cfg.sampleBits == 10) ? 20 : 16;
    char capCmd[64];
    snprintf(capCmd, sizeof(capCmd), "CAP%u %lu %d\n", bitArg, (unsigned long)nSamples, divider);

    cdc->purge();
    cdc->writeString(capCmd);

    // Leer cabecera de datos: DATA <samples> <crc_hex> <captureUs>
    std::string header = readLineFromCdc(cdc, timeoutMs);
    if (header.rfind("DATA", 0) != 0) {
        m_stats.droppedCaptures++;
        return false;
    }

    uint32_t rxSamples = 0;
    uint32_t expectedCrc = 0;
    uint32_t captureUs = 0;
    char crcHexStr[32] = {0};

    if (sscanf(header.c_str(), "DATA %lu %31s %lu",
               (unsigned long*)&rxSamples, crcHexStr, (unsigned long*)&captureUs) < 2) {
        m_stats.droppedCaptures++;
        return false;
    }
    expectedCrc = (uint32_t)strtoul(crcHexStr, nullptr, 16);

    // Calcular tamaño del payload binario
    size_t payloadBytes = (cfg.sampleBits == 10) ? ((rxSamples * 20 + 7) / 8) : (rxSamples * 2);
    std::vector<uint8_t> rxBuffer(payloadBytes);

    if (!readExactBytes(cdc, rxBuffer.data(), payloadBytes, timeoutMs)) {
        m_stats.droppedCaptures++;
        return false;
    }

    // Validar CRC32
    uint32_t actualCrc = calculateCrc32(rxBuffer.data(), payloadBytes);
    bool crcValid = (actualCrc == expectedCrc);
    if (!crcValid) {
        m_stats.crcErrors++;
        return false;
    }

    // Desempaquetar muestras I/Q en float [-1.0f, +1.0f]
    outFrame.type = FrameType::BurstIQ;
    outFrame.sequence = ++m_sequence;
    outFrame.frequencyMHz = cfg.frequencyMHz;
    outFrame.sampleRate = cfg.sampleRate;
    outFrame.fftBins = cfg.fftBins;
    outFrame.captureUs = captureUs;
    outFrame.samples = rxSamples;
    outFrame.crcOk = true;
    outFrame.iqData.clear();
    outFrame.iqData.reserve(rxSamples * 2);

    if (cfg.sampleBits == 8) {
        for (size_t j = 0; j < rxSamples; j++) {
            int8_t iRaw = (int8_t)rxBuffer[j * 2];
            int8_t qRaw = (int8_t)rxBuffer[j * 2 + 1];
            // Normalización según radio.js: i*4 / 512.0f; q = -q*4 / 512.0f
            float iNorm = (float)(iRaw * 4) / 512.0f;
            float qNorm = -(float)(qRaw * 4) / 512.0f;
            outFrame.iqData.push_back(iNorm);
            outFrame.iqData.push_back(qNorm);
        }
    } else {
        // Desempaquetado 10-bit entrelazado (20 bits por muestra)
        uint32_t acc = 0;
        int bitsInAcc = 0;
        size_t byteIdx = 0;

        for (size_t j = 0; j < rxSamples; j++) {
            while (bitsInAcc < 20 && byteIdx < payloadBytes) {
                acc |= ((uint32_t)rxBuffer[byteIdx++]) << bitsInAcc;
                bitsInAcc += 8;
            }
            uint32_t w = acc & 0xFFFFF;
            acc >>= 20;
            bitsInAcc -= 20;

            int iRaw = w & 1023;
            int qRaw = (w >> 10) & 1023;
            if (iRaw >= 512) iRaw -= 1024;
            if (qRaw >= 512) qRaw -= 1024;

            float iNorm = (float)iRaw / 512.0f;
            float qNorm = -(float)qRaw / 512.0f;
            outFrame.iqData.push_back(iNorm);
            outFrame.iqData.push_back(qNorm);
        }
    }

    uint32_t elapsedMs = cbdos::system::getTimeMs() - startMs;
    m_stats.framesReceived++;
    m_stats.lastElapsedMs = elapsedMs;
    if (elapsedMs > 0) {
        m_stats.throughputKsps = (float)rxSamples / (float)elapsedMs;
        m_stats.fps = 1000.0f / (float)elapsedMs;
    }

    return true;
}

void EspSdrClient::resetStats() {
    m_stats = SdrStats{};
}

void EspSdrClient::streamTaskTrampoline(void* arg) {
    auto* self = static_cast<EspSdrClient*>(arg);
    if (self) {
        self->streamTaskLoop();
        // Fin ordenado: soltar la propiedad del CDC y despertar a stopStreaming()
        self->m_streaming.store(false, std::memory_order_release);
        self->m_taskAlive.store(false, std::memory_order_release);
        if (self->m_taskDoneSem) {
            xSemaphoreGive((SemaphoreHandle_t)self->m_taskDoneSem);
        }
    }
    vTaskDelete(nullptr);
}

void EspSdrClient::streamTaskLoop() {
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return;

    if (m_streamConfig.captureMode == CaptureMode::RawIQ) {
        // Fallback: Captura secuencial por ráfagas crudas
        SdrFrame frame;
        frame.iqData.reserve(4096);
        SdrFrame dspFrame;

        while (m_streaming.load(std::memory_order_acquire) && m_connected) {
            if (captureBurst(m_streamConfig, frame, 1500)) {
                if (getSdrDspEngine().process(frame, dspFrame)) {
                    if (m_streamCallback) {
                        m_streamCallback(dspFrame);
                    }
                } else if (m_streamCallback) {
                    m_streamCallback(frame);
                }
            } else {
                cbdos::system::sleepMs(20);
            }
            cbdos::system::yieldTask();
        }
        cdc->purge();
        return;
    }

    // Modo On-Chip Spectrum (SPEC / SPC1 de ESPARGOS): 200+ espectros/segundo.
    // FREQ/GAIN/BANDWIDTH, el comando SPEC y su ACK ya fueron procesados de forma
    // síncrona en startStreaming(); aquí sólo se consume el flujo binario.
    const uint16_t n = m_streamConfig.fftBins;
    const size_t frameLen = 32 + n; // 4 (SPC1) + 24 (hdr) + n (bins) + 4 (crc)

    std::vector<uint8_t> rxBuf;
    rxBuf.reserve(8192);
    uint8_t chunk[2048];

    uint32_t lastFpsCheckMs = cbdos::system::getTimeMs();
    uint32_t framesThisPeriod = 0;

    while (m_streaming.load(std::memory_order_acquire) && m_connected) {
        size_t bytesRead = 0;
        cbdos::usb::UsbIoResult res = cdc->read(chunk, sizeof(chunk), bytesRead, 20);

        if (res == cbdos::usb::UsbIoResult::Ok && bytesRead > 0) {
            rxBuf.insert(rxBuf.end(), chunk, chunk + bytesRead);

            size_t at = 0;
            while (rxBuf.size() - at >= 4) {
                const uint8_t* b = rxBuf.data() + at;
                if (b[0] != 'S' || b[1] != 'P') {
                    at++;
                    continue;
                }

                // Descartar o procesar SPECEND (fin de stream)
                if (b[2] == 'E' && b[3] == 'C') {
                    if (rxBuf.size() - at < 8) break;
                    size_t nl = at;
                    while (nl < rxBuf.size() && rxBuf[nl] != '\n') nl++;
                    if (nl >= rxBuf.size()) break;
                    at = nl + 1;
                    continue;
                }

                // Descartar SPS1 (estadísticas del chip de 40 bytes)
                if (b[2] == 'S' && b[3] == '1') {
                    if (rxBuf.size() - at < 40) break;
                    at += 40;
                    continue;
                }

                // Trama de espectro SPC1
                if (b[2] != 'C' || b[3] != '1') {
                    at++;
                    continue;
                }

                if (rxBuf.size() - at < frameLen) {
                    break;
                }

                // Verificación CRC32 (los últimos 4 bytes son CRC little-endian)
                uint32_t expectedCrc = (uint32_t)b[frameLen - 4] |
                                      ((uint32_t)b[frameLen - 3] << 8) |
                                      ((uint32_t)b[frameLen - 2] << 16) |
                                      ((uint32_t)b[frameLen - 1] << 24);
                uint32_t actualCrc = calculateCrc32(b, frameLen - 4);
                if (actualCrc != expectedCrc) {
                    m_stats.crcErrors++;
                    at++;
                    continue;
                }

                uint8_t step = b[27];
                if (step == 0) step = 2;
                const uint8_t* bins = b + 28;

                SdrFrame frame;
                frame.type = FrameType::Spectrum;
                frame.sequence = ++m_sequence;
                frame.frequencyMHz = m_currentFreqMHz;
                frame.sampleRate = m_streamConfig.sampleRate;
                frame.fftBins = n;
                frame.crcOk = true;
                frame.spectrumDbfs.resize(n);

                float peakDb = -140.0f;
                uint16_t peakIdx = 0;

                // Decodificación exacta de la web: out[j] = (bins[(n/2 - j + n) % n] / step) - 84.3
                for (uint16_t j = 0; j < n; j++) {
                    uint16_t srcIdx = (n / 2 - j + n) % n;
                    float v = (float)bins[srcIdx];
                    float db = (v / (float)step) - 84.3f;
                    frame.spectrumDbfs[j] = db;

                    if (db > peakDb) {
                        peakDb = db;
                        peakIdx = j;
                    }
                }

                // Puente DC (suprime la fuga local del oscilador en el bin central)
                uint16_t c = n / 2;
                if (c >= 2 && c + 2 < n) {
                    float m = (frame.spectrumDbfs[c - 2] + frame.spectrumDbfs[c + 2]) / 2.0f;
                    frame.spectrumDbfs[c - 1] = m;
                    frame.spectrumDbfs[c]     = m;
                    frame.spectrumDbfs[c + 1] = m;
                }

                float spanMHz = (float)frame.sampleRate / 1000000.0f;
                float startFreqMHz = (float)frame.frequencyMHz - (spanMHz / 2.0f);
                float stepMHz = spanMHz / (float)n;
                frame.peakDbfs = peakDb;
                frame.peakFreqMHz = startFreqMHz + (float)peakIdx * stepMHz;

                m_stats.framesReceived++;
                framesThisPeriod++;

                uint32_t nowMs = cbdos::system::getTimeMs();
                if (nowMs - lastFpsCheckMs >= 1000) {
                    m_stats.fps = (float)framesThisPeriod * 1000.0f / (float)(nowMs - lastFpsCheckMs);
                    framesThisPeriod = 0;
                    lastFpsCheckMs = nowMs;
                }

                if (m_streamCallback) {
                    m_streamCallback(frame);
                }

                at += frameLen;
            }

            if (at > 0) {
                rxBuf.erase(rxBuf.begin(), rxBuf.begin() + at);
            }
        } else if (res == cbdos::usb::UsbIoResult::Disconnected || res == cbdos::usb::UsbIoResult::HardwareError) {
            break;
        }

        cbdos::system::yieldTask();
    }

    // Parada sincronizada: '\n' -> drenar hasta SPECEND -> purge (único punto que lo hace)
    drainSpecEndAndPurge();
}

bool EspSdrClient::startStreaming(const SdrConfig& cfg, SdrFrameCallback callback) {
    if (isTaskAlive()) {
        if (m_streaming.load(std::memory_order_acquire)) return true; // Ya en curso
        // Una parada anterior aún no terminó: esperar a que suelte el CDC
        if (!waitForTaskExit(1000)) return false;
    }
    if (!connect()) return false;

    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return false;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    // 1. Aplicar estado de radio sólo si difiere del ya confirmado por el firmware
    if (cfg.frequencyMHz != m_currentFreqMHz) {
        if (!tune(cfg.frequencyMHz)) return false;
    }
    if (cfg.gainMode != m_gainMode ||
        (cfg.gainMode == GainMode::Manual && cfg.gainIndex != m_gainIndex)) {
        if (!setGain(cfg.gainMode, cfg.gainIndex)) return false;
    }
    if (!m_filterBwApplied || cfg.filterBwMHz != m_filterBwMHz) {
        if (!setFilterBandwidth(cfg.filterBwMHz)) return false;
    }

    m_streamConfig = cfg;
    m_streamConfig.frequencyMHz = m_currentFreqMHz;

    // 2. Modo SPEC: enviar comando y validar ACK de forma síncrona (aún sin tarea)
    if (cfg.captureMode == CaptureMode::OnChipSpectrum) {
        const auto* prof = findSpecProfile(cfg.sampleRate, cfg.fftBins);
        // Reflejar la configuración efectiva (C9): ejes y FFT coherentes con el chip
        m_streamConfig.sampleRate = prof->sampleRate;
        m_streamConfig.fftBins    = prof->fftBins;

        cdc->purge();
        int maxHold = cfg.maxHold ? 1 : 0;
        char specCmd[64];
        snprintf(specCmd, sizeof(specCmd), "SPEC 0 %d %d %d %d %u 1\n",
                 prof->param3, prof->upf, maxHold, prof->divider, (unsigned)prof->fftBins);
        cdc->writeString(specCmd);

        // ACK verificado: "SPEC <bins> <rate> <x> <freqMHz>"; luego llegan tramas binarias.
        // readLineFromCdc lee byte a byte, por lo que el binario posterior queda en la FIFO.
        bool ackOk = false;
        uint32_t startMs = cbdos::system::getTimeMs();
        while (cbdos::system::getTimeMs() - startMs < 1500) {
            std::string ack = readLineFromCdc(cdc, 1500 - (cbdos::system::getTimeMs() - startMs));
            if (ack.rfind("SPEC ", 0) == 0) { ackOk = true; break; }
            if (ack.rfind("ERR", 0) == 0) break;
        }
        if (!ackOk) {
            drainSpecEndAndPurge(); // Por si el chip llegó a iniciar el stream
            return false;
        }
    }

    // 3. Lanzar la tarea: a partir de aquí es la única propietaria del CDC
    if (m_taskDoneSem) {
        xSemaphoreTake((SemaphoreHandle_t)m_taskDoneSem, 0); // Limpiar señal previa
    }
    m_streamCallback = callback;
    m_taskAlive.store(true, std::memory_order_release);
    m_streaming.store(true, std::memory_order_release);

    BaseType_t ret = xTaskCreatePinnedToCore(
        streamTaskTrampoline,
        "sdr_stream_task",
        8192,
        this,
        5,
        (TaskHandle_t*)&m_streamTaskHandle,
        0 // Fijar en Core 0 para dejar Core 1 100% libre para LVGL 9.5
    );

    if (ret != pdPASS) {
        m_streaming.store(false, std::memory_order_release);
        m_taskAlive.store(false, std::memory_order_release);
        m_streamTaskHandle = nullptr;
        if (cfg.captureMode == CaptureMode::OnChipSpectrum) {
            drainSpecEndAndPurge();
        }
        return false;
    }

    return true;
}

void EspSdrClient::stopStreaming() {
    m_streaming.store(false, std::memory_order_release);
    if (!isTaskAlive()) return;

    // La tarea detecta la orden en ≤ ~20 ms, drena SPECEND (≤ 300 ms) y libera el semáforo
    if (!waitForTaskExit(1000)) {
        // No debería ocurrir; la tarea sigue siendo propietaria del CDC. Los comandos
        // seguirán rechazándose hasta que termine, y se forzará re-SYNC.
        m_needsResync = true;
    }
}

EspSdrClient& getEspSdrClient() {
    static EspSdrClient s_client;
    return s_client;
}

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
