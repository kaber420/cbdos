# Especificación Técnica: GPIO Resource Manager (GRM) para CBDos

**Fecha:** 2026-09-03  
**Versión:** 1.0.0  
**Estado:** Especificación Técnica / Diseño Arquitectónico  
**Autor:** Equipo CBDos  
**Target:** ESP32-P4 / ESP32-S3 (FreeRTOS, Bare-Metal)

---

## 1. Resumen Ejecutivo

Este documento especifica la necesidad, diseño y justificación técnica para implementar un **GPIO Resource Manager (GRM)** en el núcleo de CBDos. El GRM es un subsistema de gestión de propiedad y concurrencia de pines GPIO que resuelve el problema crítico de **colisiones de hardware** entre subsistemas internos, mochilas modulares (BackpackManager) y aplicaciones Lua de usuario en un entorno bare-metal sin sistema operativo de archivos ni daemons externos.

---

## 2. Contexto y Estado Actual

### 2.1 Arquitectura GPIO Existente

CBDos implementa una HAL de GPIO basada en **polimorfismo C++ puro** (`IGpioBackend`) con backends por placa:

| Placa | Backend | Ubicación |
|-------|---------|-----------|
| ESP32-S3 (JC3248) | `S3GpioBackend` | `bsp/esp32_s3_jc3248/hal/hal_uart_s3.cpp:267` |
| ESP32-P4 (JC4880) | `P4GpioBackend` | `bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp:361` |

**API pública (`core/include/cbdos/gpio.hpp`):**
```cpp
namespace cbdos::gpio {
    bool setPinMode(int pin, PinMode mode);
    bool digitalWrite(int pin, PinLevel level);
    PinLevel digitalRead(int pin);
    bool isPinAvailable(int pin);  // Solo valida rangos y pines "reservados" hardcodeados
}
```

**Lua Bindings (`core/src/lua/LuaBridge.cpp:1670`):**
```lua
cbdos.gpio.pin_mode(pin, mode)
cbdos.gpio.digital_write(pin, level)
cbdos.gpio.digital_read(pin)
-- NO EXISTE: claim, release, is_claimed, owner
```

### 2.2 Protección Actual: Lista Estática Hardcodeada

Cada backend implementa `isPinAvailable()` con listas estáticas de pines "reservados":

```cpp
// S3GpioBackend::isPinAvailable() - hal_uart_s3.cpp:301
if (pin == 45 || pin == 47 || pin == 21 || pin == 48 || pin == 40 || pin == 39 || pin == 4 || pin == 1) return false; // LCD QSPI
if (pin == 8 || pin == 3 || pin == 42 || pin == 2 || pin == 41) return false; // Touch, Audio

// P4GpioBackend::isPinAvailable() - hal_uart_p4.cpp:410
if (pin == 5 || pin == 23 || pin == 7 || pin == 8 || pin == 3 || pin == 4) return false; // LCD, I2C, Touch
if (pin == 13 || pin == 12 || pin == 10 || pin == 9 || pin == 48 || pin == 11) return false; // Audio I2S
if (pin >= 39 && pin <= 44) return false; // MicroSD SDMMC
if (pin == 18 || pin == 19 || (pin >= 14 && pin <= 17) || pin == 54) return false; // SDIO C6
```

**Limitaciones críticas:**
- ❌ **Estático**: No admite reconfiguración en caliente
- ❌ **Binario**: Solo "disponible/no disponible", sin noción de *propietario*
- ❌ **Sin concurrencia**: Múltiples consumidores pueden escribir el mismo pin simultáneamente
- ❌ **Sin trazabilidad**: Imposible diagnosticar "quién usa el pin 33"
- ❌ **Acoplamiento fuerte**: BackpackManager no puede negociar pines dinámicamente

---

## 3. Análisis del Problema

### 3.1 Escenarios de Colisión Reales

| Escenario | Consumidor A | Consumidor B | Consecuencia |
|-----------|--------------|--------------|--------------|
| **Mochila LoRa + Display** | BackpackManager (SPI CS=33) | `hal_display` (pin 33 como backlight PWM) | Parpadeo display / LoRa falla |
| **App Lua + Radio interna** | `sensor_monitor.lua` (pin 21 ADC) | `hal_radio` (pin 21 como IRQ) | Lecturas basura / Radio sorda |
| **Hot-swap mochilas** | Mochila A (pins 33,35,36,37) | Mochila B (pins 33,34,35,36) | Cortocircuito lógico al cambiar |
| **Dual-radio USB + Backpack** | 3x ESP32-C3 CDC (GPIOs virtuales) | Mochila SPI (GPIOs físicos JP1) | Contención bus SPI / I2C |

### 3.2 Causa Raíz

> **Ausencia de un Authority Centralizado de Propiedad de Pines (Pin Ownership Authority)**

En Linux, `gpiod` / `libgpiod` / `pigpiod` proveen esto via daemon userspace. En bare-metal FreeRTOS, **debe residir en el kernel/core** como singleton thread-safe.

---

## 4. Justificación Técnica

### 4.1 Por qué NO un Daemon Externo (gpiod/pigpiod)

| Factor | Daemon Linux | GRM en Core CBDos |
|--------|--------------|-------------------|
| **Latencia** | IPC (socket/unix) ~10-100µs | Llamada directa C++ ~50ns |
| **Memoria** | Proceso separado + libgpiod | ~2KB RAM (tabla 64 pins) |
| **Determinismo** | No (scheduler Linux) | Sí (FreeRTOS critical section) |
| **Boot time** | Requiere init system | Disponible tras `initGpioBackend()` |
| **Permisos** | udev rules, root | Capabilities CBDos (Lua sandbox) |
| **Hot-swap** | udev rules complejas | Callback síncrono inmediato |
| **Integración Lua** | FFI / subprocess | API nativa `cbdos.gpio.*` |

**Conclusión:** En bare-metal, un daemon añade latencia, complejidad y puntos de fallo sin beneficio. El GRM **debe ser parte del core**.

### 4.2 Por qué NO Ampliar `isPinAvailable()`

`isPinAvailable()` es una **consulta de hardware estático** (¿existe el pin? ¿está conectado a periférico fijo?).  
El GRM gestiona **estado dinámico de propiedad** (¿quién lo usa ahora? ¿es exclusivo? ¿cuántas referencias?).

Mezclar ambos viola **Single Responsibility Principle** y rompe la abstracción HAL.

### 4.3 Requisitos No Funcionales

| Requisito | Especificación |
|-----------|----------------|
| **Thread-safety** | FreeRTOS mutex (configUSE_MUTEXES=1) |
| **Determinismo** | O(1) claim/release, sin allocación dinámica |
| **Memoria** | < 4KB RAM (tabla fija 64 entradas) |
| **Hot-swap** | Callbacks síncronos < 100µs |
| **Auditoría** | `cbdos.gpio.dump_state()` → JSON/CSV |
| **Sandbox Lua** | `claim()` verifica capabilities de la app |

---

## 5. Diseño del GPIO Resource Manager (GRM)

### 5.1 Estructura de Datos

```cpp
// core/include/cbdos/gpio_manager.hpp

namespace cbdos::gpio {

enum class PinClaimMode {
    Exclusive,      // Solo un owner (default: SPI CS, IRQ, PWM)
    SharedReadOnly, // Múltiples lectores (ADC, GPIO input)
    SharedReadWrite // Múltiples writers (requiere coordinación externa)
};

struct PinClaim {
    int pin = -1;
    std::string owner_id;           // "system:display", "backpack:lora_sx1262", "app:sensor_monitor"
    PinClaimMode mode = PinClaimMode::Exclusive;
    uint32_t ref_count = 0;         // Para shared mode
    uint32_t claimed_at_ticks = 0;  // xTaskGetTickCount() para debugging
    bool is_system = false;         // true = no liberable por apps Lua
};

class GpioResourceManager {
public:
    using ClaimCallback = std::function<void(int pin, const PinClaim& claim)>;
    using ReleaseCallback = std::function<void(int pin, const std::string& owner)>;

    // ────────────────────────────────────────────────────────────
    // Core API (C++ Core, BackpackManager, Subsistemas)
    // ────────────────────────────────────────────────────────────
    bool claimPin(int pin, const std::string& owner, PinClaimMode mode = PinClaimMode::Exclusive, bool is_system = false);
    bool releasePin(int pin, const std::string& owner);
    bool forceReleasePin(int pin); // Solo para system/backpack hot-detach

    // ────────────────────────────────────────────────────────────
    // Query API
    // ────────────────────────────────────────────────────────────
    bool isPinClaimed(int pin) const;
    bool isPinFree(int pin) const;           // !claimed && isPinAvailable(pin)
    std::optional<PinClaim> getPinClaim(int pin) const;
    std::vector<PinClaim> getAllClaims() const;
    std::vector<PinClaim> getClaimsByOwner(const std::string& owner) const;

    // ────────────────────────────────────────────────────────────
    // Observabilidad / Debug
    // ────────────────────────────────────────────────────────────
    std::string dumpStateJson() const;
    void onClaimed(ClaimCallback cb);
    void onReleased(ReleaseCallback cb);

    // ────────────────────────────────────────────────────────────
    // Integración con HAL Backend
    // ────────────────────────────────────────────────────────────
    bool configurePin(int pin, PinMode mode, const std::string& owner); // claim + setPinMode atómico
    
    static GpioResourceManager& instance();
};
```

### 5.2 Reglas de Propiedad

| Regla | Descripción |
|-------|-------------|
| **Exclusividad por defecto** | `claim(pin, owner)` → `Exclusive`. Falla si ya claimed. |
| **Sistema > Usuario** | `is_system=true` pins no liberables por Lua apps. |
| **Referencias compartidas** | `SharedReadOnly`: múltiples `claim` OK, `release` decrementa ref_count. |
| **Backpack hot-swap** | `forceReleasePin()` en `onDetach` + callback a app Lua `on_backpack_detached()`. |
| **Liberación automática** | Al `lua_close()` del estado de la app, liberar todos sus pins (atexit hook). |

### 5.3 API Lua Expandida

```lua
-- cbdos.gpio (tabla extendida)
cbdos.gpio.claim(pin, owner_id, mode)     -- mode: "exclusive" | "shared_ro" | "shared_rw"
cbdos.gpio.release(pin, owner_id)
cbdos.gpio.is_claimed(pin)                -- bool
cbdos.gpio.get_owner(pin)                 -- string | nil
cbdos.gpio.get_claims()                   -- array de {pin, owner, mode, ref_count}
cbdos.gpio.get_my_claims()                -- claims del owner_id actual (sandbox)
cbdos.gpio.dump()                         -- string JSON para debug

-- Constantes
cbdos.gpio.CLAIM_EXCLUSIVE
cbdos.gpio.CLAIM_SHARED_RO
cbdos.gpio.CLAIM_SHARED_RW
```

**Sandboxing:** El `owner_id` se inyecta automáticamente desde el contexto de la app (`app:<app_name>`), no se pasa por parámetro.

---

## 6. Integración con Subsistemas Existentes

### 6.1 BackpackManager (Prioridad Alta)

```cpp
// En BackpackManager::onAttach(descriptor)
bool attachBackpack(const BackpackDescriptor& desc) {
    // 1. Validar pines solicitados vs JP1 permitidos
    for (auto [signal, pin] : desc.pins) {
        if (!gpio::isPinAvailable(pin)) return false; // HW estático
        if (!grm.claimPin(pin, "backpack:" + desc.bid, PinClaimMode::Exclusive, true)) {
            rollbackClaims(desc); return false;
        }
    }
    // 2. Configurar modo (SPI/I2C/UART) atómicamente
    for (auto [signal, pin] : desc.pins) {
        grm.configurePin(pin, modeForSignal(signal), "backpack:" + desc.bid);
    }
    // 3. Evento sistema + launch app
    EventSystem::post(CBDOS_EVENT_BACKPACK_ATTACHED, desc);
    return true;
}

void detachBackpack(const std::string& bid) {
    auto claims = grm.getClaimsByOwner("backpack:" + bid);
    for (auto& c : claims) grm.forceReleasePin(c.pin);
    EventSystem::post(CBDOS_EVENT_BACKPACK_DETACHED, bid);
}
```

### 6.2 Subsistemas Internos (Init Temprano)

```cpp
// En initDisplayBackendP4() / initAudioBackendP4() / initRadioBackendP4()
void initDisplayBackendP4() {
    grm.claimPin(5,  "system:display:rst",   PinClaimMode::Exclusive, true);
    grm.claimPin(23, "system:display:bl",    PinClaimMode::Exclusive, true);
    grm.configurePin(5,  PinMode::Output, "system:display:rst");
    grm.configurePin(23, PinMode::Output, "system:display:bl");
    // ...
}
```

### 6.3 Apps Lua (Patrón Recomendado)

```lua
-- sensor_monitor.lua
local function init()
    -- Claim al inicio, release al final (patrón RAII via gc)
    local ok, err = cbdos.gpio.claim(21, "adc_soil")  -- owner auto: "app:sensor_monitor"
    if not ok then error("Pin 21 ocupado por: " .. cbdos.gpio.get_owner(21)) end
    cbdos.gpio.pin_mode(21, cbdos.gpio.INPUT)
    
    -- Cleanup automático al cerrar app
    return function() cbdos.gpio.release(21, "adc_soil") end
end

local cleanup = init()
-- ... loop principal ...
cleanup() -- o confiar en __gc / atexit
```

---

## 7. Plan de Implementación por Fases

| Fase | Entregable | Archivos Afectados | Esfuerzo |
|------|------------|-------------------|----------|
| **1. Core GRM** | `GpioResourceManager` singleton thread-safe | `core/include/cbdos/gpio_manager.hpp`, `core/src/system/gpio_manager.cpp` | 2 días |
| **2. Integración HAL** | `configurePin()`, `claimPin()` atómico en backends | `bsp/*/hal/hal_uart_*.cpp` (S3GpioBackend, P4GpioBackend) | 1 día |
| **3. Lua API** | Bindings `claim/release/is_claimed/get_owner/dump` | `core/src/lua/LuaBridge.cpp` (registerGpioAPI) | 1 día |
| **4. BackpackManager** | Claim/forceRelease en attach/detach | `core/src/backpack/BackpackManager.cpp` (nuevo) | 2 días |
| **5. Subsistemas** | Registrar claims en init (display, audio, radio, storage) | `bsp/*/hal/hal_*_p4.cpp`, `hal_*_s3.cpp` | 1 día |
| **6. Tests & Docs** | Unit tests (Unity), docs de migración | `tests/gpio_manager_test.cpp`, `docs/api/gpio_manager.md` | 1 día |

**Total estimado: ~8 días-hombre**

---

## 8. Análisis de Riesgos y Mitigación

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| **Deadlock mutex GRM** | Baja | Crítico | `configASSERT(!in_isr())`; timeouts en `claim()`; no locks anidados |
| **Fragmentación string owner_id** | Media | Medio | `owner_id` como `const char*` + pool estático 32 entries |
| **Apps maliciosas (claim spam)** | Media | Alto | Rate-limit por app (max 16 pins/app); sandbox capabilities |
| **Backpack hot-swap race** | Media | Alto | `forceReleasePin()` atómico + disable IRQs durante rollback |
| **Migración código legacy** | Alta | Medio | Wrapper `legacy_setPinMode()` que auto-claim con owner "legacy" |

---

## 9. Métricas de Éxito

| Métrica | Target |
|---------|--------|
| **Cero colisiones GPIO** en stress test (10 apps + 3 mochilas + 4 subsistemas) | 100% |
| **Latencia claim/release** | < 5µs (FreeRTOS mutex) |
| **Memoria GRM** | < 4KB RAM |
| **Tiempo hot-swap backpack** | < 50ms (claim rollback + app launch) |
| **Cobertura tests** | > 90% (claim, release, shared, force, sandbox) |

---

## 10. Conclusión

El **GPIO Resource Manager (GRM)** no es un "daemon" ni una capa opcional: es **infraestructura crítica** para que CBDos cumpla su promesa de **plataforma modular Plug & Play** (BackpackManager, multi-radio, apps Lua sandboxeadas) en bare-metal FreeRTOS.

La alternativa (statu quo) garantiza fallos silenciosos, debugging imposible y arquitectura frágil que no escala. El GRM propuesto añade ~2KB RAM, O(1) determinismo, y desbloquea todo el roadmap de hardware dinámico.

---

## Apéndice A: Referencias Cruzadas

- `core/include/cbdos/gpio.hpp` - HAL Interface actual
- `bsp/esp32_s3_jc3248/hal/hal_uart_s3.cpp:267` - S3GpioBackend
- `bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp:361` - P4GpioBackend
- `core/src/lua/LuaBridge.cpp:1670` - Lua API actual
- `docs/architecture/backpack_manager_and_dynamic_gpio_nfc_spec.md` - BackpackManager spec
- `docs/architecture/modular_lua_bridge_architecture.md` - Lua sandbox model

---

*Fin del documento*