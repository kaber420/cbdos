# Plan de Implementación: Motor y Lógica de BLE Shield (Basado en Bruce & Marauder)

**Documento:** `specs/architecture/plan_implementacion_ble_shield_bruce_marauder.md`  
**Estado:** Propuesta Técnica / En Aprobación  
**Fecha:** 18 de Octubre de 2026  
**Autores:** Kaber420 (Lead Architect) & Antigravity (Senior Embedded Architecture)  
**Objetivo:** Especificación técnica integral y estructuración de la lógica de detección, decodificación y centinela pasivo para **BLE Shield** en CBDos, portando los modelos de datos y firmas de reconocimiento de **Bruce** y **Marauder** hacia una arquitectura C++17 desacoplada en `core/` con visualización en **LVGL 9.5**.

---

## 1. Análisis de Modelos de Datos en Bruce y Marauder

Tanto **Bruce** como **Marauder** abordan la captura de paquetes BLE mediante el procesamiento de eventos GAP Advertising (`ADV_IND`, `ADV_NONCONN_IND`, `SCAN_RSP`). Sus capacidades se dividen en 3 subsistemas clave:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        ARQUITECTURA BLE SHIELD                         │
├──────────────────┬─────────────────────────────┬───────────────────────┤
│  1. AIR RECON    │       2. GEIGER TRACKER     │      3. SPAM IDS      │
│  (Clasificación) │      (Localizador RSSI)     │ (Centinela Anti-Spam) │
├──────────────────┼─────────────────────────────┼───────────────────────┤
│ • Apple AirTag   │ • Suavizado Exponencial     │ • AppleJuice Flood    │
│ • AirPods L/R/Bat│ • Medidor VU (0 a 100%)     │ • FastPair Spam       │
│ • Samsung Tags   │ • Modelo de Pérdida de      │ • SwiftPair Spam      │
│ • Meta Smart Gls │   Propagación (Distancia m) │ • Samsung Buds Spam   │
│ • HID (Mouse/Key)│ • Ticks audibles (ES8311)   │ • Flipper Zero Spam   │
│ • Drone Rem-ID   │   por proximidad física     │ • Alerta visual/audio │
└──────────────────┴─────────────────────────────┴───────────────────────┘
```

### 1.1 Estructuras Publicitarias GAP (AD Types)
Los paquetes de anuncio contienen registros estructurados:
* `0x01` (Flags): Descubrible general, BR/EDR soportado.
* `0x08` / `0x09` (Name): Nombre corto / completo del dispositivo.
* `0x19` (Appearance): Identificador estándar HID (`0x03C1` teclado, `0x03C2` ratón, `0x03C4` gamepad).
* `0x16` (Service Data):
  * `0xFE2C` (Google Fast Pair Service): Model ID + Salt.
  * `0xFD5A` (Samsung SmartThings Find).
  * `0xFDF7` (Meta View - Ray-Ban Stories / Gen 2).
* `0xFF` (Manufacturer Specific Data):
  * Primeros 2 bytes: **Bluetooth SIG Company ID** (Little-Endian).
  * Resto: Payload propietario.

---

## 2. Taxonomía de Firmas y Decodificadores (Modelos Bruce)

### 2.1 Ecosistema Apple (`Company ID = 0x004C`)
1. **Find My / AirTag (`Type 0x12`):**
   * Longitud: 28-30 bytes.
   * Criterio: `payload[0] == 0x12`.
   * Identifica: Balizas Apple AirTag y accesorios de la red "Find My". Estado del dispositivo (separado del dueño vs vinculado).
2. **Proximity Pairing / AirPods (`Type 0x07`):**
   * Criterio: `payload[0] == 0x07`.
   * Decodifica: Modelo de auricular (AirPods 1/2/3, Pro 1/2, Max, Beats Solo/Studio), nivel porcentual de batería independiente (auricular izquierdo, derecho y estuche) y estado de carga.
3. **Nearby Action (`Type 0x10`):**
   * Conexión rápida a Apple TV, HomePod setup, compartición Wi-Fi.

### 2.2 Ecosistema Samsung (`Company ID = 0x0075`)
1. **SmartThings Find (`Type 0x42` / `0x09` o Service `0xFD5A`):**
   * Identifica: Balizas Samsung SmartTag y SmartTag2.
2. **Galaxy Buds (`Type 0x01`):**
   * Decodifica: Modelo y batería de auriculares Galaxy Buds.

### 2.3 Privacidad: Gafas Inteligentes con Cámara
1. **Ray-Ban Meta / Luxottica:**
   * Fabricante: Meta Platforms (`0x058A`) o Luxottica Group (`0x08F5`).
   * Servicio: `0xFDF7` (Meta View).
   * Alerta táctica: Detección visual en pantalla "Gafas con Cámara Activa" para protección de privacidad.

### 2.4 Ecosistema Google Fast Pair & Microsoft Swift Pair
1. **Google Fast Pair:** Anuncios con Service Data `0xFE2C`.
2. **Microsoft Swift Pair:** Anuncios con Fabricante Microsoft (`0x0006`) y subtipo `0x03`.

---

## 3. Lógica del Centinela Anti-Spam (Spam IDS)

Tanto **Bruce** como **Marauder** implementan mitigación / detección contra herramientas de ataque BLE (AppleJuice, Flipper Zero, Evil-M5):
* **Firma de Ataque:** Ráfaga rápida de anuncios con direcciones MAC aleatorias rotativas (`Random Address`) que contienen payloads repetitivos de emparejamiento.
* **Algoritmo de Detección:**
  * Ventana deslizante temporal (1000 ms a 2000 ms).
  * Contador de paquetes por segundo ($PPS$) clasificados por firma de ataque.
  * **Umbral de Alerta:** Si $PPS > 3$, el centinela transiciona a estado `ALERTING`.
  * Registro de evento: Tipo de ataque, recuento total interceptado, tasa pico y marca de tiempo.
  * Señalización: Cambio de color de la interfaz (Azul táctico a Rojo de Alerta) + señal acústica de advertencia.

---

## 4. Lógica del Modo Localizador Geiger (RSSI Tracker)

* **Dispositivo Objetivo:** El usuario selecciona cualquier dispositivo descubierto (por ejemplo, un AirTag sospechoso).
* **Filtro y Suavizado:**
  $$\text{RSSI}_{\text{smoothed}} = \alpha \cdot \text{RSSI}_{\text{new}} + (1 - \alpha) \cdot \text{RSSI}_{\text{prev}} \quad (\alpha \approx 0.35)$$
* **Proximidad Relativa:**
  $$\text{Proximity} = \text{clamp}\left(\frac{\text{RSSI}_{\text{smoothed}} - (-100)}{-30 - (-100)} \times 100, 0, 100\right)\%$$
* **Generación de Pulsos / Ticks:**
  $$\Delta t_{\text{tick}} = 1000\text{ ms} - (\text{Proximity} \times 9.5\text{ ms})$$
  (Varía desde 1 click por segundo a -100 dBm hasta 20 clicks por segundo a -30 dBm).

---

## 5. Diseño Modular de Software en CBDos

### 5.1 Capa de Tipos e Interfaces (`core/include/cbdos/`)
* **`ble_types.hpp`:**
  * `BleDeviceCategory`: Enum descriptivo de familias de dispositivos.
  * `BleDeviceRecord`: Estructura enriquecida del dispositivo.
  * `BleSpamAlert`: Información de ataques interceptados en curso.
* **`BleCompanyDatabase.hpp` / `.cpp`:**
  * Tabla estática `constexpr` en Flash (`.rodata`) ordenada con más de 200 fabricantes Bluetooth SIG.
  * Búsqueda binaria instantánea $O(\log N)$ con `std::lower_bound` (0 bytes heap RAM).

### 5.2 Capa de Servicio (`core/src/security/BleShieldService.hpp` / `.cpp`)
* Servicio Singleton reactivo en C++ agnóstico:
  * Cola de eventos protegida.
  * Despacho hacia la UI mediante callbacks o dirty flags.
  * Desacoplamiento total de la plataforma (simulable / ejecutable tanto en S3 como en P4 con hardware real o emulado).

### 5.3 Capa de UI (`core/src/ui/views/BleShieldView.hpp` / `.cpp` en LVGL 9.5)
* **Pestaña 1 (Air Recon):**
  * Chips táctiles de filtro rápido: `[Todos]`, `[🍏 Apple]`, `[🪐 Samsung]`, `[👓 Meta]`, `[📍 Trackers]`, `[🖱️ HID]`.
  * Lista táctil con tarjetas dinámicas (Nombre, Fabricante, MAC, RSSI con mini-barra de señal, icono de categoría).
  * Botón directo en cada tarjeta: `[📍 Rastrear con Geiger]`.
* **Pestaña 2 (Geiger Tracker):**
  * Dispositivo seleccionado actualmente.
  * Arco animado / Medidor VU central en LVGL 9.5 con porcentaje de proximidad (0 a 100%).
  * Indicador de RSSI numérico en dBm y distancia aproximada estimada.
  * Conmutador táctil para activar/desactivar retroalimentación acústica (*Audible Clicks*).
* **Pestaña 3 (Spam IDS):**
  * Panel de estado grande: `🛡️ SECTOR SEGURO` (Verde) vs `🚨 ATAQUE BLE DETECTADO` (Rojo pulsante).
  * Contador de paquetes maliciosos bloqueados / analizados.
  * Lista de eventos recientes de spam con sello de tiempo (AppleJuice, FastPair, SwiftPair).
  * Botón táctil para reiniciar contadores.

---

## 6. Plan de Ejecución Quirúrgica

1. **Paso 1: Definición de Tipos y Base de Datos SIG:**
   * Crear `core/include/cbdos/ble_types.hpp`.
   * Crear `core/src/network/BleCompanyDatabase.hpp` y `core/src/network/BleCompanyDatabase.cpp`.
2. **Paso 2: Implementación de la Lógica del Servicio (`BleShieldService`):**
   * Crear `core/src/security/BleShieldService.hpp` y `core/src/security/BleShieldService.cpp`.
   * Implementar parser de tramas GAP, decodificadores de Apple/Samsung/Meta/FastPair, centinela anti-spam y seguidor Geiger.
3. **Paso 3: Construcción de la Vista Táctica (`BleShieldView`):**
   * Actualizar `core/src/ui/views/BleShieldView.hpp` y `core/src/ui/views/BleShieldView.cpp` con widgets reales de LVGL 9.5.
4. **Paso 4: Integración en Compilación:**
   * Registrar las nuevas fuentes en `core/CMakeLists.txt` y `core/library.json`.
   * Validar compilación dual (`pio run -d bsp/esp32_s3_jc3248` y ESP-IDF para P4).
