#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

namespace cbdos {
namespace network {

enum class SocketType {
    Tcp,
    Udp,
    Tls
};

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ Puro para Streams de Sockets de Red
// ────────────────────────────────────────────────────────────────

class ISocketStream {
public:
    virtual ~ISocketStream() = default;

    virtual bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000) = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;

    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual int recv(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 3000) = 0;
    virtual size_t available() = 0;
    virtual void setTimeout(uint32_t timeoutMs) = 0;
    virtual void setInsecure(bool allowInsecure) { (void)allowInsecure; }
};

// ────────────────────────────────────────────────────────────────
// Factoría Abstracta para Creación de Sockets
// ────────────────────────────────────────────────────────────────

class ISocketFactory {
public:
    virtual ~ISocketFactory() = default;
    virtual std::unique_ptr<ISocketStream> createSocket(SocketType type = SocketType::Tcp) = 0;
};

void setSocketFactory(ISocketFactory* factory);
ISocketFactory* getSocketFactory();

// Helper global para instanciación conveniente
std::unique_ptr<ISocketStream> createSocket(SocketType type = SocketType::Tcp);

} // namespace network
} // namespace cbdos
