# Plan Quirúrgico: Refactorización y Modularización de Ubiquiti / OmniWISP (CBDos Native)

**Fecha:** 25 de Octubre de 2026  
**Documento:** `specs/network/plan_refactor_modular_ubiquiti_airos.md`  
**Estado:** En Ejecución  

---

## 1. Problema Identificado

1. **Antipatrón Monolítico en UI:**
   `UbiquitiView.cpp` superaba las 440 líneas conteniendo lógica de red, parseo de JSON (`cJSON`), manejo de cookies HTTP y tokens CSRF, mezclado con widgets y layouts de LVGL v9.5.
2. **Uso Ilegal de `std::thread`:**
   Tanto en `NeighborDiscoveryService.cpp` como en `UbiquitiView.cpp` se estaban lanzando hilos `std::thread`, lo cual en FreeRTOS es un antipatrón en sistemas embebidos (desperdicio de RAM, falta de afinidad de core y riesgo de stack overflow).
3. **Peligro de Bloqueo por Red:**
   Llamadas de red disparadas sin verificar antes `cbdos::network::isConnected()`.

---

## 2. Arquitectura Limpia Modular

```
┌────────────────────────────────────────────────────────┐
│ UI LAYER (LVGL v9.5 puro)                              │
│  - core/src/ui/views/UbiquitiView.hpp / .cpp           │
│    * Solo renderiza UI (BaseView + HeaderBar)          │
│    * Reactivo: se refresca con flags m_statusDirty     │
│    * Cero sockets, cero parseo cJSON                   │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ NETWORK SERVICE LAYER (Core C++ Desacoplado)           │
│  1. AirOSClient (core/include/cbdos/airos_client.hpp)  │
│     * core/src/network/AirOSClient.cpp                 │
│     * Manejo limpio de auth, cookies, CSRF y telemetría│
│     * Parseo de JSON a struct AirOSTelemetry           │
│  2. NeighborDiscoveryService                           │
│     * Tarea FreeRTOS nativa con cbdos::rtos::createTask│
│     * Control de stack (8192 B) y afinidad de Core     │
│  3. FreeRTOS Worker Task para AirOS                    │
│     * Stack controlado (16384 B) en Core 0             │
│     * No interfiere con el Core 1 de LVGL              │
└────────────────────────────────────────────────────────┘
```

---

## 3. Pasos de Ejecución

1. **Crear `cbdos::network::AirOSClient`:**
   - Header: `core/include/cbdos/airos_client.hpp`
   - Implementación: `core/src/network/AirOSClient.cpp`
   - Registro en `core/CMakeLists.txt`.
2. **Sanear `NeighborDiscoveryService.cpp`:**
   - Extirpar `#include <thread>`.
   - Utilizar `cbdos::rtos::createTask` con stack acotado (8KB) y `cbdos::rtos::deleteTask`.
3. **Reescribir `UbiquitiView` de forma quirúrgica y limpia:**
   - Dejar únicamente lógica de presentación visual.
   - Usar `AirOSClient` y `NeighborDiscoveryService`.
   - Tarea worker con `cbdos::rtos::createTask` en Core 0 con stack de 16KB.
   - Cancelación limpia al salir de la vista (`onHide()`).
4. **Verificación de Compilación Multi-Target:**
   - ESP32-S3 PlatformIO: `pio run -d bsp/esp32_s3_jc3248`
   - ESP32-P4 ESP-IDF: verificación de dependencias y sintaxis.
