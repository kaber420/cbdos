# Propuesta: Detección Específica Roku + Control ECP (`:8060`)

**Estado:** 💡 Propuesta Experimental / Borrador
**Target:** ESP32-P4 (JC4880P443C) y ESP32-S3 (JC3248W535)
**Dependencias Core:** `cbdos::network` (LAN Recon), `http_client.cpp`, LVGL 9.5, Lua 5.4
**Ubicación Oficial:** `specs/proposals/proposal_roku_ecp_control_and_fingerprint.md`
**Relacionado:** `proposal_network_diagnostics_and_lan_recon_suite.md`, `specs/api/luapp_specification.md`

---

## 1. Visión General y Por Qué Aporta Valor

Hoy LAN Recon clasifica todo como `isTv=true` genérico (`core/include/cbdos/lan_recon.hpp:86`, `LanScannerService.cpp:776-790`):

* Puertos cast: `8008/8009/1400/7000` → muchos falsos positivos (Chromecast, Sonos, AirPlay de Apple TV, SmartTV Samsung/LG).
* Vendor OUI / SSDP `SERVER` con `roku` (`lan_recon.cpp:146-176`) → ayuda, pero depende de OUI y de que SSDP responda.

Saber que es **específicamente un Roku (o probablemente un Roku)** sí aporta valor porque Roku expone una superficie controlable sin auth en LAN que una TV genérica no tiene:

* **ECP (External Control Protocol) en `8060/tcp`:** `keypress/*`, `launch/*`, `input/*`, `query/device-info|apps|active-app`. HTTP plano, sin pairing.
* **DIAL + ECP:** lanzar apps por ID, inyectar texto (`Lit_`), navegar.
* **Fingerprint barato:** `GET http://<ip>:8060/query/device-info` devuelve XML con `modelName`, `friendlyName`, `isTv`, `supportsECP`. Una sola petición distingue Roku real de "TV con 7000 abierto".

Propuesta en dos niveles de confianza (para no mentir en UI):

1. `Roku probable`: solo `8060/tcp` abierto.
2. `Roku confirmado`: `8060` + `device-info` válido y/o SSDP `SERVER: Roku/*` + vendor `Roku/TCL/Hisense-Roku`.

Esto cambia el dato de `📺 TV` a `📺 Roku (probable/confirmado)` y habilita acción contextual `Abrir control`.

Fuera de alcance explícito: AirPlay-video push desde ESP32-P4 al Roku (`7000/tcp`). Se descarta por FairPlay/RTSP cifrado + costo TLS. El video hacia Roku, si algún día se hace, será por pull (`MJPEG/MP4/HLS servido por CBDos` + canal privado), no por AirPlay.

---

## 2. Cumplimiento Arquitectónico CBDos

* **Regla 8 (Pureza `core/`):** Sin `lwip/esp_http` en `core/`. Reusar `core/src/network/http_client.cpp` + HAL `ILanScannerBackend::probeTcpPort/fetchHttpTitle` o nuevo `fetchHttpPath(ip,port,path)`. `core/` solo orquesta.
* **Regla 7 (Offline-First):** Todo bajo demanda. Nada en `app_main`. Si no hay WiFi, `LanReconView` muestra estado desconectado como hoy.
* **Regla 12 (Reactivo):** Probe `:8060` dentro de fase `PortScan` existente (worker FreeRTOS Core 0), fingerprint `device-info` solo en `BannerGrab` y solo si `hasPort(8060)`. Un host = una petición HTTP extra máximo.
* **Regla 2 (Multi-Target):** Sin `#ifdef ESP_PLATFORM` en `core/`. Mismo código P4/S3.

---

## 3. Estado Actual (puntos de toque)

| Lugar | Qué hay | Qué falta |
| :--- | :--- | :--- |
| `core/include/cbdos/lan_recon.hpp:32-53` `kLanReconPorts[]` (20/32) | `7000/AirPlay`, `8008/8009/Cast`, `1400/Sonos` | `8060 // Roku ECP` (pasa a 21/32, cabe en `uint32_t` bitmask) |
| `core/src/network/LanScannerService.cpp:63` `isHttpPort()` | `80/8080/443/8443` | Decidir si `8060` entra (ECP es HTTP pero devuelve XML, no `<title>`) |
| `core/src/network/LanScannerService.cpp:781-782` heurística `isTv` | `8008\|\|8009\|\|1400\|\|7000` | Agregar `hasPort(8060)` |
| `core/src/network/lan_recon.cpp:146-176` `isTvVendorName/isTvSsdpServer` | ya matchea `roku` | Reusar para nivel `confirmado` |
| `core/src/ui/views/LanReconView.cpp:657-681` `portShortName()` | `7000:AirPlay`, etc. | `case 8060: return "Roku"` |
| `core/src/lua/LuaBridge.cpp` bindings | `system/wifi_status/get_ip`, `uart/gpio/fs/ui` | Sin `cbdos.net/http` → una `.luapp` hoy **no puede** hablar a `:8060` |

---

## 4. Diseño Propuesto

### 4.1. Detección (Core, barato, primera iteración)

1. Agregar `8060` a `kLanReconPorts[]`.
2. `portShortName(8060) = "Roku"`.
3. `isTv` incluye `hasPort(8060)`.
4. Nuevo campo mínimo en `LanHostInfo` (compatible, con defaults):
   ```cpp
   // lan_recon.hpp
   enum class TvClass : uint8_t { Unknown=0, TvGeneric=0, RokuProbable=1, RokuConfirmed=2 };
   // o dos bools si se quiere evitar enum: bool isRoku{false}; bool rokuConfirmed{false};
   ```
5. Fingerprint solo si `hasPort(8060)` en fase `BannerGrab`:
   * `GET /query/device-info` (timeout ~800ms, una vez).
   * Parse mínimo por substring (sin XML parser): `<modelName>`, `<friendlyName>`, `<isTv>Yes/No`.
   * Si parse OK → `RokuConfirmed` + `banner = "Roku <modelName> (<friendlyName>)"`.
   * Si falla/timeout → `RokuProbable` (puerto abierto pero sin identidad).
6. SSDP como corroboración: si `ssdpServer` contiene `roku` + `8060` abierto → subir a `Confirmado` aunque `device-info` falle.

Costo: +1 puerto por host en `PortScan` (+~5% tiempo), +1 HTTP solo en candidatos Roku. Sin impacto en hosts normales.

### 4.2. Control Nativo Mínimo (segunda iteración)

No meter todo ECP en `core/`. Exponer primitiva genérica reutilizable:

```cpp
// ILanScannerBackend o IHttpClient (BSP implementa)
std::string httpGet(const std::string& ip, uint16_t port,
                    const std::string& path, uint32_t timeoutMs);
bool httpPost(const std::string& ip, uint16_t port,
              const std::string& path, const std::string& body);
```

Y un `RokuService` fino en `core/src/network/` que solo construye paths:

* `keypress/Home|Left|Right|Up|Down|Select|Back|Play|Rev|Fwd|VolUp|VolDown|VolMute|Power|Search`
* `keypress/Lit_<char-urlencoded>` para texto
* `launch/11` (Roku Home), `launch/tvinput.dtv`, `launch/<appId>?contentID=...`
* `query/apps`, `query/active-app` (para UI con lista de apps)

LanReconView: long-press en host `Roku*` → menú `Abrir control / Ver device-info / Abrir apps`.

### 4.3. Luapp `roku_remote.luapp` (tercera iteración, lo experimental útil)

Requiere antes exponer en `LuaBridge`:

```lua
-- propuesta cbdos.net (nuevo, mínimo)
cbdos.net.http_get("192.168.1.50", 8060, "/query/device-info") --> string|nil
cbdos.net.http_post("192.168.1.50", 8060, "/keypress/Home", "") --> true|false
cbdos.net.probe_tcp("192.168.1.50", 8060) --> true|false
```

Con eso la luapp es solo UI LVGL (`create_button/create_row`) + lógica:

* D-pad, volumen, Home/Back/OK.
* Campo texto → `Lit_`.
* Selector de apps desde `query/apps` cacheado en `/sdcard/apps/roku_cache.json`.
* Descubrimiento: lee último resultado LAN Recon o hace `probe_tcp` a IP manual.

Esto justifica la luapp frente a vista nativa: iteración sin flashear, compartible como archivo, personalizable por usuario.

### 4.4. Mockup LVGL (LanRecon + Control)

```
[LAN Recon] 192.168.1.50  AA:BB:CC...
  [📺 Roku confirmado]  TCL 55S525
  Puertos: 8060/Roku  7000/AirPlay  8008/Cast
  [ Abrir control ] [ Apps ] [ Info ]

[Control Roku] TCL Salón
  [◀] [▲][OK][▼] [▶]   [Back][Home]
  [Vol-][Mute][Vol+]    [Play/Pause]
  [________texto________] [Enviar]
  Apps: [YouTube][Netflix][HDMI1]
```

---

## 5. Seguridad y Límites

* ECP no tiene auth en LAN: solo enviar cuando el usuario toca botón. Nada de barridos automáticos de `keypress`, nada en arranque, nada persistente.
* Solo LAN (`192.168.x.x/10.x/172.16.x`). Nunca exponer por WAN ni Mesh.
* `device-info` puede traer `friendlyName` con PII (nombre del dueño) → no loguear en serial por defecto, solo mostrar en UI.
* `Lit_` inyecta texto: pedir confirmación antes del primer envío por sesión.
* Documentar que `8060` abierto ≠ vulnerabilidad: es comportamiento oficial Roku.

---

## 6. Plan por Fases

* **Fase 0 — Detección (0.5 día):** `8060` en `kLanReconPorts`, `portShortName`, `isTv+=8060`. Ver Roku como `TV + 8060/Roku`. Sin fingerprint.
* **Fase 1 — Fingerprint (1 día):** `fetchHttpPath(/query/device-info)` + `RokuProbable/Confirmed` + banner. Test contra Roku físico con AirPlay on/off.
* **Fase 2 — Control nativo mínimo (1-2 días):** `RokuService` + acciones ECP + menú contextual en `LanReconView`.
* **Fase 3 — Binding Lua (1 día):** `cbdos.net.http_get/post/probe_tcp` en `LuaBridge` con cuota/timeout y sandbox.
* **Fase 4 — `roku_remote.luapp` (1 día):** D-pad + launcher + caché. Distribuible por MicroSD sin recompilar.

Criterio de aceptación Fase 0/1: Roku aparece como `Roku confirmado (modelo)` con AirPlay apagado, y como `Roku probable` si `device-info` filtra/timeout pero `8060` abre.

---

## 7. Preguntas Abiertas

1. ¿`8060` entra en `isHttpPort()` o se crea `fetchHttpPath()` separado para no romper `fetchHttpTitle()` (XML vs HTML)?
2. ¿Guardamos `friendlyName/modelName` en `LanHostInfo` o solo en `banner` para no romper ABI/serialización?
3. ¿Control nativo completo o solo `keypress` básico + delegar el resto a luapp?
4. ¿Cache de `query/apps` en MicroSD o solo RAM (privacidad vs velocidad)?
