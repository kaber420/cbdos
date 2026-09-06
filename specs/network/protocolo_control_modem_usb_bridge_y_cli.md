# Protocolo de Control del Módem USB Bridge (ESP32-C3) y CLI Interactiva

Este documento especifica los dos métodos de comunicación y control soportados por el firmware del **Módem / Dongle ESP32-C3** (`tools/espnow_usb_bridge/`), tanto para control automatizado por software (**ESP32-P4 / CBDos / Gateway Python**) como para administración manual por humanos (**CLI Serial**).

---

## 🛰️ 1. Arquitectura de Control Dual (Dual-Stack)

El módem cuenta con un multiplexor de entrada en su puerto nativo USB-Serial/JTAG que discrimina automáticamente entre tramas binarias estructuradas y texto ASCII plano:

```
                  ┌──────────────────────────────────────────────┐
                  │       ESP32-C3 USB Virtual COM Port          │
                  └──────────────────────┬───────────────────────┘
                                         │
                         ¿Inicia con 0xAA 0x55?
                                        / \
                                  SÍ   /   \  NO (Texto ASCII)
                                      /     \
                                     ▼       ▼
                    ┌──────────────────┐   ┌──────────────────┐
                    │ Protocolo Binario│   │ CLI Interactiva  │
                    │   Framing CRC8   │   │   Modo Humano    │
                    └──────────────────┘   └──────────────────┘
```

---

## ⚡ 2. Forma A: Protocolo Binario Enmarcado (Machine-to-Machine)

Diseñado para comunicación de alta velocidad entre el **ESP32-P4 (USB Host CDC-ACM)** o el **Gateway Python** y el módem C3.

### 2.1. Estructura de la Trama Serial

```
┌───────────┬───────────┬───────────┬───────────┬───────────┬──────────────────────┬───────────┐
│ MAGIC_0   │ MAGIC_1   │ DIRECCIÓN │ LEN_HIGH  │ LEN_LOW   │ PAYLOAD              │ CRC8      │
│  (0xAA)   │  (0x55)   │  (1 Byte) │  (1 Byte) │  (1 Byte) │ (N Bytes, máx 260B)  │ (1 Byte)  │
└───────────┴───────────┴───────────┴───────────┴───────────┴──────────────────────┴───────────┘
```

* **Magic Bytes:** `0xAA 0x55` (sincronización de inicio de trama).
* **Direcciones (`DIR`):**
  * `0x01` (`DIR_PC_TO_DONGLE`): El host envía un paquete para ser emitido por radio ESP-NOW al aire.
  * `0x02` (`DIR_DONGLE_TO_PC`): El dongle recibió un paquete del aire (incluye MAC origen y RSSI) y lo entrega al host.
  * `0x03` (`DIR_CTRL_CMD`): Comando de control de hardware enviado desde el host al dongle.
  * `0x04` (`DIR_CTRL_RESP`): Respuesta de control y ACK enviada desde el dongle al host.
* **Longitud (`LEN`):** Big-Endian de 16 bits que indica la longitud del Payload.
* **CRC8:** Polinomio `0x8C` calculado sobre el contenido del Payload.

---

### 2.2. Tabla de Comandos de Control Binario (`DIR_CTRL_CMD = 0x03`)

| Código Hex | Macro | Payload de Entrada (Host ➔ C3) | Payload de Respuesta (C3 ➔ Host) |
| :---: | :--- | :--- | :--- |
| `0x01` | `RADIO_CMD_GET_STATUS` | `[0x01]` | `[0x01][STATUS][MAC 6B][MODO 1B][CANAL 1B][POT 1B][PEERS 1B][ALIAS...]` |
| `0x02` | `RADIO_CMD_SET_MODE` | `[0x02][0x01=Normal \| 0x02=LR]` | `[0x02][STATUS][MODO_ACTUAL 1B]` |
| `0x03` | `RADIO_CMD_SET_CHAN` | `[0x03][Canal 1..13]` | `[0x03][STATUS][CANAL_ACTUAL 1B]` |
| `0x04` | `RADIO_CMD_SET_POWER` | `[0x04][Potencia 1..84]` | `[0x04][STATUS][POT_ACTUAL 1B]` |
| `0x05` | `RADIO_CMD_SCAN_PEERS` | `[0x05]` | `[0x05][STATUS][N_PEERS 1B][Tabla de Peers...]` |
| `0x06` | `RADIO_CMD_SET_ALIAS` | `[0x06][String Alias UTF-8]` | `[0x06][STATUS][String Alias Confirmado]` |

* `STATUS`: `0x00` = `RADIO_STATUS_OK`, `0xFF` = `RADIO_STATUS_ERR`.

---

### 2.3. Desglose Detallado de `RADIO_CMD_GET_STATUS` (`0x01`)

Cuando el Host envía `0xAA 0x55 0x03 0x00 0x01 0x01 0xA2`, el C3 responde con `DIR_CTRL_RESP (0x04)`:

```
Payload de Respuesta:
Byte 0:       0x01 (Comando GET_STATUS)
Byte 1:       0x00 (Status OK)
Bytes 2..7:   MAC Address (6 Bytes, ej: 9C:CC:01:7C:0C:94)
Byte 8:       Modo de Radio (0x01 = Normal 802.11 b/g/n, 0x02 = Long Range LR)
Byte 9:       Canal Wi-Fi actual (1 al 13)
Byte 10:      Potencia de Transmisión (1 al 84, donde 84 = +21.0 dBm)
Byte 11:      Cantidad de Nodos Vecinos vistos en memoria (0 a 16)
Bytes 12..N:  String de Alias del Nodo (guardado en NVS flash, ej: "PoP1a", "PoP1b")
```

---

## 🖥️ 3. Forma B: Consola de Texto Interactiva / CLI (Modo Humano)

Al abrir el puerto `/dev/ttyACM*` desde cualquier terminal serie (`minicom`, `screen`, `idf.py monitor`, Serial Monitor de Arduino o VSCode) a **115200 baudios**, puedes escribir comandos de texto plano directamente.

### 3.1. Lista de Comandos CLI

#### `help` o `?`
Muestra el menú de ayuda con todos los comandos disponibles.

#### `status`
Muestra el resumen completo del hardware, identidad, canal y estadísticas:
```text
--- ESTADO DEL DONGLE GATEWAY ---
Nodo Alias:      PoP1a
Modo Radio:      ESP-NOW ESTÁNDAR (802.11 b/g/n) ⚡
Canal Activo:    Canal 1 (2412 + 0 MHz)
Potencia TX:     21.00 dBm (84/84)
MAC Propia:      9C:CC:01:7C:0C:94
Sniffer en Vivo: DESHABILITADO
Estadísticas:    RX: 0 paq | TX: 0 paq | Nodos vistos: 0
Tiempo Activo:   45 seg
```

#### `node <nuevo_alias>` (o `alias <nuevo_alias>`)
Asigna un nombre legible al nodo y lo **persiste en la memoria NVS flash**:
```text
node PoP1b
--> ✅ Nombre de nodo guardado en NVS: 'PoP1b'
```

#### `mode [normal|lr]`
Cambia entre modulación Wi-Fi estándar y modulación Long Range (alcance kilométrico):
* `mode lr` ➔ `✅ Modo cambiado a: ESP-NOW LONG RANGE (LR)`
* `mode normal` ➔ `✅ Modo cambiado a: ESP-NOW ESTANDAR (802.11 b/g/n)`

#### `channel <1-13>`
Cambia el canal de operación de radio de forma inmediata:
* `channel 6` ➔ `✅ Canal cambiado a: 6`

#### `power <1-84>`
Ajusta la potencia de transmisión (1 a 84, donde 84 representa 20.0 - 21.0 dBm):
* `power 84` ➔ `✅ Potencia TX cambiada a: 21.00 dBm (84/84)`

#### `peers`
Muestra la tabla formateada de nodos descubiertos por el aire con sus niveles de señal RSSI:
```text
┌──────────┬───────────────────┬──────────────┬──────────────┬─────────────┬──────────────────────────┐
│ Short ID │    Direccion MAC  │ RSSI (Senal) │ Ultimo Visto │ Total Paq   │ Ultima Peticion / URL    │
├──────────┼───────────────────┼──────────────┼──────────────┼─────────────┼──────────────────────────┤
│  0x0042  │ A4:C1:38:12:34:56 │  -54dBm 🟢   │      hace 2s │     14 pkts │ index.mesh               │
└──────────┴───────────────────┴──────────────┴──────────────┴─────────────┴──────────────────────────┘
```

#### `sniff [on|off]`
Activa o desactiva la visualización de paquetes crudos capturados en la consola serie.

#### `ping`
Emite un pulso broadcast de radio al aire para medir conectividad con nodos vecinos.

#### `reboot`
Reinicia el módem de forma controlada por software.

---

## 🔄 4. Resumen de Casos de Uso

| Escenario | Método de Comunicación | Herramienta |
| :--- | :--- | :--- |
| **Depuración rápida / Configuración inicial de nodo** | **CLI de Texto** | Terminal serial (`minicom`, PuTTY, monitor) |
| **Servidor Gateway Multi-Módem en PC** | **Protocolo Binario** | `tools/tlvgl_gateway/serial_transport.py` |
| **Control Autónomo en ESP32-P4 (CBDos)** | **Protocolo Binario** | Driver USB Host CDC-ACM (`bsp/esp32_p4_jc4880`) |
| **Auto-descubrimiento de múltiples nodos en Hub USB** | **`RADIO_CMD_GET_STATUS`** | `scan_all_dongles()` / `MultiDongleManager` |
