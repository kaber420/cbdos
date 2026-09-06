# Borrador Técnico: Liberación del Bus USB OTG y Activación Canónica de DTR en Terminal Serial P4

**Fecha:** 4 de Septiembre de 2026  
**Autor:** Antigravity AI & kaber420  
**Target Principal:** ESP32-P4 JC4880P443C (Módulo JC-ESP32P4-M3 Rev 1.3)  
**Estado:** Propuesta técnica para revisión y autorización  

---

## 1. Diagnóstico del Problema y Evidencia en Caliente

Durante las pruebas de conexión con hardware externo conectado al puerto USB OTG Host del JC4880P443C, se observó que la aplicación `SerialTerminalView` no recibía tráfico de entrada, a pesar de que el monitor serie `/dev/ttyACM0` mostraba actividad continua.

Al conectar con la consola de depuración en caliente (`ENABLE_CBDOS_SERIAL_DEBUG_CLI`) sobre `/dev/ttyACM0` e inspeccionar el estado del subsistema USB:
```text
[SERIAL_CLI_IN] usb: status
[SERIAL_CLI_OUT] 🔌 Puerto USB OTG Libre: Ningún dispositivo conectado físicamente.

I (293787) USB_LOADER_PORT: cdc_rx_callback recibido 0 bytes
I (293788) USB_LOADER_PORT: cdc_rx_callback recibido 1 bytes
I (293788) USB_LOADER_PORT: cdc_rx_callback recibido 6 bytes
I (293788) USB_LOADER_PORT: loader_port_write TX size=7 err=0
I (295802) USB_LOADER_PORT: cdc_rx_callback recibido 0 bytes
I (295803) USB_LOADER_PORT: cdc_rx_callback recibido 1 bytes
I (295803) USB_LOADER_PORT: cdc_rx_callback recibido 6 bytes
I (295803) USB_LOADER_PORT: loader_port_write TX size=7 err=0
```

### Hallazgos de Causa Raíz:

1. **Secuestro de Bus USB por `UsbCdcRadioTransport` al Arranque:**
   - En `bsp/esp32_p4_jc4880/hal/hal_mesh_p4.cpp`, la función `initMeshTransportP4()` llamaba a `s_usbRadioTransport.init(1);` de forma síncrona en `app_main()`.
   - `s_usbRadioTransport.init(1)` ejecutaba inmediatamente `loader_port_usb_cdc_init(1500)`.
   - Esto instaló el driver `cdc_acm_host` con callback de dispositivo nulo (`.new_dev_cb = NULL`) antes de que `UsbDeviceManager::init()` tuviera oportunidad de registrarse.
   - Además, abrió el dispositivo USB (`0x303A:0x1001`) y levantó la tarea FreeRTOS `usb_radio_rx`, la cual envía cada 2000 ms un paquete binario de 7 bytes (`loader_port_write TX size=7`) y consume todos los bytes de retorno en su callback privado `cdc_rx_callback`.
   - Esta acción violó la **Regla 7 (Offline-First Estricto)** al arrancar interfaces de radio/malla sin demanda explícita del usuario, e impidió que `SerialTerminalView` pudiera abrir o leer el dispositivo USB.

2. **Omisión de Línea de Control DTR y Baudrate en `P4UsbOtgPort`:**
   - En `bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`, el método `P4UsbOtgPort::open()` dejaba el estado de control en `cdc_acm_host_set_control_line_state(m_cdcDev, false, false)` y no establecía `line_coding` vía `setBaudrate(config.baudrate)`.
   - Los dispositivos USB-Serial modernos (ESP32 USB-Serial-JTAG, CP210x, FTDI, CH340) requieren **DTR=HIGH (true)** para habilitar la transmisión continua de datos hacia el host; en caso contrario, descartan o detienen el flujo saliente.

---

## 2. Modificaciones Técnicas Propuestas

### A. Desacoplamiento de Radio en Arranque (`bsp/esp32_p4_jc4880/hal/hal_mesh_p4.cpp`)
Se elimina la inicialización automática al arranque. El transporte queda registrado en el gestor de interfaces de red y en `MeshEngine`, pero en estado inactivo (`Off` / no inicializado). Solo se inicializará bajo demanda cuando la aplicación de Radio o Malla configure su modo (`setMode()`):

```cpp
void initMeshTransportP4() {
    mesh::MeshEngine::getInstance().setTransport(&s_usbRadioTransport);
    network::NetworkInterfaceManager::getInstance().registerInterface(2, &s_usbRadioTransport);
    // s_usbRadioTransport.init(1);  <-- ELIMINADO: Cumple Regla 7 Offline-First y libera USB OTG
}
```

### B. Canonicidad de Terminal USB CDC en `bsp/esp32_p4_jc4880/hal/hal_uart_p4.cpp`

1. **Configuración de Baudrate y Señal DTR en `open()`:**
```cpp
    // Aplicar baudrate solicitado por la configuración del terminal
    setBaudrate(config.baudrate);

    // Activar DTR (Data Terminal Ready) y mantener RTS inactivo (no reset)
    cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
    m_isOpen = true;
    ESP_LOGI(TAG_SERIAL, "Puerto USB OTG abierto con VID:0x%04X PID:0x%04X (DTR=1, RTS=0)", 
             devInfo->vid, devInfo->pid);
```

2. **Manejo del Pulso de Reset `[ ⚡ RST ]` en `pulseControlPin()`:**
```cpp
    bool pulseControlPin(uint32_t durationMs) override {
        if (!m_isOpen || !m_cdcDev) return false;
        // RTS=1 activa el reset físico del chip conectado
        cdc_acm_host_set_control_line_state(m_cdcDev, false, true);
        vTaskDelay(pdMS_TO_TICKS(durationMs));
        // RTS=0 libera el reset; DTR=1 mantiene viva la sesión del terminal
        cdc_acm_host_set_control_line_state(m_cdcDev, true, false);
        ESP_LOGI(TAG_SERIAL, "Pulso de reset enviado por USB OTG (RTS)");
        return true;
    }
```

---

## 3. Plan de Validación y Pruebas

1. **Compilación Multi-Target Obligatoria (Regla 2):**
   - **ESP32-P4:** `. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build`
   - **ESP32-S3:** `pio run -d bsp/esp32_s3_jc3248`
2. **Flasheo al ESP32-P4:**
   - `idf.py -p /dev/ttyACM0 flash`
3. **Verificación en Caliente vía Serial (`/dev/ttyACM0`):**
   - Ejecutar `usb: status`: Debe retornar los descriptores legítimos del dispositivo USB conectado (`VID`, `PID`, `Clase`, `Producto`).
   - Comprobar que no existe tráfico espurio ni spam de `USB_LOADER_PORT: loader_port_write TX size=7`.
4. **Verificación en UI:**
   - Ingresar a `SerialTerminalView`.
   - Seleccionar el puerto `🔌 USB: <dispositivo>`.
   - Conectar y pulsar `[ ⚡ RST ]`.
   - Confirmar recepción de los logs de arranque del dispositivo en la consola táctil.
