# Borrador: Limpieza WiFi viejo y unificación en NetworkManager

**Estado:** Borrador de planificación (sin código)
**Alcance:** Solo UI vieja duplicada. No toca HAL ni radio.
**Fecha:** 2026-09-14

---

## 1. Confirmación: hay dos gestores y el viejo quedó huérfano

**Viejo (STA manual, huérfano):**
* `core/src/ui/views/WiFiConfigView.hpp` + `WiFiConfigView.cpp` (versión `BaseView` actual)
* `core/src/ui/views/WiFiConfigView.h` (versión legacy con `HeaderBar::create()`, sin namespace `cbdos::ui`)
* Flujo: SSID/pass manual + `INetworkAdapter::connectWifi/connectWifiStatic` + `WiFiConfig` en NVS.

**Nuevo (canónico):**
* `core/src/ui/views/NetworkManagerView.hpp/cpp` — Slot 0 Radio, Slot 1 Mochila, Slot 2 USB.
* Usa `NetworkInterfaceManager` + `IRadioBackend` + scan real + `connectWifi()` solo como transporte STA.
* `ConfigView.cpp:101` ya abre `NetworkManagerView` para `id == 1`. No hay ningún `pushView(WiFiConfigView)` en todo `core/`.

### Anatomía del NetworkInterfaceManager (no es un archivo gigante)
* Son solo dos archivos chicos: `core/include/cbdos/network_interface.hpp` (83 líneas) + `core/src/network/network_interface.cpp` (54 líneas). Sin lógica pesada.
* Es un singleton con una regleta de 4 slots: `registerInterface(slot, iface) / getInterface(slot) / setAllOffline() / isAnyInterfaceActive()`. Solo guarda punteros.
* Contrato por interfaz (`INetworkInterface`): `getName / getType / getMode / setMode / isReady / sendPacket / setPacketRecvCallback` + opcionales `setChannel / getMacAddress / getTxPower`.
* Implementaciones reales (ahí sí está el peso, una por BSP):
  * Slot 0: `S3NetworkInterface` (`hal_radio_s3.cpp:296`) / `P4NetworkInterface` (`hal_radio_p4.cpp:210`).
  * Slot 1: mochila (registro dinámico, puede estar vacío).
  * Slot 2: `UsbCdcRadioTransport` (`hal_mesh_p4.cpp:30`). Slot 3: libre.
* Tipos: `RadioPacket` (ESP-NOW/LoRa), `IpNetwork` (WiFi STA/AP), `BluetoothLe`, `SerialModem`.
* Matiz AP: `InterfaceMode::WifiAccessPoint` (`network_interface.hpp:24`) existe en el contrato y `getType()` lo trata como `IpNetwork`, pero el `setMode()` de S3 (`hal_radio_s3.cpp:315`) y P4 (`hal_radio_p4.cpp:229`) **no tiene rama para AP** — se guarda el modo sin encender ningún softAP. Por eso el AP es stub y va en el plan del editor web, no en esta limpieza.

Evidencia de que el viejo es muerto:
* `grep WiFiConfigView` solo lo encuentra en su propio `.cpp/.hpp/.h`, en `core/CMakeLists.txt:32` (compilado pero jamás abierto), en `README.md:308` y en specs viejas.
* Doble cabecera `.h` vs `.hpp` con la misma clase = resto de migración a `BaseView` sin borrar el legacy.

Aclaración importante: `INetworkAdapter` (`network.hpp`) **no es muerto**. `NetworkManagerView.cpp:539` lo sigue usando para conectar a la AP elegida. Lo que muere es solo el View viejo, no el backend STA.

---

## 2. Qué limpiar (inventario)

### Borrar (UI muerta)
1. `core/src/ui/views/WiFiConfigView.h` — legacy pre-`BaseView`, nada lo incluye.
2. `core/src/ui/views/WiFiConfigView.hpp` + `WiFiConfigView.cpp` — huérfanos desde que `ConfigView` apunta al `NetworkManager`.
3. Entrada `"src/ui/views/WiFiConfigView.cpp"` en `core/CMakeLists.txt:32` + equivalente en `bsp/esp32_s3_jc3248` (PlatformIO globs o `src_filter` si lo lista explícito — verificar antes de borrar).

### Conservar (sigue en uso)
* `core/include/cbdos/network.hpp` (`INetworkAdapter`, `connectWifi`, `connectWifiStatic`) — lo usa el nuevo manager.
* `WiFiConfig` en `config_manager.hpp:8` (`loadWiFi/saveWiFi`) — guarda la STA elegida desde el nuevo manager.
* `hal_network_s3.cpp` / `hal_network_p4.cpp` — backends STA.

### Revisar antes de borrar (posibles ataduras)
* `language.hpp/cpp` `STR_WIFI_*` (`STR_WIFI_TITLE`, `STR_WIFI_ENABLE`, `STR_WIFI_SSID`, `STR_WIFI_SAVE`...): ver cuáles usa solo el viejo y cuáles reutiliza `RadioConfigView.cpp:183` u otros. No borrar strings compartidos.
* `RadioConfigView` vs `NetworkManagerView`: solapan parcial (canal/potencia). Este borrador no lo toca, pero anotarlo como siguiente duplicado a unificar.
* Docs que citan `WiFiConfigView`: `README.md:308`, `specs/ROADMAP.md:56`, `specs/proposals/proposal_network_diagnostics_and_lan_recon_suite.md:34`, `specs/architecture/*`. Actualizar a `NetworkManagerView` tras el borrado.

---

## 3. Plan de limpieza por fases (seguro, dual-target)

* **F0 — Foto:** `git status`, `idf.py build` (P4) + `pio run -d bsp/esp32_s3_jc3248` (S3) en verde antes de tocar nada. Anotar bin size base.
* **F1 — Desenganchar de build (sin borrar archivos):** comentar/sacar `WiFiConfigView.cpp` de `core/CMakeLists.txt` + filtro S3, compilar ambos targets. Si algo lo referenciaba, aquí revienta y se aborta.
* **F2 — Borrar legacy:** eliminar `WiFiConfigView.h`. Compilar ambos targets.
* **F3 — Borrar View viejo:** eliminar `WiFiConfigView.hpp/cpp`. Compilar ambos targets + probar en device: `Config > Red` abre `NetworkManagerView`, scan + connect STA sigue funcionando (regresión crítica, porque el transporte es el mismo `connectWifi`).
* **F4 — Higiene:** quitar `STR_WIFI_*` usados solo por el viejo (si los hay huérfanos), actualizar `README.md:308` y specs que lo citan, cerrar con build dual + nota de bytes ahorrados en flash.

Criterio de éxito F3: ningún `grep WiFiConfigView` en `core/` + `bsp/` salvo este borrador y specs históricas marcadas como tal.

---

## 4. Riesgos

1. **S3 PlatformIO compila por glob:** si no lista archivos explícito, borrar el `.cpp` basta; si usa `src_filter`, hay que editarlo o rompe.
2. **Strings i18n compartidos:** borrar un `STR_WIFI_*` que use `RadioConfigView` rompe ES/EN. Verificar con grep por cada ID antes.
3. **Confundir UI muerta con backend vivo:** no tocar `network.hpp`, `hal_network_*`, ni `WiFiConfig` en esta limpieza. El AP futuro (`ApConfig`, `WifiAccessPoint` real) se monta sobre el gestor nuevo, no sobre el viejo.

---

## 5. Preguntas para cerrar

1. ¿Borro también el `.h` legacy en F2 o lo quieres archivado en `specs/` como referencia?
2. ¿Actualizo `README.md:308` y ROADMAP en el mismo cambio o en commit docs aparte?
3. ¿Siguiente duplicado a unificar tras este: `RadioConfigView` vs Slot 0 del `NetworkManager`?
