#pragma once

#include "lvgl.h"
#include <string>
#include <cstdint>
#include "vterm.h"

namespace cbdos {
namespace ui {

class TerminalDisplay {
public:
    TerminalDisplay();
    ~TerminalDisplay();

    bool create(lv_obj_t* parent);
    void destroy();

    void appendText(const char* text, size_t len);
    void clear();

    bool toggleHold();
    bool isHold() const { return m_hold; }

    bool saveLogToSd(std::string& outMessage);
    lv_obj_t* getObject() const { return m_container; }

    uint16_t getCols() const { return (uint16_t)m_cols; }
    uint16_t getRows() const { return (uint16_t)m_rows; }

    void refreshDisplay();

    void scroll(int lines);
    void scrollLive();
    int getScrollbackCount() const { return m_sbCount; }
    int getView() const { return m_view; }

private:
    struct SbCell {
        uint32_t ch;
        uint8_t fg[3];
        uint8_t bg[3];
        uint8_t attr; // bit0 bold, bit1 underline, bit2 reverse, bit3 default_bg
    };

    struct RenderCell {
        uint32_t ch;
        uint32_t fg; // 0xRRGGBB
        uint32_t bg; // 0xRRGGBB
        bool bold;
        bool underline;
        bool default_bg;
        int width;
    };

    void relayout();
    void invalidateCells(int r0, int c0, int r1, int c1);
    void getScreenCell(int row, int col, RenderCell* out);
    void getSbCell(int lineFromNewest, int col, RenderCell* out);
    void getViewCell(int row, int col, RenderCell* out);
    void drawRun(lv_layer_t* layer, int row, int c0, int c1, const RenderCell* style, const char* txt);
    void onDraw(lv_event_t* e);
    void onPress(lv_event_t* e);

    static void drawEventCb(lv_event_t* e);
    static void sizeEventCb(lv_event_t* e);
    static void pressEventCb(lv_event_t* e);
    static void blinkTickCb(lv_timer_t* tm);

    static int cbDamage(VTermRect rect, void* user);
    static int cbMoverect(VTermRect dest, VTermRect src, void* user);
    static int cbMoveCursor(VTermPos pos, VTermPos old, int visible, void* user);
    static int cbSetTermProp(VTermProp prop, VTermValue* val, void* user);
    static int cbBell(void* user);
    static int cbSbPushline(int cols, const VTermScreenCell* cells, void* user);
    static int cbSbPopline(int cols, VTermScreenCell* cells, void* user);
    static int cbSbClear(void* user);

    static const VTermScreenCallbacks s_screenCallbacks;

    lv_obj_t* m_container = nullptr;
    const lv_font_t* m_font = nullptr;
    lv_timer_t* m_blinkTimer = nullptr;

    int m_cw = 10;
    int m_ch = 18;
    int m_rows = 24;
    int m_cols = 80;
    int m_x0 = 0;
    int m_y0 = 0;

    SbCell* m_sb = nullptr;
    int m_sbCap = 2000;
    int m_sbW = 120;
    int m_sbCount = 0;
    int m_sbHead = 0;
    int m_view = 0;

    VTerm* m_vt = nullptr;
    VTermScreen* m_vs = nullptr;
    VTermPos m_cursorPos = {0, 0};
    bool m_cursorVisible = true;
    bool m_cursorBlink = true;
    bool m_blinkOn = true;
    bool m_hold = false;
    bool m_altScreen = false;
    char m_lastChar = 0;
};

} // namespace ui
} // namespace cbdos
