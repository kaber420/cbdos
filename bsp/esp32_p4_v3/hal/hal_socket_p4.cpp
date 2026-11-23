#include "cbdos/socket.hpp"
#include <esp_log.h>
#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/sys.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

static const char* TAG_SOCK_P4 = "P4_SOCKET";

namespace cbdos {
namespace bsp {

class P4SocketStream : public network::ISocketStream {
public:
    explicit P4SocketStream(network::SocketType type = network::SocketType::Tcp)
        : m_type(type), m_sock(-1), m_connected(false), m_timeoutMs(5000) {}

    ~P4SocketStream() override {
        close();
    }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000) override {
        close();

        if (host.empty() || port == 0) {
            ESP_LOGE(TAG_SOCK_P4, "Host o puerto invalido: %s:%u", host.c_str(), port);
            return false;
        }

        m_timeoutMs = timeoutMs;

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = (m_type == network::SocketType::Tcp) ? SOCK_STREAM : SOCK_DGRAM;

        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%u", port);

        struct addrinfo* res = nullptr;
        int err = getaddrinfo(host.c_str(), portStr, &hints, &res);
        if (err != 0 || !res) {
            ESP_LOGE(TAG_SOCK_P4, "Resolucion DNS fallo para %s: error %d", host.c_str(), err);
            return false;
        }

        m_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_sock < 0) {
            ESP_LOGE(TAG_SOCK_P4, "Fallo al crear socket para %s:%u (errno %d)", host.c_str(), port, errno);
            freeaddrinfo(res);
            return false;
        }

        applyTimeout(m_timeoutMs);

        if (::connect(m_sock, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGW(TAG_SOCK_P4, "Fallo de conexion TCP a %s:%u (errno %d: %s)", host.c_str(), port, errno, strerror(errno));
            ::close(m_sock);
            m_sock = -1;
            freeaddrinfo(res);
            return false;
        }

        freeaddrinfo(res);
        m_connected = true;
        ESP_LOGI(TAG_SOCK_P4, "Conexion socket establecida con %s:%u (fd %d)", host.c_str(), port, m_sock);
        return true;
    }

    void close() override {
        if (m_sock >= 0) {
            ::close(m_sock);
            m_sock = -1;
        }
        m_connected = false;
    }

    bool isConnected() const override {
        return m_connected && (m_sock >= 0);
    }

    int send(const uint8_t* data, size_t len) override {
        if (!isConnected() || !data || len == 0) {
            return -1;
        }

        int written = ::send(m_sock, data, len, 0);
        if (written < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG_SOCK_P4, "Error al enviar por socket (fd %d, errno %d)", m_sock, errno);
                m_connected = false;
            }
        }
        return written;
    }

    int recv(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 3000) override {
        if (!isConnected() || !buffer || maxLen == 0) {
            return -1;
        }

        if (timeoutMs != m_timeoutMs) {
            applyTimeout(timeoutMs);
        }

        int bytesRead = ::recv(m_sock, buffer, maxLen, 0);
        if (bytesRead == 0) {
            // Conexión cerrada por el servidor
            m_connected = false;
            return 0;
        } else if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout normal de lectura no bloqueante / temporizada
                return 0;
            }
            ESP_LOGW(TAG_SOCK_P4, "Error de recepcion en socket (fd %d, errno %d: %s)", m_sock, errno, strerror(errno));
            m_connected = false;
            return -1;
        }

        return bytesRead;
    }

    size_t available() override {
        if (!isConnected()) return 0;
        int count = 0;
        if (ioctl(m_sock, FIONREAD, &count) < 0 || count < 0) {
            return 0;
        }
        return static_cast<size_t>(count);
    }

    void setTimeout(uint32_t timeoutMs) override {
        applyTimeout(timeoutMs);
    }

private:
    void applyTimeout(uint32_t timeoutMs) {
        m_timeoutMs = timeoutMs;
        if (m_sock >= 0) {
            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(m_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
    }

    network::SocketType m_type;
    int m_sock;
    bool m_connected;
    uint32_t m_timeoutMs;
};

// ────────────────────────────────────────────────────────────────
// Implementación Socket Stream con Cifrado TLS 1.2 / 1.3 (esp_tls)
// ────────────────────────────────────────────────────────────────
class P4TlsSocketStream : public network::ISocketStream {
public:
    P4TlsSocketStream()
        : m_tls(nullptr), m_connected(false), m_insecure(false), m_timeoutMs(6000) {}

    ~P4TlsSocketStream() override {
        close();
    }

    void setInsecure(bool allowInsecure) override {
        m_insecure = allowInsecure;
    }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 6000) override {
        close();

        if (host.empty() || port == 0) {
            ESP_LOGE(TAG_SOCK_P4, "[TLS] Host o puerto invalido: %s:%u", host.c_str(), port);
            return false;
        }

        m_timeoutMs = timeoutMs;
        m_tls = esp_tls_init();
        if (!m_tls) {
            ESP_LOGE(TAG_SOCK_P4, "[TLS] Fallo en esp_tls_init()");
            return false;
        }

        esp_tls_cfg_t cfg = {};
        cfg.timeout_ms = static_cast<int>(timeoutMs);
        cfg.skip_common_name = false; // Mantener SNI habilitado siempre

        if (!m_insecure) {
            cfg.crt_bundle_attach = esp_crt_bundle_attach;
        }

        int ret = esp_tls_conn_new_sync(host.c_str(), static_cast<int>(host.length()),
                                        static_cast<int>(port), &cfg, m_tls);
        if (ret != 1) {
            ESP_LOGW(TAG_SOCK_P4, "[TLS] Fallo de handshake TLS con %s:%u (insecure=%d)",
                     host.c_str(), port, m_insecure ? 1 : 0);
            esp_tls_conn_destroy(m_tls);
            m_tls = nullptr;
            return false;
        }

        applyTimeout(m_timeoutMs);
        m_connected = true;
        ESP_LOGI(TAG_SOCK_P4, "[TLS] Conexion segura establecida con %s:%u", host.c_str(), port);
        return true;
    }

    void close() override {
        if (m_tls) {
            esp_tls_conn_destroy(m_tls);
            m_tls = nullptr;
        }
        m_connected = false;
    }

    bool isConnected() const override {
        return m_connected && (m_tls != nullptr);
    }

    int send(const uint8_t* data, size_t len) override {
        if (!isConnected() || !data || len == 0) {
            return -1;
        }

        size_t totalSent = 0;
        while (totalSent < len) {
            ssize_t written = esp_tls_conn_write(m_tls, data + totalSent, len - totalSent);
            if (written > 0) {
                totalSent += static_cast<size_t>(written);
            } else if (written == ESP_TLS_ERR_SSL_WANT_WRITE || written == ESP_TLS_ERR_SSL_WANT_READ) {
                continue;
            } else {
                ESP_LOGE(TAG_SOCK_P4, "[TLS] Error al escribir en stream TLS (%d)", static_cast<int>(written));
                m_connected = false;
                return -1;
            }
        }
        return static_cast<int>(totalSent);
    }

    int recv(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 3000) override {
        if (!isConnected() || !buffer || maxLen == 0) {
            return -1;
        }

        if (timeoutMs != m_timeoutMs) {
            applyTimeout(timeoutMs);
        }

        ssize_t bytesRead = esp_tls_conn_read(m_tls, buffer, maxLen);
        if (bytesRead > 0) {
            return static_cast<int>(bytesRead);
        } else if (bytesRead == 0) {
            m_connected = false;
            return 0;
        } else if (bytesRead == ESP_TLS_ERR_SSL_WANT_READ || bytesRead == ESP_TLS_ERR_SSL_WANT_WRITE) {
            return 0;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            ESP_LOGW(TAG_SOCK_P4, "[TLS] Error de lectura en stream TLS (%d, errno %d)",
                     static_cast<int>(bytesRead), errno);
            m_connected = false;
            return -1;
        }
    }

    size_t available() override {
        if (!isConnected()) return 0;
        ssize_t avail = esp_tls_get_bytes_avail(m_tls);
        if (avail > 0) {
            return static_cast<size_t>(avail);
        }
        int sockfd = -1;
        if (esp_tls_get_conn_sockfd(m_tls, &sockfd) == ESP_OK && sockfd >= 0) {
            int count = 0;
            if (ioctl(sockfd, FIONREAD, &count) == 0 && count > 0) {
                return static_cast<size_t>(count);
            }
        }
        return 0;
    }

    void setTimeout(uint32_t timeoutMs) override {
        applyTimeout(timeoutMs);
    }

private:
    void applyTimeout(uint32_t timeoutMs) {
        m_timeoutMs = timeoutMs;
        if (m_tls) {
            int sockfd = -1;
            if (esp_tls_get_conn_sockfd(m_tls, &sockfd) == ESP_OK && sockfd >= 0) {
                struct timeval tv;
                tv.tv_sec = timeoutMs / 1000;
                tv.tv_usec = (timeoutMs % 1000) * 1000;
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            }
        }
    }

    esp_tls_t* m_tls;
    bool m_connected;
    bool m_insecure;
    uint32_t m_timeoutMs;
};

class P4SocketFactory : public network::ISocketFactory {
public:
    std::unique_ptr<network::ISocketStream> createSocket(network::SocketType type = network::SocketType::Tcp) override {
        if (type == network::SocketType::Tls) {
            return std::make_unique<P4TlsSocketStream>();
        }
        return std::make_unique<P4SocketStream>(type);
    }
};

static P4SocketFactory s_p4SocketFactory;

void initSocketBackendP4() {
    network::setSocketFactory(&s_p4SocketFactory);
    ESP_LOGI(TAG_SOCK_P4, "HAL Socket P4 inicializado");
}

} // namespace bsp
} // namespace cbdos
