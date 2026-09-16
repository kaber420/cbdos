# CyBerDeck OS — CBDos v0.2.3-dev

**CyBerDeck OS (CBDos)** es un sistema operativo embebido modular, agnostico y *offline-first* disenado para cyberdecks, consolas portatiles y dispositivos multimedia basados en microcontroladores ESP32. Incluye soporte para topologias de red federadas, jerarquicas y ruteadas (basadas en Torres/Zonas). Arquitectura `core/` desacoplada de cualquier SDK de hardware, UI en **LVGL v9.5** y soporte multi-target simultaneo.

> **Estado general:** CBDos v0.2.3-dev. El nucleo `core/`, motor de UI, audio, emuladores, terminales, malla mesh, seguridad FIDO2 y todas las vistas principales estan implementados y operativos. Soporte completo multi-target ESP32-P4 y ESP32-S3.

---

## Arquitectura Modular (Core / HAL / BSP)

```
+--------------------------------------------------------------------------+
|  APLICACIONES & UI                                                        |
|  Dashboard · Config · WiFi · Storage · Wallpaper · Music · Radio          |
|  FileManager · TextEditor · Terminal · SerialTerminal · Flasher           |
|  Cartridge · Gallery · LuaRunner · LuappView · LottieTest                 |
|  MeshConfig · NetworkManager · PowerConfig · TimeConfig                   |
|  TlvBrowser · RadioConfig · Utilities (Calc/Stopwatch/Pomodoro/Todo)      |
+--------------------------------------------------------------------------+
|  CBDos API                                                                |
|  UIManager · ThemeEngine · WallpaperManager · AudioPlayer (Helix MP3)     |
|  RadioManager · WavPlayer · WavRecorder · LuaEngine · LuappManager        |
|  HidManager · DuckyInterpreter · CartridgeManager · MeshEngine            |
|  KerberosManager (FIDO2/U2F) · SSH Client · PowerManager · ConfigManager  |
|  TimeManager · TTSEngine · SerialStreamAdapter · SshStreamAdapter        |
+--------------------------------------------------------------------------+
|  HAL Interfaces                                                           |
|  AudioHAL · StorageHAL · NetworkHAL · SystemHAL · DisplayHAL · TouchHAL   |
|  InputHAL · FlasherHAL · HIDHAL · HTTPHAL · MeshHAL · RadioHAL            |
|  SocketHAL · SSHHAL · UARTHAL · PersistenceHAL                            |
|  100% agnostico — sin dependencias directas a ESP-IDF ni Arduino.h        |
+----------------------------------+-------------------+-------------------+
|  BSP: ESP32-P4                    |  BSP: ESP32-S3    |                   |
|  bsp/esp32_p4_jc4880             |  bsp/esp32_s3_jc3248                  |
|  ESP-IDF 5.5 / CMake / Ninja     |  PlatformIO + Arduino (pioarduino)    |
+----------------------------------+-------------------+-------------------+
```

---

## Dispositivos y Targets Oficiales

### Guition JC4880P443C — `bsp/esp32_p4_jc4880` *(target principal)*

| Componente | Detalle |
|:---|:---|
| **SoC** | ESP32-P4 RISC-V Dual-Core @ 400 MHz (Chip Rev 1.3) |
| **Memoria** | 16 MB Flash · 32 MB Hexal-PSRAM |
| **Pantalla** | 4.3" IPS 480x800 MIPI-DSI (ST7701S) @ 60 FPS |
| **Tactil** | Goodix GT911 (I2C SDA=7, SCL=8, RST=3, INT=4) |
| **Audio** | Everest ES8311 (I2C + I2S MCLK=13 BCLK=12 WS=10 DOUT=9) · Amp PA=11 |
| **Almacenamiento** | MicroSD Slot 0 SDMMC 4-bit (GPIO 39-44) + LDO VO4 3.3 V |
| **Coprocesador** | ESP32-C6-MINI (Wi-Fi 6 / BT 5) via SDIO Slot 1 |
| **Framework** | ESP-IDF 5.5 nativo (CMake / Ninja) |

### Guition JC3248W535 — `bsp/esp32_s3_jc3248`

| Componente | Detalle |
|:---|:---|
| **SoC** | ESP32-S3 Xtensa Dual-Core @ 240 MHz |
| **Memoria** | 16 MB Flash · 8 MB PSRAM |
| **Pantalla** | 3.5" IPS 320x480 QSPI (AXS15231B) @ 30 FPS |
| **Audio** | PDM TX / Helix MP3 · MicroSD SPI |
| **Framework** | PlatformIO + Arduino Core (pioarduino) |

---

## Estado de Modulos

| Modulo / Vista | Descripcion | Estado | S3 | P4 |
|:---|:---|:---:|:---:|:---:|
| **Core UI Engine** | `BaseView`, `UIManager`, ciclo de vida de vistas — LVGL 9.5 | ✅ | ✅ | ✅ |
| **Theme Engine** | Paletas dinamicas: Cyberpunk · Dark · Light · Acentos | ✅ | ✅ | ✅ |
| **Wallpaper Engine** | Fondos dinamicos en PSRAM + `WallpaperConfigView` | ✅ | ✅ | ✅ |
| **Dashboard View** | Pantalla principal tipo Cyberdeck + accesos directos | ✅ | ✅ | ✅ |
| **Config View** | Menu maestro de ajustes del sistema | ✅ | ✅ | ✅ |
| **WiFi Config View** | Escaneo, conexion y gestion de credenciales | ✅ | ✅ | 🟡 via C6 |
| **Storage Config View** | Diagnostico de particiones, MicroSD, LittleFS | ✅ | ✅ | 🟡 SDMMC |
| **Audio Core (Helix)** | Decodificador MP3/AAC/WAV/FLAC Helix en PSRAM + buffer I2S | ✅ | ✅ | ✅ |
| **Music Player View** | UI de reproductor: lista, scrubber, volumen, caratulas | ✅ | ✅ | ✅ |
| **Audio Recorder View** | Grabacion de audio con VU meter y nomenclatura de archivos | ✅ | ✅ | ✅ |
| **Radio View** | Radio por internet, lista de emisoras, streaming | ✅ | ✅ | ✅ |
| **Radio Config View** | Configuracion de radio 2.4 GHz, escaneo, seleccion de canal | ✅ | ✅ | ✅ |
| **File Manager View** | Explorador de archivos universal MicroSD/Flash | ✅ | ✅ | ✅ |
| **Text Editor View** | Editor de texto con numeracion de lineas y guardado | ✅ | ✅ | ✅ |
| **Terminal View** | Consola serial interactiva con historial | ✅ | ✅ | ✅ |
| **Serial Terminal View** | Terminal UART para routers, sensores y debug | ✅ | ✅ | ✅ |
| **Flasher View** | Programador de campo autonomo (USB-C y UART) | ✅ | ➖ | ✅ |
| **Cartridge View** | Ejecutor de juegos/cartuchos retro (GBC, NES, DOOM) | ✅ | ✅ | ✅ |
| **Lua Runner View** | Ejecutor de scripts Lua con logging y output | ✅ | ✅ | ✅ |
| **Lua++ (Luapp) View** | VM Lua aislada por-app con bindings LVGL nativos | ✅ | ✅ | ✅ |
| **Lottie Test View** | Animaciones Lottie vectoriales embebidas y desde SD | ✅ | ✅ | ✅ |
| **Gallery View** | Visor de imagenes desde MicroSD | ✅ | ✅ | ✅ |
| **Mesh Config View** | Configuracion de nodo mesh: ID, canal, potencia TX | ✅ | ✅ | ✅ |
| **Network Manager View** | Escaneo WiFi, conexion y estado de red | ✅ | ✅ | ✅ |
| **Power Config View** | Modo suspension, timer auto-off, brillo | ✅ | ✅ | ✅ |
| **Time Config View** | Servidor NTP, zona horaria, horario de verano | ✅ | ✅ | ✅ |
| **TLV Browser View** | Navegador de protocolo TLV sobre ESP-NOW/WiFi | ✅ | ✅ | ✅ |
| **Utilities View** | Utilidades: Calculadora, Stopwatch, Pomodoro, Todo | ✅ | ✅ | ✅ |
| **USB HID / BadUSB Engine** | Teclado + Ratón compuesto, DuckyScript (`.dd`) y Lua reactivo | ✅ | ✅ | ✅ |
| **USB Host CDC** | Perifericos Plug & Play por puerto USB-C (modems, radios, serial) | ✅ | ➖ | ✅ |
| **Mesh Routing Engine** | Direccionamiento IPv4 Mesh, Short IDs, Pseudo-ARP | ✅ | ✅ | ✅ |
| **SSH Client** | Terminal SSH embebida con autenticacion por llaves | ✅ | ✅ | ✅ |
| **Kerberos FIDO2/U2F** | Seguridad: CTAP2, CTAPHID, U2F, COSE, CBOR, attestacion | ✅ | ⏳ | ✅ |
| **Backpack Manager** | Auto-deteccion NFC, reconfiguracion dinamica de GPIOs en JP1 | ✅ 15% | ⏳ | ✅ |
| **Lottie Animations** | Motor de animaciones vectoriales con robot mascot | ✅ | ✅ | ✅ |
| **TTS Engine (PicoTTS)** | Sintesis de voz offline (ES/EN) — bindings Lua pendientes | ✅ 75% | ✅ | ✅ |
| **Synth Sound Engine** | Motor de sintesis y generador de ondas | 📋 Planificado | ⏳ | ⏳ |
| **System Info Monitor** | Monitor de RAM, Heap, FPS y temperatura en tiempo real | ⏳ Pendiente | ⏳ | ⏳ |

*Leyenda: ✅ Operativo · 🟡 Integracion de driver en curso · 📋 Planificado · ⏳ Pendiente · ➖ No aplica*

---

## Apps Lua++ (`apps/`)

CBDos incluye un formato de micro-aplicaciones `.luapp` con VM aislada por app y bindings nativos a las APIs del sistema:

| App | Descripcion |
|:---|:---|
| `cbd_paint.luapp` | Aplicacion de pintura y dibujo |
| `sensor_monitor.luapp` | Monitor de sensores en tiempo real |
| `serial_beacon.luapp` | Beacon de transmision serial |


> Ver [`specs/api/luapp_specification.md`](specs/api/luapp_specification.md) para el formato completo de Lua++.

---

## Cartuchos y Emuladores (`cartridges/`)

Motor de cartuchos retro con emuladores nativos para ESP32-P4:

| Cartucho | Descripcion |
|:---|:---|
| `esp32_p4_doom/` | DOOM (id Software) — puerto nativo para P4 |
| `esp32_p4_gbc/` | Game Boy Color — emulador Peanut GB |
| `esp32_p4_nes/` | NES — emulador para P4 |
| `esp32_p4_lua/` | Lua Engine — ejecucion de scripts Lua en P4 |
| `common_lua_engine/` | Motor Lua compartido: CbdApi, PICO-8 API, VirtualKeyboard, AsyncFS |

Los binarios pre-compilados se encuentran en `cartridges/bin/` y `bin/`.

---

## Compilar y Flashear

### Target ESP32-P4 (ESP-IDF 5.5)

```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_p4_jc4880
idf.py build

# Flashear y monitorear:
idf.py -p /dev/ttyACM0 flash monitor
```

### Target ESP32-S3 (PlatformIO + Arduino)

```bash
# Compilar:
pio run -d bsp/esp32_s3_jc3248

# Flashear:
pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0

# Monitor serie:
pio device monitor -d bsp/esp32_s3_jc3248 -b 115200
```

### Coprocesador ESP32-C6 (Firmware SDIO)

```bash
. /home/kaber420/esp/esp-idf/export.sh
cd bsp/esp32_c6_slave
idf.py build
# Para flashear usa el C6 Flasher Bridge (ver specs/hardware/usb_c_field_flasher_milestone.md)
```

### Cartuchos (GBC, NES, DOOM para P4)

```bash
# Game Boy Color:
cd cartridges/esp32_p4_gbc && idf.py build

# NES:
cd cartridges/esp32_p4_nes && idf.py build

# DOOM:
cd cartridges/esp32_p4_doom && idf.py build
```

### Puente ESP-NOW USB (ESP32-C3)

```bash
cd tools/espnow_usb_bridge
pio run
```

---

## Branches

| Branch | Descripcion |
|:---|:---|
| `main` | Rama estable, versiones release |
| `0.2.2` | Rama de desarrollo activo |
| `feature/vector-lottie-engine` | Feature branch: motor de animaciones vectoriales Lottie |

---

## Documentacion y Especificaciones de Ingenieria

> 🌐 **Portal Web Oficial:** Documentacion limpia y guias paso a paso en [kaber420.github.io/cbdos](https://kaber420.github.io/cbdos/) (alojado en `docs/`).

Especificaciones tecnicas y arquitectura detallada en [`specs/`](specs/):
- **Mochilas Modulares & NFC:** [`specs/architecture/backpack_manager_and_dynamic_gpio_nfc_spec.md`](specs/architecture/backpack_manager_and_dynamic_gpio_nfc_spec.md)
- **USB Host CDC-ACM:** [`specs/architecture/especificacion_usb_device_manager_y_ecosistema_perifericos.md`](specs/architecture/especificacion_usb_device_manager_y_ecosistema_perifericos.md)
- **USB HID / BadUSB & Smart Automation:** [`specs/architecture/especificacion_tecnica_gestor_usb_modos_y_api_badusb.md`](specs/architecture/especificacion_tecnica_gestor_usb_modos_y_api_badusb.md)
- **Programador de Campo (USB-C & UART C6):** [`specs/hardware/usb_c_field_flasher_milestone.md`](specs/hardware/usb_c_field_flasher_milestone.md)
- **Pinouts y Puertos de Hardware:** [`specs/hardware/pinouts_and_ports.md`](specs/hardware/pinouts_and_ports.md)


---

## Reglas de Desarrollo

- **Offline-First estricto:** El sistema es 100% funcional sin red. La inicializacion de Wi-Fi/BT es exclusivamente bajo demanda desde la UI.
- **LVGL v9.5 estricto:** Prohibido usar macros o sintaxis de LVGL v8 (`lv_scr_act()`, `LV_MEM_CUSTOM`, etc.). Solo APIs v9.5 (`lv_screen_active()`, `lv_button_create()`, etc.).
- **Verificacion dual-target:** Cada cambio en `core/` debe compilar en **ambos** entornos (`idf.py build` y `pio run`).
- **Documentar hardware:** Cualquier pin, bus o registro descubierto se registra inmediatamente en `specs/hardware/pinouts_and_ports.md`.
- **Lua++ y `.luapp`:** Las micro-aplicaciones se escriben en Lua++ con VM aislada. Ver `specs/api/luapp_specification.md` para el formato y APIs disponibles.
- **Seguridad:** Nunca exponer ni registrar secrets y keys. No commitear credenciales al repositorio.

---

## Licencia

Este proyecto esta bajo la Licencia **GNU General Public License v3.0 (GPLv3)**.
Copyright (C) 2026 **kaber420** (<https://github.com/kaber420/cbdos>).

Consulta el archivo [`LICENSE`](LICENSE) para obtener los terminos y condiciones completos, o visita <https://www.gnu.org/licenses/gpl-3.0.html>.
