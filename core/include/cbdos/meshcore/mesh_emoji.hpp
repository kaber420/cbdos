#pragma once

// Emoji estaticos + Winks Lottie estilo Telegram para MeshCore.
// Filosofia: en el aire solo viajan BYTES de texto UTF-8 (133 chars max).
// La animacion NUNCA viaja: cada extremo la reproduce desde flash/SD.
//  - Carita amarilla  = 1 emoji unicode (4 B) o shortcode (:smile:).
//  - Wink animado     = token de 5-6 B (":star:", ":cat:") o un solo emoji.
// En clientes stock (Android/iOS MeshCore) se ve el texto/emoji original.

#include <cstddef>
#include <string>

namespace cbdos {
namespace meshcore {
namespace emoji {

// ── Tokens wink (los que viajan por LoRa) ─────────────────────
static constexpr const char* kStarToken = ":star:";  // 6 B -> estrella demo
static constexpr const char* kCatToken = ":cat:";    // 5 B -> catmov.json (SD)

static constexpr const char* kStarUtf8 = "\xE2\xAD\x90";          // U+2B50
static constexpr const char* kCatUtf8 = "\xF0\x9F\x90\xB1";       // U+1F431

inline std::string trimSpaces(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

// 0 = estrella, 1 = gato, -1 = no es wink.
inline int winkIdForText(const std::string& text) {
    std::string t = trimSpaces(text);
    if (t == kStarToken || t == kStarUtf8 || t == "*") return 0;
    if (t == kCatToken || t == kCatUtf8) return 1;
    return -1;
}

inline const char* winkTokenForId(int id) {
    return (id == 1) ? kCatToken : kStarToken;
}

inline const char* winkNameForId(int id) {
    return (id == 1) ? "Gato" : "Estrella";
}

// ── Caritas amarillas estaticas (1 emoji => burbuja grande) ────
struct EmojiEntry {
    const char* code;   // shortcode que puede escribir el usuario
    const char* utf8;   // emoji real que viaja (clientes stock lo ven amarillo)
    const char* name;   // nombre para el picker
};

inline const EmojiEntry* emojiTable(size_t& n) {
    static const EmojiEntry kTable[] = {
        {":smile:", "\xF0\x9F\x98\x84", "Sonrisa"},  // U+1F604
        {":laugh:", "\xF0\x9F\x98\x82", "Risa"},     // U+1F602
        {":wink:",  "\xF0\x9F\x98\x89", "Guino"},    // U+1F609
        {":heart:", "\xF0\x9F\x92\x9B", "Corazon"},  // U+1F49B amarillo
        {":like:",  "\xF0\x9F\x91\x8D", "Pulgar"},   // U+1F44D
        {":sad:",   "\xF0\x9F\x98\xA2", "Triste"},   // U+1F622
        {":angry:", "\xF0\x9F\x98\xA1", "Enojo"},    // U+1F621
        {":party:", "\xF0\x9F\x8E\x89", "Fiesta"},   // U+1F389
    };
    n = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

// Indice en emojiTable si el texto ES un solo emoji/shortcode, -1 si no.
inline int singleEmojiIndex(const std::string& text) {
    std::string t = trimSpaces(text);
    size_t n = 0;
    const EmojiEntry* tbl = emojiTable(n);
    for (size_t i = 0; i < n; ++i) {
        if (t == tbl[i].utf8 || t == tbl[i].code) return (int)i;
    }
    return -1;
}

// Texto que se envia al pulsar una carita del picker (el unicode real,
// para maxima compatibilidad con clientes stock).
inline std::string sendTextForEmoji(size_t idx) {
    size_t n = 0;
    const EmojiEntry* tbl = emojiTable(n);
    if (idx < n) return std::string(tbl[idx].utf8);
    return ":smile:";
}

}  // namespace emoji
}  // namespace meshcore
}  // namespace cbdos
