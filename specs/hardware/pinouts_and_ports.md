# 📋 Referencia Técnica Completa de Hardware y Mapa de Pines (CBDos)

Este documento centraliza **toda la información de hardware, pinouts GPIO, buses, periféricos y direcciones I2C** de las placas soportadas para evitar búsquedas repetitivas y asegurar la precisión del desarrollo.

---

## 1. Target ESP32-P4: Guition JC4880P443C (4.3" 480×800 IPS MIPI-DSI)

* **SoC Principal:** ESP32-P4-M3 (Dual-core RISC-V @ 400 MHz + LP Core, 16 MB Flash, 32 MB Hexal-PSRAM @ 200 MHz, PPA 2D Accelerator).
* **Coprocesador Inalámbrico:** ESP32-C6 (WiFi 6, Bluetooth 5 BLE, Zigbee/Thread vía SPI/SDIO).

### 📍 Tabla Completa de Pines GPIO (JC4880P443C)

| Periférico / Función | Señal / Pin | GPIO ESP32-P4 | Protocolo / Configuración | Notas de Hardware |
| :--- | :--- | :--- | :--- | :--- |
| **Pantalla LCD (ST7701S)** | MIPI DSI D0+ / D0- | Pines DSI Dedicados | MIPI-DSI 2-Lanes D-PHY | 480×800 @ 60 FPS, Color RGB565 |
| | MIPI DSI CLK+ / CLK- | Pines DSI Dedicados | MIPI-DSI Clock | LDO VO3 configurado a 2.5V |
| | **LCD Reset (RST)** | **GPIO 5** | Salida Digital (Active LOW) | Pulso de reset inicial 10ms |
| | **Backlight PWM** | **GPIO 23** | LEDC PWM @ 1000 Hz | Control de brillo 0-100% |
| **Touchscreen (GT911)** | **I2C SDA** | **GPIO 7** | Bus I2C Maestro (Puerto 0) | Compartido con códec ES8311 |
| | **I2C SCL** | **GPIO 8** | Bus I2C Maestro (400 kHz) | Pull-ups internos habilitados |
| | **Touch Reset (RST)** | **GPIO 3** | Salida Digital | Pulso reset inicial 10ms |
| | **Touch INT** | **GPIO 4** | Entrada Digital / Interrupción | Detección de pulsación táctil |
| | *Dirección I2C Touch* | `0x5D` (Backup: `0x14`) | I2C 7-bit | GT911 detectado en `0x5D` |
| **Audio (ES8311 + PA)** | **I2S MCLK** | **GPIO 13** | I2S Master Clock (256 × Fs) | 11.2896 MHz @ 44.1 kHz |
| | **I2S BCLK** | **GPIO 12** | I2S Bit Clock | Bit clock estéreo 16-bit |
| | **I2S WS / LRCK** | **GPIO 10** | I2S Word Select / Frame Sync | 44.1 kHz / 48 kHz |
| | **I2S DOUT (Speaker)** | **GPIO 9** | I2S Data Out | Salida de audio hacia DAC ES8311 |
| | **I2S DIN (Mic)** | **GPIO 48** | I2S Data In | Entrada de audio desde Micrófono |
| | **PA Enable (Amplificador)** | **GPIO 11** | Salida Digital (Active HIGH) | Habilita el amplificador de altavoz |
| | *Dirección I2C Códec* | `0x18` (7-bit) / `0x30` (8-bit) | Bus I2C Maestro (Puerto 0) | Everest Semi ES8311 |
| | *Conector de Altavoz* | JST MX 1.25 2P | Salida Analógica Mono/Estéreo | Altavoces 4Ω 2W / 8Ω 1W |
| **MicroSD (Slot SDMMC)** | **SDMMC CLK** | **GPIO 43** | SDMMC Slot 0 (Bus 4-bit) | Alimentado por LDO VO4 (3.3V) |
| | **SDMMC CMD** | **GPIO 44** | SDMMC Slot 0 | Pull-up integrado |
| | **SDMMC D0** | **GPIO 39** | SDMMC Data 0 | Velocidad estándar 20/40 MHz |
| | **SDMMC D1** | **GPIO 40** | SDMMC Data 1 | Slot 0 exclusivo de MicroSD |
| | **SDMMC D2** | **GPIO 41** | SDMMC Data 2 | Soporte nativo FAT32 y **exFAT** |
| | **SDMMC D3** | **GPIO 42** | SDMMC Data 3 | Compatible con SDSC, SDHC y SDXC (16GB a 2TB) |
| **Coprocesador Inalámbrico (ESP32-C6)** | **SDIO CLK** | **GPIO 18** | Bus SDIO 4-bit (Host Slot 1) | Reloj SDIO hacia C6 (20 MHz) |
| | **SDIO CMD** | **GPIO 19** | Bus SDIO 4-bit (Slot 1) | Línea de comando SDIO |
| | **SDIO D0** | **GPIO 14** | Bus SDIO 4-bit (Slot 1) | Línea de datos 0 |
| | **SDIO D1** | **GPIO 15** | Bus SDIO 4-bit (Slot 1) | Línea de datos 1 |
| | **SDIO D2** | **GPIO 16** | Bus SDIO 4-bit (Slot 1) | Línea de datos 2 |
| | **SDIO D3** | **GPIO 17** | Bus SDIO 4-bit (Slot 1) | Línea de datos 3 |
| | **C6 Reset (RST)** | **GPIO 54** | Salida Digital | Reset hardware del ESP32-C6 |
| | **C6 Power (ESP_3V3)** | **GPIO 36 / Pin 18** | VCC 3.3V | Carril de alimentación del C6 |
| **Consola Serial / Debug (USB 1)** | **USB D+ / D-** | Pines Dedicados | USB Serial/JTAG nativo (Full-Speed) | Flasheo de firmware, consola y monitor serie |
| | **UART TX** | **GPIO 38** | UART0 TX @ 115200 bps | Terminal interactivo / ESP-IDF Monitor |
| | **UART RX** | **GPIO 37** | UART0 RX @ 115200 bps | |
| **USB Host / OTG (USB 2)** | **USB_OTG D+ / D-** | Pines Dedicados | **USB 2.0 High-Speed OTG (480 Mbps)** | Host para Android Fastboot/ADB, CDC, HID |
| **Alimentación LDO P4** | **LDO VO3** | Canal 3 | Salida 2.5V fija | Alimentación carril MIPI DSI |
| | **LDO VO4** | Canal 4 | Salida 3.3V conmutable | Alimentación carril MicroSD / VDD_SD |

### 📍 Conector de Expansión JP1 (Pin Header 2×13)

![Diagrama de Flasheo ESP32-C6](images/esp32_c6_flasher_diagram.png)

| Pin Izq (P4 / Host) | Pin Der (C6 / Power) | Función / Conexión de Flasheo |
| :--- | :--- | :--- |
| **3V3** (Pin 1) | **5V** (Pin 2) | Alimentación principal |
| **3V3** (Pin 3) | **5V** (Pin 4) | Líneas de potencia (No se requiere cable para C6) |
| **GND** (Pin 5) | **GND** (Pin 6) | Masa de referencia |
| **GPIO 52** (Pin 7) | **GPIO 33** (Pin 8) | GPIOs de propósito general |
| **GPIO 51** (Pin 9) | **GPIO 31** (Pin 10) | GPIOs de propósito general |
| **GPIO 50** (Pin 11) | **GPIO 30** (Pin 12) | GPIOs de propósito general |
| **GPIO 49** (Pin 13) | **GPIO 29** (Pin 14) | GPIOs de propósito general |
| **GPIO 35** (Pin 15) | **GND** (Pin 16) | Masa de referencia |
| **GPIO 34** (Pin 17) | **ESP_3V3** (Pin 18) | **GPIO 34 (Cable Celeste)** ➔ a **C6_IO9** (Auto-Bootloader) \| `ESP_3V3` alimentado internamente |
| **GPIO 32** (Pin 19) | **C6_U0RXD** (Pin 20) | **Jumper 1 Horizontal Verde** (P4 TX ➔ C6 RX) |
| **GPIO 28** (Pin 21) | **C6_U0TXD** (Pin 22) | **Jumper 2 Horizontal Magenta** (P4 RX 🠄 C6 TX) |
| **I2C_SDA** (Pin 23) | **C6_IO9** (Pin 24) | **C6_IO9 (Cable Celeste)** ➔ conectado a **GPIO 34** |
| **I2C_SCL** (Pin 25) | **C6_CHIP_PU** (Pin 26) | Línea Enable C6 (Controlada por GPIO 54 interno / Dejar libre) |

### 🔌 Flasheador Universal y Presets de Programación
| Preset / Plataforma | TX (Host->Target) | RX (Host<-Target) | BOOT (IO0/IO9) | RST / EN | Baudrate | Firmware Origen |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ESP32-C6 Coprocesador (P4)** | **GPIO 32** | **GPIO 28** | **GPIO 34** | **GPIO 54** | 115200 / 460800 | Embebido SDIO / MicroSD |
| **ESP Externo (Header JP1 P4)** | **GPIO 32** | **GPIO 28** | **GPIO 34** | **GPIO 54** | 115200-921600 | `/sdcard/firmware.bin` |
| **ESP Externo (JC3248W535 S3)** | **GPIO 15** | **GPIO 16** | **GPIO 0** | Manual / -1 | 115200 | `/sdcard/firmware.bin` |
| **Personalizado / Manual** | Configurable | Configurable | Configurable | Configurable | Configurable | Embebido / MicroSD |

### 🔌 Tabla de Conectores Físicos de la Placa (JC4880P443C)

| Nº en Diagrama | Tipo de Conector / Puerto | Pines / Señales | Función en CBDos |
| :--- | :--- | :--- | :--- |
| **1** | Micrófono Integrado | I2S DIN: GPIO 48 | Grabación de notas de voz (`IAudioSource` / ES8311) |
| **2** | MX 1.25 2P Speaker | Salida DAC ES8311 + PA (GPIO 11) | Altavoz principal 4Ω/8Ω (`IAudioSink`) |
| **3** | MX 1.25 2P Batería Litio | VBAT / GND | Entrada de batería LiPo 3.7V con circuito de carga |
| **4** | Type-C (Full-Speed USB 1) | USB D+ / D- nativo | Consola serie/JTAG, flasheo inicial y CLI en caliente |
| **5** | Type-C (High-Speed USB 2) | USB OTG 2.0 (480 Mbps) | Host USB (Fastboot, Teclado/Ratón, USB CDC-ACM) |
| **6** | Pin Header 2×13 2.54 (JP1) | GPIOs 52, 51, 50, 49, 35, 34, 32, 28, I2C, 3V3, 5V, GND | Expansión para Mochilas/Backpacks (NFC, LoRa) y Flasheo C6 |
| **7** | MX 1.25 4P UART0 | TX: GPIO 38, RX: GPIO 37, 3V3, GND | Puerto serie de depuración / telemetría externa |
| **8** | Conector Cámara CSI | MIPI CSI 2-Lanes | Interfaz de cámara de alta velocidad |
| **9** | MX 1.25 4P UART Interface | TX / RX dedicado, VCC, GND | Segundo puerto UART físico para periféricos externos |
| **10** | MX 1.25 4P RS485 Interface | RS485 A, B, VCC, GND | Bus diferencial industrial RS485 |
| **11** | SH 1.0 4P I2C Interface | SDA: GPIO 7, SCL: GPIO 8, 3V3, GND | Bus I2C externo para sensores y periféricos |
| **12** | Módulo Core JC-ESP32P4-M3 | ESP32-P4 + ESP32-C6 | SoC Dual-Core 400MHz + 32MB PSRAM + Coprocesador WiFi 6 |

---

### 📟 Presets de Puertos Serie UART (`SerialTerminalView`)

| Plataforma | Puerto Físico | Pin TX | Pin RX | Preset UI | Velocidades |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ESP32-P4** | **Cabecera JP1 (C6/Mochila)** | **GPIO 32** | **GPIO 28** | `JP1 (TX:32 RX:28)` | 9600 - 921600 bps |
| **ESP32-P4** | **Cabecera JP1 (Alt 1)** | **GPIO 50** | **GPIO 49** | `JP1 Alt (TX:50 RX:49)` | 9600 - 921600 bps |
| **ESP32-P4** | **Conector MX 1.25 UART0 (7)** | **GPIO 38** | **GPIO 37** | `MX 1.25 UART0 (TX:38 RX:37)` | 9600 - 921600 bps |
| **ESP32-P4** | **Conector MX 1.25 UART (9)** | Configurable | Configurable | `MX 1.25 UART Aux` | 9600 - 921600 bps |
| **ESP32-S3** | **Header Externo JC3248** | **GPIO 15** | **GPIO 16** | `S3 Ext (TX:15 RX:16)` | 9600 - 921600 bps |

---


## 2. Target ESP32-S3: Guition JC3248W535 (3.5" 320×480 IPS QSPI)

* **SoC Principal:** ESP32-S3 (Dual-core Xtensa LX7 @ 240 MHz, 16 MB Octal-Flash, 8 MB Octal-PSRAM).
* **Conectividad:** WiFi 2.4 GHz 802.11 b/g/n + Bluetooth 5 (LE) integrados en el SoC.

### 📍 Tabla Completa de Pines GPIO (JC3248W535)

| Periférico / Función | Señal / Pin | GPIO ESP32-S3 | Protocolo / Configuración | Notas de Hardware |
| :--- | :--- | :--- | :--- | :--- |
| **Pantalla LCD (AXS15231B)** | **QSPI CS** | **GPIO 45** | QSPI Bus | Chip Select LCD |
| | **QSPI CLK** | **GPIO 47** | QSPI Clock (40/80 MHz) | Bus gráfico de alta velocidad |
| | **QSPI D0** | **GPIO 21** | QSPI Data 0 | |
| | **QSPI D1** | **GPIO 48** | QSPI Data 1 | |
| | **QSPI D2** | **GPIO 40** | QSPI Data 2 | |
| | **QSPI D3** | **GPIO 39** | QSPI Data 3 | |
| | **LCD Reset (RST)** | **GPIO 4** | Salida Digital | Active LOW |
| | **Backlight (BL)** | **GPIO 1** | PWM / Salida Digital | Control de brillo |
| **Touchscreen (AXS15231B)** | **I2C SDA** | **GPIO 8** | I2C Maestro (Puerto 0) | Touch integrado en controlador AXS |
| | **I2C SCL** | **GPIO 4** | I2C Maestro | |
| | **Touch INT** | **GPIO 3** | Entrada Digital / Interrupción | |
| **Audio / Salida Sonido (I2S)** | **I2S BCLK (Bit Clock)** | **GPIO 42** | I2S0 TX Master | Bit clock estéreo 16-bit |
| | **I2S WS / LRC (Word Select)** | **GPIO 2** | I2S0 TX Master | Frame Sync (44.1 kHz / 48 kHz) |
| | **I2S DOUT (Data Out)** | **GPIO 41** | I2S0 Data Out | Audio PCM 16-bit / Helix MP3 al altavoz |
| | **SPI MOSI** | **GPIO 11** | SPI Bus | |
| | **SPI MISO** | **GPIO 13** | SPI Bus | |
| | **SPI SCK** | **GPIO 12** | SPI Bus | |
| **USB / UART Debug** | **USB D+ / D-** | **GPIO 20 / 19** | USB Serial/JTAG nativo | Carga PlatformIO y monitor serie |

---

## 3. Registros Clave del Códec ES8311 (JC4880P443C)

* **Dirección I2C 7-bit:** `0x18` (0b0011000)
* **Dirección I2C 8-bit (esp_codec_dev / Write):** `0x30` (0b00110000)
* **Dirección I2C 8-bit (Read):** `0x31` (0b00110001)

| Registro | Nombre | Valor Estándar | Función |
| :--- | :--- | :--- | :--- |
| `0x00` | CSM_RESET | `0x80` -> `0x00` | Reset de máquina de estados y arranque |
| `0x01` | CLK_MANAGER | `0x3F` / `0x30` | Modo Esclavo I2S, reloj MCLK habilitado |
| `0x09` | SDP_IN_FMT | `0x0C` | Formato I2S 16-bit Estándar para DAC |
| `0x0A` | SDP_OUT_FMT | `0x0C` | Formato I2S 16-bit Estándar para ADC (Mic) |
| `0x12` | SYSTEM_PWR | `0x00` | Encender todos los bloques digitales |
| `0x13` | BIAS_PWR | `0x10` | Encender circuito de bias analógico |
| `0x14` | CODEC_PWR | `0x1A` | Encender DAC y etapas de salida analógicas |
| `0x32` | DAC_VOLUME | `0x00` a `0xBF` | Control de volumen DAC (`0x00`=-95.5dB, `0xBF`=0dB, `0xFF`=+32dB) |
| `0x37` | DAC_OUT_CTRL | `0x08` | Desmutear etapa de salida del DAC |
| `0x44` | ADC_DAC_MIX | `0x48` | Mezcla loopback para AEC / cancelación de eco |
