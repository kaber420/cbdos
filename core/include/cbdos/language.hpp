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

    // Selector de idioma (modal)
    STR_LANG_SPANISH = 0x0040,  // "Espanol" (igual en ambos; sin ñ por montserrat)
    STR_LANG_ENGLISH = 0x0041,  // "English" (igual en ambos)

    // WiFiConfig
    STR_WIFI_TITLE = 0x0042,
    STR_WIFI_ENABLE = 0x0043,
    STR_WIFI_ENABLED = 0x0044,
    STR_WIFI_DISABLED = 0x0045,
    STR_WIFI_SSID = 0x0046,
    STR_WIFI_STATIC = 0x0047,
    STR_WIFI_IP = 0x0048,
    STR_WIFI_SAVE = 0x0049,
    STR_WIFI_SAVED = 0x004A,
    STR_WIFI_SAVE_ERR = 0x004B,

    // StorageConfig
    STR_ST_FLASH_TITLE = 0x004C,  // sufijo tras LV_SYMBOL_DRIVE
    STR_ST_SYSTEM = 0x004D,
    STR_ST_USED_FMT = 0x004E,     // formato con %.1f/%.0f
    STR_ST_SD_TITLE = 0x004F,     // sufijo tras LV_SYMBOL_SD_CARD
    STR_ST_MOUNTED = 0x0050,
    STR_ST_NOTFOUND = 0x0051,
    STR_ST_FREE_GB = 0x0052,      // formato
    STR_ST_FREE_MB = 0x0053,      // formato
    STR_ST_INSERT = 0x0054,
    STR_ST_RELOAD = 0x0055,       // palabra tras LV_SYMBOL_REFRESH
    STR_ST_EJECT = 0x0056,        // palabra tras LV_SYMBOL_EJECT
    STR_ST_FORMAT = 0x0057,       // palabra tras LV_SYMBOL_TRASH
    STR_ST_USB_TITLE = 0x0058,    // sufijo tras LV_SYMBOL_USB
    STR_ST_STANDBY = 0x0059,
    STR_ST_USB_SUB = 0x005A,
    STR_ST_MOUNTED_OK = 0x005B,
    STR_ST_NO_SD = 0x005C,
    STR_ST_UNMOUNT_OK = 0x005D,
    STR_ST_UNMOUNT_ERR = 0x005E,
    STR_ST_FMT_TITLE = 0x005F,
    STR_ST_FMT_TEXT = 0x0060,
    STR_ST_FMT_BTN = 0x0061,
    STR_ST_CANCEL = 0x0062,
    STR_ST_FORMATTING = 0x0063,
    STR_ST_FMT_OK = 0x0064,
    STR_ST_FMT_ERR = 0x0065,

    // NetworkManager (radio slots + Wi-Fi STA + LoRa backpack + USB modem)
    STR_NET_SLOT0 = 0x0066,       // sufijo tras LV_SYMBOL_WIFI
    STR_NET_MODE = 0x0067,
    STR_NET_MODES = 0x0068,       // opciones dropdown separadas por \n
    STR_NET_CH = 0x0069,          // formato "Canal RF: %u"
    STR_NET_TX = 0x006A,          // formato "Potencia TX: +%d dBm"
    STR_NET_SWEEP = 0x006B,       // palabra tras LV_SYMBOL_REFRESH
    STR_NET_SWEEP_IDLE = 0x006C,
    STR_NET_SCAN = 0x006D,        // palabra tras LV_SYMBOL_WIFI
    STR_NET_SCAN_IDLE = 0x006E,
    STR_NET_RADIO_ON = 0x006F,
    STR_NET_RADIO_OFF = 0x0070,
    STR_NET_MODE_SAVED = 0x0071,
    STR_NET_SCANNING = 0x0072,    // texto tras emoji
    STR_NET_NO_APS = 0x0073,
    STR_NET_CONNECT_TO = 0x0074,  // formato "Conectar a %s"
    STR_NET_PASS_PH = 0x0075,     // placeholder (sin ñ por montserrat)
    STR_NET_CONNECT = 0x0076,
    STR_NET_CONNECTING = 0x0077,
    STR_NET_SWEEPING = 0x0078,    // texto tras emoji
    STR_NET_SWEEP_DONE = 0x0079,  // formato
    STR_NET_SLOT1 = 0x007A,       // sufijo tras LV_SYMBOL_DRIVE
    STR_NET_BP_OFF = 0x007B,      // texto tras emoji
    STR_NET_BP_ON = 0x007C,       // texto tras emoji
    STR_NET_SLOT2 = 0x007D,       // sufijo tras LV_SYMBOL_USB (sin acento)
    STR_NET_USB_INFO = 0x007E,    // formato con emojis
    STR_NET_USB_MODEM = 0x007F,   // alias por defecto (sin acento)
    STR_NET_USB_OFF = 0x0080,     // texto tras emoji

    STR_COUNT = 0x0081,
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
