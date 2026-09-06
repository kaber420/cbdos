# 🏛️ Documento Maestro: Plan de Limpieza, Desacoplamiento y Refactorización por Fases (CBDos v0.2.1)

## 📌 1. Visión y Objetivo Arquitectónico

El objetivo fundamental de **CBDos** es consolidarse como un verdadero **Sistema Operativo Embebido Multi-Target**, desacoplado de las plataformas de compilación subyacentes (**ESP-IDF**, **Arduino**, **FreeRTOS** o **Simulador Nativo Linux**).

Actualmente, varias partes del código en `core/` y en las aplicaciones violan la **Ley de Pureza Arquitectónica** al incluir cabeceras de plataforma (`<Arduino.h>`, `<Preferences.h>`, `<esp_log.h>`, `<esp_heap_caps.h>`, `<WiFiClient.h>`, `<esp_http_client.h>`, `<esp_ota_ops.h>`, `<sys/socket.h>`, `<freertos/...>`) o al brincarse capas y acceder directamente al hardware o a sockets sin pasar por una API centralizada.

Este documento establece la **estrategia de ejecución dividida en 6 fases incrementales**, garantizando que el sistema compile limpiamente en los targets principales (**ESP32-P4**, **ESP32-S3** y **Linux/Simulador**) en cada paso del proceso.

---

## 🏗️ 2. Modelo de Capas Estricto de CBDos

```
┌────────────────────────────────────────────────────────────────────────┐
│                   CAPA 1: APPS & USER INTERFACE                        │
│   (RadioView, FileManagerView, MusicPlayer, LuaRunner, Cartridges)     │
│   • Cero dependencias de hardware o frameworks de red/RTOS.            │
│   • Solo consumen la API de Sistema de CBDos y LVGL 9.5.               │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Consume APIs de Sistema
┌───────────────────────────────────▼────────────────────────────────────┐
│                   CAPA 2: CBDOS SYSTEM API / SDK                       │
│   (cbdos::audio, cbdos::fs, cbdos::net, cbdos::sys, cbdos::time)       │
│   • Lógica de negocio agnóstica en C++17/20 puro.                      │
│   • Manejo de estado, pipelines de decodificación y orquestación.      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Consume Interfaces Abstractas
┌───────────────────────────────────▼────────────────────────────────────┐
│                   CAPA 3: CBDOS HAL (Contratos C++)                    │
│   (IAudioSink, IPersistenceBackend, INetworkAdapter, IStorageBackend,  │
│    IHttpClient, ISocketStream, IFlashPartitionManager, IOtaUpdater)    │
│   • Interfaces puras (`= 0`) sin implementaciones de hardware.         │
│   • Tipos opacos para RTOS y gestión agnóstica de memoria.             │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Implementado por
┌───────────────────────────────────▼────────────────────────────────────┐
│                   CAPA 4: BSP (Board Support Packages)                 │
│   ┌───────────────────────────────┬────────────────────────────────┐   │
│   │    ESP32-P4 (ESP-IDF 5.5)     │    ESP32-S3 (Arduino/PIO)      │   │
│   │ • Drivers ES8311, ST7701S     │ • Drivers I2S, AXS15231B       │   │
│   │ • C6 SDIO ESP-Hosted Driver   │ • Native WiFi / Sockets Driver │   │
│   │ • NVS Flash Provider          │ • Preferences NVS Provider     │   │
│   │ • LwIP Sockets & FreeRTOS     │ • Arduino WiFiClient & RTOS    │   │
│   └───────────────────────────────┴────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 📋 3. Matriz Exhaustiva de Infracciones Detectadas en `core/`

| Módulo / Archivo | Infracción Actual | Capa Saltada | Solución Arquitectónica |
| :--- | :--- | :--- | :--- |
| **`core/src/audio/RadioManager.cpp`** | `#include <Arduino.h>`, `<WiFiClient.h>`, `<esp_http_client.h>`, `<esp_log.h>` | Capa 2/3 directas a frameworks | Inyectar `IHttpClient*` o `ISocketStream*` desde el BSP. |
| **`core/src/audio/AudioPlayer.cpp`** | Sockets BSD directos (`socket()`, `connect()`, `recv()`), `<esp_log.h>`, FreeRTOS | Saltó a sockets raw y RTOS | Separar streaming HTTP a `ISocketStream` y logging a `cbdos::log`. |
| **`core/src/ui/views/TlvBrowserView.cpp`** | Sockets POSIX raw (`gethostbyname`, `socket()`, `setsockopt()`), `<esp_log.h>` | Capa 1 a Capa 4 directa | Usar cliente de red `ISocketStream` / `cbdos::net`. |
| **`core/src/cartridge/CartridgeManager.cpp`** | `#include <Arduino.h>`, `<esp_partition.h>`, `<esp_ota_ops.h>`, `<esp_system.h>` | Saltó a particiones y OTA de ESP-IDF | Abstraer en `IFlashPartitionManager` e `IOtaUpdater`. |
| **`core/src/ui/WallpaperManager.h/.cpp`** | `#include <Arduino.h>`, `<esp_heap_caps.h>`, `SD.h`, `LittleFS.h` | Saltó a framework y memoria directa | Usar `cbdos::mem` y `cbdos::storage` / `cbdos::fs`. |
| **`core/src/lua/LuaEngine.cpp`** | `#include <esp_heap_caps.h>`, alloc PSRAM hardcoded | Saltó a asignador de ESP-IDF | Usar abstraction `cbdos::mem::alloc_psram()`. |
| **`core/src/network/ConfigManager.cpp`** | `#include <Preferences.h>`, `#ifdef ARDUINO` | NVS acoplado a Arduino | Migrar completamente a `IPersistenceBackend`. |
| **`core/src/ui/views/utilities/TodoApp.cpp`**| `#include <esp_log.h>` | Logging acoplado a ESP-IDF | Reemplazar por `cbdos::log`. |
| **Cabeceras públicas `core/include/`** | Tipos directos de FreeRTOS (`SemaphoreHandle_t`, `TaskHandle_t`) | Contaminación de API pública | Encapsular en tipos opacos en `cbdos/rtos.hpp`. |

---

## 🧩 4. Catálogo de Interfaces HAL Requeridas (`core/include/cbdos/`)

1. **`cbdos/memory.hpp`**: Funciones `alloc_psram()`, `alloc_dma()`, `alloc_internal()`, `free()`.
2. **`cbdos/log.hpp`**: Macros agnósticas `CBD_LOG_I`, `CBD_LOG_W`, `CBD_LOG_E`, `CBD_LOG_D`.
3. **`cbdos/rtos.hpp`**: Abstracciones opacas para hilos, mutex y semáforos (`MutexHandle`, `TaskHandle`).
4. **`cbdos/net/IHttpClient.hpp`**: Métodos `get()`, `post()`, manejo de cabeceras HTTP.
5. **`cbdos/net/ISocketStream.hpp`**: Conexión TCP/UDP por flujo de bytes (`connect()`, `read()`, `write()`, `close()`).
6. **`cbdos/system/IFlashPartitionManager.hpp`**: Consulta e iteración de particiones Flash.
7. **`cbdos/system/IOtaUpdater.hpp`**: Escritura y validación de slots de arranque/firmware.

---

## 🗓️ 5. Plan de Ejecución por Fases

---

### 🔹 FASE 1: Saneamiento de Infraestructura Base (Memoria, Logging & RTOS)
**Objetivo:** Erradicar `<esp_log.h>`, `<esp_heap_caps.h>` y tipos raw de FreeRTOS en las cabeceras públicas de `core/`.

#### Tareas Técnicas:
1. **Consolidación de `cbdos/log.hpp`:**
   - Crear macros estándar mapeadas según plataforma (ESP_LOG en ESP-IDF, Serial en Arduino, stdout en Linux).
2. **Consolidación de `cbdos/memory.hpp`:**
   - Implementar `cbdos::mem::alloc_psram()`, `alloc_dma()`, `alloc_internal()`, `free()`.
3. **Creación de `cbdos/rtos.hpp`:**
   - Tipos opacos y wrappers para `MutexHandle`, `TaskHandle`, `sleepMs()`.
4. **Refactorización de archivos:**
   - Reemplazar includes en `LuaEngine.cpp`, `VideoPlayerView.cpp`, `TlvBrowserView.cpp`, `AudioPlayer.cpp`, `WavPlayer.cpp`, `Mp4Parser.cpp`, `TodoApp.cpp`, `libh264/h264bsd_util.h`.

---

### 🔹 FASE 2: Desacoplamiento de Persistencia y NVS
**Objetivo:** Erradicar `<Preferences.h>` y `#ifdef ARDUINO` de `core/src/network/ConfigManager.cpp`.

#### Tareas Técnicas:
1. **Alineación con `IPersistenceBackend`:**
   - Asegurar que `ConfigManager` interactúe exclusivamente con la interfaz abstracta `cbdos::persistence::IPersistenceBackend`.
2. **Reubicación de `ConfigManager`:**
   - Mover `core/src/network/ConfigManager.*` hacia `core/src/system/SystemConfigManager.*`.
3. **Implementación de Backends en BSP:**
   - Target ESP32-P4: backend nativo NVS (`nvs_flash`).
   - Target ESP32-S3: backend `PreferencesNvsBackend`.

---

### 🔹 FASE 3: Capa de Red, Sockets y Streaming (HTTP / TCP)
**Objetivo:** Eliminar todo uso de sockets POSIX raw (`sys/socket.h`), `<WiFiClient.h>`, `<esp_http_client.h>` en `core/`.

#### Tareas Técnicas:
1. **Definición de Contratos HAL:**
   - Implementar interfaces `IHttpClient.hpp` y `ISocketStream.hpp`.
2. **Refactorización de Consumidores en `core/`:**
   - `RadioManager.cpp`: Inyectar `IHttpClient*` o `ISocketStream*`.
   - `AudioPlayer.cpp`: Extraer `runStreamPlayback` para que utilice `ISocketStream`.
   - `TlvBrowserView.cpp`: Eliminar llamadas directas a `socket()`, `connect()`, `gethostbyname()` y usar `ISocketStream`.
3. **Implementaciones en BSP:**
   - BSP ESP32-S3: Adaptador sobre WiFiClient / Arduino Network.
   - BSP ESP32-P4: Adaptador sobre LwIP / ESP-Hosted SDIO.

---

### 🔹 FASE 4: Purificación de Particiones Flash, Vistas y Cartuchos
**Objetivo:** Eliminar dependencias de Flash/OTA (`esp_ota_ops.h`, `esp_partition.h`) y `<Arduino.h>` en vistas.

#### Tareas Técnicas:
1. **`CartridgeManager`:**
   - Implementar y conectar `IFlashPartitionManager` e `IOtaUpdater`.
   - Eliminar llamadas directas a `esp_ota_*` y `esp_partition_*` en `core/`.
2. **`WallpaperManager`:**
   - Retirar `<Arduino.h>` y `<esp_heap_caps.h>`. Usar `cbdos::mem` y `cbdos::storage`.
3. **Auditoría de Vistas LVGL:**
   - Verificar que las 27 vistas en `core/src/ui/views/` no incluyan cabeceras ajenas a `core/include/`.

---

### 🔹 FASE 5: Formalización del App Framework y Catálogo Syscall
**Objetivo:** Crear una frontera inquebrantable entre las Apps y los servicios del Kernel.

#### Tareas Técnicas:
1. **Definición de Syscalls / Servicios Públicos:**
   - Publicar el catálogo formal en `core/include/cbdos/system_api.hpp`:
     - `cbdos::audio::play()`, `cbdos::audio::stop()`, `cbdos::audio::setVolume()`
     - `cbdos::fs::open()`, `cbdos::fs::listDir()`
     - `cbdos::net::httpGet()`, `cbdos::net::tcpConnect()`
     - `cbdos::sys::getBatteryLevel()`, `cbdos::sys::getBrightness()`
2. **Aislamiento de la UI:**
   - Todas las vistas heredan de `BaseView` y se comunican a través del `EventBus` o de `SystemApi`.

---

### 🔹 FASE 6: Formalización de BSPs, Tests y CI Multi-Target
**Objetivo:** Garantizar que ninguna regresión arquitectónica vuelva a entrar al repositorio.

#### Tareas Técnicas:
1. **Consolidación de BSPs:**
   - `bsp/esp32_p4_jc4880`: Registra todos los drivers ESP-IDF en el arranque.
   - `bsp/esp32_s3_jc3248`: Registra todos los drivers Arduino/PlatformIO en el arranque.
2. **Script de Auditoría Automática:**
   - Crear `./scripts/verify_architecture.sh` para verificar en segundos que `core/` no tiene includes ilegales.
3. **Validación de Compilación Cruzada:**
   - ESP32-P4 (`idf.py build`), ESP32-S3 (`pio run`) y Target Simulador Linux (`cmake`).

---

## ✅ 6. Criterios de Validación y Calidad

```bash
# 1. Verificación de Cero Inclusiones Prohibidas en core/
grep -rnE "#include <(Arduino\.h|WiFiClient\.h|Preferences\.h|esp_log\.h|esp_heap_caps\.h|esp_http_client\.h|esp_system\.h|esp_ota_ops\.h|esp_partition\.h|driver/|FreeRTOS\.h|freertos/|WiFi\.h|SPI\.h|Wire\.h|HTTPClient\.h|cJSON\.h|sys/socket\.h|netdb\.h|arpa/inet\.h|netinet/in\.h)>" core/

# 2. Verificación de Cero Tipos FreeRTOS en cabeceras públicas
grep -rnE "(SemaphoreHandle_t|TaskHandle_t|QueueHandle_t|TickType_t|BaseType_t|pdMS_TO_TICKS|portMAX_DELAY|xTaskCreate|vTaskDelete|vSemaphoreDelete|xSemaphoreCreate)" core/include/cbdos/

# 3. Compilación Multi-Target Limpia:
# ESP32-P4
. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build
# ESP32-S3
pio run -d bsp/esp32_s3_jc3248
```
