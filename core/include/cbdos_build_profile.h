#pragma once

// ============================================================================
// CBDos Build Profiles & Modular Feature Flags
// Especificación: specs/architecture/plan_perfiles_compilacion_productos_cbdos_y_tablehub.md
// ============================================================================

// Identificadores de Vista Inicial
#define CBDOS_VIEW_DASHBOARD            1
#define CBDOS_VIEW_TABLEHUB_KDS         2
#define CBDOS_VIEW_TABLEHUB_TABLETOP    3

#if defined(CONFIG_CBDOS_PRODUCT_TABLEHUB)
    // ------------------------------------------------------------------------
    // FAMILIA: TABLEHUB (Hostelería y Restauración)
    // ------------------------------------------------------------------------
    #define CBDOS_PRODUCT_NAME          "TableHub"
    #define CBDOS_FEATURE_DASHBOARD     0
    #define CBDOS_FEATURE_KIOSK_LOCK    1
    #define CBDOS_FEATURE_STAFF_PIN     1
    #define CBDOS_FEATURE_TERMINAL      0
    #define CBDOS_FEATURE_FLASHER       0
    #define CBDOS_FEATURE_LAN_RECON     0
    #define CBDOS_FEATURE_CARTRIDGE     0
    #define CBDOS_FEATURE_PICOTTS       0
    #define CBDOS_FEATURE_HELIX_CODECS  0
    #define CBDOS_FEATURE_LUA           1
    #define CBDOS_FEATURE_TABLEHUB_MQTT 1

    #if defined(CONFIG_TABLEHUB_MODE_KDS)
        #define CBDOS_PROFILE_NAME      "TableHub KDS"
        #define CBDOS_FEATURE_KDS_ALERT 1
        #define CBDOS_DEFAULT_VIEW      CBDOS_VIEW_TABLEHUB_KDS
    #elif defined(CONFIG_TABLEHUB_MODE_TABLETOP)
        #define CBDOS_PROFILE_NAME      "TableHub Tabletop"
        #define CBDOS_FEATURE_KDS_ALERT 0
        #define CBDOS_DEFAULT_VIEW      CBDOS_VIEW_TABLEHUB_TABLETOP
    #else
        #error "Debe especificar un modo de TableHub: CONFIG_TABLEHUB_MODE_KDS o CONFIG_TABLEHUB_MODE_TABLETOP"
    #endif

#else
    // ------------------------------------------------------------------------
    // FAMILIA: CYBERDECK (Default)
    // ------------------------------------------------------------------------
    #define CBDOS_PRODUCT_NAME          "CBDos Cyberdeck"
    #define CBDOS_FEATURE_DASHBOARD     1
    #define CBDOS_FEATURE_KIOSK_LOCK    0
    #define CBDOS_FEATURE_STAFF_PIN     0
    #define CBDOS_FEATURE_TABLEHUB_MQTT 0
    #define CBDOS_FEATURE_KDS_ALERT     0
    #define CBDOS_DEFAULT_VIEW          CBDOS_VIEW_DASHBOARD

    #if defined(CONFIG_CBDOS_PROFILE_LITE)
        #define CBDOS_PROFILE_NAME      "Cyberdeck Lite"
        #define CBDOS_FEATURE_TERMINAL  1
        #define CBDOS_FEATURE_FLASHER   0
        #define CBDOS_FEATURE_LAN_RECON 0
        #define CBDOS_FEATURE_CARTRIDGE 0
        #define CBDOS_FEATURE_PICOTTS   0
        #define CBDOS_FEATURE_HELIX_CODECS 0
        #define CBDOS_FEATURE_LUA       0
    #else
        #ifndef CONFIG_CBDOS_PROFILE_FULL
            #define CONFIG_CBDOS_PROFILE_FULL 1
        #endif
        #define CBDOS_PROFILE_NAME      "Cyberdeck Full"
        #define CBDOS_FEATURE_TERMINAL  1
        #define CBDOS_FEATURE_FLASHER   1
        #define CBDOS_FEATURE_LAN_RECON 1
        #define CBDOS_FEATURE_CARTRIDGE 1
        #define CBDOS_FEATURE_PICOTTS   1
        #define CBDOS_FEATURE_HELIX_CODECS 1
        #define CBDOS_FEATURE_LUA       1
    #endif

#endif
