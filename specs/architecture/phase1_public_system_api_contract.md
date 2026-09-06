# Fase 1: El Contrato Público del Sistema (API de CBDos)

> **Estado:** Documento Oficial de Arquitectura — Fase 1  
> **Versión:** CBDos v0.2.1  
> **Área:** `core/include/cbdos/`  

---

## 1. Visión y Propósito

Para convertir a CBDos en un sistema operativo embebido modular, predecible y escalable a cualquier arquitectura de hardware (ESP32-S3, ESP32-P4, simulador de escritorio o Linux/ARM64), es indispensable establecer una **barrera arquitectónica infranqueable** entre las aplicaciones y los servicios internos del sistema.

### 🏛️ La Jerarquía de Capas de CBDos

```
┌────────────────────────────────────────────────────────────────────────┐
│                        APLICACIONES / VISTAS                           │
│  TlvBrowserView   TextEditor   MediaPlayer   Clock   MeshConfigView    │
│  (Solo consumen cbdos:: namespace, no conocen hardware ni plataformas) │
├────────────────────────────────────────────────────────────────────────┤
│                  API PÚBLICA DEL SISTEMA (Contrato)                    │
│                 Cabeceras en: core/include/cbdos/*.hpp                 │
│                                                                        │
│   cbdos::display   cbdos::time   cbdos::audio   cbdos::net             │
│   cbdos::storage   cbdos::input  cbdos::system  cbdos::ui              │
├────────────────────────────────────────────────────────────────────────┤
│                 SERVICIOS INTERNOS DEL SISTEMA (Core)                  │
│                      Implementación en: core/src/                      │
│                                                                        │
│   TimeManager   MeshEngine   ConfigManager   UIManager   AudioManager  │
│   (Se comunican mediante eventos/callbacks inyectados en el arranque)  │
├────────────────────────────────────────────────────────────────────────┤
│                   HAL — Interfaces Abstractas C++                      │
│                                                                        │
│   ITimeProvider   IAudioSink   IMeshTransport   IStorageBackend        │
├────────────────────────────────────────────────────────────────────────┤
│                 DRIVERS DE PLATAFORMA / BSP (bsp/*/)                   │
│                                                                        │
│   EspIdfTimeProvider   ES8311Sink   EspNowTransport   SdmmcBackend     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Reglas del Contrato Público (Ley de Aislamiento)

1. **Agnosticismo Absoluto:** Las cabeceras en `core/include/cbdos/*.hpp` son C++ estándar y LVGL 9.5 puro. Tienen estrictamente prohibido incluir cabeceras de plataforma (`<Arduino.h>`, `<driver/...>`, `<esp_*.h>`).
2. **Encapsulamiento Total:** Las aplicaciones **nunca** interactúan con punteros de drivers, estructuras internas de reensamblado ni dependencias de bajo nivel.
3. **Puntos de Entrada Únicos:** Cada subsistema expone un único espacio de nombres claro y coherente (`namespace cbdos::<modulo>`).

---

## 3. Catálogo Oficial de APIs Públicas (`core/include/cbdos/`)

A continuación se define el contrato formal que todas las aplicaciones y componentes de interfaz tienen garantizado:

### 3.1 `cbdos::time` ([time.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/time.hpp))
Gestión del reloj central del sistema, zonas horarias y consulta de sincronización pasiva o activa.

```cpp
namespace cbdos {
namespace time {

enum class TimeSource {
    None,   // Sin sincronizar (reloj base en cero)
    Local,  // Ajustado manualmente por el usuario
    SNTP,   // Sincronizado vía Wi-Fi hacia pool.ntp.org
    RTC,    // Sincronizado por chip RTC externo
    Tower   // Sincronizado pasivamente por Micro-Broadcast PoP (Radio ESP-NOW / LoRa)
};

// API de Lectura para Aplicaciones
bool isSynced();
time_t getEpoch();
TimeSource getSource();
void getFormattedTime(char* buf, size_t len, const char* format = "%H:%M");
void getFormattedDate(char* buf, size_t len, const char* format = "%d/%m/%Y");

// API de Configuración
void setTimezone(long gmtOffsetSec, int daylightOffsetSec);
void setAutoSyncEnabled(bool enabled);
bool isAutoSyncEnabled();
void syncNtp(); // Fuerza intento de sincronización SNTP si hay Wi-Fi conectado

} // namespace time
} // namespace cbdos
```

---

### 3.2 `cbdos::display` ([display.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/display.hpp))
Control de la pantalla, retroiluminación y exclusión mutua para renderizado seguro en hilos FreeRTOS.

```cpp
namespace cbdos {
namespace display {

uint16_t getWidth();
uint16_t getHeight();
void setBrightness(uint8_t percent); // 0 .. 100%
uint8_t getBrightness();

// Mutex de LVGL (Thread Safety)
bool lock(uint32_t timeout_ms = 100);
void unlock();

} // namespace display
} // namespace cbdos
```

---

### 3.3 `cbdos::audio` ([audio.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/audio.hpp))
Reproductor de medios, control de volumen maestro y reproducción de tonos/efectos de interfaz.

```cpp
namespace cbdos {
namespace audio {

enum class PlaybackState { Stopped, Playing, Paused, Error };

bool playFile(const char* path);
void pause();
void resume();
void stop();
PlaybackState getState();

void setVolume(uint8_t volume); // 0 .. 100%
uint8_t getVolume();

uint32_t getCurrentTimeSec();
uint32_t getTotalTimeSec();

} // namespace audio
} // namespace cbdos
```

---

### 3.4 `cbdos::storage` ([storage.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/storage.hpp))
Acceso al sistema de archivos VFS (MicroSD / Flash interna), metadatos de almacenamiento y detección de medios.

```cpp
namespace cbdos {
namespace storage {

bool isMounted();
uint64_t getTotalBytes();
uint64_t getUsedBytes();
uint64_t getFreeBytes();
const char* getMountPoint(); // Ej: "/sdcard"

// Helpers de conveniencia
bool fileExists(const char* path);
bool createDirectory(const char* path);
bool removeFile(const char* path);

} // namespace storage
} // namespace cbdos
```

---

### 3.5 `cbdos::network` ([network.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/network.hpp))
Estado de conectividad TCP/IP tradicional (Wi-Fi Estación / AP).

```cpp
namespace cbdos {
namespace network {

bool isConnected();
bool isWifiEnabled();
const char* getIpAddress();
const char* getSSID();
int8_t getRssi();

} // namespace network
} // namespace cbdos
```

---

### 3.6 `cbdos::system` ([system.hpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/system.hpp))
Servicios base del sistema operativo: timers de alta resolución, logs estructurados, gestión de memoria heap/PSRAM y reinicio seguro.

```cpp
namespace cbdos {
namespace system {

enum class LogLevel { Debug, Info, Warn, Error };

uint32_t getTimeMs();
uint64_t getTimeUs();
void sleepMs(uint32_t ms);

size_t getFreeHeap();
size_t getFreePsram();

void log(LogLevel level, const char* tag, const char* format, ...);
void restart();

} // namespace system
} // namespace cbdos
```

---

## 4. Próximos Pasos en la Fase 1

1. **Creación de `cbdos.hpp` (Cabecera Maestra de Apps):**
   - Permitirá a cualquier aplicación incluir `#include <cbdos/cbdos.hpp>` para tener acceso inmediato a todas las APIs públicas del sistema.
2. **Creación del Inicializador Central `core/src/cbdos_core.cpp`:**
   - Centralizará el arranque y la interconexión de servicios del OS (Time ↔ Network ↔ UI) eliminando la necesidad de que los `main.cpp` de los BSPs manipulen la lógica interna.
3. **Verificación de Cero Contaminación:**
   - Auditoría automatizada de includes para garantizar que ninguna vista de UI acceda a clases internas o headers de hardware.
