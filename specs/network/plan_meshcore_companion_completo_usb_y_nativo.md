# Plan Técnico: MeshCore Completo en CBDos — Fase USB actual + Fase Nativa futura SX1262/SX1280

**Versión:** 1.0.0 — `RFC-CBDOS-MESHCORE-FULL`
**Estado:** Aprobado para implementación por fases
**Targets:** ESP32-P4 `JC4880P443C` / ESP32-S3 `JC3248W535`
**Alcance:** `core/include/cbdos/meshcore/*`, `core/src/meshcore/*`, `core/src/ui/views/MeshCoreView.*`
**Reglas:** Regla 7 Offline-First, Regla 8 Pureza `core/` (solo `cbdos::serial`), Regla 12 Cero polling en UI.

> Motivación: el MeshCore actual de CBDos solo hace canales públicos. Nadie usaría MeshCore incompleto. Este plan lo lleva a paridad funcional con la app Android oficial (Contactos / Canales / Mapa / Settings), manteniendo la estrategia actual por módulos USB, y deja preparada la migración futura a radio nativa sin MCU extra.

---

## 1. Decisión de arquitectura: por qué USB ahora es lo mejor

### 1.1. Topología actual (Fase USB)

```
┌─────────────────────────────────────────────────────────┐
│ CBDos Core (agnóstico, sin drivers)                     │
│  MeshCoreView LVGL9 ──> MeshCoreClient (parser LE)      │
│  ──> cbdos::serial open/read/write ("jp1"/"usb0")       │
└──────────────────────┬──────────────────────────────────┘
                       │ USB CDC-ACM 115200 / UART JP1
┌──────────────────────▼──────────────────────────────────┐
│ Dongle externo: ESP32-C3/S3 + SX1262/SX1280             │
│  Firmware MeshCore Companion oficial (BLE/USB/Serial)   │
│  ──> LoRa aire (flood / direct, ACKs, adverts)          │
└─────────────────────────────────────────────────────────┘
```

* `P4`: `USB Host CDC-ACM` en `USB-C` (`UsbDeviceManager` + `hotplug`) o `UART JP1 TX:32 RX:28 RST:54`.
* `S3`: `UART TX:15 RX:16` o USB-serial según placa.
* Framing serie: `App→Radio '<' 0x3C + len u16 LE + frame`, `Radio→App '>' 0x3E + len u16 LE + frame`.

### 1.2. Por qué no nativo todavía

| Criterio | Módulo USB (actual) | Nativo SX1262/SX1280 directo en P4/S3 (futuro) |
|---|---|---|
| Firmware radio | Oficial MeshCore, probado, con updates vía `flasher.meshcore.io` | Habría que portar stack MeshCore a CBDos o escribir driver LoRa + routing + crypto |
| Consumo | +~40-80 mA por MCU extra (C3/S3 dongle alimentado por VBUS 5V) | Ahorro real (~solo SX1262 ~10-30 mA RX, ~100 mA TX), sin segundo MCU |
| Aislamiento | Crash del dongle no tumba CBDos; `reboot` por CLI | Crash de radio tumba UI si no se aisla en task con watchdog |
| Desarrollo | Cero driver RF, solo Companion Protocol | SPI + DIO/RESET/BUSY, timing LoRa, Duty Cycle 1%, calibración TCXO, regiones |
| Backpacks | Plug&Play: misma mochila sirve en P4/S3/PC | Cada mochila `solo-chip` necesita PinMux JP1 + perfil en `Backpack Manager` |
| Tiempo | Semanas (protocolo + UI) | Meses (driver + stack + certif. + pruebas de alcance) |

Conclusión: **ahora USB, después nativo**. El `MeshCoreClient` se diseña para que el transporte sea intercambiable: hoy `cbdos::serial`, mañana `IRadioHAL` sin cambiar la UI.

### 1.3. Coexistencia con malla interna

* `MeshConfigView` = malla propia CBDos `ESP-NOW/Torres/IPv4 10.x` (`MeshEngine`). No tocar.
* `MeshCoreView` = red LoRa MeshCore interoperable con Android/iOS/Web. Renombrar títulos en UI para no confundir: `"Malla Interna"` vs `"MeshCore LoRa"`.

---

## 2. Gap analysis: qué tenemos vs app Android real

### 2.1. App Android oficial (referencia)

* `Header:` batería + nombre dongle, `Advert [Zero Hop / Flood / Clipboard]`, `⚙️ Settings`, `⋮ [Disconnect, Add Contact, Add Channel, Discover, My QR, Internet Map, Tools, About]`.
* `Tabs abajo: Contactos | Canales | Mapa`.
* `Contactos:` buscador, fila `[icono tipo, nombre, pubkey hex, Direct/Flood/X hops + hace Xh, ★, badge unread]`, `⋮ [Details, Share, Set Path, Reset Path, Ping Zero Hop, Remove, Favourite]`, `Details [nombre editable, pubkey copy, GPS+distancia, tipo, Last Advert, Out Path editable]`, `Tools [Send DM, Remote Mgmt, Telemetry, View on Map]`.
* `DM 1-1:` cifrado, `ACK tag, retry 5x direct / 3x flood, auto-reset a flood, reply, SNR, hops, paths`.
* `Canales:` `[🌐 Public / # Hashtag / 🔒 Privado, nombre, última actividad, unread]`, `Share QR+secret, Rename, Remove, Mute All/Mentions/None`, dentro `sender, sent vs received time, SNR dB, hops, View Paths por repetidor`.
* `Settings:` `Name+lat/lon+Share in Advert, Radio presets región, Identity Key export/import (fw>=1.7), BT PIN, Auto Add All/por tipo, Overwrite Oldest, Message [Auto Retry, Auto Reset, N Acks, Mark Delivered Faster, Save Drafts, Show Hops], Tools [Import/Export Config, Purge, Logs, Factory Reset, Reboot], Info [model/build/ver/maxContacts/maxChannels/storage]`.
* `Mapa:` nodos con posición, filtros tipo/tiempo, offline tiles, long-press fijar posición, MGRS/GPX/line-of-sight.

### 2.2. Estado actual CBDos

* `Cmd implementados:` `APP_START 0x01, SEND_CHANNEL 0x03, GET_MSG 0x0A, BATTERY 0x14, DEVICE_QUERY 0x16, GET_CHANNEL 0x1F, SET_CHANNEL 0x20` + CLI texto (`set name/radio/tx/freq, reboot`).
* `Parseados:` `OK 0x00, ERROR 0x01, SELF_INFO 0x05, MSG_SENT 0x06, CONTACT_MSG 0x07/0x10, CHANNEL_MSG 0x08/0x11, NO_MORE 0x0A, BATTERY 0x0C, DEVICE_INFO 0x0D, CHANNEL_INFO 0x12, MESSAGES_WAITING 0x83`.
* `Ignorados:` `CONTACT_START 0x02, CONTACT 0x03, CONTACT_END 0x04, ADVERT 0x80, ACK 0x82, NEW_ADVERT 0x8A, TRACE, TELEMETRY 0x8B`. Por eso no hay agenda aunque la UI quisiera mostrarla.
* `UI:` tabs arriba `Chats/Canales/Radio`, log mezclado canales+DMs, sin selector de destinatario DM, sin `SNR/hops/paths/ACK`, sin `QR/share/mapa/advert`.

---

## 3. Plan por fases (USB primero)

### Fase 1 — Backend Contactos + DM + Advert (desbloquea todo)

**Toca:** `core/include/cbdos/meshcore/meshcore_types.hpp`, `meshcore_client.hpp/.cpp`

Nuevos comandos App→Radio:

| Cmd | Valor | Payload | Respuesta |
|---|---|---|---|
| `SEND_DM` | `0x02` | `[timestamp u32 LE][dest pubkey 32B][texto UTF-8 ≤133]` | `MSG_SENT 0x06 [flood+tag u32+timeout u32]` luego `ACK 0x82` |
| `GET_CONTACTS` | `0x04` | `[since u32 LE opcional]` | `CONTACT_START 0x02 [count u32] + CONTACT 0x03*N + CONTACT_END 0x04 [most_recent_lastmod u32]` |
| `SET_TIME` | `0x06` | `[epoch u32 LE]` | `CURRENT_TIME 0x09` |
| `SEND_ADVERT` | `0x07` | `[type: 0=zero-hop, 1=flood]` | `OK 0x00` |
| `SET_ADVERT_NAME` | `0x08` | `[nombre UTF-8]` | `OK` |
| `ADD_UPDATE_CONTACT` | `0x09` | `[pubkey32][type][flags][out_path_len i8][out_path64][...]` | `OK/ERROR` |
| `RESET_PATH` | `0x0D` | `[pubkey32]` | `OK/ERROR` |
| `SET_ADVERT_LATLON` | `0x0E` | `[lat i32 LE][lon i32 LE][alt i32 opt]` | `OK` |
| `REMOVE_CONTACT` | `0x0F` | `[pubkey32]` | `OK/ERROR` |
| `SHARE_CONTACT` | `0x10` | `[pubkey32]` | `OK` (re-emite advert zero-hop) |
| `EXPORT_CONTACT` | `0x11` | `[pubkey32 opt; vacío = propio]` | `EXPORT_CONTACT 0x0B [card bytes]` |
| `IMPORT_CONTACT` | `0x12` | `[card bytes]` | `OK` + `PUSH ADVERT 0x80` |

Nuevos parses Radio→App:

```
CONTACT 0x03:
 [pubkey32][type u8: 1=Chat 2=Repeater 3=Room 4=Sensor][flags u8]
 [out_path_len i8: -1=flood][out_path 64B][name 32B null-pad]
 [last_advert u32][lat i32][lon i32][lastmod u32]
ADVERT 0x80 / NEW_ADVERT 0x8A: mismo layout que CONTACT
ACK 0x82: [tag 6B hex] — matchear con MSG_SENT.tag para marcar entregado
```

Nueva struct:

```cpp
struct MeshContact {
  uint8_t pubkey[32]; std::string prefixHex12; std::string name;
  uint8_t type = 0; uint8_t flags = 0; int8_t outPathLen = -1;
  uint8_t outPath[64] = {0};
  uint32_t lastAdvert = 0; double lat = 0, lon = 0; uint32_t lastmod = 0;
  float lastSnr = 0; uint8_t hops = 0;
  bool favourite = false; uint32_t unread = 0; bool valid = false;
};
struct DMThread { std::string prefixHex12; std::vector<ContactMessage> msgs; };
```

Nuevas APIs:

```cpp
bool queryContacts(uint32_t since=0);
bool sendDM(const uint8_t pubkey[32], const std::string& text);
bool sendDMByPrefix(const std::string& prefixHex12, const std::string& text);
bool resetPath(const uint8_t pubkey[32]);
bool removeContact(const uint8_t pubkey[32]);
bool shareContact(const uint8_t pubkey[32]);
bool sendAdvert(bool flood);
bool setAdvertName(const std::string& name);
bool setAdvertLatLon(double lat, double lon);
bool addOrUpdateContact(const MeshContact& c);
bool setDeviceTime(uint32_t epoch);
const std::vector<MeshContact>& getContacts() const;
const DMThread* getThread(const std::string& prefixHex12) const;
```

Lógica DM fiable:

1. `sendDM` → `MSG_SENT{flood, tag, timeoutMs}` → guarda `pending[tag]={pubkey, texto, intentos=0, deadline}`.
2. `ACK tag` → marca `entregado`, `unread=0` propio, callback `onAck`.
3. Timeout sin ACK → reintenta `hasta 5x direct / 3x flood`, último intento fuerza `flood` (auto-reset path). Si falla todo → `fallido + botón Reintentar` en UI.
4. `MESSAGES_WAITING 0x83` → `auto-drain GET_MESSAGE` ya existente, sin polling UI (Regla 12: `m_contactsDirty/m_chatDirty`).

Persistencia — flash interno primero, SD como extensión (sin dependencia de MicroSD):

* NVS (20 KB) prohibido para MeshCore: solo escalares de arranque. Todo MeshCore va por `cbdos::storage::IStorageBackend` (`core/` puro, sin `Preferences/nvs_flash`).
* Partición disponible: `spiffs 4 MB (0x400000)` en `partitions.csv`, montada en `/spiffs` con alias `/flash`. `normalizePath()` en `hal_storage_p4.cpp` enruta por defecto a flash interna; `init()` monta SPIFFS siempre y SD en modo best-effort. Sin SD el sistema sigue 100% operativo.
* Presupuesto MeshCore (<300 KB): `contactos 100×~120 B ≈ 12 KB, canales 8×~50 B, DMs 20 hilos×50 msgs×~150 B ≈ 150 KB, settings pocos KB`. Cabe sobrado aun con wallpapers/fuentes en la misma partición.
* Límites SPIFFS a respetar: `max_files 8 (P4) / 10 (S3)`, sin dirs reales (flat), desgaste por escrituras, `format_if_mount_failed=true` (riesgo de pérdida si se corrompe). Mitigación: pocos archivos, escritura coalescente (solo si `dirty` + cada 30 s, no por mensaje), escritura atómica (`tmp + rename`), un solo `contacts.msgpack` + un solo `threads.msgpack` + un solo `channels.msgpack` en vez de un archivo por hilo.

```
Tier 1 crítico — siempre en /flash (arranca sin SD):
 /flash/data/meshcore/contacts.msgpack   // agenda + favs + paths + lastmod
 /flash/data/meshcore/channels.msgpack   // 8 slots nombre+secreto+kind+mute
 /flash/data/meshcore/self.msgpack       // alias, lat/lon, presets radio, msg settings
 /flash/data/meshcore/threads.msgpack    // últimos 50 msgs por hilo (cap flash)
Tier 2 bulk — solo si hay SD (historial largo, logs):
 /sdcard/cbdos/data/meshcore/threads_full.msgpack  // hasta 200 msgs/hilo
 /sdcard/cbdos/data/meshcore/telemetry.log         // telemetría/trace
 /sdcard/cbdos/data/meshcore/gpx_export.gpx        // mapa
```

* `MeshStore::resolvePath()`: lee Tier 1 primero, si hay SD hace overlay con Tier 2; al insertar SD migra bulk a SD y deja espejo crítico en flash; al quitar SD sigue con Tier 1 sin errores.
* `makeDir("/flash/data/meshcore")` en P4 es no-op (SPIFFS flat) pero necesario en SD/FAT; llamar siempre antes de `writeFile`.

Criterio de aceptación Fase 1: `queryContacts` lista nodos reales del dongle, `sendDM` entrega con `✔`, `sendAdvert` aparece en otro teléfono Android.

### Fase 2 — UI Contactos paridad Android

**Toca:** `core/src/ui/views/MeshCoreView.hpp/.cpp` (rework, no parche)

```
Header: [◀] MeshCore LoRa | batería+nombre | [📢 Advert] [＋]
Tabs abajo: [👥 Contactos] [💬 Canales] [🗺 Mapa] [📻 Radio]
```

* `Tab Contactos:` search bar, fila `[icono tipo, nombre, prefix12, Direct/Flood/X hops + hace Xh, ★, badge unread]`. Tap → conversación. Long-press/`⋮` → `Details/Set Path/Reset Path/Ping Zero Hop (CLI ping)/Share/Remove/Fav`.
* `Vista Conversación:` burbujas con `texto, sent vs received, SNR, hops`, estado `🕒→✔→✔✔/✖ + Reintentar`. `textarea + Enviar + Reply`.
* `Vista Details:` `nombre editable, pubkey copy, GPS+distancia haversine km/mi, tipo, Last Advert fecha exacta, Hops Away + Out Path hex editable`.
* `Header Advert:` popup `Zero Hop / Flood / Copy card`. `＋:` `Add manual (pubkey hex64+nombre+tipo), Import card (pegar), Discover (queryContacts)`.
* Reactivo: `timer 40 ms solo hace client.process()` y si `m_contactsDirty` → `refreshContactsList()`. Cero polling de serie en UI.

Pantallas pequeñas: `S3 320x480` usa lista compacta 44 px/fila, conversación a pantalla completa; `P4 480x800` permite lista + detalle lado a lado si hay espacio, si no igual que S3.

### Fase 3 — Canales completos

* Lista con `[🌐 Public/# Hashtag/🔒 Privado, nombre, última actividad, unread]`. Reusa `classifyChannel()` actual + `secretToHex()`.
* Menú: `Share (QR futuro + copy secret hex + copy hashtag), Rename (SET_CHANNEL mismo idx), Remove (SET_CHANNEL vacío+ceros), Mute All/Mentions/None (solo local, persistido)`.
* Dentro: cada `ChannelMessage` ya trae `snr/pathLength/timestamp` — mostrar `SNR dB, hops, sent vs received`. Long-press `Copy/Reply/Block(sender mute)/View Paths`.
* `Add Channel:` `Create Private (random16 via RNG BSP), Join Private (secret hex32), Join Public (secreto conocido), Join Hashtag (#nombre→sha256[0..16))`, `Scan QR` reservado.
* `View Paths:` por ahora `path_len + SNR + lista de repetidores conocidos cuyo prefix coincida` (limitación MeshCore 1-byte hash: mostrar `Known Repeaters que pudieron intervenir`).

### Fase 4 — Settings + Mapa texto

* `Tab Radio:` secciones `Enlace Serial (jp1/usb0 + Reconectar + handshake), Identidad (alias, lat/lon, Share Position checkbox → SET_ADVERT_LATLON + loc policy), Radio presets región (EU868/US915 + freq/BW/SF/CR/TX → CLI set radio/tx + reboot), Contactos (Auto Add All/por tipo → manual_add_contacts + filtro local, Overwrite Oldest local), Mensajes (Auto Retry, Auto Reset, N Acks, Save Drafts, Show Hops — locales), Tools (Info/Battery/Poll/Reboot ya existen + Purge/Factory Reset/Import-Export Config), Info (SELF_INFO+DEVICE_INFO ya existen)`.
* `Tab Mapa:` v1 sin tiles (offline-first, sin RAM para mapa): `lista nodos con GPS + distancia + hace Xh + filtro tipo/tiempo + [Ver en mapa externo -> export GPX] + long-press fijar mi posición`. Tiles offline es Fase 5.
* `Identity Key export/import:` solo si `fw>=1.7`; si no, ocultar con aviso.

### Fase 5 — Avanzado (post-paridad)

* `Telemetry 0x8B, Trace path, Channel Datagram 0x3E/0x1B data_type`, `Remote CLI repeater`, `Repeater finder + noise floor viewer`, bindings Lua `cbdos.meshcore.send_text/on_message/get_nodes/get_contacts/send_advert`, notificaciones con badge gris/rojo como Android.

---

## 4. Integración en plataforma CBDos (módulos USB hoy)

### 4.1. Detección y energía

* `UsbDeviceManager` hotplug → si `VID/PID` CDC coincide con dongle conocido → toast `"Dongle MeshCore en usb0"` + `MeshCoreClient::connect("usb0")` sin borrar agenda persistida.
* `JP1 backpack LoRa (C3/S3 con MeshCore):` `Backpack Manager` lee `NTAG213 → driver=meshcore-companion, uart TX/RX/baud` → `connect("jp1")`. Al desacoplar → `disconnect()` + GPIOs Hi-Z.
* Energía: VBUS 5V alimenta dongle; CBDos no duerme el puerto mientras haya `pending ACK` o `drainPending`. `Suspend` solo cuando `cola vacía + sin threads unread críticos` o con `wake en MESSAGES_WAITING` si el BSP lo soporta.
* `Offline-first:` arranca sin dongle, muestra agenda cacheada + `Estado: Desconectado`, todo editable local y sincronizado al reconectar.

### 4.2. HAL y pureza

* `core/` solo usa `cbdos::serial::{open,close,available,read,write,writeString,getAvailablePorts,setHotplugCallback}`. Nada de `driver/gpio.h` ni `Arduino.h` en `meshcore/*` ni `MeshCoreView`.
* `BSP P4:` `hal_serial_p4.cpp` implementa `UART JP1 + USB Host CDC`. `BSP S3:` `hal_serial_s3.cpp` implementa `UART + USB-serial`.
* Tests host: `processByte()` inyecta frames `>` sintéticos sin hardware (ya expuesto para tests).

### 4.3. Archivos a crear/modificar

```
Modificar:
 core/include/cbdos/meshcore/meshcore_types.hpp   // + MeshContact, DMThread, ContactType, nuevos Cmd/PacketType
 core/include/cbdos/meshcore/meshcore_client.hpp  // + agenda, DM, advert, paths, callbacks
 core/src/meshcore/meshcore_client.cpp            // + parsers CONTACT/ADVERT/ACK, retry, persist hooks
 core/src/ui/views/MeshCoreView.hpp/.cpp         // + rework 4 tabs + conversación + details + settings/mapa
 specs/ROADMAP.md                                 // + marcar MeshCore Full por fases
Crear:
 specs/network/plan_meshcore_companion_completo_usb_y_nativo.md  // este documento
 core/src/meshcore/meshcore_store.*               // (Fase 1) serialización msgpack a IStorageBackend
 core/src/ui/views/MeshCoreContactView.*          // (Fase 2, opcional si MeshCoreView crece >1500 líneas)
 tools/meshcore_companion_vectors/                // (Fase 1) vectores binarios de prueba CONTACT/ACK/MSG
```

Estimación: `Fase 1: 3-5 días, Fase 2: 4-6 días, Fase 3: 2-3 días, Fase 4: 3-4 días` en P4+S3 con pruebas con 2 dongles reales + app Android como referencia.

---

## 5. Fase futura: MeshCore nativo sin MCU extra (SX1262/SX1280 solo-chip)

Objetivo: mochila `solo SX1262/SX1280 + antena` en `JP1 SPI`, sin C3/S3 extra, para ahorrar batería y coste. CBDos corre el stack MeshCore directamente.

### 5.1. Qué implica

* `HW:` `SPI MOSI/MISO/SCK/CS + DIO1/BUSY/RESET (+TCXO_EN)` en `JP1 2x13` vía `Dynamic PinMux`. Perfiles `sx1262-868, sx1262-915, sx1280-2.4G` en `Backpack Manager` (NFC ya prevé `mapa de pines + Lua App`).
* `SW:` portar `MeshCore core (Packet, routing flood/direct, crypto X25519/ChaCha, contact store, advert scheduler, Duty Cycle)` a `core/src/meshnative/` agnóstico + `RadioHAL SPI` en cada BSP (`hal_radio_sx1262_p4.cpp`, `hal_radio_sx1280_s3.cpp`).
* `Licencia:` MeshCore MIT — compatible con CBDos GPLv3 si se mantiene atribución y se publica el port.
* `Riesgos:` timing crítico LoRa en P4 con LVGL+audio, calibración por región, interferencia con WiFi/BT del C6, certificación RF, consumo TX mal gestionado si no hay Duty Cycle.

### 5.2. Puente de migración (sin reescribir UI)

```cpp
// Hoy:
MeshCoreClient ──> cbdos::serial ──> dongle USB
// Mañana (mismo MeshCoreClient, otro backend):
MeshCoreClient ──> IMeshTransport {
  sendFrame(bytes), onFrame(bytes)
}
// IMeshTransportSerial (USB/JP1 UART) | IMeshTransportNative (SPI SX1262/SX1280 local)
```

La UI `Contactos/Canales/Mapa` no cambia: solo cambia el transporte. `SET_TIME`, agenda y threads se reutilizan.

### 5.3. Criterio para activar nativo

1. Paridad USB completa (Fases 1-4) validada con Android.
2. `Backpack solo-chip` prototipo con `SX1262 + NTAG213` + medidas `RX/TX mA` mejores que `dongle USB` en ≥20%.
3. Pruebas de alcance `≥ dongle` en misma banda/antena y convivencia con `audio/LVGL` sin drops.

Hasta entonces: **no duplicar stack, seguir en USB**.

---

## 6. Matriz de verificación

| Prueba | Cómo | OK si |
|---|---|---|
| `queryContacts` | Conectar dongle con ≥3 contactos, pulsar Discover | Lista con nombres/prefix/hops correctos vs Android |
| `DM entregado` | Mandar DM a nodo vecino | `MSG_SENT tag` + `ACK` + `✔✔` en <30 s, aparece en Android |
| `DM flood` | Nodo a 2+ hops o sin path | Reintenta y entrega como flood, `hops>0` visible |
| `Advert` | `Flood advert` | Otro teléfono lo descubre en <2 min |
| `Canales` | Crear privado, hashtag `#test`, público | Android con mismo secret lo lee/escribe |
| `Persistencia` | Reiniciar CBDos sin dongle | Agenda + últimos 200 msgs visibles, `Desconectado` |
| `Energía` | Medir VBUS con dongle idle/TX | Idle <100 mA, TX pico documentado, suspend no cuelga drain |
| `Dual-target` | `idf.py build` + `pio run` | Compila P4+S3 sin warnings nuevos, tests host pasan |
| `Regla 8/12` | `grep -r driver/ core/src/meshcore`, perf UI | Cero includes IDF/Arduino, UI 30+ FPS sin polling |

---

## 7. Referencias

* `docs.meshcore.io/companion_protocol/` — framing `<`/`>`, `APP_START`, `CHANNEL`, `BATTERY`, `SELF_INFO`, `DEVICE_INFO`.
* `MeshCore Wiki Companion-Radio-Protocol` — `GET_CONTACTS 0x04, SEND_DM 0x02, ADVERT 0x07, CONTACT 0x03, ACK 0x82, RESET_PATH, SHARE/EXPORT/IMPORT`.
* `Terminal Chat CLI` — `advert, set name/lat/lon/freq/tx, card/import, list, to/send, reset path, public`.
* Código actual: `core/include/cbdos/meshcore/meshcore_types.hpp`, `meshcore_client.hpp`, `core/src/meshcore/meshcore_client.cpp`, `core/src/ui/views/MeshCoreView.hpp/.cpp`, `core/include/cbdos/serial.hpp`.
* Hardware: `specs/hardware/pinouts_and_ports.md`, `specs/architecture/backpack_manager_and_dynamic_gpio_nfc_spec.md`, `specs/architecture/multi_radio_hub_router_design.md`, `specs/network/especificacion_enlace_modem_usb_y_meshcore.md`.
