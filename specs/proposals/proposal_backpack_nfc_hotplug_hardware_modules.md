# 🎒 Propuesta Técnica: Sistema Modular de Mochilas de Hardware (Backpacks) con Detección Hot-Plug por NFC / RFID

**Versión:** 1.0.0 (RFC-CBDOS-HW-BACKPACK)  
**Estado:** 💡 Propuesta Formal de Arquitectura  
**Autores:** Equipo de Arquitectura CBDos & Usuario  
**Fecha:** Agosto 2026  

---

## 🏛️ 1. Visión y Concepto General

El objetivo de este subsistema es dotar al Cyberdeck **CBDos** de una capacidad de **expansión modular física en caliente (*Hot-Plug*)**. A través de accesorios acoplables denominados **Mochilas (Backpacks)** equipados con una etiqueta **NFC / RFID (o EEPROM I2C / 1-Wire)**, el sistema operativo es capaz de:

1. **Auto-Descubrimiento Inmediato:** Detectar la presencia de un módulo de hardware al acoplarse físicamente a los pines de expansión.
2. **Carga Perezosa de Drivers (*Lazy Loading*):** Mantener los drivers inactivos en memoria Flash sin consumir RAM, CPU ni batería hasta que la mochila es detectada.
3. **Mapeo Dinámico de Pines GPIO y Buses:** Configurar en tiempo de ejecución los buses (SPI, I2C, UART) y líneas de control (CS, RST, IRQ) indicados por los metadatos de la etiqueta.
4. **Integración Transparente con la HAL:** Inyectar automáticamente el nuevo driver a la capa de abstracción correspondiente (ej: registrar un backend SX1280 en `cbdos::radio::setRadioBackend()`).
5. **Experiencia de Usuario Inmersiva:** Emitir una notificación sonoro-visual (Toast LVGL) y abrir opcionalmente la app nativa o script Lua asociado.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                  CYBERDECK (CBDos - ESP32-P4 / S3)                      │
│                                                                         │
│  [ Lector NFC I2C / SPI ] ◄─── Proximidad RF ───► [ Tag NFC NTAG213 ]   │
│  [ Header GPIO / Buses  ] ◄─── Contacto Físico ──► [ Módulo Hardware  ]  │
│                                                   (SX1280 / LoRa / SDR) │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 📦 2. Estructura de Metadatos de la Etiqueta NFC (`NDEF / MessagePack`)

La etiqueta de la mochila almacena un payload ultracompacto (< 128 bytes) que describe la identidad del módulo, el bus requerido, los pines de asignación y las acciones del sistema.

### Esquema JSON / MessagePack:

```json
{
  "magic": "CBD_BP",
  "version": 1,
  "id": "rf_sx1280_2g4",
  "type": "radio",
  "name": "Módulo Semtech SX1280 (FLRC/LoRa 2.4GHz)",
  "bus": {
    "type": "spi",
    "host": 2,
    "freq_mhz": 10,
    "pins": {
      "cs": 5,
      "rst": 6,
      "busy": 7,
      "dio1": 8
    }
  },
  "hal_target": "radio_backend",
  "launch": {
    "action": "open_view",
    "view_name": "MeshView",
    "lua_script": "/sdcard/apps/flrc_tracker/main.lua"
  }
}
```

---

## 🧠 3. Ciclo de Vida y Gestión de Memoria (Eficiencia Máxima)

Para maximizar la eficiencia y evitar el agotamiento de RAM o colisiones en los buses:

### A) Estado Reposo (*Idle / Desconectado*):
* **Consumo de RAM:** 0 bytes.
* **Estado de Pines:** En alta impedancia (*Floating / Hi-Z*) o con *pull-downs* de seguridad.
* **Consumo de Batería:** 0 mA adicional.

### B) Evento de Acople (*Hot-Plug Attach*):
1. La tarea de fondo `NfcWatcherTask` detecta una etiqueta con cabecera `"CBD_BP"`.
2. `BackpackManager` parsea los metadatos y consulta el registro de fábricas (`BackpackRegistry`).
3. Si el módulo está soportado:
   * Inicializa el bus hardware configurando los pines indicados.
   * Instancia el objeto driver en PSRAM/Heap (`new DriverSX1280(cfg)`).
   * Registra el backend en la HAL del sistema (`cbdos::radio::setRadioBackend(driverInstance)`).
   * Lanza un evento en el `EventBus` (`EVENT_BACKPACK_ATTACHED`).
   * La UI muestra un Toast Cyberpunk: `[+] Mochila Conectada: SX1280 FLRC (SPI: CS=5)`.
   * Si la mochila define un `lua_script` o `view_name`, se lanza en pantalla.

### C) Evento de Desacople (*Hot-Unplug Detach*):
1. El lector NFC pierde la comunicación o se detecta interrupción física por pin `DET` (Ground Detect).
2. Se llama a `driver->deinit()`.
3. Se liberan los buffers y la memoria RAM asignada al driver (`delete driverInstance`).
4. Se restablece la HAL al backend por defecto (ej. radio interna ESP32-C6 o Wi-Fi).
5. Se restauran los pines a reposo seguro.
6. La UI emite notificación: `[-] Mochila Desconectada`.

---

## 🔌 4. Catálogo Inicial de Mochilas Planificadas

| ID Mochila | Tipo | Bus Hardware | Pines Requeridos | Integración HAL / App |
| :--- | :--- | :--- | :--- | :--- |
| `rf_sx1280_2g4` | Radio 2.4 GHz FLRC / LoRa | SPI | CS, RST, BUSY, DIO1 | `cbdos::radio` / `MeshEngine` |
| `rf_sx1262_subg`| Radio LoRa 915/868 MHz | SPI | CS, RST, BUSY, DIO1 | `cbdos::radio` / `LoRaChatView` |
| `hid_cardkb_i2c`| Teclado Físico QWERTY | I2C | SDA, SCL, INT | `cbdos::input` / Inyección LVGL |
| `nav_gps_neo6m` | GPS / GLONASS | UART | TX, RX, PPS | `cbdos::geo` / `GpsTrackerView` |
| `sdr_cc1101`    | Sub-GHz RF Analyzer | SPI | CS, GD0, GD2 | `cbdos::rf` / `SubGhzAnalyzerView`|
| `env_bme680`    | Sensor Ambiental IAQ/Temp | I2C | SDA, SCL | `cbdos::sensors` / Dashboard Widget |

---

## 💻 5. Contrato de C++ en la HAL (`core/include/cbdos/backpack.hpp`)

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace cbdos {
namespace backpack {

enum class BusType {
    None,
    SPI,
    I2C,
    UART,
    OneWire
};

struct BackpackPinout {
    int8_t cs = -1;
    int8_t rst = -1;
    int8_t irq = -1;
    int8_t busy = -1;
    int8_t sda_tx = -1;
    int8_t scl_rx = -1;
};

struct BackpackMetadata {
    std::string id;
    std::string type;
    std::string name;
    BusType bus = BusType::None;
    uint32_t speed_hz = 0;
    BackpackPinout pins;
    std::string launchAction;
    std::string launchTarget;
};

class IBackpackDriver {
public:
    virtual ~IBackpackDriver() = default;
    virtual bool init(const BackpackMetadata& meta) = 0;
    virtual void deinit() = 0;
    virtual const char* getName() const = 0;
};

class BackpackManager {
public:
    static BackpackManager& getInstance();

    bool init();
    void registerDriverFactory(const std::string& typeId, std::function<IBackpackDriver*()> factory);

    bool isBackpackAttached() const;
    const BackpackMetadata* getAttachedMetadata() const;

    void onNfcTagDetected(const uint8_t* payload, size_t len);
    void onNfcTagRemoved();

private:
    BackpackManager() = default;
};

} // namespace backpack
} // namespace cbdos
```

---

## 🚀 6. Fases de Implementación del Subsistema

* [ ] **Fase A (Especificación & HAL):** Definir `cbdos/backpack.hpp` y contratos base.
* [ ] **Fase B (Driver NFC Reader):** Implementar tarea de sondeo de etiquetas NTAG/Mifare en el BSP (PN532 / ST25R3911B / RC522).
* [ ] **Fase C (Drivers Modulares):** Implementar fábricas para SX1280 (FLRC), SX1262 (LoRa) y CardKB.
* [ ] **Fase D (Integración UI & Lua):** Notificaciones Toast dinámicas y vinculación con scripts Lua descargados en MicroSD.

---

## 🧩 7. Extensión Multi-Módulo por Mochila (Máximo 2-3 Módulos) y Bus I2C Compartido

Para mantener el diseño pragmático, modular y libre de sobre-ingeniería, una mochila física albergará como máximo **2 (o excepcionalmente 3) módulos combinados** (ej. **Módulo LoRa SX1262 por SPI/UART + Sensor Ambiental/Teclado por I2C**, o **Radio Sub-GHz + GPS**):

### 7.1. Esquema Pragmático del Descriptor (NFC Tag Payload)
```json
{
  "magic": "CBD_BP",
  "v": 2,
  "id": "bp_tactical_lora_sensors",
  "name": "Mochila Táctica (LoRa + Sensores)",
  "devices": [
    {
      "id": "radio_lora",
      "type": "sx1262",
      "bus": "spi",
      "pins": { "sck": 50, "mosi": 52, "miso": 51, "cs": 49, "rst": 35, "dio1": 34, "busy": 32 }
    },
    {
      "id": "env_sensor",
      "type": "bme680_i2c",
      "bus": "i2c",
      "i2c_addr": "0x76",
      "pins": { "sda": 7, "scl": 8 }
    }
  ],
  "launch": {
    "app": "meshcore_sensors.luapp",
    "toast": "Mochila Conectada: LoRa 915MHz + Sensor BME680"
  }
}
```

### 7.2. Principio de Conexión y Asignación de Buses:
1. **Bus I2C Nativo Compartido:** Los dispositivos secundarios (sensores ambientales, teclados CardKB, chips criptográficos o expansores) se conectan al bus **I2C (`GPIO 7` / `GPIO 8`)**, compartiendo las 2 líneas mediante sus direcciones de hardware 7-bit (`0x76`, `0x5F`, `0x48`, etc.) sin ocupar pines de selección adicionales.
2. **Periférico Principal de Alta Velocidad (SPI o UART):** El módulo principal de radio o telemetría (SX1262, CC1101 o GPS) utiliza los pines dedicados del header JP1 (`GPIO 52, 51, 50, 49, 35, 34, 32, 28`).
3. **Carga y Orquestación Directa:** El orquestador `BackpackManager` inicializa los dos dispositivos e inyecta sus APIs correspondientes a las vistas UI o scripts Lua.
4. **Desconexión Segura (*Hot-Unplug*):** Al desacoplar la mochila, se liberan los drivers y los pines vuelven a reposo seguro en alta impedancia.
