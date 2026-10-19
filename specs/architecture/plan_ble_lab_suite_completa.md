# Plan de Arquitectura: Suite Completa BLE Lab (Paridad con Bruce, GhostESP y Marauder)

**Fecha:** 2026-10-18  
**Autor:** Antigravity / Senior Embedded Systems Agent  
**Objetivo:** Implementar los módulos restantes de BLE Lab en CBDos (`BlePayloadRegistry`, `BleTransmitter`, `BleStrikeView` y `hal_ble_s3`) para alcanzar paridad funcional con las herramientas de referencia de la comunidad (*Bruce Firmware*, *GhostESP* y *ESP32Marauder*), garantizando la saturación visual inmediata en laptops Linux/Ubuntu (BlueZ), Windows, Android e iOS con paquetes BLE reales y legítimos.

---

## 1. Justificación Técnica y Análisis de Brechas

Al contrastar la arquitectura actual de CBDos contra **Bruce Firmware** y **GhostESP**:

1. **Ausencia del Perfil HID (Human Interface Device):**
   - Bruce y Flipper Zero utilizan anuncios de periféricos HID estándar (`0x1812`).
   - Todos los sistemas operativos del mundo (Linux/BlueZ, macOS, Windows, Android) priorizan activamente los periféricos de interfaz humana en la ventana de Bluetooth cuando están en modo descubrible. Sin HID, los escáneres filtran o ignoran la mayoría de beacons no conectables.
2. **Ausencia del Modo "Spam All / Kitchen Sink":**
   - GhostESP (`blespam -random`) y Bruce ("Spam All") alternan en ráfaga continua los diferentes ecosistemas. Actualmente CBDos obliga a seleccionar una categoría estática por sesión.
3. **Ergonomía de Selección:**
   - La lista de presets debe actualizarse reactivamente en la UI LVGL v9.5 según la categoría elegida, permitiendo elegir un periférico específico o el ciclo automático general.

---

## 2. Especificación de Nuevas Capacidades

### 2.1 Categoría `HidPeripherals` (Human Interface Device)
- **Service UUIDs (AD Type `0x03`):** `0x1812` (Human Interface Device).
- **Appearance (AD Type `0x19`):**
  - Teclado: `0x03C1`
  - Ratón: `0x03C2`
  - Gamepad / Mando: `0x03C4`
- **Flags (AD Type `0x01`):** `0x06` (`LE General Discoverable Mode` | `BR/EDR Not Supported`).
- **Complete Local Name (AD Type `0x09`):** Nombre real del periférico (hasta 18 caracteres).
- **Presets de Dispositivos Reales (Máximo 18 caracteres de nombre):**
  1. *Random / All (Ciclo automático)*
  2. *MX Master 3S* (Mouse `0x03C2` - 12 chars)
  3. *Magic Keyboard* (Keyboard `0x03C1` - 14 chars)
  4. *DualSense Wireless* (Gamepad `0x03C4` - 18 chars)
  5. *Keychron K2 Pro* (Keyboard `0x03C1` - 15 chars)
  6. *Razer Basilisk* (Mouse `0x03C2` - 14 chars)
  7. *Xbox Elite Pad* (Gamepad `0x03C4` - 14 chars)
  8. *MX Keys S* (Keyboard `0x03C1` - 9 chars)
  9. *Apex Pro Wireless* (Keyboard `0x03C1` - 17 chars)
  10. *DeathAdder V3* (Mouse `0x03C2` - 13 chars)

### 2.2 Modo `SpamAll` (Kitchen Sink)
- En cada ciclo de emisión, `BleTransmitter::emitOnce()` rota pseudoaleatoriamente o round-robin entre todas las categorías registradas:
  1. `ApplePairing` (AirPods, Beats)
  2. `AppleAction` (Setup modales)
  3. `AppleNotYourDevice` (Not Your AirPods popups)
  4. `AndroidFastPair` (Pixel Buds, Sony WH-1000XM)
  5. `WindowsSwiftPair` (Xbox, Surface, Microsoft Audio)
  6. `SamsungBuds` (Galaxy Buds3 Pro)
  7. `SamsungWatch` (Galaxy Watch)
  8. `GenericBeacon` (Beacons universales)
  9. `HidPeripherals` (Logitech, Keychron, DualSense)
- Cada paquete se emite con una dirección MAC aleatoria estática nueva (`Static Random Address`) para evitar deduplicación en los receptores.

---

## 3. Arquitectura de Cambios por Componente

### A. Core (`core/include/cbdos/ble_payloads.hpp`)
- Extender el enum `BlePayloadCategory`:
  ```cpp
  enum class BlePayloadCategory : uint8_t {
      ApplePairing,
      AppleAction,
      AppleNotYourDevice,
      AndroidFastPair,
      WindowsSwiftPair,
      SamsungBuds,
      SamsungWatch,
      GenericBeacon,
      HidPeripherals,      // Nuevo
      SpamAll              // Nuevo
  };
  ```
- Exponer struct y lista `kHidPresets`.

### B. Core (`core/src/ble/BlePayloadRegistry.cpp`)
- Implementar `buildHidPeripheralAdv(const char* customName)`.
- En `BlePayloadRegistry::build(category, variantIndex)`:
  - Si `category == BlePayloadCategory::SpamAll`, rotar dinámicamente entre todas las categorías activas.
  - Soportar `category == BlePayloadCategory::HidPeripherals`.

### C. Core (`core/src/ble/BleTransmitter.cpp`)
- Adaptar `emitOnce()` para manejar el ciclo multi-protocolo de `SpamAll`.

### D. UI (`core/src/ui/views/BleStrikeView.cpp` y `.hpp`)
- Agregar las nuevas opciones al dropdown de categorías:
  - `"Apple Pairing\nApple Action\nApple Not Device\nAndroid FastPair\nWindows SwiftPair\nSamsung Buds\nHID Peripherals\nSpam All (Kitchen Sink)"`
- Actualizar `updateVariantDropdown(BleCategory cat)`:
  - Para `HidPeripherals`: cargar lista de periféricos HID con ciclo automático.
  - Para `SpamAll`: deshabilitar dropdown de variantes y mostrar `"Multi-Protocolo Activo"`.

---

## 4. Fases de Ejecución

1. **Fase 1: Core Payload Registry:**
   - Declarar e implementar los generadores de frames HID y el selector round-robin de `SpamAll`.
2. **Fase 2: UI LVGL v9.5 en BleStrikeView:**
   - Actualizar strings de categorías y sincronización reactiva de menús.
3. **Fase 3: Validación de Compilación Multi-Target:**
   - Compilación limpia en ESP32-S3 PlatformIO (`pio run -d bsp/esp32_s3_jc3248 -e esp32s3_red`).
4. **Fase 4: Flasheo y Auditoría de Detección:**
   - Flasheo a la placa JC3248 y verificación directa con escaneo Bluetooth en laptop Linux (BlueZ / GNOME).

---

## 5. Resolución de Driver HAL y Validación en Hardware (JC3248W535)

### Causa Raíz Detectada en `NimBLEAdvertising` de Arduino
- El wrapper de C++ `NimBLEAdvertising::start()` fallaba con `pAdvertising->start() fallo` debido a que:
  1. Asumía un modo conectible por defecto requiriendo GATT Server.
  2. Al manipular `enableScanResponse(false)` o `setAdvertisementData()`, las flags automáticas del wrapper desbordaban el límite de 31 bytes o corrompían el frame.
  3. `esp_iface_mac_addr_set()` de Espressif devolvía error de estado al llamarse con la radio encendida.

### Solución Definitiva (Patrón GhostESP) en `hal_ble_s3.cpp`
- **Driver GAP Nativo Puro:** Se descartó el wrapper `NimBLEAdvertising` a favor de las APIs nativas del Host de NimBLE:
  - `ble_gap_adv_stop()` para control de flujo limpio entre emisiones.
  - `ble_hs_id_set_rnd(rnd_addr)` con bits `0xC0` para rotación de dirección MAC aleatoria estática real sin tocar la MAC física del chip.
  - `ble_gap_adv_set_data(params.data, params.len)` inyectando exactamente el arreglo de bytes construido por el Core.
  - `ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL)` configurado en `BLE_GAP_CONN_MODE_NON` y `BLE_GAP_DISC_MODE_GEN`.
- **Resultado:**
  - Cero errores en ejecución.
  - Rotación continua de MAC operativa.
  - Detección inmediata en laptops con Linux (BlueZ), Windows y dispositivos Android.
