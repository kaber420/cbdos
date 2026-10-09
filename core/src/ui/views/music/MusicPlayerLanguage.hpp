#pragma once

#include "cbdos/language.hpp"
#include <cstdint>
#include <lvgl.h>

namespace cbdos {
namespace music {
namespace lang {

enum class Str : uint16_t {
    TITLE,          // "Musica SD" / "SD Music"
    SELECT_TRACK,   // "Selecciona una cancion" / "Select a track"
    GREET_BITBOT,   // "Toca a BitBot para saludar" / "Tap BitBot to say hello"
    EMPTY_SD,       // Mensaje de MicroSD sin canciones
    VIEW_PLAYER,    // "Player"
    VIEW_LIST,      // "Lista" / "List"
    PLAYING,        // "Reproduciendo..." / "Playing..."
    PAUSED,         // "En Pausa" / "Paused"
    COUNT
};

inline const char* tr(Str id) {
    bool isEn = (cbdos::lang::getLanguage() == cbdos::lang::Lang::EN);

    if (isEn) {
        switch (id) {
            case Str::TITLE:        return "SD Music";
            case Str::SELECT_TRACK: return "Select a track";
            case Str::GREET_BITBOT: return "Tap BitBot to say hello";
            case Str::EMPTY_SD:     return "No songs found on MicroSD\n(Copy .mp3 files to /sdcard or /sdcard/musica)";
            case Str::VIEW_PLAYER:  return "Player";
            case Str::VIEW_LIST:    return "List";
            case Str::PLAYING:      return "Playing...";
            case Str::PAUSED:       return "Paused";
            default: break;
        }
    }

    // Default / Spanish (sin acentos para fuentes montserrat)
    switch (id) {
        case Str::TITLE:        return "Musica SD";
        case Str::SELECT_TRACK: return "Selecciona una cancion";
        case Str::GREET_BITBOT: return "Toca a BitBot para saludar";
        case Str::EMPTY_SD:     return "No se encontraron canciones en la MicroSD\n(Copia archivos .mp3 en /sdcard o /sdcard/musica)";
        case Str::VIEW_PLAYER:  return "Player";
        case Str::VIEW_LIST:    return "Lista";
        case Str::PLAYING:      return "Reproduciendo...";
        case Str::PAUSED:       return "En Pausa";
        default: break;
    }
    return "";
}

} // namespace lang
} // namespace music
} // namespace cbdos
