#pragma once
#include <string>
#include <lvgl.h>
#include "cbdos/theme.hpp"

namespace cbdos {
namespace ui {

enum class ViewMode {
    Translucent, // Isla flotante sobre wallpaper animado, contenedor en Y=58 (Dashboard, Widgets)
    Solid,       // Barra acoplada a Y=0 (100% ancho, radio 0, fondo opaco), contenedor en Y=38/44. Pausa el wallpaper animado.
    Immersive    // Pantalla completa pura: Contenedor en Y=0 (100% alto), HeaderBar oculta. Pausa el wallpaper animado.
};

class BaseView {
public:
    explicit BaseView(const std::string& name, ViewMode mode = ViewMode::Translucent)
        : m_name(name), m_container(nullptr), m_visible(false), m_viewMode(mode) {}
    virtual ~BaseView() {
        if (m_container && lv_obj_is_valid(m_container)) {
            lv_obj_delete(m_container);
            m_container = nullptr;
        }
    }

    ViewMode getViewMode() const { return m_viewMode; }
    void setViewMode(ViewMode mode) { m_viewMode = mode; }

    virtual bool onCreate(lv_obj_t* parent) = 0;

    virtual void onDestroy() {
        if (m_container && lv_obj_is_valid(m_container)) {
            lv_obj_delete(m_container);
            m_container = nullptr;
        }
    }

    virtual void onShow() {
        if (m_container && lv_obj_is_valid(m_container)) {
            lv_obj_remove_flag(m_container, LV_OBJ_FLAG_HIDDEN);
        }
        m_visible = true;
    }

    virtual void onHide() {
        if (m_container && lv_obj_is_valid(m_container)) {
            lv_obj_add_flag(m_container, LV_OBJ_FLAG_HIDDEN);
        }
        m_visible = false;
    }

    virtual void onUpdate() {}

    virtual void onThemeChanged(cbdos::theme::ThemeType theme, const cbdos::theme::ThemePalette& palette) {}

    lv_obj_t* getViewRoot() const { return m_container; }
    const std::string& getViewName() const { return m_name; }
    bool isVisible() const { return m_visible; }

protected:
    std::string m_name;
    lv_obj_t* m_container;
    bool m_visible;
    ViewMode m_viewMode;
};

} // namespace ui
} // namespace cbdos
