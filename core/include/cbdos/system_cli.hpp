#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace cbdos {
namespace cli {

// Emisor de salida por streaming
using OutputWriter = std::function<void(const std::string& chunk)>;
// Consulta si la ejecución fue abortada (ej. pulsación de tecla o Ctrl+C)
using AbortCheck = std::function<bool()>;

struct CommandContext {
    OutputWriter write;
    AbortCheck isAborted;
};

struct ExecutionOptions {
    uint32_t count = 1;          // Cantidad de repeticiones (-c <N>)
    uint32_t intervalMs = 1000;  // Intervalo en ms (-i <time>, ej. 50ms, 1s)
    uint32_t timeoutMs = 500;    // Timeout en ms (-t <time>, ej. 100ms)
};

// Parser de flags comunes (-c, -i, -t)
ExecutionOptions parseOptions(const std::string& args);

// Manejador de comando
using CommandHandler = std::function<void(const std::string& args, const CommandContext& ctx)>;

// Configuración de identidad de placa
void setBoardId(const char* boardId);
const char* getBoardId();

// Registro de comandos
void registerCommand(const std::string& prefix, CommandHandler handler);

// Despacho de comando recibido
void dispatch(const std::string& line, const CommandContext& ctx);

// Inicializar catálogo base de comandos del sistema
void init();

} // namespace cli
} // namespace cbdos
