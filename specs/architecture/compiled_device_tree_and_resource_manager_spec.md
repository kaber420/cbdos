# Especificación Técnica: Compiled Device Tree (CDTc) y Universal Resource Manager

**Estado:** Especificación V2 (Basada en Revisión de Seguridad y Rendimiento)
**Target:** ESP32-P4 / ESP32-S3 (Agnóstico, Bare-Metal)

---

## 1. Motivación y Problema Actual

La arquitectura de CBDos buscaba inicialmente abstraer el hardware mediante una clase abstracta (HAL) instanciada en el arranque, pero aún mantenía números mágicos y configuraciones de pines hardcodeadas en los BSPs (ej. `S3GpioBackend::isPinAvailable()`). 

La primera iteración de solución proponía un "Device Tree en JSON" parseado en el arranque (`app_main`). Sin embargo, una auditoría técnica profunda reveló problemas críticos con ese enfoque:
1. **Rendimiento y Memoria:** Parsear JSON requiere librerías pesadas y alocación dinámica de memoria (heap) en el "milisegundo cero" del arranque, causando fragmentación y latencia inaceptable para un entorno bare-metal.
2. **Seguridad (RCE por proximidad):** Confiar ciegamente en un descriptor NFC para reservar pines permite que un atacante cree un tag malicioso pidiendo pines del sistema (ej. el pin de Backlight o los *strapping pins*) para causar DoS (Denegación de Servicio) o brickear la consola.
3. **Visión Incompleta de Conflictos:** Las colisiones no son solo "Pines", sino "Buses" (SPI/I2C compartidos) y "Periféricos" (Canales DMA/LEDC).

## 2. Visión Arquitectónica Solución (V2)

Para resolver esto, CBDos adoptará un modelo híbrido inspirado en Zephyr OS y Linux:
1. **CDTc (Compiled Device Tree):** El hardware base se declara en archivos YAML legibles por humanos, pero un script de compilación los transforma en **estructuras C++ nativas (`constexpr`)** que no consumen RAM de inicialización ni requieren parseo.
2. **Universal Resource Manager (URM):** Un árbitro central que no solo bloquea pines, sino buses de comunicación y periféricos internos, con gestión de propiedad y limpieza automática (RAII).
3. **Overlay Dinámico Seguro (Backpacks):** El BackpackManager valida firmas o IDs de los descriptores NFC y solicita recursos al URM operando *estrictamente* dentro del "Pool Libre" de la cabecera de expansión (JP1).

---

## 3. Arquitectura del Compiled Device Tree (CDTc)

### 3.1 Archivo Declarativo (`boards/jc4880p443.yaml`)
El desarrollador o mantenedor de hardware edita este archivo. No se sube al dispositivo final.

```yaml
board: "JC4880P443C"
version: "1.3"

resources:
  system_locked:
    # Pantalla (MIPI-DPI)
    display:
      driver: "st7701s"
      bl_pin: 23
      rst_pin: 5
      pwm_channel: "LEDC_CH2"
    # Audio (I2S)
    audio:
      driver: "es8311"
      mclk_pin: 13
      bclk_pin: 12
      ws_pin: 10
      dout_pin: 9
      pa_pin: 11
      i2c_addr: 0x18
  
  expansion:
    jp1:
      allowed_pins: [20, 21, 22, 24, 25, 26, 33, 34, 35, 36, 37]
      prohibited_buses: ["SPI2_HOST"] # Si el sistema ya usa SPI2 para flash
```

### 3.2 Codegen (Build-Time)
El script `tools/cdtc.py` procesará el YAML durante `idf.py build` y generará `board_config_generated.h`:

```cpp
// Generado automáticamente - NO EDITAR
constexpr uint8_t DISPLAY_RST_PIN = 5;
constexpr uint8_t DISPLAY_BL_PIN = 23;
constexpr uint8_t AUDIO_MCLK_PIN = 13;

constexpr uint8_t JP1_ALLOWED_PINS[] = {20, 21, 22, 24, 25, 26, 33, 34, 35, 36, 37};
```

---

## 4. Universal Resource Manager (URM)

Reemplaza al simple `GpioResourceManager`. Su responsabilidad es evitar que dos drivers intenten usar el mismo canal I2C, el mismo pin o el mismo canal PWM al mismo tiempo.

### 4.1 Tipos de Recursos
- **GPIOs:** (Ej. Pin 33) Modo Exclusivo o Compartido (lectura).
- **Buses:** (Ej. `I2C_NUM_0`) Compartidos.
- **Direcciones I2C:** (Ej. Bus 0, Addr 0x76) Exclusivo.
- **Canales Periféricos:** (Ej. `LEDC_CHANNEL_2`) Exclusivo.

### 4.2 Flujo de Propiedad
Se utiliza el concepto de *Owner* (Dueño) y `ResourceHandle` (RAII en C++). Si una aplicación Lua pide un pin y luego crashea o se cierra, al destruirse el objeto de la aplicación, el `ResourceHandle` se destruye y libera el pin automáticamente (evitando *leaks* de hardware).

---

## 5. El Flujo de las Mochilas Modulares (Seguridad NFC)

Cuando el lector NFC detecta una mochila:

1. **Lectura y Validación (MsgPack/CBOR):** Se lee un payload súper ligero. El sistema revisa la firma o busca el `ID` (ej. `MOD_LORA_SX1262`) en una lista blanca de hardware conocido pre-cargada en el SO.
2. **Petición Transaccional al URM:** El `BackpackManager` le dice al URM: "Quiero el bus SPI, y los pines CS=33, MOSI=35".
3. **Bloqueo Estricto (Hardware Firewall):** El URM actúa como un cortafuegos físico:
   - El Device Tree indica *exactamente* qué pines puede usar el BackpackManager (el pool `JP1_ALLOWED_PINS`).
   - Si una mochila pide un pin ocupado por el sistema (ej. el pin de la pantalla o audio), **el gestor ni siquiera reconoce la petición** porque ese pin no existe en su lista de pines permitidos.
   - Lo que ya está ocupado por el sistema no se toca. Si hay violación de acceso, se aborta la carga de la mochila inmediatamente.
4. **Instanciación:** Se configuran los pines, se hace "Power Enable" y se lanza el driver (`meshcore`) o la Lua app.
5. **Desacoplamiento (Detach):** El URM libera los recursos, y el sistema pone los pines de la cabecera en estado seguro (`Hi-Z` o alta impedancia) para evitar cortocircuitos físicos.

---

## 6. Plan de Ejecución Propuesto (Fases)

| Fase | Objetivo | Entregables Principales |
| :--- | :--- | :--- |
| **Fase 0** | **Clean-up y Codegen** | Crear `cdtc.py`. Extraer números mágicos de `hal_uart_p4.cpp` a YAML. |
| **Fase 1** | **El URM Core** | Implementar `resource_manager.cpp` (sin parseo de mochilas aún). Proteger el arranque base. |
| **Fase 2** | **Capa Lua y HAL** | Modificar `hal_display` y binding Lua (`cbdos.gpio`) para que consuman APIs del URM. |
| **Fase 3** | **Seguridad Backpack** | Parser CBOR firmado, `BackpackManager`, control de estados attach/detach. |
