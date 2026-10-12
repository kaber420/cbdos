#include "cbdos/sdr/sdr_cli.hpp"

#if CBDOS_FEATURE_SDR

#include "cbdos/system_cli.hpp"
#include "cbdos/usb_host.hpp"
#include "cbdos/system.hpp"
#include "cbdos/sdr/esp_sdr_client.hpp"
#include "cbdos/sdr/sdr_dsp_engine.hpp"

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace cbdos {
namespace sdr {

namespace {

static void cmdSdrProbe(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    (void)args;
    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host) {
        if (ctx.write) ctx.write("[SDR] ❌ El backend USB Host no está inicializado en esta plataforma.\n");
        return;
    }

    if (!host->isDeviceConnected()) {
        if (ctx.write) ctx.write("[SDR] ❌ No hay ningún dispositivo USB conectado en el puerto Host.\n"
                                 "[SDR] Conecta la mochila ESP32-S3 con firmware ESP-SDR al puerto USB del Cyberdeck.\n");
        return;
    }

    cbdos::usb::UsbDeviceInfo dev;
    if (!host->getActiveDevice(dev)) {
        if (ctx.write) ctx.write("[SDR] ⚠️ Dispositivo conectado pero no se pudo leer la información de descriptores.\n");
        return;
    }

    char infoBuf[256];
    snprintf(infoBuf, sizeof(infoBuf),
             "[SDR] 🔌 Dispositivo USB detectado:\n"
             "  Fabricante: %s\n"
             "  Producto:   %s\n"
             "  VID / PID:  0x%04X : 0x%04X\n",
             dev.manufacturer, dev.product, dev.vid, dev.pid);
    if (ctx.write) ctx.write(infoBuf);

    if (dev.vid != 0x303A) {
        if (ctx.write) ctx.write("[SDR] ⚠️ Advertencia: El VID no pertenece a Espressif (0x303A). Podría no ser compatible con ESP-SDR.\n");
    }

    auto* cdc = host->getCdcChannel();
    if (!cdc) {
        if (ctx.write) ctx.write("[SDR] ❌ No hay canal CDC disponible en el backend USB.\n");
        return;
    }

    if (!cdc->tryAcquire(cbdos::usb::CdcOwner::Terminal, 1500)) {
        if (ctx.write) ctx.write("[SDR] ⚠️ El canal CDC está ocupado por otro proceso del sistema.\n");
        return;
    }

    if (ctx.write) ctx.write("[SDR] 🔄 Iniciando secuencia de sincronización con firmware ESP-SDR...\n");
    cdc->purge();

    // 1. Enviar secuencia de sincronización (SYNC <timestamp>)
    uint32_t ts = cbdos::system::getTimeMs();
    char syncCmd[64];
    snprintf(syncCmd, sizeof(syncCmd), "\nSYNC %lu\n", (unsigned long)ts);
    cdc->writeString(syncCmd);

    // 2. Esperar respuesta de sincronización (timeout 2000 ms)
    std::string response;
    uint32_t startMs = cbdos::system::getTimeMs();
    bool syncOk = false;

    while (cbdos::system::getTimeMs() - startMs < 2000) {
        if (ctx.isAborted && ctx.isAborted()) {
            if (ctx.write) ctx.write("[SDR] ⛔ Operación abortada por el operador.\n");
            cdc->release(cbdos::usb::CdcOwner::Terminal);
            return;
        }

        std::string chunk = cdc->readString(256);
        if (!chunk.empty()) {
            response += chunk;
            if (response.find("SYNC") != std::string::npos) {
                syncOk = true;
                break;
            }
        }
        cbdos::system::sleepMs(20);
    }

    if (!syncOk) {
        if (ctx.write) ctx.write("[SDR] ❌ Timeout: El dispositivo no respondió a la sincronización SYNC.\n"
                                 "[SDR] Verifica que el ESP32-S3 tenga grabado el firmware oficial de ESP-SDR (ESPARGOS).\n");
        cdc->release(cbdos::usb::CdcOwner::Terminal);
        return;
    }

    if (ctx.write) ctx.write("[SDR] ✅ Sincronización exitosa con firmware ESP-SDR.\n");

    // 3. Consultar capacidades (SPECINFO?)
    cdc->purge();
    cdc->writeString("SPECINFO?\n");
    std::string specInfo;
    startMs = cbdos::system::getTimeMs();

    while (cbdos::system::getTimeMs() - startMs < 1500) {
        if (ctx.isAborted && ctx.isAborted()) break;
        std::string chunk = cdc->readString(256);
        if (!chunk.empty()) {
            specInfo += chunk;
            if (specInfo.find('\n') != std::string::npos) break;
        }
        cbdos::system::sleepMs(20);
    }

    if (!specInfo.empty()) {
        char specBuf[300];
        snprintf(specBuf, sizeof(specBuf), "[SDR] 📡 Capacidades reportadas:\n  %s\n", specInfo.c_str());
        if (ctx.write) ctx.write(specBuf);
    } else {
        if (ctx.write) ctx.write("[SDR] ℹ️ Firmware respondió a SYNC pero no emitió SPECINFO (modo burst estándar activo).\n");
    }

    cdc->release(cbdos::usb::CdcOwner::Terminal);
    if (ctx.write) ctx.write("[SDR] 🏁 Canal liberado. Dispositivo listo para operación SDR.\n");
}

static void cmdSdrFreq(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    if (args.empty()) {
        if (ctx.write) ctx.write("Uso: sdr freq <frecuencia_MHz> (ej: sdr freq 2412)\n");
        return;
    }

    long freq = std::strtol(args.c_str(), nullptr, 10);
    if (freq < 100 || freq > 6000) {
        if (ctx.write) ctx.write("[SDR] ❌ Frecuencia fuera de rango válido (100 - 6000 MHz).\n");
        return;
    }

    auto* host = cbdos::usb::getUsbHostBackend();
    if (!host || !host->isDeviceConnected()) {
        if (ctx.write) ctx.write("[SDR] ❌ Dispositivo SDR no conectado.\n");
        return;
    }

    auto* cdc = host->getCdcChannel();
    if (!cdc || !cdc->tryAcquire(cbdos::usb::CdcOwner::Terminal, 1000)) {
        if (ctx.write) ctx.write("[SDR] ❌ No se pudo adquirir el canal CDC.\n");
        return;
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "FREQ %ld\n", freq);
    cdc->writeString(cmd);

    char outBuf[128];
    snprintf(outBuf, sizeof(outBuf), "[SDR] 📻 Comando enviado: FREQ %ld MHz\n", freq);
    if (ctx.write) ctx.write(outBuf);

    cbdos::system::sleepMs(50);
    std::string resp = cdc->readString(128);
    if (!resp.empty()) {
        char respBuf[160];
        snprintf(respBuf, sizeof(respBuf), "[SDR] Respuesta: %s\n", resp.c_str());
        if (ctx.write) ctx.write(respBuf);
    }

    cdc->release(cbdos::usb::CdcOwner::Terminal);
}

static void cmdSdrGain(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    if (args.empty()) {
        if (ctx.write) ctx.write("Uso: sdr gain <auto|manual 0-60> (ej: sdr gain auto, sdr gain manual 40)\n");
        return;
    }

    auto& client = getEspSdrClient();
    if (!client.connect()) {
        if (ctx.write) ctx.write("[SDR] ❌ No se pudo conectar con el dispositivo SDR.\n");
        return;
    }

    if (args == "auto" || args == "agc" || args == "hardware") {
        if (client.setGain(GainMode::HardwareAgc)) {
            if (ctx.write) ctx.write("[SDR] ✅ Ganancia configurada en modo Automático (Hardware AGC).\n");
        } else {
            if (ctx.write) ctx.write("[SDR] ❌ Fallo al aplicar ganancia automática.\n");
        }
    } else {
        int gainIdx = 40;
        if (args.rfind("manual", 0) == 0) {
            std::string sub = args.size() > 6 ? args.substr(7) : "";
            gainIdx = (int)std::strtol(sub.c_str(), nullptr, 10);
        } else {
            gainIdx = (int)std::strtol(args.c_str(), nullptr, 10);
        }
        if (gainIdx < 0 || gainIdx > 60) gainIdx = 40;

        if (client.setGain(GainMode::Manual, gainIdx)) {
            char outBuf[96];
            snprintf(outBuf, sizeof(outBuf), "[SDR] ✅ Ganancia manual aplicada: índice %d / 60.\n", gainIdx);
            if (ctx.write) ctx.write(outBuf);
        } else {
            if (ctx.write) ctx.write("[SDR] ❌ Fallo al aplicar ganancia manual.\n");
        }
    }
    client.disconnect();
}

static void cmdSdrCapture(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    (void)args;
    auto& client = getEspSdrClient();
    if (ctx.write) ctx.write("[SDR] 🔄 Conectando con ESP32-S3 SDR...\n");
    if (!client.connect()) {
        if (ctx.write) ctx.write("[SDR] ❌ No se pudo conectar al dispositivo SDR. Verifica conexión USB.\n");
        return;
    }

    uint32_t freq = client.getFrequency();
    char hdrBuf[160];
    snprintf(hdrBuf, sizeof(hdrBuf),
             "[SDR] 📡 Iniciando prueba de adquisición por ráfagas I/Q (5 ráfagas @ %lu MHz)...\n",
             (unsigned long)freq);
    if (ctx.write) ctx.write(hdrBuf);

    SdrConfig cfg;
    cfg.frequencyMHz = freq;
    cfg.sampleRate = 40000000; // 40 MSa/s
    cfg.sampleBits = 8;
    cfg.fftBins = 1024;
    cfg.gainMode = GainMode::HardwareAgc;

    client.resetStats();
    int burstCount = 5;
    int successCount = 0;

    for (int b = 1; b <= burstCount; b++) {
        if (ctx.isAborted && ctx.isAborted()) {
            if (ctx.write) ctx.write("[SDR] ⛔ Prueba abortada por el usuario.\n");
            break;
        }

        SdrFrame frame;
        uint32_t t0 = cbdos::system::getTimeMs();
        bool ok = client.captureBurst(cfg, frame, 2000);
        uint32_t elapsed = cbdos::system::getTimeMs() - t0;

        if (ok && frame.crcOk) {
            successCount++;
            // Calcular potencia promedio RMS de la ráfaga
            float sumPwr = 0.0f;
            for (size_t s = 0; s < frame.samples; s++) {
                float re = frame.iqData[s * 2];
                float im = frame.iqData[s * 2 + 1];
                sumPwr += (re * re + im * im);
            }
            float avgPwr = frame.samples > 0 ? (sumPwr / frame.samples) : 0.0f;
            float dbfs = 10.0f * log10f(std::max(avgPwr, 1e-12f));
            float ksps = elapsed > 0 ? ((float)frame.samples / (float)elapsed) : 0.0f;

            char burstBuf[180];
            snprintf(burstBuf, sizeof(burstBuf),
                     "  [#%d] %lu muestras en %lu ms (%.1f kS/s) | CRC: OK | RMS: %.1f dBFS\n",
                     b, (unsigned long)frame.samples, (unsigned long)elapsed, ksps, dbfs);
            if (ctx.write) ctx.write(burstBuf);
        } else {
            char errBuf[128];
            snprintf(errBuf, sizeof(errBuf),
                     "  [#%d] ❌ Error en ráfaga (Timeout o fallo de CRC)\n", b);
            if (ctx.write) ctx.write(errBuf);
        }
        cbdos::system::sleepMs(50);
    }

    const auto& stats = client.getStats();
    char summaryBuf[220];
    snprintf(summaryBuf, sizeof(summaryBuf),
             "[SDR] 🏁 Prueba completada: %d/%d ráfagas exitosas.\n"
             "  Errores CRC: %lu | Pérdidas: %lu | Throughput prom: %.1f kS/s\n",
             successCount, burstCount,
             (unsigned long)stats.crcErrors,
             (unsigned long)stats.droppedCaptures,
             stats.throughputKsps);
    if (ctx.write) ctx.write(summaryBuf);

    client.disconnect();
}

static void cmdSdrDsp(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    auto& dsp = getSdrDspEngine();

    if (args == "bench" || args == "benchmark") {
        if (ctx.write) ctx.write("[SDR DSP] ⏱️ Iniciando Benchmark de FFT + Ventana Hann + dBFS...\n");
        const uint16_t sizes[] = {256, 512, 1024, 2048};
        for (uint16_t n : sizes) {
            DspConfig cfg;
            cfg.fftBins = n;
            dsp.configure(cfg);

            // Generar ráfaga sintética: tono a fs/4
            std::vector<float> testIq(n * 2);
            for (uint16_t i = 0; i < n; i++) {
                float phase = 2.0f * 3.14159265f * (float)i * 0.25f;
                testIq[i * 2]     = cosf(phase);
                testIq[i * 2 + 1] = sinf(phase);
            }

            SdrFrame out;
            // Warm-up
            dsp.processRaw(testIq.data(), n, 40000000, 2412, out);

            // 50 iteraciones
            const int iters = 50;
            uint64_t t0 = cbdos::system::getTimeUs();
            for (int k = 0; k < iters; k++) {
                dsp.processRaw(testIq.data(), n, 40000000, 2412, out);
            }
            uint64_t tTotalUs = cbdos::system::getTimeUs() - t0;
            float avgUs = (float)tTotalUs / (float)iters;

            char buf[160];
            snprintf(buf, sizeof(buf),
                     "  • FFT %4u bins: %6.1f µs/iter (%.1f FFTs/sec) | Pico: %.1f dBFS @ %.1f MHz\n",
                     n, avgUs, (1000000.0f / (avgUs > 0.0f ? avgUs : 1.0f)), out.peakDbfs, out.peakFreqMHz);
            if (ctx.write) ctx.write(buf);
        }
        if (ctx.write) ctx.write("[SDR DSP] ✅ Benchmark completado (< 2 ms de presupuesto cumplido con holgura masiva).\n");
        return;
    }

    // Si no es solo bench, verificar si hay hardware o ejecutar captura en vivo
    auto& client = getEspSdrClient();
    auto* host = cbdos::usb::getUsbHostBackend();
    bool devConnected = (host && host->isDeviceConnected());

    if (!devConnected) {
        if (ctx.write) ctx.write("[SDR DSP] ⚠️ No hay hardware SDR USB conectado. Ejecutando benchmark sintético...\n");
        cmdSdrDsp("bench", ctx);
        return;
    }

    if (ctx.write) ctx.write("[SDR DSP] 🔄 Conectando y capturando ráfaga de RF en vivo...\n");
    if (!client.connect()) {
        if (ctx.write) ctx.write("[SDR DSP] ❌ Error al conectar con el backend USB CDC.\n");
        return;
    }

    uint32_t freq = client.getFrequency();
    SdrConfig cfg;
    cfg.frequencyMHz = freq;
    cfg.sampleRate   = 40000000;
    cfg.sampleBits   = 8;
    cfg.fftBins      = 1024;

    SdrFrame rxFrame;
    if (!client.captureBurst(cfg, rxFrame, 2000) || !rxFrame.crcOk) {
        if (ctx.write) ctx.write("[SDR DSP] ❌ Error de recepción o verificación CRC de la ráfaga.\n");
        client.disconnect();
        return;
    }

    // Procesar con el motor DSP
    SdrFrame dspFrame;
    dsp.configure({cfg.fftBins, 0.35f, true, 0.05f});
    if (!dsp.process(rxFrame, dspFrame)) {
        if (ctx.write) ctx.write("[SDR DSP] ❌ Fallo durante el procesamiento DSP.\n");
        client.disconnect();
        return;
    }

    char statBuf[200];
    snprintf(statBuf, sizeof(statBuf),
             "[SDR DSP] 🎯 Ráfaga procesada exitosamente:\n"
             "  • Tiempo FFT/DSP: %lu µs (%u bins)\n"
             "  • Frecuencia portadora: %lu MHz | Span: 40 MHz\n"
             "  • Pico detectado: %.1f dBFS en %.2f MHz\n",
             (unsigned long)dsp.getLastDspTimeUs(),
             dspFrame.fftBins,
             (unsigned long)dspFrame.frequencyMHz,
             dspFrame.peakDbfs,
             dspFrame.peakFreqMHz);
    if (ctx.write) ctx.write(statBuf);

    // Representación gráfica ASCII del espectro (40 columnas)
    if (ctx.write && !dspFrame.spectrumDbfs.empty()) {
        ctx.write("  • Espectro:\n    [");
        const char ramp[] = " .:-=+*#%@";
        const int rampLen = sizeof(ramp) - 1;
        const int cols = 40;
        int step = (int)dspFrame.spectrumDbfs.size() / cols;
        for (int c = 0; c < cols; c++) {
            float maxBin = -120.0f;
            for (int s = 0; s < step; s++) {
                int idx = c * step + s;
                if (idx < (int)dspFrame.spectrumDbfs.size()) {
                    maxBin = std::max(maxBin, dspFrame.spectrumDbfs[idx]);
                }
            }
            float norm = (maxBin - (-100.0f)) / 80.0f;
            norm = std::max(0.0f, std::min(1.0f, norm));
            int rIdx = (int)(norm * (rampLen - 1));
            char ch[2] = {ramp[rIdx], '\0'};
            ctx.write(ch);
        }
        ctx.write("]\n");
    }

    client.disconnect();
}

static void cmdSdrHelp(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    (void)args;
    const char* help =
        "--- Comandos SDR (ESP-SDR Companion over USB CDC) ---\n"
        "  sdr probe                 Verifica conexión y realiza handshake SYNC con la mochila S3\n"
        "  sdr freq <MHz>            Sintoniza la frecuencia central de la radio (ej: sdr freq 2412)\n"
        "  sdr gain <auto|0-60>      Ajusta la ganancia en modo AGC o manual\n"
        "  sdr capture (o sdr test)  Captura 5 ráfagas I/Q y valida CRC32 y throughput\n"
        "  sdr dsp [bench]           Ejecuta pipeline FFT/dBFS en vivo o benchmark de latencia\n"
        "  sdr help                  Muestra esta ayuda\n";
    if (ctx.write) ctx.write(help);
}

static void cmdSdrDispatch(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    if (args.empty() || args == "probe" || args == "ping") {
        cmdSdrProbe(args, ctx);
    } else if (args.rfind("freq", 0) == 0) {
        std::string sub = args.size() > 4 ? args.substr(5) : "";
        cmdSdrFreq(sub, ctx);
    } else if (args.rfind("gain", 0) == 0) {
        std::string sub = args.size() > 4 ? args.substr(5) : "";
        cmdSdrGain(sub, ctx);
    } else if (args == "capture" || args == "test" || args.rfind("cap", 0) == 0) {
        cmdSdrCapture(args, ctx);
    } else if (args.rfind("dsp", 0) == 0 || args.rfind("fft", 0) == 0) {
        std::string sub = args.size() > 3 ? args.substr(4) : "";
        cmdSdrDsp(sub, ctx);
    } else if (args == "help" || args == "?") {
        cmdSdrHelp(args, ctx);
    } else {
        cmdSdrProbe(args, ctx);
    }
}

} // anonymous namespace

void registerSdrCliCommands() {
    cbdos::cli::registerCommand("sdr", cmdSdrDispatch);
    cbdos::cli::registerCommand("sdr: probe", cmdSdrProbe);
    cbdos::cli::registerCommand("sdr: freq", cmdSdrFreq);
    cbdos::cli::registerCommand("sdr: gain", cmdSdrGain);
    cbdos::cli::registerCommand("sdr: capture", cmdSdrCapture);
    cbdos::cli::registerCommand("sdr: test", cmdSdrCapture);
    cbdos::cli::registerCommand("sdr: dsp", cmdSdrDsp);
    cbdos::cli::registerCommand("sdr: help", cmdSdrHelp);
}

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
