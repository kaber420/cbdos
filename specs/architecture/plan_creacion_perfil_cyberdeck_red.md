# Plan de Arquitectura e Implementación: Creación y Validación del Perfil Cyberdeck RED

**Documento:** `specs/architecture/plan_creacion_perfil_cyberdeck_red.md`  
**Estado:** Aprobado por el Usuario / En Ejecución  
**Fecha:** 18 de Octubre de 2026  
**Autores:** Kaber420 (Lead Architect & Project Director) & Antigravity (Senior Embedded Architecture)  
**Objetivo:** Establecer la especificación formal y el plan paso a paso para crear, integrar y probar el perfil de compilación **`Cyberdeck RED` (`CONFIG_CBDOS_PROFILE_RED`)** en el sistema operativo CBDos, validando su compilación limpia tanto en ESP32-S3 (PlatformIO) como en ESP32-P4 (ESP-IDF) **antes** de añadir las nuevas aplicaciones tácticas.

---

## 1. Justificación y Filosofía Técnica

### 1.1 El Principio "Cimentación Primero"
No se deben construir aplicaciones tácticas sobre un perfil que aún no existe en el sistema de compilación.  
El primer paso indispensable es:
1. Formalizar la tercera variante oficial de la familia Cyberdeck: **`Cyberdeck Red`**.
2. Garantizar que sus directivas de preprocesador y exclusiones purguen limpiamente los módulos recreativos no deseados (`PicoTTS`, `Cartridge`, `Helix Codecs`).
3. Comprobar que el binario resultante compile al 100% sin errores de linkeo ni símbolos huérfanos.
4. Medir la huella de Flash y RAM para validar el ahorro de recursos.

---

## 2. Definición del Perfil `Cyberdeck RED`

### 2.1 Identificadores y Constantes
* **Macro de Selección:** `CONFIG_CBDOS_PROFILE_RED`
* **Nombre de Producto:** `"CBDos Cyberdeck"`
* **Nombre de Perfil:** `"Cyberdeck Red"`
* **Color de Acento por Defecto:** `0xE63946` (Rojo Táctico Alerta)
* **Vista por Defecto al Arrancar:** `CBDOS_DEFAULT_VIEW = CBDOS_VIEW_DASHBOARD`

### 2.2 Arquitectura de Aplicaciones Tácticas (Matriz 2x2)
El ecosistema táctico de RF se organiza en 4 aplicaciones maestras consolidadas:
1. 🛡️📶 **`Wi-Fi Shield`** (Azul / Pasivo): Reconocimiento APs/Clientes, IDS Sentinel (Deauth/Beacon flood), Packet Sniffer (PCAP en MicroSD).
2. 🛡️ᛒ **`BLE Shield`** (Azul / Pasivo): Air Recon (Scan), Geiger Tracker (AirTags/SmartTags), BLE Spam Detector.
3. ⚔️📶 **`Wi-Fi Strike`** (Rojo / Ataque): Inyección Deauth/Disassoc, Beacon Spam (SSID flood), Evil Portal, PMKID Capture.
4. ⚔️ᛒ **`BLE Strike`** (Rojo / Ataque): AppleJuice (Popups iOS), FastPair Spam, SwiftPair Spam, Device Spoofer.

### 2.3 Matriz de Feature Flags
| Feature Flag | `Cyberdeck FULL` | `Cyberdeck LITE` | `Cyberdeck RED` (Fase 1: Baseline) |
| :--- | :---: | :---: | :---: |
| `CBDOS_FEATURE_DASHBOARD` | 1 | 1 | **1** |
| `CBDOS_FEATURE_TERMINAL` | 1 | 1 | **1** |
| `CBDOS_FEATURE_FLASHER` | 1 | 0 | **1** |
| `CBDOS_FEATURE_LAN_RECON` | 1 | 0 | **1** |
| `CBDOS_FEATURE_REST_PROBER` | 1 | 0 | **1** |
| `CBDOS_FEATURE_HID` | 1 | 1 | **1** |
| `CBDOS_FEATURE_SDR` | 1 (Opcional) | 0 | **1** |
| `CBDOS_FEATURE_LUA` | 1 | 0 | **1** |
| `CBDOS_FEATURE_CARTRIDGE` | **1** | 0 | **0 (Purga táctica)** |
| `CBDOS_FEATURE_PICOTTS` | **1** | 0 | **0 (Purga táctica: -1.2 MB Flash)** |
| `CBDOS_FEATURE_HELIX_CODECS` | **1** | 0 | **0 (Purga táctica)** |
| `CBDOS_FEATURE_KIOSK_LOCK` | 0 | 0 | **0** |
| `CBDOS_FEATURE_STAFF_PIN` | 0 | 0 | **0** |
| `CBDOS_FEATURE_TABLEHUB_MQTT` | 0 | 0 | **0** |
| `CBDOS_FEATURE_WIFI_SHIELD` | 0 | 0 | **0 (Inicia en 0 hasta crear la app)** |
| `CBDOS_FEATURE_BLE_SHIELD` | 0 | 0 | **0 (Inicia en 0 hasta crear la app)** |
| `CBDOS_FEATURE_WIFI_STRIKE` | 0 | 0 | **0 (Inicia en 0 hasta crear la app)** |
| `CBDOS_FEATURE_BLE_STRIKE` | 0 | 0 | **0 (Inicia en 0 hasta crear la app)** |

---

## 3. Modificaciones Quirúrgicas Requeridas

### 3.1 Cabecera Maestra: `core/include/cbdos_build_profile.h`
Insertar la rama condicional `#elif defined(CONFIG_CBDOS_PROFILE_RED)` dentro del bloque de la familia Cyberdeck:

```cpp
    #if defined(CONFIG_CBDOS_PROFILE_LITE)
        #define CBDOS_PROFILE_NAME          "Cyberdeck Lite"
        #define CBDOS_FEATURE_TERMINAL      1
        #define CBDOS_FEATURE_FLASHER       0
        #define CBDOS_FEATURE_LAN_RECON     0
        #define CBDOS_FEATURE_CARTRIDGE     0
        #define CBDOS_FEATURE_PICOTTS       0
        #define CBDOS_FEATURE_HELIX_CODECS  0
        #define CBDOS_FEATURE_LUA           0
        #define CBDOS_FEATURE_SDR           0
        #define CBDOS_FEATURE_REST_PROBER   0
        #define CBDOS_FEATURE_WIFI_SHIELD   0
        #define CBDOS_FEATURE_BLE_SHIELD    0
        #define CBDOS_FEATURE_WIFI_STRIKE   0
        #define CBDOS_FEATURE_BLE_STRIKE    0

    #elif defined(CONFIG_CBDOS_PROFILE_RED)
        #define CBDOS_PROFILE_NAME          "Cyberdeck Red"
        #define CBDOS_THEME_DEFAULT_ACCENT  0xE63946
        #define CBDOS_FEATURE_TERMINAL      1
        #define CBDOS_FEATURE_FLASHER       1
        #define CBDOS_FEATURE_LAN_RECON     1
        #define CBDOS_FEATURE_REST_PROBER   1
        #define CBDOS_FEATURE_CARTRIDGE     0
        #define CBDOS_FEATURE_PICOTTS       0
        #define CBDOS_FEATURE_HELIX_CODECS  0
        #define CBDOS_FEATURE_LUA           1
        #define CBDOS_FEATURE_SDR           1
        #define CBDOS_FEATURE_WIFI_SHIELD   0
        #define CBDOS_FEATURE_BLE_SHIELD    0
        #define CBDOS_FEATURE_WIFI_STRIKE   0
        #define CBDOS_FEATURE_BLE_STRIKE    0

    #else
        #ifndef CONFIG_CBDOS_PROFILE_FULL
            #define CONFIG_CBDOS_PROFILE_FULL 1
        #endif
        #define CBDOS_PROFILE_NAME          "Cyberdeck Full"
        #define CBDOS_FEATURE_TERMINAL      1
        #define CBDOS_FEATURE_FLASHER       1
        #define CBDOS_FEATURE_LAN_RECON     1
        #define CBDOS_FEATURE_REST_PROBER   1
        #define CBDOS_FEATURE_CARTRIDGE     1
        #define CBDOS_FEATURE_PICOTTS       1
        #define CBDOS_FEATURE_HELIX_CODECS  1
        #define CBDOS_FEATURE_LUA           1

        #if defined(CONFIG_CBDOS_FEATURE_SDR) || defined(CBDOS_ENABLE_SDR)
            #define CBDOS_FEATURE_SDR       1
        #else
            #define CBDOS_FEATURE_SDR       0
        #endif
        #define CBDOS_FEATURE_WIFI_SHIELD   0
        #define CBDOS_FEATURE_BLE_SHIELD    0
        #define CBDOS_FEATURE_WIFI_STRIKE   0
        #define CBDOS_FEATURE_BLE_STRIKE    0
    #endif
```

### 3.2 PlatformIO: `bsp/esp32_s3_jc3248/platformio.ini`
Añadir el entorno de compilación dedicado `[env:esp32s3_red]`:

```ini
; 4. Entorno Cyberdeck Red Team (Auditoría Táctica)
[env:esp32s3_red]
extends = env:esp32s3
build_flags =
    ${env:esp32s3.build_flags}
    -DCONFIG_CBDOS_PROFILE_RED=1
```

### 3.3 ESP-IDF: `bsp/esp32_p4_jc4880/main/Kconfig.projbuild`
Incorporar `CBDOS_PROFILE_RED` en la selección de variantes:

```kconfig
if CBDOS_PRODUCT_CYBERDECK
    choice CBDOS_CYBERDECK_PROFILE
        prompt "Cyberdeck Variant"
        default CBDOS_PROFILE_FULL

    config CBDOS_PROFILE_FULL
        bool "Full Cyberdeck Suite (All Apps, Tools, Flasher, TTS)"

    config CBDOS_PROFILE_RED
        bool "Red Team Tactical Edition (Security, RF Sentinel, Terminal, SDR)"

    config CBDOS_PROFILE_LITE
        bool "Lite Profile (Slim Footprint)"
    endchoice
```

---

## 4. Plan de Pruebas y Validación (Paso a Paso)

### Paso 1: Edición de Archivos de Configuración
* Modificar `core/include/cbdos_build_profile.h`.
* Modificar `bsp/esp32_s3_jc3248/platformio.ini`.
* Modificar `bsp/esp32_p4_jc4880/main/Kconfig.projbuild`.

### Paso 2: Prueba de Compilación en ESP32-S3
Ejecutar el comando de PlatformIO:
```bash
pio run -d bsp/esp32_s3_jc3248 -e esp32s3_red
```
* **Criterios de Éxito:**
  1. Compilación limpia (cero errores de compilación o linkeo).
  2. Exclusión verificada de `PicoTTSService` y `CartridgeView`.
  3. Reducción medible del tamaño del binario Flash respecto al target `esp32s3_cyberdeck` (debe ser $\approx 2.4$ MB frente a los $\approx 3.6$ MB del perfil Full).

### Paso 3: Prueba de Compilación en ESP32-P4
Ejecutar el comando de ESP-IDF:
```bash
. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
```
* **Criterios de Éxito:**
  1. Build limpio en ESP-IDF sin advertencias de Kconfig ni macros no resueltas.

### Paso 4: Reporte de Resultados y Aprobación para Fase de Apps
* Una vez validada la existencia y salud del perfil en ambas plataformas, se presentará el reporte métrico al usuario para comenzar el desarrollo del cuarteto táctico (Wi-Fi Shield, BLE Shield, Wi-Fi Strike, BLE Strike).
