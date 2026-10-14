#include "cbdos/http.hpp"
#include <algorithm>

namespace cbdos {
namespace http {

static IHttpClient* s_httpClient = nullptr;

const char* httpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::Get:    return "GET";
        case HttpMethod::Post:   return "POST";
        case HttpMethod::Put:    return "PUT";
        case HttpMethod::Delete: return "DELETE";
        case HttpMethod::Patch:  return "PATCH";
        case HttpMethod::Head:   return "HEAD";
        default:                 return "GET";
    }
}

HttpMethod stringToHttpMethod(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "POST") return HttpMethod::Post;
    if (upper == "PUT") return HttpMethod::Put;
    if (upper == "DELETE") return HttpMethod::Delete;
    if (upper == "PATCH") return HttpMethod::Patch;
    if (upper == "HEAD") return HttpMethod::Head;
    return HttpMethod::Get;
}

HttpResponse IHttpClient::get(const std::string& url, uint32_t timeoutMs) {
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = url;
    req.timeoutMs = timeoutMs;
    return execute(req);
}

HttpResponse IHttpClient::post(const std::string& url, const std::string& payload,
                              const std::string& contentType, uint32_t timeoutMs) {
    HttpRequest req;
    req.method = HttpMethod::Post;
    req.url = url;
    req.body = payload;
    req.timeoutMs = timeoutMs;
    if (!contentType.empty()) {
        req.headers.push_back({"Content-Type", contentType});
    }
    return execute(req);
}

void setClient(IHttpClient* client) {
    s_httpClient = client;
}

IHttpClient* getClient() {
    return s_httpClient;
}

HttpResponse execute(const HttpRequest& request) {
    if (s_httpClient) {
        return s_httpClient->execute(request);
    }
    HttpResponse res;
    res.statusCode = -1;
    res.success = false;
    res.errorMessage = "No HTTP client registered";
    return res;
}

HttpResponse get(const std::string& url, uint32_t timeoutMs) {
    if (s_httpClient) {
        return s_httpClient->get(url, timeoutMs);
    }
    HttpResponse res;
    res.statusCode = -1;
    res.success = false;
    res.errorMessage = "No HTTP client registered";
    return res;
}

HttpResponse post(const std::string& url, const std::string& payload, const std::string& contentType, uint32_t timeoutMs) {
    if (s_httpClient) {
        return s_httpClient->post(url, payload, contentType, timeoutMs);
    }
    HttpResponse res;
    res.statusCode = -1;
    res.success = false;
    res.errorMessage = "No HTTP client registered";
    return res;
}

} // namespace http
} // namespace cbdos

