#include "cbdos/sdr/esp_sdr_client.hpp"

#if CBDOS_FEATURE_SDR

#include "cbdos/system.hpp"
#include "cbdos/sdr/sdr_dsp_engine.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
}

EspSdrClient::~EspSdrClient() {
    disconnect();
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

    // Configuración por defecto: sintonizar 2412 MHz y ganancia automática
    tune(m_currentFreqMHz);
    setGain(m_gainMode, m_gainIndex);

    return true;
}

void EspSdrClient::disconnect() {
    if (!m_connected) return;

    if (m_streaming) {
        stopStreaming();
    }

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
    if (!m_connected) return false;
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return false;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "FREQ %lu\n", (unsigned long)freqMHz);
    cdc->writeString(cmd);

    std::string resp = readLineFromCdc(cdc, 500);
    m_currentFreqMHz = freqMHz;
    return (resp.find("OK") != std::string::npos || !resp.empty());
}

bool EspSdrClient::setGain(GainMode mode, int gainIndex) {
    if (!m_connected) return false;
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) return false;
    auto* cdc = host->getCdcChannel();
    if (!cdc) return false;

    char cmd[48];
    if (mode == GainMode::HardwareAgc) {
        snprintf(cmd, sizeof(cmd), "GAIN HARDWARE\n");
    } else {
        snprintf(cmd, sizeof(cmd), "GAIN MANUAL %d\n", gainIndex);
    }
    cdc->writeString(cmd);

    std::string resp = readLineFromCdc(cdc, 500);
    m_gainMode = mode;
    m_gainIndex = gainIndex;
    return (resp.find("OK") != std::string::npos || !resp.empty());
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
    }
    vTaskDelete(nullptr);
}

void EspSdrClient::streamTaskLoop() {
    SdrFrame frame;
    frame.iqData.reserve(4096);
    SdrFrame dspFrame;

    while (m_streaming && m_connected) {
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
        // Yield para no saturar CPU y permitir arbitraje
        cbdos::system::yieldTask();
    }
}

bool EspSdrClient::startStreaming(const SdrConfig& cfg, SdrFrameCallback callback) {
    if (m_streaming) return true;
    if (!connect()) return false;

    m_streamConfig = cfg;
    m_streamCallback = callback;
    m_streaming = true;

    BaseType_t ret = xTaskCreatePinnedToCore(
        streamTaskTrampoline,
        "sdr_stream_task",
        8192,
        this,
        5,
        (TaskHandle_t*)&m_streamTaskHandle,
        1 // Fijar en Core 1 para dejar Core 0 libre para LVGL 9.5
    );

    if (ret != pdPASS) {
        m_streaming = false;
        m_streamTaskHandle = nullptr;
        return false;
    }

    return true;
}

void EspSdrClient::stopStreaming() {
    if (!m_streaming) return;
    m_streaming = false;

    if (m_streamTaskHandle) {
        // Esperar brevemente a que termine el bucle de captura
        for (int i = 0; i < 20; i++) {
            if (eTaskGetState((TaskHandle_t)m_streamTaskHandle) == eDeleted) {
                break;
            }
            cbdos::system::sleepMs(20);
        }
        m_streamTaskHandle = nullptr;
    }
}

EspSdrClient& getEspSdrClient() {
    static EspSdrClient s_client;
    return s_client;
}

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
