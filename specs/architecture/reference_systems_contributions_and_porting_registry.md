# 🏛️ Registro Maestro de Aportes, Portados e Inspiraciones de Sistemas de Referencia (`references/`)

**Fecha de Actualización:** 30 de Octubre de 2026  
**Ubicación:** `specs/architecture/reference_systems_contributions_and_porting_registry.md`  
**Repositorios Locales Auditados:** `references/poseidon`, `references/bruce`, `references/ghostesp`, `references/esp32_marauder`, `references/TentacleOS`, `references/HomeTiles`, `references/momentum-firmware`, `references/tab5-cyberdeck` (DECK//OS), `references/Slave_I`, `references/ESP32-Bit-Pirate`, `references/SquachWatch-CYD`

---

## 1. Propósito y Criterio de Ingeniería (Portar vs. Reimplementar en C++/LVGL 9.5)

En **CBDos**, los sistemas almacenados en `references/` sirven como base de ingeniería probada para no reinventar protocolos complejos desde cero. Sin embargo, dado que `core/` exige **C++17 limpio, desacoplamiento HAL/BSP, Cero Polling innecesario y UI en LVGL 9.5 nativo**, cada módulo seleccionado de un sistema de referencia se clasifica bajo una de estas dos estrategias técnicas según su dificultad de acoplamiento:

1. **🔄 Portado Directo / Adaptación de Motor (`PORT`):**
   - Se utiliza cuando el sistema de referencia tiene un motor en C/C++ bien encapsulado y agnóstico a su UI (por ejemplo: protocolos criptográficos CTAP2/FIDO2, decodificadores de tramas 802.11/BLE, parsers VT100 como `libvterm`, o rutinas matemáticas 2D/DES).
   - Se copia o adapta la lógica algorítmica a `core/` y se le construye encima una vista nueva en **LVGL 9.5** integrada con `HeaderBar`.
2. **🧬 Implementación Propia Basada en Referencia (`CLEAN-ROOM / INSPIRED`):**
   - Se utiliza cuando el código del sistema de referencia está fuertemente acoplado a su propio framework, a un sandbox externo (ej. WebAssembly WAMR en `NucleoOS-P4`), o a pantallas pequeñas no táctiles (`TFT_eSPI` en `Bruce`/`Poseidon`).
   - En este caso auditamos cómo resolvieron los casos borde (structs, estados, registros de hardware) y **escribimos nuestra propia versión nativa en C++ para CBDos**, más rápida, ligera y acoplada a nuestra arquitectura.

---

## 2. Matriz Histórica de Aportes ya Integrados en CBDos

| Sistema Origen (`references/`) | Módulo / Característica en CBDos | Estrategia Aplicada | Archivos en CBDos / Especificación |
| :--- | :--- | :---: | :--- |
| **Poseidon** (`references/poseidon`) | **Kerberos FIDO2 / U2F Hardware Token (Passkeys CTAP2)** | 🔄 **Portado de Motor** (Protocolo CTAP2/CBOR/ECDSA P-256 sobre TinyUSB + UI LVGL 9.5 propia) | `core/src/fido2/`, `specs/drafts/ROADMAP_PORTADO_HERRAMIENTAS_CYBERDECK.md` |
| **Poseidon** (`references/poseidon`) | **LAN Recon v1/v2 & Escáner de Red Local** (`net_lanrecon.cpp`) | 🔄 **Portado y Expandido** (ICMP Ping, ARP, TCP Port Probe, OUI Vendor) | `core/src/network/LanScannerService.cpp`, `specs/drafts/BORRADOR_LAN_RECON_Y_ESCANEADOR_RED_LOCAL.md` |
| **Bruce** + **ESP32 Marauder** | **BLE Shield, Air Recon & Detección de Trackers / Skimmers** | 🔄 **Portado de Firmas y Modelos** (Taxonomía GAP, Apple Continuity, AirTag, Flipper/Spam) | `core/src/ble/`, `specs/architecture/plan_implementacion_ble_shield_bruce_marauder.md` |
| **Bruce** + **GhostESP** + **Marauder** | **BLE Lab & BLE Strike (Payloads Multiecosistema)** | 🔄 **Portado de Payloads** (FastPair, SwiftPair, AppleJuice, HID `0x1812`, ráfagas) | `core/src/ble/BleTransmitter.cpp`, `specs/architecture/plan_ble_lab_suite_completa.md` |
| **GhostESP** + **Bruce** + **Marauder** | **Wi-Fi Strike Suite & Auditoría 802.11 Real** | 🔄 **Portado de Tramas 802.11** (`esp_wifi_80211_tx`, Deauth reason `0x0007`, Beacon, Probe) | `core/src/wifi/`, `specs/architecture/plan_wifi_strike_suite_completa.md` |
| **GhostESP** (`references/ghostesp`) | **Investigación Bidireccional Touch AXS15231B (`JC3248W535`)** | 🤝 **Aporte Mutuo** (El protocolo I2C probado de CBDos se portó a GhostESP y se documentó el debounce) | `bsp/esp32_s3_jc3248/`, `specs/hardware/jc3248_ghostesp_touch_entrega_final.md` |

---

## 3. Aportes Seleccionados de `DECK//OS` (`references/tab5-cyberdeck` - Licencia MIT)

**DECK//OS** es un sistema operativo abierto para ciberdecks basado en ESP32-P4 y LVGL v9.5 puro, distribuido bajo la licencia permisiva **MIT**. Se adopta como la fuente de referencia canónica para el subsistema de emulación de terminal e interfaces direct-draw de alto rendimiento:

| # | Módulo / Característica | Implementación en DECK//OS | Estrategia en CBDos | Evaluación y Justificación Técnica |
| :-: | :--- | :--- | :---: | :--- |
| **D1** | **Motor de Emulación ANSI/VT100 (`libvterm 0.3.3`)** | `components/libvterm/` (MIT oficial, Paul Evans) | 🔄 **Sustitución Limpia (Clean Provenance)** | Motor C99 puro para terminales xterm/VT100 sin modificaciones propietarias. Cadena de custodia limpia con archivo `LICENSE` MIT oficial. Ubicado en `core/src/terminal/libvterm/`. |
| **D2** | **Renderizador Direct-Draw en LVGL 9.5 (`LV_EVENT_DRAW_MAIN`)** | `components/deck_term/deck_term.c` | 🔄 **Portado Arquitectónico a `TerminalDisplay.cpp`** | Sustituye la ineficiente concatenación de strings `#RRGGBB` por dibujo directo de fondos (`lv_draw_rect`) y etiquetas (`lv_draw_label`) con daño selectivo (`cbDamage` -> `lv_obj_invalidate_area`), reduciendo el consumo de CPU a 0% en reposo. |
| **D3** | **Scrollback de 2,000 Líneas en PSRAM (`sb_cell_t`)** | Buffer circular dinámico en PSRAM + gestos táctiles | 🔄 **Integración en `TerminalDisplay`** | Buffer de celdas compactas (codepoint UTF-32, colores RGB y atributos) con soporte de arrastre vertical táctil (`LV_EVENT_PRESSING` / `lv_indev_get_vect`). |

---

## 3.1. Bitácora de Sistemas Expulsados por Licencia Incompatible

| Sistema Expulsado | Licencia Detectada | Causa de Expulsión y Riesgo | Fecha de Purga | Medidas Correctivas Ejecutadas |
| :--- | :--- | :--- | :---: | :--- |
| **NucleoOS-P4** (`references/NucleoOS-P4`) | **PolyForm Noncommercial 1.0.0** (`COMMERCIAL.md`) | Prohíbe el uso comercial, distribución embebida en productos de hardware y despliegue en producción sin pago de licencia privada. Su presencia compromete la seguridad legal de CBDos como SO abierto. | 30 Oct 2026 | 1. Eliminación física total del repositorio local (307 MB purgados).<br>2. Sustitución de `libvterm` por la distribución limpia MIT de **DECK//OS**.<br>3. Cancelación de cualquier portado directo de código; futuros módulos (JPEG HW PPA, VNC RFB 3.8) se implementan desde especificaciones RFC/estándares ESP-IDF v5.5 (Clean-Room). |

---

---

## 4. Nuevos Aportes Seleccionados de `Slave_I` (ESP32-P4 + C6 sobre SDIO)

Tras auditar `references/Slave_I` (firmware para M5Stack Tab5 basado en arquitectura dual **ESP32-P4 + ESP32-C6 sobre SDIO `esp_hosted`** con LVGL v9), se aprueban las siguientes adaptaciones estratégicas para la placa oficial **JC4880P443C**:

| # | Módulo / Característica | Origen en `Slave_I` | ¿Portar o Crear Versión Propia? | Evaluación y Especificación en CBDos |
| :-: | :--- | :--- | :---: | :--- |
| **S1** | **Subsistema Teclado Físico USB Host (HID) e I2C Unificado** | `platforms/tab5/main/hal/components/hal_usb.cpp` (512 líneas) | 🔄 **Portado Directo de Lógica HID y Cola FreeRTOS** | Utiliza `usb/hid_host` oficial de ESP-IDF. Traduce el boot protocol de 8 bytes de teclados USB y registros ASCII de teclados I2C a una cola compartida `kb_queue` vinculada a un indev `LV_INDEV_TYPE_KEYPAD` y `lv_group_t` en LVGL v9.5. Permite navegación completa por flechas y escritura en consolas/editores. Especificación: [`specs/hardware/usb_host_hid_keyboard_p4_spec.md`](../hardware/usb_host_hid_keyboard_p4_spec.md). |
| **S2** | **Sniffer y Auditoría 802.15.4 / Zigbee con PCAP Wireshark** | `platforms/c6/components/spectra5_offensive/spectra5_offensive.c` | 🔄 **Portado de Firmware C6 y Parser P4** | Aprovecha el hardware 802.15.4 nativo del ESP32-C6 (`esp_ieee802154`), capturando tramas en ISR IRAM, con salto automático de canales 11-26 y volcado directo a archivos `.pcap` en la microSD con `LINKTYPE_IEEE802_15_4_NOFCS` (230) abribles en Wireshark. Especificación: [`specs/architecture/especificacion_coprocesador_c6_spectra5_sdio.md`](especificacion_coprocesador_c6_spectra5_sdio.md). |
| **S3** | **Motor de Inyección Raw Wi-Fi y Extractor EAPOL / PMKID Hashcat 22000** | `platforms/c6/components/spectra5_offensive/` | 🔄 **Portado de Parser C y Opcodes RPC** | 1) Bypass de validación en `libnet80211` con `-Wl,-zmuldefs` para inyección de tramas de desautenticación sin pánico en modo AP.<br>2) Parser de tramas EAPOL (M1..M4) y extracción directa de PMKID (tag `0xDD` RSN) a formato `WPA*01*...***` (Hashcat mode 22000) y volcado a `.pcap` 802.11 (LINKTYPE 105). |
| **S4** | **Auto-Provisionamiento Single-Flash del Coprocesador C6** | `platforms/tab5/main/services/c6_updater.cpp` | 🔄 **Portado de Flasher OTA SDIO** | El binario compilado del C6 se incrusta en el binario del P4 (`target_add_binary_data`). En el primer arranque, el P4 valida el CRC en NVS y flashea el C6 por OTA a través de SDIO con `rpc_ota_*` sin requerir cables ni botones físicos. |
| **S5** | **Resolución Ultraligera OUI Vendor Lookup** | `app/domain/radio/oui.cpp` | 🔄 **Portado Inmediato a Core** | Inspección en un solo ciclo del bit 1 de la MAC (`mac[0] & 0x02`) para catalogar MACs aleatorias/privadas (`(random)`), y tabla directa en Flash de los 40 fabricantes de consumo más comunes sin alocación dinámica en heap. |

---

## 5. Aportes Técnicos y Lecciones de `HomeTiles` (Hardware JC4880 P4 y UI)

Aunque **HomeTiles** es un firmware de domótica para pared (Appliance) cuya lógica de Home Assistant y MQTT no encaja en la arquitectura de un Cyberdeck OS, su hardware de referencia principal es nuestra placa gemela **Guition JC4880P443C (ESP32-P4)** y su motor gráfico es **LVGL v9.6** (100% compatible con nuestro estándar LVGL v9.5).

A continuación se registran los aportes de bajo nivel y soluciones de silicio para consulta y portado selectivo:

| # | Módulo / Característica | Origen en `HomeTiles` | ¿Portar o Crear Versión Propia? | Evaluación y Justificación Técnica en CBDos |
| :-: | :--- | :--- | :---: | :--- |
| **H1** | **Árbitro de DMA2D y Protección "PPA Wedged"** (Evita error 259 en ESP-IDF v5.5) | `src/core/display/dma2d_arbiter.h` + `.cpp` (120 líneas) | 🔄 **Portado Directo a BSP P4** | Resuelve la colisión por hardware entre el acelerador PPA y el decodificador JPEG al compartir canales DMA2D. Incluye guardia RAII (`Dma2dArbiterGuard`) y conmutación transparente a rotación por CPU (`p4_dsi_cpu_rotate.h`) si el PPA entra en timeout (>200 ms). Vital para visor VNC y streaming en P4. |
| **H2** | **Drivers de Cámara Física MIPI-CSI en P4 (SC202CS, OV5647, OV02C10)** | `src/video/local_camera/` + `local_camera_board.cpp` | 🔄 **Portado de Inicialización a BSP P4** | Provee la secuencia SCCB/I2C, configuración del regulador interno LDO Canal 3 a 2500 mV y el pipeline CSI para el conector de cinta de la JC4880. Permite dotar al Cyberdeck P4 de cámara de inspección o visión de campo sin ingeniería inversa. |
| **H3** | **Streaming de Video con Control de Flujo TCP ACK (Backpressure)** | `src/video/camera_stream.cpp` + `tcp_ack_wire.h` | 🔄 **Adaptación a `VideoStreamingService`** | Implementa triple buffering (`kFrameBufferCount = 3`) y control de ventana por TCP ACK para recepción de MJPEG: si el tráfico satura la memoria o cae el margen DMA (`kMinCameraDmaHeadroomBytes`), frena al emisor para no agotar la RAM de red de LwIP. |
| **H4** | **Configuración Fina de Hardware JC4880 (SDMMC, Panel DSI, Touch)** | `src/devices/guition_jc4880p443_portrait/` | 🔄 **Referencia y Ajustes al BSP P4** | 1) SDMMC Slot-0 (4 bits) en pines IO-MUX 39..44 con alimentación interna LDO Canal 4 a 3300 mV.<br>2) Panel ST7701 DSI 2-lanes con encendido suave de backlight (90 ms warmup para evitar destellos blancos al boot).<br>3) Filtro anti-jitter y debounce táctil GT911 (`kTouchJitterThresholdPx = 2`). |
| **H5** | **Catálogo de Material Design Icons (MDI) y Extractor para LVGL 9** | `src/fonts/` + `mdi-extractor/` | 🔄 **Reutilización de Tooling y Fuentes** | Iconos compilados para LVGL v9 en tamaños desde 15px hasta 48px y script generador en Node.js para convertir cualquier glifo MDI en arrays C de LVGL sin depender de convertidores web manuales. |

### 5.1. Similitudes y Lecciones Arquitectónicas Observadas:
* **Uso Estricto de `namespaces` en C++:** Ambos proyectos aíslan sus subsistemas en espacios de nombres limpios (`local_camera::`, `dma2d_arbiter::`, `speaker::`, `cbdos::`), evitando colisiones de símbolos y funciones globales dispersas.
* **Manejo de Idiomas e Internacionalización (`i18n.cpp`):** Manejan las traducciones mediante un catálogo monolítico de cadenas con perfiles de estado, exactamente el mismo enfoque estructurado que utilizábamos en fases tempranas de CBDos antes de modularizar las tablas de texto.
* **Patrón Ergonómico del HeaderBar con Botón "Volver" (`<`):** Valida la decisión de diseño de CBDos de usar un botón de navegación visible en la barra superior en lugar de gestos táctiles complejos (*swipes*), garantizando fiabilidad contra el ruido de pantalla y actuando como el destructor seguro de memoria de la vista activa (`popView`).

---

## 6. Nuevos Aportes Seleccionados de `ESP32-Bit-Pirate` (Hardware Hacking & Memory Exploitation)

`references/ESP32-Bit-Pirate` es un firmware multipropósito de análisis y explotación de hardware para ESP32/S3/P4. Cuenta con módulos listos para interactuar a nivel eléctrico y de protocolo con memorias no volátiles y periféricos externos para tareas de volcado (dump), lectura, borrado y flasheo.

Se registran los siguientes componentes clave para futura evaluación y portado a CBDos:

| # | Módulo / Característica | Origen en `ESP32-Bit-Pirate` | ¿Portar o Crear Versión Propia? | Evaluación y Justificación Técnica en CBDos |
| :-: | :--- | :--- | :---: | :--- |
| **B1** | **SPI Flash Flasher & Dumper Universal (JEDEC RDID 0x9F)** | `src/Services/SpiService.cpp` + `src/Data/FlashDatabase.h` | 🔄 **Portado Directo de Rutinas y Base de Datos** | Incluye base de datos con >300 modelos de memorias SPI Flash (Winbond, Macronix, Spansion, GigaDevice, etc.), soporte para modos 3-byte y 4-byte (>16MB), comandos Write Enable (`0x06`), Page Program (`0x02`/`0x12`), Sector Erase (`0x20`/`0xD8`), Chip Erase (`0xC7`), verificación bit a bit y yield cooperativo con FreeRTOS para evitar triggers del Task Watchdog. Permite convertir el Cyberdeck en programador de BIOS/chips externos con pinzas SOIC-8. |
| **B2** | **I2C EEPROM Suite (Series 24Cxx: 24C01 a 24C1024)** | `src/Services/I2cService.cpp` (L587-668) | 🔄 **Portado a Servicio I2C de CBDos** | Detección automática de capacidad (`detectMemorySizeBytes`), tamaño de página (`detectPageSizeBytes`), longitud de direccionamiento (1 o 2 bytes) y tiempos de escritura. Funciones completas de lectura secuencial, volcado de memoria (dump), escritura y borrado de EEPROMs I2C. |
| **B3** | **Smartcard 2-Wire & Protocolos SLE44xx** | `src/Services/TwoWireService.cpp` + `.h` | 🧬 **Versión Propia Inspirada / Portado Opcional** | Rutinas de lectura ATR (Answer to Reset), decodificación de estructuras protegidas, lectura y escritura de memorias de seguridad y verificación de códigos PIN/PSC (ej. tarjetas telefónicas antiguas o chips criptográficos de 2 hilos). |
| **B4** | **Hardware Pinout Analyzer (Automated Pinout Hunting)** | `src/Analyzers/PinAnalyzer.cpp` + `.h` | 🔄 **Adaptación a Tool de Diagnóstico GPIO** | Algoritmo tipo *JTAGulator* para identificar automáticamente líneas UART (TX/RX), I2C (SDA/SCL) o SPI mediante barrido y análisis de estados lógicos y transiciones en pines desconocidos. |
| **B5** | **Sniffer I2C en Tiempo Real** | `src/Services/I2cSnifferService.cpp` | 🔄 **Adaptación a Driver RMT / Interrupción** | Captura de tramas en el bus I2C (Start, Stop, ACK/NACK, Addr, Data) para depurar periféricos en placa o interceptar comunicaciones entre chips externos. |

---

## 7. Nuevos Aportes Seleccionados de `SquachWatch-CYD` (Detección de Drones y Firmas de Vigilancia)

`references/SquachWatch-CYD` contiene rutinas matemáticas de radiofrecuencia y tablas de firmas auditadas contra la IEEE para dispositivos de vigilancia aérea y física.

Se registran los siguientes componentes clave para integración en CBDos (especificación detallada en [`plan_integracion_remote_id_firmas_rf_y_pin_senuelo.md`](plan_integracion_remote_id_firmas_rf_y_pin_senuelo.md)):

| # | Módulo / Característica | Origen en `SquachWatch-CYD` | ¿Portar o Crear Versión Propia? | Evaluación y Justificación Técnica en CBDos |
| :-: | :--- | :--- | :---: | :--- |
| **S1** | **Decodificador ASTM F3411 Remote ID (Drones)** | `src/remote_id.cpp` + `include/remote_id.h` | 🔄 **Portado Directo de Parser C++** | Parser autónomo sin librerías externas que decodifica paquetes BLE AD 0x16 (0xFFFA) y Wi-Fi vendor elements. Extrae latitud, longitud, altitud barométrica, número de serie y la **posición geográfica exacta del operador/piloto**. Se integra directamente en `BleShieldService.cpp` de CBDos. |
| **S2** | **Tabla de Firmas Auditada con Confianza Escalonada** | `src/signatures.cpp` + `include/signatures.h` | 🔄 **Portado y Adaptación de Taxonomía** | Firmas verificadas contra el registro IEEE separadas por nivel de certeza (`HIGH_CONF`, `MED_CONF`, `LOW_CONF`). Permite detectar cámaras ALPR (Flock, Motorola, Genetec), bodycams Axon, detectores de disparos ShotSpotter y skimmers SPP de tarjetas sin incurrir en falsos positivos causados por direcciones MAC estándar de Espressif. |
| **S3** | **Modo Señuelo para Código de Coacción (Decoy Mode)** | Inspirado en el concepto de PIN de coacción de `src/security.cpp` | 🧬 **Versión Propia Superior (Modo Señuelo de CBDos)** | En lugar de purgar datos de forma evidente, el PIN secundario disfraza el sistema ocultando la suite táctica, la bóveda y los escáneres, presentando a CBDos exclusivamente como un reproductor MP3 y visor de utilidades estándar. |
| **--** | *Renderizado de Mascota en C++ Procedural* | `src/squachy.cpp` (9.000 líneas) | ❌ **DESCARTADO** | El enfoque de dibujar extremidades con primitivas matemáticas en C++ es rígido y difícil de mantener. En CBDos se mantiene la arquitectura moderna de **Lottie (ThorVG) en LVGL 9.5** vía JSON en MicroSD. |

---

## 8. Criterio de Decisión Rápida al Momento de Implementar

Cuando iniciemos la codificación de cualquiera de los puntos anteriores, aplicaremos esta regla práctica:
1. **Si el archivo de referencia habla con estándares (Lua C API, ESP-IDF `usb/hid_host`, `esp_ieee802154`, `jpeg_decode`/`ppa`, `libvterm`, sockets LwIP RFB/DES, árbitro DMA2D, comandos SPI Flash JEDEC 0x9F/0x02/0x03):** **Lo portamos directo** adaptando solo los nombres de funciones al namespace `cbdos::` para ahorrar semanas de depuración de protocolos.
2. **Si el archivo de referencia habla con su propio sistema de ventanas o frameworks propietarios:** **No arrastramos sus dependencias**; escribimos nuestra clase limpia en C++ usando `BaseView`, `HeaderBar`, `DefaultTheme` y `LuaBridge` de CBDos, guiándonos únicamente por sus fórmulas y estructuras de datos.
3. **Para subsistemas de `Slave_I` (USB HID Host y Radio C6 sobre SDIO):** Se aplica el mismo principio; se portan directamente los controladores que interactúan con estándares de ESP-IDF (`usb/hid_host`, `esp_ieee802154`, `libnet80211`), adaptándolos a la arquitectura de colas y vistas `BaseView` de CBDos sin importar su shell de UI.
4. **Para aportes de `HomeTiles` (Hardware JC4880 y DMA2D):** Se toman exclusivamente los drivers de bajo nivel (`dma2d_arbiter`, LDOs de SDMMC/CSI y control de flujo de video), ignorando por completo toda la capa de Home Assistant y MQTT.
5. **Para aportes de `ESP32-Bit-Pirate` (Memorias y Hardware Hacking):** Se aprovechan las tablas de comandos físicos y tiempos (`FlashDatabase.h`, opcodes SPI/I2C EEPROM) integrándolos directamente sobre los buses SPI e I2C del BSP de CBDos con interfaz gráfica LVGL 9.5 y/o comandos desde la shell.


