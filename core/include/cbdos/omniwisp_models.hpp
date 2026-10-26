#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace cbdos {
namespace omniwisp {

// Representa un nodo jerárquico físico (Ciudad, PoP, Torre, o Zona)
struct NetworkGroup {
    std::string id;       // ej: "pop_centro", "torre_norte"
    std::string name;     // ej: "Torre Cerro Colorado", "PoP Ciudad Central"
    std::string parentId; // Id del grupo superior (ej: "ciudad_valle" o vacío si es raíz)
    std::string notes;    // Ubicación, acceso, etc.
};

// Representa un Radio / AP Sectorial dentro de la infraestructura
struct SectorDevice {
    std::string id;       // UUID o slug
    std::string groupId;  // PoP o Torre a la que pertenece
    std::string name;     // ej: "Sector Norte 90° - Rocket Prism 5AC"
    std::string host;     // Dirección IP en la red ruteada (ej: "10.200.10.21")
    uint16_t port = 443;  // Puerto HTTPS
    std::string user = "ubnt";
    std::string pass;
    std::string notes;
};

enum class OmniScreen {
    GroupList,      // Pantalla 1: Listado de Ciudades / PoPs / Torres
    SectorList,     // Pantalla 2: Sectores del PoP seleccionado
    SectorMonitor,  // Pantalla 3: Monitor en vivo del Sector y tabla de CPEs
    CpeDiagnostics  // Pantalla 4: Diagnóstico profundo del CPE (alineación y cable)
};

} // namespace omniwisp
} // namespace cbdos
