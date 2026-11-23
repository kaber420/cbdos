#include "cbdos/network.hpp"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <lwip/inet.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

static const char* TAG = "CBDOS_NET_P4";

namespace cbdos {
namespace bsp {

class P4NetworkAdapter : public network::INetworkAdapter {
public:
    ~P4NetworkAdapter() override = default;

    bool init() override {
        if (m_initialized) {
            return true;
        }

        ESP_LOGI(TAG, "Inicializando subsistema de red (ESP-Hosted SDIO)...");

        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Fallo nvs_flash_init: %s", esp_err_to_name(ret));
        }

        esp_err_t err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo esp_netif_init: %s", esp_err_to_name(err));
            m_status = network::NetStatus::Error;
            return false;
        }

        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo esp_event_loop_create_default: %s", esp_err_to_name(err));
        }

        if (!m_sta_netif) {
            m_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (!m_sta_netif) {
                m_sta_netif = esp_netif_create_default_wifi_sta();
            }
        }

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Fallo esp_wifi_init: %s", esp_err_to_name(err));
            m_status = network::NetStatus::Error;
            return false;
        }

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, &instance_any_id);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, this, &instance_got_ip);

        // El scan del gestor pudo arrancar el wifi antes de existir el netif;
        // parar y rearrancar deja el orden canonico: netif -> start -> connect.
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();

        m_initialized = true;
        m_status = network::NetStatus::Disconnected;
        return true;
    }

    bool connectWifi(const char* ssid, const char* password) override {
        ESP_LOGI(TAG, "Solicitando conexion Wi-Fi -> SSID: '%s', Pass: '%s'", ssid ? ssid : "(null)", (password && strlen(password) > 0) ? "******" : "(vacio)");
        if (!ssid || strlen(ssid) == 0) {
            ESP_LOGW(TAG, "SSID nulo o vacio, cancelando conexion");
            return false;
        }
        if (!m_initialized && !init()) {
            ESP_LOGE(TAG, "Fallo al inicializar subsistema de red");
            return false;
        }

        // Asegurar DHCP habilitado
        if (m_sta_netif) {
            esp_netif_dhcpc_start(m_sta_netif);
        }

        wifi_config_t wifi_config;
        std::memset(&wifi_config, 0, sizeof(wifi_config));
        std::strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        if (password && strlen(password) > 0) {
            std::strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        }
        wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

        m_status = network::NetStatus::Connecting;
        m_ip = "0.0.0.0";

        ESP_LOGI(TAG, "Iniciando esp_wifi_connect()...");
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Fallo esp_wifi_connect: %s", esp_err_to_name(err));
            m_status = network::NetStatus::Error;
            return false;
        }

        ESP_LOGI(TAG, "esp_wifi_connect() enviado con exito al C6");
        return true;
    }

    bool connectWifiStatic(const char* ssid, const char* password, const char* ip, const char* gateway, const char* subnet, const char* dns) override {
        if (!ssid || strlen(ssid) == 0) return false;
        if (!m_initialized && !init()) return false;

        if (m_sta_netif && ip && gateway) {
            esp_netif_dhcpc_stop(m_sta_netif);

            esp_netif_ip_info_t ip_info;
            std::memset(&ip_info, 0, sizeof(ip_info));
            ip_info.ip.addr = esp_ip4addr_aton(ip);
            ip_info.gw.addr = esp_ip4addr_aton(gateway);
            ip_info.netmask.addr = esp_ip4addr_aton(subnet ? subnet : "255.255.255.0");

            esp_netif_set_ip_info(m_sta_netif, &ip_info);

            if (dns && strlen(dns) > 0) {
                esp_netif_dns_info_t dns_info;
                dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton(dns);
                dns_info.ip.type = ESP_IPADDR_TYPE_V4;
                esp_netif_set_dns_info(m_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
            }
        }

        return connectWifi(ssid, password);
    }

    void disconnectWifi() override {
        if (m_initialized) {
            esp_wifi_disconnect();
        }
        m_status = network::NetStatus::Disconnected;
        m_ip = "0.0.0.0";
        m_rssi = -127;
    }

    network::NetStatus getStatus() const override {
        return m_status;
    }

    bool isConnected() const override {
        return (m_status == network::NetStatus::Connected);
    }

    std::string getIpAddress() const override {
        return m_ip;
    }

    int8_t getRssi() const override {
        if (m_status == network::NetStatus::Connected) {
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                m_rssi = ap_info.rssi;
                return m_rssi;
            }
        }
        return -127;
    }

private:
    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
        auto* self = static_cast<P4NetworkAdapter*>(arg);
        if (!self) return;

        if (event_base == WIFI_EVENT) {
            switch (event_id) {
                case WIFI_EVENT_STA_START:
                    ESP_LOGI(TAG, "WiFi Station iniciada");
                    break;
                case WIFI_EVENT_STA_CONNECTED:
                    ESP_LOGI(TAG, "WiFi conectado al AP, esperando IP...");
                    self->m_status = network::NetStatus::Connecting;
                    break;
                case WIFI_EVENT_STA_DISCONNECTED:
                    ESP_LOGW(TAG, "WiFi desconectado");
                    self->m_status = network::NetStatus::Disconnected;
                    self->m_ip = "0.0.0.0";
                    self->m_rssi = -127;
                    break;
                default:
                    break;
            }
        } else if (event_base == IP_EVENT) {
            if (event_id == IP_EVENT_STA_GOT_IP) {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                char ip_str[IP4ADDR_STRLEN_MAX];
                esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
                self->m_ip = ip_str;
                self->m_status = network::NetStatus::Connected;
                ESP_LOGI(TAG, "WiFi conectado con éxito. Dirección IP: %s", self->m_ip.c_str());
            }
        }
    }

    network::NetStatus m_status = network::NetStatus::Disconnected;
    std::string m_ip = "0.0.0.0";
    mutable int8_t m_rssi = -127;
    esp_netif_t* m_sta_netif = nullptr;
    bool m_initialized = false;
};

static P4NetworkAdapter s_p4NetworkAdapter;

void initNetworkAdapterP4() {
    network::setNetworkAdapter(&s_p4NetworkAdapter);
    ESP_LOGI(TAG, "P4 Network Adapter (ESP-Hosted) inicializado e inyectado.");
}

} // namespace bsp
} // namespace cbdos
