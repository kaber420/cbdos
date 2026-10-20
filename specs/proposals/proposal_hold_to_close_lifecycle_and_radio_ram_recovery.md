# Propuesta Técnica: Ciclo de Vida Dual (Multitarea vs Cierre Total "Hold-to-Close") y Liberación Dinámica de RAM

**Fecha:** 2026-10-19  
**Estado:** Propuesta Formal (En Revisión por el Usuario)  
**Categoría:** Arquitectura Core / UX / Gestión de Memoria  
**Target:** ESP32-S3 (JC3248W535) y ESP32-P4 (JC4880)

---

## 1. Motivación y Problemática

En un sistema operativo embebido como **CBDos**, herramientas especializadas como **BLE Lab** o **Wi-Fi Suite** demandan cantidades significativas de memoria SRAM interna para controladores de hardware y tareas de fondo (~60 KB a 80 KB para la pila Bluetooth/NimBLE).

Actualmente:
1. Al salir de la vista con el botón estándar de volver (`<`), la aplicación se oculta/destruye parcialmente en UI, pero la radio NimBLE **permanece permanentemente activa en memoria** (`NimBLEDevice::deinit()` no se invocaba).
2. La memoria libre pasa de **131 KB** a solo **16 KB**, y no existía un mecanismo para que el usuario decidiera si deseaba **mantener la herramienta en segundo plano (multitarea)** o **cerrarla definitivamente para recuperar los 131 KB libres**.

El usuario propone un modelo ergonómico inspirado en el patrón de seguridad del botón *"Limpiar NVS"* de CBDos:
* **Toque corto:** Volver / Minimizar al Dashboard (la herramienta puede continuar trabajando en segundo plano si la memoria lo permite).
* **Mantener presionado (Hold-to-Close con cuenta regresiva):** Inicia una cuenta regresiva visual (1.5 - 2.0s). Si se mantiene hasta el final, ejecuta un **cierre total**, destruye las tareas, apaga el transceptor de radio (`deinit`) y devuelve el 100% de la SRAM al sistema operativo.

---

## 2. Experiencia de Usuario (UX)

### 2.1. Interacción en el HeaderBar

```
[ < Volver ]              BLE Lab              [Mem: 131 KB]
    │
    ├── [Toque Corto (< 500 ms)]: 
    │   └── Vuelve al menú principal.
    │       El proceso/radio BLE queda activo en fondo (multitarea).
    │
    └── [Toque Sostenido (Hold >= 500 ms)]:
        ├── Arranca timer de cuenta regresiva visual de 2.0 segundos.
        ├── El botón/etiqueta cambia visualmente:
        │   "[ Cerrando (2.0s)... ]" con progreso y color de advertencia ámbar/rojo.
        ├── Si el usuario suelta antes de tiempo:
        │   Cancela inmediatamente y regresa al estado original sin cerrar nada.
        └── Si se completa la cuenta (2.0s):
            ├── Toast de notificación: "BLE Lab cerrado (RAM liberada)".
            ├── Ejecuta shutdown() total de NimBLE y radio.
            └── Cierra la vista regresando al Dashboard con 131 KB libres.
```

### 2.2. Variante con Botón Dedicado de Cierre en HeaderBar
Como alternativa o complemento directo disponible en el `HeaderBar`:
* Lado izquierdo: `[ < Volver ]` (Toque normal para navegar al menú sin destruir el proceso).
* Lado derecho: `[ X Cerrar ]` mediante `HeaderBar::setRightAction("Cerrar", ...)`:
  * Al pulsarlo (o mantenerlo 1s), realiza el apagado total y liberación de memoria.

---

## 3. Arquitectura Técnica de Implementación

### 3.1. Nivel HAL (`bsp/esp32_s3_jc3248/hal/hal_ble_s3.cpp`)

Implementar la liberación física real en `S3BleAdapter::shutdown()`:

```cpp
void shutdown() override {
    if (!m_initialized) return;

    // 1. Detener transmisiones y escaneos activos
    stopAdvertising();
    stopScan();

    // 2. Desinicializar el Host de NimBLE y liberar objetos/tareas de FreeRTOS
    NimBLEDevice::deinit(true);

    // 3. Desactivar el controlador Bluetooth para liberar buffers de radio baseband
    esp_bt_controller_disable();

    m_initialized = false;
    m_state = BleAdapterState::Idle;
    cbdos::ble::notifyStateChanged(m_state);

    cbdos::system::log(cbdos::system::LogLevel::Info, TAG_BLE,
                       "Stack NimBLE y radio BLE desinicializados (RAM liberada)");
}
```

Al volver a entrar a la herramienta o presionar "Transmitir", `init("CBDos-Lab")` re-inicializa el controlador y NimBLE limpiamente bajo demanda.

### 3.2. Nivel Dominio Core (`BleTransmitter.cpp`)

Propagar el ciclo de apagado hacia el adaptador:

```cpp
void BleTransmitter::shutdown() {
    m_restoreScan.store(false);
    stop(); // Detiene tarea de transmisión FreeRTOS

    IBleAdapter* adapter = getAdapter();
    if (adapter) {
        adapter->shutdown(); // Apaga NimBLE y libera RAM física
    }
}
```

### 3.3. Nivel UI (`BleStrikeView.cpp` y `HeaderBar`)

Replicar el patrón probado de `ConfigView::nvs_timer_cb`:

1. Detectar eventos `LV_EVENT_PRESSED`, `LV_EVENT_RELEASED`, `LV_EVENT_PRESS_LOST` en el botón de retroceso/cierre.
2. Si es pulsación corta: invocar navegación hacia atrás normal (`UIManager::popView()`).
3. Si la pulsación supera el umbral de hold:
   * Crear un timer LVGL de 50 ms.
   * Mostrar el contador regresivo en la etiqueta (`snprintf(buf, "Cerrando %.1fs", rem)`).
   * Al expirar (1500 ms):
     * Invocar `BleTransmitter::getInstance().shutdown();`
     * Regresar a la vista anterior.

---

## 4. Beneficios del Diseño

1. **Autonomía del Usuario:** El usuario tiene control explícito sobre sus recursos: puede dejar un ataque o monitor BLE corriendo en segundo plano mientras revisa otra app, o puede destruirlo de un plumazo para liberar 80 KB de RAM interna.
2. **Prevención de Toques Accidentales:** Al requerir un toque sostenido con cuenta regresiva, nunca se apagará por error una sesión de auditoría activa.
3. **Estandarización en CBDos:** Este mismo patrón "Hold-to-Close" se puede extender a cualquier app pesada del Cyberdeck (Wi-Fi Strike, Terminal SSH, PicoTTS, Emuladores).

---

## 5. Fases de Ejecución

1. **Fase 1 (HAL & Core):** Habilitar `NimBLEDevice::deinit(true)` y `esp_bt_controller_disable()` en `hal_ble_s3.cpp` y conectarlo a `BleTransmitter::shutdown()`.
2. **Fase 2 (UI):** Integrar el mecanismo "Hold-to-Close" con cuenta regresiva en el botón de salida de `BleStrikeView`.
3. **Fase 3 (Validación en Hardware):**
   * Verificar salida normal: memoria se conserva para multitarea.
   * Verificar salida "Hold-to-Close": memoria regresa inmediatamente de 16 KB a **131 KB libres**.
   * Re-ingreso: verificar que BLE arranque nuevamente a la primera sin errores.
