# Plan Detallado: Solución Integral de Estabilidad y Descubrimiento Asíncrono para LAN Recon (ESP32-S3 y ESP32-P4)

**Documento:** `specs/architecture/plan_rediseño_lan_recon_asincrono_y_optimizacion_s3_p4.md`  
**Módulo:** `cbdos::network::lan_recon`  
**Estado:** Propuesta Formal para Aprobación del Usuario  
**Fecha:** 2026-09-28  
**Autor:** Antigravity Senior Embedded Systems Developer  

---

## 1. Análisis Causa-Raíz (Post-Mortem Técnico)

### A. ¿Por qué el ESP32-S3 se reiniciaba siempre?
1. **Llamadas Concurrentes a LwIP sin `LOCK_TCPIP_CORE()`:**  
   En `hal_lan_recon_s3.cpp`, la tarea `lan_recon_task` recorre `netif_list` e invoca `etharp_request()` y `etharp_find_addr()`. La pila LwIP en ESP-IDF/Arduino **no es reentrante**. Al llamarse desde un hilo secundario en paralelo mientras la tarea del driver Wi-Fi (`wifi_task`) o `tcpip_thread` recibe paquetes en Core 0, se genera una corrupción de memoria o violación de punteros (`LoadProhibited` / Guru Meditation Error), provocando el reinicio del microcontrolador.
2. **Desbordamiento de Stack (Stack Overflow) en `ssdpScan`:**  
   `lan_recon_task` se creaba con un stack de **8 KB** (`LanScannerService.cpp:132`). Dentro de ella, `LanScannerBackendS3::ssdpScan` reservaba un búfer de pila local de `char rx[1500];` (1.5 KB), sumado a las estructuras de red y llamadas a funciones de sockets (`recvfrom` consume hasta 2 KB de stack interno en LwIP). En cuanto llegaba una respuesta SSDP, el stack se desbordaba, activando el stack canary de FreeRTOS y reiniciando el dispositivo.
3. **Bloqueo del Task Watchdog (TWDT) en Core 0:**  
   La tarea fue anclada a Core 0 (donde corren el driver Wi-Fi y la tarea `IDLE0`). En la fase de ráfaga y la posterior "Pasada B", ejecutaba miles de operaciones en un bucle cerrado sin ceder CPU (`vTaskDelay(0)` / `sleepMs`), haciendo que el perro guardián del sistema dispare el reinicio por falta de reset del watchdog.
4. **Llamada Bloqueante de ARP dentro del Callback de SSDP:**  
   En `LanScannerService.cpp:664`, cuando SSDP recibía un paquete, la lambda invocaba `backend->resolveMacArp(dev.ip, mac)`, la cual ejecutaba `delay(180)` hasta 3 veces seguidas. El receptor UDP se congelaba durante cientos de milisegundos en medio del flujo de paquetes, saturando los buffers del socket y forzando desbordamientos.

### B. ¿Por qué en el ESP32-P4 funcionaba "a medias e intermitente"?
1. **Ráfaga ARP a 240 MHz sin Pacing (Saturación de Cola TX):**  
   Al lanzar 254 tramas broadcast seguidas sin micro-retardos, el chip transceptor Wi-Fi satura inmediatamente sus descriptores de transmisión (`ESP_ERR_NO_MEM`). La gran mayoría de las solicitudes ARP nunca salían físicamente al aire.
2. **Ventana de Escucha Truncada (500 ms):**  
   Dispositivos modernos (smartphones, Smart TVs, enchufes inteligentes) están en ahorro de energía (DTIM 3/10). Responden al cabo de 600 ms - 1200 ms. Como el código cortaba la escucha a los 500 ms exactos, esos equipos nunca entraban en la lista a menos que por casualidad estuvieran despiertos en ese medio segundo.
3. **Timeout Asfixiante de la Pasada B:**  
   Al fallar la ráfaga ARP, el escáner se ponía a probar puertos TCP 445/80/8008 secuencialmente contra 240 IPs vacías, tardando casi 2 minutos en los que la UI parecía congelada.

---

## 2. Arquitectura de la Solución

```
   [ LanReconView (UI LVGL Core 1) ]
                  │
                  ▼ (Inicia escaneo asíncrono)
   [ LanScannerService::runScan (Core 0, Stack 16 KB) ]
                  │
   ┌──────────────┴───────────────────────────────────────────────────────┐
   │ 1. PIPELINE ASÍNCRONO DE BARRIDO ARP CON PACING (0% a 50%)            │
   │    - Para cada IP de la subred:                                      │
   │        * backend->arpProbe(ip) (Con LOCK_TCPIP_CORE)                 │
   │        * sleepMs(15) -> 254 hosts × 15ms ≈ 3.8s totales             │
   │        * backend->lookupArpCacheOnly() -> Si hay nuevo host:         │
   │              Emite addFoundHost() -> ¡Aparece en vivo en la UI!      │
   ├──────────────────────────────────────────────────────────────────────┤
   │ 2. VENTANA DE GRACIA ASÍNCRONA (1.2 Segundos)                        │
   │    - 12 ciclos de 100ms con sleepMs(100)                             │
   │    - Recolecta respuestas tardías de móviles y dispositivos en DTIM   │
   ├──────────────────────────────────────────────────────────────────────┤
   │ 3. ESCANEO MULTICAST SSDP / UPNP SANEADO (50% a 70%)                 │
   │    - Búfer de 1500 bytes fuera del stack (Heap / Dinámico)           │
   │    - CERO delay bloqueante dentro del callback onDevice              │
   │    - Enlaza metadatos (Roku, Smart TV, DLNA) con hosts ya en memoria  │
   ├──────────────────────────────────────────────────────────────────────┤
   │ 4. SONDEO DE PUERTOS TCP DIRIGIDO (70% a 90%)                        │
   │    - Se ejecuta ÚNICAMENTE sobre hosts confirmados como VIVOS        │
   │    - CERO sondeos a IPs inactivas (se elimina el cuello de 2 min)   │
   ├──────────────────────────────────────────────────────────────────────┤
   │ 5. BANNER GRABBING Y TÍTULOS WEB (90% a 100%)                        │
   │    - SSH, Telnet, HTTP title sobre puertos abiertos detectados       │
   └──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Plan de Implementación Paso a Paso

### Tarea 1: Robustecer la Creación de la Tarea y Memoria de Stack
* **Archivo:** `core/src/network/LanScannerService.cpp`
* **Cambio:**
  - Aumentar el stack de la tarea de `8192` a **`16384` bytes** (16 KB) para absorber cómodamente LwIP, sockets y manipulación de strings:
    ```cpp
    m_taskHandle = cbdos::rtos::createTask(taskFn, "lan_recon_task", 16384, this, 3, 0);
    ```

### Tarea 2: Rediseñar el Pipeline de Descubrimiento en `LanScannerService.cpp`
* **Archivo:** `core/src/network/LanScannerService.cpp`
* **Cambios:**
  1. **Eliminar el `blast` ciego.** Reemplazarlo por emisión con **pacing de 15 ms**:
     ```cpp
     for (size_t i = 0; i < targets.size(); ++i) {
         if (m_abortRequested.load(std::memory_order_relaxed)) { ... }
         backend->arpProbe(targets[i]);
         
         // Colector en caliente: verificar si ya hay respuestas en caché
         uint8_t mac[6] = {0};
         if (backend->lookupArpCacheOnly(targets[i], mac)) {
             registrarHost(targets[i], mac, "arp");
         }
         
         // Ceder CPU y dar ritmo al radio Wi-Fi
         cbdos::rtos::sleepMs(15);
         setPhase(LanScanPhase::ArpSweep, (i * 45) / targets.size());
     }
     ```
  2. **Implementar Ventana de Gracia Post-Emisión (1.2 segundos):**
     - Bucle de 12 iteraciones con `cbdos::rtos::sleepMs(100)` consultando la caché ARP para capturar respuestas que llegaron durante o después de la emisión.
  3. **Suprimir la "Pasada B" en Subred Local:**
     - En escaneo de subred local directa, **no** disparar sondeos TCP masivos a las 240 IPs inactivas. Si un host de la misma subred física no respondió a ARP en 5 segundos, está apagado.
     - Conservar el sondeo L3 (ICMP / TCP fallback) exclusivamente cuando `isRouted == true` (redes distintas vía Gateway donde ARP no pasa).
  4. **Sanear el Callback de SSDP:**
     - En `backend->ssdpScan(...)`, eliminar completamente la llamada a `resolveMacArp()` con sus `delay(180)`.
     - Si el dispositivo ya fue visto en la fase ARP, simplemente actualizar `isTv = true`, `ssdpServer` y `ssdpLocation`. Si es una IP nueva, resolver la MAC con `lookupArpCacheOnly` no bloqueante.

### Tarea 3: Reparar el HAL de ESP32-S3 (`hal_lan_recon_s3.cpp`)
* **Archivo:** `bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp`
* **Cambios:**
  1. **Thread-Safety en llamadas LwIP:**
     - Usar `LOCK_TCPIP_CORE()` y `UNLOCK_TCPIP_CORE()` en `lookupArpCache()` y `sendArpRequest()`:
       ```cpp
       #include <lwip/tcpip.h>
       
       bool lookupArpCache(const ip4_addr_t& ip4, uint8_t mac[6]) {
           LOCK_TCPIP_CORE();
           for (struct netif* n = netif_list; n != nullptr; n = n->next) {
               struct eth_addr* ethRet = nullptr;
               const ip4_addr_t* ipRet = nullptr;
               s8_t idx = etharp_find_addr(n, &ip4, &ethRet, &ipRet);
               if (idx >= 0 && ethRet != nullptr) {
                   std::memcpy(mac, ethRet->addr, 6);
                   UNLOCK_TCPIP_CORE();
                   return true;
               }
           }
           UNLOCK_TCPIP_CORE();
           return false;
       }
       ```
     - Aplicar la misma protección en `sendArpRequest()`.
  2. **Búfer de SSDP Dinámico Fuera del Stack:**
     - Cambiar `char rx[1500];` por un búfer asignado en heap con `std::unique_ptr<char[]>` o vector reutilizable:
       ```cpp
       auto rx = std::unique_ptr<char[]>(new (std::nothrow) char[1500]);
       if (!rx) return false;
       ```
  3. **Pausas Amigables en SSDP:**
     - Añadir un pequeño `vTaskDelay(pdMS_TO_TICKS(5))` dentro del bucle de recepción de SSDP para alimentar el Task Watchdog Timer.

### Tarea 4: Sincronizar y Blindar el HAL de ESP32-P4 (`hal_lan_recon_p4.cpp`)
* **Archivo:** `bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp`
* **Cambios:**
  1. Aplicar las mismas protecciones `LOCK_TCPIP_CORE()` / `UNLOCK_TCPIP_CORE()` para la tabla ARP.
  2. Reemplazar el búfer de 1500 bytes de SSDP por asignación dinámica en heap/PSRAM.
  3. Asegurar que las respuestas del chip Wi-Fi ESP32-C6 fluyan sin pérdida de descriptores.

### Tarea 5: Feedback de Estado en la Interfaz Gráfica (`LanReconView.cpp`)
* **Archivo:** `core/src/ui/views/LanReconView.cpp`
* **Cambios:**
  1. Si `!backend->isNetworkConnected()`, mostrar en la tarjeta de información un texto evidente en color de advertencia:  
     `"Wi-Fi desconectado · Ve a Ajustes > Red para asociarte a un AP"`
  2. Al pulsar "Iniciar Escaneo" sin red, emitir aviso claro sin iniciar tarea zombi.
  3. Mantener la reactividad fluida de la lista conforme entran los hosts en vivo durante el pacing.

---

## 4. Matriz de Archivos a Modificar

| Archivo | Objetivo del Cambio |
| :--- | :--- |
| [`core/src/network/LanScannerService.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/network/LanScannerService.cpp) | Stack 16 KB, pacing 15ms, colector continuo, ventana de gracia 1.2s, saneo SSDP, eliminación de timeouts lentos. |
| [`bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp) | Thread-safety LwIP (`LOCK_TCPIP_CORE`), búfer SSDP dinámico en heap, prevención de reinicios. |
| [`bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp) | Paridad LwIP core lock, búfer dinámico, estabilidad idéntica en P4. |
| [`core/src/ui/views/LanReconView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/LanReconView.cpp) | Detección visual clara de Wi-Fi desconectado y reactividad en tiempo real. |

---

## 5. Criterios de Aceptación y Validación

1. **Cero Reinicios en ESP32-S3:**  
   Al pulsar "Iniciar Escaneo", el ESP32-S3 nunca más se reiniciará por `Guru Meditation`, `Stack Canary` ni `Task Watchdog`.
2. **Descubrimiento Consistente y Rápido:**  
   El escaneo de una red `/24` completa (254 IPs) tomará aproximadamente **5 a 7 segundos en total** (en lugar de casi 2 minutos).
3. **Descubrimiento Confiable en Ambas Plataformas:**  
   Tanto en S3 como en P4, los dispositivos (router, computadoras, móviles, Smart TVs, Roku) aparecerán de manera consistente en la lista.
4. **Compilación Limpia Multi-Target:**  
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248` (0 errores).  
   - ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build` (0 errores).
