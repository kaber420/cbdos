#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "security/WifiShieldService.hpp"
#include "cbdos/system.hpp"

namespace cbdos {
namespace bsp {

static const char* TAG_WIFI = "HAL_WIFI_S3";

static TaskHandle_t s_scanTask = nullptr;
static volatile bool s_scanRunning = false;
static volatile bool s_promiscuous = false;
static bool s_wifiStarted = false;

static radio::WifiAuthMode mapAuth(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return radio::WifiAuthMode::Open;
        case WIFI_AUTH_WEP: return radio::WifiAuthMode::WEP;
        case WIFI_AUTH_WPA_PSK: return radio::WifiAuthMode::WPA;
        case WIFI_AUTH_WPA2_PSK: return radio::WifiAuthMode::WPA2;
        case WIFI_AUTH_WPA_WPA2_PSK: return radio::WifiAuthMode::WPA2;
        case WIFI_AUTH_WPA2_ENTERPRISE: return radio::WifiAuthMode::WPA2Enterprise;
        case WIFI_AUTH_WPA3_PSK: return radio::WifiAuthMode::WPA3;
        case WIFI_AUTH_WPA2_WPA3_PSK: return radio::WifiAuthMode::WPA3;
        case WIFI_AUTH_WPA3_ENT_192: return radio::WifiAuthMode::WPA3Enterprise;
        default: return radio::WifiAuthMode::Unknown;
    }
}

static void wifiEnsureStarted() {
    if (!s_wifiStarted) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true, false);
        s_wifiStarted = true;
        cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Stack Wi-Fi STA iniciado para Shield");
    }
}

// ────────────────────────────────────────────────────────────────
// Modo promiscuo: captura 802.11 para IDS / Sniffer
// ────────────────────────────────────────────────────────────────

static void wifiPromiscuousCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!buf) return;
    auto* pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
    const uint8_t* p = pkt->payload;
    if (!p) return;

    radio::WifiRawFrame frame;
    uint8_t fc0 = p[0];
    frame.type = static_cast<radio::WifiFrameType>((fc0 >> 2) & 0x03);
    frame.subtype = (fc0 >> 4) & 0x0F;

    // Layout 802.11: addr1=dst, addr2=src, addr3=bssid (tramas de gestion)
    memcpy(frame.da, p + 4, 6);
    memcpy(frame.sa, p + 10, 6);
    memcpy(frame.bssid, p + 16, 6);
    frame.channel = pkt->rx_ctrl.channel;
    frame.rssi = static_cast<int8_t>(pkt->rx_ctrl.rssi);
    frame.timestampMs = millis();

    security::WifiShieldService::getInstance().processRawFrame(frame);
}

static bool wifiSnifferControl(bool enable) {
    if (enable) {
        wifiEnsureStarted();
        if (!s_promiscuous) {
            esp_wifi_set_promiscuous_rx_cb(wifiPromiscuousCb);
            esp_wifi_set_promiscuous(true);
            s_promiscuous = true;
            cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Modo promiscuo 802.11 ACTIVADO");
        }
    } else {
        if (s_promiscuous) {
            esp_wifi_set_promiscuous(false);
            esp_wifi_set_promiscuous_rx_cb(nullptr);
            s_promiscuous = false;
            cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Modo promiscuo 802.11 DESACTIVADO");
        }
    }
    return true;
}

// ────────────────────────────────────────────────────────────────
// Escaneo de APs (Recon)
// ────────────────────────────────────────────────────────────────

static void wifiScanWorker(void* param) {
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Task de recon Wi-Fi iniciada");
    while (s_scanRunning) {
        if (!s_promiscuous) {
            int n = WiFi.scanNetworks(false, true, false, 120);
            for (int i = 0; i < n; ++i) {
                radio::WifiApRecord ap;
                uint8_t* bssid = WiFi.BSSID(i);
                if (bssid) memcpy(ap.bssid, bssid, 6);
                ap.ssid = WiFi.SSID(i).c_str();
                ap.hidden = (ap.ssid.empty());
                ap.rssi = static_cast<int8_t>(WiFi.RSSI(i));
                ap.channel = static_cast<uint8_t>(WiFi.channel(i));
                ap.auth = mapAuth(static_cast<wifi_auth_mode_t>(WiFi.encryptionType(i)));
                ap.lastSeenMs = millis();
                security::WifiShieldService::getInstance().processApRecord(ap);
            }
            WiFi.scanDelete();
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    s_scanTask = nullptr;
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Task de recon Wi-Fi detenida");
    vTaskDelete(NULL);
}

static bool wifiScanControl(bool start) {
    if (start) {
        wifiEnsureStarted();
        if (!s_scanRunning) {
            s_scanRunning = true;
            if (!s_scanTask) {
                xTaskCreatePinnedToCore(wifiScanWorker, "wifi_scan_task", 6144, NULL, 1, &s_scanTask, 0);
            }
            cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Recon Wi-Fi iniciado");
            return true;
        }
    } else {
        if (s_scanRunning) {
            s_scanRunning = false;
            cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Recon Wi-Fi detenido");
            return true;
        }
    }
    return false;
}

void initWifiShieldS3() {
    auto& svc = security::WifiShieldService::getInstance();
    svc.registerScanControl(wifiScanControl);
    svc.registerSnifferControl(wifiSnifferControl);
    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_WIFI, "Driver Wi-Fi Shield registrado");
}

} // namespace bsp
} // namespace cbdos
