#include "ThemeEngine.hpp"
#include "cbdos/system.hpp"
#include "cbdos_build_profile.h"

namespace cbdos {

namespace theme {

static const ThemePalette PALETTE_CYBERPUNK = {
    .bg = 0x0A0E17,
    .panel = 0x141C2B,
    .panelBorder = 0x233247,
    .primary = 0x00E5FF,
    .secondary = 0xFF2A6D,
    .accent = 0x00E5FF,
    .textPrimary = 0xFFFFFF,
    .textSecondary = 0x8A99AD,
    .success = 0x00FF88,
    .warning = 0xFFB800,
    .error = 0xFF3366
};

static const ThemePalette PALETTE_MATRIX = {
    .bg = 0x000000,
    .panel = 0x051508,
    .panelBorder = 0x008F11,
    .primary = 0x00FF41,
    .secondary = 0x008F11,
    .accent = 0x00FF41,
    .textPrimary = 0x00FF41,
    .textSecondary = 0x00A82D,
    .success = 0x00FF41,
    .warning = 0xADFF2F,
    .error = 0xFF3333
};

static const ThemePalette PALETTE_SOLAR_AMBER = {
    .bg = 0x100C08,
    .panel = 0x1E1710,
    .panelBorder = 0x3D2E20,
    .primary = 0xFFB000,
    .secondary = 0xD48800,
    .accent = 0xFFB000,
    .textPrimary = 0xFFD280,
    .textSecondary = 0xB38640,
    .success = 0x76E05B,
    .warning = 0xFFB000,
    .error = 0xFF4422
};

ThemeType getCurrentTheme() {
    return ui::ThemeEngine::getInstance().getCurrentTheme();
}

void setTheme(ThemeType theme) {
    ui::ThemeEngine::getInstance().setTheme(theme);
}

const ThemePalette& getPalette() {
    return ui::ThemeEngine::getInstance().getPalette();
}

const ThemePalette& getPalette(ThemeType theme) {
    return ui::ThemeEngine::getInstance().getPalette(theme);
}

const char* getThemeName(ThemeType theme) {
    return ui::ThemeEngine::getInstance().getThemeName(theme);
}

} // namespace theme

namespace ui {

ThemeEngine& ThemeEngine::getInstance() {
    static ThemeEngine instance;
    return instance;
}

ThemeEngine::ThemeEngine()
#if defined(CONFIG_CBDOS_PROFILE_RED)
    : m_currentTheme(cbdos::theme::ThemeType::Matrix) {
#else
    : m_currentTheme(cbdos::theme::ThemeType::Cyberpunk) {
#endif
}

void ThemeEngine::init() {
#if defined(CONFIG_CBDOS_PROFILE_RED)
    m_currentTheme = cbdos::theme::ThemeType::Matrix;
#else
    m_currentTheme = cbdos::theme::ThemeType::Cyberpunk;
#endif
}

void ThemeEngine::setTheme(cbdos::theme::ThemeType theme) {
    if (m_currentTheme == theme) return;
    m_currentTheme = theme;
    
    const auto& palette = getPalette(theme);
    for (auto& cb : m_callbacks) {
        if (cb) {
            cb(m_currentTheme, palette);
        }
    }
}

cbdos::theme::ThemeType ThemeEngine::getCurrentTheme() const {
    return m_currentTheme;
}

const cbdos::theme::ThemePalette& ThemeEngine::getPalette() const {
    return getPalette(m_currentTheme);
}

const cbdos::theme::ThemePalette& ThemeEngine::getPalette(cbdos::theme::ThemeType theme) const {
    switch (theme) {
        case cbdos::theme::ThemeType::Matrix:
            return cbdos::theme::PALETTE_MATRIX;
        case cbdos::theme::ThemeType::SolarAmber:
            return cbdos::theme::PALETTE_SOLAR_AMBER;
        case cbdos::theme::ThemeType::Cyberpunk:
        default:
            return cbdos::theme::PALETTE_CYBERPUNK;
    }
}

const char* ThemeEngine::getThemeName(cbdos::theme::ThemeType theme) const {
    switch (theme) {
        case cbdos::theme::ThemeType::Cyberpunk:
            return "Cyberpunk Neon";
        case cbdos::theme::ThemeType::Matrix:
            return "Matrix Terminal";
        case cbdos::theme::ThemeType::SolarAmber:
            return "Solar Amber";
        default:
            return "Unknown";
    }
}

void ThemeEngine::registerCallback(ThemeChangeCallback callback) {
    m_callbacks.push_back(callback);
}

} // namespace ui
} // namespace cbdos
