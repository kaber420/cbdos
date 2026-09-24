# Plan de Migración: AudioHAL hacia Device Tree (ESP32-P4)

> **Documento de Especificación y Plan Técnico**  
> **Ubicación:** `specs/architecture/plan_migracion_audiohal_a_device_tree.md`  
> **Fecha:** 2026-09-24  
> **Fase / Paso:** Fase A (Nivel 1) — Paso 3 del Plan de Migración CDT  
> **Objetivo:** Migrar los pines hardcodeados del subsistema de audio en `AudioHAL.h` y `AudioHAL.cpp` hacia el Device Tree autogenerado (`cbdos_device_tree.h`), manteniendo la estabilidad del códec ES8311, la salida I2S DMA dúplex y el amplificador NS4150.

---

## 1. Análisis del Código Actual en Funcionamiento

Al auditar la implementación actual en [`bsp/esp32_p4_jc4880/hal/AudioHAL.h`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/AudioHAL.h) y [`bsp/esp32_p4_jc4880/hal/AudioHAL.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/AudioHAL.cpp), se identifican cuatro bloques funcionales directos:

### A. Definición de Macros en Header (`AudioHAL.h`)
```cpp
#define BOARD_AUDIO_I2S_PORT       I2S_NUM_0
#define BOARD_AUDIO_MCLK_GPIO      13
#define BOARD_AUDIO_BCLK_GPIO      12
#define BOARD_AUDIO_WS_GPIO        10
#define BOARD_AUDIO_DOUT_GPIO      9
#define BOARD_AUDIO_DIN_GPIO       48
#define BOARD_AUDIO_PA_GPIO        11
#define BOARD_AUDIO_CODEC_ADDR     0x30
```

### B. Control del Amplificador de Potencia (PA / NS4150)
* **Arranque Silencioso (Anti-Pop):** En `AudioHAL.cpp:29-40`, GPIO 11 (`BOARD_AUDIO_PA_GPIO`) se configura como salida y se fuerza a nivel bajo (`0`) antes de inicializar I2S o el códec.
* **Driver Códec:** En `AudioHAL.cpp:113`, se entrega `es8311_cfg.pa_pin = BOARD_AUDIO_PA_GPIO` a la configuración de `es8311_codec_new()`.
* **Activación Post-Estabilización:** En `AudioHAL.cpp:175-177` (post `esp_codec_dev_open`) y en `AudioHAL.cpp:212-214` (tras cambiar sample rate), se eleva a nivel alto (`1`) para habilitar el altavoz.

### C. Mapeo de Señales I2S Dúplex (TX Altavoz + RX Micrófono)
En `AudioHAL.cpp:70-82`, la estructura `i2s_std_config_t` asigna:
* `mclk` = `(gpio_num_t)BOARD_AUDIO_MCLK_GPIO` (13)
* `bclk` = `(gpio_num_t)BOARD_AUDIO_BCLK_GPIO` (12)
* `ws`   = `(gpio_num_t)BOARD_AUDIO_WS_GPIO`   (10)
* `dout` = `(gpio_num_t)BOARD_AUDIO_DOUT_GPIO` (9)
* `din`  = `(gpio_num_t)BOARD_AUDIO_DIN_GPIO`  (48)

### D. Bus de Control I2C y Dependencia Compartida
En `AudioHAL.cpp:43-46`:
```cpp
audio_codec_i2c_cfg_t i2c_cfg = {};
i2c_cfg.port = (uint8_t)BOARD_TOUCH_I2C_PORT;
i2c_cfg.addr = BOARD_AUDIO_CODEC_ADDR;
i2c_cfg.bus_handle = TouchHAL::getInstance().getI2cBusHandle();
```
* **Hallazgo clave del código:** El códec ES8311 no abre un bus I2C maestro independiente, sino que se engancha al bus I2C creado previamente por `TouchHAL`.
* `BOARD_AUDIO_CODEC_ADDR` está fijado en `0x30` (dirección de 8-bit write requerida por la API de `esp_codec_dev` de Espressif, equivalente a `0x18 << 1`).
* `BOARD_TOUCH_I2C_PORT` (`0`) proviene de `TouchHAL.h`.

---

## 2. Correspondencia con el Device Tree (`cbdos_device_tree.h`)

En `bsp/esp32_p4_jc4880/hal/cbdos_device_tree.h`, el espacio de nombres `audio` ya contiene las constantes generadas desde el JSON de la placa:

```cpp
// 3. Audio (I2S + Codec)
namespace audio {
    constexpr int PIN_MCLK = 13;
    constexpr int PIN_BCLK = 12;
    constexpr int PIN_WS = 10;
    constexpr int PIN_DOUT = 9;
    constexpr int PIN_DIN = 48;
    constexpr int PIN_PA = 11;
}
```

La correspondencia es 100% idéntica a los valores probados en el código activo. No se requieren cambios en `boards/jc4880p443.json` ni regenerar el árbol para este paso.

---

## 3. Plan de Modificaciones Paso a Paso

### 3.1. Modificación en `bsp/esp32_p4_jc4880/hal/AudioHAL.h`
1. Agregar include del Device Tree:
   ```cpp
   #include "cbdos_device_tree.h"
   ```
2. Eliminar las 6 macros de pines hardcodeados:
   * `BOARD_AUDIO_MCLK_GPIO`
   * `BOARD_AUDIO_BCLK_GPIO`
   * `BOARD_AUDIO_WS_GPIO`
   * `BOARD_AUDIO_DOUT_GPIO`
   * `BOARD_AUDIO_DIN_GPIO`
   * `BOARD_AUDIO_PA_GPIO`
3. **Preservar explícitamente:**
   * `#define BOARD_AUDIO_I2S_PORT   I2S_NUM_0` (hasta Paso 7 de ampliación JSON).
   * `#define BOARD_AUDIO_CODEC_ADDR 0x30` (hasta Paso 7 de ampliación JSON).

### 3.2. Modificación en `bsp/esp32_p4_jc4880/hal/AudioHAL.cpp`
Sustituir cada macro por su símbolo en `cbdos::board::audio::`:
* Línea 29: `if (BOARD_AUDIO_PA_GPIO >= 0)` $\rightarrow$ `if (cbdos::board::audio::PIN_PA >= 0)`
* Línea 31: `(1ULL << BOARD_AUDIO_PA_GPIO)` $\rightarrow$ `(1ULL << cbdos::board::audio::PIN_PA)`
* Línea 38: `gpio_set_level((gpio_num_t)BOARD_AUDIO_PA_GPIO, 0);` $\rightarrow$ `gpio_set_level((gpio_num_t)cbdos::board::audio::PIN_PA, 0);`
* Línea 39: Log de silencio PA $\rightarrow$ usa `cbdos::board::audio::PIN_PA`
* Línea 71: `.mclk = (gpio_num_t)cbdos::board::audio::PIN_MCLK,`
* Línea 72: `.bclk = (gpio_num_t)cbdos::board::audio::PIN_BCLK,`
* Línea 73: `.ws = (gpio_num_t)cbdos::board::audio::PIN_WS,`
* Línea 74: `.dout = (gpio_num_t)cbdos::board::audio::PIN_DOUT,`
* Línea 75: `.din = (gpio_num_t)cbdos::board::audio::PIN_DIN,`
* Línea 113: `es8311_cfg.pa_pin = cbdos::board::audio::PIN_PA;`
* Línea 175-176: Habilitación de PA $\rightarrow$ `cbdos::board::audio::PIN_PA`
* Línea 212-213: Habilitación de PA tras sample rate $\rightarrow$ `cbdos::board::audio::PIN_PA`

---

## 4. Criterios de Aceptación y Validación

1. **Compilación Limpia:**
   ```bash
   . /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build
   ```
   *Criterio:* `Project build complete.` sin advertencias de macros no definidas ni errores de enlace.
2. **Cero Regresiones de Símbolos:**
   Verificar que no queden referencias a `BOARD_AUDIO_*_GPIO` en ningún archivo del workspace.
3. **Actualización del Tracker:**
   Actualizar la tabla de estado en `specs/architecture/PLAN_MIGRACION_CDT_Y_DRIVERS.md` marcando el Paso 3 como `✅`.
