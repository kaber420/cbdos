#include "cbdos/system.hpp"
#include "cbdos/board_identity.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/network.hpp"
#include "cbdos/radio.hpp"
#include "cbdos/ui.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "cbdos/config_manager.hpp"
#include "cbdos/time.hpp"
#include "cbdos/tts.hpp"
#include "PicoTTSService.hpp"
#include "../../core/src/lua/LuaBridge.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <JC3248W535.h>

extern JC3248W535_Display& get_s3_display_driver();
extern JC3248W535_Touch& get_s3_touch_driver();

namespace cbdos {
namespace bsp {
    void initPersistenceBackend();
    void initStorageBackend();
    void initAudioBackendS3();
    void initUartBackendS3();
    void initGpioBackendS3();
    void initMeshTransportS3();
    void initHttpClientS3();
    void initSocketBackendS3();
    void initSshBackendS3();
    void initHidDriverS3();
    void initNetworkAdapterS3();
    void initRadioBackendS3();
    cbdos::time::ITimeProvider* getArduinoTimeProvider();
}
}

static const char* TAG = "CBDos_Main";
static lv_display_t* s_lv_display = nullptr;
static lv_indev_t* s_lv_indev = nullptr;

static uint32_t lvgl_tick_get_cb(void) {
    return (uint32_t)millis();
}

static void lvgl_display_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    // Si la UI está pausada (ej: script gráfico de Lua corriendo), no sobreescribir el canvas de hardware
    if (!LuaBridge::isUIPaused()) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);

        JC3248W535_Display& drv = get_s3_display_driver();
        if (drv.getCanvas()) {
            drv.getCanvas()->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
            drv.flush();
        }
    }
    lv_display_flush_ready(display);
}

static void lvgl_touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    // Si la UI está pausada por Lua, ceder el control táctil exclusivo al script
    if (LuaBridge::isUIPaused()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    cbdos::input::TouchPoint tp;
    if (cbdos::input::getTouch(tp) && tp.isPressed) {
        data->point.x = tp.x;
        data->point.y = tp.y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup() {
    Serial.begin(115200);
    cbdos::system::sleepMs(500);
    // Identidad v1 para el flasheador web (banner + comando CBDOS:VERSION?).
    Serial.println(cbdos::board_identity::bannerFor(
        *cbdos::board_identity::findBoard("jc3248w535")).c_str());
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "=== Iniciando CyBerDeck OS (CBDos v0.2.3-dev) [Target: ESP32-S3] ===");
    
    // Inyectar el backend de persistencia NVS, Almacenamiento, Audio, UART, GPIO, Radio, Red, Sockets, Transporte de Malla, Cliente HTTP y USB HID
    cbdos::bsp::initPersistenceBackend();
    cbdos::bsp::initStorageBackend();
    cbdos::bsp::initAudioBackendS3();
    cbdos::bsp::initUartBackendS3();
    cbdos::bsp::initGpioBackendS3();
    cbdos::bsp::initNetworkAdapterS3();
    cbdos::bsp::initSocketBackendS3();
    cbdos::bsp::initSshBackendS3();
    cbdos::bsp::initRadioBackendS3();
    cbdos::bsp::initMeshTransportS3();
    cbdos::bsp::initHttpClientS3();
    cbdos::bsp::initHidDriverS3();

    // Registrar servicio de TTS (Offline-First: permanece en reposo hasta su primer uso)
    cbdos::tts::setTTSService(&cbdos::tts::PicoTTSService::getInstance());

    // Conectar time <--> mesh mediante callbacks (sin acoplamiento directo entre módulos)
    cbdos::time::setTowerSyncRequestCallback([]() {
        cbdos::mesh::MeshEngine::getInstance().sendTowerProbe();
    });
    cbdos::mesh::MeshEngine::getInstance().setEpochReceivedCallback([](time_t epoch) {
        cbdos::time::setEpoch(epoch, cbdos::time::TimeSource::Tower);
    });

    // Cargar configuraciones del sistema desde NVS
    SystemConfig sysCfg;
    ConfigManager::getInstance().loadSystem(sysCfg);
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Preferencias NVS: Brillo=%d%%, Vol=%d%%, Auto-WiFi=%s", 
                       sysCfg.brightness, sysCfg.volume, sysCfg.autoConnectWifi ? "SI" : "NO");

    // 1. Inicializar Subsistema de Pantalla a través de la API
    if (!cbdos::display::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando Display en S3");
        return;
    }
    cbdos::display::setBrightness(sysCfg.brightness);
    
    // 2. Inicializar Entrada Táctil
    if (!cbdos::input::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: Touch no detectado o fallo en bus I2C");
    } else {
        get_s3_display_driver().setTouchRotation(&get_s3_touch_driver());
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Touch AXS15231B calibrado y asociado a la rotación");
    }

    // 3. Inicializar Subsistema de Radio según NVS (Offline-First / Determinista)
    cbdos::radio::init();
    TimeConfig timeCfg;
    ConfigManager::getInstance().loadTime(timeCfg);
    cbdos::time::init(cbdos::bsp::getArduinoTimeProvider());
    cbdos::time::setNtpServer(timeCfg.ntpServer.c_str());
    cbdos::time::setTimezone(timeCfg.gmtOffsetSeconds, timeCfg.daylightOffsetSeconds);
    cbdos::time::setNtpEnabled(timeCfg.enabled);

    cbdos::radio::RadioConfig radioCfg;
    ConfigManager::getInstance().loadRadio(radioCfg);

    if (sysCfg.autoConnectWifi && radioCfg.enabled && (radioCfg.mode == cbdos::radio::RadioMode::WifiSta || radioCfg.mode == cbdos::radio::RadioMode::Hybrid)) {
        WiFiConfig wifiCfg;
        if (ConfigManager::getInstance().loadWiFi(wifiCfg) && wifiCfg.ssid.length() > 0) {
            cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Auto-conectando a Wi-Fi: %s", wifiCfg.ssid.c_str());
            if (wifiCfg.useStaticIp) {
                IPAddress ip, gw, sub;
                if (ip.fromString(wifiCfg.staticIp.c_str()) && gw.fromString(wifiCfg.gateway.c_str())) {
                    sub.fromString(wifiCfg.subnet.length() > 0 ? wifiCfg.subnet.c_str() : "255.255.255.0");
                    WiFi.config(ip, gw, sub);
                }
            }
            WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.password.c_str());
        }
    }

    // 4. Inicializar LVGL 9.5 con fuente de Ticks
    lv_init();
    lv_tick_set_cb(lvgl_tick_get_cb);
    
    auto caps = cbdos::display::getCapabilities();
    
    // Asignar memoria para el buffer de pantalla completa en PSRAM externa (307 KB) idéntico a espOS32
    uint32_t buf_size = 320 * 480 * 2;
    uint8_t *buf = (uint8_t *)ps_malloc(buf_size);
    if (!buf) {
        buf_size = 480 * 40 * 2;
        buf = (uint8_t *)malloc(buf_size);
    }

    s_lv_display = lv_display_create(caps.width, caps.height);
    lv_display_set_color_format(s_lv_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_lv_display, buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(s_lv_display, lvgl_display_flush_cb);

    // Configurar Input Táctil y vincularlo explícitamente al display en LVGL 9
    s_lv_indev = lv_indev_create();
    lv_indev_set_type(s_lv_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_lv_indev, lvgl_touch_read_cb);
    lv_indev_set_display(s_lv_indev, s_lv_display);
    lv_timer_t* indev_timer = lv_indev_get_read_timer(s_lv_indev);
    if (indev_timer) {
        lv_timer_set_period(indev_timer, 10);
    }

    // 5. Inicializar Almacenamiento MicroSD SPI (VFS LVGL A:) y Audio I2S
    cbdos::storage::init();
    cbdos::audio::init();
    cbdos::audio::setVolume(sysCfg.volume);

    // 6. Inicializar Sistema de Interfaz Gráfica Universal (Fase 1)
    if (!cbdos::ui::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando UI Core en S3");
        return;
    }

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "CyBerDeck OS v0.2.0 iniciado en ESP32-S3 a %d FPS!", caps.targetFps);
}

extern void cbdos_hid_s3_poll();

void loop() {
    cbdos_hid_s3_poll();

    // Respuesta al flasheador web: "CBDOS:VERSION?" -> banner de identidad.
    while (Serial.available()) {
        static String s3IdLine;
        char c = (char)Serial.read();
        if (c == '\n') {
            std::string line(s3IdLine.c_str());
            s3IdLine = "";
            if (cbdos::board_identity::isVersionQuery(line)) {
                Serial.println(cbdos::board_identity::bannerFor(
                    *cbdos::board_identity::findBoard("jc3248w535")).c_str());
            }
        } else if (c != '\r') {
            s3IdLine += c;
            if (s3IdLine.length() > 64) s3IdLine = "";
        }
    }

    if (LuaBridge::checkAndClearNeedsRefresh()) {
        lv_obj_t* scr = lv_screen_active();
        if (scr && lv_obj_is_valid(scr)) {
            lv_obj_invalidate(scr);
            lv_refr_now(NULL);
        }
    }

    if (LuaBridge::isUIPaused()) {
        cbdos::system::sleepMs(10);
        return;
    }

    uint32_t time_till_next = lv_timer_handler();
    cbdos::ui::update();
    if (time_till_next > 10) time_till_next = 10;
    if (time_till_next > 0) {
        cbdos::system::sleepMs(time_till_next);
    } else {
        cbdos::system::sleepMs(1);
    }
}
