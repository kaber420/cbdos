# 📓 Changelog de Desarrollo (CBDos)

Todos los cambios notables del proyecto, características nuevas, refactorizaciones y correcciones de errores se documentarán en este archivo de forma cronológica.

## [Unreleased / En Desarrollo] - v0.2.4

### Refactorización (En curso)
- Modularización de `RadioView` para separar la UI de la lógica de red y estado.
- Limpieza y organización de código.

### Añadido
- Nuevo directorio `specs/project_management/` para seguimiento del proyecto y refactorización.
- Soporte integral de internacionalización de interfaz (i18n) usando `cbdos::lang::tr()`, abarcando todas las aplicaciones base y de sistema (Fase 1 y Fase 2).

### Modificado
- Eliminación de código muerto (`WavPlayer.cpp` y `WavPlayer.hpp`), cuyas funciones ahora las maneja `AudioPlayer`.

### Solucionado
- ...

---
## [v0.2.3] - Hito Anterior
### Completado
- Migración a LVGL v9.5.
- Soporte Multi-Target (ESP32-P4 y ESP32-S3).
- Aplicaciones base: MediaPlayer, Radio, FileManager, SerialTerminal, Flasher.
