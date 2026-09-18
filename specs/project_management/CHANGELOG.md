# 📓 Changelog de Desarrollo (CBDos)

Todos los cambios notables del proyecto, características nuevas, refactorizaciones y correcciones de errores se documentarán en este archivo de forma cronológica.

## [Unreleased / En Desarrollo] - v0.2.4

### Refactorización (En curso)
- Modularización de `RadioView` para separar la UI de la lógica de red y estado.
- Limpieza y organización de código.

### Añadido
- Nuevo directorio `specs/project_management/` para seguimiento del proyecto y refactorización.
- Soporte integral de internacionalización de interfaz (i18n) usando `cbdos::lang::tr()`, abarcando todas las aplicaciones base y de sistema (Fase 1 y Fase 2).
- Internacionalización completa de `HidView` (título, pestañas, botones de acción y ayudas).

### Modificado
- Eliminación de código muerto (`WavPlayer.cpp` y `WavPlayer.hpp`), cuyas funciones ahora las maneja `AudioPlayer`.

### Solucionado
- **CartridgeView**: Corrección de la capacidad mostrada en ranuras de cartuchos; se eliminó el condicional basado en la resolución de pantalla que forzaba 4.0 MB en ambas ranuras y se añadió detección dinámica del tamaño real de partición (`info.partitionSize`) con fallbacks nominales (4.0 MB para Slot 1 y 2.0 MB para Slot 2).
- **CartridgeManager**: Corrección en `flashFromSD()` donde la ruta resuelta (`resolvedPath` con prefijo `/sdcard`) no se preservaba para `stat()`, provocando falsos positivos de archivo inválido o vacío al flashear binarios. Se implementó `fstat(fileno(f))` con respaldo `fseek`/`ftell` sobre el flujo abierto.
- **CartridgeManager (Simulación)**: Asignación adecuada de 2.0 MB a `ESP_PARTITION_SUBTYPE_APP_OTA_2` en modo host/simulación.

---
## [v0.2.3] - Hito Anterior
### Completado
- Migración a LVGL v9.5.
- Soporte Multi-Target (ESP32-P4 y ESP32-S3).
- Aplicaciones base: MediaPlayer, Radio, FileManager, SerialTerminal, Flasher.
