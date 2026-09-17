# 🛠️ Estado de Refactorización y Modularización

Este documento rastrea el progreso de la limpieza técnica del código, la traducción de variables/comentarios y la modularización de archivos gigantes en el proyecto.

## 🔄 En Progreso Actual
- **RadioView**: Refactorizando para separar lógica de UI (`RadioViewUI`), estado (`RadioViewState`) y controlador HTTP/Icecast (`RadioView`). (Ver detalles en la conversación de modularización).

## 🌍 Progreso de Traducción (Inglés/Español)
**Avance Global Estimado:** `[▓░░░░░░░░░] ~10%`
*Registro de qué módulos ya tienen variables, métodos y comentarios estandarizados según la norma del proyecto.*

- [ ] **Core UI Engine** (`UIManager`, `BaseView`, etc.)
- [ ] **Apps Multimedia** (`MusicPlayer`, `RadioView`, etc.)
- [ ] **Apps Sistema** (`FileManager`, `ConfigView`, etc.)
- [ ] **HAL / Interfaces** (`NetworkHAL`, `AudioHAL`, etc.)
- [ ] **Componentes Base** (`HeaderBar`, `ThemeEngine`, etc.)

## 📝 Pendientes de Modularización (To-Do)
*Añadir aquí las vistas o módulos del `core/` que aún tengan archivos monolíticos (ej. > 800 líneas).*

- [ ] (Añadir aquí archivos o módulos pendientes de modularizar)

## ✅ Completados Recientemente
- [x] Desacoplamiento total de `core/` y `bsp/` (Fase 1 completada).
- [x] Migración total de vistas y componentes a **LVGL v9.5**.

