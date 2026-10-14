#include "RestProberService.hpp"
#include "cbdos/network.hpp"
#include "cbdos/storage.hpp"

#include <cstdio>
#include <sstream>
#include <algorithm>

namespace cbdos {
namespace network {

namespace {

std::string escapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

std::string sanitizeFilename(const std::string& name) {
    std::string clean;
    clean.reserve(name.size());
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-') {
            clean += c;
        } else if (c == ' ') {
            clean += '_';
        }
    }
    if (clean.empty()) clean = "req_item";
    return clean;
}

// Parser elemental de JSON clave-valor para archivos de colecciones guardadas
bool parseJsonField(const std::string& json, const std::string& key, std::string& outValue) {
    std::string target = "\"" + key + "\"";
    size_t pos = json.find(target);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos + target.size());
    if (pos == std::string::npos) return false;

    // Buscar primer caracter util
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return false;

    if (json[pos] == '"') {
        // String delimitado
        size_t start = pos + 1;
        std::string val;
        for (size_t i = start; i < json.size(); ++i) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                char next = json[i + 1];
                if (next == '"') val += '"';
                else if (next == '\\') val += '\\';
                else if (next == 'n') val += '\n';
                else if (next == 'r') val += '\r';
                else if (next == 't') val += '\t';
                else val += next;
                i++;
            } else if (json[i] == '"') {
                outValue = val;
                return true;
            } else {
                val += json[i];
            }
        }
        return false;
    }
    return false;
}

} // namespace

RestProberService& RestProberService::getInstance() {
    static RestProberService instance;
    return instance;
}

RestProberService::RestProberService() {
    m_mutex = cbdos::rtos::createMutex();
}

RestProberService::~RestProberService() {
    if (m_mutex) {
        cbdos::rtos::deleteMutex(m_mutex);
        m_mutex = nullptr;
    }
}

std::string RestProberService::getStorageDirectory() {
    std::string dir = "/sdcard/requests";
    if (!cbdos::storage::isSdMounted()) {
        dir = "/littlefs/requests";
    }
    cbdos::storage::makeDir(dir.c_str());
    return dir;
}

bool RestProberService::sendRequestAsync(const http::HttpRequest& request,
                                         std::function<void(const http::HttpResponse&)> onComplete) {
    bool expected = false;
    if (!m_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false; // Ya hay una petición en curso
    }

    if (m_mutex) {
        cbdos::rtos::lockMutex(m_mutex);
        m_activeRequest = request;
        m_onComplete = onComplete;
        cbdos::rtos::unlockMutex(m_mutex);
    } else {
        m_activeRequest = request;
        m_onComplete = onComplete;
    }

    // Verificar si hay red activa antes de lanzar la tarea
    if (!cbdos::network::isConnected()) {
        http::HttpResponse res;
        res.statusCode = 0;
        res.success = false;
        res.errorMessage = "Sin conexion de red (Wi-Fi inactivo)";
        
        if (m_mutex) cbdos::rtos::lockMutex(m_mutex);
        m_lastResponse = res;
        auto cb = m_onComplete;
        m_busy.store(false, std::memory_order_release);
        if (m_mutex) cbdos::rtos::unlockMutex(m_mutex);

        if (cb) cb(res);
        return false;
    }

    // Despachar a tarea en Core 0
    cbdos::rtos::TaskHandle task = cbdos::rtos::createTask(
        taskWorker, "rest_prober_task", 12288, this, 3, 0);

    if (!task && m_busy.load(std::memory_order_acquire)) {
        // En stubs síncronos el worker corre de inmediato; si falló la creación:
        m_busy.store(false, std::memory_order_release);
        return false;
    }

    return true;
}

void RestProberService::taskWorker(void* param) {
    auto* self = static_cast<RestProberService*>(param);
    if (self) {
        self->runWorker();
    }
    cbdos::rtos::deleteTask(nullptr);
}

void RestProberService::runWorker() {
    http::HttpRequest reqCopy;
    std::function<void(const http::HttpResponse&)> cbCopy;

    if (m_mutex) cbdos::rtos::lockMutex(m_mutex);
    reqCopy = m_activeRequest;
    cbCopy = m_onComplete;
    if (m_mutex) cbdos::rtos::unlockMutex(m_mutex);

    // Ejecución a través del contrato HAL cbdos::http
    http::HttpResponse response = cbdos::http::execute(reqCopy);

    if (m_mutex) cbdos::rtos::lockMutex(m_mutex);
    m_lastResponse = response;
    m_busy.store(false, std::memory_order_release);
    if (m_mutex) cbdos::rtos::unlockMutex(m_mutex);

    if (cbCopy) {
        cbCopy(response);
    }
}

http::HttpResponse RestProberService::getLastResponse() const {
    http::HttpResponse res;
    if (m_mutex) cbdos::rtos::lockMutex(m_mutex);
    res = m_lastResponse;
    if (m_mutex) cbdos::rtos::unlockMutex(m_mutex);
    return res;
}

http::HttpRequest RestProberService::getLastRequest() const {
    http::HttpRequest req;
    if (m_mutex) cbdos::rtos::lockMutex(m_mutex);
    req = m_activeRequest;
    if (m_mutex) cbdos::rtos::unlockMutex(m_mutex);
    return req;
}

bool RestProberService::saveRequest(const std::string& name, const http::HttpRequest& req) {
    std::string dir = getStorageDirectory();
    std::string safeName = sanitizeFilename(name);
    std::string path = dir + "/" + safeName + ".json";

    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << escapeJsonString(name) << "\",\n";
    ss << "  \"method\": \"" << http::httpMethodToString(req.method) << "\",\n";
    ss << "  \"url\": \"" << escapeJsonString(req.url) << "\",\n";
    ss << "  \"body\": \"" << escapeJsonString(req.body) << "\"\n";
    ss << "}\n";

    return cbdos::storage::writeFile(path.c_str(), ss.str());
}

bool RestProberService::loadRequest(const std::string& filename, http::HttpRequest& outReq) {
    std::string dir = getStorageDirectory();
    std::string path = dir + "/" + filename;
    std::string content = cbdos::storage::readFile(path.c_str());
    if (content.empty()) return false;

    std::string methodStr;
    std::string urlStr;
    std::string bodyStr;

    if (parseJsonField(content, "method", methodStr)) {
        outReq.method = http::stringToHttpMethod(methodStr);
    }
    if (parseJsonField(content, "url", urlStr)) {
        outReq.url = urlStr;
    }
    if (parseJsonField(content, "body", bodyStr)) {
        outReq.body = bodyStr;
    }
    return true;
}

std::vector<SavedRequestInfo> RestProberService::listSavedRequests() {
    std::vector<SavedRequestInfo> list;
    std::string dir = getStorageDirectory();
    std::vector<cbdos::storage::FileEntry> entries = cbdos::storage::listDir(dir.c_str());

    for (const auto& entry : entries) {
        if (entry.isDirectory) continue;
        if (entry.name.size() > 5 && entry.name.substr(entry.name.size() - 5) == ".json") {
            SavedRequestInfo info;
            info.filename = entry.name;
            info.name = entry.name.substr(0, entry.name.size() - 5);
            std::string content = cbdos::storage::readFile((dir + "/" + entry.name).c_str());
            std::string parsedName;
            if (parseJsonField(content, "name", parsedName) && !parsedName.empty()) {
                info.name = parsedName;
            }
            std::string methodStr, urlStr, bodyStr;
            if (parseJsonField(content, "method", methodStr)) {
                info.request.method = http::stringToHttpMethod(methodStr);
            }
            if (parseJsonField(content, "url", urlStr)) {
                info.request.url = urlStr;
            }
            if (parseJsonField(content, "body", bodyStr)) {
                info.request.body = bodyStr;
            }
            list.push_back(info);
        }
    }
    return list;
}

bool RestProberService::deleteRequest(const std::string& filename) {
    std::string dir = getStorageDirectory();
    std::string path = dir + "/" + filename;
    return cbdos::storage::deleteFile(path.c_str());
}

std::vector<SavedRequestInfo> RestProberService::getBuiltinPresets() const {
    std::vector<SavedRequestInfo> presets;

    {
        SavedRequestInfo p;
        p.name = "Shelly 1 Relay Toggle";
        p.request.method = http::HttpMethod::Post;
        p.request.url = "http://192.168.1.100/relay/0?turn=toggle";
        presets.push_back(p);
    }
    {
        SavedRequestInfo p;
        p.name = "Shelly Device Status";
        p.request.method = http::HttpMethod::Get;
        p.request.url = "http://192.168.1.100/status";
        presets.push_back(p);
    }
    {
        SavedRequestInfo p;
        p.name = "Tasmota Status Info";
        p.request.method = http::HttpMethod::Get;
        p.request.url = "http://192.168.1.100/cm?cmnd=Status";
        presets.push_back(p);
    }
    {
        SavedRequestInfo p;
        p.name = "WLED JSON State";
        p.request.method = http::HttpMethod::Get;
        p.request.url = "http://192.168.1.100/json/state";
        presets.push_back(p);
    }
    {
        SavedRequestInfo p;
        p.name = "Roku ECP Device Info";
        p.request.method = http::HttpMethod::Get;
        p.request.url = "http://192.168.1.100:8060/query/device-info";
        presets.push_back(p);
    }
    {
        SavedRequestInfo p;
        p.name = "JSONPlaceholder Test";
        p.request.method = http::HttpMethod::Get;
        p.request.url = "http://jsonplaceholder.typicode.com/todos/1";
        presets.push_back(p);
    }

    return presets;
}

} // namespace network
} // namespace cbdos
