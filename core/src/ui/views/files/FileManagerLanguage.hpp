#pragma once

#include "cbdos/language.hpp"
#include <cstdint>
#include <lvgl.h>

namespace cbdos {
namespace files {
namespace lang {

enum class Str : uint16_t {
    TITLE,              // "Explorador" / "File Explorer"
    TAB_SD,             // "MicroSD" / "MicroSD"
    TAB_FLASH,          // "Flash Interna" / "Internal Flash"
    CALC,               // "Calculando..." / "Calculating..."
    EMPTY_DEL,          // "Papelera vacia / No hay archivos borrados"
    EMPTY,              // "Carpeta vacia" / "Empty folder"
    DIR,                // "<DIR>"
    CANCEL,             // "Cancelar" / "Cancel"
    DEL_BTN,            // "Eliminar" / "Delete"
    DEL_TITLE,          // "Confirmar Eliminacion" / "Confirm Deletion"
    FOR_TITLE,          // "Recuperacion Forense" / "Forensic Recovery"
    BACKUP_FLASH,       // "Backup a Flash Interna" / "Backup to Flash"
    RESTORE_SD,         // "Restaurar en MicroSD" / "Restore to SD"
    CLOSE,              // "Cerrar" / "Close"
    COUNT
};

inline const char* tr(Str id) {
    bool isEn = (cbdos::lang::getLanguage() == cbdos::lang::Lang::EN);

    if (isEn) {
        switch (id) {
            case Str::TITLE:        return "File Explorer";
            case Str::TAB_SD:       return "MicroSD";
            case Str::TAB_FLASH:    return "Internal Flash";
            case Str::CALC:         return "Calculating...";
            case Str::EMPTY_DEL:    return "Trash empty / No deleted files";
            case Str::EMPTY:        return "Empty directory";
            case Str::DIR:          return "<DIR>";
            case Str::CANCEL:       return "Cancel";
            case Str::DEL_BTN:      return "Delete";
            case Str::DEL_TITLE:    return "Confirm Deletion";
            case Str::FOR_TITLE:    return "Forensic Recovery";
            case Str::BACKUP_FLASH: return "Backup to Internal Flash";
            case Str::RESTORE_SD:   return "Restore to MicroSD";
            case Str::CLOSE:        return "Close";
            default: break;
        }
    }

    // Default / Spanish (sin acentos para compatibilidad con montserrat)
    switch (id) {
        case Str::TITLE:        return "Explorador";
        case Str::TAB_SD:       return "MicroSD";
        case Str::TAB_FLASH:    return "Flash Interna";
        case Str::CALC:         return "Calculando...";
        case Str::EMPTY_DEL:    return "Papelera vacia / Sin archivos borrados";
        case Str::EMPTY:        return "Carpeta vacia";
        case Str::DIR:          return "<DIR>";
        case Str::CANCEL:       return "Cancelar";
        case Str::DEL_BTN:      return "Eliminar";
        case Str::DEL_TITLE:    return "Confirmar Eliminacion";
        case Str::FOR_TITLE:    return "Recuperacion Forense";
        case Str::BACKUP_FLASH: return "Backup a Flash Interna";
        case Str::RESTORE_SD:   return "Restaurar en MicroSD";
        case Str::CLOSE:        return "Cerrar";
        default: break;
    }
    return "";
}

} // namespace lang
} // namespace files
} // namespace cbdos
