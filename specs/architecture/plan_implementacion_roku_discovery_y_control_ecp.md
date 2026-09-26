# Plan de Arquitectura e Implementación: Descubrimiento y Control Roku ECP (:8060)

**Estado:** Aprobado / En Ejecución  
**Target:** ESP32-P4 (JC4880P443C) y ESP32-S3 (JC3248W535)  
**Dependencias:** `cbdos::network`, `IHttpClient`, LVGL 9.5, BSD Sockets  
**Ubicación:** `specs/architecture/plan_implementacion_roku_discovery_y_control_ecp.md`  
**Relacionado:** `specs/proposals/proposal_roku_ecp_control_and_fingerprint.md`  

---

## 1. Justificación y Diagnóstico Técnico

### 1.1. Diagnóstico del Descubrimiento Previo
1. **SSDP Multicast Malformado:**  
   En las implementaciones HAL de ambas plataformas (`hal_lan_recon_p4.cpp` y `hal_lan_recon_s3.cpp`), el paquete de búsqueda UPnP emitía la cabecera:
   ```http
   MAN: "ns=01; ns=01"
   ```
   Dicho formato no se ajusta al estándar UPnP Device Architecture 1.0/2.0, el cual exige:
   ```http
   MAN: "ssdp:discover"
   ```
   Como consecuencia, el stack de red de los dispositivos Roku descarta el datagrama UDP, impidiendo el descubrimiento multicast.
2. **Omisión del Puerto 8060 en el Escáner Activo:**  
   La lista de puertos `kLanReconPorts[]` en `core/include/cbdos/lan_recon.hpp` no incluía el puerto `8060` (puerto del External Control Protocol de Roku). Al recurrir a escaneo por sondeo TCP, solo se identificaba el puerto `7000` (Apple AirPlay), clasificando el dispositivo genéricamente como `TV (7000/AirPlay)`.

---

## 2. Fases de Implementación

### Fase 1: Corrección de SSDP en los HALs (Multiplataforma)
* **Objetivo:** Enviar paquetes válidos UPnP / SSDP a `239.255.255.250:1900`.
* **Archivos:**
  * `bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp`
  * `bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp`
* **Cambios:**
  * Reemplazar `MAN: "ns=01; ns=01"` por `MAN: "ssdp:discover"`.
  * Emitir búsqueda amplia `ST: ssdp:all` y búsqueda directa `ST: roku:ecp`.

### Fase 2: Registro del Puerto 8060 en LAN Recon (Core)
* **Objetivo:** Detección de Roku por sondeo TCP fallback y visualización en UI.
* **Archivos:**
  * `core/include/cbdos/lan_recon.hpp`: Agregar `8060` a `kLanReconPorts[]`.
  * `core/src/network/lan_recon.cpp`: Ajustar clasificación inteligente de dispositivos Roku.
  * `core/src/ui/views/LanReconView.cpp`: Mapear puerto 8060 a etiqueta `"Roku"`.

### Fase 3: Primitiva de Control ECP y Binding HTTP
* **Objetivo:** Permitir el envío de peticiones POST/GET al puerto 8060 tanto en C++ como a través de Lua (`.luapp`).
* **Archivos:**
  * `core/src/lua/bindings/LuaBindings_Network.cpp`: Exponer `cbdos.http.post()` y `cbdos.http.get()`.
  * Creación o soporte para el envío de eventos ECP: `/keypress/Home`, `/keypress/Back`, etc.

### Fase 4: Validación y Compilación Limpia
* Validar compilación en ESP32-P4 (ESP-IDF) y ESP32-S3 (PlatformIO).
