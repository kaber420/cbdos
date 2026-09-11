# Capacidades Reales del ESP32-C6 vía ESP-Hosted-MCU (SDIO)

> **Fecha de Investigación:** Septiembre 2026
> **Fuente Principal:** [`espressif/esp-hosted-mcu`](https://github.com/espressif/esp-hosted-mcu) — Documentación oficial verificada en vivo (README, `docs/architecture.md`, `docs/getting-started-mcu.md`, `docs/features/openthread.md`)
> **Hardware Target:** Guition JC4880P443C — ESP32-P4 (Host) + ESP32-C6 (Coprocesador vía SDIO Slot 1)
> **Estado:** Investigación con datos verificados. Sin invenciones.
> **Decisión de Proyecto (10/09/2026):** ESP-NOW y Thread/Zigbee **DESCARTADOS** en CBDos hasta que Espressif implemente soporte nativo en ESP-Hosted. Protocolos activos: **Wi-Fi + Bluetooth/BLE únicamente.**

---

## 1. Lo que el README oficial dice que es ESP-Hosted-MCU

El primer párrafo del README oficial dice literalmente:

> **"Turn an Espressif SoC into a Wi-Fi, Bluetooth and OpenThread co-processor for your host."**
> "Your main processor keeps running your application, while the ESP SoC handles the heavy lifting of the radio and network stack over a simple transport bus."
> "It supports Linux and MCU hosts over **SDIO, SPI, and UART** using an Espressif SoC as the coprocessor."

**Conclusión directa:** El framework está diseñado explícitamente para Wi-Fi, Bluetooth **y** OpenThread/802.15.4. No solo Wi-Fi.

---

## 2. Tabla de Protocolos Soportados (Verificada)

Esta tabla viene directamente del diagrama de arquitectura oficial (`docs/architecture.md`):

```
Co-Processor firmware
├── Wi-Fi 2.4 / 5 GHz       ← Soportado nativamente
├── Bluetooth (Classic + BLE) ← Soportado nativamente (vía HCI)
└── 802.15.4 / OpenThread / Zigbee (RCP) ← Soportado, con matiz (ver §5)
```

| Protocolo | ¿Soportado en SDIO? | Tipo de Soporte | Canal de Datos |
|:---|:---:|:---|:---|
| **Wi-Fi (STA/AP)** | ✅ Sí | Nativo, stock firmware | Bus SDIO (plano de datos) |
| **Bluetooth Classic + BLE 5** | ✅ Sí | Nativo, stock firmware | Bus SDIO (HCI multiplexado) |
| **802.15.4 / OpenThread** | ✅ Sí | Soportado oficialmente, con requisito extra | SDIO (lifecycle RPC) + **UART dedicado** (datos Spinel) |
| **Zigbee (RCP)** | ✅ Sí | Igual que OpenThread | SDIO (lifecycle) + **UART dedicado** (datos Spinel) |
| **ESP-NOW** | ⚠️ No nativo | Requiere overlay/Custom RPC | No incluido en stock firmware |

> [!IMPORTANT]
> Esta tabla es la realidad documentada, no una especulación. Wi-Fi **y** Bluetooth salen del firmware stock sin tocar nada. 802.15.4/OpenThread/Zigbee también están en el framework, pero su tráfico de datos (Spinel) necesita un UART físico adicional al bus SDIO. ESP-NOW no está en el firmware stock y requiere trabajo adicional.

---

## 3. Wi-Fi + Bluetooth: Funcionan sin modificar nada

### 3.1 Cómo se transportan en SDIO

El transporte SDIO es un bus compartido por dos planos, distinguidos por el campo `if_type` de cada trama (`docs/architecture.md`):

| `if_type` | Plano | Qué transporta |
|:---:|:---|:---|
| `ESP_STA_IF` | Datos | Tramas de red Wi-Fi (Station) |
| `ESP_AP_IF` | Datos | Tramas de red Wi-Fi (Access Point) |
| `ESP_HCI_IF` | Datos | Tráfico HCI de Bluetooth (Classic + BLE) |
| `ESP_SERIAL_IF` | Control | RPC serializado (comandos de control de features) |
| `ESP_PRIV_IF` | Privado | Eventos internos y handshake |

**Wi-Fi y Bluetooth comparten el mismo cable SDIO sin conflicto**, porque el campo `if_type` los separa en el nivel del driver.

### 3.2 Bluetooth: HCI sobre SDIO

El firmware stock del C6 actúa como **controlador HCI** (Host Controller Interface). El ESP32-P4 ejecuta el **stack de Bluetooth** completo (NimBLE o BlueDroid), y los comandos/eventos HCI viajan multiplexados en el mismo bus SDIO. No es necesario un UART adicional para Bluetooth.

Cita directa de `docs/getting-started-mcu.md`, tabla de buses:

| Bus | Bluetooth |
|:---|:---|
| **SDIO** | Hosted-HCI on the bus |
| SPI Full-Duplex | Hosted-HCI on the bus |
| UART | Wi-Fi **and** BT both as Hosted-HCI on UART |

---

## 4. El ESP32-C6 soporta SDIO: Confirmación Oficial

De la tabla de coprocesadores soportados en `docs/getting-started-mcu.md`:

| Coprocesador | SDIO | SPI Full-Duplex | SPI Half-Duplex | UART |
|:---|:---:|:---:|:---:|:---:|
| ESP32-C6 | ✅ | ✅ | ✅ | ✅ |

> [!NOTE]
> Los SoC limitados a SDIO son **ESP32, ESP32-C5, ESP32-C6 y ESP32-C61** (usan GPIOs SDIO fijos en silicio). El C6 usa GPIO 18–23 como pines SDIO físicos (no son remapeables). Esto coincide exactamente con el cableado del módulo JC-ESP32P4-M3.

---

## 5. OpenThread / Zigbee (802.15.4): Soportado Oficialmente con Arquitectura de Dos Buses

### 5.1 Fuente verificada

Cita literal de `docs/features/openthread.md` (leído en vivo el 10/09/2026):

> "Unlike Wi-Fi and Bluetooth (which ride the main SPI/SDIO ESP-Hosted transport), 802.15.4 uses **two separate links**:
> - **RCP lifecycle / control** rides the shared ESP-Hosted transport as **RPC** (requires RPC ext-v2 on the host).
> - **802.15.4 spinel data** rides a **dedicated UART** between host and RCP. The 'over Hosted transport' option exists in Kconfig but is **not implemented yet** — UART is the supported path today (and the only path for Zigbee)."

### 5.2 Arquitectura real para OpenThread/Zigbee

```
ESP32-P4 (Host)                        ESP32-C6 (RCP)
┌──────────────────────┐              ┌────────────────────┐
│ Stack OpenThread/     │              │ Radio 802.15.4     │
│ Zigbee (eh_host_ot)  │──[SDIO RPC]─▶│ (lifecycle control)│
│                       │              │                    │
│ Driver Spinel        │──[UART TX/RX]▶│ Spinel (datos)    │
└──────────────────────┘              └────────────────────┘
```

**El SDIO solo lleva el ciclo de vida (init/start/stop)**. Los paquetes Spinel (datos 802.15.4 reales) van por UART.

### 5.3 Implicación para el hardware JC4880P443C

El módulo JC-ESP32P4-M3 **no expone físicamente** un UART dedicado entre el P4 y el C6 en el PCB principal (solo hay SDIO y las líneas de debug/flash vía JP1). Para usar OpenThread/Zigbee se necesitaría:
- Identificar pines UART disponibles en ambos SoC accesibles via conector.
- O usar el UART de debug del C6 (C6_U0RXD/TXD en JP1) reasignándolo como canal Spinel (implicaría perder la capacidad de flasheo por ese puerto en producción).

> [!WARNING]
> Esta es la limitación real de 802.15.4 en este hardware específico, **no** una limitación del firmware. El firmware está listo; el problema es rutear un UART físico entre ambos chips.

---

## 6. ESP-NOW: No Nativo, Requiere Overlay Custom RPC

### 6.1 Estado real

El firmware stock de ESP-Hosted-MCU **no incluye** proxying de la API `esp_now.h`. El framework no expone `esp_now_send()` ni `esp_now_recv()` al host de manera transparente como lo hace con `esp_wifi_*` o HCI.

### 6.2 Camino para añadirlo

El propio framework provee el mecanismo de extensión llamado **Custom RPC** (`peer_data_transfer`), que es el "escape hatch" oficial para implementar cualquier funcionalidad adicional sobre el mismo bus SDIO. Para ESP-NOW:

1. **Lado esclavo (C6):** Añadir un componente/overlay que registre callbacks para IDs de comando personalizados y llame a las funciones `esp_now_*` localmente.
2. **Lado host (P4):** Implementar una librería shim que serialice los comandos ESP-NOW en el formato Custom RPC y los envíe con `esp_hosted_send_custom_data()`.
3. **Referencia real existente:** ESPHome mantiene un fork (`esphome/esp-hosted-firmware`) que ya implementa exactamente esto, con binarios precompilados.

### 6.3 La "via libre" del SDIO

El mismo bus SDIO que porta Wi-Fi y BT puede portar ESP-NOW una vez implementado el overlay. No requiere cable adicional. Solo firmware adicional en el C6.

---

## 7. Correcciones al Documento Anterior (`investigacion_firmware_c6_sdio.md`)

El documento previo contenía datos **correctos** en lo referente al hardware (pines SDIO, causa del problema SPI vs SDIO, procedimiento de flasheo). Sin embargo, omitía completamente las capacidades de Bluetooth, 802.15.4 y ESP-NOW. Esta sección las documenta sin invalidar lo anterior.

| Aspecto | Doc anterior | Realidad verificada |
|:---|:---|:---|
| Bus de conexión C6-P4 | SDIO 4-bit ✅ | SDIO 4-bit ✅ |
| Pines SDIO | Correctos ✅ | Correctos ✅ |
| Firmware stock: solo Wi-Fi | Implícito ⚠️ | Incompleto — también soporta BT y 802.15.4 |
| Bluetooth via SDIO | No mencionado | ✅ Funciona, HCI multiplexado en SDIO |
| 802.15.4 via SDIO | No mencionado | ✅ Lifecycle vía SDIO + datos vía UART (limitación de HW en este módulo) |
| ESP-NOW | No mencionado | ⚠️ Requiere Custom RPC overlay |

---

## 8. Thread/Zigbee en JC4880P443C: No Viable con la Solución del Fabricante (Estado Septiembre 2026)

> [!CAUTION]
> **Thread y Zigbee NO son viables en este hardware con el firmware oficial de Espressif.** Esta sección documenta el porqué para no perder el tiempo en el futuro.

### 8.1 La causa técnica real (no una limitación de diseño nuestro)

El bus SDIO entre el ESP32-P4 y el ESP32-C6 en el módulo JC-ESP32P4-M3 es la **única conexión de datos** entre ambos chips. No hay ningún UART físico interno entre ellos.

Para que Thread/Zigbee funcione, el protocolo Spinel (los paquetes 802.15.4 reales) necesita un **UART dedicado** porque la opción que existiría en Kconfig para mandarlo por SDIO/SPI:

> *"The 'over Hosted transport' option exists in Kconfig but is **not implemented yet** — UART is the supported path today"*
> — `docs/features/openthread.md`, Espressif oficial

Espressif ya sabe que tienen que implementarlo. Lo pusieron en el menú de configuración. Y luego no lo terminaron.

### 8.2 Versión actual del componente `espressif/esp_hosted`

| Parámetro | Valor |
|:---|:---|
| Versión en el proyecto CBDos | **3.0.7** ✅ (ya es la última) |
| Commit SHA | `6997a850fa72a3057f9a8d3662254eb527a0b64b` |
| Ruta | `bsp/esp32_p4_jc4880/managed_components/espressif__esp_hosted/` |
| Repositorio upstream | https://github.com/espressif/esp-hosted-mcu |

**No hay que actualizar nada.** La managed component ya está en la versión más reciente (3.0.7).

### 8.3 ¿Cuándo podría ser viable?

Solo si Espressif implementa la opción `Spinel over Hosted transport` que ya tienen en Kconfig pero sin código. Cuando eso ocurra, no haría falta ningún cable adicional — el mismo bus SDIO que hoy transporta Wi-Fi y BT transportaría también los paquetes Thread/Zigbee.

**Acción recomendada:** Monitorear el repositorio `espressif/esp-hosted-mcu` para detectar cuando implementen esa feature. Hasta entonces, **no invertir tiempo en Thread/Zigbee en este hardware**.

### 8.4 Workarounds descartados

| Workaround | Por qué descartado |
|:---|:---|
| UART físico externo con jumpers/mochila | Inviable en producción, requiere hardware adicional y modificación del PCB |
| Reasignar UART de debug del C6 (JP1) | Pierde la capacidad de flashear el C6 en producción |
| Fork comunitario con Spinel-over-SDIO | No existe ninguno maduro o soportado a fecha de esta investigación |

---

## 9. Referencias y Fuentes Primarias

Todos los datos de este documento fueron extraídos directamente de las siguientes URLs en la fecha indicada. Sin interpretaciones, sin inventos.

| Documento | URL | Leído |
|:---|:---|:---|
| README oficial esp-hosted-mcu | https://github.com/espressif/esp-hosted-mcu | 10/09/2026 |
| Arquitectura y protocolo de tramas | https://github.com/espressif/esp-hosted-mcu/blob/main/docs/architecture.md | 10/09/2026 |
| Getting Started: MCU Host | https://github.com/espressif/esp-hosted-mcu/blob/main/docs/getting-started-mcu.md | 10/09/2026 |
| Feature: OpenThread/Zigbee | https://github.com/espressif/esp-hosted-mcu/blob/main/docs/features/openthread.md | 10/09/2026 |
| Registro del componente oficial | https://components.espressif.com/components/espressif/esp_hosted | — |
