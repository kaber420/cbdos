# Por Qué Existimos: Objetivo del Compiled Device Tree (CDT)

> **Fecha:** 2026-09-23  
> **Propósito:** Este documento responde una sola pregunta: **¿para qué hacemos esto?**  
> No es una especificación técnica. Es el *porqué* honesto, escrito tras auditar el código real y descartar lo que los planes anteriores prometían sin haberlo construido.

---

## 1. El problema real que queremos resolver

CBDos corre hoy en **dos placas**:

| Placa | SoC | BSP |
|---|---|---|
| Guition JC4880P443C | ESP32-P4 | `bsp/esp32_p4_jc4880` |
| Guition JC3248W535 | ESP32-S3 | `bsp/esp32_s3_jc3248` |

Cada placa es un mundo aparte. La información de hardware (pines, direcciones I2C, timings de pantalla, voltajes LDO) vive **dispersa**:

- Macros `#define` en headers de HALs
- Números hardcodeados en el `.cpp` de los drivers
- Specs Markdown que **se contradicen entre sí** (ej. Touch RST: el JSON dice GPIO 3, el doc de pinouts dice GPIO 22)
- Listas de pines "reservados" duplicadas en cada backend GPIO

**Consecuencia concreta:** añadir una tercera placa hoy obliga a copiar un BSP entero y **revisar cada archivo a mano** buscando números mágicos, sin garantía de no dejar ninguno olvidado.

---

## 2. Nuestro objetivo (el único)

> **Soportar nuevas placas con el mismo SoC cambiando solo datos de configuración, sin reescribir los drivers.**

En la práctica, para una nueva placa ESP32-P4 queremos que el trabajo sea:

1. Escribir `boards/nuevaplaca.json` (pines, LDO, expansion, qué panel usa)
2. Escribir `panels/panelx.h` (init sequence y timings del panel concreto)
3. Compilar

**Y nada más.** Ni una línea tocada en `DisplayHAL.cpp`, `AudioHAL.cpp` ni `TouchHAL.cpp`.

Ese es el criterio que decide si el proyecto tiene éxito o no.

---

## 3. Qué NO es este proyecto (lo que nos vendieron antes y no es cierto)

Para que nadie (ni agentes de IA futuros) vuelva a inflar este documento:

| Promesa antigua | Realidad |
|---|---|
| "Cargar una placa nueva sin recompilar" con `board.cdt` | **No existe.** No hay partición, no hay loader, no hay binario. Si algún día se hace, será una fase aparte con su propio plan. |
| "Hardware Firewall" con owners y RAII | **No existe.** Hoy hay una whitelist estática de 12 pines en un `if`. El URM es código al que nadie llama. |
| "DTB real tipo Linux" | **No lo es.** Es un JSON → header constexpr. Útil para organizar datos, no es un device tree de runtime con overlays. |
| "Funciona igual para S3 y P4" | **Parcial.** La arquitectura core/HAL/BSP sí es multi-target; el CDT solo está implementado para P4 hasta el Paso 15 del plan. |
| Que los drivers oficiales de Espressif traigan la config de la placa | **No la traen.** Ellos dan el *motor* (protocolo MIPI, ST7701, GT911); los *datos* de nuestra PCB (voltajes LDO, timings, pines) los tenemos que poner nosotros. |

---

## 4. Qué obtendremos al terminar (definición de éxito)

### Inmediato (Fase A del plan — Nivel 1)
- Todos los pines del sistema en **un solo JSON** como fuente de verdad
- Whitelist de expansión JP1 real y verificable
- Cero macros de pines duplicadas entre `.h` y `.cpp` de los HALs
- Especs de hardware corregidos y sin contradicciones

### Multi-placa (Fase B del plan — Nivel 2)
- Timings de pantalla, LDO, puertos I2S/I2C y codec addr **también** en el JSON
- Init sequence del panel aislada en `panels/`
- **Prueba de aceptación:** crear un JSON ficticio de "otra placa P4", compilar, y ver que ningún driver de display/audio/touch necesita ediciones

### Recursos (Fase C del plan)
- URM funcional con owners, estados y wiring real (no el esqueleto muerto actual)
- Pines NC (1, 2) conocidos por el sistema
- S3 dentro del mismo sistema de JSON/header

---

## 5. Cómo trabaja (para agentes y humanos futuros)

1. **Este documento** define el *porqué*. No cambia salvo que cambie el objetivo de negocio.
2. **`CDT_CANONICAL.md`** define el *qué* (mapa de hardware, contrato de nombres). Debe reflejar la estructura `boards/` + `panels/`.
3. **`PLAN_MIGRACION_CDT_Y_DRIVERS.md`** define el *cómo* (pasos atómicos con criterio de build).
4. **Ningún otro spec de arquitectura de hardware es válido** si contradice estos tres. Los residuales van a `specs/history/`.

**Regla anti-humo:** si un paso del plan no se puede verificar con `idf.py build` (o `pio run`) y una observación concreta, no se acepta como completado.

---

## 6. Decisiones abiertas (deben resolverse, no omitirse)

| Decisión | Contexto | Responsable de cerrarla |
|---|---|---|
| ~~Touch RST/INT~~ | **Cerrado 2026-09-23:** esquemático → **22/21** (3,4=NC). JSON/código a corregir en Paso 1. | ✅ |
| Codec ES8311 addr | 0x18 (7-bit docs) = 0x30 (write 8-bit IDF). **Sin conflicto.** Confirmar solo si se duda al JSON Paso 7. | ✅ resuelto |
| Defaults UART: ¿32/28 (JP1) o 38/37 (consola)? | Código usa 32/28 como default, 38/37 como preset | Decisión de producto |
| `board.cdt` binario | Sin partición ni loader es trabajo huérfano | Hacer solo con caso de uso real; si no, eliminar del plan |
| Init sequence en JSON vs `panels/*.h` | Tabla binaria opaca | Recomendación: `panels/*.h` (ya reflejado en plan) |

---

## 7. Historial honesto

- **Sep 20-22:** se escribieron múltiples specs y planes (fase 0, fase 2, CDT canónico) sin compilar el BSP desde el 16 de septiembre. El renombrado de símbolos del generador rompió `DisplayHAL` y `hal_uart_p4` sin que nadie lo detectara.
- **Sep 23:** auditoría del código real, reparación del build (Paso 0), revisión crítica del plan y reescritura de este documento y del plan de migración con el objetivo multi-placa explícito.

**Lección:** un plan sin verificación de build y sin contraste con el código es papel. Este documento existe para que eso no se repita.
