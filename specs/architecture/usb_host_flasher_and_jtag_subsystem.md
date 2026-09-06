# 🔌 Arquitectura de Expansión USB Host: Flasheo Universal, Módems de Radio y JTAG/SWD

Este documento detalla la especificación técnica, diseño arquitectónico y casos de uso para el soporte de **USB Host (OTG)** en **CBDos**, permitiendo que los dispositivos anfitriones (como el **ESP32-P4 JC4880P443C**) interactúen con microcontroladores externos (ESP32-S3, ESP32-C3, STM32, nRF52, etc.) mediante **Flasheo Plug & Play por USB-C**, **Módems de Radio / Sniffers dedicados** y capacidades de depuración/programación por **JTAG y SWD**.

---

## 🏛️ 1. Visión General del Subsistema USB Host en CBDos

El SoC **ESP32-P4** incorpora un controlador **USB 2.0 High-Speed OTG (Host/Device)** con transceptor PHY integrado, capaz de alimentar y comunicarse con periféricos externos mediante un puerto físico USB-C estándar.

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│                            ESP32-P4 (HOST CBDos v0.2.1+)                         │
│                                                                                  │
│   ┌──────────────────────────────────────────────────────────────────────────┐   │
│   │                        APLICACIONES Y CORE CBDos                         │   │
│   │  • FlasherView (Selector de Firmware .bin en MicroSD / SPIFFS)          │   │
│   │  • RadioManager / MeshEngine (Comunicaciones TLV y Malla)                │   │
│   │  • JtagDebuggerView / LogicAnalyzer (Depuración y Volcado de Memoria)   │   │
│   └─────────────────────────────────────┬────────────────────────────────────┘   │
│                                         │                                        │
│   ┌─────────────────────────────────────▼────────────────────────────────────┐   │
│   │                    CAPA DE TRANSPORTE HAL (bsp/esp32_p4)                 │   │
│   │  • USB Host Core Stack (ESP-IDF 5.5 nativo)                              │   │
│   │  • Clases de Dispositivos: CDC-ACM, CH34x VCP, CP210x VCP, FTDI, HID     │   │
│   └─────────────────────────────────────┬────────────────────────────────────┘   │
└─────────────────────────────────────────┼────────────────────────────────────────┘
                                          │ Conector USB-C (5V VBUS + D+ / D-)
                                          ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     DISPOSITIVOS Y TARGETS EXTERNOS SOPORTADOS                   │
│                                                                                  │
│   1. [FLASHEO DIRECTO] ──▶ Tarjetas ESP32-S3 (JC3248), ESP32-C3, C6, ESP32 DevKits│
│   2. [RADIO / MÓDEM]   ──▶ Dongle USB C3/S3 (Puente ESP-NOW / LoRa / Sniffer)    │
│   3. [JTAG / SWD]      ──▶ Sonda de Depuración / Reprogramación de Chips ARM/ESP  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## ⚡ 2. Flasheo Autónomo por USB-C (USB Host Flasher)

### 2.1. Ventajas frente al flasheo por pines GPIO / UART:
- **Cero Cables Sueltos:** No requiere conectar jumpers individuales (TX, RX, BOOT, RST, GND).
- **Alimentación Integrada:** El Host P4 provee los 5V necesarios por el carril VBUS del cable USB-C.
- **Detección Plug & Play:** Al conectar el dispositivo externo, CBDos lo detecta automáticamente por interrupción USB.

### 2.2. Controladores de Silicio Soportados (Drivers VCP / CDC):

| Driver en CBDos | Chips Compatibles | Comportamiento de Auto-Reset a Bootloader |
| :--- | :--- | :--- |
| **`usb_host_cdc_acm`** | ESP32-S3, ESP32-C3, ESP32-C6, ESP32-P4 (USB Nativo) | Envío de comandos de línea CDC para conmutar `DTR/RTS` virtuales. La ROM entra en modo de descarga de forma autónoma. |
| **`usb_host_ch34x_vcp`** | WCH CH340, CH341, CH9102 (Placas NodeMCU, Clones DevKit, M5Stack) | Conmuta las señales DTR/RTS conectadas a los transistores del circuito EN/GPIO0. |
| **`usb_host_cp210x_vcp`** | Silicon Labs CP2102, CP2104, CP2108 (Placas de desarrollo oficiales) | Secuencia estándar de auto-reset por pines de control VCP. |

### 2.3. Flujo de Ejecución del Flasher USB:
1. El usuario inserta una tarjeta objetivo mediante cable USB-C a la JC4880P443C.
2. El stack USB Host enumera el dispositivo y notifica a `FlasherServiceP4`:
   ```text
   [USB_HOST] Dispositivo detectado: VID=0x303A, PID=0x1001 (ESP32-S3 USB-Serial/JTAG)
   ```
3. En la app **Flasheador Universal**, el usuario selecciona el preset **"USB Target (Auto-Detect)"**.
4. Mediante el **Selector de Archivos**, el usuario escoge la imagen (ej. `/sdcard/firmwares/cbdos_s3.factory.bin`).
5. El motor `esp-serial-flasher` envía los paquetes de borrado y escritura sobre los endpoints `BULK_OUT` / `BULK_IN` del puerto USB.

---

## 📡 3. Coprocesadores y Módems de Radio Externos por USB

Conectar un microcontrolador secundario (como un **ESP32-C3** o **ESP32-S3 Headless**) mediante USB permite desacoplar tareas intensivas de radiofrecuencia del SoC principal.

```text
┌──────────────────────────────────────┐                ┌──────────────────────────────────────┐
│  ESP32-P4 HOST (CBDos)               │                │  DONGLE USB EXTERNO (ESP32-C3)       │
│                                      │                │                                      │
│  • Radio 1 Interna (ESP32-C6):       │    USB-C CDC   │  • Radio 2 Externa (ESP32-C3 / LoRa):│
│    - Conexión Wi-Fi TCP/IP a Router  │ ────────────── │    - Canal Fijo ESP-NOW (Alternet)   │
│    - Servidor TLVGL / Navegación     │ 115200-2Mbps   │    - Sniffer Wi-Fi Promiscuo         │
│    - Bluetooth BLE                   │                │    - Modulación Sub-GHz (SX1262)     │
└──────────────────────────────────────┘                └──────────────────────────────────────┘
```

### 3.1. Casos de Uso Principales:

1. **Operación Multibanda Simultánea (Dual-Radio):**
   - El **C6 interno** se mantiene conectado a internet mediante Wi-Fi convencional.
   - El **C3 externo** transmite y recibe simultáneamente en la red mallada descentralizada (ESP-NOW / Alternet Mesh) sin colisiones de canal ni cambios de tiempo de radio.

2. **Módem de Largo Alcance (LoRa / FLRC / Sub-GHz):**
   - Conectando un módulo **Semtech SX1262 (433/868/915 MHz)** o **SX1280 (2.4 GHz)** al bus SPI del C3 externo, el C3 actúa como un módem USB inteligente:
     - El Host P4 envía tramas compactas TLV por USB.
     - El C3 modula y transmite los paquetes a kilómetros de distancia en frecuencias Sub-GHz con confirmación por hardware (ACK).

3. **Sniffer de Espectro y Ciberseguridad:**
   - El C3 se configura en modo promiscuo continuo, capturando paquetes en el aire (Beacon frames, tramas ESP-NOW, anuncios BLE) y enviando las métricas de canal y RSSI por USB para que CBDos las dibuje en tiempo real en la pantalla táctil de 4.3".

4. **Protocolo de Enmarcado Serial (Dongle USB Bridge):**
   - Especificado en `tools/espnow_usb_bridge/`:
     ```text
     [0xAA 0x55] [DIR 1B] [LEN 2B] [PAYLOAD: MeshHeader + TLV] [CRC8 1B]
     ```

---

## 🛠️ 4. Capacidades JTAG y SWD en CBDos

Tanto el **ESP32-P4** como el **ESP32-S3** soportan operaciones avanzadas sobre el estándar IEEE 1149.1 (**JTAG**) y ARM Serial Wire Debug (**SWD**), utilizables en dos direcciones:

### 4.1. Como Dispositivos Objetivo (Target JTAG / USB-Serial-JTAG):
- Los chips **ESP32-S3, ESP32-C3, ESP32-C6 y ESP32-P4** integran en su silicio un controlador **USB-Serial/JTAG**.
- A través de sus líneas USB expone simultáneamente:
  1. Puerto Serie virtual (para logs y flasheo).
  2. Interfaz **JTAG directa**, lo que permite conectarlos a una computadora y depurarlos con `openocd -f board/esp32s3-builtin.cfg` y `gdb` sin necesidad de sondas externas como ESP-Prog, FT2232 o J-Link.

### 4.2. Como Sonda y Programador JTAG / SWD Anfitrión (Host Debugger):
- Gracias a la velocidad de cómputo del ESP32-P4 (400 MHz RISC-V Dual-Core) y a sus pines GPIO de alta velocidad, CBDos puede operar como una **Sonda de Programación y Diagnóstico Portátil**:
  - **Programación de Chips Brickeados:** Si un microcontrolador objetivo tiene la Flash corrupta o el bootloader UART deshabilitado, CBDos puede forzar la escritura de memoria mediante las líneas JTAG (`TCK`, `TMS`, `TDI`, `TDO`).
  - **Soporte Multi-Arquitectura:** Capacidad de programar y depurar microcontroladores externos **ARM Cortex-M (STM32, RP2040, nRF52, SAMD)** mediante el protocolo **SWD (SWCLK + SWDIO)** directamente desde la interfaz gráfica de CBDos.
  - **Lectura de Registros y Volcado de Memoria:** Extracción de firmware y análisis forense de microcontroladores en campo sin equipo de laboratorio.

---

## 📋 5. Resumen de la Matriz de Conectividad

| Función | Medio Físico | Hardware Requerido | Estado en CBDos |
| :--- | :--- | :--- | :--- |
| **Flasheo Autónomo UART** | Pines GPIO (JP1) | Cables Jumper (TX, RX, BOOT, RST) | **Completamente Operativo (v0.2.1)** |
| **Flasheo USB Host (CDC/VCP)** | Puerto USB-C | Cable USB-C a USB-C estándar | **Drivers enlazados; requiere capa de transporte** |
| **Módem / Dongle ESP-NOW** | Puerto USB-C | ESP32-C3 / S3 con firmware `tools/espnow_usb_bridge` | **Firmware disponible en `tools/`** |
| **Módem Sub-GHz / LoRa** | Puerto USB-C / JP1 | Módulo SX1262 / SX1280 acoplado | **Diseño especificado en `draft_unified_radio_manager`** |
| **Sonda JTAG / SWD Host** | Pines JP1 / USB-C | Pines GPIO de alta velocidad / USB | **Arquitectura soportada por hardware P4/S3** |
