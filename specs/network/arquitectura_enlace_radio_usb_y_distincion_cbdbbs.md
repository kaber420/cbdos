# Arquitectura de Enlace de Radio Módem USB y Distinción de cbdBBS

---

## 📌 1. Introducción y Visión General

En la evolución de la plataforma **CBDos**, el subsistema de comunicaciones inalámbricas opera bajo un modelo desacoplado y multi-interfaz. Este documento establece dos definiciones arquitectónicas fundamentales:

1. **La Arquitectura del Enlace de Radio Módem USB:** Integración física y lógica entre el procesador principal (**ESP32-P4**) y el coprocesador de radio (**ESP32-C3**) mediante el puerto USB-OTG High-Speed (CDC-ACM), permitiendo la captura, emisión e inyección de balizas y paquetes ESP-NOW en tiempo real.
2. **Distinción Formal entre Infraestructura Mesh y la Aplicación cbdBBS:** Separación estricta entre el motor de red y enrutamiento del sistema operativo (**MeshEngine / NetworkManager**) y la aplicación de interfaz de usuario para tablones de anuncios, foros descentralizados y mensajería comunitaria (**cbdBBS**, anteriormente referenciada de forma preliminar como *MeshCore* en el dashboard).

---

## 🏗️ 2. Topología de Hardware y Flujo de Datos

```
 +-------------------------------------------------------------------------+
 |                               ESP32-P4                                  |
 |                                                                         |
 |  +--------------------+   +-------------------+   +------------------+  |
 |  | NetworkManagerView |   |    MeshConfig     |   |      cbdBBS      |  |
 |  | (Gestor Interfaces)|   | (Torres / Balizas)|   | (Tablón / Foros) |  |
 |  +---------+----------+   +---------+---------+   +--------+---------+  |
 |            |                        |                      |            |
 |            v                        v                      v            |
 |  +-------------------------------------------------------------------+  |
 |  |                   MeshEngine / NetworkManager                     |  |
 |  +----------------------------------+--------------------------------+  |
 |                                     |                                   |
 |                                     v                                   |
 |  +-------------------------------------------------------------------+  |
 |  |               UsbCdcMeshTransport (HAL ESP32-P4)                  |  |
 |  |      (Hilo RX Asíncrono + TX Bufferizado en CDC-ACM Host)         |  |
 |  +----------------------------------+--------------------------------+  |
 |                                     |                                   |
 |                                     v                                   |
 |                         [ USB-OTG High-Speed ]                          |
 +-------------------------------------+-----------------------------------+
                                       |
                   Cable USB Tipo-C a Tipo-C (12 Mbps CDC-ACM)
                                       |
 +-------------------------------------+-----------------------------------+
 |                         [ USB-Serial-JTAG ]                             |
 |                                                                         |
 |  +-------------------------------------------------------------------+  |
 |  |               Firmware Módem USB Bridge (ESP32-C3)                |  |
 |  |     (Enmarcado 0xAA 0x55, Control de Potencia, Gestión NVS)       |  |
 |  +----------------------------------+--------------------------------+  |
 |                                     |                                   |
 |                                     v                                   |
 |  +-------------------------------------------------------------------+  |
 |  |                  Radio Wi-Fi / ESP-NOW (2.4 GHz)                  |  |
 |  |             (Canal 1..13, 802.11b/g/n y Long Range)               |  |
 |  +-------------------------------------------------------------------+  |
 |                               ESP32-C3                                  |
 +-----------------------------------------+-------------------------------+
```

---

## 📡 3. Protocolo de Enlace Módem USB (`0xAA 0x55`)

La comunicación entre el ESP32-P4 y el módem ESP32-C3 se realiza mediante tramas binarias protegidas por CRC8:

### 3.1. Direcciones de Tráfico y Códigos de Función
| Código (`DIR`) | Nombre | Origen $\rightarrow$ Destino | Descripción |
|---|---|---|---|
| `0x01` | `DIR_PC_TO_DONGLE` | P4 $\rightarrow$ C3 | Paquete de datos de radio para ser transmitido al aire por ESP-NOW. |
| `0x02` | `DIR_DONGLE_TO_PC` | C3 $\rightarrow$ P4 | Paquete de radio recibido del aire `[MAC (6B) \| RSSI (1B) \| DATA]`. |
| `0x03` | `DIR_CMD_REQ` | P4 $\rightarrow$ C3 | Comando de control o telemetría hacia el módem. |
| `0x04` | `DIR_CMD_RESP` | C3 $\rightarrow$ P4 | Respuesta estructurada al comando de control con estado y alias. |

### 3.2. Procesamiento de Balizas y Tráfico en Vivo en el P4
1. **Captura en el C3:** Al recibir un paquete del aire (ej. baliza de torre PoP, anuncio de nodo o paquete de datos), la función `onDataRecv()` del C3 empaqueta la dirección MAC de origen (6 bytes), el RSSI (1 byte) y la carga útil cruda, enviando la trama `0x02` por USB.
2. **Recepción en el P4:** La tarea `UsbCdcMeshTransport` del P4 escucha de forma asíncrona la tubería IN del CDC-ACM, extrae el payload y ejecuta `injectRxData(src_mac, data, len, rssi)`.
3. **Decodificación en `MeshEngine`:**
   - Si la trama corresponde a una baliza de torre (`PKT_TOWER_ANNOUNCE` / `PoP Broadcast`), se actualiza la tabla `m_discoveredTowers` registrando canal, RSSI y Short ID.
   - Si corresponde a tráfico de datos o mensajería de `cbdBBS`, se enruta hacia los manejadores de servicio registrados.

---

## 🏷️ 4. Renombramiento Formal: de *MeshCore* (Mockup) a `cbdBBS`

### 4.1. Causa y Justificación del Renombramiento
Durante las fases iniciales de desarrollo, la vista de mensajería y tablón comunitario fue denominada temporalmente *"MeshCore"*. Esto generó confusión de conceptos:
* **MeshCore / MeshEngine:** Corresponde estrictamente al **núcleo de infraestructura y enrutamiento** de bajo nivel (capa 2 y capa 3 de red mesh, resolución Pseudo-ARP, TLVGL, descubrimiento de balizas y gestión de interfaces físicas).
* **cbdBBS:** Es la **aplicación de usuario final** (capa 7 de aplicación) inspirada en los sistemas BBS (*Bulletin Board Systems*) comunitarios, que permite:
  - Publicación y lectura de mensajes locales en canales temáticos (`#general`, `#alertas`, `#mercado`).
  - Radar de usuarios y dispositivos comunitarios conectados a la misma malla o torre.
  - Almacenamiento local de tablones y notas públicas fuera de línea.

### 4.2. Plan de Reestructuración en el Código Fuente
1. **Dashboard (`DashboardView.cpp`):**
   - Actualizar el registro de la aplicación:
     ```cpp
     // Antes:
     {"meshcore", "MeshCore", LV_SYMBOL_WIFI, 0x00F5D4, false, ""}
     // Nuevo:
     {"cbdbbs", "CBD BBS", LV_SYMBOL_LIST, 0x00F5D4, false, ""}
     ```
2. **Espacio de Nombres y Clases:**
   - Mover / renombrar de `cbdos::apps::meshcore` a `cbdos::apps::cbdbbs` (`BbsView`, `BbsEngine`).
3. **Gestor de Interfaces de Red (`NetworkManagerView.cpp`):**
   - Eliminar dependencias con mockups estáticos y vincular las ranuras de hardware reales:
     - **Slot 0:** Radio Integrada (ESP-NOW / Wi-Fi STA en ESP32-S3 o ESP-Hosted en P4).
     - **Slot 1:** Mochila de Expansión LoRa (SX1262 en JP1).
     - **Slot 2:** Módem USB CDC-ACM (ESP32-C3 en puerto USB High-Speed).

---

## ✅ 5. Estado de Implementación y Próximos Hitos

- [x] Protocolo binario de control y transporte USB implementado en C3 (`tools/espnow_usb_bridge`).
- [x] Verificación de enlace físico bidireccional P4 $\leftrightarrow$ C3 por puerto USB OTG High-Speed.
- [x] Emisión de paquetes ESP-NOW desde el P4 a través del módem USB C3.
- [ ] Implementación del hilo continuo `UsbCdcMeshTransport` en el BSP del ESP32-P4 para inyección de balizas a `MeshEngine`.
- [ ] Refactorización y renombramiento de la UI de la aplicación a `cbdBBS`.
- [ ] Vinculación dinámica de la tarjeta de red Slot 2 en `NetworkManagerView`.
