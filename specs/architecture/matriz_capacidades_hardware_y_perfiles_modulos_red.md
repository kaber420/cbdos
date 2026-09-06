# 📋 Especificación Técnica: Matriz de Capacidades de Hardware y Perfiles Dinámicos de Módulos de Red (CBDos v0.2.1)

**Documento:** `docs/architecture/matriz_capacidades_hardware_y_perfiles_modulos_red.md`  
**Versión:** 1.0.0 (RFC-CBDOS-NET-03)  
**Estado:** 📐 Especificación y Diseño de Implementación  
**Autor:** Equipo de Arquitectura de Software CBDos & Usuario  
**Fecha:** Agosto 2026  

---

## 🧭 1. Motivación y Objetivo

El Cyberdeck CBDos está diseñado para operar con múltiples radios integradas y periféricos modulares de radiofrecuencia (acoplados por cabezal de expansión JP1/NFC, bus SDIO o puertos USB-CDC).

Cada transceptor de hardware posee un conjunto **único y excluyente** de características físicas (bandas de frecuencia, esquemas de modulación, anchos de banda, factores de dispersión y protocolos soportados).

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    MATRIZ DE DISPOSITIVOS Y CAPACIDADES RF                      │
├──────────────────┬─────────────────┬──────────────────────┬──────────────────────┤
│ Semtech SX1262   │ Semtech SX1280  │ ESP32-C6 (SDIO)      │ TI CC1101 (Sub-GHz)  │
├──────────────────┼─────────────────┼──────────────────────┼──────────────────────┤
│ • Sub-GHz LoRa   │ • 2.4 GHz LoRa  │ • Wi-Fi 6 (802.11ax) │ • Sub-GHz ISM        │
│ • FSK / GFSK     │ • FLRC 1.3 Mbps │ • Bluetooth 5.3 LE   │ • OOK / ASK          │
│ • SF5 - SF12     │ • GFSK / BLE PHY│ • Zigbee 3.0 (802.15)│ • 2-FSK / GFSK / MSK │
│ • BW 7.8 - 500k  │ • BW 203 - 1625k│ • OpenThread (Matter)│ • Keeloq / Remotes   │
└──────────────────┴─────────────────┴──────────────────────┴──────────────────────┘
```

### Problema que resuelve
Actualmente, los menús de configuración de red exponen opciones globales que pueden no coincidir con el transceptor físicamente conectado. Intentar aplicar un modo no soportado (ej. solicitar modulación *FLRC* en un chip *SX1262*, o *Zigbee* en un *SX1280*) provoca errores de inicialización o estados inconsistentes.

### Solución Arquitectónica
Implementar un **Registro de Capacidades de Hardware (`HardwareCapabilityRegistry`)** desacoplado en `core/`, que permita a la interfaz de usuario ([NetworkManagerView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/NetworkManagerView.cpp)) consultar dinámicamente los modos válidos para el módulo activo y poblar los desplegables (*dropdowns*) de forma segura y consistente.

---

## 🏗️ 2. Estructura de Datos en `core/` (Zero Platform Pollution)

En `core/include/cbdos/network_capabilities.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace cbdos {
namespace network {

/// @brief Identificador del Chipset / Controlador de Radio
enum class ChipsetType : uint8_t {
    Unknown = 0,
    Esp32C6_SDIO,    // Coprocesador integrado 2.4 GHz
    Esp32S3_Internal,// Radio integrada SoC S3
    Semtech_SX1262,  // Mochila LoRa Sub-GHz (433/868/915 MHz)
    Semtech_SX1280,  // Mochila LoRa / FLRC 2.4 GHz
    TI_CC1101,       // Transceptor Sub-GHz multi-modulación
    Nordic_NRF24L01, // Transceptor GFSK 2.4 GHz
    USB_Serial_TNC   // Módem externo TNC / SLIP (C3, Heltec, T-Beam)
};

/// @brief Banderas de modulación física
enum ModulationFlags : uint32_t {
    MOD_NONE          = 0,
    MOD_LORA_SUBGHZ   = (1 << 0),
    MOD_LORA_2V4GHZ   = (1 << 1),
    MOD_FLRC_HIGH_SPD = (1 << 2),  // Fast Long-Range Communication (hasta 1.3 Mbps)
    MOD_FSK_GFSK      = (1 << 3),
    MOD_OOK_ASK       = (1 << 4),
    MOD_WIFI_80211    = (1 << 5),
    MOD_BLE_5X        = (1 << 6),
    MOD_IEEE_802154   = (1 << 7)   // Zigbee / Thread
};

/// @brief Banderas de protocolos de alto nivel soportados
enum ProtocolFlags : uint32_t {
    PROTO_NONE        = 0,
    PROTO_MESHCORE    = (1 << 0),
    PROTO_ALTERNET    = (1 << 1),
    PROTO_MESHTASTIC  = (1 << 2),
    PROTO_LORAWAN     = (1 << 3),
    PROTO_TCP_IP_LWIP = (1 << 4),
    PROTO_ZIGBEE_30   = (1 << 5),
    PROTO_OPENTHREAD  = (1 << 6),
    PROTO_KISS_TNC    = (1 << 7)
};

/// @brief Descriptor completo de capacidades de un módulo
struct ModuleCapabilities {
    ChipsetType chipset;
    const char* displayName;
    const char* manufacturer;
    uint32_t supportedBands;       // Ej. 433, 868, 915, 2400 MHz
    uint32_t modulations;          // Bitmask de ModulationFlags
    uint32_t protocols;            // Bitmask de ProtocolFlags
    
    // Opciones para la UI
    std::vector<const char*> modulationLabels;
    std::vector<const char*> bandwidthLabels;
    std::vector<const char*> spreadingFactorLabels;
    std::vector<const char*> powerLevelLabels;
    
    bool supportsHighSpeedTransfer; // Ej. FLRC / Wi-Fi
    bool supportsSniffing;          // Ej. CC1101, SX1280
};

} // namespace network
} // namespace cbdos
```

---

## 📊 3. Matriz Detallada de Especificaciones por Módulo

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        ESPECIFICACIÓN FORMAL DE CHIPSETS                               │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### 3.1. Semtech SX1262 (Mochila Sub-GHz LoRa)
* **Banda:** Sub-GHz (433 / 868 / 915 MHz).
* **Modulaciones:** `LoRa`, `(G)FSK`.
* **Anchos de Banda (BW):** `7.8 kHz`, `10.4 kHz`, `15.6 kHz`, `20.8 kHz`, `31.25 kHz`, `41.7 kHz`, `62.5 kHz`, `125 kHz`, `250 kHz`, `500 kHz`.
* **Spreading Factors (SF):** `SF5`, `SF6`, `SF7`, `SF8`, `SF9`, `SF10`, `SF11`, `SF12`.
* **Protocolos:** MeshCore Sub-GHz, Meshtastic, LoRaWAN Class A/C.
* **Potencia TX:** +22 dBm (160 mW con PA integrado).

### 3.2. Semtech SX1280 (Mochila 2.4 GHz LoRa / FLRC)
* **Banda:** 2.4 GHz ISM (2400 – 2500 MHz).
* **Modulaciones:**
  * `LoRa 2.4 GHz` (Largo alcance a 2.4 GHz).
  * `FLRC` (Fast Long-Range Communication @ 260 kbps, 520 kbps, 1040 kbps, **1300 kbps**).
  * `(G)FSK` & `BLE Physical Layer`.
* **Anchos de Banda (BW):** `203 kHz`, `406 kHz`, `812 kHz`, `1625 kHz`.
* **Spreading Factors (SF):** `SF5`, `SF6`, `SF7`, `SF8`, `SF9`, `SF10`, `SF11`, `SF12` (en modo LoRa).
* **Protocolos:** CBD-Net Alta Velocidad (archivos/fotos vía FLRC), MeshCore 2.4G.
* **Potencia TX:** +13 dBm.

### 3.3. ESP32-C6 (Coprocesador SDIO Slot 1)
* **Banda:** 2.4 GHz ISM.
* **Modulaciones:**
  * `Wi-Fi 6 (802.11ax / b/g/n)` con Target Wake Time (TWT).
  * `Bluetooth 5.3 LE` (1M, 2M, Coded PHY para largo alcance).
  * `IEEE 802.15.4` (O-QPSK PHY @ 250 kbps).
* **Protocolos:**
  * Wi-Fi STA / SoftAP (Pila TCP/IP LwIP).
  * BLE GATT / BLE Mesh.
  * **Zigbee 3.0** (Coordinador, Router o End Device domótico).
  * **OpenThread** (Router de borde / red Matter).
  * ESP-NOW / ESP-NOW LR (Largo Alcance).

### 3.4. Texas Instruments CC1101 (Mochila Sub-GHz Multi-Propósito)
* **Banda:** Sub-GHz (300-348 MHz, 387-464 MHz, 779-928 MHz).
* **Modulaciones:** `2-FSK`, `GFSK`, `MSK`, `OOK`, `ASK`.
* **Data Rate:** 0.6 kbps a 500 kbps.
* **Protocolos:** RF Sniffer, controles remotos Keeloq, sensores meteorológicos 433 MHz.

---

## 🎛️ 4. Integración Dinámica en la Interfaz de Usuario (`NetworkManagerView`)

```text
       ┌───────────────────────────────┐
       │   Detección NFC / Mochila     │
       └──────────────┬────────────────┘
                      ▼
       ┌───────────────────────────────┐
       │ HardwareCapabilityRegistry    │
       │ -> Obtiene ModuleCapabilities │
       └──────────────┬────────────────┘
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              NetworkManagerView (LVGL 9.5)                  │
├─────────────────────────────────────────────────────────────┤
│ 1. Actualiza Dropdown "Modulación" con opciones válidas.    │
│ 2. Si selecciona LoRa -> Muestra selector SF y BW.          │
│ 3. Si selecciona FLRC -> Muestra selector Bitrate (1.3M).   │
│ 4. Si selecciona C6 -> Muestra Zigbee / Thread / Wi-Fi / BLE│
└─────────────────────────────────────────────────────────────┘
```

### Algoritmo de Actualización de UI:
1. Al abrir la vista o al conectarse una mochila por NFC, `NetworkManagerView` invoca `getCapabilitiesForActiveDevice()`.
2. Se limpian las opciones de los desplegables (`lv_dropdown_clear_options`).
3. Se concatena la lista de opciones válidas desde `capabilities.modulationLabels`.
4. Según la modulación seleccionada, los contenedores auxiliares (`m_boxLoraParams`, `m_boxFlrcParams`, `m_boxZigbeeParams`) se muestran u ocultan con `lv_obj_remove_flag / lv_obj_add_flag(..., LV_OBJ_FLAG_HIDDEN)`.

---

## 🚀 5. Fases de Implementación Propuestas

| Fase | Componente | Descripción |
| :--- | :--- | :--- |
| **Fase 1** | `core/include/cbdos/network_capabilities.hpp` | Definición de estructuras de datos, tipos de chipsets y flags. |
| **Fase 2** | `core/src/network/HardwareCapabilityRegistry.cpp` | Registro central con las tablas estáticas de especificaciones para SX1262, SX1280, ESP32-C6, CC1101, etc. |
| **Fase 3** | `core/src/ui/views/NetworkManagerView.cpp` | Refactorización de desplegables y parámetros visuales para consumir el registro dinámico. |
| **Fase 4** | `bsp/` (ESP32-P4 / ESP32-S3) | Enlace de detección física (NFC / auto-identificación de pines) con el `HardwareCapabilityRegistry`. |
