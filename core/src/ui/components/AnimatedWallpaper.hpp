#ifndef ANIMATED_WALLPAPER_HPP
#define ANIMATED_WALLPAPER_HPP

#include "wallpapers/IWallpaperEffect.hpp"
#include <lvgl.h>
#include <memory>

namespace cbdos {
namespace ui {

/**
 * @brief Host LVGL para fondos animados vectoriales.
 * Mantiene únicamente el canvas, el timer VSYNC y una instancia del efecto activo.
 */
class AnimatedWallpaper {
public:
    static AnimatedWallpaper& getInstance() {
        static AnimatedWallpaper instance;
        return instance;
    }

    enum class Style {
        Constellation = 0,
        Waves = 1,
        ComicDrive = 2,
        Fireflies = 3,
        TouchSwarm = 4,
        Synthwave80s = 5,
        MatrixRain = 6
    };

    void init(lv_obj_t* parent);
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    void destroy();
    void setStyle(Style style);
    Style getStyle() const { return m_style; }

private:
    AnimatedWallpaper();
    ~AnimatedWallpaper();

    static void drawCallback(lv_event_t* e);
    static void timerCallback(lv_timer_t* timer);

    std::unique_ptr<IWallpaperEffect> createEffect(Style style);
    void ensureActiveEffect();
    void draw(lv_layer_t* layer);
    void updatePhysics();

    lv_obj_t* m_canvasObj;
    lv_timer_t* m_timer;
    bool m_running;
    uint32_t m_tick;
    Style m_style;
    std::unique_ptr<IWallpaperEffect> m_activeEffect;
};

} // namespace ui
} // namespace cbdos

#endif // ANIMATED_WALLPAPER_HPP
