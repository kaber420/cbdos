#include "cbdos/http.hpp"
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>

static const char* TAG_HTTP = "ESP_HTTP_P4";

namespace cbdos {
namespace bsp {

class EspIdfHttpClient : public http::IHttpClient {
private:
    static esp_err_t httpEventHandler(esp_http_client_event_t* evt) {
        if (!evt || !evt->user_data) return ESP_OK;
        auto* response = static_cast<http::HttpResponse*>(evt->user_data);

        if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->header_key && evt->header_value) {
            // Si ya existe la cabecera (ej. múltiples Set-Cookie), concatenamos con "; " o actualizamos
            std::string key = evt->header_key;
            std::string val = evt->header_value;
            bool merged = false;
            if (strcasecmp(key.c_str(), "Set-Cookie") == 0) {
                // Extraer solo el par clave=valor principal antes del primer ';' para facilitar reenvío en Cookie:
                for (auto& existing : response->headers) {
                    if (strcasecmp(existing.key.c_str(), "Set-Cookie") == 0) {
                        existing.value += "; " + val;
                        merged = true;
                        break;
                    }
                }
            }
            if (!merged) {
                response->headers.push_back({std::move(key), std::move(val)});
            }
        }
        return ESP_OK;
    }

public:
    http::HttpResponse execute(const http::HttpRequest& request) override {
        http::HttpResponse response;
        response.statusCode = 0;
        response.success = false;

        esp_http_client_config_t config = {};
        config.url = request.url.c_str();
        config.timeout_ms = request.timeoutMs;
        config.disable_auto_redirect = false;
        config.max_redirection_count = 3;
        config.user_agent = "CBDos-Http/1.0 (ESP32-P4)";
        config.buffer_size = 2048;
        config.buffer_size_tx = 1024;
        config.event_handler = httpEventHandler;
        config.user_data = &response;
        if (request.insecure) {
            config.skip_cert_common_name_check = true;
        }

        esp_http_client_method_t espMethod = HTTP_METHOD_GET;
        switch (request.method) {
            case http::HttpMethod::Get:    espMethod = HTTP_METHOD_GET; break;
            case http::HttpMethod::Post:   espMethod = HTTP_METHOD_POST; break;
            case http::HttpMethod::Put:    espMethod = HTTP_METHOD_PUT; break;
            case http::HttpMethod::Delete: espMethod = HTTP_METHOD_DELETE; break;
            case http::HttpMethod::Patch:  espMethod = HTTP_METHOD_PATCH; break;
            case http::HttpMethod::Head:   espMethod = HTTP_METHOD_HEAD; break;
        }
        config.method = espMethod;

        int64_t tStart = esp_timer_get_time();

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            response.errorMessage = "Fallo inicializando esp_http_client";
            return response;
        }

        esp_http_client_set_header(client, "Connection", "close");
        for (const auto& hdr : request.headers) {
            esp_http_client_set_header(client, hdr.key.c_str(), hdr.value.c_str());
        }

        int writeLen = 0;
        if (!request.body.empty() &&
            (request.method == http::HttpMethod::Post ||
             request.method == http::HttpMethod::Put ||
             request.method == http::HttpMethod::Patch ||
             request.method == http::HttpMethod::Delete)) {
            writeLen = static_cast<int>(request.body.length());
        }

        esp_err_t err = esp_http_client_open(client, writeLen);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_HTTP, "Error conexion HTTP: %s", esp_err_to_name(err));
            response.errorMessage = esp_err_to_name(err);
            esp_http_client_cleanup(client);
            return response;
        }

        if (writeLen > 0) {
            int written = esp_http_client_write(client, request.body.c_str(), writeLen);
            if (written < 0) {
                ESP_LOGE(TAG_HTTP, "Error escribiendo payload HTTP");
                response.errorMessage = "Error enviando cuerpo HTTP";
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return response;
            }
        }

        int contentLength = esp_http_client_fetch_headers(client);
        int statusCode = esp_http_client_get_status_code(client);
        response.statusCode = statusCode;
        response.success = (statusCode >= 200 && statusCode < 300);

        // Captura defensiva de respuesta con límite de memoria (OOM protection)
        size_t maxCap = request.maxBodyCaptureBytes > 0 ? request.maxBodyCaptureBytes : 32768;
        char buf[1024];
        int r = 0;
        while ((r = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0) {
            if (response.body.size() + r > maxCap) {
                size_t allowed = maxCap > response.body.size() ? (maxCap - response.body.size()) : 0;
                if (allowed > 0) {
                    response.body.append(buf, allowed);
                }
                response.truncated = true;
                break;
            } else {
                response.body.append(buf, r);
            }
            if (esp_http_client_is_complete_data_received(client)) break;
        }

        int64_t tEnd = esp_timer_get_time();
        response.rttMs = static_cast<uint32_t>((tEnd - tStart) / 1000);

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return response;
    }
};

static EspIdfHttpClient s_p4HttpClient;

void initHttpClientP4() {
    http::setClient(&s_p4HttpClient);
}

} // namespace bsp
} // namespace cbdos
