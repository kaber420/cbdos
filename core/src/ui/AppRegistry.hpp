#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace cbdos {
namespace ui {

enum class AppOrigin {
    SYSTEM_NATIVE,   // Compilada en el firmware C++
    LUA_SCRIPT,      // Dinámica desde MicroSD (.luapp)
    CARTRIDGE        // Ejecutable / partición dedicada
};

enum class IconType {
    SYSTEM_BUILTIN,  // Icono del sistema gestionado por SystemIcons
    FS_IMAGE,        // Ruta a imagen binaria en Filesystem (SD / Flash)
    LVGL_SYMBOL      // Símbolo tipográfico vectorial LVGL (ej. LV_SYMBOL_AUDIO)
};

struct AppIconDescriptor {
    IconType type = IconType::LVGL_SYMBOL;
    std::string source = "";         // ID de icono del sistema o ruta en filesystem
    std::string fallbackSymbol = ""; // Símbolo LVGL para fallback elegante
};

struct AppDescriptor {
    std::string id;
    std::string title;
    AppOrigin origin = AppOrigin::SYSTEM_NATIVE;
    AppIconDescriptor icon;
    uint32_t accentColor = 0x3B82F6;
    bool visibleInDashboard = true;
    std::function<void()> launchAction;
};

class AppRegistry {
public:
    static AppRegistry& getInstance();

    // Inicializa el catálogo con las apps del sistema respetando los flags de perfil de compilación
    void initSystemApps();

    // Registro dinámico
    void registerApp(const AppDescriptor& app);
    void unregisterApp(const std::string& id);

    // Obtener apps visibles para el Dashboard
    const std::vector<AppDescriptor>& getVisibleApps() const { return m_apps; }

    // Buscar descriptor por ID
    const AppDescriptor* findApp(const std::string& id) const;

    // Rescanear aplicaciones externas (.luapp en SD / LittleFS)
    void rescanExternalApps();

    // Limpiar catálogo completo
    void clear();

private:
    AppRegistry() = default;
    ~AppRegistry() = default;
    AppRegistry(const AppRegistry&) = delete;
    AppRegistry& operator=(const AppRegistry&) = delete;

    std::vector<AppDescriptor> m_apps;
    bool m_initialized = false;
};

} // namespace ui
} // namespace cbdos
