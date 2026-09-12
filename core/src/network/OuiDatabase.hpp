#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace cbdos {
namespace network {

// ────────────────────────────────────────────────────────────────
// OuiDatabase: resolucion de fabricante por OUI (primeros 3 bytes
// de la MAC). Tabla estatica const (vive en Flash/.rodata, sin RAM
// dinamica) ordenada ascendentemente por OUI para busqueda binaria
// O(log N) con std::lower_bound. C++17 puro.
// ────────────────────────────────────────────────────────────────

struct OuiEntry {
    uint32_t oui;       // 24 bits big-endian: mac[0]<<16 | mac[1]<<8 | mac[2]
    const char* vendor; // nombre canonico del fabricante
};

const OuiEntry* getOuiTable();
std::size_t getOuiTableSize();

// Convierte mac[0..2] a OUI de 24 bits.
uint32_t macToOui(const uint8_t mac[6]);

// Busqueda binaria. Retorna "Unknown" si el OUI no esta registrado.
std::string lookupVendorByOui(uint32_t oui);
std::string lookupVendorByMac(const uint8_t mac[6]);

} // namespace network
} // namespace cbdos
