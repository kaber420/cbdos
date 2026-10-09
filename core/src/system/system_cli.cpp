#include "cbdos/system_cli.hpp"
#include "cbdos/system.hpp"
#include "cbdos/board_identity.hpp"
#include "LuaEngine.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cctype>

namespace cbdos {
namespace cli {

#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(__riscv)
static std::string s_activeBoardId = "jc4880p443";
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
static std::string s_activeBoardId = "jc3248w535";
#else
static std::string s_activeBoardId = "jc4880p443";
#endif

void setBoardId(const char* boardId) {
    if (boardId && boardId[0]) {
        s_activeBoardId = boardId;
    }
}

const char* getBoardId() {
    return s_activeBoardId.c_str();
}

struct RegisteredCmd {
    std::string prefix;
    CommandHandler handler;
};

static std::vector<RegisteredCmd> s_commands;
static bool s_initialized = false;

static std::string trimString(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        start++;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        end--;
    }
    return s.substr(start, end - start);
}

static uint32_t parseTimeMs(const std::string& token) {
    if (token.empty()) return 1000;
    std::string lowerToken = token;
    std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), ::tolower);

    if (lowerToken.size() > 2 && lowerToken.substr(lowerToken.size() - 2) == "ms") {
        long val = std::strtol(lowerToken.substr(0, lowerToken.size() - 2).c_str(), nullptr, 10);
        return (val > 0) ? (uint32_t)val : 10;
    }
    if (lowerToken.size() > 1 && lowerToken.back() == 's') {
        float sec = std::strtof(lowerToken.substr(0, lowerToken.size() - 1).c_str(), nullptr);
        return (sec > 0.0f) ? (uint32_t)(sec * 1000.0f) : 10;
    }
    long val = std::strtol(lowerToken.c_str(), nullptr, 10);
    if (val <= 0) return 10;
    // Si no tiene unidad y es menor a 10, interpretar como segundos; de lo contrario milisegundos
    if (val <= 10) return (uint32_t)(val * 1000);
    return (uint32_t)val;
}

ExecutionOptions parseOptions(const std::string& args) {
    ExecutionOptions opts;
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < args.size()) {
        while (i < args.size() && (args[i] == ' ' || args[i] == '\t')) i++;
        if (i >= args.size()) break;
        size_t start = i;
        while (i < args.size() && args[i] != ' ' && args[i] != '\t') i++;
        tokens.push_back(args.substr(start, i - start));
    }

    for (size_t t = 0; t < tokens.size(); ++t) {
        if (tokens[t] == "-c" && t + 1 < tokens.size()) {
            long c = std::strtol(tokens[++t].c_str(), nullptr, 10);
            if (c > 0) opts.count = (uint32_t)c;
        } else if (tokens[t] == "-i" && t + 1 < tokens.size()) {
            opts.intervalMs = parseTimeMs(tokens[++t]);
        } else if (tokens[t] == "-t" && t + 1 < tokens.size()) {
            opts.timeoutMs = parseTimeMs(tokens[++t]);
        }
    }

    if (opts.intervalMs < 10) {
        opts.intervalMs = 10; // Piso de seguridad para evitar asfixia del planificador
    }
    return opts;
}

static bool sleepWithAbort(uint32_t ms, const AbortCheck& isAborted) {
    const uint32_t step = 10;
    uint32_t elapsed = 0;
    while (elapsed < ms) {
        if (isAborted && isAborted()) {
            return true;
        }
        uint32_t chunk = (ms - elapsed > step) ? step : (ms - elapsed);
        cbdos::system::sleepMs(chunk);
        elapsed += chunk;
    }
    if (isAborted && isAborted()) {
        return true;
    }
    return false;
}

void registerCommand(const std::string& prefix, CommandHandler handler) {
    s_commands.push_back({prefix, handler});
}

// ----------------- Comandos Base de Sistema -----------------

static void cmdStatus(const std::string& args, const CommandContext& ctx) {
    ExecutionOptions opts = parseOptions(args);
    char buf[256];

    for (uint32_t i = 0; i < opts.count; ++i) {
        float cpuTemp = cbdos::system::getCpuTemperature();
        size_t freeHeap = cbdos::system::getFreeHeap();
        size_t totalHeap = cbdos::system::getTotalHeap();
        size_t freePsram = cbdos::system::getFreePsram();
        size_t totalPsram = cbdos::system::getTotalPsram();
        uint32_t upSec = cbdos::system::getTimeMs() / 1000;
        uint32_t upMin = upSec / 60;
        uint32_t upHour = upMin / 60;

        if (opts.count == 1) {
            snprintf(buf, sizeof(buf),
                "[SYS_STATUS]\n"
                "  CPU Temp:    %.1f °C\n"
                "  RAM Libre:   %u KB / %u KB\n"
                "  PSRAM Libre: %.1f MB / %.1f MB\n"
                "  Uptime:      %uh %um %us\n",
                cpuTemp,
                (unsigned)(freeHeap / 1024), (unsigned)(totalHeap / 1024),
                (freePsram / (1024.0f * 1024.0f)), (totalPsram / (1024.0f * 1024.0f)),
                (unsigned)upHour, (unsigned)(upMin % 60), (unsigned)(upSec % 60));
        } else {
            snprintf(buf, sizeof(buf),
                "[SYS_STATUS #%u/%u] Temp: %.1f °C | RAM: %u/%u KB | PSRAM: %.1f/%.1f MB | Up: %us\n",
                (unsigned)(i + 1), (unsigned)opts.count,
                cpuTemp,
                (unsigned)(freeHeap / 1024), (unsigned)(totalHeap / 1024),
                (freePsram / (1024.0f * 1024.0f)), (totalPsram / (1024.0f * 1024.0f)),
                (unsigned)upSec);
        }

        if (ctx.write) ctx.write(buf);

        if (i + 1 < opts.count) {
            if (sleepWithAbort(opts.intervalMs, ctx.isAborted)) {
                if (ctx.write) ctx.write("\n--- [SYS] Muestreo cancelado por el operador ---\n");
                return;
            }
        }
    }
}

static void cmdInfo(const std::string& args, const CommandContext& ctx) {
    (void)args;
    const auto* board = cbdos::board_identity::findBoard(getBoardId());
    const char* boardName = board ? board->boardName : "Desconocida";
    const char* soc = board ? board->soc : "Desconocido";

    char buf[256];
    snprintf(buf, sizeof(buf),
        "[SYS_INFO]\n"
        "  Sistema:  CyBerDeck OS (CBDos) v%s\n"
        "  Placa:    %s (%s)\n"
        "  SoC:      %s\n",
        cbdos::board_identity::version(),
        boardName, getBoardId(),
        soc);

    if (ctx.write) ctx.write(buf);
}

static void cmdTemp(const std::string& args, const CommandContext& ctx) {
    ExecutionOptions opts = parseOptions(args);
    char buf[128];

    float minTemp = 999.0f;
    float maxTemp = -999.0f;

    for (uint32_t i = 0; i < opts.count; ++i) {
        float t = cbdos::system::getCpuTemperature();
        if (t < minTemp) minTemp = t;
        if (t > maxTemp) maxTemp = t;

        if (opts.count == 1) {
            snprintf(buf, sizeof(buf), "[SYS] CPU Temp: %.1f °C\n", t);
        } else {
            snprintf(buf, sizeof(buf), "[SYS] CPU Temp: %.1f °C (iter %u/%u)\n", t, (unsigned)(i + 1), (unsigned)opts.count);
        }

        if (ctx.write) ctx.write(buf);

        if (i + 1 < opts.count) {
            if (sleepWithAbort(opts.intervalMs, ctx.isAborted)) {
                if (ctx.write) ctx.write("\n--- [SYS] Muestreo de temperatura cancelado ---\n");
                return;
            }
        }
    }

    if (opts.count > 1 && ctx.write) {
        snprintf(buf, sizeof(buf), "--- Resumen Térmico (%u muestras a %ums, min: %.1f °C, max: %.1f °C) ---\n",
            (unsigned)opts.count, (unsigned)opts.intervalMs, minTemp, maxTemp);
        ctx.write(buf);
    }
}

static void cmdMem(const std::string& args, const CommandContext& ctx) {
    ExecutionOptions opts = parseOptions(args);
    char buf[160];

    for (uint32_t i = 0; i < opts.count; ++i) {
        size_t freeHeap = cbdos::system::getFreeHeap();
        size_t totalHeap = cbdos::system::getTotalHeap();
        size_t freePsram = cbdos::system::getFreePsram();
        size_t totalPsram = cbdos::system::getTotalPsram();

        if (opts.count == 1) {
            snprintf(buf, sizeof(buf), "[SYS] RAM: %u KB / %u KB | PSRAM: %.1f MB / %.1f MB\n",
                (unsigned)(freeHeap / 1024), (unsigned)(totalHeap / 1024),
                (freePsram / (1024.0f * 1024.0f)), (totalPsram / (1024.0f * 1024.0f)));
        } else {
            snprintf(buf, sizeof(buf), "[SYS] RAM: %u/%u KB | PSRAM: %.1f/%.1f MB (#%u/%u)\n",
                (unsigned)(freeHeap / 1024), (unsigned)(totalHeap / 1024),
                (freePsram / (1024.0f * 1024.0f)), (totalPsram / (1024.0f * 1024.0f)),
                (unsigned)(i + 1), (unsigned)opts.count);
        }

        if (ctx.write) ctx.write(buf);

        if (i + 1 < opts.count) {
            if (sleepWithAbort(opts.intervalMs, ctx.isAborted)) {
                if (ctx.write) ctx.write("\n--- [SYS] Muestreo de memoria cancelado ---\n");
                return;
            }
        }
    }
}

static void cmdUptime(const std::string& args, const CommandContext& ctx) {
    (void)args;
    uint32_t upSec = cbdos::system::getTimeMs() / 1000;
    uint32_t upMin = upSec / 60;
    uint32_t upHour = upMin / 60;

    char buf[128];
    snprintf(buf, sizeof(buf), "[SYS] Uptime: %uh %um %us (%u s)\n",
        (unsigned)upHour, (unsigned)(upMin % 60), (unsigned)(upSec % 60), (unsigned)upSec);
    if (ctx.write) ctx.write(buf);
}

static void cmdHelp(const std::string& args, const CommandContext& ctx) {
    (void)args;
    const char* helpText =
        "=== Comandos del Sistema CyBerDeck OS ===\n"
        "  status [-c N] [-i T]   Métricas globales (CPU Temp, RAM, PSRAM, Uptime)\n"
        "  info                   Identidad del firmware y hardware\n"
        "  temp [-c N] [-i T]     Temperatura del SoC (ej: temp -c 10 -i 50ms)\n"
        "  mem [-c N] [-i T]      Métricas de memoria Heap y PSRAM libre\n"
        "  uptime                 Tiempo de actividad del sistema\n"
        "  help / ?               Mostrar este manual de comandos\n"
        "Flags opcionales:\n"
        "  -c <N>       Cantidad de iteraciones (default: 1)\n"
        "  -i <T>       Intervalo entre muestras (ms o s, ej: 50ms, 1s, mín 10ms)\n"
        "  -t <T>       Timeout máximo de respuesta para pruebas de radio/red\n"
        "* Cualquier otra expresión será evaluada por el intérprete Lua++.\n";
    if (ctx.write) ctx.write(helpText);
}

void init() {
    if (s_initialized) return;
    s_initialized = true;

    // Registrar alias cortos y jerárquicos
    registerCommand("sys: status", cmdStatus);
    registerCommand("status", cmdStatus);
    registerCommand("sys: info", cmdInfo);
    registerCommand("info", cmdInfo);
    registerCommand("sys: temp", cmdTemp);
    registerCommand("temp", cmdTemp);
    registerCommand("sys: mem", cmdMem);
    registerCommand("mem", cmdMem);
    registerCommand("sys: uptime", cmdUptime);
    registerCommand("uptime", cmdUptime);
    registerCommand("sys: help", cmdHelp);
    registerCommand("help", cmdHelp);
    registerCommand("?", cmdHelp);
}

void dispatch(const std::string& line, const CommandContext& ctx) {
    std::string trimmed = trimString(line);
    if (trimmed.empty()) return;

    if (!s_initialized) {
        init();
    }

    // 1. Prioridad: Consulta de versión para Flasheador Web
    if (cbdos::board_identity::isVersionQuery(trimmed)) {
        const auto* board = cbdos::board_identity::findBoard(getBoardId());
        if (board) {
            std::string banner = cbdos::board_identity::bannerFor(*board) + "\n";
            if (ctx.write) ctx.write(banner);
        } else {
            if (ctx.write) ctx.write("CBDOS:BOARD=unknown SOC=unknown VER=" + std::string(cbdos::board_identity::version()) + "\n");
        }
        return;
    }

    // 2. Prioridad: Reinicio a Bootloader
    if (trimmed == "CBDOS:BOOTLOADER") {
        if (ctx.write) ctx.write("OK: REBOOTING TO BOOTLOADER\n");
        cbdos::system::sleepMs(100);
        cbdos::system::restartToBootloader();
        return;
    }

    // 3. Comandos registrados (prefijos o coincidencia exacta)
    for (const auto& reg : s_commands) {
        bool match = false;
        std::string args;
        if (trimmed == reg.prefix) {
            match = true;
            args = "";
        } else if (trimmed.rfind(reg.prefix + " ", 0) == 0) {
            match = true;
            args = trimString(trimmed.substr(reg.prefix.size() + 1));
        } else if (trimmed.rfind(reg.prefix + ":", 0) == 0) {
            match = true;
            args = trimString(trimmed.substr(reg.prefix.size() + 1));
        }

        if (match) {
            reg.handler(args, ctx);
            return;
        }
    }

    // 4. Fallback: Intérprete Lua++
    std::string outRes;
    bool ok = ::LuaEngine::getInstance().executeString(trimmed, &outRes);
    if (ok) {
        if (!outRes.empty()) {
            std::string msg = "[SERIAL_CLI_OUT] OK: " + outRes + "\n";
            if (ctx.write) ctx.write(msg);
        }
    } else {
        std::string err = "[SERIAL_CLI_ERR] " + ::LuaEngine::getInstance().getLastError() + "\n";
        if (ctx.write) ctx.write(err);
    }
}

} // namespace cli
} // namespace cbdos
