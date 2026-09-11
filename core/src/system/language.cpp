#include "cbdos/language.hpp"
#include "cbdos/persistence.hpp"
#include <cstring>

namespace cbdos {
namespace lang {

namespace {

// Orden = orden de StrId (0x0000..0x003F). Misma forma que tendran los packs SD.
// ES sin acentos (estilo del codigo actual, evita tofu en montserrat 12/14/16).
static const char* const lang_es[] = {
    "Dashboard",                                   // 0x0000
    "Navegador",                                   // 0x0001
    "Galeria",                                     // 0x0002
    "Archivos",                                    // 0x0003
    "Utilidades",                                  // 0x0004
    "Cartuchos",                                   // 0x0005
    "Lua Runner",                                  // 0x0006
    "Editor",                                      // 0x0007
    "Radio Online",                                // 0x0008
    "Flasheador",                                  // 0x0009
    "Terminal",                                    // 0x000A
    "HID Control",                                 // 0x000B
    "MeshCore",                                    // 0x000C
    "Grabadora",                                   // 0x000D
    "Musica",                                      // 0x000E
    "Kerberos FIDO",                               // 0x000F
    "Lottie Test",                                 // 0x0010
    "Configuracion",                               // 0x0011
    "Configuracion",                               // 0x0012 CFG_TITLE
    "Sistema y Energia",                           // 0x0013
    "Apagado, reinicio, standby y auto-suspension",// 0x0014
    "Redes e Interfaces",                          // 0x0015
    "Wi-Fi, ESP-NOW, LoRa y Ranuras de Hardware",  // 0x0016
    "Fecha y Hora",                                // 0x0017
    "Zona horaria, horario de verano y NTP",       // 0x0018
    "Almacenamiento",                              // 0x0019
    "Gestion de MicroSD, Flash y USB",             // 0x001A
    "Fondo de Pantalla",                           // 0x001B
    "Elegir wallpaper de SD o Flash",              // 0x001C
    "Sistema",                                     // 0x001D
    "Diagnostico de hardware y memoria",           // 0x001E
    "Acerca de CBDos",                             // 0x001F
    "v0.2.3-dev, Licencia GPLv3 y Repo",           // 0x0020
    "Resetear NVS",                                // 0x0021
    "Manten presionado 3s para borrar",            // 0x0022
    "Soltar para cancelar (%.1fs)",                // 0x0023 formato
    "NVS borrado completamente",                   // 0x0024
    "Modo USB",                                    // 0x0025
    "Actual: HID teclado/raton (tocar=cambiar a HOST)",  // 0x0026
    "Actual: HOST modem/flasher (tocar=cambiar a HID)",  // 0x0027
    "Modo HOST->HID: reiniciando...",              // 0x0028
    "Modo HID->HOST: reiniciando...",              // 0x0029
    "Idioma",                                      // 0x002A
    "Actual: Espanol (tocar para elegir)",         // 0x002B
    "Current: English (tap to choose)",            // 0x002C (se muestra en EN)
    "Language: English. Rebooting...",             // 0x002D
    "Idioma: Espanol. Reiniciando...",             // 0x002E
    "CyBerDeck OS",                                // 0x002F
    "Version 0.2.3-dev (Universal Core)",          // 0x0030
    "Autor / Mantenedor",                          // 0x0031
    "Repositorio Oficial",                         // 0x0032
    "Sitio Web",                                   // 0x0033
    "Licencia de Software",                        // 0x0034
    "Enlace de la Licencia",                       // 0x0035
    "Hardware Target",                             // 0x0036
    "Cerrar",                                      // 0x0037
    "Musica SD",                                   // 0x0038
    "Selecciona una cancion",                      // 0x0039
    "Toca a BitBot para saludar",                  // 0x003A
    "No se encontraron canciones en la MicroSD\n(Copia archivos .mp3 en /sdcard o /sdcard/musica)", // 0x003B
    "Player",                                      // 0x003C
    "Lista",                                       // 0x003D
    "Reproduciendo...",                            // 0x003E
    "En Pausa",                                    // 0x003F
    "Espanol",                                     // 0x0040 (sin ñ: montserrat 12/14/16 no la trae)
    "English",                                     // 0x0041
    // WiFi 0x0042-0x004B
    "WiFi",
    "Activar Wi-Fi (Coprocesador)",
    "Estado: Wi-Fi Habilitado",
    "Estado: Desactivado (Modo Seguro Offline)",
    "SSID (Nombre de Red):",
    "Usar IP Estatica",
    "Direccion IP:",
    "Guardar y Conectar",
    "WiFi guardado correctamente",
    "Error al guardar WiFi",
    // Storage 0x004C-0x0065
    "Flash Interna (NOR)",
    "SISTEMA",
    "Usado: %.1f MB / Total: %.1f MB (%.0f%%)",
    "Tarjeta MicroSD",
    "MONTADA",
    "NO DETECTADA",
    "Libre: %.2f GB / Total: %.2f GB (FAT32/exFAT)",
    "Libre: %.1f MB / Total: %.1f MB (FAT32)",
    "Inserta una tarjeta MicroSD formateada en FAT32.",
    "Recargar",
    "Expulsar",
    "Formato",
    "Almacenamiento USB (HS)",
    "EN ESPERA",
    "Puerto USB OTG listo para unidades Mass Storage (MSC).",
    "MicroSD detectada y montada",
    "No se detecto tarjeta MicroSD",
    "MicroSD desmontada con seguridad",
    "Error al desmontar MicroSD",
    "Formatear MicroSD",
    "Formatear la MicroSD a FAT32?\n\nADVERTENCIA: se eliminaran todos los archivos.",
    "Formatear",
    "Cancelar",
    "Formateando a FAT32...",
    "MicroSD formateada y montada OK!",
    "Error al formatear MicroSD",
};

static const char* const lang_en[] = {
    "Dashboard",
    "Browser",
    "Gallery",
    "Files",
    "Utilities",
    "Cartridges",
    "Lua Runner",
    "Editor",
    "Online Radio",
    "Flasher",
    "Terminal",
    "HID Control",
    "MeshCore",
    "Recorder",
    "Music",
    "Kerberos FIDO",
    "Lottie Test",
    "Settings",
    "Settings",
    "System & Power",
    "Shutdown, reboot, standby and auto-sleep",
    "Networks & Interfaces",
    "Wi-Fi, ESP-NOW, LoRa and Hardware Slots",
    "Date & Time",
    "Timezone, daylight saving and NTP",
    "Storage",
    "MicroSD, Flash and USB management",
    "Wallpaper",
    "Choose wallpaper from SD or Flash",
    "System",
    "Hardware and memory diagnostics",
    "About CBDos",
    "v0.2.3-dev, GPLv3 License and Repo",
    "Reset NVS",
    "Hold 3s to erase",
    "Release to cancel (%.1fs)",
    "NVS fully erased",
    "USB Mode",
    "Current: HID keyboard/mouse (tap=switch to HOST)",
    "Current: HOST modem/flasher (tap=switch to HID)",
    "HOST->HID mode: rebooting...",
    "HID->HOST mode: rebooting...",
    "Language",
    "Actual: Espanol (tocar para elegir)",
    "Current: English (tap to choose)",
    "Language: English. Rebooting...",
    "Idioma: Espanol. Reiniciando...",
    "CyBerDeck OS",
    "Version 0.2.3-dev (Universal Core)",
    "Author / Maintainer",
    "Official Repository",
    "Website",
    "Software License",
    "License Link",
    "Hardware Target",
    "Close",
    "SD Music",
    "Select a song",
    "Tap BitBot to say hi",
    "No songs found on MicroSD\n(Copy .mp3 files to /sdcard or /sdcard/music)",
    "Player",
    "List",
    "Playing...",
    "Paused",
    "Espanol",
    "English",
    // WiFi 0x0042-0x004B
    "WiFi",
    "Enable Wi-Fi (Coprocessor)",
    "Status: Wi-Fi Enabled",
    "Status: Disabled (Offline Safe Mode)",
    "SSID (Network Name):",
    "Use Static IP",
    "IP Address:",
    "Save and Connect",
    "WiFi saved successfully",
    "Error saving WiFi",
    // Storage 0x004C-0x0065
    "Internal Flash (NOR)",
    "SYSTEM",
    "Used: %.1f MB / Total: %.1f MB (%.0f%%)",
    "MicroSD Card",
    "MOUNTED",
    "NOT FOUND",
    "Free: %.2f GB / Total: %.2f GB (FAT32/exFAT)",
    "Free: %.1f MB / Total: %.1f MB (FAT32)",
    "Insert a FAT32-formatted MicroSD card.",
    "Reload",
    "Eject",
    "Format",
    "USB Storage (HS)",
    "STANDBY",
    "USB OTG port ready for Mass Storage (MSC) drives.",
    "MicroSD detected and mounted",
    "No MicroSD card detected",
    "MicroSD safely unmounted",
    "Error unmounting MicroSD",
    "Format MicroSD",
    "Format MicroSD to FAT32?\n\nWARNING: all files will be erased.",
    "Format",
    "Cancel",
    "Formatting to FAT32...",
    "MicroSD formatted and mounted OK!",
    "Error formatting MicroSD",
};

static_assert(sizeof(lang_es) / sizeof(lang_es[0]) == (size_t)StrId::STR_COUNT,
              "lang_es fuera de sync con StrId");
static_assert(sizeof(lang_en) / sizeof(lang_en[0]) == (size_t)StrId::STR_COUNT,
              "lang_en fuera de sync con StrId");

static Lang s_lang = Lang::ES;

}  // namespace

const char* tr(StrId id) {
    uint16_t i = (uint16_t)id;
    if (i >= (uint16_t)StrId::STR_COUNT) return "?";
    const char* s = (s_lang == Lang::EN) ? lang_en[i] : lang_es[i];
    if (!s || !*s) s = lang_es[i];  // fallback ES
    return s ? s : "?";
}

Lang getLanguage() { return s_lang; }

const char* getLanguageCode() { return (s_lang == Lang::EN) ? "en" : "es"; }

void setLanguage(Lang lang) {
    s_lang = lang;
    auto* backend = persistence::getBackend();
    if (backend && backend->begin("cbdos_sys", false)) {
        backend->setString("lang", (lang == Lang::EN) ? "en" : "es");
        backend->end();
    }
}

void setLanguageByCode(const char* code) {
    if (code && (std::strcmp(code, "en") == 0 || std::strcmp(code, "EN") == 0)) {
        setLanguage(Lang::EN);
    } else {
        setLanguage(Lang::ES);
    }
}

void initLanguage() {
    auto* backend = persistence::getBackend();
    if (!backend || !backend->begin("cbdos_sys", true)) return;
    std::string code = backend->getString("lang", "es");
    backend->end();
    s_lang = (code == "en" || code == "EN") ? Lang::EN : Lang::ES;
}

bool loadSdPack(const char* path) {
    (void)path;
    return false;  // Fase 2: packs /sdcard/lang/*.tlv. Fallback a Flash.
}

}  // namespace lang
}  // namespace cbdos
