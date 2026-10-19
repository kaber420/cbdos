# Plan Técnico: Captura PCAP a MicroSD y Pendientes de Wi-Fi Shield

**Documento:** `specs/architecture/plan_pcap_sd_y_pendientes_wifi_shield.md`
**Estado:** Propuesta / Aprobado para ejecución por fases
**Fecha:** 18 de Octubre de 2026
**Autores:** Kaber420 (Lead Architect) & Antigravity (Senior Embedded Architecture)
**Objetivo:** Definir la arquitectura y el plan de ejecución para el volcado continuo de
tramas 802.11 a `.pcap` en la MicroSD (pestaña *Sniffer*) y cerrar el resto de capacidades
pendientes de **Wi-Fi Shield**, manteniendo `core/` agnóstico y verificando compilación
dual-target (ESP32-S3 y ESP32-P4).

---

## 1. Estado Actual (base ya implementada)

| Componente | Archivo | Estado |
| :--- | :--- | :---: |
| Tipos de datos Wi-Fi | `core/include/cbdos/wifi_types.hpp` | ✅ |
| Servicio Recon + IDS + contadores | `core/src/security/WifiShieldService.{hpp,cpp}` | ✅ |
| Vista 3 pestañas | `core/src/ui/views/WifiShieldView.{hpp,cpp}` | ✅ |
| HAL S3 (scan + promiscuo) | `bsp/esp32_s3_jc3248/hal/hal_wifi_s3.cpp` | ✅ |
| Volcado `.pcap` a SD | — | ❌ Pendiente |
| Canal hopping | — | ❌ Pendiente |
| Karma / baseline Rogue AP | — | ❌ Pendiente |
| Soporte P4 (ESP-Hosted/C6) | — | ❌ Pendiente |

**Limitación actual del firmware:** el callback promiscuo (`wifiPromiscuousCb`) resume la
trama a `radio::WifiRawFrame` (solo cabecera) y la descarta. No se conserva el buffer crudo,
por lo que hoy no hay material para PCAP.

---

## 2. Alcance

1. **Captura PCAP real** a `/sdcard/captures/wifi_YYYYMMDD_HHMMSS.pcap` con cabecera
   **Radiotap** (canal, RSSI, tasa) para análisis directo en Wireshark.
2. **Motor de captura desacoplado** del callback de radio (ring buffer + task escritora).
3. **API de streaming de almacenamiento** en la HAL para no cargar archivos en RAM.
4. **UI de Sniffer** con control de grabación, contadores, tamaño de archivo y rotación.
5. **Canal hopping** configurable para cobertura multi-canal.
6. **Pendientes IDS**: detección Karma, baseline de Rogue AP y métricas por canal.
7. **Soporte P4** (viabilidad + plan de driver sobre C6/ESP-Hosted).

---

## 3. Arquitectura de Captura

Flujo desacoplado en tres etapas para no bloquear el callback de radio ni el bus SD:

```
┌──────────────────────┐    push (lock-free)    ┌──────────────────────┐
│  Callback promiscuo   │ ─────────────────────▶ │  Ring Buffer PCAP    │
│  (contexto Wi-Fi/idf) │   WiFiTask / Core 0    │  (PSRAM, SPSC)       │
└──────────────────────┘                         └──────────┬───────────┘
                                                            │ pop
                                                 ┌──────────▼───────────┐
                                                 │  PcapWriterTask      │
                                                 │  buffer + flush 1 Hz │
                                                 │  → HAL storage stream│
                                                 └──────────┬───────────┘
                                                            │ fwrite
                                                 ┌──────────▼───────────┐
                                                 │  MicroSD (SDMMC/SPI) │
                                                 └──────────────────────┘
```

**Reglas de diseño:**

- **Nunca escribir a SD dentro del callback de radio** (el callback corre en el contexto de
  la pila Wi-Fi; un `fwrite` bloqueante provocaría pérdida de paquetes y watchdog).
- El callback **copia** el frame a un slot del ring buffer y hace `push` sin bloqueo.
  Si el buffer está lleno, se incrementa un contador `dropped` (se descarta la trama).
- La `PcapWriterTask` (Core 1, prioridad baja) consume el ring, serializa cabeceras y
  escribe en bloques; hace `fflush` ~1 vez/segundo o cada N KB.
- El ring buffer reside en **PSRAM** (S3: 8 MB; P4: 32 MB) con slots de tamaño fijo.

### 3.1 Ring Buffer SPSC

```cpp
// core/include/cbdos/wifi_capture_types.hpp (nuevo)
struct PcapFrameSlot {
    uint32_t tsSec;
    uint32_t tsUsec;
    uint16_t inclLen;     // longitud capturada
    uint16_t origLen;     // longitud original (rx_ctrl.sig_len)
    uint8_t  channel;     // canal de recepción
    int8_t   rssi;        // dBm
    uint8_t  rate;        // 0.5 Mbps units (si disponible)
    uint8_t  data[256];   // snapLen por defecto (configurable 128/256/512)
};
```

- Capacidad sugerida: **256 slots** (≈ 64 KB + overhead con snapLen 256).
- Índices `head`/`tail` `std::atomic<uint32_t>` (productor único = callback,
  consumidor único = writer task).
- Sin mutex en la ruta caliente; `memory_order_acquire/release`.

---

## 4. Formato PCAP + Radiotap

### 4.1 Cabecera Global PCAP (24 bytes, little-endian, endianness nativa ESP32)

| Offset | Tamaño | Campo | Valor |
| :---: | :---: | :--- | :--- |
| 0 | 4 | magic_number | `0xA1B2C3D4` |
| 4 | 2 | version_major | `2` |
| 6 | 2 | version_minor | `4` |
| 8 | 4 | thiszone | `0` |
| 12 | 4 | sigfigs | `0` |
| 16 | 4 | snaplen | `snapLen` (128/256/512) |
| 20 | 4 | network | `127` = `LINKTYPE_IEEE802_11_RADIOTAP` |

### 4.2 Cabecera de Paquete (16 bytes)

| Offset | Tamaño | Campo |
| :---: | :---: | :--- |
| 0 | 4 | ts_sec (epoch UTC) |
| 4 | 4 | ts_usec |
| 8 | 4 | incl_len (`it_len + frame_len`) |
| 12 | 4 | orig_len (`it_len + rx_ctrl.sig_len`) |

> Timestamps deben ser **hora de pared** (epoch) para que Wireshark ordene bien.
> Fuente: `gettimeofday()` + `esp_timer` para precisión de microsegundos.

### 4.3 Cabecera Radiotap (mínima pero útil)

`it_present` = `RATE | CHANNEL | DB_ANTSIGNAL` = `(1<<2)|(1<<3)|(1<<5)` = `0x2C`.

| Campo | Bits | Contenido |
| :--- | :---: | :--- |
| it_version | 1 | `0` |
| it_pad | 1 | `0` |
| it_len | 2 | `14` (little-endian) |
| it_present | 4 | `0x0000002C` |
| Rate | 1 | tasa en unidades de 500 kbps |
| Channel freq | 2 | MHz según canal (tabla 2.4/5 GHz) |
| Channel flags | 1 | `0x00A0` (2 GHz) / `0x0100` (5 GHz) |
| Antenna signal | 1 | RSSI (dBm, `int8_t`) |

`it_len = 8 + 1 + 4 + 1 = 14` (todos los campos de 1 byte están naturalmente alineados).

**Tabla canal→frecuencia** necesaria (función `wifiChannelToFreqMhz(uint8_t ch)`):
2.4 GHz: `ch1 = 2412`, paso 5 MHz; `ch14 = 2484`.
5 GHz: tabla fija (36=5180, 40=5200, 44=5220, 48=5240, 52=5260, 56=5280, 60=5300,
64=5320, 100=5500, …, 149=5745, 153=5765, 157=5785, 161=5805, 165=5825).

---

## 5. API de Streaming de Almacenamiento

Hoy `cbdos::storage::IStorageBackend` solo expone `writeFile(path, content)` que carga el
archivo completo en RAM (`core/include/cbdos/storage.hpp:49`) — **inservible para captura
continua**. Se añade una API de flujo con implementaciones por defecto nulas (compatibilidad
total con backends existentes):

```cpp
// Añadir a core/include/cbdos/storage.hpp
class IStorageBackend {
    // ... existentes ...
    virtual void* openWriteStream(const char* path, bool append = false) { (void)path; (void)append; return nullptr; }
    virtual size_t writeStream(void* handle, const uint8_t* data, size_t len) { (void)handle; (void)data; (void)len; return 0; }
    virtual bool   flushStream(void* handle) { (void)handle; return false; }
    virtual bool   closeStream(void* handle) { (void)handle; return false; }
    virtual uint64_t sizeStream(const char* path) { (void)path; return 0; }
};

// Wrappers públicos en el namespace cbdos::storage
void*  openWriteStream(const char* path, bool append = false);
size_t writeStream(void* handle, const uint8_t* data, size_t len);
bool   flushStream(void* handle);
bool   closeStream(void* handle);
uint64_t sizeStream(const char* path);
```

**Implementación S3** (`bsp/esp32_s3_jc3248/hal/hal_storage_s3.cpp`):
usar `FILE*`/`fopen("wb"/"ab")` + `fwrite`/`fflush`/`fclose` (mismo patrón que
`core/src/audio/WavRecorder.cpp:100`). Resolver la ruta con el mismo `cleanPath` ya usado.

**Implementación P4** (`bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp`): `fopen` sobre VFS
FAT de la SDMMC. Verificar que el sistema de archivos esté montado antes de abrir.

> Alternativa pragmática: dado que `WavRecorder.cpp` ya usa `FILE*` en `core/`, se podría
> usar `fopen` directo desde el nuevo `PcapWriterService`. Se recomienda igualmente la API
> HAL para mantener el desacoplamiento y soportar futuros backends (USB, etc.).

---

## 6. Motor de Captura (Core)

Nuevo componente agnóstico `core/src/security/WifiCaptureService.{hpp,cpp}`
(o extender `WifiShieldService`):

```cpp
struct WifiCaptureConfig {
    bool      enabled = false;
    uint16_t  snapLen = 256;        // bytes por trama (128/256/512)
    bool      radiotap = true;
    uint16_t  rotateSizeKb = 0;     // 0 = sin rotación
    uint32_t  flushIntervalMs = 1000;
    uint8_t   channelHopDwellMs = 0; // 0 = sin hopping
    const char* dir = "/sdcard/captures";
};

class WifiCaptureService {
public:
    static WifiCaptureService& getInstance();
    bool start(const WifiCaptureConfig& cfg);
    void stop();
    bool isActive() const;

    // Llamado por el HAL desde el callback promiscuo (copia al ring)
    void pushRawFrame(const uint8_t* frame, uint16_t frameLen, uint16_t origLen,
                      uint8_t channel, int8_t rssi, uint8_t rate);

    // Métricas para la UI
    uint64_t getBytesWritten() const;
    uint32_t getPacketsCaptured() const;
    uint32_t getPacketsDropped() const;
    std::string getCurrentFile() const;

    // El writer task necesita la implementación HAL de escritura
    using StreamOpen  = std::function<void*(const char*, bool)>;
    using StreamWrite = std::function<size_t(void*, const uint8_t*, size_t)>;
    using StreamFlush = std::function<bool(void*)>;
    using StreamClose = std::function<bool(void*)>;
    void registerStorageHooks(StreamOpen, StreamWrite, StreamFlush, StreamClose);
};
```

**Responsabilidades:**
- `pushRawFrame`: copia a `PcapFrameSlot` y encola (sin bloqueo).
- Task `pcap_writer` (Core 1): abre archivo, escribe cabecera global, consume slots,
  escribe `pcaprec_hdr + radiotap + frame`, `fflush` periódico, cierra al parar.
- Rotación: al superar `rotateSizeKb`, cerrar y abrir `wifi_<ts>_partN.pcap`.
- Nombre de archivo con calendario local desde `cbdos::time`.

**Cambio en el HAL S3** (`hal_wifi_s3.cpp`): en `wifiPromiscuousCb`, además de
`processRawFrame(...)`, llamar a `WifiCaptureService::pushRawFrame(p, sig_len, sig_len, ch, rssi, rate)`
cuando la captura esté activa. Paso del buffer crudo `pkt->payload` + `pkt->rx_ctrl.sig_len`.

---

## 7. UI — Pestaña Sniffer

Ampliar `WifiShieldView::setupSnifferTab` / `updateSnifferDisplay`:

- **Botón Grabar/Detener** (rojo) que llama `WifiCaptureService::start/stop`.
- **Selectores** (dropdown LVGL 9.5):
  - SnapLen: `128 / 256 / 512`.
  - Rotación: `Off / 8 / 32 / 128 MB`.
  - Canal: `Todos (hop) / fijo`.
- **Tarjetas de métricas**: `Tramas capturadas`, `KB escritos`, `Descartadas`, `Tiempo`.
- **Etiqueta de archivo actual** (`/sdcard/captures/wifi_....pcap`) + indicador "● REC".
- **Hint** cuando no hay SD montada (`cbdos::storage::isSdMounted()`).

Reutilizar el patrón de `lv_timer` a 250 ms ya presente en la vista
(`WifiShieldView.cpp`), y el estilo de tarjetas de las pestañas IDS/Recon.

---

## 8. Canal Hopping

- 2.4 GHz: canales 1–13 (14 opcional), 5 GHz: subconjunto seleccionable.
- Implementación por **software timer** en el HAL: `esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE)`
  cada `channelHopDwellMs` (p.ej. 300–500 ms) mientras la captura está activa.
- El campo `channel` de Radiotap refleja el canal real en el momento de captura
  (`pkt->rx_ctrl.channel`), no el del hop, garantizando coherencia.
- Nota: al fijar canal (modo Recon o conexión STA), deshabilitar hopping
  automáticamente.

---

## 9. Coexistencia con la Radio / STA

| Escenario | Comportamiento |
| :--- | :--- |
| Recon activo | `WiFi.scanNetworks` en task; promiscuo OFF. |
| Sniffer/IDS activo | Promiscuo ON; se pausa el scan (ya implementado). |
| Wi-Fi STA conectado | Promiscuo puede degradar/romper la conexión; advertir en UI y no iniciar captura automáticamente. |
| Cambio de modo | Al alternar, detener captura y scan limpiamente antes de reconfigurar. |

Añadir `WifiShieldService` un flag de estado de radio para la UI (p.ej.
`RadioActivity { Idle, Scanning, Capturing }`).

---

## 10. Pendientes del IDS

1. **Karma** (`WifiAlertType::Karma`): detectar respuestas (Probe Response / Assoc) a
   sondas que no fueron emitidas por un cliente conocido; requiere correlacionar
   `Probe Request` (subtype `0x04`) y `Probe Response` (`0x05`).
2. **Baseline Rogue AP**: además del "mismo SSID en varios BSSID" ya implementado,
   permitir una **lista blanca de BSSID** por SSID (persistible en NVS) y marcar
   Rogue solo si el BSSID no está en el baseline.
3. **Métricas por canal**: contador de tramas por canal para el waterfall del Sniffer.
4. **Alertas sonoras** (`audioAlert`) vía `AudioHAL`/ES8311 (S3 no tiene DAC dedicado;
   evaluar tono por I2S/PDM).
5. **Persistencia de configuración** (`WifiIdsConfig`, `WifiCaptureConfig`) en NVS,
   reutilizando el patrón de `BleSpamIdsConfig` (aún no persistido tampoco).

---

## 11. Soporte P4 (ESP-Hosted / C6)

- El P4 **no tiene radio Wi-Fi nativa**; usa el coprocesador ESP32-C6 vía `esp_hosted`
  (`bsp/esp32_p4_jc4880/managed_components/espressif__esp_hosted`).
- **Escaneo**: soportado por la API remota (`esp_wifi_scan_*`).
- **Modo promiscuo**: **no soportado por ESP-Hosted** (la captura cruda no se reenvía al host).
  → En P4, la pestaña Sniffer debe indicar "No soportado (requiere radio nativa)" y el IDS
    pasivo quedará limitado a lo derivable del escaneo (Rogue AP/Evil Twin por SSID/BSSID).
- Plan: crear `hal_wifi_p4.cpp` que registre `ScanControl` (recon) y deje
  `SnifferControl` como no-op con log informativo.

---

## 12. Rendimiento y Memoria (presupuestos)

| Recurso | Objetivo |
| :--- | :--- |
| Ring buffer | ≤ 96 KB PSRAM (256 slots × (slot hdr + snapLen 256)) |
| Copia en callback | < 50 µs por trama (memcpy ≤ 256 B) |
| Escritura SD | bloques de 4–16 KB, `fflush` cada ≤ 1 s |
| Pérdida aceptable | < 1 % en tráfico denso (medido por `packetsDropped`) |
| Impacto UI | sin bloqueos; métricas por `lv_timer` a 250 ms |

---

## 13. Plan de Ejecución por Fases

### Fase A — Streaming de almacenamiento
- [ ] A1. Añadir API `open/write/flush/closeStream` a `IStorageBackend` + wrappers.
- [ ] A2. Implementar en `hal_storage_s3.cpp` (FILE*) y `hal_storage_p4.cpp` (VFS).
- [ ] A3. Verificar dual-target.

### Fase B — Captura PCAP
- [ ] B1. Crear `wifi_capture_types.hpp` (slot, config) y `WifiCaptureService`.
- [ ] B2. Implementar ring buffer SPSC + task writer + cabeceras PCAP/Radiotap.
- [ ] B3. Tabla `wifiChannelToFreqMhz` + timestamps epoch.
- [ ] B4. Enlazar hook en `wifiPromiscuousCb` (HAL S3) con el buffer crudo.
- [ ] B5. Registrar fuentes en `core/CMakeLists.txt`; verificar dual-target.

### Fase C — UI Sniffer
- [ ] C1. Botón Grabar/Detener + indicador REC.
- [ ] C2. Dropdowns SnapLen/Rotación/Canal.
- [ ] C3. Tarjetas de métricas + archivo actual + aviso sin SD.
- [ ] C4. Verificación en hardware (JC3248).

### Fase D — Canal hopping
- [ ] D1. Hopping por software en HAL (`esp_wifi_set_channel`).
- [ ] D2. Integración con modo de radio y UI.

### Fase E — IDS avanzado
- [ ] E1. Karma (Probe Req/Resp).
- [ ] E2. Baseline Rogue AP + NVS whitelist.
- [ ] E3. Métricas por canal.
- [ ] E4. Persistencia NVS de `WifiIdsConfig`/`WifiCaptureConfig`.

### Fase F — P4
- [ ] F1. `hal_wifi_p4.cpp` (recon sí, promiscuo no).
- [ ] F2. Estado "no soportado" en UI para Sniffer en P4.

---

## 14. Riesgos

| Riesgo | Mitigación |
| :--- | :--- |
| Escritura SD lenta → pérdida de paquetes | Ring buffer + escritura por bloques + contador `dropped` visible |
| Promiscuo rompe STA | No iniciar captura con STA activo; advertencia en UI |
| Wear de la SD | Rotación configurable + no captura por defecto |
| Timestamps no-epoch en Wireshark | Usar `gettimeofday` (RTC/NTP o epoch mesh) |
| ESP-Hosted sin promiscuo (P4) | Diagnóstico claro en UI; recon degradado |
| Corrupción del `.pcap` al cortar corriente | `fflush` periódico + cabecera global al inicio |

---

## 15. Criterios de Aceptación

1. El `.pcap` generado **abre en Wireshark** con canal y RSSI correctos (Radiotap).
2. Capturando 60 s con tráfico denso: `dropped < 1 %` y sin reinicios/watchdog.
3. Recon, IDS y Sniffer funcionan de forma independiente y conmutan sin cuelgues.
4. Rotación de archivos correcta y cierre limpio al detener o al expulsar la SD.
5. Compilación **dual-target** (`esp32s3_red` y P4) sin errores.
6. En P4 la UI informa correctamente la no disponibilidad de captura cruda.

---

## 16. Trazabilidad

- Plan marco: `specs/architecture/plan_perfil_red_wifi_ids_y_ble_hunter.md`
- Roadmap de portado: `specs/drafts/ROADMAP_PORTADO_HERRAMIENTAS_CYBERDECK.md`
- Código base: `core/src/security/WifiShieldService.{hpp,cpp}`,
  `bsp/esp32_s3_jc3248/hal/hal_wifi_s3.cpp`, `core/src/ui/views/WifiShieldView.{hpp,cpp}`
- Precedente de escritura a SD: `core/src/audio/WavRecorder.cpp`
