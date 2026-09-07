# Propuesta de Arquitectura: Suite de Diagnóstico de Red, Primitivas HAL y LAN Recon

**Estado:** 💡 Propuesta de Arquitectura & Aplicación  
**Target:** ESP32-P4 (Guition JC4880P443C) y ESP32-S3 (JC3248W535)  
**Dependencias Core:** C++ agnóstico, LVGL 9.5, lwIP / FreeRTOS (vía HAL), Lua 5.4  
**Ubicación Oficial:** `specs/proposals/proposal_network_diagnostics_and_lan_recon_suite.md`  

---

## 1. Visión General y Filosofía de Diseño

En el desarrollo de un Cyberdeck táctico y estación de campo portátil como **CBDos**, el diagnóstico de redes y el reconocimiento de infraestructura local (LAN Recon) constituyen una capacidad fundamental.

Sin embargo, para mantener la integridad de CBDos como un **verdadero Sistema Operativo** y no como un cúmulo de aplicaciones aisladas, esta funcionalidad se concibe bajo el principio de **modularidad y componibilidad estricta**:

> **Principio de Diseño:**  
> Las herramientas de red (ICMP/Ping, ARP Lookup, Port Probing, DNS/mDNS y OUI Vendor Resolution) no deben ser un monolito cerrado dentro de una app, sino **primitivas independientes de la capa del sistema (`cbdos::network`)**. La aplicación gráfica **LAN Recon**, los comandos de la consola **Terminal** y los scripts automatizados de **Lua** son simplemente consumidores de estos servicios.

### Objetivos Principales:
1. **Primitivas Desacopladas de Red:** Proporcionar contratos C++ puros para operaciones fundamentales de red (Ping, ARP, Sondeo de Puertos TCP, mDNS y SSDP).
2. **Reconocimiento LAN Táctico (LAN Recon):**
   * Descubrimiento de hosts activos en subredes locales (/24) en menos de 3 segundos mediante barrido ARP optimizado.
   * Identificación inmediata de fabricantes mediante base de datos MAC OUI en MicroSD (`/sdcard/cbdos/oui.bin`).
   * Perfilado de servicios críticos (Top Ports: SSH 22, Web 80/443, Cámaras RTSP 554, MQTT 1883, SMB 445).
3. **Consola Interactiva (CLI / Terminal):** Comandos estándar tipo UNIX (`ping`, `arp`, `nc -z / scanport`, `ssdp`, `camrecon`) disponibles en `TerminalView` y a través de la consola serial USB-JTAG `/dev/ttyACM0`.
4. **Automatización en Lua (`sys.net.*`):** Capacidad para que auditores y desarrolladores escriban scripts personalizados (ej. centinela de presencia de dispositivos, alertas por sonido de intrusión o exportación JSON a MicroSD).
5. **UI Táctil de Alta Resolución (LVGL 9.5):** Vista rica adaptada a pantallas táctiles de 480x800 y 320x480 con tarjetas interactivas, badges de protocolos y acciones contextuales de un toque.
6. **Inspector de Cámaras IP e IoT (SSDP / UPnP & mDNS-SD):** Descubrimiento especializado de cámaras de seguridad (Hikvision, Dahua, Axis, ONVIF) y dispositivos inteligentes mediante consultas multicast UDP (SSDP 1900 y mDNS 5353), extrayendo automáticamente el modelo, fabricante exacto y endpoints de streaming RTSP/HTTP sin depender únicamente del escaneo de puertos.

---

## 2. Cumplimiento de Reglas Arquitectónicas de CBDos

* **Regla 7 (Offline-First Estricto):** Cero llamadas a interfaces de red en el arranque (`app_main`). Las primitivas de diagnóstico y el LAN Recon operan **exclusivamente bajo demanda**. Si el usuario abre la herramienta sin conectividad previa, la interfaz notifica reactivamente el estado desconectado y ofrece acceso directo a `WiFiConfigView`.
* **Regla 8 (Pureza de `core/` y Cero Platform Pollution):** El código en `core/` es C++ estándar y LVGL 9.5. Queda estrictamente prohibido incluir `<lwip/...>`, `<esp_ping.h>`, `<WiFi.h>` o usar `#ifdef ESP_PLATFORM` en `core/`. Todo acceso a la pila TCP/IP se efectúa mediante interfaces abstractas HAL inyectadas en tiempo de inicialización por los BSPs.
* **Regla 12 (Arquitectura Reactiva y Cero Polling Innecesario):** Las operaciones de sondeo (Ping, ARP Sweep, Port Probe) se ejecutan de manera asíncrona en tareas secundarias de FreeRTOS controladas por el BSP, reportando resultados a la UI o al script mediante callbacks y banderas atómicas (`m_dirty = true`).
* **Regla 2 (Verificación Multi-Target Obligatoria):** Compilación limpia garantizada tanto en ESP-IDF 5.5 (ESP32-P4) como en PlatformIO/Arduino Core (ESP32-S3).

---

## 3. Arquitectura del Sistema y Diagrama de Capas

```
┌────────────────────────────────────────────────────────────────────────┐
│                        CONSUMIDORES / FRONTEND                         │
│   [ App LAN Recon ]      [ Consola Terminal ]     [ Scripts en Lua ]   │
│   (LanReconView LVGL)    (ping, arp, scanport)    (sys.net.ping, etc.) │
└──────────┬───────────────────────┬─────────────────────────┬───────────┘
           │                       │                         │
┌──────────▼───────────────────────▼─────────────────────────▼───────────┐
│              CORE / SERVICIOS DEL SISTEMA (Agnóstico C++)              │
│                                                                        │
│  • NetworkDiagService: Orquestador y despachador asíncrono             │
│  • OuiDatabase: Lookup de fabricantes en MicroSD / RAM caché           │
│  • LuaBridge: Bindings `sys.net.*`                                     │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │ Interfaces Abstractas HAL
┌──────────────────────────────────▼─────────────────────────────────────┐
│                 CAPA HAL DE RED (`core/include/cbdos/`)                │
│                                                                        │
│  • INetworkPingBackend: Contrato para ICMP Echo Request/Reply          │
│  • INetworkArpBackend: Contrato para ARP Lookup y Subnet Sweep         │
│  • INetworkPortProbeBackend: Contrato para TCP Connect Prober          │
│  • INetworkDiscoveryBackend: Contrato Multicast para SSDP y mDNS       │
│  • INetworkDnsBackend: Contrato para Resolución de Nombres y DNS       │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │ Implementaciones Específicas
┌──────────────────────────────────▼─────────────────────────────────────┐
│                     CAPA DE SOPORTE DE HARDWARE (BSP)                  │
│                                                                        │
│  [ BSP ESP32-P4 (JC4880P443C) ]       [ BSP ESP32-S3 (JC3248W535) ]    │
│  • ESP-IDF 5.5 lwIP / esp_ping        • Arduino Core / pioarduino lwIP │
│  • Sockets no bloqueantes BSD          • Sockets lwIP no bloqueantes    │
│  • etharp_request & netif hooks        • lwip_arp_find_addr hooks       │
│  • Sockets UDP Multicast (1900/5353)   • Sockets UDP Multicast (WiFi)   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Contratos de Interfaces HAL (`core/include/cbdos/`)

### 4.1. Estructuras de Datos Universales (`network_types.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace network {

// Resultado de una solicitud ICMP Ping
struct PingResult {
    bool success = false;
    uint32_t rttMs = 0;       // Round Trip Time en milisegundos
    uint8_t ttl = 0;          // Time To Live
    uint16_t seqNumber = 0;
    std::string ip;
};

// Entrada de vecino en la red (ARP / Host Discovery)
struct LanHostEntry {
    std::string ip;
    std::string mac;
    std::string vendor;       // Resuelto mediante OUI Database
    std::string hostname;     // Resuelto mediante mDNS o DNS inverso
    uint32_t responseTimeMs = 0;
    std::vector<uint16_t> openPorts;

    // Metadatos de Inspección de Cámaras IP e IoT (vía SSDP / mDNS)
    bool isCamera = false;
    std::string cameraModel;          // ej. "Hikvision DS-2CD2043G2" o "Dahua IPC-HDBW"
    std::string streamUrl;            // ej. "rtsp://192.168.1.45:554/live" o snapshot JPEG
    std::string webAdminUrl;          // ej. "http://192.168.1.45:80"
    std::string upnpLocation;         // URL descriptor XML (ej. "http://192.168.1.45:80/description.xml")
    std::vector<std::string> services; // Lista de servicios descubiertos (ej. "_rtsp._tcp", "_onvif._tcp")
};

// Resultado de comprobación de puerto TCP
struct PortProbeResult {
    std::string ip;
    uint16_t port;
    bool isOpen = false;
    uint32_t responseTimeMs = 0;
};

// Dispositivo anunciado vía SSDP / UPnP M-SEARCH
struct SsdpDeviceInfo {
    std::string ip;
    std::string location;      // Location URL (ej. http://192.168.1.45:80/upnp.xml)
    std::string server;        // Cadena SERVER (ej. "Linux/3.10 UPnP/1.0 Hikvision-IPCamera")
    std::string usn;           // Unique Service Name
    std::string st;            // Search Target
    std::string friendlyName;  // Extraído del descriptor o cabecera
    bool isCamera = false;
};

// Registro de servicio descubierto vía Multicast DNS (mDNS)
struct MdnsServiceRecord {
    std::string serviceName;   // ej. "Camara-Patio"
    std::string serviceType;   // ej. "_rtsp._tcp", "_http._tcp", "_onvif._tcp"
    std::string ip;
    uint16_t port = 0;
    std::string hostTarget;    // ej. "hikvision-cam.local"
    std::vector<std::string> txtRecords; // Pares clave=valor del registro TXT
};

} // namespace network
} // namespace cbdos
```

---

### 4.2. Interfaz HAL de Ping / ICMP (`INetworkPingBackend.hpp`)

```cpp
#pragma once
#include "network_types.hpp"

namespace cbdos {
namespace network {

class INetworkPingBackend {
public:
    virtual ~INetworkPingBackend() = default;

    /// @brief Realiza un ping síncrono bloqueante con timeout
    virtual PingResult pingOnce(const std::string& target, uint32_t timeoutMs = 2000) = 0;

    /// @brief Inicia una ráfaga de pings asíncronos en segundo plano
    virtual bool startPingStream(const std::string& target, 
                                 uint32_t count, 
                                 uint32_t intervalMs, 
                                 std::function<void(const PingResult&)> onResult,
                                 std::function<void(uint32_t sent, uint32_t received, uint32_t minMs, uint32_t avgMs, uint32_t maxMs)> onComplete) = 0;

    /// @brief Detiene cualquier ráfaga de pings activa
    virtual void stopPingStream() = 0;
};

void setNetworkPingBackend(INetworkPingBackend* backend);
INetworkPingBackend* getNetworkPingBackend();

} // namespace network
} // namespace cbdos
```

---

### 4.3. Interfaz HAL de ARP y Barrido de Subred (`INetworkArpBackend.hpp`)

```cpp
#pragma once
#include "network_types.hpp"

namespace cbdos {
namespace network {

class INetworkArpBackend {
public:
    virtual ~INetworkArpBackend() = default;

    /// @brief Consulta la tabla ARP local del sistema operativo sin generar tráfico
    virtual bool lookupArpTable(const std::string& ip, std::string& outMac) = 0;

    /// @brief Envía una solicitud ARP activa y espera respuesta (resolución unitaria)
    virtual bool queryArp(const std::string& ip, std::string& outMac, uint32_t timeoutMs = 500) = 0;

    /// @brief Inicia un barrido de subred ARP asíncrono
    /// @param baseSubnet Subred en formato "192.168.1." o similar
    /// @param startHost Primer octeto a probar (ej. 1)
    /// @param endHost Último octeto a probar (ej. 254)
    /// @param onHostDiscovered Callback invocado cada vez que un dispositivo responde
    /// @param onProgress Callback periódico de avance (porcentaje 0-100)
    /// @param onFinished Callback de finalización
    virtual bool startArpSweep(const std::string& baseSubnet,
                               uint8_t startHost,
                               uint8_t endHost,
                               std::function<void(const LanHostEntry&)> onHostDiscovered,
                               std::function<void(uint8_t progressPercent)> onProgress,
                               std::function<void(uint32_t totalHostsFound)> onFinished) = 0;

    /// @brief Cancela el barrido en curso
    virtual void cancelSweep() = 0;

    /// @brief Consulta si hay un barrido en progreso
    virtual bool isSweeping() const = 0;
};

void setNetworkArpBackend(INetworkArpBackend* backend);
INetworkArpBackend* getNetworkArpBackend();

} // namespace network
} // namespace cbdos
```

---

### 4.4. Interfaz HAL de Sondeo de Puertos TCP (`INetworkPortProbeBackend.hpp`)

```cpp
#pragma once
#include "network_types.hpp"

namespace cbdos {
namespace network {

class INetworkPortProbeBackend {
public:
    virtual ~INetworkPortProbeBackend() = default;

    /// @brief Comprueba de forma síncrona si un puerto TCP está abierto mediante TCP-Connect no bloqueante
    virtual PortProbeResult probePort(const std::string& ip, uint16_t port, uint32_t timeoutMs = 800) = 0;

    /// @brief Sondea una lista de puertos sobre un host de manera asíncrona
    virtual bool scanPortsAsync(const std::string& ip,
                                const std::vector<uint16_t>& ports,
                                uint32_t timeoutMs,
                                std::function<void(const PortProbeResult&)> onPortChecked,
                                std::function<void()> onFinished) = 0;

    virtual void cancel() = 0;
};

void setNetworkPortProbeBackend(INetworkPortProbeBackend* backend);
INetworkPortProbeBackend* getNetworkPortProbeBackend();

} // namespace network
} // namespace cbdos
```

---

### 4.5. Interfaz HAL de Descubrimiento Multicast SSDP y mDNS (`INetworkDiscoveryBackend.hpp`)

```cpp
#pragma once
#include "network_types.hpp"

namespace cbdos {
namespace network {

class INetworkDiscoveryBackend {
public:
    virtual ~INetworkDiscoveryBackend() = default;

    /// @brief Envía paquete M-SEARCH SSDP (UDP 239.255.255.250:1900) para descubrir dispositivos UPnP y cámaras
    /// @param searchTarget Objetivo de búsqueda (ej. "ssdp:all", "urn:schemas-upnp-org:device:MediaServer:1")
    /// @param mxSeconds Ventana de tiempo en segundos para esperar respuestas escalonadas (típico: 2 o 3)
    /// @param onDeviceDiscovered Callback ejecutado al recibir y parsear cada respuesta HTTPU (NOTIFY / 200 OK)
    /// @param onFinished Callback de finalización del periodo de escucha
    virtual bool startSsdpScan(const std::string& searchTarget,
                               uint8_t mxSeconds,
                               std::function<void(const SsdpDeviceInfo&)> onDeviceDiscovered,
                               std::function<void(uint32_t totalFound)> onFinished) = 0;

    /// @brief Realiza una consulta DNS Multicast (UDP 224.0.0.251:5353) para enumerar servicios mDNS-SD
    /// @param serviceType Tipo de servicio DNS-SD (ej. "_rtsp._tcp.local", "_http._tcp.local", "_onvif._tcp.local")
    /// @param timeoutMs Tiempo máximo de escucha en milisegundos
    /// @param onServiceFound Callback ejecutado con cada registro PTR/SRV/TXT resuelto
    /// @param onFinished Callback al concluir el timeout
    virtual bool browseMdns(const std::string& serviceType,
                            uint32_t timeoutMs,
                            std::function<void(const MdnsServiceRecord&)> onServiceFound,
                            std::function<void(uint32_t totalFound)> onFinished) = 0;

    /// @brief Detiene cualquier escucha multicast activa en segundo plano
    virtual void cancelDiscovery() = 0;

    /// @brief Indica si hay un proceso de descubrimiento multicast en curso
    virtual bool isDiscovering() const = 0;
};

void setNetworkDiscoveryBackend(INetworkDiscoveryBackend* backend);
INetworkDiscoveryBackend* getNetworkDiscoveryBackend();

} // namespace network
} // namespace cbdos
```

---

## 5. Servicio de Base de Datos OUI (Identificación de Fabricantes)

Para identificar marcas de dispositivos a partir de su MAC (primeros 3 bytes / 24 bits):

* **Almacenamiento Offline en MicroSD:**  
  Archivo binario compacto `/sdcard/system/oui.bin` o archivo de texto indexado `/sdcard/system/oui.txt`.
* **Estructura Binaria Compacta (`oui.bin`):**  
  Cada registro ocupa exactamente 24 bytes:
  * 3 bytes: Prefijo OUI (ej. `0xDC, 0xA6, 0x32` = Raspberry Pi Trading).
  * 21 bytes: Nombre truncado ASCII con relleno nulo (ej. `"Raspberry Pi Ltd"`).
* **Búsqueda Eficiente:**
  Búsqueda binaria `O(log N)` directamente sobre el archivo en MicroSD sin cargar megabytes a la memoria RAM.
* **Caché en PSRAM:**
  Caché MRU (Most Recently Used) de los últimos 64 fabricantes consultados para resolución instantánea durante el barrido ARP.

---

## 6. Comandos para la Consola Terminal (`TerminalView` / Serial CLI)

Las primitivas quedan enlazadas al procesador de comandos de la consola del sistema:

| Comando | Sintaxis | Descripción |
| :--- | :--- | :--- |
| **`ping`** | `ping <ip\|hostname> [-c count] [-t timeout]` | Envía paquetes ICMP Echo Request y calcula RTT (mín, prom, máx). |
| **`arp`** | `arp [-a]` | Muestra la tabla de direcciones IP/MAC y fabricantes conocidos. |
| **`scanport`** / **`nc`** | `scanport <ip> <port>` o `nc -z <ip> <port>` | Comprueba si un puerto TCP específico está escuchando. |
| **`ssdp`** | `ssdp [all\|cam]` | Envía consulta M-SEARCH y lista dispositivos UPnP/IoT anunciados con Server/Location. |
| **`mdns`** | `mdns [service_type]` | Descubre y resuelve servicios mDNS-SD (ej. `_rtsp._tcp`, `_http._tcp`). |
| **`camrecon`** | `camrecon` | Barrido especializado en cámaras IP combinando RTSP 554, ONVIF, SSDP y mDNS. |
| **`lanrecon`** | `lanrecon [sweep\|top]` | Ejecuta un barrido rápido por terminal imprimiendo una tabla en formato ASCII de los hosts encontrados. |

---

## 7. Integración con Scripts en Lua (`sys.net.*`)

En el archivo `core/src/lua/LuaBridge.cpp`, se registran las siguientes funciones dentro del espacio de nombres `sys.net`:

```lua
-- ==========================================================
-- Ejemplo 1: Diagnóstico de Conectividad Simple
-- ==========================================================
local rtt = sys.net.ping("1.1.1.1", 1500)
if rtt >= 0 then
    print("Internet OK - Latencia: " .. rtt .. " ms")
else
    print("Sin salida a Internet")
end

-- ==========================================================
-- Ejemplo 2: Centinela de Red (Alerta de Intrusos)
-- ==========================================================
print("Escaneando red local...")
sys.net.arp_sweep("192.168.1.", 1, 254, function(host)
    print(string.format("Host: %s | MAC: %s | Fabricante: %s", 
          host.ip, host.mac, host.vendor))
    
    -- Si detecta una cámara RTSP abierta, emitir un tono de audio
    if sys.net.probe_port(host.ip, 554, 300) then
        print("⚠️ Cámara de seguridad encontrada en: " .. host.ip)
        sys.beep(880, 200)
    end
end)

-- ==========================================================
-- Ejemplo 3: Inspector Especializado de Cámaras (mDNS / SSDP)
-- ==========================================================
print("Buscando cámaras IP vía SSDP y mDNS...")
sys.net.discover_cameras(function(cam)
    print(string.format("📹 Cámara detectada: %s (%s) en %s", 
          cam.model, cam.vendor, cam.ip))
    if cam.stream_url ~= "" then
        print("   Stream RTSP/HTTP: " .. cam.stream_url)
    end
    if cam.web_url ~= "" then
        print("   Panel de Control Web: " .. cam.web_url)
    end
end)
```

---

## 8. Diseño de la Aplicación Gráfica Táctil (`LanReconView`)

### 8.1. Layout en Pantalla (LVGL 9.5)

```
┌──────────────────────────────────────────────────────────────────┐
│ [ < Utilidades ]          LAN RECON              [ ⚙️ Opciones ] │  <- HeaderBar
├──────────────────────────────────────────────────────────────────┤
│ 🌐 Red: CyberLab_5G   IP: 192.168.1.142   GW: 192.168.1.1        │  <- Tarjeta de Estado
│ Subred: 192.168.1.0/24  |  Hosts: 7  |  📹 Cámaras: 2           │
├──────────────────────────────────────────────────────────────────┤
│ [ ▶ Iniciar Escaneo ]   [ ⏹ Detener ]   [ 💾 Exportar Reporte ]   │  <- Botonera Táctil
│ Filtro: [ ● Todos (7) ]  [ 📹 Cámaras & IoT (2) ]  [ 💻 SSH (1) ] │  <- Filtro Táctil
│ ▰▰▰▰▰▰▰▰▰▰▰▰▰▰▱▱▱▱▱▱▱▱▱▱▱▱▱▱ 58% (148/254 IPs)            │  <- Barra Progreso
├──────────────────────────────────────────────────────────────────┤
│ LISTA DE DISPOSITIVOS DETECTADOS                                 │
│ ┌──────────────────────────────────────────────────────────────┐ │
│ │ 🖥️ 192.168.1.1 (Gateway / Router)                           │ │
│ │ MAC: DC:02:8E:44:11:00  •  Fabricante: TP-Link Technologies  │ │
│ │ Puertos: [ 80 HTTP ] [ 443 HTTPS ] [ 53 DNS ]               │ │
│ │ Acciones: [ Ping ] [ Probar Puertos ]                        │ │
│ └──────────────────────────────────────────────────────────────┘ │
│ ┌──────────────────────────────────────────────────────────────┐ │
│ │ 📹 192.168.1.45 (Cámara IP - Hikvision DS-2CD2043G2)         │ │
│ │ MAC: 44:19:B6:12:34:56  •  Fabricante: Hikvision Digital      │ │
│ │ Servicios: [ SSDP UPnP ] [ mDNS _rtsp._tcp ] [ ONVIF: 8000 ] │ │
│ │ Stream: rtsp://192.168.1.45:554/Streaming/Channels/101       │ │
│ │ Acciones: [ Ping ] [ 📹 Inspeccionar Cámara ] [ 🌐 Web (80) ] │ │
│ └──────────────────────────────────────────────────────────────┘ │
│ ┌──────────────────────────────────────────────────────────────┐ │
│ │ 💻 192.168.1.120 (Servidor de Campo)                         │ │
│ │ MAC: B8:27:EB:AA:BB:CC  •  Fabricante: Raspberry Pi Found.   │ │
│ │ Puertos: [ 22 SSH ]                                          │ │
│ │ Acciones: [ Ping ] [ 🚀 Conectar SSH ]                       │ │  <- Salto a App SSH
│ └──────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

### 8.2. Características Clave de la UI:
* **Colores y Estilos:** Integración nativa con `DefaultTheme` y la paleta Dark/Cyan de CBDos.
* **Perfiles de Escaneo Seleccionables:**
  * *Fast Discovery (ARP Only):* ~2 segundos para listar todas las IPs y MACs.
  * *Standard Audit (ARP + Top 10 Ports):* Identifica IPs y verifica puertos 21, 22, 23, 80, 443, 445, 554, 1883, 3389, 8080.
  * *Camera & IoT Hunt:* Dispara ráfagas multicast SSDP (M-SEARCH) y mDNS queries (`_rtsp._tcp`, `_onvif._tcp`) en paralelo con escaneo de puertos de streaming (554, 8000, 8899).
  * *Custom Range:* Permite definir rangos arbitrarios de IPs o puertos específicos.
* **Filtros Táctiles Rápidos:** Botonera segmentada para alternar entre "Todos", "Cámaras & IoT" y "Servidores / SSH".
* **Inspector Contextual de Cámaras IP:** Al pulsar `[ 📹 Inspeccionar Cámara ]`, se abre un modal flotante con la ficha técnica completa del dispositivo: modelo extraído de las cabeceras SSDP/mDNS, fabricante exacto, URL de descriptor XML, compatibilidad ONVIF detectada y URI completa para streaming RTSP o snapshot JPEG.
* **Integración Directa con la App SSH:** Si un host tiene el puerto 22 abierto, la tarjeta muestra el botón destacado `[ 🚀 Conectar SSH ]`, abriendo la vista `SshClientView` con la IP precompletada.
* **Exportación de Auditoría:** Guarda un informe estructurado JSON y texto plano en `/sdcard/reports/lan_recon_YYYYMMDD_HHMMSS.json`.

---

## 9. Plan de Implementación por Fases

### Fase 1: Interfaces HAL y Primitivas Core (Agnóstico)
1. Crear las cabeceras de contratos en `core/include/cbdos/`:
   * `network_types.hpp` (estructuras para ping, host, puertos, SSDP y mDNS)
   * `INetworkPingBackend.hpp`
   * `INetworkArpBackend.hpp`
   * `INetworkPortProbeBackend.hpp`
   * `INetworkDiscoveryBackend.hpp` (SSDP M-SEARCH y mDNS browsing)
2. Implementar el despachador de red y el servicio de base de datos OUI (`OuiDatabase.hpp/.cpp`) en `core/src/network/`.

### Fase 2: Implementación de Backends HAL en los BSPs
1. **ESP32-P4 (`bsp/esp32_p4_jc4880`):**
   * Implementación de `PingBackendP4.cpp` usando `esp_ping`.
   * Implementación de `ArpBackendP4.cpp` usando hooks de `etharp` de ESP-IDF lwIP.
   * Implementación de `PortProbeBackendP4.cpp` usando sockets BSD no bloqueantes.
   * Implementación de `DiscoveryBackendP4.cpp` mediante sockets UDP multicast (239.255.255.250:1900 para SSDP y 224.0.0.251:5353 para mDNS).
2. **ESP32-S3 (`bsp/esp32_s3_jc3248`):**
   * Implementaciones equivalentes sobre el stack de red de Arduino Core / lwIP (`WiFiUdp` / sockets lwIP).

### Fase 3: Integración en CLI y LuaBridge
1. Añadir comandos de consola `ping`, `arp`, `scanport`, `ssdp`, `mdns` y `camrecon` en `core/src/terminal/` y en el Serial Debug CLI.
2. Registrar las funciones en `core/src/lua/LuaBridge.cpp` bajo `sys.net.*` (incluyendo `sys.net.discover_cameras`).

### Fase 4: Aplicación Gráfica Táctil (`LanReconView`)
1. Crear `LanReconView.hpp` y `LanReconView.cpp` en `core/src/ui/views/utilities/`.
2. Implementar el filtrado táctil por categorías (Todos, Cámaras & IoT, Servidores).
3. Añadir el modal interactivo de inspección de cámaras con detalles RTSP/ONVIF.
4. Registrar la vista en `UIManager` y añadir el icono correspondiente en el menú de Utilidades.
5. Enlazar la acción del botón `[ 🚀 Conectar SSH ]` para integración fluida con la propuesta de cliente SSH.

### Fase 5: Pruebas y Validación Multi-Target
1. Validación de compilación en `idf.py build` (P4) y `pio run` (S3).
2. Verificación de tiempos de respuesta en laboratorio conectando el Cyberdeck a routers, cámaras IP (Hikvision/Dahua/RTSP) y dispositivos IoT de prueba.
