#include "DisplayHAL.h"
#include "ST7701_Init.h"
#include <esp_log.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_ldo_regulator.h>
#include <esp_cache.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "cbdos_device_tree.h"

static const char *TAG = "DisplayHAL";

#define LCD_LEDC_TIMER       LEDC_TIMER_0
#define LCD_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LCD_LEDC_CHANNEL     LEDC_CHANNEL_0
#define LCD_LEDC_DUTY_RES    LEDC_TIMER_10_BIT
#define LCD_LEDC_FREQ        1000

/* 8 barras de color de prueba para verificación visual inmediata en panel 480x800 */
static const uint16_t kTestBarColors[8] = {
    0xF800, // Rojo
    0x07E0, // Verde
    0x001F, // Azul
    0xFFE0, // Amarillo
    0x07FF, // Cyan
    0xF81F, // Magenta
    0xFFFF, // Blanco
    0x0000, // Negro
};

static void drawInitialTestPattern(uint16_t *fb, int w, int h) {
    if (!fb) return;
    int bar_w = w / 8;
    for (int y = 0; y < h; ++y) {
        uint16_t *row = fb + (size_t)y * w;
        for (int x = 0; x < w; ++x) {
            int bar = x / bar_w;
            if (bar > 7) bar = 7;
            row[x] = kTestBarColors[bar];
        }
    }
}

DisplayHAL::DisplayHAL() {}
DisplayHAL::~DisplayHAL() {}

esp_err_t DisplayHAL::initBacklight() {
    ESP_LOGI(TAG, "Inicializando Retroiluminación PWM en GPIO %d (%d Hz)...", cbdos::board::DEVICE_TREE_DISPLAY_PIN_BL, LCD_LEDC_FREQ);

    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = LCD_LEDC_MODE;
    timer_conf.duty_resolution = LCD_LEDC_DUTY_RES;
    timer_conf.timer_num = LCD_LEDC_TIMER;
    timer_conf.freq_hz = LCD_LEDC_FREQ;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error timer LEDC: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t channel_conf = {};
    channel_conf.gpio_num = cbdos::board::DEVICE_TREE_DISPLAY_PIN_BL;
    channel_conf.speed_mode = LCD_LEDC_MODE;
    channel_conf.channel = LCD_LEDC_CHANNEL;
    channel_conf.intr_type = LEDC_INTR_DISABLE;
    channel_conf.timer_sel = LCD_LEDC_TIMER;
    channel_conf.duty = (currentBrightness * 1023) / 100;
    channel_conf.hpoint = 0;
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error canal LEDC: %s", esp_err_to_name(ret));
        return ret;
    }

    setBrightness(currentBrightness);
    return ESP_OK;
}

void DisplayHAL::setBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    currentBrightness = percent;
    // 10-bit duty resolution -> 0 .. 1023
    uint32_t duty = (percent * 1023) / 100;
    ledc_set_duty(LCD_LEDC_MODE, LCD_LEDC_CHANNEL, duty);
    ledc_update_duty(LCD_LEDC_MODE, LCD_LEDC_CHANNEL);
}

void DisplayHAL::turnOn() {
    setBrightness(currentBrightness > 0 ? currentBrightness : 70);
}

void DisplayHAL::turnOff() {
    ledc_set_duty(LCD_LEDC_MODE, LCD_LEDC_CHANNEL, 0);
    ledc_update_duty(LCD_LEDC_MODE, LCD_LEDC_CHANNEL);
}

esp_err_t DisplayHAL::initMipiDsi() {
    ESP_LOGI(TAG, "Inicializando Bus MIPI-DSI (2 Lanes D-PHY)...");

    // 0. Habilitar alimentación LDO para el transceiver MIPI D-PHY (VO3 @ 2.5V)
    static esp_ldo_channel_handle_t ldo_mipi_phy = nullptr;
    esp_ldo_channel_config_t ldo_cfg = {};
    ldo_cfg.chan_id = BOARD_DISP_DSI_LDO_CH;
    ldo_cfg.voltage_mv = 2500;
    esp_err_t ret = esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi_phy);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al habilitar LDO MIPI-DSI (2500mV): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LDO MIPI-DSI (VO3 2.5V) energizado correctamente");
    }

    // Habilitar LDO VO4 (3.3V) para microSD
    static esp_ldo_channel_handle_t ldo_sd = nullptr;
    esp_ldo_channel_config_t sd_ldo_cfg = {};
    sd_ldo_cfg.chan_id = BOARD_DISP_SD_LDO_CH;
    sd_ldo_cfg.voltage_mv = 3300;
    esp_ldo_acquire_channel(&sd_ldo_cfg, &ldo_sd);

    vTaskDelay(pdMS_TO_TICKS(50));

    // 1. Configurar Bus Host MIPI DSI (2 Data Lanes @ 500 Mbps)
    esp_lcd_dsi_bus_config_t bus_config = {};
    bus_config.bus_id = 0;
    bus_config.num_data_lanes = BOARD_DISP_DSI_LANES;
    bus_config.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
    bus_config.lane_bit_rate_mbps = 500;
    ret = esp_lcd_new_dsi_bus(&bus_config, &dsiBusHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al crear bus DSI: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Configurar Canal IO DBI para comandos del controlador ST7701S
    esp_lcd_dbi_io_config_t dbi_config = {};
    dbi_config.virtual_channel = 0;
    dbi_config.lcd_cmd_bits = 8;
    dbi_config.lcd_param_bits = 8;
    ret = esp_lcd_new_panel_io_dbi(dsiBusHandle, &dbi_config, &ioHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al crear panel IO DBI: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Timings DPI nativos para panel 480x800 ST7701S @ 34 MHz
    esp_lcd_dpi_panel_config_t dpi_config = {};
    dpi_config.virtual_channel = 0;
    dpi_config.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi_config.dpi_clock_freq_mhz = 34; // 34 MHz Pixel Clock
    dpi_config.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.out_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.num_fbs = 2; // Doble buffer hardware
    dpi_config.video_timing.h_size = (uint32_t)width;
    dpi_config.video_timing.v_size = (uint32_t)height;
    dpi_config.video_timing.hsync_pulse_width = 12;
    dpi_config.video_timing.hsync_back_porch = 42;
    dpi_config.video_timing.hsync_front_porch = 42;
    dpi_config.video_timing.vsync_pulse_width = 2;
    dpi_config.video_timing.vsync_back_porch = 8;
    dpi_config.video_timing.vsync_front_porch = 166;
    dpi_config.flags.use_dma2d = true;

    ESP_LOGI(TAG, "Creando panel DPI nativo (%dx%d @ 34MHz, 2 fbs)...", width, height);
    ret = esp_lcd_new_panel_dpi(dsiBusHandle, &dpi_config, &panelHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al crear panel DPI: %s", esp_err_to_name(ret));
        return ret;
    }

    // 4. Pulso de Reset por Hardware en GPIO (leído del CDT)
    int rst_pin = cbdos::board::DEVICE_TREE_DISPLAY_PIN_RST;
    ESP_LOGI(TAG, "Ejecutando pulso de Reset en GPIO %d...", rst_pin);
    gpio_config_t rst_conf = {};
    rst_conf.pin_bit_mask = (1ULL << rst_pin);
    rst_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&rst_conf);
    gpio_set_level((gpio_num_t)rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // 5. Transmisión de tabla de inicialización DCS verificada para ST7701S
    ESP_LOGI(TAG, "Enviando secuencia de inicialización DCS (%d comandos)...", (int)JC4880_ST7701_INIT_CMDS_SIZE);
    for (size_t i = 0; i < JC4880_ST7701_INIT_CMDS_SIZE; i++) {
        const st7701_panel_init_cmd_t *cmd = &jc4880_st7701_init_cmds[i];
        ret = esp_lcd_panel_io_tx_param(ioHandle, cmd->cmd, cmd->data, cmd->len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error transmitiendo comando DCS 0x%02X: %s", cmd->cmd, esp_err_to_name(ret));
            return ret;
        }
        if (cmd->delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(cmd->delay_ms));
        }
    }

    // 6. Inicializar panel DPI
    ESP_LOGI(TAG, "Inicializando panel DPI...");
    ret = esp_lcd_panel_init(panelHandle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar panel DPI: %s", esp_err_to_name(ret));
        return ret;
    }

    // 7. Obtener los framebuffers asignados por el controlador DPI y limpiarlos en negro puro
    ret = esp_lcd_dpi_panel_get_frame_buffer(panelHandle, 2, &fb0, &fb1);
    if (ret == ESP_OK) {
        const size_t fb_bytes = (size_t)width * height * sizeof(uint16_t);
        if (fb0) {
            memset(fb0, 0, fb_bytes);
            esp_cache_msync(fb0, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }
        if (fb1) {
            memset(fb1, 0, fb_bytes);
            esp_cache_msync(fb1, fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }
        ESP_LOGI(TAG, "Framebuffers DPI inicializados a negro puro: fb0=%p, fb1=%p", fb0, fb1);
    } else {
        ESP_LOGW(TAG, "Aviso: no se pudieron obtener framebuffers DPI: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Panel ST7701S inicializado con éxito.");
    return ESP_OK;
}

esp_err_t DisplayHAL::init(int h_res, int v_res) {
    if (initialized) return ESP_OK;

    // Ignoramos h_res y v_res hardcodeados y leemos la verdad absoluta del CDT
    width = cbdos::board::DEVICE_TREE_DISPLAY_WIDTH;
    height = cbdos::board::DEVICE_TREE_DISPLAY_HEIGHT;

    ESP_LOGI(TAG, "=== Inicializando DisplayHAL (Driver: %s MIPI-DSI) ===", cbdos::board::DEVICE_TREE_DISPLAY_DRIVER);

    esp_err_t ret = initMipiDsi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error crítico inicializando bus MIPI-DSI");
        return ret;
    }

    ret = initBacklight();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Aviso: no se pudo inicializar backlight PWM");
    }

    initialized = true;
    ESP_LOGI(TAG, "DisplayHAL inicializado con éxito a %dx%d @ 60 FPS", width, height);
    return ESP_OK;
}
