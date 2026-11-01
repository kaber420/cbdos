# Plan Formal y Especificación Técnica: Liberación de SRAM Interna (Stack BLE), Telemetría Real en HeaderBar y Coexistencia Wi-Fi + Bluetooth en NetworkManager

**Documento:** `specs/architecture/plan_liberacion_sram_ble_y_coexistencia_network_manager.md`  
**Estado:** Implementado y Verificado Multi-Target (ESP32-S3 PlatformIO + ESP32-P4 ESP-IDF)  
**Fecha:** 31 de Octubre de 2026  
**Relacionado con:** `specs/architecture/especificacion_ciclo_vida_apps_longpress_y_telemetria_headerbar.md`

---

## 1. Diagnóstico Técnico Riguroso (Auditoría de Código Fuente)

Tras las pruebas en hardware real reportadas por el usuario, se auditaron los subsistemas de radio, memoria y UI, identificando tres causas raíz exactas:

### 1.1. ¿Por qué el icono de Bluetooth permanecía siempre encendido?
En `core/src/ui/components/HeaderBar.cpp` (`HeaderBar::updateBluetoothStatus()`), se programó la condición:
```cpp
} else if (cbdos::security::BleShieldService::getInstance().isScanning() ||
           cbdos::security::BleShieldService::getInstance().isIdsEnabled()) {
```
Al auditar `core/include/cbdos/ble_types.hpp` (línea 103):
```cpp
struct BleSpamIdsConfig {
    bool enabled = true; // Interruptor maestro del filtro lógico IDS
```
`m_idsConfig.enabled` es un flag de configuración booleano que nace en `true` desde el arranque del sistema para indicar qué reglas evaluar *cuando* el escáner esté activo, **no** indica que la radio Bluetooth esté encendida ni escaneando. Por ello, `isIdsEnabled()` devolvía `true` permanentemente desde el boot, manteniendo el icono encendido en azul aunque el hardware BLE estuviese completamente apagado.

### 1.2. ¿Por qué no se vacía la memoria SRAM interna al cerrar las aplicaciones BLE?
En el ESP32-S3 (320 KB de SRAM interna + 8 MB de PSRAM externa), la memoria de trabajo de LVGL vive en PSRAM (2 MB), pero el stack de radio y los contenedores estándar de C++ (`std::vector`, `std::string`) se alojan en la **SRAM interna (`MALLOC_CAP_INTERNAL`)**:

1. **Retención del Controlador y Host NimBLE (~45–60 KB de SRAM interna):**
   * Cuando `BleStrikeView` o `BleShieldView` inician una operación, `S3BleAdapter::init()` (`bsp/esp32_s3_jc3248/hal/hal_ble_s3.cpp`) invoca `NimBLEDevice::init()`. Esto arranca el controlador físico `esp_bt_controller_init()` + `esp_bt_controller_enable(ESP_BT_MODE_BLE)` y crea la tarea FreeRTOS `nimble_host` en SRAM interna.
   * En `hal_ble_s3.cpp` (líneas 244–254), el método `S3BleAdapter::shutdown()` solo detenía el escaneo/advertising pero **no ejecutaba `NimBLEDevice::deinit(true)`**.
   * Además, `BleStrikeView::onForceClose()` y `BleShieldView::onForceClose()` solo detenían la acción en curso (`stop()` / `stopScan()`), sin invocar `shutdown()` sobre el adaptador cuando ninguna otra tarea BLE quedaba activa.
   * Se verificó en el código fuente de `NimBLE-Arduino 2.5.1` (`src/NimBLEDevice.cpp` línea 1029 y `src/nimble/porting/nimble/src/nimble_port.c` línea 220) que `NimBLEDevice::deinit(true)` ejecuta de forma ordenada:
     1. `nimble_port_stop()` (detiene la tarea FreeRTOS del host).
     2. `esp_nimble_deinit()` (libera colas y buffers HCI/GAP/GATT del host).
     3. `esp_bt_controller_disable()` y `esp_bt_controller_deinit()` (desmonta el controlador BT y devuelve toda la SRAM interna al heap del sistema).
     4. Libera las instancias dinámicas `m_pScan`, `m_bleAdvertising` y `m_pServer`.

2. **Retención de Capacidad en `std::vector` de Servicios Singleton:**
   * `BleShieldService` reserva memoria en SRAM interna para `m_devices`, `m_recentSpamTimestamps` y `m_spamLog`.
   * `clearDevices()` ejecutaba `m_devices.clear()`, lo cual destruye los elementos pero **conserva el bloque de memoria asignado (`capacity()`)** en SRAM interna a menos que se realice `shrink_to_fit()` o `.swap()` con un vector vacío.
   * Lo mismo ocurre en `WifiShieldService` con `m_accessPoints` y `m_alertLog`.

### 1.3. ¿Por qué no se puede apagar Bluetooth desde el Gestor de Redes y cómo opera la coexistencia?
* El silicio ESP32-S3 dispone de un subsistema RF de 2.4 GHz con **coexistencia por hardware/software (`SW_COEXIST`)**, lo que le permite operar **Wi-Fi (STA / ESP-NOW) y Bluetooth LE simultáneamente** compartiendo la antena mediante multiplexación en el tiempo (TDM).
* Sin embargo, en `core/src/ui/views/NetworkManagerView.cpp` y `core/src/ui/components/QuickSettingsPanel.cpp`, solo se implementaron controles para:
  * **Slot 0:** Radio Wi-Fi / ESP-NOW / Long Range.
  * **Slot 1:** Mochila LoRa.
  * **Slot 2:** Módem USB.
* No existe ningún interruptor maestro ni panel de estado para **Bluetooth LE**, impidiendo al usuario apagar manualmente el stack BLE o verificar si está consumiendo SRAM interna.

---

## 2. Arquitectura de la Solución Propuesta

### Fase 1: Estados Reales del Adaptador BLE y Desinicialización Completa (`core/include/cbdos/ble.hpp` y `hal_ble_s3.cpp`)

1. **Ampliación honesta de `BleAdapterState` en `core/include/cbdos/ble.hpp`:**
   ```cpp
   enum class BleAdapterState : uint8_t {
       Unavailable = 0,  // El target no posee hardware BT (ej. ESP32-P4 sin coprocesador BLE)
       Off,              // Hardware presente, pero stack NimBLE/BT apagado (0 KB de SRAM consumida)
       Idle,             // Stack BLE inicializado en RAM, en espera (uso normal / standby)
       Scanning,         // Escaneo GAP activo en hardware (Modo Defensa / Air Recon)
       Advertising       // Emisión GAP activa en hardware (Modo Ataque / BLE Lab)
   };
   ```

2. **Implementación de liberación real de SRAM en `S3BleAdapter::shutdown()` (`bsp/esp32_s3_jc3248/hal/hal_ble_s3.cpp`):**
   * Estado inicial al arrancar el SO (`S3BleAdapter`): `m_state = BleAdapterState::Off`, `m_initialized = false`.
   * En `S3BleAdapter::shutdown()`:
     * Detener cualquier emisión (`stopAdvertising()`) o escaneo (`stopScan()`).
     * Llamar a `NimBLEDevice::deinit(true)` para destruir el host FreeRTOS, deshabilitar y desinicializar el controlador BT de Espressif y liberar todos los objetos internos (`m_pScan = nullptr`).
     * Actualizar `m_initialized = false`, `m_state = BleAdapterState::Off` y notificar a la UI mediante `cbdos::ble::notifyStateChanged(m_state)`.
     * Registrar en log la cantidad de SRAM interna libre antes y después del `shutdown()` para trazabilidad en consola serie.

---

### Fase 2: Limpieza Profunda de SRAM al Cerrar Aplicaciones (`onForceClose`)

1. **Método `BleShieldService::shutdownAndReleaseMemory()` (`core/src/security/BleShieldService.hpp/.cpp`):**
   * Detiene el escaneo (`stopScan()`).
   * Limpia el objetivo Geiger (`clearGeigerTarget()`).
   * Vacía y libera físicamente la capacidad en SRAM interna de los vectores:
     ```cpp
     std::vector<radio::BleDeviceRecord>().swap(m_devices);
     std::vector<uint32_t>().swap(m_recentSpamTimestamps);
     std::vector<std::string>().swap(m_spamLog);
     ```

2. **`BleStrikeView::onForceClose()` (`core/src/ui/views/BleStrikeView.cpp`):**
   * Ejecuta `BleTransmitter::getInstance().shutdown()` (detiene la tarea `ble_tx_task` sin restaurar escaneo).
   * Si `BleShieldService::getInstance().isScanning()` es `false`, invoca `cbdos::ble::getAdapter()->shutdown()`, descargando NimBLE de la SRAM interna.

3. **`BleShieldView::onForceClose()` (`core/src/ui/views/BleShieldView.cpp`):**
   * Ejecuta `BleShieldService::getInstance().shutdownAndReleaseMemory()`.
   * Si `BleTransmitter::getInstance().isTransmitting()` es `false`, invoca `cbdos::ble::getAdapter()->shutdown()`, descargando NimBLE de la SRAM interna.

4. **`WifiShieldView::onForceClose()` (`core/src/ui/views/WifiShieldView.cpp`):**
   * Además de detener escaneo, sniffer e IDS, libera la capacidad de los vectores de APs e historial en `WifiShieldService` para devolver SRAM interna.

---

### Fase 3: Telemetría Exacta de Bluetooth en `HeaderBar` (`core/src/ui/components/HeaderBar.cpp`)

Se corrige `HeaderBar::updateBluetoothStatus()` eliminando la lectura errónea de `isIdsEnabled()` y basándose estrictamente en el estado físico del adaptador y transceptor:

| Condición Real de Hardware | Estado Visual del Icono `LV_SYMBOL_BLUETOOTH` | Significado para el Usuario |
| :--- | :--- | :--- |
| `BleTransmitter::isTransmitting()` o `state == Advertising` | 🔴 **Visible — Rojo (`#EF4444`)** | **Modo Ataque:** Emisión de paquetes BLE activa en el aire. |
| `BleShieldService::isScanning()` o `state == Scanning` | 🔵 **Visible — Azul (`#3B82F6`)** | **Modo Defensa / Escucha:** Escáner GAP activo recibiendo tramas. |
| `state == Idle` (Stack inicializado sin TX/RX) | 🟢 **Visible — Verde (`#10B981`)** | **Modo Normal / En Espera:** Bluetooth encendido en RAM, sin ataque ni escaneo. |
| `state == Off` o `state == Unavailable` | ⚪ **Oculto (`LV_OBJ_FLAG_HIDDEN`)** | **Apagado:** Transceptor BLE apagado y SRAM interna liberada. |

---

### Fase 4: Control Maestro de Bluetooth LE y Coexistencia en `NetworkManagerView` y `QuickSettingsPanel`

Dado que el ESP32 soporta coexistencia Wi-Fi + BLE simultánea, se añade el control independiente del subsistema Bluetooth:

1. **En `QuickSettingsPanel` (`core/src/ui/components/QuickSettingsPanel.cpp`):**
   * Debajo del switch **"Conexión WiFi"**, se agrega la fila **"Bluetooth LE (`LV_SYMBOL_BLUETOOTH`)"** con su propio interruptor (`lv_switch`):
     * Refleja si el stack BLE está encendido (`state != Off && state != Unavailable`).
     * Al apagarlo desde Ajustes Rápidos: detiene inmediatamente `BleTransmitter::shutdown()`, `BleShieldService::shutdownAndReleaseMemory()` y ejecuta `adapter->shutdown()`, apagando el icono de la `HeaderBar` y liberando los ~50 KB de SRAM interna al instante.
     * Al encenderlo: inicializa el adaptador en modo normal (`adapter->init("CBDos")`, estado `Idle`, icono verde).

2. **En `NetworkManagerView` (`core/src/ui/views/NetworkManagerView.hpp/.cpp`):**
   * Se añade la tarjeta dedicada **"Coexistencia Bluetooth LE (2.4 GHz)"**:
     * Muestra el estado actual en tiempo real (`Apagado - SRAM Libre`, `En Espera (Idle)`, `Escaneando (Defensa)`, `Transmitiendo (Ataque)`) y la memoria SRAM interna libre actual.
     * Incluye un interruptor maestro ON/OFF para encender o apagar por completo el stack Bluetooth LE en coexistencia con el modo Wi-Fi / ESP-NOW activo en el Slot 0.

---

## 3. Archivos a Modificar (Tras Aprobación)

1. `core/include/cbdos/ble.hpp` — Adición de `BleAdapterState::Off`.
2. `bsp/esp32_s3_jc3248/hal/hal_ble_s3.cpp` — Implementación de `NimBLEDevice::deinit(true)` en `shutdown()` y estado inicial `Off`.
3. `core/src/security/BleShieldService.hpp` y `.cpp` — Método `shutdownAndReleaseMemory()` con liberación real de `std::vector` (`swap`).
4. `core/src/security/WifiShieldService.hpp` y `.cpp` — Liberación real de vectores en cierre forzado.
5. `core/src/ui/views/BleStrikeView.cpp` y `BleShieldView.cpp` — Llamada a `adapter->shutdown()` en `onForceClose()`.
6. `core/src/ui/components/HeaderBar.cpp` — Corrección de `updateBluetoothStatus()` con los 4 estados reales (Rojo / Azul / Verde / Oculto).
7. `core/src/ui/components/QuickSettingsPanel.hpp` y `.cpp` — Interruptor rápido de Bluetooth LE.
8. `core/src/ui/views/NetworkManagerView.hpp` y `.cpp` — Tarjeta de control de Bluetooth LE en coexistencia con Wi-Fi.

---

## 4. Plan de Verificación Multi-Target

1. Compilación limpia en **ESP32-S3** (`pio run -d bsp/esp32_s3_jc3248 -e esp32s3_red`).
2. Compilación limpia en **ESP32-P4** (`idf.py -C bsp/esp32_p4_jc4880 build`).

