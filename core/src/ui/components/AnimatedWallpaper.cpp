#include "AnimatedWallpaper.hpp"
#include "wallpapers/ConstellationEffect.hpp"
#include "wallpapers/WavesEffect.hpp"
#include "wallpapers/ComicDriveEffect.hpp"
#include "wallpapers/FirefliesEffect.hpp"
#include "wallpapers/TouchSwarmEffect.hpp"
#include "wallpapers/Synthwave80sEffect.hpp"
#include "wallpapers/MatrixRainEffect.hpp"
#include "cbdos/display.hpp"
#include "cbdos/log.hpp"

namespace cbdos {
namespace ui {

static const char* TAG = "AnimatedWallpaper";

static bool getActiveTouchPoint(lv_point_t& outPoint) {
    for (lv_indev_t* indev = lv_indev_get_next(nullptr);
         indev != nullptr;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER &&
            lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            lv_indev_get_point(indev, &outPoint);
            return true;
        }
    }
    return false;
}

AnimatedWallpaper::AnimatedWallpaper()
    : m_canvasObj(nullptr),
      m_timer(nullptr),
      m_running(false),
      m_tick(0),
      m_style(Style::Constellation),
      m_activeEffect(nullptr) {
}

AnimatedWallpaper::~AnimatedWallpaper() {
    destroy();
}

std::unique_ptr<IWallpaperEffect> AnimatedWallpaper::createEffect(Style style) {
    switch (style) {
        case Style::Constellation: return std::unique_ptr<IWallpaperEffect>(new ConstellationEffect());
        case Style::Waves:         return std::unique_ptr<IWallpaperEffect>(new WavesEffect());
        case Style::ComicDrive:    return std::unique_ptr<IWallpaperEffect>(new ComicDriveEffect());
        case Style::Fireflies:     return std::unique_ptr<IWallpaperEffect>(new FirefliesEffect());
        case Style::TouchSwarm:    return std::unique_ptr<IWallpaperEffect>(new TouchSwarmEffect());
        case Style::Synthwave80s:  return std::unique_ptr<IWallpaperEffect>(new Synthwave80sEffect());
        case Style::MatrixRain:    return std::unique_ptr<IWallpaperEffect>(new MatrixRainEffect());
        default:                   return std::unique_ptr<IWallpaperEffect>(new ConstellationEffect());
    }
}

void AnimatedWallpaper::ensureActiveEffect() {
    if (m_activeEffect) return;

    const auto caps = cbdos::display::getCapabilities();
    int w = (m_canvasObj && lv_obj_is_valid(m_canvasObj)) ? lv_obj_get_width(m_canvasObj) : 0;
    int h = (m_canvasObj && lv_obj_is_valid(m_canvasObj)) ? lv_obj_get_height(m_canvasObj) : 0;
    if (w <= 0) w = (caps.width > 0) ? caps.width : 480;
    if (h <= 0) h = (caps.height > 0) ? caps.height : 800;

    m_activeEffect = createEffect(m_style);
    if (m_activeEffect) {
        m_activeEffect->init(w, h);
    }
}

void AnimatedWallpaper::setStyle(Style style) {
    if (m_style == style && m_activeEffect) return;

    m_style = style;
    m_activeEffect.reset();
    m_tick = 0;

    if (m_canvasObj && lv_obj_is_valid(m_canvasObj)) {
        ensureActiveEffect();
        lv_obj_invalidate(m_canvasObj);
    }
}

void AnimatedWallpaper::init(lv_obj_t* parent) {
    if (!parent) return;

    // Si ya existe un canvas válido sobre el mismo padre, solo aseguramos el efecto activo
    if (m_canvasObj && lv_obj_is_valid(m_canvasObj) && lv_obj_get_parent(m_canvasObj) == parent) {
        ensureActiveEffect();
        start();
        lv_obj_invalidate(m_canvasObj);
        return;
    }

    if (m_canvasObj) {
        destroy();
    }

    m_canvasObj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_canvasObj);
    lv_obj_set_size(m_canvasObj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_align(m_canvasObj, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(m_canvasObj, lv_color_hex(0x0a0d1a), 0);
    lv_obj_set_style_bg_opa(m_canvasObj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(m_canvasObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(m_canvasObj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(m_canvasObj, 0);

    lv_obj_add_event_cb(m_canvasObj, drawCallback, LV_EVENT_DRAW_MAIN, this);

    ensureActiveEffect();

    CBD_LOG_I(TAG, "AnimatedWallpaper inicializado exitosamente");
    start();
}

void AnimatedWallpaper::start() {
    if (m_running) return;

    if (!m_timer) {
        m_timer = lv_timer_create(timerCallback, 16, this);
    } else {
        lv_timer_resume(m_timer);
    }
    m_running = true;
}

void AnimatedWallpaper::stop() {
    if (!m_running) return;

    if (m_timer) {
        lv_timer_pause(m_timer);
    }
    m_running = false;
}

void AnimatedWallpaper::destroy() {
    stop();
    if (m_timer) {
        lv_timer_delete(m_timer);
        m_timer = nullptr;
    }
    if (m_canvasObj) {
        if (lv_obj_is_valid(m_canvasObj)) {
            lv_obj_delete(m_canvasObj);
        }
        m_canvasObj = nullptr;
    }
    m_activeEffect.reset();
}

void AnimatedWallpaper::timerCallback(lv_timer_t* timer) {
    auto* self = static_cast<AnimatedWallpaper*>(lv_timer_get_user_data(timer));
    if (!self || !self->m_canvasObj || !lv_obj_is_valid(self->m_canvasObj)) return;

    self->m_tick++;
    self->updatePhysics();
    lv_obj_invalidate(self->m_canvasObj);
}

void AnimatedWallpaper::updatePhysics() {
    if (!m_activeEffect) return;

    int width = lv_obj_get_width(m_canvasObj);
    int height = lv_obj_get_height(m_canvasObj);
    if (width <= 0) width = 480;
    if (height <= 0) height = 800;

    lv_point_t touchPoint = {0, 0};
    bool isTouchPressed = getActiveTouchPoint(touchPoint);
    m_activeEffect->update(width, height, isTouchPressed, touchPoint);
}

void AnimatedWallpaper::drawCallback(lv_event_t* e) {
    auto* self = static_cast<AnimatedWallpaper*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer) return;

    self->draw(layer);
}

void AnimatedWallpaper::draw(lv_layer_t* layer) {
    if (!m_activeEffect) return;

    int width = lv_obj_get_width(m_canvasObj);
    int height = lv_obj_get_height(m_canvasObj);
    if (width <= 0) width = 480;
    if (height <= 0) height = 800;

    m_activeEffect->draw(layer, width, height, m_tick);
}

} // namespace ui
} // namespace cbdos
