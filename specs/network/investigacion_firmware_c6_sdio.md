# Investigación y Diagnóstico: Firmware Esclavo ESP32-C6 (SDIO vs SPI)

> **Fecha:** 20 de Agosto de 2026  
> **Estado:** Documento de Diagnóstico y Plan de Corrección  
> **Hardware:** Placa Guition JC4880P443C (Módulo JC-ESP32P4-M3 Rev 1.3)

---

## 1. Resumen del Problema y Causa Raíz

Durante las pruebas de comunicación Wi-Fi entre el procesador principal **ESP32-P4** y el coprocesador inalámbrico **ESP32-C6**:
1. Se grabó en el ESP32-C6 un binario precompilado (`network_adapter.bin` extraído de `esp-hosted-mcu/slave/`).
2. **Error técnico cometido:** Dicho binario fue compilado con las opciones por defecto del repositorio sin verificar previamente su archivo de configuración (`sdkconfig`).
3. Por defecto en la plantilla de Espressif `esp-hosted-mcu`, la interfaz de transporte suele configurarse en **SPI Esclavo**.
4. Sin embargo, en el diseño de hardware del módulo Guition **JC-ESP32P4-M3**, **NO existen pistas de bus SPI** entre el P4 y el C6. La única conexión física existente en el circuito impreso son las **6 líneas del bus SDIO 4-bit** (`GPIO 14..19` en el P4, conectadas a `GPIO 18..23` en el C6).
5. Como consecuencia, el ESP32-C6 quedó configurado escuchando en un bus inexistente (SPI), ignorando todas las tramas enviadas por el ESP32-P4 a través del bus SDIO, lo que provocaba timeout y fallo de comunicación en el Host.

---

## 2. Evidencia Física del Hardware (Esquemático Oficial Guition)

El plano de ingeniería del módulo `JC-ESP32P4-M3` confirma la topología de conexión física:

### Bus de Datos SDIO Interno:
* **`SDIO CLK`:** ESP32-P4 `GPIO 18` ➔ ESP32-C6 `GPIO 19`
* **`SDIO CMD`:** ESP32-P4 `GPIO 19` ➔ ESP32-C6 `GPIO 18`
* **`SDIO D0`:** ESP32-P4 `GPIO 14` ➔ ESP32-C6 `GPIO 20`
* **`SDIO D1`:** ESP32-P4 `GPIO 15` ➔ ESP32-C6 `GPIO 21`
* **`SDIO D2`:** ESP32-P4 `GPIO 16` ➔ ESP32-C6 `GPIO 22`
* **`SDIO D3`:** ESP32-P4 `GPIO 17` ➔ ESP32-C6 `GPIO 23`

### Control de Encendido y Reset:
* **Reset Hardware:** ESP32-P4 `GPIO 54` ➔ ESP32-C6 `CHIP_PU` (Pin 76 con pullup `R21 10k` a `VCC3V3`).
* **Línea de Alimentación:** `ESP_3V3` (Pines 77 y 78 con polarización en `GPIO 36` vía `R44 10k` y condensadores `C39/C40 22µF`).

---

## 3. Entorno Oficial Creado (`bsp/esp32_c6_slave`)

Para eliminar cualquier ambigüedad o dependencia externa, se integró el entorno oficial del coprocesador en el repositorio:

* **Ruta:** `bsp/esp32_c6_slave`
* **Configuración Clave (`sdkconfig.defaults.esp32c6`):**
  ```ini
  CONFIG_ESP_SDIO_HOST_INTERFACE=y
  CONFIG_ESP_SDIO_DEFAULT_SPEED=y
  CONFIG_ESP_SDIO_STREAMING_MODE=y
  ```
* **Comando de Compilación:**
  ```bash
  cd bsp/esp32_c6_slave && idf.py build
  ```
* **Evidencia de Integración:** El compilador incluye explícitamente `sdio_slave_api.c.obj`, garantizando el driver esclavo SDIO en el binario `network_adapter.bin`.

---

## 4. Negociación de Velocidad del Bus SDIO (20 MHz vs 40 MHz)

* **Handshake Inicial (20 MHz / 4-bit):**
  * Entrega un ancho de banda de **80 Mbps (10 MB/s)**.
  * Diseñado para máxima tolerancia a ruido electromagnético y estabilidad de señal en la fase de descubrimiento (`CMD5`).
* **Modo Alta Velocidad (40 MHz / 4-bit):**
  * Puede habilitarse una vez comprobado el enlace para un ancho de banda de **160 Mbps (20 MB/s)**.

---

## 5. Procedimiento de Flasheo del ESP32-C6

1. **Herramienta Puente:** Cargar `tools/c6_flasher_bridge` en el ESP32-P4.
2. **Conexiones en Conector JP1:**
   * `3V3` ➔ `ESP_3V3` (Pin 1/3 Izq ➔ Pin 18 Der).
   * `GPIO 32` ➔ `C6_U0RXD` (Pin 19 Izq ➔ Pin 20 Der).
   * `GPIO 28` ➔ `C6_U0TXD` (Pin 21 Izq ➔ Pin 22 Der).
   * `C6_IO9` (Pin 24 Der) a `GND` durante 2 segundos.
3. **Comando de Flasheo:**
   ```bash
   python -m esptool --chip esp32c6 -p /dev/ttyACM0 -b 115200 write_flash 0x0 /home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_c6_slave/build/network_adapter.bin
   ```
4. **Post-Flasheo:** Retirar los 2 jumpers de datos y el cable de `IO9` (manteniendo solo la alimentación a `ESP_3V3`).

---

## 6. Estado Actual del Sistema Operativo CBDos

1. **Estabilidad del Host (ESP32-P4):**
   * El driver de red en CBDos fue protegido contra `assert()` para evitar pánicos o reinicios del sistema operativo.
   * Se incorporó el **Interruptor Maestro de Wi-Fi** en la vista de ajustes (**Ajustes ➔ Wi-Fi**), que opera en **Modo Seguro Offline** por defecto.
   * La pantalla IPS 4.3" a 60 FPS, el touch Goodix GT911, el audio ES8311 MP3 y la MicroSD continúan operando con total estabilidad e independencia.

