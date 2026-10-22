#include "TerminalDisplay.hpp"
#include "../../themes/DefaultTheme.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace cbdos {
namespace ui {

static constexpr uint32_t kBg      = 0x121417;   // Fondo terminal oscuro
static constexpr uint32_t kFg      = 0xD3D7CF;   // Texto por defecto

// Conversión UTF-8
static int utf8_put(uint32_t cp, char* o) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | (cp >> 6)); o[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        o[0] = (char)(0xE0 | (cp >> 12)); o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (char)(0x80 | (cp & 0x3F)); return 3;
    }
    o[0] = (char)(0xF0 | (cp >> 18)); o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); o[3] = (char)(0x80 | (cp & 0x3F)); return 4;
}

TerminalDisplay::TerminalDisplay() = default;

TerminalDisplay::~TerminalDisplay() {
    destroy();
}

void TerminalDisplay::destroy() {
    if (m_refreshTimer) {
        lv_timer_delete(m_refreshTimer);
        m_refreshTimer = nullptr;
    }
    if (m_vt) {
        vterm_free(m_vt);
        m_vt = nullptr;
        m_vs = nullptr;
    }
    m_container = nullptr;
    m_lblText = nullptr;
}

bool TerminalDisplay::create(lv_obj_t* parent) {
    if (!parent) return false;

    // Contenedor principal con scroll nativo táctil de LVGL
    m_container = lv_obj_create(parent);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_container, 1);
    lv_obj_set_style_bg_opa(m_container, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(m_container, lv_color_hex(kBg), 0);
    lv_obj_set_style_border_width(m_container, 0, 0);
    lv_obj_set_style_pad_all(m_container, 4, 0);
    lv_obj_set_scrollbar_mode(m_container, LV_SCROLLBAR_MODE_AUTO);

    // Etiqueta con soporte de recoloreado inline (#RRGGBB texto#)
    m_lblText = lv_label_create(m_container);
    lv_obj_set_size(m_lblText, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_recolor(m_lblText, true);
    lv_obj_set_style_text_font(m_lblText, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(m_lblText, lv_color_hex(kFg), 0);
    lv_label_set_text(m_lblText, "#34e2e2 [CBDos VT100 Terminal]#\n");

    // Inicializar emulador libvterm (64 cols x 18 rows)
    m_rows = 18;
    m_cols = 64;
    m_vt = vterm_new(m_rows, m_cols);
    if (!m_vt) return false;

    vterm_set_utf8(m_vt, 1);
    m_vs = vterm_obtain_screen(m_vt);
    vterm_screen_enable_altscreen(m_vs, 1);

    vterm_screen_set_callbacks(m_vs, &s_screenCallbacks, this);
    vterm_screen_reset(m_vs, 1);

    // Temporizador de refresco desacoplado (30ms limitador a ~33 FPS fluidos)
    m_refreshTimer = lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<TerminalDisplay*>(lv_timer_get_user_data(t));
        if (self && self->m_dirty) {
            self->m_dirty = false;
            self->refreshDisplay();
        }
    }, 30, this);

    return true;
}

void TerminalDisplay::appendText(const char* text, size_t len) {
    if (!m_vt || !text || len == 0 || m_hold) return;

    if (m_altScreen) {
        vterm_input_write(m_vt, text, len);
    } else {
        size_t start = 0;
        for (size_t i = 0; i < len; i++) {
            if (text[i] == '\n') {
                const char prev = (i > 0) ? text[i - 1] : 0;
                vterm_input_write(m_vt, text + start, i - start);
                start = i;
                if (prev != '\r') {
                    vterm_input_write(m_vt, "\r", 1);
                }
            }
        }
        vterm_input_write(m_vt, text + start, len - start);
    }
    vterm_screen_flush_damage(m_vs);
    m_dirty = true;
}

void TerminalDisplay::clear() {
    if (!m_vt) return;
    const char clearSeq[] = "\x1b[2J\x1b[H";
    vterm_input_write(m_vt, clearSeq, sizeof(clearSeq) - 1);
    vterm_screen_flush_damage(m_vs);
    m_dirty = true;
}

bool TerminalDisplay::toggleHold() {
    m_hold = !m_hold;
    return m_hold;
}

bool TerminalDisplay::saveLogToSd(std::string& outMessage) {
    if (!m_vs) {
        outMessage = "[ERR] Terminal no inicializada.\n";
        return false;
    }

    FILE* f = fopen("/sd/terminal.log", "w");
    if (!f) f = fopen("/spiffs/terminal.log", "w");
    if (!f) {
        outMessage = "[ERR] No se pudo crear archivo en SD/SPIFFS.\n";
        return false;
    }

    for (int r = 0; r < m_rows; r++) {
        char lineBuf[256];
        int len = 0;
        for (int c = 0; c < m_cols; c++) {
            VTermScreenCell sc;
            VTermPos p = {r, c};
            if (vterm_screen_get_cell(m_vs, p, &sc) && sc.chars[0]) {
                len += utf8_put(sc.chars[0], lineBuf + len);
            } else {
                lineBuf[len++] = ' ';
            }
        }
        lineBuf[len++] = '\n';
        fwrite(lineBuf, 1, len, f);
    }

    fclose(f);
    outMessage = "[SYS] Terminal guardada en terminal.log\n";
    return true;
}

void TerminalDisplay::refreshDisplay() {
    if (!m_vt || !m_vs || !m_lblText) return;

    std::string out;
    out.reserve(m_rows * (m_cols + 16));

    uint32_t lastRgb = 0xFFFFFFFF;
    bool inColor = false;

    for (int r = 0; r < m_rows; r++) {
        int lastNonSpace = -1;
        for (int c = 0; c < m_cols; c++) {
            VTermScreenCell sc;
            VTermPos pos = {r, c};
            if (vterm_screen_get_cell(m_vs, pos, &sc) && sc.chars[0] && sc.chars[0] != ' ') {
                lastNonSpace = c;
            }
        }

        int colLimit = (lastNonSpace >= 0) ? lastNonSpace + 1 : 0;
        if (m_cursorVisible && r == m_cursorPos.row && m_cursorPos.col >= colLimit) {
            colLimit = std::min(m_cursorPos.col + 1, m_cols);
        }

        for (int c = 0; c < colLimit; c++) {
            bool isCursor = (m_cursorVisible && r == m_cursorPos.row && c == m_cursorPos.col);

            VTermScreenCell sc;
            VTermPos pos = {r, c};
            uint32_t ch = ' ';
            uint32_t fgRgb = 0xD3D7CF; // Default FG (gris claro)
            uint32_t bgRgb = 0x121417; // Default BG (oscuro)

            if (vterm_screen_get_cell(m_vs, pos, &sc)) {
                if (sc.chars[0]) ch = sc.chars[0];
                VTermColor fgCol = sc.fg;
                VTermColor bgCol = sc.bg;
                if (!VTERM_COLOR_IS_DEFAULT_FG(&fgCol)) {
                    vterm_screen_convert_color_to_rgb(m_vs, &fgCol);
                    fgRgb = ((uint32_t)fgCol.rgb.red << 16) | ((uint32_t)fgCol.rgb.green << 8) | fgCol.rgb.blue;
                }
                if (!VTERM_COLOR_IS_DEFAULT_BG(&bgCol)) {
                    vterm_screen_convert_color_to_rgb(m_vs, &bgCol);
                    bgRgb = ((uint32_t)bgCol.rgb.red << 16) | ((uint32_t)bgCol.rgb.green << 8) | bgCol.rgb.blue;
                }
                if (sc.attrs.reverse) {
                    std::swap(fgRgb, bgRgb);
                }
            }

            if (isCursor) {
                // Invertir colores o dibujar bloque para el cursor
                fgRgb = 0x121417;
                bgRgb = 0xD3D7CF;
                if (ch == ' ') ch = '_';
            }

            if (fgRgb != lastRgb) {
                if (inColor) {
                    out.push_back('#');
                }
                char hexBuf[10];
                snprintf(hexBuf, sizeof(hexBuf), "#%06x ", (unsigned int)(fgRgb & 0xFFFFFF));
                out += hexBuf;
                lastRgb = fgRgb;
                inColor = true;
            }

            if (ch == '#') {
                out += "##";
            } else if (ch < 0x80) {
                out.push_back((char)ch);
            } else {
                char ubuf[5];
                int ulen = utf8_put(ch, ubuf);
                out.append(ubuf, ulen);
            }
        }

        if (r < m_rows - 1) {
            out.push_back('\n');
        }
    }

    if (inColor) {
        out.push_back('#');
    }

    lv_label_set_text(m_lblText, out.c_str());
}

// ------------------- Callbacks VTerm -------------------

const VTermScreenCallbacks TerminalDisplay::s_screenCallbacks = {
    &TerminalDisplay::cbDamage,
    nullptr, // moverect
    &TerminalDisplay::cbMoveCursor,
    &TerminalDisplay::cbSetTermProp,
    &TerminalDisplay::cbBell,
    nullptr, // resize
    &TerminalDisplay::cbSbPushline,
    &TerminalDisplay::cbSbPopline,
    &TerminalDisplay::cbSbClear,
};

int TerminalDisplay::cbDamage(VTermRect, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (self) self->m_dirty = true;
    return 1;
}

int TerminalDisplay::cbMoveCursor(VTermPos pos, VTermPos, int visible, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (!self) return 1;
    self->m_cursorPos = pos;
    self->m_cursorVisible = (visible != 0);
    self->m_dirty = true;
    return 1;
}

int TerminalDisplay::cbSetTermProp(VTermProp prop, VTermValue* val, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (!self) return 1;
    switch (prop) {
        case VTERM_PROP_CURSORVISIBLE:
            self->m_cursorVisible = val->boolean;
            self->m_dirty = true;
            break;
        case VTERM_PROP_ALTSCREEN:
            self->m_altScreen = val->boolean;
            self->m_dirty = true;
            break;
        default: break;
    }
    return 1;
}

int TerminalDisplay::cbBell(void*) {
    return 1;
}

int TerminalDisplay::cbSbPushline(int, const VTermScreenCell*, void*) {
    return 1;
}

int TerminalDisplay::cbSbPopline(int, VTermScreenCell*, void*) {
    return 0;
}

int TerminalDisplay::cbSbClear(void*) {
    return 1;
}

} // namespace ui
} // namespace cbdos
