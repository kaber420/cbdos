# 👆 Especificación Técnica: Arquitectura de Entrada Táctil Dinámica y Negociación Multi-Touch (CBDos v0.2.1)

## 📌 1. Visión General
Este documento define el modelo de gestión de entrada táctil para **CBDos**, diseñado para operar de forma agnóstica entre diferentes microcontroladores y controladores táctiles (Goodix GT911, AXS15231B, CST816S, FT6336).

El sistema implementa un mecanismo de **Negociación Dinámica de Capacidades (Capability Negotiation)** que permite a aplicaciones avanzadas (como emuladores, DOOM o gamepads virtuales) solicitar y consumir múltiples puntos de contacto simultáneos cuando el hardware lo soporte, manteniendo un fallback elegante a modo monotáctil en pantallas que solo soporten un punto.

---

## 🏗️ 2. Capacidades de Hardware por Target

| Target / Placa | Controlador Táctil | Tipo de Bus | Puntos Físicos Máximos | Modo por Defecto |
| :--- | :--- | :--- | :--- | :--- |
| **ESP32-P4 (JC4880P443C)** | **Goodix GT911** | I2C Maestro (400 kHz) | **5 Puntos (True Multi-Touch)** | 1 Punto (LVGL UI) |
| **ESP32-S3 (JC3248W535)** | **AXS15231B / CST816S** | I2C Maestro | **1 a 2 Puntos (Gestos básicos)** | 1 Punto (LVGL UI) |
| **Simulador Linux Nativo** | Mouse SDL2 / X11 | Eventos de Ventana | **1 Punto** | 1 Punto |

---

## 🔄 3. Modelo de Negociación Dinámica (Bajo Demanda)

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   APLICACIÓN / JUEGO (DOOM / GAMEPAD)                  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ 1. requestMultiTouch(requested_points)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                      CBDOS INPUT SUBSYSTEM (core/)                     │
│   • Consulta al BSP las capacidades reales del hardware táctil.        │
│   • Si el hardware soporta >= puntos pedidos -> Habilita buffer multi. │
│   • Si el hardware es monotáctil -> Retorna puntos reales concedidos.  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ 2. Retorna granted_points (ej: 5 o 1)
                                    ▼
┌───────────────────────────────────┴────────────────────────────────────┐
│                    ADAPTACIÓN DE LA APLICACIÓN                         │
├───────────────────────────────────┬────────────────────────────────────┤
│ granted_points >= 2 (P4 / GT911)  │ granted_points == 1 (S3 / Fallback)│
├───────────────────────────────────┼────────────────────────────────────┤
│ • Controles de doble pulgar       │ • Controles monotáctiles con       │
│ • D-Pad analógico + Disparo       │   retención de estado              │
│ • Gestos de pellizco (Pinch-Zoom) │ • Botones virtuales optimizados    │
└───────────────────────────────────┴────────────────────────────────────┘
```

---

## 📜 4. Definición del Contrato HAL C++ (`core/include/cbdos/input.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace input {

static constexpr uint8_t MAX_TOUCH_POINTS = 5;

struct TouchPoint {
    uint16_t x;          // Coordenada X (0..width-1)
    uint16_t y;          // Coordenada Y (0..height-1)
    uint16_t strength;   // Presión / Área de contacto
    uint8_t  id;         // Identificador único del dedo (0..4)
    bool     is_pressed; // Estado de pulsación
};

struct MultiTouchState {
    uint8_t point_count;                      // Dedos activos en el instante actual (0 = libre)
    TouchPoint points[MAX_TOUCH_POINTS];      // Datos de cada punto de contacto
};

/**
 * @brief Interfaz abstracta para controladores táctiles (implementada en BSP)
 */
class ITouchBackend {
public:
    virtual ~ITouchBackend() = default;
    virtual bool init(uint16_t width, uint16_t height) = 0;
    virtual uint8_t getMaxPoints() const = 0;
    virtual bool setTrackingPoints(uint8_t points) = 0;
    virtual bool readRaw(MultiTouchState& outState) = 0;
};

void setTouchBackend(ITouchBackend* backend);
ITouchBackend* getTouchBackend();

/**
 * @brief Solicita formalmente al sistema la activación de modo multi-touch
 * @param requestedPoints Número de puntos deseados (ej: 2 para gamepad, 5 para piano)
 * @return uint8_t Número de puntos reales concedidos por el hardware
 */
uint8_t requestMultiTouch(uint8_t requestedPoints);

/**
 * @brief Libera el modo multitáctil y regresa al modo estándar de 1 punto para LVGL
 */
void releaseMultiTouch();

/**
 * @brief Obtiene el estado actual de todos los puntos de contacto activos
 */
bool getMultiTouchState(MultiTouchState& state);

/**
 * @brief Función de conveniencia monotáctil para LVGL y widgets convencionales
 */
bool getSingleTouch(uint16_t& x, uint16_t& y, bool& pressed);

} // namespace input
} // namespace cbdos
```

---

## 🕹️ 5. Ejemplo de Integración en Aplicaciones de Alto Rendimiento

```cpp
// Ejemplo en DOOM Launcher o Emulador de Gamepad
void GamepadView::onEnter() {
    // Solicitar 2 puntos de contacto (pulgar izquierdo + pulgar derecho)
    m_activePoints = cbdos::input::requestMultiTouch(2);
    m_isMultiTouch = (m_activePoints >= 2);
}

void GamepadView::onExit() {
    // Liberar multitouch al salir de la app para que LVGL regrese al modo estándar
    cbdos::input::releaseMultiTouch();
}

void GamepadView::updateInput() {
    cbdos::input::MultiTouchState touchState;
    if (!cbdos::input::getMultiTouchState(touchState)) return;

    if (m_isMultiTouch) {
        // Modo Dual-Touch Real
        for (uint8_t i = 0; i < touchState.point_count; i++) {
            const auto& pt = touchState.points[i];
            if (pt.x < 400) {
                processDpadInput(pt.x, pt.y);
            } else {
                processActionButtons(pt.x, pt.y);
            }
        }
    } else {
        // Fallback Monotáctil (1 solo dedo activo)
        if (touchState.point_count > 0) {
            processSingleTouchFallback(touchState.points[0].x, touchState.points[0].y);
        }
    }
}
```

---

## ⚙️ 6. Modo de Diagnóstico en la UI (LVGL 9.5)

Se definirá una vista en el menú de configuración y diagnósticos del sistema:
* **Test Táctil:** Pantalla interactiva que dibuja círculos de diferente color para cada dedo detectado con sus coordenadas `(X, Y)` y presión.
* **Auto-Reporte de Hardware:** Muestra en pantalla el chip detectado (`Goodix GT911 @ 0x5D`), bus I2C y número máximo de dedos soportados.
