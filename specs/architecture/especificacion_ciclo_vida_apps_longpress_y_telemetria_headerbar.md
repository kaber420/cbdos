# Especificación Técnica: Ciclo de Vida de Aplicaciones, Cierre Definitivo (Long-Press) y Telemetría Visual en HeaderBar

## 1. Contexto y Problemática

En versiones anteriores de CBDos, el botón "Volver" (`m_btnBack`) en la `HeaderBar` ejecutaba invariablemente `UIManager::getInstance().popView()`. Este comportamiento producía una desconexión crítica entre la capa visual (UI) y los subsistemas de hardware y red en segundo plano:
1. **Destrucción visual sin limpieza de hardware:** Al salir de una vista como `BleStrikeView`, la interfaz gráfica se destruía (`onDestroy()`), pero los servicios del transceptor (`BleTransmitter`) continuaban emitiendo paquetes de radio o ataques en segundo plano de manera inadvertida.
2. **Falta de visibilidad de hardware (Zero Feedback):** El usuario no tenía indicación en el Dashboard ni en la `HeaderBar` de que el transceptor BLE o la radio continuaban operando y consumiendo batería.
3. **Peligro en pantallas táctiles compactas:** En pantallas pequeñas (ej. placa JC3248W535 de 3.2", 320x480), agregar múltiples botones pequeños contiguos (`[‹]` y `[✕]`) generaba riesgo de pulsaciones accidentales ("fat-finger").

---

## 2. Principios de Diseño

1. **Regla Universal del Sistema Operativo (Predecible y Consistente):**
   - **Tap Corto (`< 1000 ms`):** Minimizar / Salir a segundo plano.
     - Música: Sigue sonando de fondo.
     - BLE Strike / Ataque: Sigue transmitiendo en segundo plano.
     - BLE Shield: Sigue monitoreando pasivamente.
     - Cero modales ni fricción innecesaria.
   - **Long-Press (`> 1000 ms`):** Cierre Definitivo Forzado (Kill & Cleanup).
     - Al mantener presionado el botón "Volver" por más de 1 segundo, el botón destella en color de advertencia (rojo/acento), se invoca `onForceClose()`, deteniendo de inmediato la radio o audio (`BleTransmitter::stop()`, etc.), se destruye la app y se muestra un Toast táctico: *"Cerrado por completo"*.

2. **Telemetría Honesta en la HeaderBar (Hardware Real):**
   - Se incorpora el icono de estado de Bluetooth (`LV_SYMBOL_BLUETOOTH`) en el bloque derecho de la `HeaderBar`.
   - Su color refleja fielmente la operación real del transceptor:
     - 🔴 **Rojo Táctico (`#EF4444`):** Transmisión activa / Modo Ataque (`BleTransmitter::isTransmitting() == true`). Visible en cualquier pantalla si corre en 2° plano.
     - 🔵 **Azul Táctico (`#3B82F6`):** Modo Defensa / Escucha Pasiva (`BleShieldService::isMonitoring() == true`).
     - 🟢 **Verde (`#10B981`):** Conexión / Enlace activo.
     - ⚪ **Gris Oscuro (`#475569`) o Inactivo:** Transceptor en reposo.

---

## 3. Arquitectura e Interfaces C++

### 3.1. Extensión de `BaseView`
```cpp
namespace cbdos {
namespace ui {

class BaseView {
public:
    // ... métodos existentes ...
    
    // Invocado cuando el usuario fuerza el cierre definitivo de la app (Long-Press en Volver)
    virtual void onForceClose() {
        // Implementación por defecto: llama a onDestroy()
        onDestroy();
    }
};

} // namespace ui
} // namespace cbdos
```

### 3.2. Gestión de Eventos en `HeaderBar`
`HeaderBar` gestiona un temporizador táctil (`m_backLongPressTimer`) asociado a `m_btnBack`:
- `LV_EVENT_PRESSED`: Inicia el temporizador de 1000 ms.
- `LV_EVENT_RELEASED`:
  - Si el temporizador no expiró (`!m_backLongPressed`): cancela el temporizador y despacha `m_onBackCb()` (Minimizar / Salir).
  - Si ya expiró (`m_backLongPressed`): no hace nada adicional (el cierre forzado ya fue despachado o se finaliza el gesto).
- `LV_EVENT_PRESS_LOST`: Cancela el temporizador y limpia flags.
- **Timer Callback (1000 ms cumplidos):**
  - Marca `m_backLongPressed = true`.
  - Da feedback visual (cambio de color de fondo a rojo o destello).
  - Invoca `m_onForceCloseCb()`.

### 3.3. Control en `UIManager`
```cpp
void UIManager::popView(bool forceClose = false);
```
- Si `forceClose == true`:
  - Llama a `topView->onForceClose()`.
  - Emite `UIManager::showToast("App cerrada por completo")`.
- Si `forceClose == false`:
  - Llama al ciclo estándar `topView->onHide()` y `topView->onDestroy()`, dejando los daemons de hardware corriendo si están desacoplados.

### 3.4. Actualización Reactiva de Telemetría en `HeaderBar::update()`
`HeaderBar::update()` consulta periódicamente los servicios de radio reales:
```cpp
if (m_labelBluetooth && lv_obj_is_valid(m_labelBluetooth)) {
    if (cbdos::ble::BleTransmitter::getInstance().isTransmitting()) {
        lv_obj_remove_flag(m_labelBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(m_labelBluetooth, lv_color_hex(0xEF4444), 0); // Rojo Ataque
    } else if (cbdos::security::BleShieldService::getInstance().isMonitoring()) {
        lv_obj_remove_flag(m_labelBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(m_labelBluetooth, lv_color_hex(0x3B82F6), 0); // Azul Defensa
    } else {
        lv_obj_add_flag(m_labelBluetooth, LV_OBJ_FLAG_HIDDEN);
    }
}
```

---

## 4. Matriz de Compatibilidad Multi-Target
- **ESP32-P4 (`bsp/esp32_p4_jc4880`):** Soporta `LV_SYMBOL_BLUETOOTH` de forma nativa en Montserrat 16.
- **ESP32-S3 (`bsp/esp32_s3_jc3248`):** Soporta `LV_SYMBOL_BLUETOOTH` en perfiles `esp32s3`, `esp32s3_red`, `esp32s3_cyberdeck`. Dimensiones optimizadas para 320x480.

