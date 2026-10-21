# 💽🎬 Propuesta Formal: Unidad USB Virtual Selectiva (Virtual FAT16 MSC) y Reproductor de Video (`.avi` MJPEG / `.mpg` MPEG-1)

**Fecha:** 21 de Octubre de 2026  
**Estado:** 💡 Propuesta Formal  
**Categoría:** Seguridad Táctica (BadUSB / Transferencia Segura) & Multimedia de Alto Rendimiento  
**Código de Referencia Auditado (`references/NucleoOS-P4/`):**
- **Unidad USB Virtual:** `components/nv_usb/nv_usb_msc.c` (276 líneas C) + `include/nv_usb.h`
- **Reproductor de Video:** `components/nv_vplayer/nv_vplayer.c`, `vp_avi.c/h`, `nv_mpeg1_impl.c` + `pl_mpeg.h`

---

## 1. Visión General y Justificación Técnica

Al auditar los subsistemas de bajo nivel de `NucleoOS-P4`, destacan dos módulos en C puro que son directamente portables a **CBDos** y resuelven casos de uso clave sin inflar la memoria ni comprometer la arquitectura:

### 1.1. Unidad USB Virtual Selectiva (`VirtualUsbDrive` — Sintetizador FAT16 en Vivo)
- **Problema actual en modos USB:** Exponer la tarjeta MicroSD completa por USB Mass Storage (MSC) a una computadora ajena o servidor bajo auditoría es un **riesgo de seguridad** (el host puede leer todas nuestras claves SSH, scripts, capturas o corromper el sistema de archivos FAT32 si ambos intentan escribir a la vez).
- **Cómo lo resuelve `nv_usb_msc.c` (276 líneas C):**
  - En lugar de montar la partición física de la MicroSD, **sintetiza un volumen FAT16 virtual de 32 MB en tiempo real sector por sector (`512 bytes`)** cada vez que el host USB pide leer un bloque SCSI (`tud_msc_read10_cb`).
  - Solo consume **16 KB de RAM (`ROOT_SECTORS * 512`)** para el directorio raíz (con soporte de nombres largos LFN) y **cero bytes duplicados para los archivos**: el contenido de cada archivo publicado se lee al vuelo desde un buffer en RAM o haciendo `fseek/fread` sobre un único archivo autorizado de la MicroSD.
- **Casos de Uso en CBDos:**
  1. **Payloads Híbridos BadUSB (HID + Pendrive Virtual):** Desde un script Lua o DuckyScript, publicar instantáneamente un solo ejecutable, script `.ps1`/`.sh` o certificado en el pendrive virtual (`CBDOS_DRIVE`), y usar `cbdos.hid` para ejecutarlo en la máquina objetivo en segundos, sin exponer el resto de la MicroSD.
  2. **Entrega Segura de Herramientas de Red en Campo:** Conectar CBDos por USB a un PC o servidor sin red para pasarle únicamente un firmware de router, un backup de configuración o un paquete de diagnóstico seleccionado desde el explorador de archivos.

### 1.2. Reproductor de Video Hardware/Software (`.avi` MJPEG y `.mpg` MPEG-1)
- **Cómo lo resuelve `nv_vplayer`:**
  1. **MJPEG en contenedor `.avi` con Audio PCM (`vp_avi.c`):** Lee frames completos alineados a sectores de 512 bytes con 3 buffers en anillo (`VP_ISLOTS = 3`) y los decodifica con el **Hardware JPEG Decoder + PPA** en ESP32-P4 (logrando **29.9 FPS reales en 1024×576 / 1280×720** con audio sincronizado).
  2. **MPEG-1 en contenedor `.mpg` (`pl_mpeg.h` + `nv_mpeg1_impl.c`):** Usa la biblioteca single-header `pl_mpeg.h` con planos YUV alojados en SRAM interna para decodificar video MPEG-1 + audio MP2 por software (entre **20 y 46 FPS en 320×240 / 480×320**), lo que permite que funcione **tanto en ESP32-P4 como en ESP32-S3 (`jc3248`)**.
- **Casos de Uso en CBDos:**
  - Reproducir grabaciones de cámaras de seguridad IP/DVR descargadas en campo, tutoriales técnicos offline, animaciones de arranque o contenido multimedia en la MicroSD.

---

## 2. Arquitectura e Integración en `CBDos` (`core/` y `bsp/`)

### 2.1. Subsistema 1: `cbdos::usb::VirtualDrive` (Basado en `nv_usb_msc.c`)

El código de `references/NucleoOS-P4/components/nv_usb/nv_usb_msc.c` utiliza directamente los callbacks estándar de **TinyUSB (`tud_msc_*`)**, que es exactamente el mismo stack USB que ya usamos en CBDos para `ESP32-S3` y `ESP32-P4`.

#### Contrato C++ en `core/include/cbdos/virtual_drive.hpp`
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace cbdos {
namespace usb {

struct VirtualFileEntry {
    std::string virtualName;   // Nombre visible en la PC (ej. "setup_agent.ps1" o "router_fw.bin")
    std::string sdSourcePath;  // Ruta real en la MicroSD (si viene de disco)
    const uint8_t* memData = nullptr; // O puntero a buffer en RAM/Flash (si es generado en vivo)
    uint32_t sizeBytes = 0;
};

class VirtualDrive {
public:
    static constexpr size_t kMaxPublishedFiles = 12;

    static VirtualDrive& getInstance();

    // Define la etiqueta del volumen (ej. "CBDOS_TOOL") y publica la lista de archivos
    bool publishFiles(const std::vector<VirtualFileEntry>& files, const char* volumeLabel = "CBDOS_DRV");

    // Agrega un único archivo de la SD o desde memoria en caliente (notifica Media Changed al host)
    bool publishSingleFile(const char* virtualName, const char* sdPath);
    bool publishMemoryBuffer(const char* virtualName, const uint8_t* data, uint32_t size);

    // Desmonta / vacía la unidad virtual
    void clear();
    bool isPublished() const;
};

} // namespace usb
} // namespace cbdos
```

#### Integración en `LuaBridge` (`cbdos.usb.*` / `cbdos.hid.*`)
Permite que cualquier script `.lua` o `.luapp` arme un pendrive virtual a medida en 2 líneas antes de lanzar un ataque o tarea de mantenimiento HID:
```lua
-- Publicar un script generado en memoria y un binario de la SD como único contenido del USB
cbdos.usb.drive_publish({
    { name = "autorun_diag.ps1", content = "Get-NetAdapter | Out-File C:\\net.txt" },
    { name = "putty.exe", path = "/sdcard/tools/putty.exe" }
}, "NET_RESCUE")

-- Combinar con HID para abrir el archivo desde el pendrive recién montado
hid.press_gui("r")
hid.delay(500)
hid.type("powershell -ep bypass E:\\autorun_diag.ps1\n")
```

---

### 2.2. Subsistema 2: `cbdos::media::VideoPlayer` (Basado en `nv_vplayer.c` + `vp_avi.c` + `pl_mpeg.h`)

#### Estrategia de Portado e Integración con LVGL 9.5
En lugar de apagar LVGL como hace NucleoOS en algunos modos, en CBDos integraremos el reproductor respetando nuestra regla de oro (**`BaseView` + `HeaderBar` + LVGL 9.5**):

1. **Buffer de Salida Triple Anillo en PSRAM (`s_ring[3]`):**
   - El hilo decodificador (`vplayer_task`) escribe los frames RGB565 decodificados (vía **HW JPEG + PPA** en P4 o **`pl_mpeg`** en S3/P4) en el anillo de buffers alineados a 64 bytes.
2. **Renderizado en `VideoPlayerView` (LVGL 9.5):**
   - La vista contiene un `lv_image` / `lv_canvas` apuntando al slot publicado actual (`s_cur`) con controles superpuestos táctiles auto-ocultables (Play/Pausa, barra de progreso con *seek* real usando el índice `idx1` de AVI, volumen y tiempo).
3. **Reloj Maestro de Sincronización Audio/Video (`clk_ms`):**
   - Se porta la lógica de reloj de `nv_vplayer.c` (L109-175), donde el reloj de pared (`esp_timer_get_time`) se ajusta suavemente contra el reloj real de muestras consumidas por el DAC I2S (`cbdos::audio`), descartando frames tardíos (`frame drop`) si la tarjeta MicroSD tarda en leer, garantizando que el audio jamás pierda sincronía ni tartamudee.

#### Integración en `LuaBridge` (`cbdos.video.*`)
```lua
-- Reproducir un video dentro de un contenedor de una app .luapp o a pantalla completa
cbdos.video.play("/sdcard/videos/captura_camara01.avi")
cbdos.video.seek(15000) -- saltar al segundo 15
cbdos.video.pause()
```

---

## 3. Evaluación de Portabilidad: ¿Portar Directo o Crear Versión Propia?

| Subsistema | Archivos en `NucleoOS-P4` | Estrategia Recomendada | Nivel de Esfuerzo |
| :--- | :--- | :---: | :---: |
| **1. Sintetizador FAT16 (`VirtualDrive`)** | `components/nv_usb/nv_usb_msc.c` (276 líneas) | 🔄 **Portado Directo (95% idéntico)** | **Bajo:** El código es C99 puro y autónomo; sintetiza Boot Sector, tablas FAT16, entradas LFN y callbacks `tud_msc_*` de TinyUSB. Solo requiere reemplazar `fopen/fread` por `cbdos::storage`. |
| **2. Demuxer AVI + Reloj A/V (`vp_avi`)** | `components/nv_vplayer/vp_avi.c` y `vp_avi.h` (450 líneas) | 🔄 **Portado Directo (90% idéntico)** | **Bajo-Medio:** Es un parser RIFF/AVI limpio y probado con tests de host y lecturas alineadas a sectores para máxima velocidad en SD. |
| **3. Decodificador MPEG-1 (`pl_mpeg`)** | `components/nv_vplayer/pl_mpeg.h` y `nv_mpeg1_impl.c` | 🔄 **Portado Directo (100% C estándar)** | **Bajo:** Biblioteca de un solo encabezado sin dependencias externas; funciona tanto en ESP32-S3 como en ESP32-P4. |
| **4. Vista `VideoPlayerView` y Controles** | `components/nv_apps/video_app.cpp` | 🧬 **Versión Propia en LVGL 9.5 (`BaseView`)** | **Medio:** Construiremos la interfaz con nuestro `DefaultTheme`, `HeaderBar` y bindings para `.luapp`. |

---

## 4. Plan de Implementación por Fases

1. **Fase 1 — Unidad USB Virtual Selectiva (`VirtualDrive`):**
   - Portar la lógica de síntesis FAT16 de `nv_usb_msc.c` a `core/src/usb/VirtualDrive.cpp` y conectarla a los descriptores MSC de TinyUSB en `bsp/esp32_s3_jc3248` y `bsp/esp32_p4_jc4880`.
   - Exponer `cbdos.usb.drive_publish()` y `cbdos.usb.drive_clear()` en `LuaBridge` + selector "Compartir archivo por USB" en el explorador de archivos.
2. **Fase 2 — Motor de Video AVI/MJPEG y MPEG-1 (`VideoPlayerService`):**
   - Portar `vp_avi.c`, `pl_mpeg.h` y el bucle de sincronización A/V de `nv_vplayer.c` usando el acelerador Hardware JPEG/PPA en P4 y `pl_mpeg` en S3/P4.
3. **Fase 3 — App `VideoPlayerView` en LVGL 9.5 y Binding `cbdos.video`:**
   - Crear la vista reproductora integrada con `HeaderBar`, controles táctiles de seek/volumen y soporte para abrir archivos `.avi` y `.mpg` desde la MicroSD o desde scripts `.luapp`.
