# ⚡ Guía de Flasheo e Instalación

Esta guía te muestra cómo flashear **CBDos** en tus placas soportadas utilizando herramientas oficiales y sencillas.

---

## 1. Requisitos Previos

* Cable USB-C de datos de buena calidad (asegúrate de que no sea solo de carga).
* Tarjeta MicroSD (formateada en **FAT32** o **exFAT**).
* Para compilar desde código fuente:
  * **ESP32-P4:** ESP-IDF v5.5 instalado.
  * **ESP32-S3:** VS Code con extensión PlatformIO instalada.

---

## 2. Flashear en ESP32-P4 (Guition JC4880P443C)

El target principal de CBDos utiliza **ESP-IDF 5.5** nativo.

### Método Rápido (Compilación y Flasheo Directo)

1. Conecta la placa mediante el puerto USB **UART/JTAG** (o puerto CDC).
2. Abre una terminal y exporta el entorno de ESP-IDF:
   ```bash
   . /ruta/a/esp-idf/export.sh
   ```
3. Navega al directorio del BSP de P4 y compila:
   ```bash
   cd bsp/esp32_p4_jc4880
   idf.py build
   ```
4. Flashea y abre el monitor serial para comprobar el arranque:
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```

> [!TIP]
> Si la placa no entra en modo bootloader automáticamente, mantén presionado el botón **BOOT**, presiona una vez **RST** y suelta **BOOT**.

---

## 3. Flashear en ESP32-S3 (Guition JC3248W535)

Para la placa basada en ESP32-S3 se utiliza **PlatformIO** con el core pioarduino.

### Compilar y Flashear con PlatformIO

1. Conecta la placa por USB.
2. Desde la raíz del repositorio ejecuta:
   ```bash
   # Compilar el proyecto
   pio run -d bsp/esp32_s3_jc3248

   # Flashear al dispositivo
   pio run -d bsp/esp32_s3_jc3248 -t upload --upload-port /dev/ttyACM0

   # Monitorear la salida por consola
   pio device monitor -d bsp/esp32_s3_jc3248 -b 115200
   ```

---

## 4. Preparación de la Tarjeta MicroSD

Para disfrutar de todas las funciones multimedia y aplicaciones sin compilar:

1. Formatea tu MicroSD en **FAT32**.
2. Copia tus archivos en las siguientes carpetas estándar en la raíz de la tarjeta:
   * `/music/` — Archivos de audio `.mp3` o `.wav`.
   * `/apps/` — Aplicaciones dinámicas `.luapp`.
   * `/cartridges/` — Cartuchos de juegos PICO-8 o binarios retro.
   * `/wallpapers/` — Fondos de pantalla en formato `.jpg` o `.bin` RGB565.
3. Inserta la tarjeta en la ranura MicroSD del dispositivo antes de encenderlo.
