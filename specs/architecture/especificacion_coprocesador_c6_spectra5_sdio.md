# 📡 Especificación Técnica: Coprocesador de Radio ESP32-C6 sobre SDIO (Motor Spectra5)

> **Estado:** Aprobado / Especificación de Arquitectura de Radio  
> **Fecha:** 21 de Octubre de 2026  
> **Target Primario:** Guition JC4880P443C (ESP32-P4 RISC-V @ 400 MHz + ESP32-C6 U3 por SDIO)  
> **Sistema de Referencia Auditado:** `references/Slave_I` (`platforms/c6/components/spectra5_offensive/`, `hosted_radio_engine.cpp`, `c6_updater.cpp`)

---

## 1. Visión General del Subsistema de Radio

La placa oficial de **CBDos** (**Guition JC4880P443C**) integra físicamente un procesador host de alta potencia (**ESP32-P4**) sin radio integrada, complementado por un módulo **ESP32-C6 (U3)** soldado en el circuito impreso.

Para maximizar el rendimiento del sistema y evitar colapsar la CPU principal con interrupciones de radio de microsegundos, se adopta el modelo de coprocesamiento de **Slave I / Spectra5**:
* El **ESP32-P4** ejecuta la interfaz de usuario en LVGL v9.5, la lógica de aplicaciones, el sistema de archivos en microSD y la red cableada Ethernet.
* El **ESP32-C6** opera como transceptor inteligente de radiofrecuencia para **Wi-Fi 6 (2.4 GHz), BLE 5.4 y 802.15.4 (Zigbee / Thread / Matter)**.
* La interconexión física se realiza mediante un bus **SDIO de 4 bits @ 50 MHz** utilizando el protocolo de alto rendimiento **`esp_hosted`** con un canal de comandos binarios privados (RPC).

---

## 2. Topología de Pines y Buses Físicos (JC4880P443C)

Todos los pines están conectados y verificados según el esquema de hardware (`specs/hardware/pinouts_and_ports.md`):

```
┌───────────────────────────────────────┐             ┌────────────────────────────────────────┐
│             ESP32-P4                  │             │          ESP32-C6 (Módulo U3)          │
│   (Host Master - 400 MHz RISC-V)     │             │     (Front-End Radio Wi-Fi/BLE/15.4)   │
│                                       │             │                                        │
│   GPIO 14 ────────────────────────────┼─ SDIO_D0 ───┼─ GPIO 18 (SDIO Data 0)                 │
│   GPIO 15 ────────────────────────────┼─ SDIO_D1 ───┼─ GPIO 19 (SDIO Data 1)                 │
│   GPIO 16 ────────────────────────────┼─ SDIO_D2 ───┼─ GPIO 20 (SDIO Data 2)                 │
│   GPIO 17 ────────────────────────────┼─ SDIO_D3 ───┼─ GPIO 21 (SDIO Data 3)                 │
│   GPIO 18 ────────────────────────────┼─ SDIO_CLK ──┼─ GPIO 17 (SDIO Clock 50 MHz)           │
│   GPIO 19 ────────────────────────────┼─ SDIO_CMD ──┼─ GPIO 16 (SDIO Command)                │
│                                       │             │                                        │
│   GPIO 54 ────────────────────────────┼─ CHIP_PU ───┼─ EN (Línea de Reset C6)                │
│   GPIO 6  ────────────────────────────┼─ IRQ/HS ────┼─ GPIO 2 (Interrupción de Datos)        │
└───────────────────────────────────────┘             └────────────────────────────────────────┘
```

---

## 3. Protocolo Binario RPC sobre Canal Privado `esp_hosted`

Para no interferir con el tráfico de red TCP/IP normal, los comandos de auditoría e inyección de radio viajan por el canal privado de eventos de `esp_hosted`:
* **Identificador de Evento Privado:** `#define SPECTRA5_PRIV_EVENT_COMMAND 0x55`
* **Transporte:** `esp_hosted_send_priv_command(data, len)` (máximo 255 bytes por trama).

### 3.1. Opcodes de Comando (P4 $\rightarrow$ C6)
```cpp
enum class CommandOpcode : uint8_t {
    Nop             = 0,
    GetCapabilities = 1,
    SetMode         = 2,
    SetChannel      = 3,
    StartMonitor    = 4,
    StopMonitor     = 5,
    ScanStations    = 6,   // Enumeración de clientes vinculados a un BSSID
    StartDeauth     = 7,   // Inyección de tramas de desautenticación
    StartBeacon     = 8,   // Beacon spamming
    StartProbe      = 9,   // Inyección de probe requests
    InjectRaw       = 10,  // Inyección de trama 802.11 arbitraria
    Stop            = 11,
    SaeFlood        = 12,  // Flood de tramas SAE commit (WPA3 router DoS)
    StartKarma      = 13,  // Captura de solicitudes de red probe requests (Karma)
    StartDetect     = 14,  // Detección defensiva pasiva de ataques de desautenticación
    StartSniff      = 15,  // Estadísticas en tiempo real de tipos de paquetes
    ZigbeeScan      = 16,  // Escaneo de energía en canales 802.15.4 (11 al 26)
    Ieee154Sniff    = 17,  // Sniffer promiscuo 802.15.4 (parámetro: canal; 0 = stop; 0xFF = hop)
};
```

### 3.2. Eventos Asíncronos (C6 $\rightarrow$ P4)
| Event ID | Propósito | Carga Útil (*Payload*) |
| :---: | :--- | :--- |
| `0x55` | Resultado de comando ejecutado | `opcode (u8)`, `return_code (i8)` |
| `0x57` | Cliente Wi-Fi (Station) descubierto | `mac[6]`, `bssid[6]`, `rssi (i8)` |
| `0x58` | Captura EAPOL / Handshake | `kind (1..4)`, `ap[6]`, `client[6]`, `pmkid[16]` |
| `0x59` | Alerta de desautenticación detectada | `src_mac[6]` (MAC del atacante) |
| `0x5D` | Trama cruda EAPOL 802.11 para PCAP | Trama 802.11 completa (para volcado `.pcap`) |
| `0x5E` | Trama cruda IEEE 802.15.4 | `channel (u8)`, `lqi (u8)`, `rssi (i8)`, `frame_data[]` |

---

## 4. Capacidades Técnicas del Firmware en C6

### 4.1. Bypass de Sanity Check en Inyección Raw (`libnet80211`)
Por defecto, la librería binaria de Wi-Fi de Espressif bloquea tramas de gestión ofensivas (Deauth, Disassoc, tramas con BSSID foráneo). El firmware implementa la técnica probada en Marauder / Porkchop / Slave I:
* Sobrescribe el símbolo `ieee80211_raw_frame_sanity_check()` forzando retorno `0` (permitido).
* Enlace en CMake del C6 con el flag de linker: `-Wl,-zmuldefs`.

### 4.2. Sniffer 802.15.4 (Zigbee / Thread) con Exportación PCAP
1. El driver oficial `esp_ieee802154` del C6 reporta tramas en interrupción de hardware mediante `esp_ieee802154_receive_done`.
2. Para no bloquear el microcontrolador, la función reside en **IRAM (`IRAM_ATTR`)** y copia la trama en una cola FreeRTOS (`xQueueSendFromISR`).
3. Una tarea persistente extrae tramas, filtra paquetes ACK vacíos, realiza salto automático de canal (canales 11 a 26 cada 700 ms) y las envía al P4 con una tasa controlada de ~5 tramas/segundo para no saturar el canal SDIO.
4. En el P4, se generan archivos `.pcap` en la microSD con encabezado `LINKTYPE_IEEE802_15_4_NOFCS` (ID `230`), abribles de forma transparente en **Wireshark**.

### 4.3. Extracción de PMKID y Formateo Hashcat 22000
El parser en C6 inspecciona las tramas de datos no cifradas con LLC/SNAP EtherType `0x888E`:
* Identifica el mensaje **M1** del 4-way handshake.
* Si el tag vendor `0xDD` contiene la suite RSN PMKID `00:0F:AC:04`, extrae los 16 bytes del PMKID.
* El P4 genera la línea Hashcat en formato estándar:
  ```
  WPA*01*<PMKID_HEX>*<AP_MAC>*<CLIENT_MAC>*<ESSID_HEX>***
  ```
  Almacenándola en `/sd/wifi/handshakes.hc22000` para auditorías de recuperación de claves.

---

## 5. Auto-Provisionamiento Single-Flash (Flasheo C6 desde P4)

Para que el usuario de CBDos nunca tenga que conectar cables al C6 ni manipular botones físicos (`BOOT`/`RST`):
1. El binario compilado del firmware C6 (`network_adapter.bin`) se incrusta dentro del ejecutable del P4 mediante CMake:
   ```cmake
   target_add_binary_data(${COMPONENT_TARGET} "wifi_c6_fw/network_adapter.bin" BINARY)
   ```
2. Al arrancar el ESP32-P4:
   * Calcula el CRC32 del binario C6 embebido.
   * Consulta en la memoria NVS del P4 la clave `c6_crc`.
   * Si no existe o difiere, activa la pantalla de aviso en LVGL (`ARMING RADIO...`).
   * Transfiere el firmware al C6 en bloques de 1400 bytes mediante las APIs de OTA de `esp_hosted`:
     `rpc_ota_begin()`, `rpc_ota_write()`, `rpc_ota_end()`.
   * Registra el nuevo CRC32 en NVS y reinicia el sistema limpiamente.
