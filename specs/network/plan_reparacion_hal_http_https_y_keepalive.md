# Plan de Reparación: Capa HAL HTTP (HTTPS, Streaming y Keep-Alive)

**Documento:** `specs/network/plan_reparacion_hal_http_https_y_keepalive.md`
**Módulo:** `cbdos::bsp::EspIdfHttpClient` (P4) & `cbdos::bsp::ArduinoHttpClient` (S3)
**Estado:** ⬜ Planificado — Bloqueante previo a Fase 7 del RestProber
**Fecha:** 17 de Octubre de 2026
**Autor:** kaber420 & Antigravity Senior Embedded Systems Developer
**Ámbito:** `bsp/esp32_p4_jc4880/hal/hal_http_p4.cpp`, `bsp/esp32_s3_jc3248/hal/hal_http_s3.cpp`
**Precedencia:** Este plan precede a la **Fase 7** de
[`especificacion_herramienta_pruebas_api_restprober.md`](especificacion_herramienta_pruebas_api_restprober.md)
el importador de colecciones depende de un cliente HTTP con HTTPS funcional
(descarga de specs por URL) y con streaming (descarga de colecciones de 256 KB
sin OOM).

---

## 1. Motivación y Justificación

Antes de extender el RestProber con el importador de colecciones externas
(Postman v2.1 / OpenAPI 3), se auditaron ambos adaptadores concretos del
contrato `IHttpClient` (`core/include/cbdos/http.hpp`). El hallazgo es
crítico: **HTTPS está roto en ambos targets** y la gestión de memoria de la
respuesta en S3 no aplica el patrón de streaming que el P4 ya resuelve.

No se plantea reemplazar la arquitectura: `IHttpClient` se mantiene como
contrato canónico y cada HAL sigue siendo una implementación fina sobre la
API nativa de su plataforma. Lo que se repara es el contenido de los
adaptadores.

### 1.1. Contexto externo evaluado

Se evaluó la adopción de la librería externa
| [`esp32-http-client` (PedroFnseca, 2026)](https://github.com/PedroFnseca/esp32-http-client),
cuya propuesta central es el parseo directo del stream TCP hacia variables C
(`getBody("temp", &var)`), evitando materializar la respuesta en heap.

**Veredicto: no se adopta como dependencia**, por tres razones:

1. **Problema distinto:** RestProber necesita exhibir el cuerpo de la
   respuesta completo en la UI (visor de payloads), por lo que la respuesta
   debe capturarse en memoria de forma acotada — el streaming hacia variables
   vinculadas no aplica directamente.
2. **Licencia:** es GPL-3.0. CBDos es GPL-3.0 (`LICENSE`), por lo que la
   compatibilidad existe legalmente, pero su capa es Arduino-only y el P4
   compila contra ESP-IDF puro; introduciría divergencia entre HALs que hoy
   comparten el mismo contrato.
3. **Aislamiento:** el resto de la lib asume `HTTPClient` de Arduino por
   debajo, duplicando el rol del adaptador que ya poseemos.

**Lo que SÍ se rescata de su aproximación (sin dependencia):**

| Técnica | Origen (esp32-http-client) | Aplicación en CBDos |
| :--- | :--- | :--- |
| Lectura en streaming por chunks desde `Stream*` | `BufferedStreamReader` sobre `HTTPClient::getStreamPtr()` | Sustituir `getString()` en S3 por bucle de chunks idéntico al patrón P4 |
| HTTP/1.1 Keep-Alive (`setReuse(true)`) | Reuso de conexión TCP/TLS entre peticiones | Cache de sesión por host con TTL en ambos targets |
| Descodificación de `Transfer-Encoding: chunked` sin buffer completo | Automática al leer del stream | Ya implícita en el patrón de streaming adoptado |

---

## 2. Diagnóstico Actual

### 2.1. ESP32-P4 (`hal_http_p4.cpp`) — ESP-IDF `esp_http_client`

| # | Hallazgo | Evidencia | Impacto |
| :-: | :--- | :--- | :--- |
| P1 | El cert bundle compilado no está conectado: falta `crt_bundle_attach` en `esp_http_client_config_t` | `sdkconfig:2117` (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`) vs. `hal_http_p4.cpp:19-27` (config sin cert) | **HTTPS falla** en el handshake (`ESP_ERR_ESP_TLS` / certificado no verificado) a pesar de que el bundle está en el binario |
| P2 | Se fuerza `Connection: close` explícitamente y se llama `esp_http_client_init()` / `cleanup()` por cada petición | `hal_http_p4.cpp:47, 41, 107` | Cero reuso de sesión: cada request paga handshake TLS completo (~700 ms) + ~45 KB de buffers TLS dinámicos transitorios |
| P3 | ✅ Lectura de respuesta en streaming por chunks de 1 KB con buffer capping | `hal_http_p4.cpp:85-101` | Ninguno — es el patrón a replicar en S3 |

### 2.2. ESP32-S3 (`hal_http_s3.cpp`) — Arduino `HTTPClient`

| # | Hallazgo | Evidencia | Impacto |
| :-: | :--- | :--- | :--- |
| S1 | Transporte fijo en `WiFiClient` plano; conexión no segura para cualquier URL | `hal_http_s3.cpp:4, 16, 26` (`begin(client, url)` con client no-secure) | **HTTPS no funciona en absoluto**: con URL https se intenta TCP plano y el handshake TLS falla siempre; no hay autofallback a `NetworkClientSecure` |
| S2 | `http.getString()` materializa la respuesta completa en heap como `String` y luego `substr()` la copia de nuevo para truncar | `hal_http_s3.cpp:48-55` | Doble alocación + riesgo de OOM *antes* de aplicar el cap: una respuesta de 5 MB alocaría ~10 MB (original + copia) con heap de ~300 KB |
| S3 | Sin `setReuse(true)` y el `HTTPClient`/`WiFiClient` se construyen por petición; `http.end()` destruye el socket | `hal_http_s3.cpp:19-20, 59` | Sin reuso de sesión; idéntico costo que P2 |
| S4 | ✅ Buffer capping lógico correcto (`maxBodyCaptureBytes`, default 32768) | `hal_http_s3.cpp:49-52` | Como concepto está bien; es la ejecución (full-alloc + copy) lo que hay que corregir |

### 2.3. Matriz de severidad

| Target | HTTPS | Streaming en RX | Keep-Alive | Prioridad de reparación |
| :--- | :-: | :-: | :-: | :-: |
| ESP32-P4 | ❌ Roto (config) | ✅ Correcto | ❌ Ausente | **Alta** (P1 fácil, P2 media) |
| ESP32-S3 | ❌ Roto (total) | ❌ Ausente | ❌ Ausente | **Crítica** (S1 bloquea el caso de uso de importar por URL https) |

---

## 3. Diseño de la Reparación

### 3.1. Principios

1. **Contrato intacto:** `core/include/cbdos/http.hpp` no se modifica. Las
   estructuras `HttpRequest`/`HttpResponse` ya admiten lo necesario (URL
   absoluta con esquema, headers, cap, timeout).
2. **Solo HALs:** ningún cambio en `core/`, UI o servicios. Todo lo
   reparado vive en los dos archivos de `bsp/*/hal/`.
3. **Streaming como regla:** el patrón de lectura por chunks del P4 se
   convierte en el patrón canónico para ambos targets.
4. **Dependencias cero:** ninguna librería nueva en `core/library.json`;
   las tools (mbedtls crt bundle, NetworkClientSecure) ya existen en el
   toolchain de cada target.
5. **Sin simulaciones:** si la sesión cacheada expira o es inválida, se
   reconstruye; nunca se reporta éxito sin llegar al servidor.

### 3.2. Reparación P4

#### R-P1 · Conectar el cert bundle (fix de HTTPS)

```cpp
// hal_http_p4.cpp — esp_http_client_config_t
#include <esp_crt_bundle.h>
// ...
config.crt_bundle_attach = esp_crt_bundle_attach;   // <-- está en sdkconfig, no en runtime
```

* El bundle FULL (200 certs CA) queda enlazado estáticamente al flash si el
  linker lo referencia; el consumo adicional de RAM en runtime es el crt
  bundle index (~1.5 KB) y no los PEM completos.
* Futuro (fuera de este plan): opción opcional por-request `dangerTls`
  (skip_verify) para LAN con certificados self-signed de routers/IoT, detrás
  de un token explícito opt-in del usuario en la UI (nunca default).

#### R-P2 · Cache de sesión por host con TTL

Problema: `esp_http_client_init/cleanup` por request destruye la sesión TLS.

Diseño:

```cpp
struct CachedSession {
    std::string hostPort;        // "192.168.1.5:443" (host:puerto, clave de cache)
    uint64_t createdAtMs = 0;    // esp_timer_get_time() de recrear
    esp_http_client_handle_t handle = nullptr;
};

esp_http_client_handle_t acquireSession(const http::HttpRequest& req);
void releaseSession(esp_http_client_handle_t h, bool reusable);
```

* **Clave:** `host:puerto` (no URL completa; el path cambia por request).
* **TTL:** 30 s de inactividad; al expirar u ocupar otra entrada, la sesión
  vieja se hace `close + cleanup` y se libera su ~45 KB de TLS.
* **Máx: 1 sesión viva** (mono-tarea del RestProber, Core 0; la complejidad
  LRU multi-sesión no se justifica aún).
* **Invalidación:** respuesta HTTP inválida (`statusCode == 0` /
  `ESP_ERR_HTTP_*`), cambio de `hostPort`, o `Connection: close` del
  servidor → la sesión no se devuelve a la cache.
* Se elimina el header explícito `Connection: close`; `esp_http_client`
  indica keep-alive HTTP/1.1 por defecto y el reuso del handle preserva la
  conexión si el servidor lo permite.
* **Efecto esperado:** ráfagas contra la misma API (patrón típico de
  RestProber: distintos endpoints del mismo host) pasan de ~750 ms (con TLS)
  a ~60-100 ms por request, según mediciones de la lib externa.

### 3.3. Reparación S3

#### R-S1 · Streaming RX (fix de memoria) + R-S2 · transporte seguro (fix de HTTPS)

Estructura resultante del `execute()`:

```cpp
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>

// Miembros persistentes de la clase (no stack):
NetworkClient        m_client;         // para http://
NetworkClientSecure  m_clientSecure;   // para https://  (solo v2/https via crt bundle)

http::HttpResponse execute(const http::HttpRequest& request) override {
    // 1.ubicar esquema de la URL: prefix "https://" decide el transporte
    // 2. reuso de conexión: HTTPClient como miembro persistente + setReuse(true)
    //    NO se vuelve a begin() si host/path-base coincide con la última sesión
    // 3. lectura de RX en streaming: EXACTAMENTE el patrón P4:
    //    Stream* s = http.getStreamPtr();
    //    while (len > 0 && CHUNK loop) { s->read(buf, readable); cap por maxBodyCaptureBytes; }
    //    http.getString() queda PROHIBIDO en este archivo
    // 4. click final: end() solo si sesión no reusable / timeout / cambio de host
}
```

Detalles:

* **Selección de transporte por esquema de URL,** no por heurística:
  `request.url.rfind("https://", 0) == 0` → `m_clientSecure`, resto → `m_client`.
  Ya que la URL es el único campo de la URL fuente y el esquema es explícito,
  no se interpreta ni se fuerza. Para cert verification, se usa
  `NetworkClientSecure::setCACerts` con el cert bundle cuando esté disponible
  en el arduino core o, en su defecto, verificación estándar con CA del
  sistema de mbedtls cuando existe.
* **Streaming RX:** nunca se alocan `String` completos. Lectura de chunks
  con un `WiFiClient` `read()`/`HTTPClient::getSize()` y acumulación en
  `response.body` (`std::string`) hasta `maxBodyCaptureBytes`, marcando
  `truncated = true` y `break` al alcanzar el cap. El resto del payload se
  descarta leyendo y soltando `read()` hasta agotar (para cerrar el socket
  de forma limpia) o abortando si el servidor permite.
* **`client.setTimeout(request.timeoutMs)`** se corrige por target: en
  arduino-esp32 v3 (NetworkClient), la unidad es ms — se conserva; en v2
  era segundos. El build es v3, así que el valor actual ya es correcto, se
  documenta para evitar regresión.

### 3.4. Cambios exactos por archivo (fuera de alcance)

| Archivo | Acción | Motivo |
| :--- | :--- | :--- |
| `core/include/cbdos/http.hpp` | Ninguno | Contrato estable |
| `core/src/network/*Service*` | Ninguno | El servicio orquesta por contrato |
| `bsp/esp32_p4_jc4880/hal/hal_http_p4.cpp` | 🔧 R-P1 + R-P2 | Fix HTTPS + sesión cacheada |
| `bsp/esp32_s3_jc3248/hal/hal_http_s3.cpp` | 🔧 R-S1 + R-S2 | Fix streaming + fix HTTPS |
| `core/include/cbdos_build_profile.h` | Ninguno | El parser del P4 ya usa `esp_crt_bundle.h` vía include en el HAL |

---

## 4. Riesgos y Mitigaciones

| Riesgo | Probabilidad | Impacto | Mitigación |
| :--- | :-: | :-: | :--- |
| Reuso incorrecto de sesión en P4 causa `HTTP_TRANSPORT_ERR` en requests encadenados advers | Media | Medio | Cada respuesta no-2xx o con `ESP_ERR_HTTP_*` invalida la sesión; el PATH `close` del server se respeta y descarta |
| Sesión cacheada `handle` con FD de socket filtrándose si la petición se cancela a mitad de lectura | Media | Alto | `releaseSession()` siempre en todos los paths `return` del `execute()`; RAII local `struct SessionGuard` con destructor que llama `releaseSession` |
| Stream de respuesta S3 con `Transfer-Encoding: chunked` y `Content-Length` nulo ← la libёл `getSize()` devuelve `-1` | Alta | Bajo | El bucle de streaming usa el propio `getStreamPtr()` + `isFinished()`/`connected()` hasta agotar, no confía en `Content-Length` |
| Bundle de CA P4 agrandando el binario | Baja | Bajo | Full bundle ya está enlazado por `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL`; `esp_crt_bundle_attach` solo enlaza símbolos del índice |
| Ráfagas de Keep-Alive saturando el socket close si el servidor no responde | Baja | Medio | TTL de 30 s invalida sesión inactiva; `esp_http_client` marca la sesión como inválida antes de reusarla si `isChunked/connected` falla |
| Regresión de redirect (`setFollowRedirects`) S3 al migrar a streaming | Baja | Bajo | `HTTPC_FORCE_FOLLOW_REDIRECTS` se conserva; los headers de la request se reinyectan por HTTPClient en redirects internos |

---

## 5. Plan de Implementación Paso a Paso

| Fase | Tarea | Archivos Clave | Estado |
| :---: | :--- | :--- | :---: |
| **Fase R1** | **Fix HTTPS en P4:** `#include <esp_crt_bundle.h>` + `config.crt_bundle_attach = esp_crt_bundle_attach` | `bsp/esp32_p4_jc4880/hal/hal_http_p4.cpp` | ⬜ |
| **Fase R2** | **Fix streaming + HTTPS en S3:** eliminar `getString()`, bucle de chunks sobre `getStreamPtr()`, doble transporte `NetworkClient`(plain)/`NetworkClientSecure`(https) por esquema de URL | `bsp/esp32_s3_jc3248/hal/hal_http_s3.cpp` | ⬜ |
| **Fase R3** | **Keep-Alive P4:** cache de sesión por host con TTL 30 s, RAII `SessionGuard`, invalidación por error o `Connection: close` | `bsp/esp32_p4_jc4880/hal/hal_http_p4.cpp` | ⬜ |
| **Fase R4** | **Keep-Alive S3:** `HTTPClient`/clientes de transporte como miembros persistentes con `setReuse(true)`, invalidación equivalente a R3 | `bsp/esp32_s3_jc3248/hal/hal_http_s3.cpp` | ⬜ |
| **Fase R5** | **Validación multi-target:** build limpio P4 + S3, matriz manual de casos (http get, https get contra bundle-CA, ráfaga keep-alive, respuesta > 32 KB truncada, respuesta chunked sin Content-Length) alimentada al spec de regresión del RestProber | Ambos BSPs + `specs/network/` | ⬜ |

**Criterio de aceptación:** una ráfaga de 5 peticiones seguidas contra el
mismo host https no paga handshake TLS después de la primera, ninguna
petición replica una respuesta en memoria por encima de
`maxBodyCaptureBytes`, y las dos implementaciones comparten el mismo
diagnóstico (`statusCode`/`rttMs`/`truncated`/`errorMessage`) ante los
mismos seis casos de la matriz de la Fase R5.
