# 📡 Especificación Maestra de Arquitectura: Subsistema de Radio Integrada, Enlace y Redes para CBDos

**Documento:** `docs/drafts/BORRADOR_ARQUITECTURA_SUBSISTEMA_RADIO_Y_RED.md`  
**Versión:** 2.0.0-DRAFT  
**Estado:** Propuesta de Arquitectura Formal  
**Autor:** Equipo de Arquitectura CBDos  
**Fecha:** Agosto 2026  

---

## 🏛️ 1. Resumen Ejecutivo y Objetivos del Subsistema

En **CBDos**, la comunicación inalámbrica opera sobre enlaces de radio directos y protocolos de paquetes estructurados (Wi-Fi TCP/IP, ESP-NOW Normal, ESP-NOW Long Range, Gateways TLVGL y Nodos de datos).

Este documento establece la arquitectura definitiva para el subsistema de radio y red, resolviendo de forma estructural:

1. **Autoridad Única sobre el Silicio RF (Inversión de Control):** Centralizar el control del transceptor 2.4 GHz en una sola entidad (`cbdos::radio`), eliminando la competencia caótica donde Wi-Fi, ESP-NOW y rutinas de arranque manipulaban el hardware de forma descoordinada.
2. **Modos de Operación Reales y Fieles:**
   - **Radio Apagada (OFF):** Cero emisión RF, módem desenergizado (`WiFi.mode(WIFI_OFF)` / `esp_wifi_stop()`).
   - **Wi-Fi Estación (STA):** Pila TCP/IP estándar para routers e Internet (LwIP, DHCP, DNS, HTTP, NTP).
   - **ESP-NOW Normal:** Capa 2 directa a 1-2 Mbps para datagramas de alta velocidad entre dispositivos CBDos.
   - **ESP-NOW Long Range (LR):** Modulación propietaria 802.11 LR a 250 kbps y +20 dBm de potencia para enlaces de largo alcance.
   - **Híbrido / Coexistencia:** Wi-Fi STA en canal router + recepción simultánea ESP-NOW en dicho canal.
3. **Descubrimiento y Barrido Multicanal Determinista (*Channel Hopping Sweep 1..13*):** Exploración canal por canal sin borrado destructivo de resultados, con *dwell time* garantizado (75 ms) para detectar gateways (como el Dongle USB C3) y nodos en el aire.
4. **Secuencia de Arranque Determinista:** Carga de configuración persistente desde NVS antes de inicializar la UI, arrancando el microcontrolador exactamente en el modo y canal deseados sin sobreescrituras arbitrarias.

---

## 🏗️ 2. Arquitectura de Capas y Contratos HAL

El diseño sigue estrictamente el principio de **desacoplamiento total del hardware**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CAPA DE APLICACIÓN Y UI                            │
│    RadioConfigView    •    QuickSettingsPanel    •    TlvBrowserView        │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (Comandos de UI y Eventos)
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                    GESTOR DE RADIO DEL SISTEMA (CORE)                       │
│                           cbdos::radio / RadioManager                       │
│  • Persistencia NVS (namespace: "cbdos_radio")                              │
│  • Control de Alimentación (ON / OFF / Modo Seguro)                         │
│  • Selección de Modo: Wi-Fi STA | ESP-NOW Normal | ESP-NOW LR | Híbrido     │
│  • Asignación de Canal RF (1..13) y Potencia TX (+2 a +20 dBm)              │
│  • Servicio de Descubrimiento de Nodos & Barrido Multicanal (Sweep 1..13)   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ (Interfaz Abstracta C++ HAL)
                                       ▼
                     ┌────────────────────────────────────┐
                     │          CBDOS RADIO HAL           │
                     │         cbdos::radio::IRadioBackend│
                     └─────────────────┬──────────────────┘
                                       │
                ┌──────────────────────┴──────────────────────┐
                ▼                                             ▼
 ┌─────────────────────────────┐               ┌─────────────────────────────┐
 │    BSP ESP32-S3 (JC3248)    │               │    BSP ESP32-P4 (JC4880)    │
 │ • Driver nativo esp_wifi    │               │ • Control Coprocesador C6   │
 │ • WiFi.scanNetworks()       │               │ • ESP-Hosted / Stub limpio  │
 │ • esp_wifi_set_protocol(LR) │               │ • Pureza Multi-Target       │
 └─────────────────────────────┘               └─────────────────────────────┘
```

### 2.1. Contrato C++ de la Capa de Abstracción (`IRadioBackend`)

```cpp
namespace cbdos {
namespace radio {

enum class RadioMode {
    Off = 0,        // Radio apagada (Cero emisión RF)
    WifiSta = 1,    // Wi-Fi Estación TCP/IP
    EspNow = 2,     // ESP-NOW Normal (1-2 Mbps)
    EspNowLR = 3,   // ESP-NOW Long Range (250 kbps, +20dBm)
    Hybrid = 4      // Híbrido (Wi-Fi STA + ESP-NOW en canal asociado)
};

struct WifiApInfo {
    std::string ssid;
    int8_t rssi = -127;
    uint8_t channel = 1;
    bool isEncrypted = false;
    std::string bssid;
};

struct DiscoveredNode {
    uint8_t mac[6] = {0};
    uint16_t short_id = 0;
    uint32_t uuid = 0;
    char name[32] = {0};
    uint8_t channel = 1;
    int8_t rssi = -127;
    uint8_t supported_modes = 0;
    uint32_t last_seen_ms = 0;
};

struct RadioConfig {
    bool enabled = true;
    RadioMode mode = RadioMode::EspNow;
    uint8_t channel = 1;
    int8_t txPower = 20; // +20 dBm (2..20)
};

using WifiScanCallback = std::function<void(const std::vector<WifiApInfo>& aps, bool success)>;
using ChannelSweepCallback = std::function<void(uint8_t currentChannel, uint8_t totalChannels, const std::vector<DiscoveredNode>& nodes, bool finished)>;

class IRadioBackend {
public:
    virtual ~IRadioBackend() = default;

    virtual bool setPower(bool on) = 0;
    virtual bool isPowered() const = 0;

    virtual bool setMode(RadioMode mode) = 0;
    virtual RadioMode getMode() const = 0;

    virtual bool setChannel(uint8_t channel) = 0;
    virtual uint8_t getChannel() const = 0;

    virtual bool setTxPower(int8_t dbm) = 0;
    virtual int8_t getTxPower() const = 0;

    virtual bool startWifiScan(WifiScanCallback cb) = 0;
    virtual bool startChannelSweep(ChannelSweepCallback cb) = 0;
    virtual void stopScan() = 0;
};

void setRadioBackend(IRadioBackend* backend);
IRadioBackend* getRadioBackend();

bool init();
bool isRadioPowered();
void setRadioPower(bool on);
bool setMode(RadioMode mode);
RadioMode getMode();
const char* getModeName(RadioMode mode);
uint8_t getChannel();
bool setChannel(uint8_t channel);
int8_t getTxPower();
bool setTxPower(int8_t dbm);
bool startWifiScan(WifiScanCallback cb);
bool startChannelSweep(ChannelSweepCallback cb);
void stopScan();

} // namespace radio
} // namespace cbdos
```

---

## ⚡ 3. Secuencia de Arranque Determinista (Boot Lifecycle)

El microcontrolador sigue un orden estricto de inicialización donde la radio es configurada exclusivamente por su configuración persistente en NVS:

```
[ Encendido / Reset Hardware ]
               │
               ▼
[ 1. Inyectar Backend NVS (initPersistenceBackend) ]
               │
               ▼
[ 2. Inyectar Backend de Radio (initRadioBackend) ]
               │
               ▼
[ 3. Inyectar Backend de Transporte de Paquetes (initMeshTransport) ]
               │
               ▼
[ 4. cbdos::radio::init() ] ──▶ Cargar RadioConfig desde NVS ("cbdos_radio")
               │
               ├───────────────────────────────┬───────────────────────────────┐
               ▼                               ▼                               ▼
      [ enabled == false ]            [ mode == EspNow / LR ]        [ mode == WifiSta ]
               │                               │                               │
               ▼                               ▼                               ▼
       [ esp_wifi_stop() ]            [ esp_wifi_set_channel(ch) ]   [ Modo Wi-Fi STA ]
       [ WiFi.mode(WIFI_OFF) ]        [ Protocolo LR / 11BGN ]                 │
       (Cero emisión RF)              [ init MeshEngine(ch) ]                  ▼
                                      (NO conecta a routers)         ¿sysCfg.autoConnectWifi?
                                                                      ├── Sí: Conectar a SSID
                                                                      └── No: Modo reposo
               │
               ▼
[ 5. Inicializar Display, Touch, Audio y UI Core (LVGL 9.5) ]
```

---

## 📡 4. Protocolo de Sondeo y Barrido Multicanal (Channel Hopping Sweep)

El descubrimiento de nodos y gateways en el aire opera mediante tramas estructuradas de Capa 2 sobre broadcast:

### 4.1. Trama de Sondeo Activo (`Probe Request` - Tag 0x01)

```
┌─────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ MicroChunk  │ Control Byte │ Dst Short ID │ Tag Servicio │ Client Short │ Client Name  │
│  (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │ (Len + Bytes)│
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x01 0xAA   │ 0x4F (Signal)│ 0xFFFF (Bcast│ 0x01 (PROBE) │ 0xXXXX       │ "CBDos S3"   │
└─────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

### 4.2. Trama de Respuesta de Nodo / Gateway (`Probe Response` - Tag 0x02)

```
┌─────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ MicroChunk  │ Control Byte │ Dst Short ID │ Tag Servicio │ Node ID      │ Canal RF     │ Modos        │ Node Name    │
│  (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (2 Bytes)  │   (1 Byte)   │   (1 Byte)   │ (Len + Bytes)│
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 0x01 0xBB   │ 0x0F (Signal)│ Client Short │ 0x02 (RESP)  │ 0x0001       │ 0x01 (Ch 1)  │ 0x02 (LR/Norm│ "Gateway C3" │
└─────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────┘
```

### 4.3. Algoritmo de Barrido Multicanal No Destructivo

1. **Paso Inicial:** Se limpia la lista de nodos descubiertos **una sola vez** al inicio del barrido (`sweepNodes.clear()`). Se guarda el canal activo original.
2. **Iteración por Canales (Ch = 1 .. 13):**
   - Sintonizar hardware al canal `Ch` mediante `esp_wifi_set_channel(Ch)`.
   - Emitir `Probe Request` broadcast.
   - **Dwell Time:** Escuchar durante **75 ms** en segundo plano (tarea FreeRTOS).
   - Recopilar todas las respuestas de `Probe Response` recibidas en ese canal, agregándolas a la lista acumulativa sin borrar las respuestas de canales anteriores.
   - Notificar progreso a la interfaz gráfica (`currentChannel`, `totalChannels = 13`, `nodes`).
3. **Paso Final:** Al concluir el canal 13, restaurar el canal original o permitir al usuario sintonizar la radio directamente al canal del nodo seleccionado mediante el botón **"Sintonizar"**.

---

## 🗄️ 5. Estructura de Persistencia en NVS (`cbdos_radio`)

| Clave | Tipo | Valor por Defecto | Descripción |
| :--- | :--- | :--- | :--- |
| `enabled` | `bool` | `true` | Radio energizada (ON) o desenergizada (OFF). |
| `mode` | `uint8_t` | `2` (`EspNow`) | Modo operativo por defecto (`0: Off, 1: WifiSta, 2: EspNow, 3: EspNowLR, 4: Hybrid`). |
| `channel` | `uint8_t` | `1` | Canal de radio primario (1 al 13). |
| `tx_pwr` | `uint8_t` | `20` | Potencia de transmisión en dBm (+2 a +20 dBm). |

---

## 🖥️ 6. Especificación de Interfaz Gráfica (`RadioConfigView`)

La interfaz gráfica en LVGL 9.5 provee un panel claro y contextual:

1. **Tarjeta de Alimentación (Master Switch):**
   - Switch interactivo para encender o apagar la radio física.
   - Estado en tiempo real: *"Encendida (Módem RF Activo)"* vs *"Apagada (Modo Seguro / Offline)"*.
2. **Tarjeta de Configuración de Modo y Canal:**
   - Dropdown con los 4 modos de operación reales.
   - Slider de Canal RF (1 a 13) con indicador numérico.
   - Slider de Potencia TX (+2 a +20 dBm).
3. **Panel Wi-Fi Contextual (Visible en modo Wi-Fi o Híbrido):**
   - Botón **"🔍 Escanear Redes Wi-Fi"**.
   - Lista interactiva de APs con RSSI en dBm, indicador de canal e icono de candado si requiere contraseña.
   - Diálogo modal con teclado virtual para ingresar contraseña y conectar.
4. **Panel de Enlace CBDos Contextual (Visible en ESP-NOW, LR o Híbrido):**
   - Botón **"📡 Barrido Multicanal (Ch 1..13)"**.
   - Barra de progreso del barrido en vivo.
   - Lista de Nodos y Gateways descubiertos en el aire con su canal de operación y RSSI.
   - Botón **"Sintonizar"** en cada nodo para fijar la radio al canal exacto del nodo seleccionado.
5. **Botón Guardar Preferencias:**
   - Persistencia atómica en NVS y aplicación instantánea en hardware con Toast de confirmación.

---

## 🧪 7. Plan de Pruebas y Validación

1. **Prueba de Persistencia ESP-NOW:**
   - Configurar ESP-NOW LR en Canal 6 y guardar.
   - Reiniciar el dispositivo por hardware.
   - Verificar en consola y UI que el sistema inicia en Canal 6 con ESP-NOW activo y sin activar Wi-Fi STA.
2. **Prueba de Detección del Bridge C3:**
   - Con el bridge USB C3 encendido en Canal 1, ejecutar el barrido multicanal desde el S3.
   - Verificar que *"Dongle Gateway USB"* aparezca con Canal 1 y RSSI en dBm.
   - Tocar *"Sintonizar"* y comprobar que el canal del S3 cambia al Canal 1.
3. **Prueba de Modo Seguro / Radio OFF:**
   - Apagar la radio desde la UI y guardar.
   - Reiniciar y verificar que el módem permanece en `WIFI_OFF` con cero emisión RF.
4. **Compilación Multi-Target:**
   - Compilación limpia en PlatformIO para ESP32-S3 (`jc3248w535`).
   - Compilación limpia en ESP-IDF 5.5 para ESP32-P4 (`JC4880P443C`).
