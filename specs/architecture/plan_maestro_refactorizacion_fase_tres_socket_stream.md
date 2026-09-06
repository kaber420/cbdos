# Plan Maestro de Refactorización - Fase 3: Desacoplamiento de Sockets y Capa de Red (`ISocketStream`)

## 🎯 Objetivo General
Completar la **Fase 3** del roadmap desacoplando las conexiones directas a nivel de socket de red en `core/` (`TlvBrowserView` y `AudioPlayer`), inyectando el contrato `ISocketStream` implementado limpiamente en cada BSP.

---

## 📋 Fases de Ejecución Paso a Paso

### 🔹 Paso 1: Contrato HAL y Despachador Agnóstico en `core/`
1. **Crear [`core/include/cbdos/socket.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/socket.hpp):**
   - Declarar `ISocketStream`, `ISocketFactory`, `setSocketFactory()`, `getSocketFactory()`, `createSocket()`.
2. **Crear [`core/src/network/socket.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/network/socket.cpp):**
   - Despachador de factoría global de sockets.
3. **Actualizar [`core/CMakeLists.txt`](file:///home/kaber420/Documentos/proyectos/cbdos/core/CMakeLists.txt):**
   - Incluir `src/network/socket.cpp`.

---

### 🔹 Paso 2: Implementación en Capa de Soporte de Placa (`bsp/`)
1. **BSP ESP32-P4 ([`bsp/esp32_p4_jc4880/hal/hal_socket_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_socket_p4.cpp)):**
   - Implementar `P4SocketStream` utilizando los sockets BSD de LwIP provistos por ESP-IDF y ESP-Hosted (WiFi 6 / C6).
   - Implementar `P4SocketFactory` y la función de inyección `cbdos::bsp::initSocketBackendP4()`.
   - Registrar la llamada en `bsp/esp32_p4_jc4880/main/main.cpp`.
2. **BSP ESP32-S3 ([`bsp/esp32_s3_jc3248/hal/hal_socket_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_socket_s3.cpp)):**
   - Implementar `S3SocketStream` utilizando `WiFiClient` (o sockets LwIP) del framework Arduino.
   - Implementar `S3SocketFactory` y la función de inyección `cbdos::bsp::initSocketBackendS3()`.
   - Registrar la llamada en `bsp/esp32_s3_jc3248/src/main.cpp`.

---

### 🔹 Paso 3: Refactorización Quirúrgica de Consumidores en `core/`
1. **Refactorizar [`core/src/ui/views/TlvBrowserView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TlvBrowserView.cpp):**
   - Reemplazar `socket()`, `connect()`, `send()`, `recv()`, `close()` por `ISocketStream`.
   - Purgar `#include <sys/socket.h>`, `<netdb.h>`, `<arpa/inet.h>`, `<unistd.h>`.
2. **Refactorizar [`core/src/audio/AudioPlayer.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.cpp):**
   - Reemplazar la gestión de socket HTTP en streaming de radio por `ISocketStream`.
   - Purgar `#include <sys/socket.h>`, `<netdb.h>`, `<arpa/inet.h>`, `<unistd.h>`.

---

### 🔹 Paso 4: Verificación Multi-Target y Pruebas
1. **Compilación simultánea:**
   - ESP32-P4: `idf.py build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
2. **Validación en hardware:**
   - Flashear P4 y verificar streaming de radio online y consultas al Gateway TLV.
