# 🎮 Propuesta Técnica y Arquitectura: Consolas de Fantasía (LIKO-12 & TIC-80) y Modo Cartucho de Alto Rendimiento

**Documento:** `docs/proposals/proposal_fantasy_consoles_runtimes_liko12_tic80_and_cartridge_architecture.md`  
**Versión:** 1.0.0  
**Estado:** Propuesta Técnica y de Arquitectura (Fase 7 del Roadmap)  
**Autor:** Equipo de Arquitectura de Software CBDos  
**Fecha:** Agosto 2026  

---

## 📌 1. Introducción y Visión General

El sistema operativo **CBDos** cuenta en su hardware estrella (**ESP32-P4 Dual RISC-V @ 400 MHz, 32 MB Hexal-PSRAM**) con una capacidad de procesamiento y ancho de banda de memoria sin precedentes en sistemas embebidos portátiles. 

Esta propuesta define la integración de **Runtimes de Consolas de Fantasía (LIKO-12 y TIC-80)** y el diseño del subsistema **Cartridge Engine (Modo Cartucho Dedicado)**, permitiendo ejecutar tanto juegos táctiles casuales como simuladores 3D intensivos (ej. *Voxel Space*, raycasters y juegos retro) a 60 FPS continuos aprovechando el nuevo BSP estandarizado.

---

## 🎯 2. Comparativa de Modelos de Ejecución: ¿App Integrada o Modo Cartucho?

Para responder a la necesidad de máxima potencia sin perder la integración con el sistema operativo, se evalúan dos enfoques:

```
+──────────────────────────────────────────+──────────────────────────────────────────+
|      MODELO A: APP LUA INTEGRADA         |    MODELO B: MODO CARTUCHO DEDICADO      |
|         (Modo Ventana / Casual)          |        (Full Hardware Performance)       |
+──────────────────────────────────────────+──────────────────────────────────────────+
| • Corre como una tarea estándar en LVGL. | • Pausa tareas secundarias no esenciales.|
| • Ideal para juegos táctiles simples de  | • Dedica Core 1 al render / scripting    |
|   LIKO-12 (ajedrez, cartas, puzzles).    |   y Core 0 a Audio I2S y Touch GT911.    |
| • Comparte buffers de UI de LVGL.        | • Acceso directo a Framebuffer y Canvas  |
| • Salida rápida al menú principal.       |   sin sobrecarga de widgets.             |
|                                          | • Ideal para TIC-80 3D y juegos pesados. |
+──────────────────────────────────────────+──────────────────────────────────────────+
```

### 💡 Recomendación Arquitectónica: *Arquitectura Híbrida Unificada*
El sistema utilizará un **Lanzador de Cartuchos Unificado (`CartridgeView`)**:
1. Al pulsar un archivo `.lk12`, `.tic` o `.lua` en el Explorador de Archivos, CBDos entra en **Modo Cartucho**.
2. Asigna la mitad superior de la pantalla ($480 \times 320$ o $480 \times 272$) al framebuffer del juego con escalado pixel-perfect.
3. Asigna la mitad inferior a la **UI de Gamepad Virtual Táctil** con respuesta háptica y latencia ultrabaja, o transfiere el 100% de la pantalla si el juego es táctil puro (LIKO-12).
4. El botón físico o gesto de cabecera permite pausar, guardar partida (*save state*) y regresar a CBDos.

---

## 🏗️ 3. Arquitectura del Runtime LIKO-12 en CBDos

LIKO-12 es un entorno 100% Lua diseñado con soporte táctil nativo.

```
┌─────────────────────────────────────────────────────────────┐
│                    Cartucho LIKO-12 (.lk12)                 │
│              (Almacenado en MicroSD /sdcard/games/)         │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 Lk12CartridgeParser (C++)                   │
│   • Extrae bloques: _code_, _sprites_, _map_, _sfx_         │
│   • Carga paletas de 16 colores personalizables             │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 Liko12LuaBridge (API Engine)                │
│   • GPU API: pset, pget, rect, rectb, circ, sprite, map     │
│   • Audio API: sfx, music (mapeado a sintetizador PCM)      │
│   • Touch API: touchpressed(x,y), touchmoved, touchreleased │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 CBDos Hardware Abstraction Layer            │
│   • Video: Canvas LVGL 9.5 en PSRAM (192x128 -> 480x320)    │
│   • Audio: IAudioSink (Everest ES8311 I2S @ 44.1 kHz)       │
│   • Input: GT911 Touch Controller (Mapeo de Coordenadas)    │
│   • Storage: IStorageBackend (Lectura directa de MicroSD)   │
└─────────────────────────────────────────────────────────────┘
```

### Especificaciones de LIKO-12 en CBDos:
- **Resolución nativa:** $192 \times 128$ píxeles.
- **Escalado en pantalla 4.3" (480x800):** $2.5\times$ ($480 \times 320$) centrado en la parte superior.
- **Entrada dual:** 
  - *Modo Táctil:* El juego recibe toques directos en el área de pantalla.
  - *Modo Gamepad:* D-Pad y botones virtuales en los $480 \times 480$ píxeles inferiores.

---

## 🕹️ 4. Arquitectura del Runtime TIC-80 en CBDos

Para TIC-80 (`.tic`), se utiliza el núcleo de ejecución oficial en C (`tic80core`) compilado como servicio nativo:

- **Resolución nativa:** $240 \times 136$ píxeles (16 colores).
- **Escalado exacto $2\times$:** $480 \times 272$ píxeles.
- **Soporte de Audio:** Sintetizador tracker de 4 canales estéreo con generador de ondas complejas conectado directamente a `IAudioSink`.
- **Rendimiento 3D (Voxel Space):** Aprovechamiento de la FPU RISC-V del ESP32-P4 a 400 MHz para renderizar más de 60 FPS estables.

---

## 📱 5. Diseño del Layout de Pantalla y Gamepad Virtual (480 × 800)

```
(0, 0)
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│                PANTALLA DE JUEGO ESCALADA                   │
│          TIC-80 (480x272)  /  LIKO-12 (480x320)             │
│                                                             │
├─────────────────────────────────────────────────────────────┤ (0, 320)
│ [⚙️ MENÚ / PAUSA]       [💾 SAVE]       [🔊 VOLUMEN: 80%]    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│        ( ▲ )                               ( X )            │
│                                                             │
│   ( ◄ )     ( ► )                     ( Y )     ( A )       │
│                                                             │
│        ( ▼ )                               ( B )            │
│                                                             │
│                    [SELECT]    [START]                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘ (480, 800)
```

---

## 🗺️ 6. Integración en el Roadmap General de CBDos

Se incorpora formalmente esta capacidad en la **Fase 7** del roadmap general del sistema:

1. **Fase 1 (Completada):** Agnosticismo de Plataforma y Limpieza Base.
2. **Fase 2 (En Progreso):** Storage HAL (`IStorageBackend`), Audio Sink (`IAudioSink`), UART (`IUartBackend`) y Purga `weak`.
3. **Fase 3:** Capa de Red y Sockets (`ISocketStream`).
4. **Fase 4:** Flash Partition Manager y Actualizador OTA.
5. **Fase 5:** SDK de Aplicaciones Lua de CBDos (`.luapp`).
6. **Fase 6:** Simulador Linux x86_64 para pruebas en PC.
7. **Fase 7 (Nueva):** **Gaming & Fantasy Consoles Subsystem:**
   - Implementación de `CartridgeView` y Gamepad Táctil de baja latencia.
   - Integración de Runtime **LIKO-12** (`.lk12`).
   - Integración de Runtime **TIC-80** (`.tic`).
   - Soporte para Gamepads externos USB HID y Bluetooth.
