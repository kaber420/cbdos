#include "LVGL_Port.h"
#include "DisplayHAL.h"
#include "TouchHAL.h"
#include "hal_usb_hid_host_p4.hpp"
#include "cbdos/ui.hpp"
#include "../../core/src/lua/LuaBridge.hpp"
#include "misc/cache/instance/lv_image_cache.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_cache.h>
#include <esp_timer.h>

static const char *TAG = "LVGL_Port";

static uint32_t lvgl_tick_cb(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

LVGL_Port::LVGL_Port() {}
LVGL_Port::~LVGL_Port() {
    running = false;
    if (ppaClient) {
        ppa_unregister_client(ppaClient);
        ppaClient = nullptr;
    }
    if (lvglMux) {
        vSemaphoreDelete(lvglMux);
        lvglMux = nullptr;
    }
    if (lvRenderBuf) {
        free(lvRenderBuf);
        lvRenderBuf = nullptr;
    }
}

bool LVGL_Port::lock(uint32_t timeout_ms) {
    if (!lvglMux) return true;
    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvglMux, ticks) == pdTRUE;
}

void LVGL_Port::unlock() {
    if (lvglMux) {
        xSemaphoreGive(lvglMux);
    }
}

void LVGL_Port::setOrientation(cbdos::display::Orientation orientation) {
    if (!display) return;
    currentOrientation = orientation;
    const bool landscape = isLandscape();
    lv_display_set_resolution(display, landscape ? height : width, landscape ? width : height);
    lv_obj_t* scr = lv_screen_active();
    if (scr && lv_obj_is_valid(scr)) {
        lv_obj_invalidate(scr);
    }
}

void LVGL_Port::flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    if (!LuaBridge::isUIPaused()) {
        esp_lcd_panel_handle_t panel_handle = DisplayHAL::getInstance().getPanelHandle();
        if (panel_handle) {
            int w = DisplayHAL::getInstance().getWidth();
            int h = DisplayHAL::getInstance().getHeight();
            size_t fb_bytes = (size_t)w * h * sizeof(uint16_t);

            LVGL_Port& self = LVGL_Port::getInstance();
            self.activeDpiFb = (self.activeDpiFb == 0) ? 1 : 0;
            uint8_t* target_fb = (uint8_t*)DisplayHAL::getInstance().getFrameBuffer(self.activeDpiFb);

            if (!target_fb) {
                target_fb = px_map;
            } else {
                const auto ori = self.currentOrientation;
                if (ori == cbdos::display::Orientation::Portrait) {
                    memcpy(target_fb, px_map, fb_bytes);
                } else {
                    const bool is90or270 = (ori == cbdos::display::Orientation::Landscape ||
                                            ori == cbdos::display::Orientation::LandscapeInverted);
                    const uint32_t src_w = is90or270 ? (uint32_t)h : (uint32_t)w;
                    const uint32_t src_h = is90or270 ? (uint32_t)w : (uint32_t)h;
                    bool rotatedByHw = false;

                    if (self.rotationMode == RotationMode::HardwarePPA && self.ppaClient != nullptr) {
                        ppa_srm_rotation_angle_t ppa_rot = PPA_SRM_ROTATION_ANGLE_0;
                        if (ori == cbdos::display::Orientation::Landscape) {
                            ppa_rot = PPA_SRM_ROTATION_ANGLE_270;
                        } else if (ori == cbdos::display::Orientation::PortraitInverted) {
                            ppa_rot = PPA_SRM_ROTATION_ANGLE_180;
                        } else if (ori == cbdos::display::Orientation::LandscapeInverted) {
                            ppa_rot = PPA_SRM_ROTATION_ANGLE_90;
                        }

                        esp_cache_msync(px_map, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

                        ppa_srm_oper_config_t srm_cfg = {};
                        srm_cfg.in.buffer = px_map;
                        srm_cfg.in.pic_w = src_w;
                        srm_cfg.in.pic_h = src_h;
                        srm_cfg.in.block_w = src_w;
                        srm_cfg.in.block_h = src_h;
                        srm_cfg.in.block_offset_x = 0;
                        srm_cfg.in.block_offset_y = 0;
                        srm_cfg.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

                        srm_cfg.out.buffer = target_fb;
                        srm_cfg.out.buffer_size = fb_bytes;
                        srm_cfg.out.pic_w = w;
                        srm_cfg.out.pic_h = h;
                        srm_cfg.out.block_offset_x = 0;
                        srm_cfg.out.block_offset_y = 0;
                        srm_cfg.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

                        srm_cfg.rotation_angle = ppa_rot;
                        srm_cfg.scale_x = 1.0f;
                        srm_cfg.scale_y = 1.0f;
                        srm_cfg.mode = PPA_TRANS_MODE_BLOCKING;

                        if (ppa_do_scale_rotate_mirror(self.ppaClient, &srm_cfg) == ESP_OK) {
                            rotatedByHw = true;
                        }
                    }

                    if (!rotatedByHw) {
                        lv_display_rotation_t lv_rot = LV_DISPLAY_ROTATION_0;
                        if (ori == cbdos::display::Orientation::Landscape) {
                            lv_rot = LV_DISPLAY_ROTATION_270;
                        } else if (ori == cbdos::display::Orientation::PortraitInverted) {
                            lv_rot = LV_DISPLAY_ROTATION_180;
                        } else if (ori == cbdos::display::Orientation::LandscapeInverted) {
                            lv_rot = LV_DISPLAY_ROTATION_90;
                        }
                        lv_draw_rotate(px_map, target_fb, src_w, src_h,
                                       src_w * sizeof(uint16_t), w * sizeof(uint16_t),
                                       lv_rot, LV_COLOR_FORMAT_RGB565);
                    }
                }
            }

            // Asegurar coherencia de memoria caché antes de que el motor DPI transmita
            esp_cache_msync(target_fb, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

            // Notificar al controlador DPI para intercambiar y escanear este fotograma
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, w, h, target_fb);
        }
    }
    lv_display_flush_ready(disp);
}

void LVGL_Port::touchReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
    if (LuaBridge::isUIPaused()) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint16_t x = 0;
    uint16_t y = 0;
    bool pressed = false;

    if (TouchHAL::getInstance().read(&x, &y, &pressed)) {
        if (pressed) {
            const auto ori = LVGL_Port::getInstance().getOrientation();
            if (ori == cbdos::display::Orientation::Landscape) {
                data->point.x = y;
                data->point.y = 479 - x;
            } else if (ori == cbdos::display::Orientation::LandscapeInverted) {
                data->point.x = 799 - y;
                data->point.y = x;
            } else if (ori == cbdos::display::Orientation::PortraitInverted) {
                data->point.x = 479 - x;
                data->point.y = 799 - y;
            } else {
                data->point.x = x;
                data->point.y = y;
            }
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void LVGL_Port::keyboardReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
    QueueHandle_t q = cbdos::bsp::getUsbKeyboardQueue();
    static cbdos::bsp::KeyboardEvent lastEvent = {0, false};
    cbdos::bsp::KeyboardEvent ev;

    if (q && xQueueReceive(q, &ev, 0) == pdTRUE) {
        lastEvent = ev;
        data->key = ev.key;
        data->state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    } else {
        data->key = lastEvent.key;
        data->state = lastEvent.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    }
}

void LVGL_Port::lvglTask(void* arg) {
    LVGL_Port* self = static_cast<LVGL_Port*>(arg);
    ESP_LOGI(TAG, "Tarea LVGL 9 iniciada en Core %d", xPortGetCoreID());

    while (self->running) {
        if (LuaBridge::isUIPaused()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (self->lock(10)) {
            lv_timer_handler();
            cbdos::ui::update();
            self->unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    vTaskDelete(NULL);
}

esp_err_t LVGL_Port::init(int h_res, int v_res) {
    width = h_res;
    height = v_res;

    ESP_LOGI(TAG, "=== Inicializando LVGL 9.5 Port para ESP32-P4 ===");

    // 1. Crear Mutex de sincronización
    lvglMux = xSemaphoreCreateRecursiveMutex();
    if (!lvglMux) {
        ESP_LOGE(TAG, "No se pudo crear el mutex de LVGL");
        return ESP_ERR_NO_MEM;
    }

    // 2. Inicializar el Core de LVGL 9, Caché de Imágenes en PSRAM y Tick Timer
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);
    lv_image_cache_resize(2 * 1024 * 1024, true);
    ESP_LOGI(TAG, "Caché de imágenes LVGL 9 configurado a 2 MB (activado: %s)",
             lv_image_cache_is_enabled() ? "SÍ" : "NO");

    // 3. Asignar búfer de renderizado dedicado de LVGL (completamente desacoplado del DMA de hardware)
    size_t buffer_size_bytes = width * height * sizeof(lv_color16_t);
    lvRenderBuf = heap_caps_aligned_alloc(64, buffer_size_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lvRenderBuf) {
        ESP_LOGE(TAG, "Error: No hay suficiente PSRAM para lvRenderBuf");
        return ESP_ERR_NO_MEM;
    }
    memset(lvRenderBuf, 0, buffer_size_bytes);
    activeDpiFb = 0;

    ESP_LOGI(TAG, "Búfer de renderizado LVGL dedicado asignado en PSRAM: %p (%u KB)",
             lvRenderBuf, (unsigned)(buffer_size_bytes / 1024));

    // 3.1. Registrar cliente PPA SRM para rotación por hardware (con fallback a software)
    ppa_client_config_t ppa_cfg = {};
    ppa_cfg.oper_type = PPA_OPERATION_SRM;
    ppa_cfg.max_pending_trans_num = 1;
    ppa_cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;
    if (ppa_register_client(&ppa_cfg, &ppaClient) == ESP_OK) {
        ESP_LOGI(TAG, "PPA SRM cliente registrado para rotación por hardware");
    } else {
        ppaClient = nullptr;
        ESP_LOGW(TAG, "PPA SRM no disponible, usando rotación por software");
    }

    // 4. Crear Display en LVGL 9 con renderizado FULL en buffer independiente
    display = lv_display_create(width, height);
    if (!display) {
        ESP_LOGE(TAG, "Error creando display LVGL 9");
        return ESP_FAIL;
    }

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, lvRenderBuf, NULL, buffer_size_bytes, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flushCallback);
    setOrientation(cbdos::display::Orientation::Landscape);

    // 5. Crear e Inicializar Dispositivo de Entrada Táctil
    indev = lv_indev_create();
    if (indev) {
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, touchReadCallback);
        lv_indev_set_display(indev, display);
    }

    // 5.1. Crear e Inicializar Dispositivo de Entrada de Teclado Físico
    kbIndev = lv_indev_create();
    if (kbIndev) {
        lv_indev_set_type(kbIndev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(kbIndev, keyboardReadCallback);
        lv_indev_set_display(kbIndev, display);

        // Crear grupo global por defecto de LVGL para navegación por foco y teclas
        defaultGroup = lv_group_create();
        lv_group_set_default(defaultGroup);
        lv_indev_set_group(kbIndev, defaultGroup);
        ESP_LOGI(TAG, "Dispositivo de entrada KEYPAD y grupo de navegación creados con éxito");
    }

    // 6. Lanzar Tarea FreeRTOS para el loop de LVGL
    running = true;
    BaseType_t res = xTaskCreatePinnedToCore(
        lvglTask,
        "lvgl_task",
        32 * 1024,
        this,
        5,
        &taskHandle,
        1 // Pin to Core 1
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Fallo al crear tarea FreeRTOS de LVGL");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL 9.5 Port inicializado exitosamente!");
    return ESP_OK;
}
