# 🗺️ Hoja de Ruta Maestra: Roadmap Global de Refactorización y Desacoplamiento de CBDos

**Documento:** `docs/architecture/roadmap_global_refactorizacion_y_desacoplamiento_cbdos.md`  
**Versión:** 1.0.0  
**Estado:** Documento Oficial de Seguimiento y Planificación Arquitectónica  
**Autor:** Equipo de Arquitectura de Software CBDos  
**Fecha:** Agosto 2026  

---

## 📌 1. Visión y Estado Global del Sistema

El objetivo central de **CBDos** es consolidarse como un **Sistema Operativo Embebido Multi-Target** de alto rendimiento, donde el 100% de la lógica de aplicaciones, interfaz gráfica (LVGL 9.5), decodificadores multimedia y motor de scripting Lua residen en un núcleo (`core/`) completamente desacoplado de las plataformas subyacentes (**ESP-IDF**, **Arduino**, **FreeRTOS** o **Simulador Nativo Linux/PC**).

### 📊 Diagrama de Madurez y Estado de las 6 Fases

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 🟢 FASE 1: Infraestructura Base & Conectividad                [100% DONE]   │
│ • IPersistenceBackend (NVS Flash P4 / Preferences S3)                       │
│ • IRadioBackend & INetworkAdapter (ESP-Hosted SDIO / Native WiFi)           │
│ • IMeshTransport (Protocolo de Malla P4 / S3)                              │
│ • IHttpClient, IHidDriver, ITimeProvider (NTP)                              │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 🟢 FASE 2: Periféricos Locales & Multimedia                   [100% DONE]   │
│ • IStorageBackend (MicroSD SDMMC 4-bit P4 / SPI S3 / SPIFFS) ──► [✅ 100%]   │
│ • IAudioSink & IAudioSource (I2S DMA / ES8311 / S3 I2S)      ──► [✅ 100%]   │
│ • IUartBackend (Terminal Serie multi-puerto / JP1 / MX 1.25) ──► [✅ 100%]   │
│ • IGpioBackend (Control digital de pines para mochilas / Lua)──► [✅ 100%]   │
│ • Purga definitiva de 'weak symbols' en cbdos_core.cpp       ──► [✅ 100%]   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ 🌐 FASE 3: Capa de Red, Sockets y Streaming Transparente      [COMPLETADO]  │
│ • Erradicar <sys/socket.h>, <netdb.h>, <arpa/inet.h> de core/──► [✅ 100%]   │
│ • Crear e inyectar contrato abstracto ISocketStream (TCP/UDP) ──► [✅ 100%]   │
│ • Migrar TlvBrowserView y RadioManager a ISocketStream       ──► [✅ 100%]   │
│ • Extraer el streaming HTTP de AudioPlayer hacia ISocketStream ─► [✅ 100%]   │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ ⏳ FASE 4: Particiones Flash, Actualizador OTA y Vistas       [PLAN LISTO]  │
│ • Desacoplar CartridgeManager (IFlashPartitionManager & IOtaUpdater)        │
│ • Eliminar llamadas directas a <esp_ota_ops.h> y <esp_partition.h>          │
│ • Limpieza de WallpaperManager y auditoría de las 27 vistas en UI           │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ ⏳ FASE 5: Formalización del App Framework y SDK Público      [PENDIENTE]   │
│ • Publicar catálogo formal de Syscalls de CBDos (cbdos::sys, cbdos::net)   │
│ • Aislar completamente el entorno de ejecución de Lua Apps (.luapp)        │
│ • Estandarizar el ciclo de vida de aplicaciones de terceros                 │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ ⏳ FASE 6: Simulador PC Linux y CI Automatizado Anti-Regresión[PENDIENTE]   │
│ • Target de compilación en Linux nativo (SDL2 / LVGL en PC)                 │
│ • Script scripts/verify_architecture.sh para validar cero includes ilegales│
│ • Pipeline de pruebas y validación continua multi-target                    │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ ⏳ FASE 7: Gaming, Fantasy Consoles & Cartridge Engine        [PROPUESTA]   │
│ • Integración de Runtimes LIKO-12 (.lk12) y TIC-80 (.tic)                   │
│ • CartridgeView con Gamepad Virtual táctil de baja latencia y audio I2S     │
│ • Soporte para Gamepads externos USB HID y Bluetooth                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 📋 2. Matriz Detallada de Componentes por Fases

| Fase | Componente / Subsistema | Contrato HAL | Target ESP32-P4 (ESP-IDF) | Target ESP32-S3 (Arduino) | Target PC (Simulador) | Estado Actual |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **F1** | Persistencia Clave-Valor | `IPersistenceBackend` | `nvs_flash` nativo | `Preferences.h` | Archivo JSON local | ✅ **Completado** |
| **F1** | Radio & Wi-Fi | `IRadioBackend` | ESP-Hosted SDIO (C6) | Wi-Fi Nativo SoC | Socket TAP virtual | ✅ **Completado** |
| **F1** | Control de Red | `INetworkAdapter` | `esp_netif` LwIP | `WiFiClass` Arduino | NetworkManager POSIX | ✅ **Completado** |
| **F1** | Red de Malla Militar | `IMeshTransport` | TLV Packet Driver | TLV Packet Driver | Loopback UDP | ✅ **Completado** |
| **F1** | Cliente HTTP | `IHttpClient` | `esp_http_client` | `HTTPClient` Arduino | `libcurl` / POSIX | ✅ **Completado** |
| **F1** | Emulación USB HID | `IHidDriver` | TinyUSB CDC/HID | USB HAL S3 | Teclado X11 / Virtual | ✅ **Completado** |
| **F1** | Sincronización NTP | `ITimeProvider` | `esp_sntp` | `time.h` Arduino | `clock_gettime` | ✅ **Completado** |
| **F2** | **Almacenamiento (SD/Flash)** | `IStorageBackend` | **SDMMC 4-bit + SPIFFS** | **SD SPI + LittleFS** | **Carpeta `./fs_root`** | ✅ **Completado** |
| **F2** | **Salida de Audio (I2S)** | `IAudioSink` | **ES8311 I2S DMA** | **JC3248 I2S DMA** | **ALSA / PulseAudio** | ✅ **Completado** |
| **F2** | **Micrófono (Entrada I2S)** | `IAudioSource` | **ES8311 ADC DMA** | **No-op / I2S Mic** | **Microphone Pulse** | ✅ **Completado** |
| **F2** | **Terminal Serie / JP1** | `IUartBackend` | **`driver/uart` UART1** | **`HardwareSerial`** | **PTY Terminal POSIX** | ✅ **Completado** |
| **F2** | **Control GPIO Digital** | `IGpioBackend` | **`driver/gpio` nativo** | **Arduino GPIO Core** | **Virtual GPIO Pin Map** | ✅ **Completado** |
| **F3** | **Sockets TCP/UDP Stream** | `ISocketStream` | **LwIP Sockets BSD** | **WiFiClient S3** | **POSIX Sockets** | ✅ **Completado** |
| **F4** | **Particiones y Cartuchos** | `IFlashPartitionManager`| **`esp_partition.h`** | **Particiones Custom** | **Memoria RAM Mapeada**| ⏳ *Pendiente F4* |
| **F4** | **Flasheador OTA** | `IOtaUpdater` | **`esp_ota_ops.h`** | **Update.h Arduino** | **Escritura a Imagen** | ⏳ *Pendiente F4* |
| **F5** | **Syscalls SDK / App Life** | `SystemApi` | **Llamadas Directas** | **Llamadas Directas** | **Sandbox / Proceso** | ⏳ *Pendiente F5* |
| **F6** | **CI y Simulador Linux** | CMake Desktop Tool | **ESP-IDF 5.5** | **pioarduino** | **GCC Linux x86_64** | ⏳ *Pendiente F6* |
| **F7** | **Fantasy Consoles & Gaming** | `ICartridgeRuntime` | **LIKO-12 / TIC-80 / Core**| **LIKO-12 Lua Lite** | **Render SDL2 / Native** | 📝 *Propuesta* |

---

## 🛠️ 3. Plan de Trabajo y Cronograma de Ejecución

### 🔹 FASE 2 (En Curso): Multimedia, UART y Purga de Weak Symbols
* **Documento Técnico:** [`docs/architecture/especificacion_tecnica_fase_dos_audio_sink_uart_y_purga.md`](file:///home/kaber420/Documentos/proyectos/cbdos/docs/architecture/especificacion_tecnica_fase_dos_audio_sink_uart_y_purga.md)
* **Objetivos Inmediatos:**
  1. Implementar `IAudioSink` e `IAudioSource` en `core/include/cbdos/audio.hpp`.
  2. Conectar `AudioPlayer.cpp`, `WavPlayer.cpp` y `WavRecorder.cpp` para emitir/capturar muestras PCM mediante las interfaces.
  3. Implementar `P4AudioSink`/`P4AudioSource` (ES8311) en `bsp/esp32_p4_jc4880` y `S3AudioSink` en `bsp/esp32_s3_jc3248`.
  4. Implementar `IUartBackend` para la terminal interactiva y el puerto de expansión JP1.
  5. Purgar todos los símbolos `__attribute__((weak))` restantes en `cbdos_core.cpp`.

---

### 🔹 FASE 3: Sockets BSD y Streaming de Red Transparente
* **Documento Técnico:** [`docs/architecture/phase3_sockets_and_streaming_subphases_plan.md`](file:///home/kaber420/Documentos/proyectos/cbdos/docs/architecture/phase3_sockets_and_streaming_subphases_plan.md)
* **Objetivos:**
  1. Crear el contrato `ISocketStream` (`connect()`, `read()`, `write()`, `available()`, `close()`).
  2. Implementar `P4SocketStream` (sobre LwIP de ESP-IDF) y `S3SocketStream` (sobre `WiFiClient` de Arduino).
  3. Refactorizar [`TlvBrowserView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TlvBrowserView.cpp) y [`RadioManager.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/RadioManager.cpp) para eliminar `<sys/socket.h>`, `<netdb.h>` y `<arpa/inet.h>`.
  4. Extraer el bucle de streaming HTTP de `AudioPlayer.cpp` para recibir paquetes a través de `ISocketStream`.

---

### 🔹 FASE 4: Particiones Flash, Actualizador OTA y Limpieza de Vistas
* **Objetivos:**
  1. Crear interfaces `IFlashPartitionManager` e `IOtaUpdater`.
  2. Refactorizar [`CartridgeManager.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/cartridge/CartridgeManager.cpp) eliminando `#include <esp_ota_ops.h>` y `#include <esp_partition.h>`.
  3. Refactorizar [`WallpaperManager.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/WallpaperManager.cpp) para usar `cbdos::mem` y `cbdos::storage`.
  4. Auditoría estricta de las 27 vistas en `core/src/ui/views/` (garantizar cero dependencias de hardware o frameworks).

---

### 🔹 FASE 5: Formalización del App Framework y SDK Público
* **Objetivos:**
  1. Consolidar el SDK público en `core/include/cbdos/system_api.hpp`.
  2. Estandarizar la API de aplicaciones Lua (`.luapp`) y el ciclo de vida `onCreate`, `onUpdate`, `onDestroy`.
  3. Publicar la documentación oficial para desarrolladores en `docs/api/how_to_create_an_app.md`.

---

### 🔹 FASE 6: Simulador Nativo en PC y CI Multi-Target
* **Objetivos:**
  1. Crear un target CMake para Linux/PC usando SDL2 para renderizar la pantalla y capturar eventos de mouse/teclado.
  2. Crear el script `./scripts/verify_architecture.sh` para verificar automáticamente en el repositorio que no existan inclusiones ilegales en `core/`.
  3. Asegurar compilación simultánea 100% limpia en:
     - ESP-IDF 5.5 (`idf.py build`)
     - PlatformIO (`pio run`)
     - Simulador Linux (`cmake --build build_pc`)

---

## 🧪 4. Criterios de Aprobación y Validación Continua

Para dar por concluida cada una de las fases, se deben cumplir tres reglas inviolables:

1. **Ley de Pureza de `core/`:**
   ```bash
   # Debe retornar 0 coincidencias en core/
   grep -rnE "#include <(Arduino\.h|WiFiClient\.h|Preferences\.h|esp_log\.h|esp_heap_caps\.h|esp_http_client\.h|esp_system\.h|esp_ota_ops\.h|esp_partition\.h|driver/|FreeRTOS\.h|freertos/|sys/socket\.h|netdb\.h)>" core/
   ```

2. **Compilación Multi-Target Limpia (Zero Warnings / Zero Errors):**
   - ESP32-P4: `. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build`
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`

3. **Validación Funcional en Hardware Real:**
   - La funcionalidad refactorizada debe probarse y responder con fluidez idéntica o superior al código original de `espOS32`.

---

## 🌐 5. Roadmap de Ecosistema y Red Social Descentralizada (CBD-Net)

| Módulo / Característica | Descripción y Enfoque Técnico | Especificación Asociada | Estado |
| :--- | :--- | :--- | :--- |
| **Motor Semántico de Conceptos** | Tokenización de vocabulario denso (1/2/3 bytes con 32 bancos temáticos) para reducir textos a 8-15 bytes y eliminar errores de traducción regional. | [`docs/proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md`](../proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md) | 💡 Propuesta Formal |
| **Foros Técnicos Descentralizados** | Tableros temáticos estilo Clan GSM / Reddit (`#hardware`, `#esquemas`, `#reparacion`) con suscripción selectiva a hilos y autores. | [`docs/proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md`](../proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md) | 💡 Propuesta Formal |
| **Sincronización por Delta (Cherry-Picking)** | Protocolo de sincronización incremental (`SINCE_ID` / Cursor) para descargar exclusivamente respuestas o posts no presentes en la caché MicroSD. | [`docs/proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md`](../proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md) | 💡 Propuesta Formal |
| **Winks & Reacciones Vectoriales** | Animaciones de pantalla completa ThorVG/Lottie a 60 FPS con sonido local ES8311, activadas mediante tokens de radio de 1 o 2 bytes. | [`docs/proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md`](../proposals/proposal_semantic_concept_tokenization_and_airtime_optimization.md) | 💡 Propuesta Formal |
| **Transporte Híbrido Multicapa** | Descarga rápida de assets pesados vía WiFi / FLRC 2.4 GHz y comunicación de campo de ultra bajo consumo vía LoRa / ESP-NOW. | [`docs/proposals/proposal_backpack_nfc_hotplug_hardware_modules.md`](../proposals/proposal_backpack_nfc_hotplug_hardware_modules.md) | 💡 Propuesta Formal |

