# 📋 Plan Técnico de Separación: Persistencia NVS de Sistema vs. Almacenamiento de Datos de Aplicación (MessagePack/FS)

## 📌 1. Fundamento Técnico y Separación de Responsabilidades

Uno de los errores más graves en sistemas embebidos es saturar y desgastar la partición **NVS (Non-Volatile Storage)** utilizándola como base de datos para almacenar colecciones dinámicas, listas de reproducción o datos complejos de aplicaciones (como estaciones de radio favoritas, historiales o notas).

### 🛑 La Regla de Oro del Almacenamiento:
1. **NVS (KV-Store de Sistema):** **ÚNICAMENTE** para parámetros de configuración escalares y vitales para el arranque del hardware/OS (valores primitivos de tamaño fijo: `uint8_t`, `int32_t`, `bool`, strings cortos como SSID/Password).
2. **Sistema de Archivos (SD / SPIFFS / LittleFS):** Para **todos** los datos de aplicaciones, listas dinámicas, colecciones y registros estructurados, utilizando serialización eficiente como **MessagePack (msgpack)** o **TLV Binario**.

---

## ⚖️ 2. Comparativa Arquitectónica: NVS vs. Application Data Storage

| Criterio | NVS (KV-Store de Sistema) | Application Data Storage (MessagePack / FS) |
| :--- | :--- | :--- |
| **Medio de Almacenamiento** | Partición NVS en Flash interna (20 KB) | **Partición interna de Datos en Flash (2-4 MB)** + MicroSD (Opcional) |
| **Punto de Montaje** | N/A (Drivers NVS directos) | `/flash` (Interno autónomo) o `/sd` (MicroSD masiva) |
| **Tipo de Datos** | Parámetros escalares individuales de tamaño fijo | Colecciones dinámicas, structs complejos, listas de objetos |
| **Ejemplos de Datos** | Brillo, Volumen, Timeout, GMT Offset, Credenciales WiFi | Estaciones de Radio Favoritas, Tareas Todo, Notas de Texto, Historial |
| **Formato de Serialización** | Clave-Valor nativo (16 bytes max por clave) | **MessagePack binario** o **TLV binario** |
| **Autonomía sin MicroSD** | 100% Autónomo | **100% Autónomo** (usa la partición Flash interna `/flash`) |
| **Riesgo de Corrupción** | Crítico (si se corrompe NVS, el sistema no arranca) | Aislado a la aplicación (no afecta el arranque del OS) |

---

## 🏛️ 3. Mapeo Canónico con la Tabla de Particiones de 16 MB Flash

Tanto en el **ESP32-P4** como en el **ESP32-S3**, el mapa de memoria Flash interna de 16 MB (`16,777,216 bytes`) está distribuido de forma unificada:

```
┌───────────┬──────────────┬──────────────┬──────────────┬──────────────┬──────────────────┐
│ nvs (20K) │ otadata (8K) │ app0 (5 MB)  │ app1 (4 MB)  │ app2 (2 MB)  │ spiffs (~4.9 MB) │
│ (Ajustes) │ (Boot OTA)   │ (CBDos OS)   │ (Cart. Gran) │ (Cart. Ch.)  │ (Assets + Datos) │
└───────────┴──────────────┴──────────────┴──────────────┴──────────────┴──────────────────┘
```

### Tabla Oficial de Particiones (16 MB):

| Partición | Subtipo | Offset | Tamaño | Punto de Montaje | Propósito Oficial |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`nvs`** | `nvs` | `0x009000` | **20 KB** (`0x005000`) | N/A | Ajustes básicos de arranque (brillo, volumen, WiFi SSID/Pass). |
| **`otadata`** | `ota` | `0x00E000` | **8 KB** (`0x002000`) | N/A | Selector de partición de arranque OTA (`app0`, `app1`, `app2`). |
| **`app0`** | `ota_0` | `0x010000` | **5 MB** (`0x500000`) | N/A | **CBDos OS Firmware Principal**. |
| **`app1`** | `ota_1` | `0x510000` | **4 MB** (`0x400000`) | N/A | **Cartuchos Grandes** (Juegos pesados, Doom, emuladores nativos). |
| **`app2`** | `ota_2` | `0x910000` | **2 MB** (`0x200000`) | N/A | **Cartuchos Chicos** (Emulador GBC, utilidades compactas). |
| **`spiffs`** | `spiffs` | `0xB10000` | **~4.9 MB** (`0x4F0000`) | `/flash` o `/spiffs` | **Assets del sistema (wallpapers, fuentes) + Datos de Apps (MessagePack)**. |
| **MicroSD** | Externa | N/A | Variable | `/sd` | Almacenamiento masivo (archivos MP3, ROMs, backups). |

---

## 💾 4. Arquitectura del Modelo de Almacenamiento

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           CAPA DE APLICACIONES                          │
├────────────────────────────────────┬────────────────────────────────────┤
│           Ajustes de OS            │        Aplicación Radio Web        │
│    (Brillo, Volumen, WiFi, Tema)   │   (Lista de Estaciones Favoritas)  │
└──────────────────┬─────────────────┴──────────────────┬─────────────────┘
                   │                                    │
                   ▼ (Valores Escalares)                ▼ (Colección MessagePack)
┌────────────────────────────────────┐ ┌────────────────────────────────────┐
│      SystemConfigManager (NVS)     │ │   RadioFavoritesStorage (msgpack)  │
│       • Brightness (uint8)         │ │   • Station: {name, url, genre}    │
│       • Volume (uint8)             │ │   • /flash/data/radio_fav.msgpack  │
│       • ScreenTimeout (uint32)     │ └────────────────┬───────────────────┘
│       • WifiSsid / Password        │                  │
└──────────────────┬─────────────────┘                  ▼
                   │                     ┌──────────────────────────────────┐
                   ▼                     │    cbdos::storage (VFS)          │
┌────────────────────────────────────┐   │    • Sin SD -> /flash (spiffs)   │
│       IPersistenceBackend          │   │    • Con SD -> /sd (MicroSD)     │
│    (Abstracción KV-Store en core)  │   └──────────────────────────────────┘
└──────────────────┬─────────────────┘
                   │
     ┌─────────────┴─────────────┐
     ▼                           ▼
┌──────────────┐          ┌──────────────┐
│ EspIdfNvs    │          │ ArduinoNvs   │
│ (ESP32-P4)   │          │ (ESP32-S3)   │
└──────────────┘          └──────────────┘
```

---

## 💾 4. Especificación de los Dos Subsistemas

### 4.1. Subsistema 1: Configuración de Sistema (`SystemConfigManager` + `IPersistenceBackend`)
* **Ubicación:** `core/src/system/config/`
* **Contenido Exclusivo:**
  * `sys.brightness` (uint8_t: 0-100)
  * `sys.volume` (uint8_t: 0-100)
  * `sys.timeout_sec` (uint32_t)
  * `sys.theme` (std::string: "dark", "light")
  * `sys.gmt_offset` (int32_t)
  * `net.wifi_ssid` (std::string)
  * `net.wifi_pass` (std::string)
  * `net.wifi_auto` (bool)

* **Interfaz C++ (`core/include/cbdos/persistence.hpp`):**
```cpp
namespace cbdos {
namespace persistence {

class IPersistenceBackend {
public:
    virtual ~IPersistenceBackend() = default;
    virtual bool begin(const char* nameSpace, bool readOnly = false) = 0;
    virtual void end() = 0;
    virtual bool setUChar(const char* key, uint8_t value) = 0;
    virtual uint8_t getUChar(const char* key, uint8_t defaultVal = 0) = 0;
    virtual bool setInt(const char* key, int32_t value) = 0;
    virtual int32_t getInt(const char* key, int32_t defaultVal = 0) = 0;
    virtual bool setString(const char* key, const std::string& value) = 0;
    virtual std::string getString(const char* key, const std::string& defaultVal = "") = 0;
};

} // namespace persistence
} // namespace cbdos
```

---

### 4.2. Subsistema 2: Almacenamiento de Radio y Datos de Apps (MessagePack / Binario)
* **Ubicación:** `core/src/audio/RadioFavoritesStorage.hpp/.cpp`
* **Estructura de Datos de Estación:**
```cpp
struct RadioStation {
    std::string name;
    std::string streamUrl;
    std::string genre;
    int32_t bitrate;
};
```

* **Formato de Archivo en Disco:** `/sd/cbdos/data/radio/favorites.msgpack` (o fallback a `/spiffs/data/radio/favorites.msgpack`).
* **Esquema MessagePack:**
```
[
  { "name": "Ibiza Chill", "url": "http://stream.ibiza.fm/live", "genre": "Ambient", "br": 128 },
  { "name": "Retro Synth", "url": "http://stream.synth.fm/live", "genre": "Synthwave", "br": 192 }
]
```
* **Ventajas de MessagePack:**
  * Cero fragmentación de NVS.
  * Formato binario ultra-compacto (30-50% más pequeño que JSON).
  * Parseo instantáneo en streaming sin reservar grandes bloques de memoria RAM.
  * Si el usuario añade 100 estaciones, el archivo crece en la tarjeta SD de forma transparente sin poner en riesgo la memoria interna del microcontrolador.

---

## 🛠️ 5. Plan de Ejecución Refinado

### Fase 1: Limpieza Radical de `ConfigManager`
1. **Poda de `ConfigManager`:**
   * Eliminar de `ConfigManager` todo lo que no sea configuración escalar de sistema (extirpar favoritos de radio, extirpar tablas dinámicas).
   * Reducir `ConfigManager` de 1,045 líneas a menos de 200 líneas limpias de C++ estándar.
2. **Implementación de `IPersistenceBackend`:**
   * Crear la interfaz en `core/include/cbdos/persistence.hpp`.
   * Implementar `EspIdfNvsBackend.cpp` en `bsp/esp32_p4_jc4880`.
   * Implementar `ArduinoNvsBackend.cpp` en `bsp/esp32_s3_jc3248`.

### Fase 2: Almacenamiento MessagePack para Radio Web
1. **Crear `RadioFavoritesStorage`:**
   * Implementar lectura y escritura binaria MessagePack hacia el sistema de archivos (`/sd/cbdos/data/radio_favorites.msg`).
   * Desacoplar la UI de `RadioView` para que lea y escriba mediante este servicio de almacenamiento de disco.

### Fase 3: Desacoplamiento del Pipeline de Audio
1. **Separar `IAudioSink` (Hardware I2S) de `IAudioDecoder` (Helix MP3/AAC).**
2. **Centralizar el `HttpAudioStreamer` para consumo de flujos de radio por sockets.**

---

## 📋 6. Criterios de Aceptación y Validación

* [ ] `core/` no incluye ninguna cabecera de `<Preferences.h>`, `<Arduino.h>` ni `<nvs_flash.h>`.
* [ ] La partición NVS solo almacena pares clave-valor de configuración básica del OS.
* [ ] Las estaciones de radio favoritas se guardan y leen exclusivamente desde archivo en SD/Flash mediante serialización estructurada.
* [ ] Compilación multi-target limpia:
  * `idf.py build` (ESP32-P4).
  * `pio run -d bsp/esp32_s3_jc3248` (ESP32-S3).
