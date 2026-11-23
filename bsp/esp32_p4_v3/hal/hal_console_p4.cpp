#include "hal_console_p4.hpp"
#include "cbdos/system_cli.hpp"
#include "cbdos/system.hpp"
#include "cbdos/ducky.hpp"
#include "cbdos/tts.hpp"
#include "cbdos/usb_host.hpp"
#include "usb_cdc_loader_port.hpp"

#include <esp_loader_io.h>
#include <driver/usb_serial_jtag.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>
#include <cstdio>
#include <cstring>

static const char* TAG = "HAL_CONSOLE_P4";

#define ENABLE_CBDOS_SYSTEM_CONSOLE 1

#if ENABLE_CBDOS_SYSTEM_CONSOLE

static void cmdDucky(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    std::string duckyCmd = args;
    while (!duckyCmd.empty() && (duckyCmd.front() == ' ' || duckyCmd.front() == '\t')) {
        duckyCmd.erase(0, 1);
    }
    if (duckyCmd.empty()) {
        if (ctx.write) ctx.write("[DUCKY] Uso: ducky: <script DuckyScript>\n");
        return;
    }
    ::cbdos::ducky::DuckyInterpreter::getInstance().loadFromString(duckyCmd);
    ::cbdos::ducky::DuckyInterpreter::getInstance().run();
    while (::cbdos::ducky::DuckyInterpreter::getInstance().getState() == ::cbdos::ducky::ExecutionState::Running) {
        if (ctx.isAborted && ctx.isAborted()) {
            if (ctx.write) ctx.write("[DUCKY] Ejecución cancelada por el operador.\n");
            return;
        }
        ::cbdos::ducky::DuckyInterpreter::getInstance().step();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (ctx.write) ctx.write("[DUCKY] Script ejecutado correctamente.\n");
}

static void cmdUsb(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    (void)args;
    auto* backend = ::cbdos::usb::getUsbHostBackend();
    ::cbdos::usb::UsbDeviceInfo dev;
    char buf[256];

    if (!backend || !backend->getActiveDevice(dev) || !dev.isConnected) {
        if (ctx.write) ctx.write("[USB] 🔌 Puerto USB OTG Libre: Ningún dispositivo conectado físicamente.\n");
    } else {
        snprintf(buf, sizeof(buf),
            "[USB] ⚡ Dispositivo USB Conectado:\n"
            "  Fabricante:  %s\n"
            "  Producto:    %s\n"
            "  VID:PID:     0x%04X : 0x%04X\n"
            "  Clase:       %d\n",
            dev.manufacturer, dev.product,
            (unsigned)dev.vid, (unsigned)dev.pid,
            (int)dev.devClass);
        if (ctx.write) ctx.write(buf);
    }
}

static void cmdC3Status(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    (void)args;
    if (ctx.write) ctx.write("[C3] 🔍 Sondeando módem ESP32-C3 en puerto USB OTG High-Speed...\n");

    esp_loader_error_t err = loader_port_usb_cdc_init(1500);
    if (err != ESP_LOADER_SUCCESS) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "[C3_ERR] ❌ No se detectó dispositivo USB en el puerto OTG (err=%d)\n", err);
        if (ctx.write) ctx.write(errBuf);
        return;
    }

    // 1. Drenar buffer previo
    uint8_t trash[128];
    while (loader_port_read(trash, sizeof(trash), 10) == ESP_LOADER_SUCCESS);

    // 2. Enviar trama binaria GET_STATUS con CRC8 correcto (0x5E)
    uint8_t get_status_frame[] = { 0xAA, 0x55, 0x03, 0x00, 0x01, 0x01, 0x5E };
    loader_port_write(get_status_frame, sizeof(get_status_frame), 500);

    // 3. Buscar magic bytes 0xAA 0x55
    bool synced = false;
    uint8_t b = 0;
    int tries = 0;
    while (tries++ < 50) {
        if (ctx.isAborted && ctx.isAborted()) {
            if (ctx.write) ctx.write("[C3] Sondeo cancelado.\n");
            return;
        }
        if (loader_port_read(&b, 1, 50) == ESP_LOADER_SUCCESS && b == 0xAA) {
            if (loader_port_read(&b, 1, 50) == ESP_LOADER_SUCCESS && b == 0x55) {
                synced = true;
                break;
            }
        }
    }

    if (!synced) {
        if (ctx.write) ctx.write("[C3_ERR] ⚠️ Timeout esperando sincronización de trama 0xAA 0x55 del C3\n");
        return;
    }

    uint8_t dir = 0, len_h = 0, len_l = 0;
    loader_port_read(&dir, 1, 50);
    loader_port_read(&len_h, 1, 50);
    loader_port_read(&len_l, 1, 50);
    uint16_t plen = (len_h << 8) | len_l;
    if (plen > 0 && plen < 128) {
        uint8_t p[128] = {0};
        loader_port_read(p, plen, 200);
        uint8_t crc = 0;
        loader_port_read(&crc, 1, 50);

        if (dir == 0x04 && plen >= 11 && p[0] == 0x01 && p[1] == 0x00) {
            char mac_str[24];
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", p[2], p[3], p[4], p[5], p[6], p[7]);
            const char* mode_str = (p[8] == 2) ? "Long Range (LR) 🚀" : "Normal (802.11 b/g/n) ⚡";
            uint8_t chan = p[9];
            float pwr = p[10] * 0.25f;
            uint8_t peers = p[11];
            char alias_str[32] = {0};
            if (plen > 11) {
                size_t alen = plen - 11;
                if (alen > 31) alen = 31;
                memcpy(alias_str, p + 11, alen);
                alias_str[alen] = '\0';
            } else {
                strcpy(alias_str, "N/A");
            }
            char reportBuf[384];
            snprintf(reportBuf, sizeof(reportBuf),
                "\n======================================================\n"
                "  🛰️ [P4 USB Host] ESP32-C3 MÓDEM DE RADIO ENLACE OK!\n"
                "======================================================\n"
                "  Nodo Alias:     %s\n"
                "  MAC Hardware:   %s\n"
                "  Modo Radio:     %s\n"
                "  Canal Activo:   Canal %u\n"
                "  Potencia TX:    %.2f dBm\n"
                "  Peers en Aire:  %u\n"
                "======================================================\n\n",
                alias_str, mac_str, mode_str, chan, pwr, peers);
            if (ctx.write) ctx.write(reportBuf);
        } else {
            char errBuf[128];
            snprintf(errBuf, sizeof(errBuf), "[C3_ERR] Formato de payload no reconocido (dir=0x%02X len=%u)\n", dir, plen);
            if (ctx.write) ctx.write(errBuf);
        }
    }
}

static void cmdC3Ping(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    cbdos::cli::ExecutionOptions opts = cbdos::cli::parseOptions(args);
    char buf[160];

    for (uint32_t i = 0; i < opts.count; ++i) {
        // Trama de paquete de radio: DIR_PC_TO_DONGLE (0x01)
        uint8_t ping_data[] = { 0x01, 0x00, 0xAA, 0x55, 'P', '4', '_', 'R', 'A', 'D', 'I', 'O' };
        uint8_t tx_frame[32];
        tx_frame[0] = 0xAA;
        tx_frame[1] = 0x55;
        tx_frame[2] = 0x01;
        tx_frame[3] = 0x00;
        tx_frame[4] = sizeof(ping_data);
        memcpy(tx_frame + 5, ping_data, sizeof(ping_data));

        uint8_t crc = 0;
        for (size_t k = 0; k < sizeof(ping_data); k++) {
            uint8_t extract = ping_data[k];
            for (uint8_t t = 8; t; t--) {
                uint8_t sum = (crc ^ extract) & 0x01;
                crc >>= 1;
                if (sum) crc ^= 0x8C;
                extract >>= 1;
            }
        }
        tx_frame[5 + sizeof(ping_data)] = crc;
        loader_port_write(tx_frame, 6 + sizeof(ping_data), opts.timeoutMs);

        if (opts.count == 1) {
            if (ctx.write) ctx.write("[C3_PING] ✅ Trama emitida al aire por radio ESP-NOW vía C3.\n");
        } else {
            snprintf(buf, sizeof(buf), "[C3_PING #%u/%u] ✅ Trama enviada al aire por ESP-NOW.\n",
                (unsigned)(i + 1), (unsigned)opts.count);
            if (ctx.write) ctx.write(buf);
        }

        if (i + 1 < opts.count) {
            uint32_t step = 10;
            uint32_t elapsed = 0;
            while (elapsed < opts.intervalMs) {
                if (ctx.isAborted && ctx.isAborted()) {
                    if (ctx.write) ctx.write("[C3_PING] Cancelado por el operador.\n");
                    return;
                }
                uint32_t chunk = (opts.intervalMs - elapsed > step) ? step : (opts.intervalMs - elapsed);
                vTaskDelay(pdMS_TO_TICKS(chunk));
                elapsed += chunk;
            }
        }
    }
}

static void cmdTts(const std::string& args, const cbdos::cli::CommandContext& ctx) {
    std::string textToSpeak = args;
    while (!textToSpeak.empty() && (textToSpeak.front() == ' ' || textToSpeak.front() == '\t')) {
        textToSpeak.erase(0, 1);
    }
    if (textToSpeak.empty()) {
        if (ctx.write) ctx.write("[TTS] Uso: tts: <texto a sintetizar>\n");
        return;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "[TTS] 🗣️ Sintetizando voz: \"%s\"...\n", textToSpeak.c_str());
    if (ctx.write) ctx.write(buf);

    bool ok = ::cbdos::tts::speak(textToSpeak);
    if (ok) {
        if (ctx.write) ctx.write("[TTS] ✅ Texto encolado en PicoTTS.\n");
    } else {
        if (ctx.write) ctx.write("[TTS_ERR] ❌ Error al iniciar síntesis TTS (¿MicroSD montada con diccionarios?).\n");
    }
}

static void serial_console_task(void* arg) {
    (void)arg;

    // Configurar driver USB-Serial-JTAG para recepción por interrupción de hardware
    usb_serial_jtag_driver_config_t usb_s_cfg = {
        .tx_buffer_size = 1024,
        .rx_buffer_size = 2048,
    };
    usb_serial_jtag_driver_install(&usb_s_cfg);

    cbdos::cli::setBoardId("jc4880p443");
    cbdos::cli::init();

    // Registrar comandos específicos del hardware del P4
    cbdos::cli::registerCommand("ducky:", cmdDucky);
    cbdos::cli::registerCommand("ducky", cmdDucky);
    cbdos::cli::registerCommand("usb: status", cmdUsb);
    cbdos::cli::registerCommand("usb: info", cmdUsb);
    cbdos::cli::registerCommand("usb", cmdUsb);
    cbdos::cli::registerCommand("c3: status", cmdC3Status);
    cbdos::cli::registerCommand("c3: probe", cmdC3Status);
    cbdos::cli::registerCommand("radio: probe", cmdC3Status);
    cbdos::cli::registerCommand("c3: ping", cmdC3Ping);
    cbdos::cli::registerCommand("tts:", cmdTts);
    cbdos::cli::registerCommand("tts", cmdTts);

    ESP_LOGI(TAG, "Consola del Sistema CyBerDeck OS lista en /dev/ttyACM0 (Zero-Polling activo)");
    printf("\n=== CyBerDeck OS System Console (/dev/ttyACM0) ===\n");
    printf("Escribe 'help' o 'status' para iniciar.\n");
    printf("cbdos> ");
    fflush(stdout);

    std::string line_buf;
    line_buf.reserve(256);
    uint8_t ch;

    cbdos::cli::CommandContext ctx;
    ctx.write = [](const std::string& chunk) {
        printf("%s", chunk.c_str());
        fflush(stdout);
    };
    ctx.isAborted = []() -> bool {
        uint8_t peek;
        int r = usb_serial_jtag_read_bytes(&peek, 1, 0);
        return (r > 0);
    };

    while (1) {
        // Bloqueo puro por interrupción hardware (consumo CPU 0.0% en reposo)
        int read_bytes = usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY);
        if (read_bytes > 0) {
            if (ch == '\r' || ch == '\n') {
                if (ch == '\r') {
                    // Consumir el \n si el terminal envió \r\n juntos
                    uint8_t next_ch;
                    if (usb_serial_jtag_read_bytes(&next_ch, 1, 0) > 0) {
                        if (next_ch != '\n') {
                            // si no es \n, no hacer nada
                        }
                    }
                }
                printf("\r\n");
                fflush(stdout);
                if (!line_buf.empty()) {
                    // Drenar cualquier byte residual de fin de línea
                    uint8_t drain;
                    while (usb_serial_jtag_read_bytes(&drain, 1, 0) > 0) {
                        if (drain != '\r' && drain != '\n') break;
                    }
                    cbdos::cli::dispatch(line_buf, ctx);
                    line_buf.clear();
                }
                printf("cbdos> ");
                fflush(stdout);
            } else if (ch == '\b' || ch == 127) { // Backspace o DEL
                if (!line_buf.empty()) {
                    line_buf.pop_back();
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (ch == 3) { // Ctrl+C
                line_buf.clear();
                printf("^C\r\ncbdos> ");
                fflush(stdout);
            } else if (ch >= 32 && ch <= 126) {
                if (line_buf.size() < 256) {
                    line_buf += (char)ch;
                    printf("%c", ch);
                    fflush(stdout);
                }
            }
        }
    }
}

#endif // ENABLE_CBDOS_SYSTEM_CONSOLE

namespace cbdos {
namespace bsp {

void initSystemConsoleP4() {
#if ENABLE_CBDOS_SYSTEM_CONSOLE
    xTaskCreatePinnedToCore(serial_console_task, "sys_console_task", 6144, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "Tarea serial_console_task creada en Core 0");
#endif
}

} // namespace bsp
} // namespace cbdos
