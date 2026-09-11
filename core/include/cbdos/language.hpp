#pragma once
// Fase 1 i18n ES-EN — Solo ejecutable (PLAN_FASE1_I18N_ES_EN.md).
// Tablas ES/EN compiladas en Flash. Sin SD, sin LoRa/BBS.
// Reserva TLV: E0 = UI. Los valores de StrId son futuros param_id E0:
// NUNCA renumerar, solo anadir al final.

#include <cstdint>

namespace cbdos {
namespace lang {

enum class Lang : uint8_t {
    ES = 0,
    EN = 1,
};

// Valores fijos = futuro E0:param_id.
enum class StrId : uint16_t {
    // Dashboard (view + 17 apps)
    STR_DASHBOARD = 0x0000,
    STR_APP_BROWSER = 0x0001,
    STR_APP_GALLERY = 0x0002,
    STR_APP_FILES = 0x0003,
    STR_APP_UTILITIES = 0x0004,
    STR_APP_CARTRIDGE = 0x0005,
    STR_APP_LUA = 0x0006,
    STR_APP_EDITOR = 0x0007,
    STR_APP_RADIO = 0x0008,
    STR_APP_FLASHER = 0x0009,
    STR_APP_TERMINAL = 0x000A,
    STR_APP_HID = 0x000B,
    STR_APP_MESHCORE = 0x000C,
    STR_APP_RECORDER = 0x000D,
    STR_APP_MUSIC = 0x000E,
    STR_APP_KERBEROS = 0x000F,
    STR_APP_LOTTIE = 0x0010,
    STR_APP_CONFIG = 0x0011,

    // Config
    STR_CFG_TITLE = 0x0012,
    STR_CFG_SYS_POWER = 0x0013,
    STR_CFG_SYS_POWER_SUB = 0x0014,
    STR_CFG_NET = 0x0015,
    STR_CFG_NET_SUB = 0x0016,
    STR_CFG_DATETIME = 0x0017,
    STR_CFG_DATETIME_SUB = 0x0018,
    STR_CFG_STORAGE = 0x0019,
    STR_CFG_STORAGE_SUB = 0x001A,
    STR_CFG_WALLPAPER = 0x001B,
    STR_CFG_WALLPAPER_SUB = 0x001C,
    STR_CFG_SYSTEM = 0x001D,
    STR_CFG_SYSTEM_SUB = 0x001E,
    STR_CFG_ABOUT = 0x001F,
    STR_CFG_ABOUT_SUB = 0x0020,
    STR_CFG_RESET_NVS = 0x0021,
    STR_CFG_RESET_NVS_SUB = 0x0022,
    STR_CFG_RESET_COUNTDOWN = 0x0023,  // formato: "Soltar para cancelar (%.1fs)"
    STR_CFG_NVS_CLEARED = 0x0024,
    STR_CFG_USB_MODE = 0x0025,
    STR_CFG_USB_HID_SUB = 0x0026,
    STR_CFG_USB_HOST_SUB = 0x0027,
    STR_CFG_USB_TO_HID = 0x0028,
    STR_CFG_USB_TO_HOST = 0x0029,
    STR_CFG_LANGUAGE = 0x002A,
    STR_CFG_LANG_SUB_ES = 0x002B,
    STR_CFG_LANG_SUB_EN = 0x002C,
    STR_CFG_LANG_TO_EN = 0x002D,
    STR_CFG_LANG_TO_ES = 0x002E,

    // About
    STR_ABOUT_TITLE = 0x002F,
    STR_ABOUT_VERSION = 0x0030,
    STR_ABOUT_AUTHOR = 0x0031,
    STR_ABOUT_REPO = 0x0032,
    STR_ABOUT_WEBSITE = 0x0033,
    STR_ABOUT_LICENSE = 0x0034,
    STR_ABOUT_LICENSE_LINK = 0x0035,
    STR_ABOUT_HW = 0x0036,
    STR_ABOUT_CLOSE = 0x0037,

    // MusicPlayer
    STR_MUSIC_TITLE = 0x0038,
    STR_MUSIC_SELECT = 0x0039,
    STR_MUSIC_GREET = 0x003A,
    STR_MUSIC_EMPTY = 0x003B,
    STR_MUSIC_VIEW_PLAYER = 0x003C,  // palabra "Player"
    STR_MUSIC_VIEW_LIST = 0x003D,    // "Lista"/"List"
    STR_MUSIC_PLAYING = 0x003E,
    STR_MUSIC_PAUSED = 0x003F,

    STR_COUNT = 0x0040,
};

// Texto en idioma actual. Fallback a ES si falta clave. Nunca nullptr.
const char* tr(StrId id);

// Idioma en memoria (default ES hasta initLanguage()).
Lang getLanguage();
const char* getLanguageCode();  // "es" | "en"

// Fija idioma en memoria + persiste NVS cbdos_sys/lang (si hay backend).
void setLanguage(Lang lang);
void setLanguageByCode(const char* code);  // "es"/"en", otro -> ES

// Lee NVS cbdos_sys/lang (default "es") y fija memoria. Llamar tras
// initPersistenceBackend() y antes de la UI. Sin backend -> queda ES.
void initLanguage();

// Hook Fase 2 (packs SD /sdcard/lang/*.tlv): stub con fallback a Flash.
bool loadSdPack(const char* path);

}  // namespace lang
}  // namespace cbdos
