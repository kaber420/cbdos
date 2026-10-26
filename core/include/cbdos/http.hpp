#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace http {

enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head
};

const char* httpMethodToString(HttpMethod method);
HttpMethod stringToHttpMethod(const std::string& str);

struct HttpHeader {
    std::string key;
    std::string value;
};

struct HttpRequest {
    HttpMethod method = HttpMethod::Get;
    std::string url;
    std::vector<HttpHeader> headers;
    std::string body;
    uint32_t timeoutMs = 8000;
    size_t maxBodyCaptureBytes = 32768; // Límite defensivo de 32 KB para evitar OOM
    bool insecure = true;               // Permitir certificados autofirmados (ej. Ubiquiti AirOS / LAN routers)
};

struct HttpResponse {
    int statusCode = 0;
    std::string statusText;
    std::vector<HttpHeader> headers;
    std::string body;
    uint32_t rttMs = 0;        // Tiempo total de ida y vuelta en milisegundos
    bool success = false;
    bool truncated = false;    // Indica si el cuerpo excedió maxBodyCaptureBytes
    std::string errorMessage;  // Detalle de fallo en caso de error de conexión/DNS
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // Contrato principal completo
    virtual HttpResponse execute(const HttpRequest& request) = 0;

    // Métodos de conveniencia retrocompatibles
    virtual HttpResponse get(const std::string& url, uint32_t timeoutMs = 5000);
    virtual HttpResponse post(const std::string& url, const std::string& payload,
                              const std::string& contentType = "application/json",
                              uint32_t timeoutMs = 5000);
};

void setClient(IHttpClient* client);
IHttpClient* getClient();

// Funciones globales de conveniencia
HttpResponse execute(const HttpRequest& request);
HttpResponse get(const std::string& url, uint32_t timeoutMs = 5000);
HttpResponse post(const std::string& url, const std::string& payload,
                  const std::string& contentType = "application/json",
                  uint32_t timeoutMs = 5000);

} // namespace http
} // namespace cbdos

