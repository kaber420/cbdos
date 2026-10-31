#include "TerminalDisplay.hpp"
#include "../../fonts/cbdos_fonts.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

namespace cbdos {
namespace ui {

static constexpr uint32_t kBg = 0x121417; // Fondo terminal oscuro
static constexpr uint32_t kFg = 0xD8E1F0; // Texto por defecto (blanco suave)

// Cyberpunk ANSI Palette optimizada para legibilidad sobre fondo oscuro
static const uint32_t s_palette[16] = {
    0x101320, 0xFF3B5C, 0x39FF14, 0xF3E600, 0x3D8BFF, 0xFF2A6D, 0x00F0FF, 0xC8D0E0,
    0x5F6884, 0xFF6B85, 0x7DFF5C, 0xFFF35C, 0x7AB0FF, 0xFF6FA0, 0x6BF7FF, 0xFFFFFF,
};

static inline int utf8_put(uint32_t cp, char* o) {
    if (cp < 0x80) {
        o[0] = static_cast<char>(cp);
        return 1;
    }
    if (cp < 0x800) {
        o[0] = static_cast<char>(0xC0 | (cp >> 6));
        o[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        o[0] = static_cast<char>(0xE0 | (cp >> 12));
        o[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        o[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return 3;
    }
    o[0] = static_cast<char>(0xF0 | (cp >> 18));
    o[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    o[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    o[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
}

static inline uint32_t rgbOf(VTermScreen* vs, VTermColor c, bool isFg) {
    if (isFg && VTERM_COLOR_IS_DEFAULT_FG(&c)) return kFg;
    if (!isFg && VTERM_COLOR_IS_DEFAULT_BG(&c)) return kBg;
    vterm_screen_convert_color_to_rgb(vs, &c);
    return ((uint32_t)c.rgb.red << 16) | ((uint32_t)c.rgb.green << 8) | c.rgb.blue;
}

static inline void fillRect(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t rgb, lv_opa_t opa) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(rgb);
    d.bg_opa = opa;
    lv_area_t a = {x1, y1, x2, y2};
    lv_draw_rect(layer, &d, &a);
}

TerminalDisplay::TerminalDisplay() = default;

TerminalDisplay::~TerminalDisplay() {
    destroy();
}

void TerminalDisplay::destroy() {
    if (m_blinkTimer) {
        lv_timer_delete(m_blinkTimer);
        m_blinkTimer = nullptr;
    }
    if (m_vt) {
        vterm_free(m_vt);
        m_vt = nullptr;
        m_vs = nullptr;
    }
    if (m_sb) {
        free(m_sb);
        m_sb = nullptr;
    }
    m_container = nullptr;
}

bool TerminalDisplay::create(lv_obj_t* parent) {
    if (!parent) return false;

    m_container = lv_obj_create(parent);
    if (!m_container) return false;

    lv_obj_remove_style_all(m_container);
    lv_obj_set_size(m_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(m_container, 1);
    lv_obj_remove_flag(m_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_container, LV_OBJ_FLAG_CLICKABLE);

    m_font = &cbdos_font_mono_17;
    lv_font_glyph_dsc_t g;
    m_cw = lv_font_get_glyph_dsc(m_font, &g, 'M', 0) ? g.adv_w : 10;
    m_ch = lv_font_get_line_height(m_font) + 2;
    if (m_cw <= 0) m_cw = 10;
    if (m_ch <= 0) m_ch = 18;

    lv_obj_add_event_cb(m_container, drawEventCb, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(m_container, sizeEventCb, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_add_event_cb(m_container, pressEventCb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(m_container, pressEventCb, LV_EVENT_PRESSING, this);

    m_rows = 24;
    m_cols = 80;
    m_vt = vterm_new(m_rows, m_cols);
    if (!m_vt) return false;

    vterm_set_utf8(m_vt, 1);
    m_vs = vterm_obtain_screen(m_vt);
    vterm_screen_set_callbacks(m_vs, &s_screenCallbacks, this);
    vterm_screen_set_damage_merge(m_vs, VTERM_DAMAGE_SCROLL);
    vterm_screen_enable_altscreen(m_vs, 1);

    VTermState* st = vterm_obtain_state(m_vt);
    VTermColor fg, bg;
    vterm_color_rgb(&fg, (kFg >> 16) & 0xFF, (kFg >> 8) & 0xFF, kFg & 0xFF);
    vterm_color_rgb(&bg, (kBg >> 16) & 0xFF, (kBg >> 8) & 0xFF, kBg & 0xFF);
    vterm_state_set_default_colors(st, &fg, &bg);
    for (int i = 0; i < 16; i++) {
        VTermColor c;
        vterm_color_rgb(&c, (s_palette[i] >> 16) & 0xFF, (s_palette[i] >> 8) & 0xFF, s_palette[i] & 0xFF);
        vterm_state_set_palette_color(st, i, &c);
    }
    vterm_screen_reset(m_vs, 1);

    // Scrollback de 2,000 líneas en PSRAM (o fallback escalonado)
    m_sbCap = 2000;
    m_sbW = 120;
#if defined(ESP_PLATFORM)
    m_sb = reinterpret_cast<SbCell*>(heap_caps_calloc(static_cast<size_t>(m_sbCap) * m_sbW, sizeof(SbCell), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!m_sb) {
        m_sb = reinterpret_cast<SbCell*>(calloc(static_cast<size_t>(m_sbCap) * m_sbW, sizeof(SbCell)));
    }
#else
    m_sb = reinterpret_cast<SbCell*>(calloc(static_cast<size_t>(m_sbCap) * m_sbW, sizeof(SbCell)));
#endif
    if (!m_sb && m_sbCap > 500) {
        m_sbCap = 500;
        m_sb = reinterpret_cast<SbCell*>(calloc(static_cast<size_t>(m_sbCap) * m_sbW, sizeof(SbCell)));
    }

    lv_obj_update_layout(m_container);
    relayout();

    m_blinkTimer = lv_timer_create(blinkTickCb, 530, this);
    return true;
}

void TerminalDisplay::relayout() {
    if (!m_container || !m_vt) return;
    int32_t w = lv_obj_get_width(m_container);
    int32_t h = lv_obj_get_height(m_container);
    if (w <= 0 || h <= 0) return;

    static constexpr int kPad = 4;
    int cols = (w - 2 * kPad) / m_cw;
    int rows = (h - 2 * kPad) / m_ch;
    if (cols < 20) cols = 20;
    if (rows < 5) rows = 5;
    if (cols > 120) cols = 120;
    if (rows > 60) rows = 60;

    m_x0 = (w - cols * m_cw) / 2;
    m_y0 = (h - rows * m_ch) / 2;
    if (m_x0 < 0) m_x0 = 0;
    if (m_y0 < 0) m_y0 = 0;

    if (cols == m_cols && rows == m_rows) return;
    m_cols = cols;
    m_rows = rows;
    vterm_set_size(m_vt, rows, cols);
    vterm_screen_flush_damage(m_vs);
    lv_obj_invalidate(m_container);
}

void TerminalDisplay::appendText(const char* text, size_t len) {
    if (!m_vt || !text || len == 0 || m_hold) return;

    if (m_altScreen) {
        vterm_input_write(m_vt, text, len);
    } else {
        size_t start = 0;
        for (size_t i = 0; i < len; i++) {
            if (text[i] == '\n') {
                const char prev = i ? text[i - 1] : m_lastChar;
                vterm_input_write(m_vt, text + start, i - start);
                start = i;
                if (prev != '\r') {
                    vterm_input_write(m_vt, "\r", 1);
                }
            }
        }
        vterm_input_write(m_vt, text + start, len - start);
    }
    if (len > 0) m_lastChar = text[len - 1];
    vterm_screen_flush_damage(m_vs);
}

void TerminalDisplay::clear() {
    if (!m_vt) return;
    const char clearSeq[] = "\x1b[2J\x1b[H";
    vterm_input_write(m_vt, clearSeq, sizeof(clearSeq) - 1);
    vterm_screen_flush_damage(m_vs);
    m_sbCount = 0;
    m_sbHead = 0;
    m_view = 0;
    if (m_container) lv_obj_invalidate(m_container);
}

bool TerminalDisplay::toggleHold() {
    m_hold = !m_hold;
    return m_hold;
}

void TerminalDisplay::scroll(int lines) {
    int v = m_view + lines;
    if (v < 0) v = 0;
    if (v > m_sbCount) v = m_sbCount;
    if (v == m_view) return;
    m_view = v;
    if (m_container) lv_obj_invalidate(m_container);
}

void TerminalDisplay::scrollLive() {
    if (m_view == 0) return;
    m_view = 0;
    if (m_container) lv_obj_invalidate(m_container);
}

void TerminalDisplay::refreshDisplay() {
    if (m_container) lv_obj_invalidate(m_container);
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

    char lineBuf[512];
    // 1. Scrollback desde la linea más antigua hacia la más reciente
    for (int i = m_sbCount - 1; i >= 0; i--) {
        int len = 0;
        for (int c = 0; c < m_cols && c < m_sbW; c++) {
            RenderCell rc;
            getSbCell(i, c, &rc);
            uint32_t cp = rc.ch ? rc.ch : ' ';
            len += utf8_put(cp, lineBuf + len);
        }
        while (len > 0 && lineBuf[len - 1] == ' ') len--;
        lineBuf[len++] = '\n';
        fwrite(lineBuf, 1, len, f);
    }

    // 2. Pantalla activa (live screen)
    for (int r = 0; r < m_rows; r++) {
        int len = 0;
        for (int c = 0; c < m_cols; c++) {
            RenderCell rc;
            getScreenCell(r, c, &rc);
            uint32_t cp = rc.ch ? rc.ch : ' ';
            len += utf8_put(cp, lineBuf + len);
        }
        while (len > 0 && lineBuf[len - 1] == ' ') len--;
        lineBuf[len++] = '\n';
        fwrite(lineBuf, 1, len, f);
    }

    fclose(f);
    outMessage = "[SYS] Terminal guardada en terminal.log (Scrollback + Pantalla)\n";
    return true;
}

void TerminalDisplay::invalidateCells(int r0, int c0, int r1, int c1) {
    if (!m_container || m_cw <= 0 || m_ch <= 0) return;
    lv_area_t o;
    lv_obj_get_coords(m_container, &o);
    lv_area_t a = {
        static_cast<int32_t>(o.x1 + m_x0 + c0 * m_cw),
        static_cast<int32_t>(o.y1 + m_y0 + r0 * m_ch),
        static_cast<int32_t>(o.x1 + m_x0 + c1 * m_cw + m_cw),
        static_cast<int32_t>(o.y1 + m_y0 + r1 * m_ch - 1)
    };
    lv_obj_invalidate_area(m_container, &a);
}

void TerminalDisplay::getScreenCell(int row, int col, RenderCell* o) {
    VTermScreenCell c;
    VTermPos p = {row, col};
    if (!m_vs || !vterm_screen_get_cell(m_vs, p, &c)) {
        memset(o, 0, sizeof(*o));
        o->fg = kFg;
        o->bg = kBg;
        o->default_bg = true;
        o->width = 1;
        return;
    }
    o->ch = c.chars[0];
    o->width = c.width > 0 ? c.width : 1;
    o->bold = c.attrs.bold;
    o->underline = (c.attrs.underline != 0);
    o->fg = rgbOf(m_vs, c.fg, true);
    o->bg = rgbOf(m_vs, c.bg, false);
    o->default_bg = VTERM_COLOR_IS_DEFAULT_BG(&c.bg);
    if (c.attrs.reverse) {
        std::swap(o->fg, o->bg);
        o->default_bg = false;
    }
}

void TerminalDisplay::getSbCell(int lineFromNewest, int col, RenderCell* o) {
    memset(o, 0, sizeof(*o));
    o->width = 1;
    o->fg = kFg;
    o->bg = kBg;
    o->default_bg = true;
    if (!m_sb || lineFromNewest < 0 || lineFromNewest >= m_sbCount || col >= m_sbW) return;
    int slot = (m_sbHead - 1 - lineFromNewest + m_sbCap) % m_sbCap;
    const SbCell* s = &m_sb[static_cast<size_t>(slot) * m_sbW + col];
    o->ch = s->ch;
    o->fg = (static_cast<uint32_t>(s->fg[0]) << 16) | (static_cast<uint32_t>(s->fg[1]) << 8) | s->fg[2];
    o->bg = (static_cast<uint32_t>(s->bg[0]) << 16) | (static_cast<uint32_t>(s->bg[1]) << 8) | s->bg[2];
    o->bold = (s->attr & 1) != 0;
    o->underline = (s->attr & 2) != 0;
    o->default_bg = (s->attr & 8) != 0;
}

void TerminalDisplay::getViewCell(int row, int col, RenderCell* o) {
    if (row < m_view) {
        getSbCell(m_view - 1 - row, col, o);
    } else {
        getScreenCell(row - m_view, col, o);
    }
}

void TerminalDisplay::drawRun(lv_layer_t* layer, int row, int c0, int c1, const RenderCell* style, const char* txt) {
    lv_area_t o;
    lv_obj_get_coords(m_container, &o);
    int32_t x = o.x1 + m_x0 + c0 * m_cw;
    int32_t y = o.y1 + m_y0 + row * m_ch;
    if (!style->default_bg) {
        fillRect(layer, x, y, x + (c1 - c0) * m_cw - 1, y + m_ch - 1, style->bg, LV_OPA_COVER);
    }
    if (txt[0]) {
        lv_draw_label_dsc_t d;
        lv_draw_label_dsc_init(&d);
        d.font = m_font;
        d.color = lv_color_hex(style->fg);
        d.text = txt;
        d.text_local = 1;
        if (style->underline) d.decor = LV_TEXT_DECOR_UNDERLINE;
        lv_area_t a = {x, y, x + (c1 - c0) * m_cw + m_cw, y + m_ch - 1};
        lv_draw_label(layer, &d, &a);
    }
}

void TerminalDisplay::onDraw(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer || !m_container || m_cw <= 0 || m_ch <= 0) return;

    lv_area_t o;
    lv_obj_get_coords(m_container, &o);
    const lv_area_t* clip = &layer->_clip_area;

    fillRect(layer, std::max(o.x1, clip->x1), std::max(o.y1, clip->y1),
             std::min(o.x2, clip->x2), std::min(o.y2, clip->y2), kBg, LV_OPA_COVER);

    int r0 = (clip->y1 - o.y1 - m_y0) / m_ch;
    int r1 = (clip->y2 - o.y1 - m_y0) / m_ch;
    if (r0 < 0) r0 = 0;
    if (r1 >= m_rows) r1 = m_rows - 1;

    char txt[1024];
    for (int r = r0; r <= r1; r++) {
        RenderCell run, c;
        int start = 0;
        size_t n = 0;
        getViewCell(r, 0, &run);
        for (int col = 0; col <= m_cols; col++) {
            bool end = (col == m_cols);
            if (!end) getViewCell(r, col, &c);
            bool same = !end && c.fg == run.fg && c.bg == run.bg && c.underline == run.underline &&
                        c.default_bg == run.default_bg;
            if (!same) {
                txt[n] = '\0';
                drawRun(layer, r, start, col, &run, txt);
                if (end) break;
                run = c;
                start = col;
                n = 0;
            }
            uint32_t cp = c.ch ? c.ch : ' ';
            if (cp > 0x20) {
                lv_font_glyph_dsc_t gdsc;
                if (!lv_font_get_glyph_dsc(m_font, &gdsc, cp, 0)) {
                    cp = '?';
                }
            }
            if (n + 4 < sizeof(txt)) {
                n += static_cast<size_t>(utf8_put(cp, txt + n));
            }
            if (c.width == 2 && col + 1 < m_cols) {
                col++;
                if (n + 1 < sizeof(txt)) txt[n++] = ' ';
            }
        }
    }

    // Cursor nativo alfa-blended (60% translúcido)
    if (m_view == 0 && m_cursorVisible && m_blinkOn && m_cursorPos.row >= r0 && m_cursorPos.row <= r1) {
        int32_t x = o.x1 + m_x0 + m_cursorPos.col * m_cw;
        int32_t y = o.y1 + m_y0 + m_cursorPos.row * m_ch;
        fillRect(layer, x, y, x + m_cw - 1, y + m_ch - 1, 0x00E5FF, LV_OPA_60);
    }

    // Indicador de scrollback en el borde inferior
    if (m_view > 0) {
        fillRect(layer, o.x1, o.y2 - 3, o.x2, o.y2, 0xFF3B5C, LV_OPA_COVER);
    }
}

void TerminalDisplay::onPress(lv_event_t* e) {
    static int32_t acc = 0;
    lv_indev_t* indev = lv_indev_active();
    if (!indev || m_ch <= 0) return;

    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        acc = 0;
        return;
    }

    lv_point_t v;
    lv_indev_get_vect(indev, &v);
    acc += v.y;
    int lines = acc / m_ch;
    if (lines != 0) {
        acc -= lines * m_ch;
        scroll(lines);
    }
}

// ------------------- Callbacks LVGL -------------------

void TerminalDisplay::drawEventCb(lv_event_t* e) {
    auto* self = static_cast<TerminalDisplay*>(lv_event_get_user_data(e));
    if (self) self->onDraw(e);
}

void TerminalDisplay::sizeEventCb(lv_event_t* e) {
    auto* self = static_cast<TerminalDisplay*>(lv_event_get_user_data(e));
    if (self) self->relayout();
}

void TerminalDisplay::pressEventCb(lv_event_t* e) {
    auto* self = static_cast<TerminalDisplay*>(lv_event_get_user_data(e));
    if (self) self->onPress(e);
}

void TerminalDisplay::blinkTickCb(lv_timer_t* tm) {
    auto* self = static_cast<TerminalDisplay*>(lv_timer_get_user_data(tm));
    if (!self || !self->m_container) return;
    self->m_blinkOn = self->m_cursorBlink ? !self->m_blinkOn : true;
    self->invalidateCells(self->m_cursorPos.row, self->m_cursorPos.col,
                          self->m_cursorPos.row + 1, self->m_cursorPos.col + 1);
}

// ------------------- Callbacks VTerm -------------------

const VTermScreenCallbacks TerminalDisplay::s_screenCallbacks = {
    cbDamage,
    cbMoverect,
    cbMoveCursor,
    cbSetTermProp,
    cbBell,
    nullptr, // resize
    cbSbPushline,
    cbSbPopline,
    cbSbClear,
};

int TerminalDisplay::cbDamage(VTermRect r, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (self && self->m_view == 0) {
        self->invalidateCells(r.start_row, r.start_col, r.end_row, r.end_col);
    }
    return 1;
}

int TerminalDisplay::cbMoverect(VTermRect dest, VTermRect src, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (self && self->m_view == 0) {
        self->invalidateCells(
            std::min(dest.start_row, src.start_row),
            std::min(dest.start_col, src.start_col),
            std::max(dest.end_row, src.end_row),
            std::max(dest.end_col, src.end_col)
        );
    }
    return 1;
}

int TerminalDisplay::cbMoveCursor(VTermPos pos, VTermPos old, int visible, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (!self) return 1;
    (void)visible;
    self->invalidateCells(old.row, old.col, old.row + 1, old.col + 1);
    self->m_cursorPos = pos;
    self->m_blinkOn = true;
    self->invalidateCells(pos.row, pos.col, pos.row + 1, pos.col + 1);
    return 1;
}

int TerminalDisplay::cbSetTermProp(VTermProp prop, VTermValue* val, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (!self) return 1;
    switch (prop) {
        case VTERM_PROP_CURSORVISIBLE:
            self->m_cursorVisible = val->boolean;
            self->invalidateCells(self->m_cursorPos.row, self->m_cursorPos.col,
                                  self->m_cursorPos.row + 1, self->m_cursorPos.col + 1);
            break;
        case VTERM_PROP_CURSORBLINK:
            self->m_cursorBlink = val->boolean;
            break;
        case VTERM_PROP_ALTSCREEN:
            self->m_altScreen = val->boolean;
            if (self->m_container) lv_obj_invalidate(self->m_container);
            break;
        default: break;
    }
    return 1;
}

int TerminalDisplay::cbBell(void*) {
    return 1;
}

int TerminalDisplay::cbSbPushline(int cols, const VTermScreenCell* cells, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (!self || !self->m_sb) return 0;

    SbCell* line = &self->m_sb[static_cast<size_t>(self->m_sbHead) * self->m_sbW];
    for (int i = 0; i < self->m_sbW; i++) {
        SbCell* s = &line[i];
        memset(s, 0, sizeof(*s));
        if (i >= cols) {
            s->attr = 8;
            s->fg[0] = static_cast<uint8_t>((kFg >> 16) & 0xFF);
            s->fg[1] = static_cast<uint8_t>((kFg >> 8) & 0xFF);
            s->fg[2] = static_cast<uint8_t>(kFg & 0xFF);
            continue;
        }
        const VTermScreenCell& c = cells[i];
        uint32_t fg = rgbOf(self->m_vs, c.fg, true);
        uint32_t bg = rgbOf(self->m_vs, c.bg, false);
        bool dbg = VTERM_COLOR_IS_DEFAULT_BG(&c.bg);
        if (c.attrs.reverse) {
            uint32_t x = fg;
            fg = bg;
            bg = x;
            dbg = false;
        }
        s->ch = c.chars[0];
        s->fg[0] = static_cast<uint8_t>((fg >> 16) & 0xFF);
        s->fg[1] = static_cast<uint8_t>((fg >> 8) & 0xFF);
        s->fg[2] = static_cast<uint8_t>(fg & 0xFF);
        s->bg[0] = static_cast<uint8_t>((bg >> 16) & 0xFF);
        s->bg[1] = static_cast<uint8_t>((bg >> 8) & 0xFF);
        s->bg[2] = static_cast<uint8_t>(bg & 0xFF);
        s->attr = static_cast<uint8_t>((c.attrs.bold ? 1 : 0) | (c.attrs.underline ? 2 : 0) | (dbg ? 8 : 0));
    }

    self->m_sbHead = (self->m_sbHead + 1) % self->m_sbCap;
    if (self->m_sbCount < self->m_sbCap) self->m_sbCount++;
    if (self->m_view > 0 && self->m_view < self->m_sbCount) self->m_view++;
    return 1;
}

int TerminalDisplay::cbSbPopline(int cols, VTermScreenCell* cells, void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (!self || !self->m_sb || self->m_sbCount == 0) return 0;

    self->m_sbHead = (self->m_sbHead - 1 + self->m_sbCap) % self->m_sbCap;
    self->m_sbCount--;
    const SbCell* line = &self->m_sb[static_cast<size_t>(self->m_sbHead) * self->m_sbW];
    for (int i = 0; i < cols; i++) {
        memset(&cells[i], 0, sizeof(cells[i]));
        cells[i].width = 1;
        if (i < self->m_sbW) {
            cells[i].chars[0] = line[i].ch;
            vterm_color_rgb(&cells[i].fg, line[i].fg[0], line[i].fg[1], line[i].fg[2]);
            vterm_color_rgb(&cells[i].bg, line[i].bg[0], line[i].bg[1], line[i].bg[2]);
        }
    }
    return 1;
}

int TerminalDisplay::cbSbClear(void* user) {
    auto* self = static_cast<TerminalDisplay*>(user);
    if (self) {
        self->m_sbCount = 0;
        self->m_sbHead = 0;
        self->m_view = 0;
    }
    return 1;
}

} // namespace ui
} // namespace cbdos
