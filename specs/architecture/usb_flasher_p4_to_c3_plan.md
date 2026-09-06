# 📱 Plan Maestro: Flasheador Autónomo USB-C en Campo (ESP32-P4 -> ESP32-C3)

**Objetivo:** Permitir que el CyberDeck **ESP32-P4 (JC4880P443C)** funcione como un programador de campo autónomo (*standalone field programmer*), capaz de flashear microcontroladores **ESP32-C3 / S3 / C6** conectando un simple cable Tipo-C a Tipo-C entre el puerto USB Host del P4 y el microcontrolador objetivo, sin necesidad de usar una PC/laptop.

---

## 🔍 1. Estado Actual vs. Lo Que Falta

| Componente | Estado Actual | Lo que se necesita |
| :--- | :--- | :--- |
| **Interfaz de Usuario (`FlasherView`)** | ✅ Implementada en LVGL 9 con selector de presets, barra de progreso y selección de `.bin`. | Conectar el backend USB Host con los eventos de conexión/desconexión del cable. |
| **Driver de Flasheo (`esp-serial-flasher`)** | ✅ Integrado para UART. | Implementar el transporte `loader_port_usb_cdc` para `esp-serial-flasher`. |
| **Stack USB Host en ESP32-P4** | ⚠️ Componentes presentes pero no inicializados como Host CDC. | Integrar `espressif/usb_host_cdc_acm` y crear la tarea de enumeración USB Host. |
| **Control de Reset / Bootloader (DTR/RTS)** | ⚠️ Solo por pines físicos GPIO. | Control de líneas virtuales DTR/RTS por comandos USB CDC (`cdc_acm_host_set_control_line_state`). |

---

## 🛠️ 2. Arquitectura del Flasheo USB-C Nativo

```
┌────────────────────────────────────────────────────────────────────────┐
│                   ESP32-P4 (HOST PROGRAMADOR - CBDos)                  │
│                                                                        │
│   [ MicroSD: /sdcard/firmware.bin ]                                    │
│                   │                                                    │
│                   ▼                                                    │
│   [ FlasherView / FlasherServiceP4 ]                                   │
│                   │                                                    │
│                   ▼                                                    │
│   [ esp-serial-flasher (Loader Protocol) ]                             │
│                   │                                                    │
│                   ▼                                                    │
│   [ USB Host CDC-ACM Driver (USB-Serial/JTAG) ]                        │
│                   │                                                    │
│                   ▼                                                    │
│        Puerto USB-C (USB 2.0 OTG Host)                                 │
└───────────────────┬────────────────────────────────────────────────────┘
                    │  Cable USB Tipo-C a Tipo-C
                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│               ESP32-C3 / S3 / C6 (TARGET EN CAMPO)                     │
│                                                                        │
│        Puerto USB-C (USB-Serial/JTAG de Fábrica)                       │
│                   │                                                    │
│                   ▼                                                    │
│   [ ROM Bootloader (Recibe DTR/RTS y escribe en Flash) ]               │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 📋 3. Pasos de Implementación

### Paso 1: Configuración de Dependencias USB Host en ESP-IDF
* Agregar a `bsp/esp32_p4_jc4880/main/idf_component.yml`:
  ```yaml
  espressif/usb_host_cdc_acm: "^1.3.0"
  ```
* Habilitar en `sdkconfig`:
  - `CONFIG_USB_HOST_ENABLED=y`
  - `CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=512`

---

### Paso 2: Implementación de la Capa de Transporte USB para `esp-serial-flasher`
Crear `bsp/esp32_p4_jc4880/hal/usb_cdc_loader_port.cpp`:
1. Inicializar el driver `usb_host_install()`.
2. Registrar el driver `cdc_acm_host_install()`.
3. Detectar la conexión del dispositivo (Vendor ID `0x303A` de Espressif - USB Serial JTAG).
4. Implementar los callbacks de lectura/escritura de `loader_port`:
   - `loader_port_usb_write(data, size, timeout)`
   - `loader_port_usb_read(data, size, timeout)`
5. Implementar el control de Reset y Bootloader por DTR/RTS virtual USB:
   ```c
   // Entrar en Bootloader por USB:
   // RTS=1, DTR=0 (Reset activo) -> RTS=0, DTR=1 (Bootloader activo)
   cdc_acm_host_set_control_line_state(cdc_dev, true, false);
   vTaskDelay(pdMS_TO_TICKS(100));
   cdc_acm_host_set_control_line_state(cdc_dev, false, true);
   ```

---

### Paso 3: Integración en `hal_flasher_p4.cpp`
* Conectar el preset **`🔌 USB-Serial Nativo (Puerto USB-C ESP32-C3/S3)`** para que active el transporte USB CDC en vez del puerto UART físico.
* Emitir telemetría visual a `FlasherView`:
  - *"Esperando conexión de cable USB-C..."*
  - *"ESP32-C3 detectado por USB. Sincronizando Bootloader..."*
  - *"Escribiendo Flash: 45%..."*
  - *"¡Flasheo completado! Reiniciando microcontrolador..."*

---

## 🧪 4. Plan de Verificación en Hardware

1. Compilar el firmware del P4 con `idf.py build`.
2. Flashear el P4 y verificar el arranque de CBDos.
3. Copiar el archivo `espnow_usb_bridge_c3.bin` en `/sdcard/`.
4. Conectar un **ESP32-C3 nuevo** mediante el cable **Tipo-C a Tipo-C** al puerto USB del P4.
5. Abrir la app **Flasher** en el P4, pulsar **[ Flashear ]** y verificar que el C3 se programe al 100% y comience a emitir por ESP-NOW.
