# Plan Técnico: Integración de NeighborDiscoveryService en OmniWISP (CBDos Native)

**Documento:** `specs/network/plan_integracion_discovery_omniwisp.md`  
**Módulo:** `cbdos::omniwisp` / `cbdos::network`  
**Estado:** Propuesta Formal Auditada (Aprobada por AI Auditor GLM-5.3)  
**Cumplimiento de Reglas:** LVGL v9.5 estricto, Core agnóstico C++17, Hardware real (cero mocks), Responsive (JC3248 320x480 y JC4880 480x800), FreeRTOS Core 0 / Core 1 Thread Safety.

---

## 1. Contexto y Diagnóstico

### 1.1 Situación Actual
- **Motor en Hardware Real:**
  - `bsp/esp32_s3_jc3248/hal/hal_neighbor_discovery_s3.cpp`: Broadcast UDP `10001` (Ubiquiti UBNT Discovery probe v1 `\x01\x00\x00\x00`, decodificador TLV de MAC, IP, Firmware, Hostname, Modelo) y Broadcast UDP `5678` (MikroTik MNDP).
  - `bsp/esp32_p4_jc4880/hal/hal_neighbor_discovery_p4.cpp`: Soporte idéntico para ESP32-P4 en ESP-IDF.
- **Auditoría de Thread Safety (Crucial):**
  - Actualmente `NeighborDiscoveryService.cpp` ejecuta `discoveryTask` en Core 0 modificando `m_devices` (`std::vector`) mientras la UI en Core 1 lee `getDiscoveredDevices()`. **Debe incorporarse un `std::mutex` en el servicio y `std::atomic<bool> m_isScanning`** para prevenir carreras de datos y crashes en runtime.
- **La Brecha en la UI de OmniWISP:**
  - `OmniwispAddApModal.cpp` únicamente tiene formulario manual. Obliga a teclear IPs y hostnames a mano.
  - Falta un flujo táctil de descubrimiento y selección de vecinos UBNT/MNDP.

---

## 2. Arquitectura de Experiencia de Usuario (UX) en Campo

```
                 OmniWISP Main View
                 ┌──────────────────┐
                 │ [+ Acciones ▼ ]  │
                 │   - AP Manual    │
                 │   - Descubrir AP │ <── Abre modal en pestaña Discovery
                 └─────────┬────────┘
                           │
 ┌─────────────────────────────────────────────────────────┐
 │ OmniwispAddApModal (300 x 380 px)                       │
 │ ┌─────────────────────────────────────────────────────┐ │
 │ │ [ LV_SYMBOL_SEARCH Descubrir ] [ LV_SYMBOL_EDIT ]   │ │ <-- Pestañas
 │ └─────────────────────────────────────────────────────┘ │
 │                                                         │
 │ [ Pestaña Descubrimiento ]                              │
 │  [ LV_SYMBOL_REFRESH Escanear Red ]   (Estado: Listo)  │
 │  ┌───────────────────────────────────────────────────┐  │
 │  │ Contenedor Scrollable Dedicado                    │  │
 │  │ ┌───────────────────────────────────────────────┐ │  │
 │  │ │ [UBNT] LiteBeam 5AC - Sector_Norte            │ │  │
 │  │ │ IP: 192.168.1.20 | FW: WA.v8.7.1              │ │  │
 │  │ └───────────────────────────────────────────────┘ │  │
 │  │ ┌───────────────────────────────────────────────┐ │  │
 │  │ │ [UBNT] PowerBeam 5AC - Enlace_Troncal         │ │  │
 │  │ │ IP: 10.200.1.45 | FW: XC.v8.7.1               │ │  │
 │  │ └───────────────────────────────────────────────┘ │  │
 │  └───────────────────────────────────────────────────┘  │
 │                                                         │
 │ Al tocar un radio:                                      │
 │ 1. Cambia a pestaña Manual                              │
 │ 2. Auto-rellena Host (192.168.1.20) y Nombre (Sector)   │
 │ 3. Hace lv_obj_scroll_to_view(s_taPass) y abre teclado  │
 │    para ingresar/confirmar contraseña                   │
 │ 4. [ Guardar en Vault ]                                 │
 └─────────────────────────────────────────────────────────┘
```

---

## 3. Especificación Técnica y Directrices LVGL v9.5

1. **Thread-Safety en Core y Servicio:**
   - En `NeighborDiscoveryService.hpp` y `.cpp`:
     - Proteger `m_devices` con `mutable std::mutex m_mutex;` en todos los métodos (`clear()`, `getDiscoveredDevices()`, `findByIp()`, y al insertar en `discoveryTask`).
     - Declarar `std::atomic<bool> m_isScanning{false};`.
   - **Zero cross-thread LVGL calls:** Prohibir llamadas directas a widgets LVGL desde la tarea de background. La UI consulta el estado de forma síncrona en Core 1 mediante un `lv_timer` local del modal (`m_timer`).

2. **Ergonomía en Pantalla 320x480 (JC3248W535):**
   - El modal no debe desbordarse con el teclado virtual de CBDos.
   - El contenedor de la lista de vecinos es un `lv_obj_create` con scroll independiente (`LV_SCROLLBAR_MODE_AUTO`), altura fija (~210px), permitiendo navegar la lista sin interferir con el contenedor padre.
   - Al seleccionar un vecino y pasar a la pestaña de credenciales, invocar `lv_obj_scroll_to_view(s_taPass, LV_ANIM_ON)` para que el campo permanezca visible sobre el teclado virtual.
   - **Prohibicion Absoluta de Emojis:** Queda terminantemente prohibido el uso de emojis en cualquier label, boton, string o codigo fuente. Toda iconografia visual debe ser exclusivamente mediante macros oficiales `LV_SYMBOL_*` de LVGL (ej: `LV_SYMBOL_REFRESH`, `LV_SYMBOL_SEARCH`, `LV_SYMBOL_EDIT`) o texto limpio en ASCII.
   - Usar simbolos nativos de LVGL (`LV_SYMBOL_REFRESH`, `LV_SYMBOL_SEARCH`, `LV_SYMBOL_EDIT`) garantizando soporte tipografico en cualquier BSP.

3. **Ciclo de Vida Limpio (Zero Memory Leaks ni Punteros Colgados):**
   - En `OmniwispAddApModal::close()`:
     - Eliminar el `lv_timer` local si existe (`lv_timer_delete(s_scanTimer)`).
     - Cerrar teclado (`UIManager::closeKeyboard()`).
     - Destruir el modal de forma asíncrona segura (`lv_obj_delete_async(s_mask)`).

---

## 4. Archivos a Modificar

1. **`core/include/cbdos/neighbor_discovery.hpp` & `core/src/network/NeighborDiscoveryService.cpp`:**
   - Incorporar `std::mutex` y `std::atomic<bool>` para thread safety total entre Core 0 (worker) y Core 1 (LVGL).
2. **`core/src/ui/views/omniwisp/OmniwispAddApModal.hpp` & `.cpp`:**
   - Agregar sistema de pestañas (`Descubrir` / `Manual`).
   - Contenedor de lista de vecinos y botón `[Escanear]`.
   - Polling seguro de vecinos con `lv_timer`.
   - Autocompletado inmediato al seleccionar un vecino.
3. **`core/src/ui/views/omniwisp/OmniwispMainView.cpp`:**
   - Agregar opción de descubrimiento directo en el menú de acciones (`Descubrir AP`).

---

## 5. Criterios de Aceptación
1. Compilación limpia en ESP32-S3 (`pio run -d bsp/esp32_s3_jc3248`) y ESP32-P4 (`idf.py -C bsp/esp32_p4_jc4880 build`).
2. Cero datos falsos: solo se listan equipos reales que respondan a los paquetes UDP 10001 / 5678.
3. Cero congelamientos de UI y cero crashes por concurrencia multi-core.
