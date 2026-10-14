#include "cbdos/http.hpp"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

namespace cbdos {
namespace bsp {

class ArduinoHttpClient : public http::IHttpClient {
public:
    http::HttpResponse execute(const http::HttpRequest& request) override {
        http::HttpResponse response;
        response.statusCode = 0;
        response.success = false;

        WiFiClient client;
        client.setTimeout(request.timeoutMs);

        HTTPClient http;
        http.setTimeout(request.timeoutMs);
        http.setUserAgent("CBDos-Http/1.0 (ESP32-S3)");
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

        uint32_t t0 = millis();

        if (http.begin(client, request.url.c_str())) {
            for (const auto& hdr : request.headers) {
                http.addHeader(hdr.key.c_str(), hdr.value.c_str());
            }

            const char* methodStr = http::httpMethodToString(request.method);
            int httpCode = 0;
            if (!request.body.empty() &&
                (request.method == http::HttpMethod::Post ||
                 request.method == http::HttpMethod::Put ||
                 request.method == http::HttpMethod::Patch ||
                 request.method == http::HttpMethod::Delete)) {
                httpCode = http.sendRequest(methodStr, (uint8_t*)request.body.c_str(), request.body.length());
            } else {
                httpCode = http.sendRequest(methodStr);
            }

            response.rttMs = millis() - t0;
            response.statusCode = httpCode;

            if (httpCode > 0) {
                response.success = (httpCode >= 200 && httpCode < 300);
                std::string fullBody = http.getString().c_str();
                size_t maxCap = request.maxBodyCaptureBytes > 0 ? request.maxBodyCaptureBytes : 32768;
                if (fullBody.length() > maxCap) {
                    response.body = fullBody.substr(0, maxCap);
                    response.truncated = true;
                } else {
                    response.body = std::move(fullBody);
                }
            } else {
                response.errorMessage = http.errorToString(httpCode).c_str();
            }
            http.end();
        } else {
            response.errorMessage = "Fallo al iniciar conexion HTTP";
        }

        return response;
    }
};

static ArduinoHttpClient s_s3HttpClient;

void initHttpClientS3() {
    http::setClient(&s_s3HttpClient);
}

} // namespace bsp
} // namespace cbdos
