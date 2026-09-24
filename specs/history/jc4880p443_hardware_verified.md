# Especificación Técnica Verificada: Guition JC4880P443C (ESP32-P4)

> **Fuente de Verdad:** Verificado al 100% contra los esquemáticos de ingeniería (`JC4880P443_V1.0`), hojas de datos y código oficial del fabricante (`guition-jc4880p443`). Sin suposiciones ni datos obsoletos.

---

## 1. Pines Internos Soldados (Motherboard / Internal)
Componentes integrados en el PCB. Uso exclusivo del Kernel del SO (`system:*`). No reasignables por software.

| Subsistema / Periférico | Señal / Función | GPIO ESP32-P4 | Protocolo / Configuración | Notas de Hardware |
| :--- | :--- | :--- | :--- | :--- |
| **Pantalla LCD (ST7701S)** | MIPI DSI D0+ / D0- | Pines Dedicados | MIPI-DSI 2-Lanes D-PHY | 480×800 @ 60 FPS |
| | MIPI DSI CLK+ / CLK- | Pines Dedicados | MIPI-DSI Clock | Alimentado por LDO VO3 (2.5V) |
| | **LCD Reset (RST)** | **GPIO 5** | Salida Digital (Active LOW) | Pulso de reset inicial 10ms |
| | **Backlight PWM** | **GPIO 23** | LEDC PWM @ 1000 Hz | Control de brillo vía MP3202 (`IC1`) |
| **Touchscreen (Goodix GT911)** | **I2C SDA** | **GPIO 7** | Bus I2C Maestro 0 | Compartido con Códec ES8311 |
| | **I2C SCL** | **GPIO 8** | Bus I2C Maestro 0 (400 kHz) | Pull-ups 5.1k a 3.3V (R65/R71) |
| | **Touch Reset (RST)** | **GPIO 22** | Salida Digital | Pin 23 del módulo P4 |
| | **Touch INT** | **GPIO 21** | Entrada Digital / Interrupción | Pin 22 del módulo P4 |
| | *Dirección I2C Touch* | `0x5D` (Backup: `0x14`) | I2C 7-bit | GT911 detectado en `0x5D` |
| **Audio (ES8311 + NS4150)** | **I2S MCLK** | **GPIO 13** | I2S Master Clock (256 × Fs) | Reloj maestro DAC/ADC |
| | **I2S BCLK** | **GPIO 12** | I2S Bit Clock | Bit clock estéreo 16-bit |
| | **I2S WS / LRCK** | **GPIO 10** | I2S Word Select / Frame Sync | 44.1 kHz / 48 kHz |
| | **I2S DOUT (Speaker)** | **GPIO 9** | I2S Data Out | Salida de audio hacia DAC ES8311 |
| | **I2S DIN (Mic)** | **GPIO 48** | I2S Data In (`ES7210_SDOUT`) | Entrada de micrófono MSM381A |
| | **PA Enable (Amp)** | **GPIO 11** | Salida Digital (`PA_CTRL`) | Habilita amplificador NS4150 |
| | *Control Códec I2C* | Bus 0 (GPIO 7 / 8) | Dirección I2C: `0x18` (7-bit) | Configuración de volumen y etapas |
| **MicroSD (Slot SDMMC 4-bit)** | **SDMMC D0** | **GPIO 39** | Bus SDMMC 4-bit | Ranura MicroSD física |
| | **SDMMC D1** | **GPIO 40** | Bus SDMMC 4-bit | |
| | **SDMMC D2** | **GPIO 41** | Bus SDMMC 4-bit | |
| | **SDMMC D3** | **GPIO 42** | Bus SDMMC 4-bit | Soporte FAT32 y exFAT |
| | **SDMMC CLK** | **GPIO 43** | Reloj SDMMC | 20 / 40 MHz |
| | **SDMMC CMD** | **GPIO 44** | Comando SDMMC | Pull-up integrado |
| | **Alimentación TF** | LDO VO4 (3.3V) | Regulador interno P4 | Conmutado vía MOSFET Q1 |
| **Sensor de Batería** | **BAT_ADC** | **GPIO 53** | Entrada Analógica (ADC) | Divisor resistivo R52 (68k) / R57 (100k) |
| **Coprocesador ESP32-C6** | **SDIO D0..D3** | **GPIO 14, 15, 16, 17** | Bus SDIO 4-bit (Slot 1) | Enlace de alta velocidad con C6 |
| | **SDIO CLK / CMD** | **GPIO 18 / 19** | Reloj y Comando SDIO | Bus SDIO hacia C6 |
| | **Alimentación C6** | **GPIO 36** | Salida Digital (`ESP_3V3`) | Control de energía del C6 |
| | **Reset C6** | **GPIO 54** | Salida Digital (`C6_CHIP_PU`) | Reset por hardware del C6 |
| **Botón BOOT** | **BOOTMODE** | **GPIO 35** | Entrada Digital | Pulsador físico SW1 |

---

## 2. Conectores Físicos de Expansión (Expansion Ports)

| Conector | Tipo Físico | Señales / Pines Asignados | Uso / Descripción |
| :--- | :--- | :--- | :--- |
| **JP1** | Cabecera 2×13 Pines | GPIOs: `28, 29, 30, 31, 32, 33, 34, 35, 49, 50, 51, 52`<br>I2C: `7 (SDA), 8 (SCL)`<br>Líneas C6: `C6_RXD, C6_TXD, C6_IO9, C6_CHIP_PU`<br>Potencia: `3.3V, 5.0V, GND` | Cabecera principal para módulos de expansión, sensores y flasheo C6 |
| **J5 (UART Aux)** | JST MX 1.25mm 4-Pin | Pin 1: `VCC (5V)`<br>Pin 2: **GPIO 26 (TX1)**<br>Pin 3: **GPIO 27 (RX1)**<br>Pin 4: `GND` | Segundo puerto UART físico para periféricos externos serie |
| **J4 (RS-485)** | JST MX 1.25mm 4-Pin | Pin 1: `5V`<br>Pin 2: **Ao (Diferencial +)**<br>Pin 3: **Bo (Diferencial -)**<br>Pin 4: `GND` | Bus industrial RS-485 (Transceptor MAX485 en GPIO 26/27 con dirección automática) |
| **CN2 (UART0 Debug)** | JST MX 1.25mm 4-Pin | Pin 1: `VIN`<br>Pin 2: **GPIO 38 (UART0_TXD)**<br>Pin 3: **GPIO 37 (UART0_RXD)**<br>Pin 4: `GND` | Consola interactiva / depuración por cable serie externo |
| **CSI (Cámara)** | Conector FPC 15-Pin | MIPI CSI 2-Lanes + I2C (GPIO 7/8) + señales `CSI_IO0`, `CSI_IO1` | Interfaz para sensores de cámara de alta resolución |
| **CN1 (Altavoz)** | JST MX 1.25mm 2-Pin | Salida analógica del amplificador NS4150 (`SPEAKER_P`, `SPEAKER_N`) | Conexión directa para altavoces 4Ω 2W / 8Ω 1W |
| **CN4 (Batería)** | JST MX 1.25mm 2-Pin | `BAT+`, `BAT-` con circuito de carga integrado (IP5306) | Conexión para celda LiPo de 3.7V |
| **USB 1** | USB Type-C | D+ / D- nativos (Full-Speed) | Puerto USB Serial/JTAG nativo para flasheo y monitor |
| **USB 2** | USB Type-C | D+ / D- nativos (High-Speed 480 Mbps) | Puerto USB Host OTG (BadUSB, Teclados, Fastboot, Unidades) |

---

## 3. Pines Físicamente No Conectados (NC / Unconnected)
Estos pines del SoC ESP32-P4 están físicamente sin ruteo en el PCB:
* **GPIO 1, GPIO 2, GPIO 3, GPIO 4**
* Marcados como `NC` en el URM para evitar intentos de asignación.
