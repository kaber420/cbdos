#include "cbdos/system.hpp"
#include "cbdos/board_identity.hpp"
#include "cbdos/display.hpp"
#include "cbdos/input.hpp"
#include "cbdos/storage.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/network.hpp"
#include "cbdos/radio.hpp"
#include "cbdos/flasher.hpp"
#include "cbdos/uart.hpp"
#include "cbdos/ui.hpp"
#include "cbdos/usb_manager.hpp"
#include "cbdos/mesh/mesh_engine.hpp"
#include "LVGL_Port.h"
#include "cbdos/config_manager.hpp"
#include "cbdos/time.hpp"
#include "cbdos/tts.hpp"
#include "PicoTTSService.hpp"
#include <esp_log.h>
#include <nvs_flash.h>
#include "usb_device_manager.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace cbdos {
namespace bsp {
    void initPersistenceBackend();
    void initStorageBackend();
    void initAudioBackendP4();
    void initUartBackendP4();
    void initGpioBackendP4();
    void initMeshTransportP4();
    void initHttpClientP4();
    void initSocketBackendP4();
    void initSshBackendP4();
    void initHidDriverP4();
    void initNetworkAdapterP4();
    void initRadioBackendP4();
    cbdos::time::ITimeProvider* getEspIdfTimeProvider();
}
}

static const char* TAG = "CBDos_Main";

extern "C" void app_main(void) {

    // Identidad v1 para el flasheador web: banner parseable por WebSerial.
    // Formato estable: CBDOS:BOARD=<board_id> SOC=<soc> VER=<version>
    // + responder "CBDOS:VERSION?" si algún día se cablea el RX CDC al CLI.
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "%s",
        cbdos::board_identity::bannerFor(
            *cbdos::board_identity::findBoard("jc4880p443")).c_str());
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "=== Iniciando CyBerDeck OS (CBDos v0.2.3-dev) ===");
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Soporte Flasheador Coprocesador C6: %s", 
                       cbdos::flasher::isSupported() ? "HABILITADO" : "DESHABILITADO");

    // 0. Inicializar NVS Flash para persistencia de configuraciones
    esp_err_t nvsRet = nvs_flash_init();
    if (nvsRet == ESP_ERR_NVS_NO_FREE_PAGES || nvsRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvsRet = nvs_flash_init();
    }
    if (nvsRet != ESP_OK) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando NVS Flash: %s", esp_err_to_name(nvsRet));
    }

    // Inyectar el backend de persistencia NVS, Almacenamiento, Audio, UART, GPIO, Radio, Red, Sockets, Transporte de Malla, Cliente HTTP y USB HID
    cbdos::bsp::initPersistenceBackend();
    cbdos::bsp::initStorageBackend();
    cbdos::bsp::initAudioBackendP4();
    cbdos::bsp::initUartBackendP4();
    cbdos::bsp::initGpioBackendP4();
    cbdos::bsp::initNetworkAdapterP4();
    cbdos::bsp::initSocketBackendP4();
    cbdos::bsp::initSshBackendP4();
    cbdos::bsp::initRadioBackendP4();
    cbdos::bsp::initMeshTransportP4();
    cbdos::bsp::initHttpClientP4();
    cbdos::bsp::initHidDriverP4();
    // Gestor USB de sistema: el único PHY HS es exclusivo por arranque.
    // Decide qué stack es dueño del hardware; las apps solo piden modo vía UsbManager.
    cbdos::usb::UsbManager::getInstance().init();
    if (cbdos::usb::UsbManager::getInstance().getBootMode() ==
        cbdos::usb::UsbMode::Host) {
        cbdos::usb::UsbDeviceManager::getInstance().init();
    } else {
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG,
                           "USB modo HID: stack Host no iniciado (PHY libre para TinyUSB)");
    }

    // Registrar servicio de TTS (Offline-First: permanece en reposo hasta su primer uso)
    cbdos::tts::setTTSService(&cbdos::tts::PicoTTSService::getInstance());

    // Inicializar radio determinista segun NVS (Offline-First)
    cbdos::radio::init();

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
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "Preferencias NVS: Brillo=%d%%, Vol=%d%%, Auto-WiFi=%s, TZ Offset=%ld", 
                       sysCfg.brightness, sysCfg.volume, sysCfg.autoConnectWifi ? "SI" : "NO", (long)sysCfg.gmtOffsetSeconds);

    // Inicializar Servicio de Hora NTP con zona horaria persistida
    TimeConfig timeCfg;
    ConfigManager::getInstance().loadTime(timeCfg);
    cbdos::time::init(cbdos::bsp::getEspIdfTimeProvider());
    cbdos::time::setNtpServer(timeCfg.ntpServer.c_str());
    cbdos::time::setTimezone(timeCfg.gmtOffsetSeconds, timeCfg.daylightOffsetSeconds);
    cbdos::time::setNtpEnabled(timeCfg.enabled);

    // 1. Inicializar Subsistema de Almacenamiento (MicroSD / Flash)
    if (!cbdos::storage::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: MicroSD no insertada o no montada al inicio");
    }

    // 2. Inicializar Subsistema de Pantalla a través de la API
    if (!cbdos::display::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando Display");
        return;
    }
    // Aplicar brillo configurado
    cbdos::display::setBrightness(sysCfg.brightness);
    
    // 3. Inicializar Entrada Táctil a través de la API
    if (!cbdos::input::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: Touch no detectado o fallo inicializacion");
    }

    // 4. Inicializar Subsistema de Audio (I2S + Códec ES8311)
    if (!cbdos::audio::init()) {
        cbdos::system::log(cbdos::system::LogLevel::Warn, TAG, "Aviso: Fallo al inicializar subsistema de audio");
    }
    // Aplicar volumen configurado
    cbdos::audio::setVolume(sysCfg.volume);
    
    // 5. Inicializar Puerto LVGL 9.5
    if (LVGL_Port::getInstance().init() != ESP_OK) {
        cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando LVGL 9.5 Port");
        return;
    }

    // 6. Inicializar Sistema de Interfaz Gráfica Universal
    if (LVGL_Port::getInstance().lock()) {
        if (!cbdos::ui::init()) {
            cbdos::system::log(cbdos::system::LogLevel::Error, TAG, "Error inicializando UI Core");
            LVGL_Port::getInstance().unlock();
            return;
        }
        LVGL_Port::getInstance().unlock();
    }

    // 7. Autoconexión Wi-Fi en segundo plano si estaba activo
    if (sysCfg.autoConnectWifi) {
        xTaskCreatePinnedToCore([](void*) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            WiFiConfig wifiCfg;
            if (ConfigManager::getInstance().loadWiFi(wifiCfg) && wifiCfg.ssid.length() > 0) {
                cbdos::system::log(cbdos::system::LogLevel::Info, "AutoWiFi", "Iniciando conexion automatica a '%s'...", wifiCfg.ssid.c_str());
                if (wifiCfg.useStaticIp) {
                    cbdos::network::connectWifiStatic(
                        wifiCfg.ssid.c_str(),
                        wifiCfg.password.c_str(),
                        wifiCfg.staticIp.c_str(),
                        wifiCfg.gateway.c_str(),
                        wifiCfg.subnet.length() > 0 ? wifiCfg.subnet.c_str() : "255.255.255.0",
                        wifiCfg.dns1.c_str()
                    );
                } else {
                    cbdos::network::connectWifi(wifiCfg.ssid.c_str(), wifiCfg.password.c_str());
                }
            }
            vTaskDelete(nullptr);
        }, "auto_wifi_task", 4096, nullptr, 1, nullptr, 0);
    }

    auto caps = cbdos::display::getCapabilities();
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG, "CyBerDeck OS v0.2.0 iniciado y operando a %d FPS en %dx%d!", 
                       caps.targetFps, caps.width, caps.height);
}



