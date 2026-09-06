# 📐 Borrador de Diseño: Administrador Unificado de Radios y Redes (`RadioManager`)

## 1. Motivación y Diagnóstico Actual
Actualmente en CBDos:
1. Las configuraciones de **Wi-Fi**, **ESP-NOW** y **Radio** se gestionan en menús separados, generando inconsistencias de estado.
2. Si el usuario apaga el radio en los ajustes pero está en Wi-Fi, la pantalla de **Diagnóstico** sigue mostrando *"Esperando trama..."*, ya que el motor de comunicaciones no está sincronizado con el estado real del hardware.
3. No existe un estándar claro para conectar módulos de radio externos (ej. Semtech **SX1280** para 2.4 GHz LoRa/FLRC o **SX1262** para Sub-GHz 915/433 MHz) a través de los puertos de expansión (JP1 / SPI).

---

## 2. Arquitectura de Abstracción: Capa Física de Radio (`IRadioDriver`)

Todo hardware de radio (interno o externo) implementará una interfaz agnóstica de C++ en `core/include/cbdos/radio/`:

```cpp
namespace cbdos {
namespace radio {

enum class RadioType {
    InternalSoC,     // ESP32-C6 (P4) o Radio Interna (S3)
    SemtechSX1280,   // 2.4 GHz LoRa / FLRC / Ranging
    SemtechSX1262,   // Sub-GHz LoRa (433/868/915 MHz)
    CustomSPI,       // Módulo experimental definido por el desarrollador
    CustomUART       // Módulo serie externo
};

enum class RadioMode {
    Off,             // Radio apagada / Modo bajo consumo
    WiFiStation,     // Conexión a Router / Internet (Solo Radio Integrada)
    WiFiAP,          // Punto de Acceso Local
    DirectBroadcast, // Paquetes directos Capa 2 (Micro-Broadcast 7 bytes)
    LoRa,            // Modulación LoRa estándar
    FLRC,            // Fast Line of Sight (hasta 1.3 Mbps, SX1280)
    OpenThread_802154 // Protocolo 802.15.4 / Thread (C6)
};

struct RadioCapabilities {
    bool supportsWiFi;
    bool supportsDirectBroadcast;
    bool supportsLoRa;
    bool supportsFLRC;
    bool supportsThread;
    uint32_t minFreqKhz;
    uint32_t maxFreqKhz;
};

class IRadioDriver {
public:
    virtual ~IRadioDriver() = default;
    virtual bool init() = 0;
    virtual void powerOff() = 0;
    virtual bool setMode(RadioMode mode) = 0;
    virtual RadioMode getMode() const = 0;
    virtual bool setFrequency(uint32_t freqKhz) = 0;
    virtual bool setTxPower(int8_t powerDbm) = 0;
    virtual bool sendPacket(const uint8_t* data, size_t len) = 0;
    virtual void onPacketReceived(void (*callback)(const uint8_t* data, size_t len, int8_t rssi)) = 0;
    virtual RadioCapabilities getCapabilities() const = 0;
    virtual bool isHardwareReady() const = 0;
};

} // namespace radio
} // namespace cbdos
```

---

## 3. Experiencia de Usuario: Menú Unificado de Conectividad

La UI agrupa todas las interfaces en una sola vista coherente:

```text
┌──────────────────────────────────────────────────────────┐
│ 📶 CONECTIVIDAD Y RADIOS                                 │
├──────────────────────────────────────────────────────────┤
│ 📻 Radio Integrada (ESP32-C6 / ESP32-S3)                 │
│    Modo: [ Wi-Fi (Infraestructura) ▼ ]                   │
│    Estado: Conectado a 'romero24' (IP: 192.168.1.50)     │
│    [ Configurar Red / Seguridad > ]                      │
├──────────────────────────────────────────────────────────┤
│ 📡 Slot de Expansión 1 (Puerto SPI / JP1)                 │
│    Hardware: [ Semtech SX1280 (2.4 GHz) ▼ ]              │
│    Modo:     [ FLRC (Alta Velocidad 1.3 Mbps) ▼ ]        │
│    Canal:    2.440 GHz | Potencia: +13 dBm               │
│    Estado:   🟢 En escucha activa                        │
├──────────────────────────────────────────────────────────┤
│ 🛠️ Slot de Expansión 2 (Módulos de Desarrollador)        │
│    Hardware: [ Personalizado / Custom GPIO ▼ ]           │
│    [ Configurar Pines (MISO/MOSI/SCK/CS/DIO1) > ]         │
└──────────────────────────────────────────────────────────┘
```

---

## 4. Comportamiento Inteligente del Diagnóstico
1. **Sincronización con el Hardware:** 
   - Si la radio seleccionada está en modo `Off` o en modo `WiFiStation`, el panel de diagnóstico mostrará:
     *`"Radio de paquetes directos: DESACTIVADA (Modo actual: Wi-Fi)"`* en lugar de quedarse estancado en *"Esperando trama..."*.
2. **Telemetría en Vivo:**
   - Si un módulo SX1280 o SX1262 está activo, mostrará RSSI, SNR, paquetes enviados/recibidos y frecuencia en tiempo real.

---

## 5. Próximos Pasos Propuestos
1. Crear la interfaz base `core/include/cbdos/radio/IRadioDriver.hpp`.
2. Implementar `InternalC6RadioDriver` y `InternalS3RadioDriver` mapeados a los BSPs correspondientes.
3. Unificar la vista de Ajustes de Red en la interfaz gráfica de LVGL 9.5.
