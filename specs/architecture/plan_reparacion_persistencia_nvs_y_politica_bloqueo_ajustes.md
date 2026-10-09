# 🔐 Plan de Reparación: Persistencia NVS de Seguridad y Política de Bloqueo de Ajustes

**Fecha:** 2026-10-08  
**Estado:** ✅ Completado y Flasheado en Hardware Dual-Target (ESP32-S3 y ESP32-P4)  
**Versión de CBDos:** `v0.2.4-dev`  
**Ubicación:** `specs/architecture/plan_reparacion_persistencia_nvs_y_politica_bloqueo_ajustes.md`  

---

## 🎯 1. Análisis de Causa Raíz

Durante las pruebas físicas en el hardware ESP32-S3 se identificaron dos fallas funcionales críticas en el subsistema de seguridad:

### 1.1. Causa Raíz de la Falta de Persistencia
1. **Ausencia de `init()` en el arranque del BSP:**
   - Ni `bsp/esp32_s3_jc3248/src/main.cpp` ni `bsp/esp32_p4_jc4880/main/main.cpp` invocan `cbdos::security::LockService::getInstance().init()` durante la secuencia de inicio de NVS.
   - Consecuencia: Al arrancar o reiniciar la placa, el servicio permanece sin inicializar (`m_initialized = false`), manteniendo en memoria RAM los valores por defecto (`Disabled`, PIN `1234`), ignorando las claves guardadas en el namespace `cbdos_sec`.
2. **Detección errónea de clicks en LVGL v9 (`target` vs `current_target`):**
   - En `SecurityConfigView.cpp`, el callback `policyCardClickedCb` utilizaba `lv_event_get_target(e)`.
   - Cuando el usuario toca el texto dentro del botón de la tarjeta (título o subtítulo), LVGL devuelve el objeto `lv_label`, cuyo `user_data` es `nullptr` (`0`).
   - El código interpretaba que se había presionado la tarjeta `0` (`Disabled`) que ya estaba activa, descartando el evento sin llegar a persistir nada en NVS.
3. **Falta de Lazy-Init defensivo:**
   - Los métodos `getPolicy()`, `setPolicy()`, `hasCustomPin()` y `forceSetPin()` no garantizaban internamente la carga previa de NVS si el servicio no había sido inicializado explícitamente.

---

## 🧭 2. Recomendación Técnica: ¿Cuándo debe pedirse PIN al entrar a Configuración?

Para responder a la pregunta de diseño arquitectónico en **CBDos**:

### Recomendación por Política:
1. **Política `LockPolicy::Lockscreen` (Recomendada para Cyberdeck Personal / Campo):**
   - **Comportamiento:** Exige PIN a pantalla completa al arrancar el dispositivo o al reactivar la pantalla tras inactividad/suspensión.
   - **Ajustes:** **NO debe pedir PIN nuevamente al entrar a Configuración.** El usuario legítimo ya demostró su identidad al desbloquear el sistema. Pedir el PIN cada vez que se ajusta el brillo o se conecta a una red Wi-Fi introduce fricción innecesaria y arruina la ergonomía de uso táctico.
   - **Protección interna:** Solo se vuelve a pedir el PIN si el usuario intenta modificar o desactivar la política de seguridad dentro de `SecurityConfigView`.

2. **Política `LockPolicy::SettingsOnly` (Modo Kiosco / TableHub / Modo Préstamo):**
   - **Comportamiento:** El dispositivo arranca directamente al Dashboard o a su app de comensal/cocina sin pedir PIN. La pantalla se reactiva sin bloquear. Cualquiera puede usar las herramientas operativas abiertas (reproductor, radio, visor de notas, calculadora, terminal pública).
   - **Ajustes:** **SÍ DEBE PEDIR PIN OBLIGATORIAMENTE al intentar entrar a Configuración (`ConfigView`).**
   - **Puntos de entrada blindados:**
     - Al tocar el icono de "Configuración" en el Dashboard (`AppRegistry`).
     - Al tocar el acceso directo de Ajustes en el panel superior rápido (`QuickSettingsPanel`).

3. **Política `LockPolicy::Disabled`:**
   - Acceso completamente libre. Sin PIN en ninguna parte.

---

## 🏗️ 3. Plan de Cambios de Código

### 3.1. Robustecimiento de `LockService` (`security.hpp` y `LockService.cpp`)
- Modificar `getPolicy()`, `setPolicy()`, `hasCustomPin()` y `forceSetPin()` para asegurar lazy-init:
  ```cpp
  LockPolicy LockService::getPolicy() {
      if (!m_initialized) init();
      return m_policy;
  }
  ```
- Añadir logs informativos detallados para confirmar cada lectura y escritura exitosa en NVS (`cbdos_sec`).

### 3.2. Corrección del Callback Táctil en `SecurityConfigView.cpp`
- Reemplazar `lv_event_get_target(e)` por `lv_event_get_current_target(e)`.
- Remover explícitamente `LV_OBJ_FLAG_CLICKABLE` de los labels de título y descripción de las tarjetas de política.

### 3.3. Inicialización en el Arranque del Sistema
- En `bsp/esp32_s3_jc3248/src/main.cpp`:
  - En `setup()`: Invocar `cbdos::security::LockService::getInstance().init();` inmediatamente después de `cbdos::lang::initLanguage()`.
  - Tras `cbdos::ui::init()`: Si la política cargada de NVS es `LockPolicy::Lockscreen`, ejecutar:
    ```cpp
    if (cbdos::security::LockService::getInstance().getPolicy() == cbdos::security::LockPolicy::Lockscreen) {
        cbdos::security::LockService::getInstance().lock();
        cbdos::ui::LockPinModal::show(nullptr, nullptr, nullptr, true);
    }
    ```
- Aplicar la misma secuencia en `bsp/esp32_p4_jc4880/main/main.cpp` para mantener la paridad multi-target limpia.

### 3.4. Blindaje del Acceso a Configuración bajo `SettingsOnly`
- En `core/src/ui/AppRegistry.cpp`:
  - Al abrir "Configuración": Si la política es `SettingsOnly`, invocar `LockPinModal::show(...)` antes de hacer `pushView(std::make_shared<ConfigView>())`.
- En `core/src/ui/components/QuickSettingsPanel.cpp`:
  - Al pulsar el botón de engranaje (Ajustes): Si la política es `SettingsOnly`, solicitar PIN antes de abrir `ConfigView`.

---

## 📋 4. Lista de Pasos de Ejecución Propuestos

- [x] **Paso 1:** Blindar lazy-init y NVS en `core/include/cbdos/security.hpp` y `core/src/security/LockService.cpp`.
- [x] **Paso 2:** Corregir detección de click de tarjetas (`current_target`) en `core/src/ui/views/SecurityConfigView.cpp`.
- [x] **Paso 3:** Integrar `LockService::init()` y bloqueo de arranque en `bsp/esp32_s3_jc3248/src/main.cpp` y `bsp/esp32_p4_jc4880/main/main.cpp`.
- [x] **Paso 4:** Proteger acceso a `ConfigView` bajo `SettingsOnly` en `AppRegistry.cpp` y `QuickSettingsPanel.cpp`.
- [x] **Paso 5:** Compilación y verificación dual-target (`pio run` en S3 y `idf.py build` en P4).
- [x] **Paso 6:** Flasheo y prueba de confirmación física en ESP32-S3 y ESP32-P4.
