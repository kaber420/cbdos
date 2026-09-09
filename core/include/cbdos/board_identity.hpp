#pragma once
// CBDos — Identidad de placa/firmware para el flasheador web.
// v1 minimalista (sin DTB): banner por serial + comando CBDOS:VERSION?
// El DTB completo (pines/drivers/periféricos) queda reservado a futuro;
// este header es la semilla: solo board_id + soc + version.
//
// Uso en cada BSP (2 líneas):
//   #include "cbdos/board_identity.hpp"
//   cbdos::board_identity::printBanner(<printFn>);   // al boot
//   // en el loop CLI, ante una línea recibida:
//   // if (cbdos::board_identity::isVersionQuery(line))
//   //     printFn(cbdos::board_identity::bannerFor(BOARD).c_str());
//
// Formato del banner (estable, parseable por la web con regex):
//   CBDOS:BOARD=<board_id> SOC=<soc> VER=<version>

#include <string>

namespace cbdos {
namespace board_identity {

// Versión única del firmware. Actualizar en cada release.
inline const char* version() { return "0.2.3-dev"; }

// Comandos / prefijos del protocolo v1.
inline const char* kBannerPrefix = "CBDOS:BOARD=";
inline const char* kVersionQuery = "CBDOS:VERSION?";

struct BoardInfo {
    const char* boardId;   // ej: "jc4880p443" — clave de filtro en Releases
    const char* soc;       // ej: "esp32-p4"   — clave de filtro en Releases
    const char* boardName; // ej: "Guition JC4880P443C" — solo display
};

// Catálogo v1. Añadir una entrada por tablilla futura (sin romper nada).
inline const BoardInfo kBoards[] = {
    {"jc4880p443", "esp32-p4", "Guition JC4880P443C"},
    {"jc3248w535", "esp32-s3", "Guition JC3248W535"},
};
inline constexpr int kBoardCount = 2;

inline const BoardInfo* findBoard(const char* boardId) {
    if (!boardId) return nullptr;
    for (int i = 0; i < kBoardCount; ++i) {
        const char* a = kBoards[i].boardId;
        const char* b = boardId;
        bool eq = true;
        while (*a || *b) { if (*a++ != *b++) { eq = false; break; } }
        if (eq) return &kBoards[i];
    }
    return nullptr;
}

// "CBDOS:BOARD=jc4880p443 SOC=esp32-p4 VER=0.2.3-dev"
inline std::string bannerFor(const BoardInfo& b) {
    std::string s(kBannerPrefix);
    s += b.boardId;
    s += " SOC=";
    s += b.soc;
    s += " VER=";
    s += version();
    return s;
}

// true si la línea es el comando de versión (ignora \r\n y espacios).
inline bool isVersionQuery(const std::string& line) {
    std::string t;
    t.reserve(line.size());
    for (char c : line) {
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        t += c;
    }
    const char* q = kVersionQuery;
    if (t.size() != 14) return false; // strlen("CBDOS:VERSION?") == 14
    for (size_t i = 0; i < 14; ++i) {
        if (t[i] != q[i]) return false;
    }
    return true;
}

// true si la línea es un banner válido (para que la web lo detecte al boot).
inline bool isBanner(const std::string& line) {
    if (line.size() < 12) return false;
    for (int i = 0; i < 12; ++i) {
        if (line[i] != kBannerPrefix[i]) return false;
    }
    return true;
}

} // namespace board_identity
} // namespace cbdos
