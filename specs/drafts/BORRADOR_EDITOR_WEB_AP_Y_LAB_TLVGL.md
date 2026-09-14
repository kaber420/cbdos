# Borrador: Editor Web por AP + Lab TLVGL/ESP-NOW

**Estado:** Borrador de planificación (sin código, solo diseño)
**Targets:** ESP32-S3 (JC3248W535, WiFi nativo) y ESP32-P4 (JC4880P443C, vía C6 por SDIO)
**Fecha:** 2026-09-14

---

## 1. Aclaración previa (lo que causó confusión)

Hay **dos webs distintas**, no mezclarlas:

| Web | Dónde vive | Para qué | Requiere internet |
|:---|:---|:---|:---:|
| **Web pública** (`site/` -> `docs/` -> `kaber420.github.io/cbdos`) | GitHub Pages | Docs, manuales, `LuaPlayground` (hoy read-only) | Sí |
| **Editor AP** (este borrador) | Dentro del ESP32 (Flash/SD) | Tablet se conecta al WiFi del deck y edita Lua/Ducky directo a `/sdcard/` | No, offline-first |

Este documento planea solo el **Editor AP**. El Lab Svelte público es opcional y secundario.

Aclaración Lua: el editor Lua es **prioridad #1** (es lo más usado). Lo único que se descarta en v1 es *ejecutar* Lua dentro del navegador (wasmoon/fengari, pesado e innecesario). Editar Lua sí, ejecutar Lua lo hace el ESP32 con `LuaEngine` + `LuaRunnerView`.

---

## 2. Objetivo

Desde una tablet, sin PC ni internet:

1. Conectarse al AP `CBDos-S3` / `CBDos-P4`.
2. Abrir `http://192.168.4.1/` en el navegador.
3. Editar con comodidad (teclado real de tablet) scripts **Lua/Luapp** (prioridad 1), **DuckyScript `.dd`** (prioridad 2) y páginas **TLVGL/CBML** (prioridad 3).
4. Guardar directo a MicroSD (`/sdcard/apps/`, `/sdcard/scripts/`, `/sdcard/www/`).
5. Opcional: ejecutar el script en el propio deck tras guardar.

---

## 3. Gap actual verificado (por eso no existe aún)

* `core/include/cbdos/network.hpp` + `hal_network_s3.cpp` + `hal_network_p4.cpp`: solo `WIFI_STA` + `connectWifi()`. No hay `startAP()`.
* `core/include/cbdos/http.hpp` + `hal_http_s3.cpp`: solo `IHttpClient` (saliente). No hay `IHttpServer`.
* `core/include/cbdos/socket.hpp`: solo `connect/send/recv`. No hay `listen/bind/accept`.
* `core/include/cbdos/storage.hpp`: sí existe `readFile/writeFile/listDir` — es la base para el API del editor, no hay que inventarla.

Conclusión: hay que diseñar `AP + HTTP Server + API archivos`. Nada de esto toca LVGL ni rompe pureza de `core/` si se hace como HAL nuevo.

---

## 4. Diseño propuesto: Mini-Editor AP (en el device)

No servir el `site/` Svelte desde el ESP32 (no cabe, requiere build pesado). Servir **un solo `editor.html`** autocontenido (~150KB gzipped) desde LittleFS o `/sdcard/www/`, con CodeMirror mínimo vendorizado (no CDN, porque no hay internet en campo).

### 4.1 API mínima (mapea 1:1 a `storage.hpp` existente)

```
GET  /                              -> editor.html (estático)
GET  /api/files?path=/sdcard/apps   -> storage::listDir()
GET  /api/file?path=/sdcard/apps/x  -> storage::readFile()
POST /api/save {path, content}      -> storage::writeFile()
POST /api/run  {path}               -> LuaRunnerView / DuckyInterpreter (fase 2)
GET  /api/status                    -> heap, SD montada, AP clientes
```

### 4.2 Modos del editor (en este orden)

1. **Lua / `.luapp` (v1):** highlight Lua, snippets `cbdos.ui/hid/mesh/audio/storage/sys`, templates de `apps/*.luapp`, validación estática de APIs (lista blanca, sin ejecutar). Guardado a `/sdcard/apps/`.
2. **Ducky / `.dd` (v2):** highlight custom (`STRING, DELAY, GUI, ENTER, ALT...`), linter contra comandos del `DuckyInterpreter` real, templates BadUSB. Guardado a `/sdcard/scripts/`.
3. **TLVGL / CBML (v3):** textarea HTML-like + botón compilar (port del `tlv_dict.py:encode_hybrid_text` a JS en el browser) + preview canvas `480x800 / 320x480` + dato de bytes y nº de tramas ESP-NOW. Sirve como banco de pruebas del punto 5.

### 4.3 Flujo AP (on-demand, respeta offline-first)

```
Deck (AP apagado por defecto) -> usuario activa "Editor Web" en UI
  -> startAP("CBDos-S3", 192.168.4.1) + httpd start
  -> tablet se conecta, edita, guarda a SD
  -> usuario apaga AP -> WiFi off, vuelta a bajo consumo
```

SSIDs sugeridos: `CBDos-S3-XXXX`, `CBDos-P4-XXXX` (sufijo MAC). Sin contraseña en campo o con clave impresa en Config. Captive portal solo si se quiere comodidad (DNS 192.168.4.1), opcional.

---

## 5. Diseño prueba TLVGL por ESP-NOW con diccionarios

Usar el propio editor AP (modo 3) como herramienta de prueba, no hardware extra al inicio.

* **Base existente:** `tools/tlvgl_gateway/tlv_dict.py` (VIP `0x80-0xBF` 1B + Core `0xC0` 2B), `tlvgl_compiler.py`, `content/*.tlvgl`, `specs/network/plan_tlvgl_esp_now.md` (micro-chunks 250B: `[Idx|Tot 1B][MsgID 1B][Mesh 3B][payload 245B]`).
* **Fase T1 (sin radio):** compilar mismas páginas con/sin diccionario, medir bytes. Referencia esperada: `config ~195B = 1 trama`, `clima ~321B = 2 tramas`, `bento ~365B = 2 tramas`. Criterio: % ahorro + nº tramas evitadas.
* **Fase T2 (loopback PC):** `tlvgl_server.py + gateway_router.py` en PC + dongle `tools/espnow_usb_bridge` (C3), pedir `espnow://clima.mesh`, verificar reensamblado por `MsgID`.
* **Fase T3 (aire real):** cliente S3 nativo (`TlvBrowserView::render`) contra nodo C3 sensor headless (plantilla `clima.tlvgl` + telemetría inyectada). Medir latencia y pérdida con 1 vs 2 chunks.
* P4 vía C6 queda fuera de T1-T3 (requiere RPC AP/ESP-NOW en `esp32_c6_slave`).

---

## 6. Fases sugeridas (solo planificación)

* **F0 — Decisión:** S3 primero (nativo), P4/C6 después. Confirmar ubicación del `editor.html` (LittleFS vs `/sdcard/www/`).
* **F1 — S3 AP + Editor Lua:** `startAP()` + `IHttpServer` + `/api/files/save` + editor Lua mínimo. Criterio de éxito: editar desde tablet y ver el archivo en `FileManagerView`.
* **F2 — Ducky:** añadir modo `.dd` + linter + `/api/run` hacia `DuckyInterpreter`.
* **F3 — TLVGL:** añadir modo CBML + preview + métricas T1.
* **F4 — T2/T3 radio:** pruebas ESP-NOW reales.
* **F5 — Port P4/C6:** replicar AP/httpd vía ESP-Hosted SDIO.

Fuera de alcance v1: auth multiusuario, HTTPS en el AP, ejecutar Lua en el browser, servir Svelte completo desde el ESP32, editor multi-lenguaje genérico (Python/C/etc.).

---

## 7. Riesgos abiertos

1. **P4 sin radio propia:** todo AP/ESP-NOW depende del firmware `bsp/esp32_c6_slave`. Si el RPC ESP-Hosted no soporta AP, F5 se bloquea.
2. **RAM S3:** servir `editor.html` + httpd + LVGL a la vez con 8MB PSRAM es viable pero hay que medir con `heap_caps` y gzip obligatorio.
3. **Seguridad AP abierto:** cualquiera cerca puede escribir la SD. Mitigación: AP solo bajo demanda + token simple en `/api/save` + avisar en UI.
4. **Cohabitar WiFi AP + ESP-NOW:** mismo canal (fijar canal 1 en ambos) o se pisan. Definir en `RadioConfigView`.

---

## 8. Preguntas para cerrar el plan

1. ¿S3 primero y P4 después, ok?
2. ¿`editor.html` en LittleFS (siempre disponible) o en SD (`/sdcard/www/`, editable)?
3. ¿AP abierto o con clave? ¿Captive portal sí/no?
4. ¿`/api/run` en v1 o solo guardar y ejecutar a mano desde `LuaRunnerView`?
