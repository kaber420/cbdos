#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "cbdos/http.hpp"
#include "cbdos/rtos.hpp"

namespace cbdos {
namespace network {

struct SavedRequestInfo {
    std::string name;
    std::string filename;
    http::HttpRequest request;
};

class RestProberService {
public:
    static RestProberService& getInstance();

    RestProberService(const RestProberService&) = delete;
    RestProberService& operator=(const RestProberService&) = delete;

    // Ejecuta una petición HTTP en segundo plano en Core 0
    bool sendRequestAsync(const http::HttpRequest& request,
                          std::function<void(const http::HttpResponse&)> onComplete = nullptr);

    // Estado del servicio
    bool isBusy() const { return m_busy.load(std::memory_order_acquire); }
    http::HttpResponse getLastResponse() const;
    http::HttpRequest getLastRequest() const;

    // Gestión de Colecciones / Peticiones Guardadas (cbdos::storage)
    bool saveRequest(const std::string& name, const http::HttpRequest& req);
    bool loadRequest(const std::string& filename, http::HttpRequest& outReq);
    std::vector<SavedRequestInfo> listSavedRequests();
    bool deleteRequest(const std::string& filename);

    // Presets integrados para IoT y diagnóstico rápido
    std::vector<SavedRequestInfo> getBuiltinPresets() const;

private:
    RestProberService();
    ~RestProberService();

    static void taskWorker(void* param);
    void runWorker();

    std::string getStorageDirectory();

    cbdos::rtos::MutexHandle m_mutex{nullptr};
    std::atomic<bool> m_busy{false};

    http::HttpRequest m_activeRequest;
    http::HttpResponse m_lastResponse;
    std::function<void(const http::HttpResponse&)> m_onComplete;
};

} // namespace network
} // namespace cbdos
