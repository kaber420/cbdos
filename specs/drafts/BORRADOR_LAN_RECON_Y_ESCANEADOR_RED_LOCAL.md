# Especificación Técnica y Borrador de Arquitectura: LAN Recon & Device Scanner

**Documento:** `docs/drafts/BORRADOR_LAN_RECON_Y_ESCANEADOR_RED_LOCAL.md`  
**Estado:** Borrador de Arquitectura y Especificación de Integración  
**Módulo:** `cbdos::network::lan_recon`  
**Targets Soportados:** 
- **ESP32-P4** (Guition JC4880P443C - ESP-IDF 5.5 nativo, 480x800 MIPI-DPI)
- **ESP32-S3** (JC3248W535 - PlatformIO / Arduino Core, 320x480 QSPI)  
**UI:** LVGL 9.5 (Agnóstico, Touch-First)  
**Origen:** Portado y mejorado desde *Poseidon Pentesting Firmware* (`poseidon/src/features/net_lanrecon.cpp`)

---

## 📌 1. Visión General y Objetivos en Campo

El módulo **LAN Recon & Device Scanner** convierte a **CBDos** en una herramienta portátil de reconocimiento, inventario y auditoría de redes locales. Diseñada para administradores de sistemas, especialistas en seguridad y técnicos de telecomunicaciones, permite auditar de forma inmediata cualquier red Wi-Fi o Ethernet local a la que el Cyberdeck se conecte.

### Capacidades Principales:
1. **Barrido de Subred Asíncrono (ARP / ICMP Sweep):**
   * Descubre hosts activos en el segmento local (`/24`) en segundos sin degradar la tasa de refresco (60 FPS) de la pantalla táctil.
2. **Resolución de Fabricantes por MAC (OUI Lookup Flash Engine):**
   * Identifica marcas y tipos de equipos (Raspberry Pi, MikroTik, Ubiquiti, Espressif, Apple, Intel, Cisco, cámaras IP Dahua/Hikvision) mediante una base de datos estática en memoria Flash con búsqueda binaria $O(\log N)$.
3. **Escaneo Rápido de Servicios y Puertos Críticos:**
   * Sondeo de puertos clave:
     * `22` (SSH - Administración remota)
     * `23` (Telnet - Dispositivos legacy/routers)
     * `80` / `443` (HTTP/HTTPS - Paneles web)
     * `554` (RTSP - Transmisión de vídeo de cámaras IP / CCTV)
     * `3389` (RDP - Escritorio remoto Windows)
     * `8080` / `8443` (HTTP/HTTPS alternativos)
     * `1900` (SSDP / UPnP)
4. **Captura de Banners y Títulos Web (Banner Grabbing):**
   * Extracción del contenido de la etiqueta `<title>` en servidores web y obtención de la cadena de versión de servicios SSH/Telnet (`SSH-2.0-OpenSSH...`).
5. **Sinergia Táctil Directa con el Cliente SSH de CBDos:**
   * Al seleccionar en la lista cualquier dispositivo que tenga el puerto 22 abierto, la interfaz ofrece un botón directo **"Conectar SSH"**, pre-cargando la dirección IP en el cliente SSH sin necesidad de introducirla manualmente.
6. **Exportación Forense a MicroSD:**
   * Almacenamiento opcional del inventario en formato CSV estandarizado en `/sdcard/recon/lan_<timestamp>.csv`.

---

## 🏛️ 2. Arquitectura de Software y Pureza de `core/`

En estricto cumplimiento con la **Regla 8 (Ley de Pureza Arquitectónica de `core/`)**, la separación entre la lógica de negocio y las llamadas al hardware/SDK se organiza mediante el patrón de interfaces abstractas HAL:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                               CAPA UI: LVGL 9.5 (core/)                                │
│  LanReconView (Hereda de BaseView)                                                     │
│  - Renderizado táctil reactivo a 60 FPS en Core 1                                      │
│  - Tarjetas de dispositivos con badges de puertos ([22 SSH], [80 HTTP], [554 CCTV])    │
│  - Acceso directo "Conectar SSH" -> invoca UIManager::pushView(SshClientView)          │
└───────────────────────────────────────────┬────────────────────────────────────────────┘
                                            │ Callbacks de progreso y nuevos hosts
                                            ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        SERVICIO DE NEGOCIO: LanScannerService (core/)                  │
│  - Orquestador de fases: Idle -> ArpSweep -> PortScan -> BannerGrab -> Done            │
│  - Consulta a OuiDatabase (Tabla estática Flash en core/src/network/OuiDatabase.cpp)   │
│  - Almacena el vector de LanHostInfo y genera reportes CSV                             │
└───────────────────────────────────────────┬────────────────────────────────────────────┘
                                            │ Contrato Abstracto C++ Puro
                                            ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        INTERFAZ HAL: ILanScannerBackend (core/)                        │
│  - uint32_t getSubnetMask() / getLocalIp()                                             │
│  - bool resolveMacArp(uint32_t ip, uint8_t mac[6])                                     │
│  - bool testTcpPort(uint32_t ip, uint16_t port, uint32_t timeoutMs)                    │
│  - std::string grabTcpBanner(uint32_t ip, uint16_t port, uint32_t timeoutMs)           │
│  - std::string fetchHttpTitle(uint32_t ip, uint16_t port, uint32_t timeoutMs)          │
└───────────────────────────────────────────┬────────────────────────────────────────────┘
                                            │
                     ┌──────────────────────┴──────────────────────┐
                     ▼                                             ▼
┌───────────────────────────────────────────┐ ┌───────────────────────────────────────────┐
│     BSP ESP32-S3 (PlatformIO / Arduino)   │ │      BSP ESP32-P4 (ESP-IDF 5.5 Nativo)    │
│     bsp/esp32_s3_jc3248/hal/              │ │      bsp/esp32_p4_jc4880/hal/             │
│     hal_lan_recon_s3.cpp                  │ │      hal_lan_recon_p4.cpp                 │
│  - LwIP etharp_find_addr()                │ │  - LwIP etharp_find_addr()                │
│  - Sockets BSD no bloqueantes (select)    │ │  - Sockets BSD no bloqueantes (select)    │
│  - FreeRTOS Worker anclado en Core 0      │ │  - FreeRTOS Worker anclado en Core 0      │
└───────────────────────────────────────────┘ └───────────────────────────────────────────┘
```

---

## ⚙️ 3. Ejecución Concurrente en Core 0 de la CPU (Aislamiento FreeRTOS)

Para garantizar que la pantalla táctil de CBDos nunca sufra caídas de cuadros por segundo ni congelamientos:

* **Core 1 (Hilo Principal UI):** Ejecuta el bucle de eventos de LVGL 9.5 (`lv_timer_handler()`) y la lectura táctil del controlador Goodix GT911.
* **Core 0 (Tarea de Fondo `lan_recon_task`):** La tarea del escáner se lanza con prioridad baja/media (ej. prioridad 3) y stack en PSRAM/SRAM:
  ```cpp
  // Anclado explícito a Core 0 para no colisionar con la UI
  cbdos::rtos::createTask(lanScannerWorker, "lan_recon_worker", 8192, this, 3, 0);
  ```
* **Comunicación Reactiva y Segura (Regla 12):**
  * El worker en Core 0 actualiza una cola de eventos o estado compartido protegido por mutex (`cbdos::rtos::MutexHandle`).
  * La vista en LVGL marca su bandera interna `m_dirty = true` mediante un callback seguro para refrescar la lista de tarjetas en el siguiente ciclo sin sondeo (*zero polling*).

---

## 🗄️ 4. Estructuras de Datos y Contratos de Código

### `core/include/cbdos/lan_recon.hpp`

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace network {

enum class LanScanPhase {
    Idle,
    ArpSweep,
    PortScan,
    BannerGrab,
    Completed,
    Aborted
};

struct LanHostInfo {
    std::string ip;
    uint8_t mac[6]{0};
    std::string vendor;
    std::string banner;
    uint32_t openPortsBitmask{0};
    bool alive{false};

    bool hasPort(uint16_t port) const;
};

struct LanScanProgress {
    LanScanPhase phase{LanScanPhase::Idle};
    uint8_t percentage{0};
    uint16_t hostsDiscovered{0};
    uint16_t currentHostIndex{0};
};

using OnHostFoundCallback = std::function<void(const LanHostInfo& host)>;
using OnProgressCallback = std::function<void(const LanScanProgress& progress)>;
using OnScanFinishedCallback = std::function<void(const std::vector<LanHostInfo>& results)>;

class ILanScannerBackend {
public:
    virtual ~ILanScannerBackend() = default;

    virtual bool isNetworkConnected() const = 0;
    virtual std::string getLocalIp() const = 0;
    virtual std::string getSubnetMask() const = 0;

    virtual bool pingHost(const std::string& ip, uint32_t timeoutMs = 150) = 0;
    virtual bool resolveMacArp(const std::string& ip, uint8_t mac[6]) = 0;
    virtual bool probeTcpPort(const std::string& ip, uint16_t port, uint32_t timeoutMs = 250) = 0;
    virtual std::string grabTcpBanner(const std::string& ip, uint16_t port, uint32_t timeoutMs = 500) = 0;
    virtual std::string fetchHttpTitle(const std::string& ip, uint16_t port, uint32_t timeoutMs = 800) = 0;
};

void setLanScannerBackend(ILanScannerBackend* backend);
ILanScannerBackend* getLanScannerBackend();

} // namespace network
} // namespace cbdos
```

---

## 📱 5. Diseño de la Interfaz Táctil (LVGL 9.5)

### Componentes de `LanReconView`:
1. **HeaderBar de Estado:**
   * Muestra la IP local del Cyberdeck, la subred asignada (ej. `192.168.1.0/24`) y el número de hosts descubiertos.
   * Botón de acción principal: `[ Iniciar Escaneo ]` / `[ Detener ]`.
2. **Barra de Progreso y Fase:**
   * Widget `lv_bar` de estilo sobrio que indica el avance porcentual y una etiqueta con la fase en curso (`Barrido ARP...`, `Escaneando puertos...`, `Capturando títulos...`).
3. **Lista de Tarjetas Táctiles (`lv_list` / Contenedor Flex):**
   * Cada fila/tarjeta representa un dispositivo encontrado:
     * **Icono:** Router (si coincide con Gateway), Servidor (si tiene puerto 22/80), Cámara (si tiene puerto 554 RTSP), o Genérico.
     * **Texto Principal:** Dirección IP (`192.168.1.15`) en negrita.
     * **Subtítulo:** Fabricante OUI (`Raspberry Pi Foundation`) y MAC (`B8:27:EB:...`).
     * **Badges de Puertos:** Chips de colores con los puertos detectados (`22 SSH`, `80 HTTP`, `443 HTTPS`, `554 RTSP`).
     * **Acción Rápida SSH:** Si el host expone el puerto 22, aparece un botón interactivo táctil:
       ```
       [ >_ Conectar SSH ]
       ```
       Al ser pulsado, llama directamente a la navegación de CBDos:
       ```cpp
       UIManager::getInstance().pushView(std::make_shared<TerminalView>(host.ip));
       ```
4. **Acción de Exportación CSV:**
   * Botón en la barra inferior para guardar el informe en la tarjeta MicroSD bajo el directorio `/sdcard/recon/`.

---

## 🗓️ 6. Plan de Trabajo Paso a Paso

1. **Paso 1: Contratos e Interfaces en `core/`**
   * Crear `core/include/cbdos/lan_recon.hpp`.
   * Crear `core/src/network/OuiDatabase.hpp` y `core/src/network/OuiDatabase.cpp` con la tabla estática optimizada de fabricantes.
2. **Paso 2: Implementación de la Capa de Negocio (`core/src/network/`)**
   * Crear `core/src/network/LanScannerService.hpp` y `core/src/network/LanScannerService.cpp`.
   * Integrar la lógica asíncrona mediante la abstracción `cbdos::rtos`.
3. **Paso 3: Backends HAL en `bsp/`**
   * Implementar `bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp`.
   * Implementar `bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp`.
   * Inyectar los backends en las secuencias de inicio de ambos targets.
4. **Paso 4: Interfaz Gráfica LVGL 9.5 e Integración en Dashboard**
   * Crear `core/src/ui/views/LanReconView.hpp` y `core/src/ui/views/LanReconView.cpp`.
   * Registrar la aplicación en `DashboardView.cpp`.
5. **Paso 5: Verificación de Compilación Cruzada Multi-Target (Regla 2)**
   * Compilar en ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`.
   * Compilar en ESP32-P4: `idf.py build` en `bsp/esp32_p4_jc4880`.
