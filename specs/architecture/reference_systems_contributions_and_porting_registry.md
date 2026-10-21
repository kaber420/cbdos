# 🏛️ Registro Maestro de Aportes, Portados e Inspiraciones de Sistemas de Referencia (`references/`)

**Fecha de Actualización:** 21 de Octubre de 2026  
**Ubicación:** `specs/architecture/reference_systems_contributions_and_porting_registry.md`  
**Repositorios Locales Auditados:** `references/poseidon`, `references/bruce`, `references/ghostesp`, `references/esp32_marauder`, `references/TentacleOS`, `references/HomeTiles`, `references/momentum-firmware`, `references/NucleoOS-P4`

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

## 3. Nuevos Aportes Seleccionados de `NucleoOS-P4` (En Cola de Implementación)

Tras auditar `references/NucleoOS-P4`, se aprobaron **dos grandes bloques de evolución** para CBDos: las mejoras al ecosistema **Lua++ (`cbdos.lua`)** y las **3 herramientas de Redes / Servidores / Hardware P4**.

Para cada punto se define claramente si conviene **portar su código C/C++ directamente** o **crear nuestra versión nativa inspirada en la suya**:

### 3.1. Bloque A: Evolución del Ecosistema Lua++ y Automatización
*(Detalle técnico en [`comparison_nucleoos_p4_vs_cbdos_lua.md`](comparison_nucleoos_p4_vs_cbdos_lua.md) y [`proposal_lightweight_cron_short_id_launcher.md`](../proposals/proposal_lightweight_cron_short_id_launcher.md))*

| # | Funcionalidad | Origen en `NucleoOS-P4` | ¿Portar o Crear Versión Propia? | Justificación Técnica de Integración en CBDos |
| :-: | :--- | :--- | :---: | :--- |
| **A1** | **Trazas de Error Completas (`luaL_traceback`) y `.last_error`** | `ports/luaapp/luaapp.c` (L708-725, L809-812) | 🔄 **Portado Directo (~20 líneas C)** | El callback `traceback()` de `luaapp.c` usa API estándar de Lua 5.4 (`luaL_traceback`). Se integra directo en `LuappView.cpp` y `LuaBindings_UI.cpp`. |
| **A2** | **Timers Reactivos en Lua (`cbdos.timer.every` / `after` / `cancel`)** | `ports/luaapp/lib/nvrt.lua` (L61-79) | 🧬 **Versión Propia Nativa sobre `lv_timer_t`** | En NucleoOS los timers hacen polling en cada frame porque no tienen LVGL. En CBDos es mucho más limpio y eficiente crearlos sobre `lv_timer_create` de LVGL 9.5 vinculados a `LuappView`. |
| **A3** | **Persistencia Clave-Valor por App (`cbdos.storage.save` / `load`)** | `ports/luaapp/lib/nvrt.lua` (L81-113) | 🧬 **Versión Propia en C++ (`LuaBindings_FS.cpp`)** | NucleoOS lo hace en Lua sobre su sandbox WASI. En CBDos es más rápido serializar la tabla/valor desde C++ en `/sdcard/apps/.data/<app>.json` con escritura atómica (`.tmp` -> `rename`). |
| **A4** | **Apps `.luapp` en Carpeta + `require()` relativo** | `ports/luaapp/luaapp.c` (`searcher` L640-665) | 🔄 **Adaptación de `searcher` C** | La función `searcher()` de `luaapp.c` que transforma `require("a.b")` en la ruta local de la app es C puro de Lua y se adapta fácilmente a `LuappView.cpp` y `LuappManager.cpp`. |
| **A5** | **Primitivas 2D Avanzadas para `cbdos.canvas` / `cbdos.gfx`** | `ports/luaapp/gfx.c` (600 líneas C) | 🔄 **Portado Selectivo de Rutinas C** | `gfx.c` es código C autónomo que opera sobre buffers `uint16_t*` RGB565 (igual que nuestro `LuaCanvasCtx`). Podemos portar sus funciones de arcos, bordes antialias, polígonos y rotación directamente a `LuaBindings_Canvas.cpp`. |

---

### 3.2. Bloque B: Redes, Administración de Servidores y Aceleración Hardware P4
*(Detalle técnico en [`proposal_vt100_terminal_vnc_client_and_hw_jpeg_ppa.md`](../proposals/proposal_vt100_terminal_vnc_client_and_hw_jpeg_ppa.md))*

| # | Funcionalidad para Redes / Campo | Origen en `NucleoOS-P4` | ¿Portar o Crear Versión Propia? | Evaluación de Dificultad y Estrategia Recomendada |
| :-: | :--- | :--- | :---: | :--- |
| **B1** | **Aceleración Hardware JPEG + Escalador PPA en ESP32-P4** (Para Cámaras IP en `LAN NetLab` y VNC) | `components/nv_apps/gallery_jpeg_hw.cpp` (286 líneas) | 🔄 **Portado Directo al BSP P4 (Muy Fácil)** | `gallery_jpeg_hw.cpp` usa directamente las APIs oficiales de ESP-IDF v5.5 (`driver/jpeg_decode.h` y `driver/ppa.h`) sin depender de NucleoOS. Ya resuelve los 3 bugs difíciles del P4: redondeo MCU 16×16, orden BGR para LVGL RGB565 y alineación de caché de 64 bytes en PSRAM. Se porta casi 1:1 a `bsp/esp32_p4_jc4880/`. |
| **B2** | **Emulador de Terminal ANSI/VT100 (`libvterm`)** (Para SSH interactivo `htop`/`nano` y Consola UART) | `components/libvterm/` + `components/nv_apps/terminal_app.cpp` | ⚖️ **Híbrido: Portar `libvterm` C intacto + Crear Widget LVGL 9.5 propio** | 1) La carpeta `components/libvterm/` es C puro estándar sin dependencias: **se copia intacta** a `core/`.<br>2) `terminal_app.cpp` (1,924 líneas) tiene partes acopladas al shell WASI de NucleoOS, por lo que **no conviene copiarlo entero**: tomamos solo su lógica de pintado de celdas dañadas (`damage`), paleta ANSI y barra de teclas (`CTRL`, `ESC`, `TAB`, flechas) en una clase limpia `VTermWidget` conectada a `cbdos::ssh` y `cbdos::uart`. |
| **B3** | **Cliente de Escritorio Remoto VNC (RFB 3.8 + Tight-JPEG / ZRLE + mDNS)** | `components/nv_secondscreen/ss_vnc.cpp` (1,175 líneas) + `ss_des.c` (180 líneas) | ⚖️ **Híbrido: Portar Motor RFB/DES + Crear Vista `VncViewerView` propia** | 1) `ss_des.c` (autenticación VNC) y el parser de codificaciones RFB de `ss_vnc.cpp` (`Raw`, `CopyRect`, `Hextile`, `Zlib`, `ZRLE`, `Tight-JPEG` y escalado de filas al vuelo sin buffer 4K) están muy bien aislados en una sola tarea FreeRTOS con sockets LwIP estándar: **se portan directamente**.<br>2) La parte de `ss_engine.cpp` que apaga LVGL es específica de su placa; en CBDos es mejor crear nuestra propia `VncViewerView` derivada de `BaseView` para que funcione tanto en **ESP32-P4** como en **ESP32-S3** e integrarla con el escáner `LAN NetLab` (puerto 5900). |

---

## 4. Criterio de Decisión Rápida al Momento de Implementar

Cuando iniciemos la codificación de cualquiera de los puntos anteriores, aplicaremos esta regla práctica:
1. **Si el archivo de `NucleoOS-P4` habla con estándares (Lua C API, ESP-IDF `jpeg_decode`/`ppa`, `libvterm`, sockets LwIP RFB/DES):** **Lo portamos directo** adaptando solo los nombres de funciones al namespace `cbdos::` para ahorrar semanas de depuración de protocolos.
2. **Si el archivo de `NucleoOS-P4` habla con su propio sistema de ventanas (`nv_ui_kit`, `nv_wasm`, `nv_event_bus`):** **No arrastramos sus dependencias**; escribimos nuestra clase limpia en C++ usando `BaseView`, `HeaderBar`, `DefaultTheme` y `LuaBridge` de CBDos, guiándonos únicamente por sus fórmulas y estructuras de datos.
