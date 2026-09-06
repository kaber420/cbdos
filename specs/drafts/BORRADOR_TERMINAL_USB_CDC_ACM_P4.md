# 📋 Borrador: Integración de USB-Serial CDC-ACM en Terminal y Consola de CBDos

## 1. Objetivo y Visión General
Permitir que el usuario, tras flashear o conectar un **ESP32-C3 / C6 / S3** mediante un cable **USB-C a USB-C** (o a través de un HUB USB) al ESP32-P4, pueda:
1. Abrir la aplicación **Terminal Serie** en CBDos (`SerialTerminalView`).
2. Conectarse de forma instantánea al canal **USB CDC-ACM** sin necesidad de cables UART/GPIO adicionales.
3. Visualizar en pantalla en tiempo real los mensajes de arranque (Banner, versión, modo de radio activo, canal, RSSI).
4. Enviar comandos interactivos en texto plano (ej: `channel 6`, `mode espnow`, `status`, `reboot`) mediante el teclado táctil integrado de CBDos.

---

## 2. Flujo de Experiencia de Usuario (UI Flow)

```text
[ Conectar Cable USB-C Directo o Hub USB con múltiples ESP32 ]
                 │
                 ▼
[ Abrir App Terminal Serie en CBDos ]
                 │
                 ▼
[ Selector "Puerto / Dispositivo": Menú Dinámico ]
  ├── 🔌 USB CDC 0 (/dev/ttyACM0 - C3 #1)
  ├── 🔌 USB CDC 1 (/dev/ttyACM1 - C3 #2)
  ├── 🔌 USB CDC 2 (/dev/ttyACM2 - C6 / H2)
  ├── 📌 JP1 Header (TX:32 RX:28)
  └── 📌 JP1 Header Alt (TX:50 RX:49)
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│ 📺 Pantalla Terminal CBDos (4.3" IPS @ 60 FPS)              │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ [Baud: 115200] [Puerto: USB CDC 0 (ACM0)] [⏸] [🗑] [💾]│ │
│ ├─────────────────────────────────────────────────────────┤ │
│ │ ========================================                │ │
│ │ 📡 ESP32-C3 RF Modem #1 (Node ID: 0x01)                 │ │
│ │ Mode: ESP-NOW (Canal 1 - 2412 MHz)                      │ │
│ │ MAC: 24:DC:C3:4A:12:F0                                  │ │
│ │ Status: Escuchando paquetes...                          │ │
│ │ ========================================                │ │
│ │ cmd>                                                    │ │
│ ├─────────────────────────────────────────────────────────┤ │
│ │ [ Entrada: channel 6                                 ]  │ │
│ │ [ Enviar ] [ ⌨ Teclado ] [ Ctrl+C ] [ Enter ]           │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Arquitectura Técnica de Software

### 3.1. Abstracción HAL y Enumeración Dinámica Multi-Puerto
Actualmente, `cbdos::uart` solo maneja el periférico UART físico por GPIO. Para mantener la **Ley de Pureza Arquitectónica de `core/`** (Zero Platform Pollution), la capa HAL expondrá una lista dinámica de puertos detectados:

```cpp
namespace cbdos {
namespace serial {

enum class PortType {
    UART_HARDWARE,
    USB_CDC_ACM
};

struct SerialDevice {
    uint8_t id;             // ID interno (0, 1, 2...)
    PortType type;
    std::string displayName;// ej. "USB CDC 0 (ACM0)", "USB CDC 1 (ACM1)", "JP1 (TX:32 RX:28)"
    std::string devicePath; // ej. "/dev/ttyACM0", "/dev/ttyACM1", "/dev/uart0"
    bool isConnected;       // Estado de conexión física
    int txPin;              // Solo UART (-1 si es USB)
    int rxPin;              // Solo UART (-1 si es USB)
};

// APIs del Subsistema Serial Multi-Dispositivo
std::vector<SerialDevice> getAvailableDevices();
bool open(uint8_t deviceId, uint32_t baudrate);
void close();
bool isOpen();
uint8_t getActiveDeviceId();
size_t available();
size_t read(uint8_t* buffer, size_t maxLen);
size_t write(const uint8_t* data, size_t len);
void flush();

} // namespace serial
} // namespace cbdos
```

### 3.2. Adaptador BSP en ESP32-P4 (`bsp/esp32_p4_jc4880`)
- Aprovecha la infraestructura existente en `usb_cdc_loader_port.cpp` y `espressif/usb_host_cdc_acm`.
- Implementa una cola FreeRTOS o RingBuffer dedicada para captura continua de caracteres recibidos desde el endpoint CDC-ACM sin bloquear el renderizado de LVGL.
- Detecta eventos de desconexión / reconexión USB en caliente.

### 3.3. Comportamiento en `SerialTerminalView` (Core LVGL 9.5)
- En el desplegable de **Presets**, se añade automáticamente:
  - `🔌 USB CDC (C3/C6/S3)` (Disponible cuando hay dispositivo USB enumerado).
  - `JP1 (TX:32 RX:28)` (Pines de expansión de cabecera).
  - `JP1 Alt (TX:50 RX:49)`.
- Cuando se selecciona `USB CDC`, el polling del timer de LVGL lee directamente del RingBuffer USB.

---

## 4. Firmware Ligero Interactivo para el ESP32-C3 (Módem RF / Bridge)

Para que la prueba sea inmediata sin necesidad de protocolos binarios complejos al inicio, el firmware del C3 implementará un CLI de comandos ASCII básico:

### Comandos de Texto Soportados:
1. **`status`**: Devuelve estado de radio, canal actual, potencia TX y conteo de paquetes.
2. **`mode <espnow|espnow_lr|ble|lora>`**: Conmuta la modulación activa de la radio.
3. **`channel <1-14>`**: Cambia el canal RF en caliente.
4. **`txpower <level>`**: Ajusta la potencia de transmisión (dBm).
5. **`send <texto|hex>`**: Emite un paquete crudo de prueba por el aire.
6. **`reboot`**: Reinicia el microcontrolador.

---

## 5. Plan de Implementación y Fases

| Fase | Tarea | Componente |
| :--- | :--- | :--- |
| **Fase 1** | Implementar RingBuffer de lectura/escritura bidireccional en `usb_cdc_loader_port.cpp` para modo terminal (no flasher). | `bsp/esp32_p4_jc4880` |
| **Fase 2** | Exponer la opción de puerto `USB CDC` en la capa HAL agnóstica de `cbdos::uart` / `cbdos::serial`. | `core/include/cbdos/` |
| **Fase 3** | Añadir el preset en `SerialTerminalView.cpp` para que el usuario pueda alternar entre UART y USB CDC desde la UI. | `core/src/ui/views/` |
| **Fase 4** | Crear un firmware de ejemplo para ESP32-C3 (`c3_rf_bridge_cli.bin`) listo para flashear desde la MicroSD de CBDos. | Herramientas / Firmware |
