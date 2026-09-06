# Plan Formal: Migración Integral a ESP-Hosted 3.0.6 en Target ESP32-P4 + ESP32-C6

## 1. Contexto y Objetivos
- **Objetivo:** Actualizar el stack de comunicación entre el SoC Host (ESP32-P4) y el Coprocesador Inalámbrico (ESP32-C6) a **ESP-Hosted 3.0.6**.
- **Beneficios:**
  1. Soporte oficial y desacoplado para Wi-Fi 6, BLE 5.3 y radio 802.15.4 (OpenThread).
  2. Protocolo de transporte unificado y robusto sobre SDIO Slot 1 (4-bit).
  3. Eliminación de reinicios por desalineación de versiones entre Host y Coprocesador.

---

## 2. Diagnóstico del Estado Actual
1. **Host (ESP32-P4):** Ya enlazado y compilando limpiamente contra `espressif/esp_hosted: 3.0.6`.
2. **Coprocesador (ESP32-C6):** Mantiene en su Flash SPI interna el firmware esclavo de la versión anterior (`1.4.7`).
3. **Binario Embebido (`c6_slave.bin`):** Requiere ser reemplazado por la compilación limpia del proyecto `coprocessor` de la versión 3.0.6.
4. **Hardware JP1:** Conexiones físicas reducidas a **3 líneas lógicas** (TX Pin 19->20, RX Pin 21->22, Boot Pin 17->24). La alimentación (`GPIO 36`) y el reset (`GPIO 54`) se gestionan internamente por software.

---

## 3. Plan de Ejecución Paso a Paso

### Paso 1: Configurar y Compilar el Firmware del Coprocesador C6 (Versión 3.0.6)
- **Directorio de origen:** `bsp/esp32_p4_jc4880/managed_components/espressif__esp_hosted/examples/mcu_hosted_sdio_sdmmc_combined/cp` (o `coprocessor`).
- **Target:** `esp32c6`.
- **Configuraciones Kconfig clave:**
  - `CONFIG_ESP_HOSTED_CP_TRANSPORT_BUS_SDIO=y` (Transporte SDIO).
  - `CONFIG_ESP_HOSTED_CP_FEAT_WIFI=y` (Wi-Fi 6 habilitado).
  - `CONFIG_ESP_HOSTED_CP_FEAT_OPENTHREAD=y` (Soporte OpenThread habilitado).
- **Salida:** Binario combinado o archivo `.bin` listo para ser flasheado en offset `0x0`.

### Paso 2: Actualizar el Binario Embebido en el P4
- Copiar el binario generado en el Paso 1 a la ruta oficial del proyecto P4:
  `bsp/esp32_p4_jc4880/main/assets/c6_slave.bin`.

### Paso 3: Asegurar Desacoplamiento Offline-First en el Host P4
- En `bsp/esp32_p4_jc4880/main/main.cpp`:
  - Mantener la inicialización de red y Wi-Fi **estrictamente bajo demanda**.
  - Evitar que tareas en segundo plano llamen a `esp_wifi_init()` de forma síncrona en el arranque antes de que el C6 esté actualizado.

### Paso 4: Recompilar y Flashear el Host ESP32-P4
- Compilar el proyecto completo del P4 con `idf.py build`.
- Flashear al ESP32-P4 vía puerto serie `/dev/ttyACM0`.

### Paso 5: Flasheo Autónomo del Coprocesador ESP32-C6
- Iniciar el P4.
- Abrir la aplicación **Flasher** en la pantalla táctil (o disparar el servicio de flasheo).
- Ejecutar la programación del C6 mediante el preset interno (utilizando los 3 puentes de JP1 y la energización interna `GPIO 36`).
- Verificar que la barra de progreso llegue al 100% y el C6 quede sincronizado en versión 3.0.6.

---

## 4. Criterios de Verificación y Éxito
1. **Arranque P4:** El ESP32-P4 arranca en frío sin reinicios, inicializando pantalla ST7701S, táctil GT911, audio ES8311 y LVGL 9.5.
2. **Flasheo C6:** `esp-serial-flasher` sincroniza con la ROM del C6 a través de UART1 (GPIO 32/28) y escribe el binario 3.0.6 sin errores de timeout.
3. **Enlace SDIO 3.0.6:** Al solicitar conexión Wi-Fi bajo demanda, el P4 y el C6 completan el handshake SDIO en versión 3.0.6 y obtienen dirección IP correctamente.
