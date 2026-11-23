#include "cbdos/time.hpp"
#include <esp_sntp.h>
#include <esp_log.h>
#include <string>

namespace cbdos {
namespace bsp {

class EspIdfTimeProvider : public cbdos::time::ITimeProvider {
private:
    std::string m_server;

    static void time_sync_notification_cb(struct timeval *tv) {
        ESP_LOGI("EspIdfTime", "Sincronizacion SNTP exitosa");
        cbdos::time::notifySynced(cbdos::time::TimeSource::SNTP);
    }

public:
    EspIdfTimeProvider() {
        m_server = "pool.ntp.org";
    }

    void init(long gmtOffsetSec, int daylightOffsetSec, const char* ntpServer) override {
        if (ntpServer) m_server = ntpServer;
        // En ESP-IDF, esp_sntp requiere que LwIP esté iniciado (esp_netif).
        // Por la Ley Offline-First, no tocamos LwIP/SNTP en el arranque inicial.
    }
    
    void setTimezone(long gmtOffsetSec, int daylightOffsetSec) override {
        // TZ is handled by core
    }
    
    void setServer(const char* ntpServer) override {
        if (ntpServer) {
            m_server = ntpServer;
            if (esp_sntp_enabled()) {
                esp_sntp_setservername(0, m_server.c_str());
            }
        }
    }
    
    void syncNtp() override {
        if (esp_sntp_enabled()) {
            esp_sntp_stop();
        }
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        esp_sntp_setservername(0, m_server.c_str());
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_setservername(2, "time.google.com");
        esp_sntp_init();
    }
    
    void stopNtp() override {
        if (esp_sntp_enabled()) {
            esp_sntp_stop();
        }
    }
};

static EspIdfTimeProvider s_timeProvider;

cbdos::time::ITimeProvider* getEspIdfTimeProvider() {
    return &s_timeProvider;
}

} // namespace bsp
} // namespace cbdos
