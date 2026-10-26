# Plan de Implementación: Servicio de Red NeighborDiscoveryService (Descubrimiento Multimarca de Vecinos L2/L3) e Integración en OmniWISP / LanRecon

**Documento:** `specs/network/plan_implementacion_neighbor_discovery_service.md`  
**Estado:** Propuesta Técnica / Plan de Trabajo  
**Alineación:** Core C++17 desacoplado, LVGL v9.5 reactivo, cero datos falsos, cero bloqueos en UI.

---

## 1. Motivación y Diagnóstico Actual

1. **El escaneo actual bloquea la UI:**
   En `UbiquitiView.cpp`, la telemetría se consulta de forma síncrona con llamadas `cbdos::http::execute()` en el hilo principal de LVGL. Si la antena o router no responde o la IP no existe, la pantalla y el táctil se congelan durante 8 a 13 segundos.
2. **Falta de Descubrimiento de Vecinos de Red (Neighbor Discovery):**
   `LanScannerService` únicamente implementa sondeo ARP y SSDP (puerto 1900 para Smart TVs y routers domésticos). En redes profesionales e infraestructura de red:
   - **Ubiquiti (airMAX, EdgeMAX, UniFi):** Emplea **UBNT Discovery v1/v2** en broadcast UDP puerto **`10001`**.
   - **MikroTik (RouterOS, RouterBOARD):** Emplea **MNDP (MikroTik Neighbor Discovery Protocol)** en broadcast UDP puerto **`5678`**.
   - **Cisco / switches empresariales:** Emplean **CDP** o **LLDP** (Link Layer Discovery Protocol, IEEE 802.1AB).

Al igual que se implementó `ssdpScan` en el HAL para enriquecer el escáner de red, se requiere un servicio universal de descubrimiento de vecinos (**`NeighborDiscoveryService`**) que opere de forma no bloqueante y exponga los dispositivos encontrados a **OmniWISP**, **LanRecon** y cartuchos **Lua**.

---

## 2. Arquitectura del Servicio: `NeighborDiscoveryService`

```
┌────────────────────────────────────────────────────────┐
│                      UI / VISTAS                       │
│   OmniWISP View (C++)   LanRecon View    Lua Scripts   │
└───────────────────────────▲────────────────────────────┘
                            │ Notificaciones reactivas (callbacks / dirty flag)
┌───────────────────────────┴────────────────────────────┐
│       cbdos::network::NeighborDiscoveryService         │
│  - Orquestador desacoplado en Core C++17               │
│  - Cache en RAM de vecinos descubiertos                │
│  - Tarea en background (Worker sin bloquear LVGL)      │
└───────────────────────────▲────────────────────────────┘
                            │ Contrato HAL (INeighborDiscoveryBackend)
┌───────────────────────────┴────────────────────────────┐
│                    BSP / HAL SOCKETS                   │
│  - UDP 10001 Broadcast (Ubiquiti UBNT Discovery)      │
│  - UDP 5678 Broadcast (MikroTik MNDP)                  │
│  - Raw L2 / Promiscuo (CDP / LLDP)                     │
└────────────────────────────────────────────────────────┘
```

---

## 3. Estructuras de Datos y Contrato (`core/include/cbdos/neighbor_discovery.hpp`)

```cpp
namespace cbdos {
namespace network {

enum class NeighborBrand {
    Unknown,
    Ubiquiti,
    MikroTik,
    Cisco,
    Generic
};

struct NeighborDevice {
    std::string ip;
    uint8_t mac[6]{0};
    NeighborBrand brand{NeighborBrand::Unknown};
    std::string protocol;     // "ubnt", "mndp", "cdp", "lldp"
    std::string model;        // ej. "PowerBeam 5AC Gen2", "RB750Gr3", "LiteBeam 5AC"
    std::string identity;     // Hostname / SSID del AP / Nombre en RouterOS
    std::string firmware;     // ej. "WA.v8.7.1", "RouterOS v7.14"
    std::string interface;    // Interfaz reportada (ej. "ath0", "ether1")
    uint32_t uptimeSec{0};
    uint32_t lastSeenMs{0};

    std::string getMacString() const;
    std::string getBrandString() const;
};

using OnNeighborDiscoveredCb = std::function<void(const NeighborDevice& dev)>;

class NeighborDiscoveryService {
public:
    static NeighborDiscoveryService& getInstance();

    // Inicia un escaneo activo en background (envía broadcasts UBNT y MNDP)
    bool triggerActiveScan(uint32_t timeoutMs = 3000);

    // Consulta de dispositivos en caché
    std::vector<NeighborDevice> getDiscoveredDevices() const;
    bool findByIp(const std::string& ip, NeighborDevice& outDev) const;
    void clear();

    // Registro de observadores reactivos
    void setDeviceCallback(OnNeighborDiscoveredCb cb);

    // Estado del escaneo
    bool isScanning() const;
};

} // namespace network
} // namespace cbdos
```

---

## 4. Fases de Implementación

### Fase 1: Protocolos de Descubrimiento en el HAL
1. **Ubiquiti Discovery (UDP 10001):**
   - Paquete de sondeo v1: `\x01\x00\x00\x00` enviado a `255.255.255.255:10001`.
   - Parser de TLVs de respuesta:
     - Tipo 0x01: MAC Address (6 bytes).
     - Tipo 0x02: IP Address / MAC.
     - Tipo 0x03: Versión de firmware (string).
     - Tipo 0x0B: Nombre / Hostname (string).
     - Tipo 0x0C: Modelo / Plataforma (ej. "WA", "XC", "2WA").
     - Tipo 0x0D: ESSID / Nombre de red inalámbrica.
2. **MikroTik MNDP (UDP 5678):**
   - Paquete de consulta MNDP / escucha de avisos broadcast periódicos en puerto `5678`.
   - Decodificación TLV:
     - Tipo 0x01: MAC.
     - Tipo 0x05: Identity (Hostname del RouterOS).
     - Tipo 0x07: Versión de RouterOS.
     - Tipo 0x08: Modelo de RouterBOARD / Plataforma.
     - Tipo 0x09: Uptime.
     - Tipo 0x10: Nombre de la interfaz.

### Fase 2: Implementación en HAL S3 y P4
- Implementar los métodos de socket UDP con timeout en `hal_lan_recon_s3.cpp` y `hal_lan_recon_p4.cpp` (usando `std::unique_ptr` para buffers fuera del stack, protegiendo contra desbordamientos de pila FreeRTOS).

### Fase 3: Integración en la App Nativa `OmniWISP`
1. **Renombrar formalmente:**
   - Cambiar nombres de vista y registros a **OmniWISP** en Core y Cartuchos.
2. **Desacoplar la petición HTTP:**
   - Mover la autenticación y lectura de `/status.cgi` a una tarea en background (`TaskHandle_t` / worker thread).
   - La UI muestra estado *"Conectando..."* sin detener el render ni el tacto de LVGL.
3. **Selector y Lista de Vecinos descubiertos:**
   - Vista dividida: pestañas o desplegable con *"Vecinos en Red"* (poblado por `NeighborDiscoveryService`) y *"Conexión Manual"*.
   - Al pulsar un vecino descubierto, cargar sus credenciales desde el `VaultService` si existen, o solicitar contraseña y guardarla.

### Fase 4: Enriquecimiento de `LanReconView`
- Cuando `LanScannerService` detecte un host cuya IP o MAC coincida con un vecino encontrado por `NeighborDiscoveryService`:
  - Mostrar en la columna de detalles el fabricante y modelo real (ej: `[UBNT] LiteBeam 5AC` o `[MikroTik] RB750Gr3` en lugar de una IP vacía).
  - Indicar el método de descubrimiento como `"ubnt"` o `"mndp"`.

---

## 5. Criterios de Aceptación
1. Cero datos simulados ni tráfico inventado (honestidad absoluta con el hardware de red).
2. La UI nunca se traba al escanear: el hilo de LVGL responde al tacto en todo momento.
3. Detección exitosa de equipos Ubiquiti y MikroTik conectados al mismo segmento de red mediante broadcast UDP.
4. Compatibilidad limpia en compilación tanto para ESP32-S3 (PlatformIO) como ESP32-P4 (ESP-IDF).
