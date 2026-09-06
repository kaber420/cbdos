# Especificación Técnica - Fase 3: Capa de Red, Sockets y Streaming Transparente (`ISocketStream`)

## 📌 1. Visión General y Objetivos Arquitectónicos

La **Fase 3** del plan maestro de desacoplamiento de **CBDos v0.2.1** tiene como propósito erradicar la última fuente de acoplamiento de red y llamadas directas a APIs de sockets POSIX/BSD en el núcleo agnóstico `core/`.

### 🎯 Objetivos Principales:
1. **Erradicación de Headers de Plataforma en `core/`:** Eliminar cualquier inclusión de `<sys/socket.h>`, `<netdb.h>`, `<arpa/inet.h>`, `<netinet/in.h>` y `<unistd.h>` de los módulos de interfaz de usuario y audio.
2. **Definición del Contrato Abstracto `ISocketStream`:** Establecer una interfaz orientada a flujos de bytes bidireccionales (TCP y UDP) que abstraiga la resolución DNS, timeouts de conexión, buffers de recepción y envío.
3. **Soporte Transparente Multi-Target:**
   - **ESP32-P4:** Backend sobre el stack LwIP de ESP-IDF conectado al bus SDIO de ESP-Hosted (WiFi 6 con ESP32-C6).
   - **ESP32-S3:** Backend sobre `WiFiClient` / LwIP de Arduino Core.
   - **Linux / Simulador:** Backend nativo sobre sockets POSIX estándar de Linux.
4. **Migración Quirúrgica de Clientes de Red en `core/`:**
   - **`TlvBrowserView`:** Navegador TLV ligero que consulta el Gateway HTTP/TLS.
   - **`AudioPlayer`:** Motor de streaming de Radio Online Icecast / Shoutcast con soporte de redirecciones HTTP (301/302/307).

---

## 🏛️ 2. Diagrama de Arquitectura de Red Desacoplada

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CBDOS CORE (C++ Puro)                          │
├──────────────────────────────────────┬──────────────────────────────────────┤
│  Vistas de Red (TlvBrowserView)      │  Streaming Multimedia (AudioPlayer)  │
│  - Navegación web binaria TLV        │  - Streaming Radio Online HTTP       │
│  - Interacción con Gateway           │  - Decodificación Helix MP3         │
└──────────────────────────────────────┴──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CONTRATO HAL: cbdos::network::ISocketStream              │
│                                                                             │
│  • virtual bool connect(host, port, timeoutMs) = 0;                         │
│  • virtual void close() = 0;                                                │
│  • virtual bool isConnected() const = 0;                                    │
│  • virtual int send(const uint8_t* data, size_t len) = 0;                   │
│  • virtual int recv(uint8_t* buf, size_t maxLen, uint32_t timeoutMs) = 0;  │
│  • virtual size_t available() = 0;                                          │
│  • virtual void setTimeout(uint32_t timeoutMs) = 0;                         │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼                                     ▼
┌──────────────────────────────────────┐┌─────────────────────────────────────┐
│     bsp/esp32_p4_jc4880 (ESP-IDF)    ││     bsp/esp32_s3_jc3248 (Arduino)   │
├──────────────────────────────────────┤├─────────────────────────────────────┤
│  P4SocketStream & P4SocketFactory    ││  S3SocketStream & S3SocketFactory   │
│  - LwIP BSD Sockets                  ││  - WiFiClient de Arduino Core       │
│  - ESP-Hosted SDIO Link (WiFi 6)     ││  - WiFi integrado ESP32-S3          │
└──────────────────────────────────────┘└─────────────────────────────────────┘
```

---

## 📜 3. Contratos de Interfaz C++ Puros (`core/include/cbdos/socket.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

namespace cbdos {
namespace network {

enum class SocketType {
    Tcp,
    Udp
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
```

---

## 🔍 4. Análisis Detallado de Migración por Componente

### 4.1. `TlvBrowserView` (`core/src/ui/views/TlvBrowserView.cpp`)
- **Estado Actual:** Crea un socket BSD directo (`socket(AF_INET, SOCK_STREAM, 0)`), realiza resolución DNS con `gethostbyname()` o `inet_addr()`, y utiliza `send()` y `recv()`.
- **Estado Desacoplado:**
  ```cpp
  auto client = cbdos::network::createSocket(cbdos::network::SocketType::Tcp);
  if (!client || !client->connect(p->host, p->port, 4000)) {
      CBD_LOG_E(TAG, "No se pudo conectar al Gateway %s:%u", p->host.c_str(), p->port);
      // Notificar error en UI
      return;
  }
  client->send(packet, total_tx);
  
  std::vector<uint8_t> response_buf;
  uint8_t rx_chunk[1024];
  int r;
  while ((r = client->recv(rx_chunk, sizeof(rx_chunk), 3000)) > 0) {
      response_buf.insert(response_buf.end(), rx_chunk, rx_chunk + r);
      if (response_buf.size() > 65536) break;
  }
  client->close();
  ```

### 4.2. `AudioPlayer` (`core/src/audio/AudioPlayer.cpp` - Streaming de Radio)
- **Estado Actual:** Resuelve con `getaddrinfo()`, abre socket con timeout `SO_RCVTIMEO` de 6 segundos, gestiona redirecciones HTTP analizando `Location:` y lee trozos PCM/MP3 con `recv()`.
- **Estado Desacoplado:**
  ```cpp
  auto client = cbdos::network::createSocket(cbdos::network::SocketType::Tcp);
  if (!client || !client->connect(host, (uint16_t)std::stoi(port), 6000)) {
      CBD_LOG_E(TAG, "[Stream] Fallo de conexion a %s:%s", host.c_str(), port.c_str());
      // Reintentar o fallar
  }
  client->send((const uint8_t*)req, reqLen);
  
  // Lectura de cabeceras y bucle de streaming decodificado con Helix:
  int nRead = client->recv(inBuf + bytesLeft, toRead, 5000);
  ```

---

## 🚀 5. Beneficios Técnicos y Garantías

1. **Cero Dependencia de Headers POSIX / LwIP en Core:** La capa de UI y multimedia queda 100% aislada de APIs del sistema operativo de bajo nivel.
2. **Soporte Nativo para TLS / HTTPS Futuro:** Al abstraer `ISocketStream`, en el futuro se podrá añadir un `TlsSocketStream` (basado en mbedTLS o BearSSL) simplemente implementando el contrato `ISocketStream` sin tocar una sola línea de `TlvBrowserView` ni `AudioPlayer`.
3. **Simulador de Escritorio (Linux/macOS):** Permite ejecutar `TlvBrowserView` y Radio Online directamente en Linux usando sockets estándar del kernel sin recompilar el core.
