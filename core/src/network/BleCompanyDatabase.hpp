#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cbdos {
namespace network {

/**
 * @brief Entrada en la base de datos de fabricantes asignados por Bluetooth SIG
 */
struct BleCompanyEntry {
    uint16_t companyId; // Identificador oficial de 16 bits del Bluetooth SIG
    const char* vendor; // Nombre canónico del fabricante
};

const BleCompanyEntry* getBleCompanyTable();
std::size_t getBleCompanyTableSize();

/**
 * @brief Búsqueda binaria O(log N) en Flash (.rodata) de fabricante por Company ID
 * @param companyId Identificador de 16 bits obtenido del Manufacturer Specific Data
 * @return Nombre del fabricante o "Unknown" si no está registrado
 */
std::string lookupVendorByCompanyId(uint16_t companyId);

} // namespace network
} // namespace cbdos
