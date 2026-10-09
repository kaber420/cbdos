# 🛰️ Especificación Técnica: Protocolo de Consola Estilo AT/SCPI y Direccionamiento Multidispositivo en Hubs USB

**Fecha:** 2026-10-09  
**Estado:** Aprobado y Estandarizado  
**Versión:** `v1.0.0`  
**Ubicación:** `specs/architecture/especificacion_protocolo_consola_at_scpi_y_direccionamiento_hub.md`  

---

## 🎯 1. Principios de Diseño y Filosofía

CBDos adopta una sintaxis de consola interactiva y máquina a máquina (M2M) basada en los estándares industriales de **telecomunicaciones (módems AT V.250 / 3GPP)** e **instrumentación de laboratorio (SCPI IEEE-488.2)**.

### Ventajas Técnicas:
1. **Determinismo y Cero Ambigüedad:** El uso del delimitador de dos puntos (`:`) permite a parsers de hardware, scripts remotos y operadores separar instantáneamente el subsistema, el identificador físico de dispositivo y el payload de comando sin necesidad de heurísticas ni suposiciones.
2. **Direccionamiento Jerárquico en Hubs USB:** Permite controlar de forma aislada múltiples dongles o coprocesadores idénticos (ej. dos o más ESP32-C3 o C6) conectados a través de un concentrador USB.
3. **Agnóstico al Medio de Transporte:** El mismo comando funciona idénticamente por cable serie (`/dev/ttyACM0`), por Bluetooth, por interfaz WebSerial o empaquetado en tramas de radioenlace por el aire (ESP-NOW / LoRa Mesh).

---

## 📐 2. Gramática Formal del Protocolo

```text
[<subsistema>[:<id>]:] <comando> [flags o argumentos]
```

Donde:
* `<subsistema>`: Nombre del subsistema o tipo de periférico (`sys`, `usb`, `c3`, `c6`, `radio`, `tts`, `ducky`).
* `[:<id>]`: *(Opcional)* Índice de instancia o número de puerto físico del concentrador USB (`0`, `1`, `2`, ...). Si se omite, se asume el dispositivo primario o activo.
* `<comando>`: Verbo de acción o consulta (`status`, `ping`, `temp`, `list`, `info`, `reboot`).
* `[flags]`: Modificadores universales (`-c <N>`, `-i <T>`, `-t <T>`).

---

## 🔌 3. Catálogo de Subsistemas y Direccionamiento

### 3.1 Subsistema Central del Sistema Operativo (`sys:`)
Métricas, telemetría y control del procesador anfitrión (ESP32-P4 / ESP32-S3):

| Comando | Alias Rápido | Propósito | Salida / Ejemplo |
| :--- | :--- | :--- | :--- |
| `sys: status` | `status` | Reporte completo de CPU Temp, Heap, PSRAM y Uptime | Bloque `[SYS_STATUS]` |
| `sys: temp [-c N] [-i T]` | `temp` | Medición de temperatura del SoC con ráfagas | `[SYS] CPU Temp: XX.X °C` |
| `sys: mem` | `mem` | Memoria libre y total en Heap y PSRAM | `[SYS] RAM: XX KB \| PSRAM: XX MB` |
| `sys: info` | `info` | Identidad de placa, SoC y versión de CBDos | Bloque `[SYS_INFO]` |
| `sys: uptime` | `uptime` | Tiempo transcurrido desde el encendido | `[SYS] Uptime: Xh Ym Zs` |
| `sys: reboot` | `reboot` | Reinicio controlado por software del SoC | - |

---

### 3.2 Subsistema de Concentrador y Puertos USB Host (`usb:`)
Administración del bus USB OTG High-Speed y enumeración de periféricos:

| Comando | Propósito | Comportamiento |
| :--- | :--- | :--- |
| `usb: list` / `usb: ls` | Listar topología del bus USB | Muestra el concentrador Hub y todos los dispositivos conectados con su puerto, VID:PID, Fabricante, Producto y MAC Serial |
| `usb: info` | Estado del dispositivo activo | Muestra descriptores detallados del periférico en uso |
| `usb:<id>: info` | Estado del dispositivo en puerto `<id>` | Consulta directa a un puerto específico del Hub (ej. `usb:1: info`, `usb:2: info`) |

#### Formato de Salida de `usb: list`:
```text
[USB_TOPOLOGY]
  Bus 01: High-Speed USB Host Controller (ESP32-P4 OTG PHY)
  |__ Hub: 4-Port High-Speed Hub [VID: 0x05E3, PID: 0x0610]
      |__ Port 1: [0x303A:0x1001] "ESP32-C3 Radio Modem" (MAC: 84:F7:03:1A:22:10) -> dev_id=1
      |__ Port 2: [0x303A:0x1001] "ESP32-C3 Sniffer"     (MAC: 84:F7:03:99:40:12) -> dev_id=2
      |__ Port 3: [0x1546:0x01A8] "u-blox 7 GPS Module"   (CDC ACM)                 -> dev_id=3
      |__ Port 4: [Vacío]
```

---

### 3.3 Coprocesadores de Radio ESP32-C3 (`c3:`)
Control y diagnóstico de módems de radio USB ESP-NOW:

* **Dispositivo único o predeterminado:**
  * `c3: status`: Diagnóstico del enlace USB y lectura del estado del módem.
  * `c3: probe`: Detección en caliente del dispositivo en el bus.
  * `c3: ping [-c N] [-i T] [-t T]`: Envío de tramas de prueba RF al aire con reporte de RTT.
* **Direccionamiento en concentrador (Múltiples C3):**
  * `c3:1: ping`: Envía paquete de radio específicamente a través del C3 conectado en el **Puerto 1**.
  * `c3:2: ping`: Envía paquete de radio específicamente a través del C3 conectado en el **Puerto 2**.
  * `c3:2: status`: Lee la MAC, canal y potencia de transmisión del C3 en el **Puerto 2**.

---

### 3.4 Síntesis de Voz y BadUSB (`tts:`, `ducky:`)
* `tts: <texto>`: Síntesis de voz offline en altavoz mediante PicoTTS.
* `ducky: <script>`: Ejecución de scripts DuckyScript v2 por el puerto USB HID.

---

### 3.5 Consultas Críticas WebFlasher y Queries (`?`)
* `CBDOS:VERSION?`: Query de identidad de tablilla para el flasheador web WebSerial.
* `CBDOS:BOOTLOADER`: Reinicio inmediato al modo ROM bootloader para actualización de firmware.

---

## 🛡️ 4. Reglas de Control, Seguridad e Invariantes

1. **Zero-Polling por Defecto:** Ningún subsistema genera tráfico push cíclico no solicitado a la consola. La telemetría es estrictamente bajo demanda (Request-Response).
2. **Cancelación Interactiva:** Durante ráfagas de paquetes o mediciones (`-c N` con `N > 1`), cualquier pulsación de tecla (`Ctrl+C`, `q`, `ESC`, `Enter`) interrumpe inmediatamente el ciclo y devuelve el control al prompt `cbdos> `.
3. **Tolerancia a Formatos de Salto de Línea:** El receptor maneja transparentemente terminaciones `\r` (CR), `\n` (LF) y secuencias compuestas `\r\n` (CRLF) sin interpretar caracteres de nueva línea como comandos vacíos ni disparadores falsos de aborto.
4. **Fallback Transparente a Lua++:** Cualquier línea de entrada que no coincida con un prefijo de subsistema registrado ni con un alias directo es derivada automáticamente al motor de ejecución interactivo Lua++.
