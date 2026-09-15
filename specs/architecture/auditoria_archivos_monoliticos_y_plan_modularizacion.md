# 📋 Auditoría Técnica de Archivos Monolíticos y Plan de Modularización (CBDos v0.2.1)

> **Fecha de Elaboración:** 2026-09-14  
> **Alcance:** Código fuente propio en `core/` y `bsp/` (excluyendo bibliotecas externas vendored y código autogenerado).  
> **Objetivo:** Identificar con precisión quirúrgica qué archivos exceden límites sanos de mantenibilidad (> 500 - 3,000 líneas), justificar por qué alcanzaron ese tamaño, qué responsabilidades mezclan y proponer su partición modular.

---

## 1. Criterios de Exclusión (Falsos Positivos)

Para evitar desinformación o métricas infladas, los siguientes directorios y archivos **NO** se consideran candidatos a división debido a que son estándares de la industria, motores externos o código autogenerado que debe preservarse intacto:

| Categoría | Rutas / Componentes | Líneas Aprox. | Razón de Exclusión |
| :--- | :--- | :---: | :--- |
| **Código Autogenerado** | `bsp/esp32_c6_slave/main/common/proto/esp_hosted_rpc.pb-c.*` | ~46,150 | Archivos C/H generados por el compilador Protobuf. No se tocan manualmente. |
| **Motor TTS** | `core/src/tts/picotts/lib/*` | ~25,000 | Implementación oficial de SVOX PicoTTS C puro. |
| **Motor Lua** | `core/src/lua/liblua/*` | ~15,000 | Intérprete estándar Lua 5.4. No debe bifurcarse ni alterarse internamente. |
| **Emulador Doom** | `bsp/esp32_s3_jc3248/lib/doomgeneric/*` | ~20,000 | Código de DoomGeneric adaptado para framebuffer embebido. |
| **Emulador GameBoy** | `bsp/esp32_s3_jc3248/lib/peanut_gb/*` | ~14,000 | Emulador Peanut-GB / Walnut CGB de un solo archivo de cabecera. |
| **Decodificador Helix** | `core/src/audio/libhelix/*` | ~10,000 | Biblioteca en punto fijo de RealNetworks para MP3/AAC. |
| **Cripto & CBOR** | `core/src/security/kerberos/tinycbor/*`, `minimp3.h` | ~3,500 | Bibliotecas de terceros para CBOR y decodificación MP3 mínima. |
| **Assets de Datos** | `core/assets/mascot_robot_assets.h` | 604 | Matrices de bytes y datos crudos de vectores/animación. |

---

## 2. Inventario Exhaustivo de Archivos Propios (≥ 500 Líneas)

A continuación se muestra el censo completo y verificado mediante conteo directo de líneas (`wc -l`):

| # | Líneas | Ruta del Archivo | Subsistema | Tipo / Rol |
| :-: | :-: | :--- | :--- | :--- |
| 1 | **2,918** | [`core/src/ui/views/MeshCoreView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/MeshCoreView.cpp) | UI / MeshCore | Vista LVGL monolítica de mensajería Mesh |
| 2 | **2,348** | [`core/src/lua/LuaBridge.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/LuaBridge.cpp) | Core / Lua | Registro central de bindings del sistema para Lua |
| 3 | **1,432** | [`core/src/meshcore/meshcore_client.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/meshcore/meshcore_client.cpp) | Core / Red Malla | Cliente de protocolo MeshCore (traspaso, frames, cripto) |
| 4 | **1,284** | [`core/src/ui/views/RadioView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RadioView.cpp) | UI / Multimedia | App "Radio Online" (Streaming Web por Wi-Fi) |
| 5 | **904** | [`core/src/network/LanScannerService.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/network/LanScannerService.cpp) | Core / Red | Servicio de escaneo LAN (ARP, TCP, mDNS, OUI) |
| 6 | **836** | [`core/src/ui/views/FileManagerView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/FileManagerView.cpp) | UI / Sistema | Explorador de archivos VFS/SD/LittleFS |
| 7 | **827** | [`core/src/ui/views/TextEditorView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TextEditorView.cpp) | UI / Apps | Editor de texto con teclado en pantalla LVGL 9 |
| 8 | **821** | [`core/src/ui/components/AnimatedWallpaper.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/components/AnimatedWallpaper.cpp) | UI / Gráficos | Fondos animados (Matrix, Starfield, Canvas LVGL) |
| 9 | **811** | [`bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp) | BSP / P4 | Driver UART P4 + Consola CLI interactiva serial |
| 10 | **803** | [`core/src/ui/views/FlasherView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/FlasherView.cpp) | UI / Flasher | Interfaz de flasheo de particiones y binarios |
| 11 | **786** | [`core/src/ui/views/LuaRunnerView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/LuaRunnerView.cpp) | UI / Lua | Consola y ejecutor de scripts `.lua` |
| 12 | **774** | [`bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_storage_p4.cpp) | BSP / P4 | Inicialización SDMMC 4-bit, LDO VO4, VFS y LittleFS |
| 13 | **741** | [`core/src/audio/AudioPlayer.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/audio/AudioPlayer.cpp) | Core / Audio | Pipeline de audio Helix MP3/AAC, buffer circular PSRAM |
| 14 | **716** | [`bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_lan_recon_p4.cpp) | BSP / P4 | Sockets crudos y paquetes de red para escaneo en P4 |
| 15 | **691** | [`bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_lan_recon_s3.cpp) | BSP / S3 | Equivalente de reconocimiento de red para ESP32-S3 |
| 16 | **684** | [`core/src/ui/views/LanReconView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/LanReconView.cpp) | UI / Red | Visualización de topología y hosts de red descubiertos |
| 17 | **656** | [`bsp/esp32_s3_jc3248/src/GBCLauncher.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/src/GBCLauncher.cpp) | BSP / S3 | Lanzador de GameBoy Color (DMA, Framebuffer, I2S) |
| 18 | **612** | [`bsp/esp32_s3_jc3248/src/LuaLauncher.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/src/LuaLauncher.cpp) | BSP / S3 | Lanzador y setup de entorno de apps Lua en S3 |
| 19 | **603** | [`core/src/ui/views/NetworkManagerView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/NetworkManagerView.cpp) | UI / Red | Gestor de conexiones Wi-Fi y escaneo de APs |
| 20 | **602** | [`core/src/ui/views/RadioConfigView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RadioConfigView.cpp) | UI / Radio RF | Panel de configuración de radiofrecuencia/potencia/canales |
| 21 | **563** | [`core/src/ui/views/utilities/PomodoroApp.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/utilities/PomodoroApp.cpp) | UI / Utilidad | Temporizador Pomodoro con integración de audio/Lottie |
| 22 | **558** | [`core/src/ui/modals/SshConnectModal.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/modals/SshConnectModal.cpp) | UI / Modales | Modal de conexión SSH y teclado de credenciales |
| 23 | **555** | [`core/src/security/kerberos/kerberos_core/ctap2.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/security/kerberos/kerberos_core/ctap2.cpp) | Core / Cripto | Parser y despachador de comandos FIDO2/CTAP2 |
| 24 | **531** | [`core/src/ui/views/TlvBrowserView.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TlvBrowserView.cpp) | UI / Diagnóstico | Visor de paquetes en formato TLV |
| 25 | **507** | [`core/src/tts/PicoTTSService.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/tts/PicoTTSService.cpp) | Core / TTS | Puente entre sintetizador de voz y buffers I2S |

---

## 3. Diagnóstico en Profundidad de los 4 Archivos Críticos (> 1,200 Líneas)

### 1. `MeshCoreView.cpp` (2,918 Líneas) — El Mayor Monolito del Sistema
* **Ubicación:** `core/src/ui/views/MeshCoreView.cpp`
* **Por qué creció tanto:**
  1. Combina 4 vistas principales en el mismo archivo: Mensajes de Chat, Lista de Nodos/Contactos, Tab de Mapa/Métricas y Tab de Configuración de Enlace.
  2. Implementa internamente modales complejos: exportación de claves, visualización de perfiles, emparejamiento.
  3. Contiene lógica de negocio pura que no pertenece a la UI: decodificación de emojis ASCII/Unicode, cálculo de distancias, filtros de ordenamiento de paquetes y validación de direcciones cortas.
* **Propuesta Quirúrgica de Desacoplamiento:**
  * `core/src/ui/views/mesh/MeshChatTab.cpp`: Componente visual exclusivo de la bandeja de entrada y burbujas de chat.
  * `core/src/ui/views/mesh/MeshNodesTab.cpp`: Lista de contactos, RSSI, saltos y estado de batería de nodos remotos.
  * `core/src/ui/views/mesh/MeshModals.cpp`: Diálogos modales de configuración y exportación de identidad.
  * Mantener `MeshCoreView.cpp` con menos de 400 líneas como mero coordinador de pestañas y despachador de eventos.

---

### 2. `LuaBridge.cpp` (2,348 Líneas) — Monolito de Bindings
* **Ubicación:** `core/src/lua/LuaBridge.cpp`
* **Por qué creció tanto:**
  1. Cada nueva capacidad añadida a CBDos (control de teclado BadUSB, scripts Ducky, clientes SSH, reproducción MP3, lectura de sensores GPIO, escaneo LAN) añadió sus funciones C++ `l_xxx` en este único archivo.
  2. Incluye directamente más de 15 cabeceras de subsistemas dispares (`cbdos/hid.hpp`, `cbdos/audio.hpp`, `cbdos/ssh.hpp`, `cbdos/network.hpp`, etc.).
* **Propuesta Quirúrgica de Desacoplamiento:**
  * Crear subcarpeta `core/src/lua/bindings/`:
    * `LuaBinding_HID.cpp` (badusb, duckyscript, macros de teclado)
    * `LuaBinding_Audio.cpp` (reproducción de archivos, sintetizador TTS, volumen)
    * `LuaBinding_Net.cpp` (wifi, scan lan, ssh, sockets)
    * `LuaBinding_Sys.cpp` (almacenamiento, NVS, memoria, display, leds, gpio)
  * `LuaBridge.cpp` solo orquesta el registro de tablas llamando a `registerHID(L)`, `registerAudio(L)`, etc. Reducción estimada: de 2,348 a 150 líneas.

---

### 3. `meshcore_client.cpp` (1,432 Líneas) — Capa de Red Malla
* **Ubicación:** `core/src/meshcore/meshcore_client.cpp`
* **Por qué creció tanto:**
  1. No separa el nivel de enlace (transporte serial/radio) del nivel de red (enrutamiento de paquetes y confirmaciones ACK).
  2. Administra tanto las colas de retransmisión como el cifrado y la serialización binaria en el mismo flujo.
* **Propuesta Quirúrgica de Desacoplamiento:**
  * `mesh_framing.cpp`: Empaquetado, cálculo de checksums y parsing del flujo de bytes.
  * `mesh_router.cpp`: Tablas de rutas, cálculo de métricas de salto y retransmisión de paquetes no locales.
  * `meshcore_client.cpp`: API pública limpia para la UI y la capa de aplicación.

---

### 4. `RadioView.cpp` (1,284 Líneas) — App "Radio Online" (Streaming Wi-Fi)
* **Ubicación:** `core/src/ui/views/RadioView.cpp`
* **Aclaración de Identidad:**
  * **NO ES radio RF física/SDR.**
  * Es la aplicación de **Web Radio / Streaming por Internet** (`"Radio Online"`).
* **Por qué creció tanto:**
  1. Contiene la barra de reproducción (`buildPlayerBar`) con volumen y metadatos dinámicos ICY.
  2. Pestaña de Emisoras Favoritas (`buildFavoritesView`) con persistencia en LittleFS.
  3. Pestaña de Búsqueda Online (`buildExploreView`) con gestión de peticiones HTTP asíncronas en segundo plano y renderizado paginado de resultados.
  4. Formulario de Emisora Manual (`buildAddManualView`) con integración de teclado virtual.
* **Propuesta Quirúrgica de Desacoplamiento:**
  * `RadioPlayerBar.cpp`: Widget reutilizable de control de reproducción y metadata en streaming.
  * `RadioExploreTab.cpp`: Pestaña de búsqueda online y consulta de directorios web.
  * `RadioFavoritesTab.cpp`: Lista de emisoras guardadas por el usuario.

---

## 4. Archivos Monolíticos del Coprocesador (`bsp/esp32_c6_slave`)

Para referencia técnica, el firmware del coprocesador inalámbrico (ESP32-C6 bajo SDIO) tiene archivos extensos heredados y adaptados de la arquitectura ESP-Hosted:

* `slave_wifi_std.c` (**2,788 líneas**): Manejador estándar de eventos y estados de Wi-Fi de bajo nivel.
* `slave_control.c` (**1,907 líneas**): Máquina de estados de control y mensajería RPC.
* `esp_hosted_coprocessor.c` (**1,309 líneas**): Lógica de puente entre buses SDIO/SPI y la pila de red.
* `spi_slave_api.c` / `spi_hd_slave_api.c` / `sdio_slave_api.c` (**~950 - 1,000 líneas c/u**): Drivers de capa física de transporte esclavo.

*Nota:* Estos archivos del C6 son interfaces de bajo nivel de framework de Espressif; su refactorización debe hacerse con extrema cautela para no romper la compatibilidad con el driver anfitrión SDIO del ESP32-P4.

---

## 5. Conclusión y Recomendación de Prioridad

De los 25 archivos propios que superan las 500 líneas:
1. **Prioridad 1 (Urgente):** [MeshCoreView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/MeshCoreView.cpp) (2,918 l.) y [LuaBridge.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/lua/LuaBridge.cpp) (2,348 l.). Representan más de 5,200 líneas de código concentradas en solo dos archivos del núcleo agnóstico.
2. **Prioridad 2 (Alta):** [meshcore_client.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/meshcore/meshcore_client.cpp) (1,432 l.) y [RadioView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/RadioView.cpp) (1,284 l.).
3. **Prioridad 3 (Media):** Vistas UI de aplicaciones de más de 800 líneas ([LanScannerService.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/network/LanScannerService.cpp), [FileManagerView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/FileManagerView.cpp), [TextEditorView.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/core/src/ui/views/TextEditorView.cpp)).
