# 🔐 Plan de Implementación: Ventana Completa de Seguridad (`SecurityConfigView`)

**Fecha:** 2026-10-08  
**Estado:** Implementado y Validado Dual-Target (ESP32-S3 y ESP32-P4)  
**Versión de CBDos:** `v0.2.4-dev`  
**Ubicación:** `specs/architecture/plan_implementacion_security_config_view.md`  

---

## 🎯 1. Objetivo y Principios de Diseño

El subsistema de seguridad core (`LockService` y `LockPinModal`) ya se encuentra implementado y persistido en NVS (`cbdos_sec`). El propósito de este plan es diseñar e implementar la interfaz de usuario de configuración del sistema como una **ventana completa de primer nivel (`SecurityConfigView`)** derivada de `BaseView`, descartando el uso de modales flotantes para la gestión de políticas del dispositivo.

### Principios Fundamentales:
1. **Cero Modales para Ajustes de Sistema:** La configuración de políticas, estados y credenciales reside en una vista dedicada a pantalla completa con navegación estándar (`pushView` / `popView`), scroll fluido y soporte de temas (`DefaultTheme`).
2. **Rol Acotado de `LockPinModal`:** El modal de PIN se restringe exclusivamente al momento en que el usuario debe teclear físicamente los 4 dígitos del PIN (autorización de cambio o desbloqueo).
3. **Integración con el Ciclo de Vida:** Conexión con `PowerManager` para que el modo `LockPolicy::Lockscreen` se active automáticamente al despertar de reposo (`LightSleep` o apagado de pantalla).
4. **LVGL v9.5 Estricto y Multitarget:** Compatibilidad limpia para las pantallas de 480×800 (ESP32-P4) y 320×480 (ESP32-S3).

---

## 🏗️ 2. Arquitectura de la Vista (`SecurityConfigView`)

### 2.1. Definición de la Clase
- **Ubicación de Cabecera:** `core/src/ui/views/SecurityConfigView.hpp`
- **Ubicación de Implementación:** `core/src/ui/views/SecurityConfigView.cpp`

```cpp
#pragma once
#include "BaseView.hpp"
#include "cbdos/security.hpp"
#include <lvgl.h>

namespace cbdos {
namespace ui {

class SecurityConfigView : public BaseView {
public:
    SecurityConfigView();
    ~SecurityConfigView() override = default;

    bool onCreate(lv_obj_t* parent) override;
    void onResume() override;

private:
    // Contenedores y widgets dinámicos
    lv_obj_t* m_policyCards[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* m_policyRadios[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* m_pinStatusLabel = nullptr;

    // Callbacks de interacción
    static void policyCardClickedCb(lv_event_t* e);
    static void changePinClickedCb(lv_event_t* e);
    static void lockNowClickedCb(lv_event_t* e);

    // Métodos de refresco de UI
    void refreshPolicySelection();
    void refreshPinStatus();
};

} // namespace ui
} // namespace cbdos
```

---

## 🎨 3. Estructura y Componentes Visuales

La vista se organizará en tarjetas verticales (`applyRaisedCard`) dentro de `m_container`:

```
┌─────────────────────────────────────────────────────────────┐
│ [ < ]  Seguridad y Bloqueo                     [12:00] [98%]│ <-- HeaderBar nativa
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 🛡️ POLÍTICA DE SEGURIDAD DEL DISPOSITIVO                 │ │
│ │                                                         │ │
│ │ ┌─────────────────────────────────────────────────────┐ │ │
│ │ │ ( ) Desactivado                                     │ │ │
│ │ │     Acceso libre sin restricción de PIN.            │ │ │
│ │ └─────────────────────────────────────────────────────┘ │ │
│ │ ┌─────────────────────────────────────────────────────┐ │ │
│ │ │ (•) Bloqueo de Pantalla (Recomendado Cyberdeck)     │ │ │
│ │ │     Exige PIN tras arrancar o reactivar pantalla.   │ │ │
│ │ └─────────────────────────────────────────────────────┘ │ │
│ │ ┌─────────────────────────────────────────────────────┐ │ │
│ │ │ ( ) Solo Ajustes (Modo Kiosco / TableHub)           │ │ │
│ │ │     Operación abierta, exige PIN para configuración.│ │ │
│ │ └─────────────────────────────────────────────────────┘ │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 🔑 CREDENCIALES DE ACCESO                                │ │
│ │ Estado: PIN personalizado configurado                   │ │
│ │                                                         │ │
│ │ [ 🔑 Cambiar Código PIN ]                               │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ ⚡ ACCIONES INMEDIATAS                                    │ │
│ │                                                         │ │
│ │ [ 🔒 Bloquear Dispositivo Ahora ]                       │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔄 4. Flujos de Interacción y Lógica

### 4.1. Cambio de Política (`LockPolicy`)
1. Al tocar una de las tarjetas de política (`Disabled`, `Lockscreen`, `SettingsOnly`):
2. Si se intenta desactivar el bloqueo o cambiar la política existiendo un PIN activo, se solicita verificación rápida con `LockPinModal::show(...)`.
3. Tras la verificación, se llama a `LockService::getInstance().setPolicy(selectedPolicy)`.
4. Se actualizan los indicadores visuales y se muestra un Toast de confirmación.

### 4.2. Flujo Guiado de Cambio de PIN
1. El usuario pulsa **"Cambiar Código PIN"**.
2. **Paso A (Validación de Identidad):** Se abre `LockPinModal` con título *"Ingrese PIN Actual"*.
3. Si el PIN ingresado es correcto:
   - Se abre un segundo `LockPinModal` con título *"Ingrese Nuevo PIN"*.
4. **Paso B (Confirmación y Guardado):**
   - El nuevo PIN se almacena inmediatamente en NVS mediante `LockService::getInstance().forceSetPin(newPin)`.
   - Se refresca la etiqueta de estado del PIN en la vista.
   - Toast: *"PIN actualizado correctamente"*.

### 4.3. Bloqueo Inmediato ("Bloquear Ahora")
1. El usuario pulsa **"Bloquear Dispositivo Ahora"**.
2. Se invoca `LockService::getInstance().lock()`.
3. Se despliega `LockPinModal::show(nullptr, nullptr, nullptr, true)` en modo **pantalla completa (`fullScreen = true`)**.
4. La única forma de regresar al SO es introduciendo el PIN configurado.

---

## 🌐 5. Cadenas de Idioma Necesarias (`language.hpp` / `language.cpp`)

Se complementará el enum `StrId` en `core/include/cbdos/language.hpp` para soportar la ventana completa:

| Identificador `StrId` | Valor | Español (`lang_es`) | Inglés (`lang_en`) |
| :--- | :---: | :--- | :--- |
| `STR_SEC_POLICY_DISABLED` | `0x00DA` | "Desactivado" | "Disabled" |
| `STR_SEC_POLICY_DISABLED_SUB`| `0x00DB`| "Acceso libre sin restriccion de PIN" | "Open access without PIN restrictions" |
| `STR_SEC_POLICY_LOCKSCREEN` | `0x00DC` | "Bloqueo de Pantalla" | "Screen Lock" |
| `STR_SEC_POLICY_LOCKSCREEN_SUB`|`0x00DD`| "Exige PIN tras arrancar o reactivar" | "Requires PIN on startup or wake" |
| `STR_SEC_POLICY_SETTINGS` | `0x00DE` | "Solo Ajustes (Kiosco)" | "Settings Only (Kiosk)" |
| `STR_SEC_POLICY_SETTINGS_SUB`| `0x00DF` | "Exige PIN para entrar a configuracion"| "Requires PIN to enter settings" |
| `STR_SEC_CREDENTIALS_TITLE` | `0x00E0` | "Credenciales de Acceso" | "Access Credentials" |
| `STR_SEC_STATUS_DEFAULT_PIN`| `0x00E1` | "PIN por defecto activo (1234)" | "Default PIN active (1234)" |
| `STR_SEC_STATUS_CUSTOM_PIN` | `0x00E2` | "PIN personalizado configurado" | "Custom PIN configured" |
| `STR_SEC_BTN_LOCK_NOW` | `0x00E3` | "Bloquear Dispositivo Ahora" | "Lock Device Now" |
| `STR_SEC_TOAST_PIN_SAVED` | `0x00E4` | "PIN guardado correctamente" | "PIN successfully saved" |
| `STR_SEC_TOAST_POL_SAVED` | `0x00E5` | "Politica de seguridad actualizada"| "Security policy updated" |

---

## 🔌 6. Integración en `ConfigView`

En `core/src/ui/views/ConfigView.cpp`:
1. Agregar el `#include "SecurityConfigView.hpp"`.
2. Agregar una tarjeta en la lista de opciones:
   - Título: `cbdos::lang::tr(StrId::STR_SEC_POLICY_TITLE)` ("Seguridad y Bloqueo").
   - Subtítulo dinámico: Política actual (`Disabled`, `Lockscreen`, `SettingsOnly`).
   - Icono derecho: `LV_SYMBOL_SETTINGS` o `LV_SYMBOL_RIGHT`.
   - Identificador `id = 11`.
3. Manejador del click (`btn_event_cb`):
   ```cpp
   } else if (id == 11) {
       UIManager::getInstance().pushView(std::make_shared<SecurityConfigView>());
   }
   ```

---

## 📋 7. Lista de Pasos de Ejecución para la Próxima Sesión

- [x] **Paso 1: Cadenas de Idioma:**
  - Agregar `STR_SEC_POLICY_*` y `STR_SEC_CREDENTIALS_*` en `language.hpp` y `language.cpp`.
- [x] **Paso 2: Implementación de la Vista:**
  - Crear `core/src/ui/views/SecurityConfigView.hpp`.
  - Crear `core/src/ui/views/SecurityConfigView.cpp` con las tarjetas de política, cambio de PIN y bloqueo inmediato.
- [x] **Paso 3: Enlace en Configuración del Sistema:**
  - Conectar navegación en `core/src/ui/views/ConfigView.cpp`.
- [x] **Paso 4: Registro en Build System:**
  - Añadir `src/ui/views/SecurityConfigView.cpp` a `core/CMakeLists.txt` y `core/library.json`.
- [x] **Paso 5: Integración con Suspensión:**
  - Enlazar llamada de `LockService::getInstance().lock()` en eventos de reactivación de pantalla en `PowerManager`.
- [x] **Paso 6: Compilación y Validación Dual-Target:**
  - Compilar en ESP32-S3 (PlatformIO): `pio run -d bsp/esp32_s3_jc3248`.
  - Compilar en ESP32-P4 (ESP-IDF 5.5): `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`.
