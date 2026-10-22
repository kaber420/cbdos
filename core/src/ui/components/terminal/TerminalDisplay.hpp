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

private:
    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_lblText = nullptr;
    lv_timer_t* m_refreshTimer = nullptr;

    bool m_hold = false;
    bool m_cursorVisible = true;
    bool m_altScreen = false;
    bool m_dirty = false;
    char m_lastChar = 0;

    int m_rows = 24;
    int m_cols = 80;

    VTerm*       m_vt = nullptr;
    VTermScreen* m_vs = nullptr;
    VTermPos     m_cursorPos = {0, 0};

    // Callbacks VTerm
    static int cbDamage(VTermRect rect, void* user);
    static int cbMoveCursor(VTermPos pos, VTermPos old, int visible, void* user);
    static int cbSetTermProp(VTermProp prop, VTermValue* val, void* user);
    static int cbBell(void* user);
    static int cbSbPushline(int cols, const VTermScreenCell* cells, void* user);
    static int cbSbPopline(int cols, VTermScreenCell* cells, void* user);
    static int cbSbClear(void* user);

    static const VTermScreenCallbacks s_screenCallbacks;
};

} // namespace ui
} // namespace cbdos
