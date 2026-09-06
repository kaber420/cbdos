# Plan Técnico: Purga de Polling Zombie, Liberación de USB OTG y Preservación de PTMP en ESP32-P4

**Fecha:** 4 de Septiembre de 2026  
**Autor:** Antigravity AI & kaber420  
**Target:** ESP32-P4 JC4880P443C & ESP32-S3 JC3248  
**Ubicación:** `docs/drafts/PLAN_PURGA_POLLING_ZOMBIE_Y_TRANSPORTE_PTMP_P4.md`  

---

## 1. Contexto y Objetivos

### El Ecosistema PTMP de CBDos (Point-to-Multipoint)
El sistema operativo CBDos cuenta con su propio protocolo de red ad-hoc nativo en `core/src/mesh/MeshEngine.cpp`:
* **Descubrimiento de Torres:** Sincronización horaria (`sendTowerProbe()`, balizas de época).
* **Navegación Alternet:** Peticiones y renderizado de páginas TLVGL por el aire.
* **Ruteo Descentralizado:** Enrutamiento por saltos (TTL, reensamblado de micro-fragmentos y Pseudo-ARP).
* **Estado en S3:** Opera de forma nativa e integrada en capa física ESP-NOW (`bsp/esp32_s3_jc3248/hal/hal_mesh_s3.cpp`).

### El Problema Identificado en ESP32-P4
En el ESP32-P4, la capa de abstracción física (`bsp/esp32_p4_jc4880/hal/hal_mesh_p4.cpp`) arrastraba un código zombie del mockup preliminar de radio:
1. **Llamada de arranque forzada:** `initMeshTransportP4()` ejecutaba síncronamente `s_usbRadioTransport.init(1)` durante el booteo en `main.cpp`, violando la **Regla 7 (Offline-First)**.
2. **Polling Ciego Ininterrumpido:** Levantaba la tarea `usb_radio_rx` que ejecutaba un bucle cada 2000 ms enviando la trama de 7 bytes `RADIO_CMD_GET_STATUS` (`loader_port_write TX size=7 err=0`).
3. **Secuestro de USB CDC:** La tarea capturaba el bus USB CDC en exclusiva y consumía todos los datos entrantes en `cdc_rx_callback`.
4. **Bloqueo del Terminal Serial:** La aplicación `SerialTerminalView` (`P4UsbOtgPort`) no podía abrir el dispositivo ni recibir caracteres de hardware externo conectado (ESP32-S3, routers, consolas).

---

## 2. Arquitectura de la Solución

```
┌────────────────────────────────────────────────────────────────────────┐
│                        NÚCLEO AGNOSTICO (core/)                        │
│                                                                        │
│   [ MeshEngine.cpp ]                                                   │
│   • Protocolo PTMP (Torres / Balizas / TLVGL / Epoch)                  │
│   • Totalmente agnóstico a la plataforma (INALTERADO)                  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                   CAPA DE SOPORTE HARDWARE P4 (bsp/)                   │
│                                                                        │
│  [ hal_mesh_p4.cpp ]                       [ hal_uart_p4.cpp ]         │
│  • Purgado de polling zombie (2s)          • P4UsbOtgPort canónico     │
│  • Cero secuestro en arranque              • DTR=true / RTS=false      │
│  • Modo pasivo / bajo demanda              • Línea de datos limpia     │
│  • Respeta Regla 7 (Offline-First)         • Consola serie funcional   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                     PUERTO FÍSICO USB OTG HIGH-SPEED                   │
│                     (JC4880P443C - 480x800 MIPI-DPI)                   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Modificaciones Quirúrgicas Detalladas

### A. Archivo [`bsp/esp32_p4_jc4880/hal/hal_mesh_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_mesh_p4.cpp)

1. **Eliminar el polling infinito de 2 segundos en `rxTaskLoop()`:**
   - Suprimir la comprobación `if (!m_macValid)` que ejecuta `queryModemStatus()` periódicamente.
   - La tarea de recepción (si se inicia) solo escuchará paquetes entrantes pasivamente, sin emitir tráfico basura al bus USB.
2. **Eliminar la inicialización síncrona en el arranque:**
   - En `initMeshTransportP4()`, eliminar `s_usbRadioTransport.init(1);`.
   - El transporte permanece registrado en `MeshEngine` y en `NetworkInterfaceManager`, pero en reposo (`ready = false`), hasta que una aplicación o servicio solicite explícitamente encender la radio (`setMode()`).

### B. Archivo [`bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp)

1. **Activación Canónica de Señales de Control en `P4UsbOtgPort::open()`:**
   - Configurar el baudrate solicitado con `setBaudrate(config.baudrate)`.
   - Activar **`DTR=true, RTS=false`** con `cdc_acm_host_set_control_line_state(m_cdcDev, true, false)`.
   - Esto indica al microcontrolador/router externo que una terminal está abierta y lista para recibir su flujo de salida.
2. **Control de Reset en `P4UsbOtgPort::pulseControlPin()`:**
   - Generar el pulso físico de RTS para resetear el chip externo (`DTR=false, RTS=true`).
   - Tras el retardo, restaurar **`DTR=true, RTS=false`** para capturar inmediatamente el banner de arranque del dispositivo.

---

## 4. Plan de Compilación y Validación

### Paso 1: Verificación Multi-Target
1. Compilar ESP32-P4 (IDF 5.5):
   ```bash
   bash -c '. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build'
   ```
2. Compilar ESP32-S3 (PlatformIO):
   ```bash
   pio run -d bsp/esp32_s3_jc3248
   ```

### Paso 2: Flasheo y Monitoreo en Caliente
1. Flashear ESP32-P4 por `/dev/ttyACM0`:
   ```bash
   idf.py -p /dev/ttyACM0 flash
   ```
2. Inspección por consola serial:
   - Comprobar que cesó al 100% la ráfaga de `USB_LOADER_PORT: loader_port_write TX size=7`.
   - Enviar `usb: status`: Debe retornar el hardware detectado en USB OTG sin interferencias.

### Paso 3: Validación en Pantalla Táctil
1. Ingresar a la aplicación **Terminal Serial** en la interfaz táctil del P4.
2. Seleccionar el puerto `🔌 USB` detectado y presionar `[ Conectar ]`.
3. Presionar el botón `[ ⚡ RST ]`:
   - El chip conectado (ESP32-S3 u otro microcontrolador) se reinicia por hardware.
   - El texto del bootloader y los logs de salida fluyen en vivo hacia la consola táctil.
