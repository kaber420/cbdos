#include "cbdos/socket.hpp"
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace cbdos {
namespace bsp {

class S3SocketStream : public network::ISocketStream {
public:
    explicit S3SocketStream(network::SocketType type = network::SocketType::Tcp)
        : m_type(type), m_timeoutMs(5000) {}

    ~S3SocketStream() override {
        close();
    }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000) override {
        close();
        if (host.empty() || port == 0) return false;
        m_timeoutMs = timeoutMs;
        m_client.setTimeout((m_timeoutMs + 999) / 1000);
        return m_client.connect(host.c_str(), port, timeoutMs);
    }

    void close() override {
        if (m_client.connected()) {
            m_client.stop();
        }
    }

    bool isConnected() const override {
        return const_cast<WiFiClient&>(m_client).connected();
    }

    int send(const uint8_t* data, size_t len) override {
        if (!data || len == 0 || !m_client.connected()) return -1;
        size_t written = m_client.write(data, len);
        return (written > 0) ? static_cast<int>(written) : -1;
    }

    int recv(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 3000) override {
        if (!buffer || maxLen == 0) return -1;
        if (!m_client.connected() && !m_client.available()) return 0;

        if (timeoutMs != m_timeoutMs) {
            setTimeout(timeoutMs);
        }

        unsigned long start = millis();
        while (!m_client.available() && m_client.connected()) {
            if (millis() - start >= m_timeoutMs) {
                return 0; // Timeout alcanzado sin datos
            }
            delay(2);
        }

        int bytesAvailable = m_client.available();
        if (bytesAvailable <= 0) {
            if (!m_client.connected()) return 0;
            return 0;
        }

        size_t toRead = static_cast<size_t>(bytesAvailable) < maxLen ? static_cast<size_t>(bytesAvailable) : maxLen;
        int bytesRead = m_client.read(buffer, toRead);
        return bytesRead;
    }

    size_t available() override {
        return static_cast<size_t>(m_client.available());
    }

    void setTimeout(uint32_t timeoutMs) override {
        m_timeoutMs = timeoutMs;
        m_client.setTimeout((timeoutMs + 999) / 1000);
    }

private:
    network::SocketType m_type;
    WiFiClient m_client;
    uint32_t m_timeoutMs;
};

class S3TlsSocketStream : public network::ISocketStream {
public:
    S3TlsSocketStream()
        : m_insecure(false), m_timeoutMs(6000) {}

    ~S3TlsSocketStream() override {
        close();
    }

    void setInsecure(bool allowInsecure) override {
        m_insecure = allowInsecure;
    }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 6000) override {
        close();
        if (host.empty() || port == 0) return false;
        m_timeoutMs = timeoutMs;
        if (m_insecure) {
            m_client.setInsecure();
        }
        m_client.setTimeout((m_timeoutMs + 999) / 1000);
        return m_client.connect(host.c_str(), port, timeoutMs);
    }

    void close() override {
        if (m_client.connected()) {
            m_client.stop();
        }
    }

    bool isConnected() const override {
        return const_cast<WiFiClientSecure&>(m_client).connected();
    }

    int send(const uint8_t* data, size_t len) override {
        if (!data || len == 0 || !m_client.connected()) return -1;
        size_t written = m_client.write(data, len);
        return (written > 0) ? static_cast<int>(written) : -1;
    }

    int recv(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 3000) override {
        if (!buffer || maxLen == 0) return -1;
        if (!m_client.connected() && !m_client.available()) return 0;

        if (timeoutMs != m_timeoutMs) {
            setTimeout(timeoutMs);
        }

        unsigned long start = millis();
        while (!m_client.available() && m_client.connected()) {
            if (millis() - start >= m_timeoutMs) {
                return 0;
            }
            delay(2);
        }

        int bytesAvailable = m_client.available();
        if (bytesAvailable <= 0) {
            return 0;
        }

        size_t toRead = static_cast<size_t>(bytesAvailable) < maxLen ? static_cast<size_t>(bytesAvailable) : maxLen;
        return m_client.read(buffer, toRead);
    }

    size_t available() override {
        return static_cast<size_t>(m_client.available());
    }

    void setTimeout(uint32_t timeoutMs) override {
        m_timeoutMs = timeoutMs;
        m_client.setTimeout((timeoutMs + 999) / 1000);
    }

private:
    WiFiClientSecure m_client;
    bool m_insecure;
    uint32_t m_timeoutMs;
};

class S3SocketFactory : public network::ISocketFactory {
public:
    std::unique_ptr<network::ISocketStream> createSocket(network::SocketType type = network::SocketType::Tcp) override {
        if (type == network::SocketType::Tls) {
            return std::make_unique<S3TlsSocketStream>();
        }
        return std::make_unique<S3SocketStream>(type);
    }
};

static S3SocketFactory s_s3SocketFactory;

void initSocketBackendS3() {
    network::setSocketFactory(&s_s3SocketFactory);
}

} // namespace bsp
} // namespace cbdos
