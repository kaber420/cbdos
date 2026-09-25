> ⚠️ **AVISO DE ESPECIFICACIÓN:** Las asignaciones de pines del sistema y del Device Tree se rigen de forma canónica por [CDT_CANONICAL.md](file:///home/kaber420/Documentos/proyectos/cbdos/specs/architecture/CDT_CANONICAL.md).
> Nota: El Touch Goodix GT911 opera en **GPIO 22 (RST)** y **GPIO 21 (INT)** verificado contra los esquemáticos de ingeniería (`JC4880P443_V1.0`).

# Especificación Oficial de Hardware y Conectores (CBDos)

> **Documento Oficial Limpio:** Datos 100% reales contrastados directamente contra los esquemáticos de ingeniería (`JC4880P443_V1.0`), la fotografía y guía oficial de fábrica (`JC4880P443C_I_W Specifications-EN-V1.0.pdf`, pág. 5) y el código BSP oficial de Guition.

---

## 1. Target Principal: Guition JC4880P443C (ESP32-P4 RISC-V @ 400 MHz)

### 📍 A. Pines Internos Soldados (Motherboard / Internal)
Periféricos soldados fijos en el PCB. Manejados por drivers del SO (`system:*`). No reasignables.

| Periférico / Subsistema | Función / Señal | GPIO ESP32-P4 | Protocolo / Configuración | Notas de Hardware |
| :--- | :--- | :--- | :--- | :--- |
| **Pantalla LCD (ST7701S)** | MIPI DSI 2-Lanes | Pines Dedicados | DSI D0+, D0-, CLK+, CLK- | 480×800 @ 60 FPS, LDO VO3 (2.5V) |
| | **LCD Reset (RST)** | **GPIO 5** | Salida Digital (Active LOW) | Pulso inicial 10ms |
| | **Backlight PWM** | **GPIO 23** | LEDC PWM @ 1000 Hz | Driver step-up MP3202 (`IC1`) |
| **Touchscreen (Goodix GT911)** | **I2C SDA** | **GPIO 7** | Bus I2C Maestro 0 | Compartido con Códec ES8311 |
| | **I2C SCL** | **GPIO 8** | Bus I2C Maestro 0 (400 kHz) | Pull-ups 5.1k a 3.3V (R65/R71) |
| | **Touch Reset (RST)** | **GPIO 22** | Salida Digital | Pin 23 del módulo P4 |
| | **Touch INT** | **GPIO 21** | Entrada Digital / Interrupción | Pin 22 del módulo P4 |
| | *Dirección I2C* | `0x5D` (Backup: `0x14`) | I2C 7-bit | GT911 detectado en `0x5D` |
| **Audio (ES8311 + NS4150)** | **I2S MCLK** | **GPIO 13** | I2S Master Clock | Reloj maestro DAC/ADC |
| | **I2S BCLK** | **GPIO 12** | I2S Bit Clock | Bit clock estéreo 16-bit |
| | **I2S WS / LRCK** | **GPIO 10** | I2S Word Select / Frame Sync | 44.1 kHz / 48 kHz |
| | **I2S DOUT (Speaker)** | **GPIO 9** | I2S Data Out | Salida al DAC del códec ES8311 |
| | **I2S DIN (Mic)** | **GPIO 48** | I2S Data In (`ES7210_SDOUT`) | Entrada del micrófono integrado |
| | **PA Enable (Amp)** | **GPIO 11** | Salida Digital (`PA_CTRL`) | Habilita amplificador de altavoz NS4150 |
| | *Control I2C Códec* | Bus I2C 0 (GPIO 7 / 8) | Dirección `0x18` (7-bit) | Configuración de volumen y etapas |
| **MicroSD (Slot SDMMC 4-bit)** | **SDMMC D0** | **GPIO 39** | Bus SDMMC 4-bit | Slot MicroSD integrado |
| | **SDMMC D1** | **GPIO 40** | Bus SDMMC 4-bit | |
| | **SDMMC D2** | **GPIO 41** | Bus SDMMC 4-bit | |
| | **SDMMC D3** | **GPIO 42** | Bus SDMMC 4-bit | |
| | **SDMMC CLK** | **GPIO 43** | Reloj SDMMC | 20 / 40 MHz |
| | **SDMMC CMD** | **GPIO 44** | Línea de Comando | Pull-up integrado |
| | **Alimentación MicroSD** | LDO VO4 (3.3V) | Regulador interno ESP32-P4 | Conmutado vía MOSFET Q1 |
| **Sensor de Batería** | **BAT_ADC** | **GPIO 53** | Entrada Analógica (ADC) | Divisor resistivo R52 (68k) / R57 (100k) |
| **Coprocesador C6 (Interno U3)** | **SDIO D0..D3** | **GPIO 14, 15, 16, 17** | Bus SDIO interno | Comunicación de alta velocidad (oculta en módulo) |
| | **SDIO CLK / CMD** | **GPIO 18 / 19** | Reloj y Comando | Bus SDIO interno hacia C6 |
| | **Reset / Power-Down C6** | **GPIO 54** | Salida Digital (`C6_CHIP_PU`) | Control de reset y bajo consumo del C6 (carril 3.3V es permanente vía TLV62569) |
| **Strapping / ROM Boot** | **Boot Mode Strapping** | **GPIO 36** | Entrada Strapping con Pull-up 10k (R44 a 3.3V) | Pin interno de configuración de ROM boot del ESP32-P4. Fijado a HIGH por hardware según especificación Espressif. No controla potencia. |
| | **Handshake P4-C6** | **GPIO 6** | `C6_IO2` | Interrupción/handshake directo P4 $\leftrightarrow$ C6 |
| | **C6 UART0 TX** | **Expuesto en JP1** | `C6_U0TXD` | Pin 22 de cabecera externa JP1 |
| | **C6 UART0 RX** | **Expuesto en JP1** | `C6_U0RXD` | Pin 20 de cabecera externa JP1 |
| | **C6 Boot Mode** | **Expuesto en JP1** | `C6_IO9` | Pin 24 de cabecera externa JP1 (Pull-up 5.1K) |
| | **C6 Reset Ext** | **Expuesto en JP1** | `C6_CHIP_PU` | Pin 26 de cabecera externa JP1 (RC 10K/1uF) |
| | **Antena RF** | **Pista LAN_OUT** | Pin 2 Módulo `U3` | Hacia antena cerámica ANT1 |
| **Botón Físico BOOT** | **BOOTMODE** | **GPIO 35** | Entrada Digital con Pull-up | Pulsador SW1 en placa |

---

### 🔌 B. Conectores Físicos Reales (Fotografía Oficial de Fábrica)

| Nº en Foto Oficial | Conector / Puerto | Tipo Físico | Señales / Pines Asignados | Función en el Sistema |
| :---: | :--- | :--- | :--- | :--- |
| **1** | **Micrófono** | Integrado en PCB | I2S DIN: **GPIO 48** | Entrada de micrófono analógico con códec ES8311 |
| **2** | **Altavoz** | Conector 2 Pines | Salida NS4150 (`SPEAKER_P`, `SPEAKER_N`) | Altavoz mono 4Ω 2W / 8Ω 1W |
| **3** | **Batería LiPo** | Conector 2 Pines | `BAT+`, `BAT-` | Entrada de batería 3.7V con cargador IP5306 |
| **4** | **USB 1 (Full-Speed)** | USB Type-C | D+ / D- nativos | Flasheo, consola interactiva y monitor serie nativo |
| **5** | **USB 2 (High-Speed)** | USB Type-C | D+ / D- nativos (480 Mbps) | Host USB OTG (BadUSB, Teclados, Fastboot, Mass Storage) |
| **6** | **Cabecera JP1** | Header 2×13 (2.54mm) | GPIOs: `28, 29, 30, 31, 32, 33, 34, 35, 49, 50, 51, 52`<br>I2C: `7 (SDA), 8 (SCL)`<br>Control C6: `RXD, TXD, IO9, CHIP_PU`<br>Potencia: `3.3V, 5.0V, GND` | Cabecera principal para módulos externos y periféricos |
| **7** | **CN2 (UART0 Debug)** | Conector 4 Pines | Pin 1: `VIN (5V)`<br>Pin 2: **GPIO 38 (TX0)**<br>Pin 3: **GPIO 37 (RX0)**<br>Pin 4: `GND` | Puerto de depuración serie y alimentación externa |
| **8** | **Cámara CSI** | FPC 15 Pines | MIPI CSI 2-Lanes + I2C (`GPIO 7, 8`) + `CSI_IO0, CSI_IO1` | Interfaz de cámara de alta resolución |
| **9** | **J5 (UART Aux)** | Conector 4 Pines | Pin 1: `5V`<br>Pin 2: **GPIO 26 (TX1)**<br>Pin 3: **GPIO 27 (RX1)**<br>Pin 4: `GND` | Segundo puerto serie físico para periféricos |
| **10** | **J4 (RS-485)** | Conector 4 Pines | Pin 1: `5V`<br>Pin 2: **Ao (Diferencial +)**<br>Pin 3: **Bo (Diferencial -)**<br>Pin 4: `GND` | Bus industrial RS-485 (Transceptor MAX485 en GPIO 26/27 con dirección automática) |
| **11** | **CN3 (I2C Externo)** | **Conector HS 1.0mm 4 Pines** | Pin 1: `GND`<br>Pin 2: `ESP_3V3`<br>Pin 3: **GPIO 8 (SCL)**<br>Pin 4: **GPIO 7 (SDA)** | Puerto I2C externo para sensores/módulos *(el folleto decía 2P por errata, el esquemático confirma 4P)* |
| **12** | **Módulo Core** | Módulo JC-ESP32P4-M3 | ESP32-P4 + ESP32-C6 | SoC Principal + Coprocesador inalámbrico |

---

### ⚠️ C. Pines No Conectados (NC) o Reservados Internos
* **GPIO 1, GPIO 2, GPIO 3, GPIO 4:** No tienen ruteo de pistas en el PCB de esta placa (NC).
* **GPIO 36:** Pin interno de *strapping* del ESP32-P4, polarizado a 3.3V mediante resistor R44 (10k). Inaccesible externamente (no presente en JP1 ni conectores MX) y reservado por hardware para el arranque seguro del SoC.

---

## 2. Target Secundario: Guition JC3248W535 (ESP32-S3)

* **SoC:** ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz (16 MB Flash, 8 MB Octal-PSRAM).
* **Conectividad:** Wi-Fi 2.4 GHz + BLE 5 nativos en el chip.

| Periférico | Señal | GPIO ESP32-S3 | Tipo / Protocolo |
| :--- | :--- | :--- | :--- |
| **Pantalla (AXS15231B)** | QSPI CS / CLK / D0..D3 | GPIO 45, 47, 21, 48, 40, 39 | Bus gráfico QSPI |
| | LCD Reset / Backlight | GPIO 4 / GPIO 1 | Salida Digital / PWM |
| **Touchscreen** | I2C SDA / SCL / INT | GPIO 8 / GPIO 4 / GPIO 3 | I2C Bus |
| **Audio I2S** | BCLK / WS / DOUT | GPIO 42 / GPIO 2 / GPIO 41 | I2S0 TX Master |
| **MicroSD Slot** | SPI CS / MOSI / MISO / SCK | GPIO 10, 11, 13, 12 | Bus SPI |
| **Puerto Serie Ext** | UART TX / RX | GPIO 15 / 16 | Cabecera externa JC3248 |
| **USB Serial** | USB D+ / D- | GPIO 20 / 19 | USB Serial/JTAG nativo |

---

## 3. Registros del Códec ES8311 (I2C Bus 0 - Dirección 0x18)

| Registro | Nombre | Valor | Función en CBDos |
| :---: | :--- | :---: | :--- |
| `0x00` | CSM_RESET | `0x00` | Arranque normal de máquina de estados |
| `0x01` | CLK_MANAGER | `0x3F` / `0x30` | Modo Esclavo I2S con MCLK activo |
| `0x09` | SDP_IN_FMT | `0x0C` | Formato I2S estándar 16-bit DAC |
| `0x0A` | SDP_OUT_FMT | `0x0C` | Formato I2S estándar 16-bit ADC (Mic) |
| `0x12` | SYSTEM_PWR | `0x00` | Bloques digitales encendidos |
| `0x13` | BIAS_PWR | `0x10` | Circuito analógico de bias encendido |
| `0x14` | CODEC_PWR | `0x1A` | Salidas analógicas y DAC encendidos |
| `0x32` | DAC_VOLUME | `0xBF` (0dB) | Control de volumen máster del DAC |
| `0x37` | DAC_OUT_CTRL | `0x08` | Desmutear salida analógica al altavoz |
| `0x44` | ADC_DAC_MIX | `0x48` | Canal de mezcla loopback / cancelación de eco |
