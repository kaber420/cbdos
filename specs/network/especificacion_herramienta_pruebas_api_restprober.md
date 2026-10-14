# Especificación Técnica: Herramienta de Pruebas de API REST (RestProber)

**Documento:** `specs/network/especificacion_herramienta_pruebas_api_restprober.md`  
**Módulo:** `cbdos::network::restprober` & `cbdos::ui::RestProberView`  
**Estado:** ✅ Implementado y Validado Multi-Target (ESP32-P4 y ESP32-S3)  
**Fecha:** 14 de Octubre de 2026  
**Autor:** Antigravity Senior Embedded Systems Developer & kaber420  
**Ámbito:** `core/include/cbdos/http.hpp`, `core/src/network/`, `core/src/ui/views/`, BSPs ESP32-S3 y ESP32-P4  

---

## 1. Visión y Justificación del Proyecto

En el ecosistema de **CBDos** como estación táctica de campo y Cyberdeck, la interacción con servicios web y dispositivos de red local (IoT, domótica, routers, pasarelas, cámaras y microservicios) es una necesidad operativa recurrente.

Actualmente, diagnosticar o controlar un dispositivo mediante HTTP exige recurrir a una PC con herramientas pesadas como Postman o Insomnia, o utilizar la línea de comandos con `curl`.

**RestProber** (o *PocketPostman / MicroPostman*) introduce un cliente gráfico HTTP/REST completo, autónomo e interactivo, diseñado específicamente para interfaces táctiles en pantallas de 320x480 (ESP32-S3) y 480x800 (ESP32-P4) bajo **LVGL 9.5**.

### Objetivos Clave:
1. **Pruebas y Automatización Local:** Enviar peticiones HTTP arbitrarias (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD`) a endpoints locales o remotos.
2. **Control de Dispositivos IoT:** Disparar webhooks, interactuar con APIs REST locales (Shelly, Tasmota, WLED, ESPHome, Home Assistant, Roku ECP).
3. **Inspección Rápida desde LAN Recon:** Enlace de un toque desde la tarjeta de cualquier host descubierto con puertos web abiertos (80, 443, 8080, 8060).
4. **Cero Bloqueo de UI (Arquitectura Asíncrona):** Todas las transacciones de red se ejecutan en segundo plano (Core 0), manteniendo la interfaz táctil fluida a 60 FPS (Core 1).
5. **Protección de Memoria Embebida:** Límites estrictos de búfer (Buffer Capping) para prevenir desbordamientos de heap (OOM) al recibir respuestas HTTP masivas.

---

## 2. Arquitectura del Sistema y Pureza de Capas

De acuerdo con las reglas de arquitectura de CBDos:
* **`core/` es agnóstico:** Cero dependencias directas de ESP-IDF (`esp_http_client.h`), Arduino (`HTTPClient.h`), sockets BSD o FreeRTOS nativo.
* **Extensión del contrato HAL `IHttpClient`:** Se expande la abstracción existente en [`core/include/cbdos/http.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/http.hpp) para admitir verbos HTTP arbitrarios, cabeceras personalizadas y métricas de latencia.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        CAPA 7: PRESENTACIÓN (UI)                       │
│                                                                        │
│   [ RestProberView (LVGL 9.5) ]         [ LanReconView (Enlace 1-tap) ]│
│   • Selector Verbo (GET/POST/...)       • Botón "Probar REST"          │
│   • URL Text Area + Teclado táctil                                     │
│   • TabView: Headers / Body / Response                                 │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Solicita ejecución asíncrona
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   CAPA 5-6: SERVICIOS CORE (Agnóstico)                 │
│                                                                        │
│   [ RestProberService (Singleton) ]                                    │
│   • Orquestación en tarea Core 0 ("rest_prober_task", 12KB stack)      │
│   • Control de timeouts, cancelación y sanitización de memoria         │
│   • Historial de peticiones en RAM (últimas 10)                        │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Invocación vía interfaz abstracta
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   CAPA HAL DE RED (`core/include/cbdos/`)              │
│                                                                        │
│   [ IHttpClient ] (Contrato C++17)                                     │
│   • HttpResponse execute(const HttpRequest& req)                       │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Implementaciones en BSP
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   CAPA DE SOPORTE DE HARDWARE (BSP)                    │
│                                                                        │
│   [ hal_http_p4.cpp (ESP-IDF) ]        [ hal_http_s3.cpp (Arduino) ]   │
│   • esp_http_client API                • HTTPClient / WiFiClient       │
│   • Conexión SSL/TLS opcional          • Manejo de cabeceras crudas    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Modelo de Datos y Extensión de Contratos (`http.hpp`)

Se extiende [`core/include/cbdos/http.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/http.hpp) para evolucionar de llamadas simples `get/post` a una estructura orientada a peticiones completas:

```cpp
namespace cbdos {
namespace http {

enum class HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head
};

struct HttpHeader {
    std::string key;
    std::string value;
};

struct HttpRequest {
    HttpMethod method = HttpMethod::Get;
    std::string url;
    std::vector<HttpHeader> headers;
    std::string body;
    uint32_t timeoutMs = 8000;
    size_t maxBodyCaptureBytes = 32768; // Límite defensivo de 32 KB para evitar OOM
};

struct HttpResponse {
    int statusCode = 0;
    std::string statusText;
    std::vector<HttpHeader> headers;
    std::string body;
    uint32_t rttMs = 0;        // Tiempo total de ida y vuelta en milisegundos
    bool success = false;
    bool truncated = false;    // Indica si el cuerpo excedió maxBodyCaptureBytes
    std::string errorMessage;  // Detalle de fallo en caso de error de conexión/DNS
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    
    // Contrato canónico avanzado
    virtual HttpResponse execute(const HttpRequest& request) = 0;

    // Métodos de conveniencia retrocompatibles
    virtual HttpResponse get(const std::string& url, uint32_t timeoutMs = 5000);
    virtual HttpResponse post(const std::string& url, const std::string& payload, 
                              const std::string& contentType = "application/json", 
                              uint32_t timeoutMs = 5000);
};

} // namespace http
} // namespace cbdos
```

---

## 4. Diseño de la Interfaz de Usuario (LVGL 9.5)

La interfaz se divide en 3 zonas ergonómicas optimizadas para interacción táctil:

```
┌────────────────────────────────────────────────────────────────────────┐
│ [ < Volver ]              RESTPROBER               [ 💾 Presets ]      │ <- HeaderBar
├────────────────────────────────────────────────────────────────────────┤
│ ┌──────────┐ ┌───────────────────────────────────────────┐ ┌─────────┐ │
│ │ GET    ▼ │ │ http://192.168.1.150:8080/api/relay/0     │ │  SEND 🚀│ │ <- Control Bar
│ └──────────┘ └───────────────────────────────────────────┘ └─────────┘ │
├────────────────────────────────────────────────────────────────────────┤
│ [  HEADERS (2)  ]   [  BODY (JSON)  ]   [★ RESPONSE: 200 OK (38ms) ]   │ <- TabView
│ ┌────────────────────────────────────────────────────────────────────┐ │
│ │ Status: 200 OK          Time: 38 ms          Size: 142 B           │ │
│ │ ────────────────────────────────────────────────────────────────── │ │
│ │ {                                                                  │ │
│ │   "device": "Shelly1",                                             │ │
│ │   "relay": 0,                                                      │ │
│ │   "ison": true,                                                    │ │
│ │   "has_timer": false                                               │ │
│ │ }                                                                  │ │
│ │                                                                    │ │
│ │                                                                    │ │
│ └────────────────────────────────────────────────────────────────────┘ │
├────────────────────────────────────────────────────────────────────────┤
│ [Q] [W] [E] [R] [T] [Y] [U] [I] [O] [P] [  DEL  ]                      │ <- Teclado
│ [A] [S] [D] [F] [G] [H] [J] [K] [L] [   ENTER   ]                      │    LVGL
│ [123] [/] [:] [.] [          ESPACIO          ] [HIDE]                 │  (Auto-toggle)
└────────────────────────────────────────────────────────────────────────┘
```

### Componentes de la Vista:
1. **Barra de Petición Superior:**
   * **Selector de Método:** Dropdown (`lv_dropdown`) con colores estándar:
     * `GET`: Cian `#00F5D4`
     * `POST`: Verde esmeralda `#10B981`
     * `PUT` / `PATCH`: Naranja `#F59E0B`
     * `DELETE`: Rojo coral `#EF4444`
     * `HEAD`: Violeta `#8B5CF6`
   * **Campo de URL:** `lv_textarea` de una sola línea con scroll horizontal y placeholder (`http://...`).
   * **Botón SEND:** Al pulsarse, cambia a estado ocupado (spinner animado o texto `"..."`), desactivando re-envíos simultáneos.
2. **Panel de Pestañas (`lv_tabview`):**
   * **Pestaña 1 - Cabeceras (Headers):** Lista de pares clave-valor (`Content-Type`, `Authorization`, `Accept`, etc.) con botones para agregar o eliminar filas.
   * **Pestaña 2 - Cuerpo (Body):** Área de texto multilinea para ingresar payloads JSON, texto plano o XML. Incluye botones rápidos para formatear o limpiar.
   * **Pestaña 3 - Respuesta (Response):**
     * Cabecera de telemetría: Badge de código HTTP, tiempo RTT y tamaño en bytes.
     * Si `statusCode >= 200 && < 300`: Badge verde.
     * Si `statusCode >= 400`: Badge rojo.
     * Área de texto de sólo lectura con scroll suave para visualizar el cuerpo de la respuesta.
     * Selector secundario para ver el cuerpo crudo o las cabeceras HTTP de respuesta.
   * **Pestaña 4 - Presets e Historial:**
     * Historial de las últimas 10 peticiones ejecutadas para re-ejecución inmediata con 1 toque.
     * Presets preconfigurados para IoT común (Roku Remote, Shelly Relay, Tasmota Info, WLED Effect).
3. **Manejo del Teclado Virtual (`lv_keyboard`):**
   * El teclado se oculta automáticamente cuando se visualiza la respuesta o se navega el scroll, y se despliega con animación fluida al tocar cualquier `lv_textarea`.

---

## 5. Integración con Otras Apps de CBDos

### 5.1. Enlace Cruzado desde LAN Recon (`LanReconView`)
Cuando LAN Recon finaliza su escaneo o el usuario inspecciona una tarjeta de host:
* Si el host tiene abiertos los puertos `80`, `443`, `8080` o `8060`, la tarjeta muestra el botón **`[ Probar REST ]`**.
* Al tocarlo, se invoca:
  ```cpp
  UIManager::getInstance().navigateTo("RestProberView");
  RestProberView::loadTargetUrl("http://" + host.ip + ":" + std::to_string(openPort) + "/");
  ```
* La aplicación se abre inmediatamente con el objetivo configurado, lista para disparar peticiones.

### 5.2. Automatización desde Terminal (Consola CLI)
Comando interactivo en el shell de CBDos:
```bash
curl -X POST http://192.168.1.150/relay/0 -d '{"turn":"toggle"}'
```
Reutiliza el mismo servicio de backend (`RestProberService`), garantizando coherencia en todo el SO.

---

## 6. Consideraciones de Seguridad y Robustez de Memoria

1. **Defensa contra Respuestas Masivas (OOM Protection):**  
   Si una petición devuelve un archivo binario o una página web pesada de 5 MB, el buffer de recepción se trunca estrictamente en `maxBodyCaptureBytes` (por defecto 32 KB), emitiendo una notificación en la interfaz (`"Respuesta truncada a 32 KB"`).
2. **Límite de Tiempo (Watchdog & Timeout):**  
   Timeout configurable por petición (default 8 s). Si el servidor destino no responde, la tarea aborta la conexión TCP sin congelar recursos ni activar el Task Watchdog Timer (TWDT).
3. **Aislamiento de Hilos:**  
   La tarea de red corre con prioridad 3 en Core 0. La comunicación con la UI en Core 1 se realiza mediante callbacks seguros protegidos por mutex o despachados al bucle principal de LVGL (`lv_async_call`).

---

## 7. Plan de Implementación Paso a Paso
 
| Fase | Tarea | Archivos Clave | Estado |
| :---: | :--- | :--- | :---: |
| **Fase 1** | **Contrato Core HTTP y Soporte de Verbos:** Extender `IHttpClient`, `HttpRequest` y `HttpResponse` para soportar `HttpMethod`, cabeceras, buffer capping y métricas RTT. | `core/include/cbdos/http.hpp`<br>`core/src/network/http_client.cpp` | ✅ **Completado** |
| **Fase 2** | **Actualización de HALs en BSPs:** Implementar `execute(HttpRequest)` en `hal_http_p4.cpp` (usando `esp_http_client_set_method`, `esp_http_client_set_header`, `esp_timer`) y en `hal_http_s3.cpp` (usando `HTTPClient::sendRequest`). | `bsp/esp32_p4_jc4880/hal/hal_http_p4.cpp`<br>`bsp/esp32_s3_jc3248/hal/hal_http_s3.cpp` | ✅ **Completado** |
| **Fase 3** | **Servicio Asíncrono `RestProberService`:** Crear el despachador thread-safe en Core 0 con `cbdos::rtos`, validación de `cbdos::network::isConnected()` y gestión de colecciones/presets con `cbdos::storage`. | `core/src/network/RestProberService.hpp`<br>`core/src/network/RestProberService.cpp` | ✅ **Completado** |
| **Fase 4** | **Construcción de Vista UI `RestProberView` (Estilo Hoppscotch):** Interfaz táctil LVGL 9.5 con selector de verbo a color, barra URL, 4 pestañas (Headers, Body, Response, Colecciones) y teclado integrado vía `UIManager::attachKeyboard`. | `core/src/ui/views/RestProberView.hpp`<br>`core/src/ui/views/RestProberView.cpp` | ✅ **Completado** |
| **Fase 5** | **Registro en el Sistema e Integración:** Añadir `RestProber` a `AppRegistry.cpp`, flag `CBDOS_FEATURE_REST_PROBER` en `cbdos_build_profile.h` y botón directo `[ REST 🚀 ]` desde `LanReconView.cpp`. Validación multi-target limpia. | `core/src/ui/AppRegistry.cpp`<br>`core/src/ui/views/LanReconView.cpp`<br>`core/include/cbdos_build_profile.h` | ✅ **Completado** |
| **Fase 6** | **Modularización y Desacoplamiento de Vista UI:** Descomposición del archivo monolítico de 716 líneas en submódulos especializados dentro de `core/src/ui/views/restprober/` (`ControlBar`, `Headers`, `Body`, `Response`, `Collections`, `Common`). El orquestador `RestProberView.cpp` se redujo a ~170 líneas. | `core/src/ui/views/RestProberView.hpp`<br>`core/src/ui/views/RestProberView.cpp`<br>`core/src/ui/views/restprober/*` | ✅ **Completado** |

---

## 8. Estructura Modular de Presentación (`core/src/ui/views/restprober/`)

Para evitar ediciones destructivas y mantener archivos concisos y mantenibles (< 180 líneas c/u):

* **[`RestProberView.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RestProberView.hpp) & [`RestProberView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RestProberView.cpp) (~170 líneas):** Orquestador del ciclo de vida (`onCreate`, `onDestroy`, `onShow`, `onHide`), integración con `HeaderBar`, creación del `lv_tabview` y despacho asíncrono hacia `RestProberService`.
* **[`RestProberCommon.hpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/restprober/RestProberCommon.hpp) (~35 líneas):** Paleta de colores y resolución cromática según el método HTTP.
* **[`RestProberView_ControlBar.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/restprober/RestProberView_ControlBar.cpp) (~80 líneas):** Selector desplegable de verbos HTTP con color reactivo, barra de URL y botón de envío (SEND 🚀).
* **[`RestProberView_Headers.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/restprober/RestProberView_Headers.cpp) (~145 líneas):** Pestaña de cabeceras HTTP, creación dinámica de pares clave-valor y eliminación reactiva.
* **[`RestProberView_Body.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/restprober/RestProberView_Body.cpp) (~70 líneas):** Editor multilínea del cuerpo de la petición con accesos rápidos a plantillas JSON y limpieza.
* **[`RestProberView_Response.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/restprober/RestProberView_Response.cpp) (~120 líneas):** Pestaña de respuesta con badge de estado HTTP coloreado, métricas RTT/tamaño y visor de payload solo-lectura.
* **[`RestProberView_Collections.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/restprober/RestProberView_Collections.cpp) (~140 líneas):** Pestaña de colecciones y presets domóticos, guardado automático en almacenamiento e hidratación de peticiones.


