# 📡 Guía Técnica de Integración Wi-Fi 6: ESP32-P4 + ESP32-C6 (ESP-Hosted SDIO)

Esta guía documenta la arquitectura, el mapa de conexiones de hardware, los pasos de flasheo del firmware esclavo en el **ESP32-C6** y la configuración en **ESP-IDF** para dotar de conectividad inalámbrica (Wi-Fi 6 y BLE 5) al **ESP32-P4** en placas como la **Guition JC4880P443C (módulo ESP32-P4-M3)**.

---

## 1. Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-P4 (Host / Maestro)                │
│                                                             │
│   Tu Aplicación / cbdos (APIs estándar esp_wifi_...)        │
│                           ▲                                 │
│                           ▼                                 │
│                   esp_wifi_remote                           │
│                           ▲                                 │
│                           ▼                                 │
│                   esp_hosted (Driver SDIO Host)             │
└───────────────────────────┬─────────────────────────────────┘
                            │  Bus SDIO 4-bit (40 MHz)
                            │  CLK[18], CMD[19], D0[14], D1[15], D2[16], D3[17], RST[54]
┌───────────────────────────┴─────────────────────────────────┐
│                    ESP32-C6 (Slave / Esclavo)               │
│                                                             │
│         esp-hosted-mcu slave firmware (SDIO Slave)          │
│                           ▲                                 │
│                           ▼                                 │
│            Radio Hardware (Wi-Fi 6 802.11ax + BLE 5)        │
└─────────────────────────────────────────────────────────────┘
```

* **ESP32-P4:** SoC RISC-V enfocado en multimedia (pantalla MIPI-DSI, cámara, audio, acelerador 2D PPA). No tiene radio integrada.
* **ESP32-C6:** Co-procesador inalámbrico dedicado. Maneja la pila física de Wi-Fi 6 y Bluetooth.
* **Protocolo de Enlace (SDIO 4-bit):** Comunicación de alta velocidad (~25 MB/s teóricos @ 40 MHz) a través del framework oficial **ESP-Hosted-MCU**.
* **Capa de Abstracción (`esp_wifi_remote`):** Permite que el ESP32-P4 utilice las APIs nativas de ESP-IDF (`esp_wifi_init`, `esp_wifi_connect`, `esp_netif`) redirigiendo todo el tráfico al C6 de forma transparente.

---

## 2. Mapa de Pines de Hardware (Guition JC4880P443C / P4-M3)

### Bus SDIO Interno (P4 <-> C6)
| Función SDIO | GPIO ESP32-P4 | Pin ESP32-C6 (Ref) | Notas |
| :--- | :--- | :--- | :--- |
| **SDIO CLK** | **GPIO 18** | GPIO 19 | Reloj del bus SDIO (40 MHz) |
| **SDIO CMD** | **GPIO 19** | GPIO 18 | Línea de comandos y respuestas |
| **SDIO D0** | **GPIO 14** | GPIO 20 | Línea de datos 0 |
| **SDIO D1** | **GPIO 15** | GPIO 21 | Línea de datos 1 |
| **SDIO D2** | **GPIO 16** | GPIO 22 | Línea de datos 2 |
| **SDIO D3** | **GPIO 17** | GPIO 23 | Línea de datos 3 |
| **C6 Reset (EN)**| **GPIO 54** | EN / CHIP_PU | **Active LOW** (Nivel bajo = Reset, Nivel alto = Operación normal) |

### 📍 Conector de Expansión JP1 (Pin Header 2×13)
En la placa **Guition JC4880P443C**, el conector de 26 pines (`JP1`) tiene alineados los pines del P4 y del C6 de forma simétrica:

```text
       COLUMNA IZQUIERDA (P4)          │       COLUMNA DERECHA (C6 / Power)
  ─────────────────────────────────────┼──────────────────────────────────────
   [Pin 1]   3V3                       │  [Pin 2]   5V
   [Pin 3]   3V3                       │  [Pin 4]   5V
   [Pin 5]   GND                       │  [Pin 6]   GND
   [Pin 7]   GPIO 52                   │  [Pin 8]   GPIO 33
   [Pin 9]   GPIO 51                   │  [Pin 10]  GPIO 31
   [Pin 11]  GPIO 50                   │  [Pin 12]  GPIO 30
   [Pin 13]  GPIO 49                   │  [Pin 14]  GPIO 29
   [Pin 15]  GPIO 35                   │  [Pin 16]  GND (Tierra C6 / P4)
   [Pin 17]  GPIO 34                   │  [Pin 18]  ESP_3V3 (Alimentación C6)
   [Pin 19]  GPIO 32 (P4 TX Puente)    │  [Pin 20]  C6_U0RXD (C6 UART RX)  ◄── [Jumper 1 Horizontal]
   [Pin 21]  GPIO 28 (P4 RX Puente)    │  [Pin 22]  C6_U0TXD (C6 UART TX)  ◄── [Jumper 2 Horizontal]
   [Pin 23]  I2C_SDA (GT911/ES8311)    │  [Pin 24]  C6_IO9   (C6 BOOT)     ◄── [Cable hacia GND]
   [Pin 25]  I2C_SCL (GT911/ES8311)    │  [Pin 26]  C6_CHIP_PU (C6 EN/Reset)
```

---

## 3. Método de Flasheo: Puente Serie con 2 Jumpers + 1 Cable

Gracias a la disposición del conector JP1, **NO necesitas adaptadores USB-Serial externos**. Puedes usar el propio ESP32-P4 como pasarela de flasheo hacia el ESP32-C6.

### Conexiones de Hardware:
1. **Jumper Horizontal 1:** Conecta directamente el pin `GPIO 32` con `C6_U0RXD` (Pines 19 ⟷ 20).
2. **Jumper Horizontal 2:** Conecta directamente el pin `GPIO 28` con `C6_U0TXD` (Pines 21 ⟷ 22).
3. **Cable a Tierra (Modo BOOT):** Conecta un cable dupont desde `C6_IO9` (Pin 24) hacia `GND` (Pin 16 o Pin 6).
4. *`C6_CHIP_PU` (Pin 26):* Se deja libre (el ESP32-P4 lo controla internamente por GPIO 54 o sube a 3.3V automáticamente).

---

### Paso a Paso para Flashear el C6:

#### Paso 1: Binario listo para grabar
El firmware esclavo oficial ya se encuentra compilado en tu proyecto:
* **Ruta del binario:** `/home/kaber420/Documentos/proyectos/esp-hosted-mcu/slave/build/network_adapter.bin`

#### Paso 2: Flashear el Firmware Puente en el P4
Cargar en el ESP32-P4 el firmware puente transparente (USB-Serial-JTAG ➔ UART1 en GPIO 32/28 a 115200 bps).

#### Paso 3: Grabar la Flash del C6 desde la PC
Con los jumpers colocados y el cable `C6_IO9` a `GND`, ejecutar desde tu terminal:
```bash
python -m esptool --chip esp32c6 -p /dev/ttyACM0 -b 115200 write_flash 0x0 /home/kaber420/Documentos/proyectos/esp-hosted-mcu/slave/build/network_adapter.bin
```

#### Paso 4: Finalización y Limpieza (¡Muy Importante!)
1. **Retirar el cable de `C6_IO9` a `GND`** (obligatorio para que el C6 pueda arrancar el firmware en modo normal).
2. **Retirar los 2 jumpers horizontales** (`GPIO 32` y `GPIO 28`).
3. Volver a flashear **CBDos v0.2.0** en el ESP32-P4:
   ```bash
   cd bsp/esp32_p4_jc4880 && idf.py -p /dev/ttyACM0 flash monitor
   ```
4. **¡Listo de por vida!** A partir de ese momento, el C6 responderá al bus SDIO interno sin cables ni jumpers adicionales.


---

## 4. Configuración en el Proyecto ESP32-P4 (Host)

### A. Dependencias (`main/idf_component.yml`)
```yaml
dependencies:
  espressif/esp_wifi_remote: "^1.3.0"
  espressif/esp_hosted: "^1.0.0"
  idf:
    version: ">=5.3.0"
```

### B. Configuración de Kconfig (`sdkconfig.defaults`)
```ini
# Wi-Fi Remote & ESP-Hosted SDIO (ESP32-C6 Slave)
CONFIG_ESP_WIFI_REMOTE_ENABLED=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y
CONFIG_ESP_HOSTED_ENABLED=y
CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y
CONFIG_ESP_HOSTED_IDF_SLAVE_TARGET="esp32c6"
CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ=40000
CONFIG_ESP_HOSTED_SDIO_PIN_CLK=18
CONFIG_ESP_HOSTED_SDIO_PIN_CMD=19
CONFIG_ESP_HOSTED_SDIO_PIN_D0=14
CONFIG_ESP_HOSTED_SDIO_PIN_D1=15
CONFIG_ESP_HOSTED_SDIO_PIN_D2=16
CONFIG_ESP_HOSTED_SDIO_PIN_D3=17
CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE=54
CONFIG_ESP_HOSTED_GPIO_SLAVE_RESET_SLAVE=54
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW=y
CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH=n
CONFIG_SDIO_RESET_ACTIVE_LOW=y
CONFIG_SDIO_RESET_ACTIVE_HIGH=n
```

### C. Buenas Prácticas de Arquitectura en CBDos
1. **Desacoplamiento del arranque:** No invocar `cbdos::network::init()` de manera síncrona/bloqueante en `app_main()` antes de iniciar la pantalla y LVGL. Si el esclavo no responde o no está alimentado, el sistema principal debe seguir funcionando al 100% con display, touch, audio y almacenamiento.
2. **Inicialización bajo demanda:** El subsistema de red se inicializa al ingresar a la pantalla de configuración de Wi-Fi o cuando el usuario solicita una conexión explícita.

---

## 5. Diagnóstico de Errores Comunes (Troubleshooting)

| Error en Log Serial | Causa Raíz | Solución |
| :--- | :--- | :--- |
| `send_scr returned 0xffffffff` / `sdio card init failed` | El ESP32-C6 no responde en el bus SDIO (está apagado, en reset continuo o sin firmware esclavo). | 1. Verificar que `RESET_ACTIVE_LOW=y` esté activo.<br>2. Flashear el firmware `esp-hosted-slave` en el C6. |
| `Task "sdio_read" should not return, Aborting now!` | El driver de `esp_hosted` aborta el hilo de FreeRTOS si la inicialización SDIO falla. | Desacoplar la inicialización de red del inicio del SO para evitar bucles de reinicio. |
| `Host WiFi Enable` no disponible | El ESP32-P4 no tiene hardware Wi-Fi nativo en el silicio. | Utilizar siempre `esp_wifi_remote` con backend `ESP-HOSTED`. |
