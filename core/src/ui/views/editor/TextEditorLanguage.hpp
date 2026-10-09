#pragma once

#include "cbdos/language.hpp"
#include <cstdint>
#include <lvgl.h>

namespace cbdos {
namespace editor {
namespace lang {

enum class Str : uint16_t {
    TITLE,          // "Editor"
    SAVE_AS,        // "Guardar Archivo Como..." / "Save File As..."
    FLASH,          // "Flash Interna" / "Internal Flash"
    SD,             // "MicroSD"
    CANCEL,         // "Cancelar" / "Cancel"
    SAVE,           // "Guardar" / "Save"
    OPEN,           // "Abrir Archivo" / "Open File"
    EMPTY,          // "No se encontraron archivos..."
    COUNT
};

inline const char* tr(Str id) {
    bool isEn = (cbdos::lang::getLanguage() == cbdos::lang::Lang::EN);

    if (isEn) {
        switch (id) {
            case Str::TITLE:    return "Editor";
            case Str::SAVE_AS:  return "Save File As...";
            case Str::FLASH:    return "Internal Flash";
            case Str::SD:       return "MicroSD";
            case Str::CANCEL:   return "Cancel";
            case Str::SAVE:     return "Save";
            case Str::OPEN:     return "Open File";
            case Str::EMPTY:    return "No text files or scripts\nfound on MicroSD.";
            default: break;
        }
    }

    // Default / Spanish (sin acentos para montserrat)
    switch (id) {
        case Str::TITLE:    return "Editor";
        case Str::SAVE_AS:  return "Guardar Archivo Como...";
        case Str::FLASH:    return "Flash Interna";
        case Str::SD:       return "MicroSD";
        case Str::CANCEL:   return "Cancelar";
        case Str::SAVE:     return "Guardar";
        case Str::OPEN:     return "Abrir Archivo";
        case Str::EMPTY:    return "No se encontraron archivos de texto\no scripts en la MicroSD.";
        default: break;
    }
    return "";
}

} // namespace lang
} // namespace editor
} // namespace cbdos
