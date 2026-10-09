# 🔐 Especificación Técnica: Subsistema Core de Bloqueo (`LockService`), `LockPinModal` e i18n Desacoplada

**Fecha:** 2026-10-08  
**Estado:** Propuesta Formal para Aprobación  
**Versión:** `v0.2.4-dev`  
**Ubicación:** `specs/architecture/plan_arquitectura_lock_service_y_i18n_desacoplada.md`

---

## 🎯 1. Propósito y Objetivos de Diseño

El presente documento define la arquitectura para dos componentes fundamentales y relacionados de **CBDos**:
1. **Subsistema Core de Seguridad y Bloqueo (`LockService` y `LockPinModal`):**
   - Eliminar el componente mockup no funcional `StaffPinModal`.
   - Proveer un servicio nativo en el sistema operativo para gestionar estados de bloqueo y autenticación por código PIN (4 a 6 dígitos).
   - Servir de manera agnóstica a diferentes perfiles:
     - **Perfil Cyberdeck:** Bloqueo de pantalla completa (lockscreen tras reposo/inactividad) y protección de operaciones críticas (formateo NVS/Flash, ajustes).
     - **Perfil TableHub (Kiosco):** Bloqueo estricto del acceso a la configuración del sistema y salida al Dashboard, permitiendo que la carta digital o cocina operen sin supervisión del cliente.
2. **Arquitectura de Idioma Desacoplada (i18n):**
   - El Core (`language.hpp` y `language.cpp`) conserva **únicamente** vocabulario universal del SO (incluyendo el nuevo subsistema de seguridad y bloqueo).
   - Los módulos de producto especializados (como `TableHub`) encapsulan sus textos de negocio en su propio diccionario (`TableHubLanguage.hpp`), consultando únicamente el idioma activo del sistema (`cbdos::lang::getLanguage()`).

---

## 🏗️ 2. Arquitectura del Subsistema de Seguridad Core

### 2.1. Lógica y Persistencia: `cbdos::security::LockService`
Ubicación: `core/include/cbdos/security.hpp` y `core/src/security/LockService.cpp`

```
┌─────────────────────────────────────────────────────────────┐
│                 cbdos::security::LockService                │
├─────────────────────────────────────────────────────────────┤
│ - Estado: isLocked() -> bool                                │
│ - Política: LockPolicy { Disabled, Lockscreen, ConfigOnly } │
│ - Métodos:                                                  │
│   * bool verifyPin(const char* pin)                         │
│   * bool setPin(const char* oldPin, const char* newPin)     │
│   * void lock()                                             │
│   * void unlock()                                           │
│   * LockPolicy getPolicy()                                  │
│   * void setPolicy(LockPolicy policy)                       │
│ - Persistencia NVS:                                         │
│   * Namespace: "cbdos_sec"                                  │
│   * Claves: "pin" (hash/salt), "policy" (uint8_t)           │
└─────────────────────────────────────────────────────────────┘
```

#### Políticas de Bloqueo (`LockPolicy`):
- `Disabled (0)`: Sin bloqueo. Acceso directo a todas las vistas.
- `Lockscreen (1)`: Bloqueo completo del dispositivo. Se requiere PIN tras arranque o suspensión para ver cualquier pantalla.
- `ConfigOnly (2)`: Kiosco / Operación abierta. Las vistas de trabajo (Dashboard o TableHub) están abiertas, pero acceder a `ConfigView` o cambiar ajustes exige ingresar el PIN.

---

### 2.2. Componente Visual Universal: `LockPinModal`
Ubicación: `core/src/ui/modals/LockPinModal.hpp` y `core/src/ui/modals/LockPinModal.cpp`  
(Reemplaza definitivamente a `StaffPinModal`).

- **Características:**
  - Componente modal en capa superior (`lv_layer_top()`).
  - Capacidad de operar en **Modo Modal Flotante** (tarjeta centrada) o **Modo Pantalla Completa (Lockscreen)** según contexto.
  - Indicadores visuales de dígitos ingresados (dots reactivos según el tema activo `DefaultTheme`).
  - Teclado numérico adaptable (`lv_btnmatrix`) optimizado para pantallas táctiles de 480×800 y 320×480.
  - No contiene PIN quemado en código (`hardcoded`); delega la validación a `LockService::verifyPin()`.
  - Soporte de callback: `show(const char* customTitle, SuccessCallback onSuccess, CancelCallback onCancel)`.

---

## 🌐 3. Diccionario i18n del Sistema (`language.hpp` y `language.cpp`)

Como la seguridad y el PIN son funciones del sistema operativo, sus cadenas forman parte de `language.hpp` (`StrId`) y `language.cpp`:

| Clave `StrId` | Texto Español (`lang_es`) | Texto Inglés (`lang_en`) |
| :--- | :--- | :--- |
| `STR_SEC_ENTER_PIN` | "Ingresar PIN" | "Enter PIN" |
| `STR_SEC_PIN_INCORRECT` | "PIN Incorrecto" | "Incorrect PIN" |
| `STR_SEC_ACCESS_GRANTED` | "Acceso Concedido" | "Access Granted" |
| `STR_SEC_LOCKED` | "Dispositivo Bloqueado" | "Device Locked" |
| `STR_SEC_UNLOCK` | "Desbloquear" | "Unlock" |
| `STR_SEC_CHANGE_PIN` | "Cambiar PIN" | "Change PIN" |
| `STR_SEC_NEW_PIN` | "Nuevo PIN (4 digitos)" | "New PIN (4 digits)" |
| `STR_SEC_CONFIRM_PIN` | "Confirmar PIN" | "Confirm PIN" |
| `STR_SEC_POLICY_TITLE` | "Seguridad y Bloqueo" | "Security & Lock" |
| `STR_SEC_POLICY_DESC` | "Proteccion de pantalla y ajustes" | "Screen and settings protection" |

---

## 🍽️ 4. Diccionario Desacoplado de TableHub (`TableHubLanguage.hpp`)

Ubicación: `core/src/ui/views/tablehub/TableHubLanguage.hpp`

Para no mezclar el negocio de restaurantes con el Core de Cyberdeck, TableHub encapsula sus textos consultando `cbdos::lang::getLanguage()`:

```cpp
namespace cbdos {
namespace tablehub {
namespace lang {

enum class Str {
    KDS_TITLE,
    KDS_STATUS_ACTIVE,
    KDS_COL_PENDING,
    KDS_COL_PREPARING,
    KDS_COL_READY,
    TABLE_TITLE,
    BTN_CALL_WAITER,
    BTN_ASK_BILL,
    TOAST_REQ_SENT,
};

const char* tr(Str id);

} // namespace lang
} // namespace tablehub
} // namespace cbdos
```

- Si el perfil compilado no incluye TableHub, este archivo no entra al binario y no gasta memoria Flash.
- Cuando TableHub requiera proteger el acceso a configuraciones, invoca a `LockPinModal::show(...)` con el servicio del sistema, sin crear modales redundantes.

---

## 📋 5. Plan de Ejecución por Fases

### Fase 1: Arquitectura de Seguridad Core
- [ ] Implementar `cbdos::security::LockService` con persistencia NVS (`cbdos_sec`).
- [ ] Implementar `LockPinModal` en `core/src/ui/modals/`.
- [ ] Eliminar / Reemplazar el mockup `StaffPinModal.cpp` y `.hpp`.
- [ ] Agregar cadenas `STR_SEC_*` a `language.hpp` y `language.cpp` (ES/EN).

### Fase 2: Diccionario Desacoplado de TableHub
- [ ] Crear `TableHubLanguage.hpp` en `core/src/ui/views/tablehub/`.
- [ ] Refactorizar `TableHubKdsView.cpp` y `TableHubTabletopView.cpp` para eliminar textos quemados y usar `TableHubLanguage`.
- [ ] Conectar la apertura de ajustes protegidos en TableHub mediante `LockPinModal`.

### Fase 3: Integración en Configuración del Sistema
- [ ] Añadir sección de Seguridad / Bloqueo en `ConfigView` para activar `LockPolicy` (Desactivado / Bloqueo Total / Solo Ajustes).
- [ ] Probar flujo de bloqueo/desbloqueo.

### Fase 4: Validación y Compilación Multi-Target
- [ ] Compilar ESP32-P4 (ESP-IDF 5.5).
- [ ] Compilar ESP32-S3 (PlatformIO).

---

## 🛡️ 6. Criterios de Aceptación
1. Cero textos quemados en las vistas de TableHub y en el modal de PIN.
2. `language.cpp` solo contiene strings del sistema operativo y seguridad global.
3. El modal de PIN es 100% funcional y reutilizable en Cyberdeck y TableHub.
4. Compilación dual limpia en ambos targets (`P4` y `S3`).
