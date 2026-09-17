# 🐛 Registro de Errores y Bugs Conocidos (Known Bugs)

Este documento centraliza el seguimiento de errores, cuelgues (crashes), fugas de memoria (memory leaks) y comportamientos inesperados identificados en el sistema.

## 🔴 Bugs Críticos (Prioridad Alta)
*Errores que causan reinicios (panics) o bloqueos totales del sistema.*

- **[BUG-001] Reinicio en MeshCore al procesar múltiples archivos Lottie**
  - **Módulo:** `MeshCore` / `UI (Lottie)`
  - **Descripción:** El sistema sufre un reinicio (posible *panic* o *Watchdog timeout*) cuando la aplicación MeshCore intenta procesar o renderizar varios archivos de animación Lottie simultáneamente.
  - **Causa probable (Por investigar):** No está claro si el cuelgue ocurre en la capa de transporte (envío/recepción de red), si se debe a que falta el remitente, o si el motor Lottie (LVGL) se queda sin memoria (RAM/PSRAM) al decodificar múltiples animaciones a la vez.
  - **Estado:** 🕵️ Investigando.

## 🟡 Bugs Menores (Prioridad Media/Baja)
*Fallos visuales, glitches o problemas que no detienen el funcionamiento general.*

- (Añadir aquí pequeños detalles o glitches gráficos a medida que se descubran).

---
*Nota para los agentes: Al solucionar un bug, moverlo de este archivo a la sección de "Solucionado" en el `CHANGELOG.md` referenciando el código del bug.*
