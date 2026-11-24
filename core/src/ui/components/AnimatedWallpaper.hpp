#ifndef ANIMATED_WALLPAPER_HPP
#define ANIMATED_WALLPAPER_HPP

#include <lvgl.h>
#include <vector>

namespace cbdos {
namespace ui {

struct Particle {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    float alpha;
    float pulseSpeed;
    float phase;
};

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
    void setStyle(Style style) { m_style = style; }
    Style getStyle() const { return m_style; }

private:
    AnimatedWallpaper();
    ~AnimatedWallpaper();

    static void drawCallback(lv_event_t* e);
    static void timerCallback(lv_timer_t* timer);

    void draw(lv_layer_t* layer);
    void updatePhysics();
    void initMatrix();

    lv_obj_t* m_canvasObj;
    lv_timer_t* m_timer;
    bool m_running;
    uint32_t m_tick;
    Style m_style;
    
    static const int PARTICLE_COUNT = 16;
    Particle m_particles[PARTICLE_COUNT];

    static const int MATRIX_COL_MAX = 64;
    static const int MATRIX_TRAIL_MAX = 24;
    struct MatrixColumn {
        float y;
        float speed;
        int length;
        int mutateCounter;
        char chars[MATRIX_TRAIL_MAX];
    };
    MatrixColumn m_matrixCols[MATRIX_COL_MAX];
    bool m_matrixInitialized;
};

} // namespace ui
} // namespace cbdos

#endif // ANIMATED_WALLPAPER_HPP
