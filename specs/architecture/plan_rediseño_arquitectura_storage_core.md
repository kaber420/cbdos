# Plan de Rediseño Arquitectónico: Desacoplamiento de Filesystem y Storage Core

> **Documento de Arquitectura y Especificación Técnica**  
> **Ubicación:** `specs/architecture/plan_rediseño_arquitectura_storage_core.md`  
> **Fecha:** 2026-09-24  
> **Estado:** 📋 Aprobado para ejecución posterior (Post-Migración CDT)  
> **Prioridad:** Alta (Deuda Técnica Estructural)  
> **Objetivo:** Desacoplar el formateo y la gestión de sistemas de archivos de los Board Support Packages (BSP) hacia el Core (`core/`), redefiniendo el contrato de `IStorageBackend` para que el BSP sea estrictamente agnóstico al sistema de archivos y unifique la funcionalidad real en ESP32-P4 y ESP32-S3.

---

## 1. Diagnóstico de la Arquitectura Actual

Actualmente existe una inversión indebida de responsabilidades entre el Core del sistema operativo y los BSPs específicos de cada tablilla:

### A. Fuga de Responsabilidades hacia el BSP
En [`core/include/cbdos/storage.hpp:38`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/storage.hpp#L38):
```cpp
class IStorageBackend {
public:
    virtual bool formatSd() = 0; // ⚠️ El Core delega la lógica de formateo al driver de hardware
    ...
};
```
Esta definición obliga a que cada tablilla invente cómo formatear un sistema de archivos, cuando la única responsabilidad del BSP debería ser proveer el canal físico de comunicación (pines, LDO y controlador de bus SDMMC/SPI).

### B. Inconsistencia Crítica entre Plataformas
1. **ESP32-P4 ([`hal_storage_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp)):**
   - Implementa formateo usando la API propietaria de IDF `esp_vfs_fat_sdcard_format`.
   - Oculta errores en bajo nivel retornando `true` cuando la operación falla.
   - Duplicó la configuración de pines y hardware dentro del método de formateo.
2. **ESP32-S3 ([`hal_storage_s3.cpp:205-208`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_storage_s3.cpp#L205-L208)):**
   - **Formateo Falso:** No formatea la tarjeta; únicamente la desmonta y vuelve a montar:
     ```cpp
     bool formatSd() override {
         unmountSd();
         return mountSd(); // ⚠️ Falsa implementación
     }
     ```
   - El usuario cree que la tarjeta fue formateada desde la UI de CBDos, pero sus archivos siguen intactos.

---

## 2. Nueva Arquitectura Propuesta

```
┌────────────────────────────────────────────────────────┐
│                   UI / Aplicaciones                    │
│      (StorageConfigView, Gestor de Archivos, CLI)      │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                   CBDos Storage Core                   │
│               (`core/src/system/storage.cpp`)           │
│                                                        │
│  - Gestión de volumen y particiones                    │
│  - Formateo real FAT32 / VFS (Lógica Unificada)        │
│  - Operaciones de ficheros (copiar, borrar, listar)    │
└───────────────────────────┬────────────────────────────┘
                            │ (Contrato Estricto de Bloques)
                            ▼
┌────────────────────────────────────────────────────────┐
│                 IStorageBackend (BSP)                  │
│                                                        │
│  - ¿Qué pines usar? (Device Tree `cbdos_device_tree.h`)│
│  - ¿Cómo energizar? (LDO / Power Control)              │
│  - Inicializar bus físico (SDMMC 4-bit / SPI)          │
│  - Montar dispositivo de bloques en VFS                │
│  - CERO conocimiento de formateo o lógica de ficheros  │
└───────────────────────────┬────────────────────────────┘
                            │
             ┌──────────────┴──────────────┐
             ▼                             ▼
    [ BSP ESP32-P4 ]              [ BSP ESP32-S3 ]
   (JC4880P443C SDMMC)            (JC3248W535 SPI)
```

---

## 3. Plan de Implementación (Tareas Detalladas)

### Tarea 1: Redefinición del Contrato en `IStorageBackend`
* Eliminar `virtual bool formatSd() = 0;` de `IStorageBackend`.
* El BSP solo provee:
  - `init()`: Levanta energía LDO y buses.
  - `mountBlockDevice()` / `unmountBlockDevice()`: Registra o desregistra el dispositivo físico en el VFS.
  - `isMediaPresent()`: Detección física de tarjeta.

### Tarea 2: Centralización del Formateo en `core/src/system/storage.cpp`
* Mudar la invocación de formateo FatFS al Core.
* El Core desmonta el VFS de alto nivel, invoca la rutina de bajo nivel correspondiente de formateo FatFS sobre el volumen registrado, y re-monta la unidad.
* Garantizar el mismo comportamiento tanto para el compilador ESP-IDF (P4) como para PlatformIO/Arduino (S3).

### Tarea 3: Limpieza de BSPs
* **`hal_storage_p4.cpp`:** Retirar el método `formatSd()`. El archivo queda reducido únicamente a `mountSd()`, `unmountSd()`, control de LDO y estadísticas de capacidad.
* **`hal_storage_s3.cpp`:** Retirar la implementación ficticia de `formatSd()`.

### Tarea 4: Corrección de UI y CLI
* Las llamadas en [`StorageConfigView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/StorageConfigView.cpp#L70) y [`LuaBindings_FS.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/bindings/LuaBindings_FS.cpp#L86) continúan llamando a `cbdos::storage::formatSd()`, pero ahora esta función opera sobre el Core desacoplado y garantiza retorno fiel.

---

## 4. Cuándo debe ejecutarse este Plan

* **Momento:** Inmediatamente después de culminar la **Fase A** (Migración de pines de hardware al CDT) o en la **Fase C** (Soporte multi-target S3).
* **Dependencias:** Requiere que el Device Tree de P4 y S3 esté estable para no generar conflictos de ramas.
* **Impacto:** Rompe el falso formateo del S3 y purga el BSP de P4 dejándolo como un driver de hardware 100% puro.
