# 🏛️ Especificación Técnica: Fase 4 - Desacoplamiento de Particiones Flash, Actualizador OTA y Gestor de Cartuchos (`IFlashPartitionManager`) (CBDos v0.2.1)

**Documento:** `docs/architecture/especificacion_tecnica_fase_cuatro_particiones_ota_y_cartuchos.md`  
**Versión:** 1.0.0  
**Estado:** Documento de Especificación Técnica para Evaluación y Aprobación  
**Autor:** Equipo de Arquitectura de Software CBDos  
**Fecha:** Agosto 2026  

---

## 📌 1. Visión General y Justificación Arquitectónica

La **Fase 4** del roadmap de refactorización y desacoplamiento de **CBDos v0.2.1** aborda la eliminación de las dependencias directas del subsistema de particionado de memoria Flash y las APIs de actualización OTA de ESP-IDF (`<esp_partition.h>`, `<esp_ota_ops.h>`, `<esp_app_format.h>`) del núcleo agnóstico `core/`.

Actualmente, el gestor de cartuchos binarios (`CartridgeManager`) y la vista visual de ranuras de aplicaciones (`CartridgeView`) contienen:
1. Inclusiones directas de cabeceras de bajo nivel de ESP-IDF en headers públicos (`core/include/cbdos/cartridge.hpp`).
2. Exposición de tipos y macros propietarias de ESP-IDF en el espacio de nombres de la UI (`esp_partition_subtype_t`, `ESP_PARTITION_SUBTYPE_APP_OTA_1`, `ESP_APP_DESC_MAGIC_WORD`).
3. Bifurcaciones condicionales `#ifdef ESP_PLATFORM` con código de simulación mezclado con la lógica de negocio en `core/src/cartridge/CartridgeManager.cpp`.

El objetivo de esta fase es encapsular estas operaciones bajo el contrato abstracto **`IFlashPartitionManager`**, permitiendo que cualquier target (ESP32-P4, ESP32-S3 o el futuro simulador de escritorio en Linux) gestione slots de firmware, valide binarios, informe metadatos de compilación y realice flasheos seguros sin contaminar la capa agnóstica de `core/`.

---

## 🏛️ 2. Diagrama de Arquitectura de Particiones y Cartuchos

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          UI & APPS (CBDos Core)                             │
│       CartridgeView  •  FileManagerView  •  ConfigView / OTA                │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼ (API Pública de Cartuchos)
┌─────────────────────────────────────────────────────────────────────────────┐
│                  DESPACHADOR AGNÓSTICO: cbdos::cartridge::*                 │
│                        (core/src/cartridge/cartridge.cpp)                   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                                       ▼ (Contrato HAL C++ Puro)
┌─────────────────────────────────────────────────────────────────────────────┐
│             CONTRATO HAL: cbdos::cartridge::IFlashPartitionManager          │
│                                                                             │
│  • virtual SlotInfo getSlotInfo(SlotId slot) = 0;                           │
│  • virtual bool isSlotInstalled(SlotId slot) = 0;                           │
│  • virtual bool setBootSlot(SlotId slot) = 0;                               │
│  • virtual bool flashSlot(SlotId targetSlot, FILE* binFile, size_t len,    │
│                           ProgressCallback progressCb) = 0;                 │
│  • virtual bool eraseSlot(SlotId slot) = 0;                                 │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                    ┌──────────────────┴──────────────────┐
                    ▼                                     ▼
┌──────────────────────────────────────┐┌─────────────────────────────────────┐
│     bsp/esp32_p4_jc4880 (ESP-IDF)    ││     bsp/esp32_s3_jc3248 (Arduino)   │
├──────────────────────────────────────┤├─────────────────────────────────────┤
│  P4FlashPartitionManager             ││  S3FlashPartitionManager            │
│  • esp_partition_find_first()        ││  • esp_partition_find()             │
│  • esp_ota_get_partition_description ││  • UpdateClass (Arduino Core)       │
│  • esp_ota_begin / write / end       ││  • Particiones OTA0/OTA1 en SPI Flash│
│  • esp_ota_set_boot_partition()      ││  • Boot Switch NVS / OTA            │
└──────────────────────────────────────┘└─────────────────────────────────────┘
```

---

## 📜 3. Especificación Detallada de Contratos HAL

### 3.1. Tipos y Enumeraciones Agnósticas (`core/include/cbdos/cartridge.hpp`)

Se erradican por completo las referencias a `esp_partition_subtype_t` y se define un identificador de ranura fuertemente tipado:

```cpp
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace cbdos {
namespace cartridge {

// ────────────────────────────────────────────────────────────────
// Identificador de Ranuras de Cartuchos / Particiones OTA
// ────────────────────────────────────────────────────────────────
enum class SlotId : uint8_t {
    Recovery = 0, // Ranura Base del Sistema Operativo (app0 / Factory)
    Slot1    = 1, // Ranura 1 de Aplicación / Juego (app1 / OTA_1)
    Slot2    = 2, // Ranura 2 de Aplicación / Juego (app2 / OTA_2)
    Custom   = 3  // Ranura Personalizada / Expansión
};

// ────────────────────────────────────────────────────────────────
// Metadatos de un Cartucho / Slot
// ────────────────────────────────────────────────────────────────
struct CartridgeSlotInfo {
    SlotId slotId = SlotId::Slot1;
    bool isInstalled = false;
    std::string projectName = "Ranura Vacía";
    std::string version = "";
    std::string compileDate = "";
    std::string compileTime = "";
    size_t partitionSize = 0;
    size_t usedBytes = 0;
};

using FlashProgressCallback = std::function<void(size_t written, size_t total)>;

// ────────────────────────────────────────────────────────────────
// Contrato HAL C++ Puro para Gestión de Particiones Flash y OTA
// ────────────────────────────────────────────────────────────────
class IFlashPartitionManager {
public:
    virtual ~IFlashPartitionManager() = default;

    // Consulta de información y estado de una ranura
    virtual CartridgeSlotInfo getSlotInfo(SlotId slot) = 0;
    virtual bool isSlotInstalled(SlotId slot) = 0;

    // Configuración de arranque
    virtual bool setBootSlot(SlotId slot) = 0;

    // Flasheo desde flujo de datos (archivo binario en SD o memoria)
    virtual bool flashSlot(SlotId targetSlot, FILE* binaryStream, size_t totalBytes, FlashProgressCallback progressCb = nullptr) = 0;

    // Borrado de ranura
    virtual bool eraseSlot(SlotId slot) = 0;
};

// ────────────────────────────────────────────────────────────────
// Funciones de Inyección y Acceso Global en Core
// ────────────────────────────────────────────────────────────────
void setPartitionManager(IFlashPartitionManager* manager);
IFlashPartitionManager* getPartitionManager();

// Helper de conveniencia de alto nivel
CartridgeSlotInfo getSlotInfo(SlotId slot);
bool isSlotInstalled(SlotId slot);
bool bootSlot(SlotId slot);
std::vector<std::string> listBinFilesOnSD(const std::string& directory = "/sdcard/cartridges");
bool flashFromSD(const std::string& sdPath, SlotId targetSlot, FlashProgressCallback progressCb = nullptr);

} // namespace cartridge
} // namespace cbdos
```

---

## 🔧 4. Mapeo de Particiones por Plataforma

### 4.1. Target ESP32-P4 (JC4880P443C - 16 MB Flash)

La tabla de particiones estándar `partitions.csv` del BSP P4 se mapea a los identificadores lógicos `SlotId` de la siguiente manera:

| Label | Tipo / Subtipo | Offset | Tamaño | Mapeo `SlotId` | Propósito |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `nvs` | `data / nvs` | `0x00009000` | 20 KB | — | Preferencias NVS persistentes |
| `otadata`| `data / ota` | `0x0000e000` | 8 KB | — | Puntero de arranque de ESP-IDF |
| `app0` | `app / ota_0` | `0x00010000` | 5 MB | `SlotId::Recovery` | CBDos Core (Sistema Operativo Base) |
| `app1` | `app / ota_1` | `0x00510000` | 4 MB | `SlotId::Slot1` | Ranura Cartucho 1 (Doom, Emuladores, Apps) |
| `app2` | `app / ota_2` | `0x00910000` | 2 MB | `SlotId::Slot2` | Ranura Cartucho 2 (Juegos Ligeros / Runtimes) |
| `spiffs`| `data / spiffs`| `0x00b10000` | 4 MB | — | Almacenamiento Flash interno `/spiffs` |
| `fatfs` | `data / fat` | `0x00f10000` | 896 KB | — | Reservado para partición FAT de sistema |

### 4.2. Target ESP32-S3 (JC3248W535 - 16 MB Flash)

En el target ESP32-S3 con framework Arduino (pioarduino), `UpdateClass` y `esp_partition` gestionan el cambio de arranque entre `app0` (CBDos) y `app1` (Cartucho), garantizando que el usuario pueda lanzar cartuchos sin riesgo de brickeo.

---

## 📋 5. Plan de Ejecución por Sub-Fases (Fase 4)

---

### 🔹 Sub-Fase 4.1: Contrato HAL y Adaptadores BSP (`IFlashPartitionManager`)

#### 1. Implementación en `core/`
- Actualizar `core/include/cbdos/cartridge.hpp` purgando headers `<esp_partition.h>` y `<esp_ota_ops.h>`.
- Crear `core/src/cartridge/cartridge.cpp` con el despachador de la factoría e implementación de `listBinFilesOnSD()`.
- Actualizar `core/CMakeLists.txt` registrando `src/cartridge/cartridge.cpp`.

#### 2. Implementación en BSP ESP32-P4 (`bsp/esp32_p4_jc4880/hal/hal_cartridge_p4.cpp`)
- Implementar `P4FlashPartitionManager` utilizando `esp_partition_find_first()`, `esp_ota_get_partition_description()`, `esp_ota_begin()`, `esp_ota_write()`, `esp_ota_end()` y `esp_ota_set_boot_partition()`.
- Registrar e inyectar `cbdos::bsp::initCartridgeBackendP4()` en `bsp/esp32_p4_jc4880/main/main.cpp`.

#### 3. Implementación en BSP ESP32-S3 (`bsp/esp32_s3_jc3248/hal/hal_cartridge_s3.cpp`)
- Implementar `S3FlashPartitionManager` utilizando las APIs nativas de particiones de ESP32 bajo Arduino.
- Registrar e inyectar `cbdos::bsp::initCartridgeBackendS3()` en `bsp/esp32_s3_jc3248/src/main.cpp`.

---

### 🔹 Sub-Fase 4.2: Migración de `CartridgeView` en Core

#### Modificaciones en `core/src/ui/views/CartridgeView.hpp` y `CartridgeView.cpp`:
- Reemplazar `esp_partition_subtype_t` por `cbdos::cartridge::SlotId`.
- Actualizar los callbacks de eventos de LVGL para pasar el enum `SlotId` casteado limpiamente como `(void*)(uintptr_t)slotId`.
- Mantener intacto el selector de archivos `.bin` desde MicroSD, barras de progreso de flasheo y diálogos de confirmación de reinicio.

---

### 🔹 Sub-Fase 4.3: Auditoría y Limpieza de Vistas UI de Core

Auditar las 27 vistas de UI registradas en `core/src/ui/views/` para verificar que ninguna incluya encabezados de plataforma:
- `ConfigView.cpp` (Ajustes de sistema)
- `WiFiConfigView.cpp` (Gestión Wi-Fi)
- `FlasherView.cpp` (Flasheador Coprocesador C6 / UART JP1)
- `WallpaperManager.cpp` y `WallpaperConfigView.cpp`
- `StorageConfigView.cpp` y `FileManagerView.cpp`

---

### 🔹 Sub-Fase 4.4: Bucle de Validación y Compilación Cruzada

```bash
# 1. Compilación ESP32-P4 (ESP-IDF 5.5)
. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build

# 2. Compilación ESP32-S3 (PlatformIO)
pio run -d bsp/esp32_s3_jc3248

# 3. Auditoría de Cero Includes Prohibidos en Core
grep -rnE "#include <(esp_partition\.h|esp_ota_ops\.h|esp_app_format\.h)>" core/
```

---

## ✅ 6. Criterios de Aceptación y Éxito de la Fase 4

1. **Cero Violaciones de Headers:** `core/` no incluye ningún archivo `<esp_partition.h>`, `<esp_ota_ops.h>` ni `<esp_app_format.h>`.
2. **Cero Tipos Crudos:** `CartridgeView` y `CartridgeManager` operan únicamente con el tipo fuertemente tipado `SlotId` y estructuras agnósticas.
3. **100% Compilación Multi-Target:** Compilación limpia en ESP-IDF (P4) y PlatformIO (S3).
4. **Preservación Total de Funcionalidad:** La lectura de metadatos de firmware, el flasheo de archivos `.bin` desde la MicroSD con barra de progreso y el reinicio hacia la partición seleccionada funcionan al 100% en el hardware real.
