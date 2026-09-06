# 🎒 BackpackManager y Auto-Detección de Mochilas Modulares por NFC

**Fecha:** 2026-09-02  
**Versión:** 1.0.0  
**Estado:** Especificación Técnica & Arquitectura de Hardware Dinámico  
**Target:** ESP32-P4 (Cabecera JP1 / Bus SPI / I2C / NFC)  

---

## 🏛️ 1. Visión General: El Administrador de Mochilas (`BackpackManager`)

El **`BackpackManager`** de CBDos es el subsistema encargado de transformar el Cyberdeck en una plataforma de hardware modular tipo *Plug & Play* físico. 

Mediante un lector **NFC (PN532 / ST25R)** integrado en el chasis trasero y etiquetas **NFC (NTAG213/215/216)** adheridas a cada mochila o accesorio intercambiable, el sistema es capaz de:
1. **Detectar el acoplamiento físico** en menos de 15 ms al acercar la mochila.
2. **Leer el descriptor de hardware** almacenado en el chip NFC.
3. **Reconfigurar dinámicamente los pines GPIO, I2C, SPI o UART** en la cabecera de expansión **JP1**.
4. **Instanciar los drivers necesarios y lanzar automáticamente la Lua App** correspondiente desde la MicroSD (`/sdcard/apps/`).
5. **Garantizar la liberación segura de recursos** al desacoplar la mochila (Hot-Swap Protection).

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                      MOCHILA / CARTUCHO MODULAR FÍSICO                          │
│                                                                                 │
│   ┌─────────────────────────────┐         ┌─────────────────────────────────┐   │
│   │   Circuito / Periférico     │         │       TAG NFC (NTAG213)         │   │
│   │   • Radio LoRa SX1262       │         │   • ID: "MOD_LORA_915"          │   │
│   │   • Sensor BME680 (I2C)     │         │   • PinMap: SPI_PINS_PRESET_A   │   │
│   │   • Sub-GHz CC1101          │         │   • App: "lora_mesh_chat.lua"   │   │
│   └──────────────┬──────────────┘         └────────────────┬────────────────┘   │
└──────────────────┼─────────────────────────────────────────┼────────────────────┘
                   │ Conexión Eléctrica JP1                  │ Lectura RF (13.56 MHz)
                   ▼                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           CYBERDECK ESP32-P4 (CBDos)                            │
│                                                                                 │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                    NFC Polling Task & Descriptor Parser                   │  │
│  └─────────────────────────────────────┬─────────────────────────────────────┘  │
│                                        │                                        │
│  ┌─────────────────────────────────────┴─────────────────────────────────────┐  │
│  │                   BackpackManager / Dynamic PinMux Engine                 │  │
│  │  • Asigna pines GPIO (MISO, MOSI, SCK, CS, RST, IRQ) en JP1              │  │
│  │  • Configura buses I2C (SDA, SCL, Frecuencia 400 kHz)                     │  │
│  │  • Enciende / Apaga líneas de alimentación (Power Gating)                 │  │
│  └─────────────────────────────────────┬─────────────────────────────────────┘  │
│                                        │                                        │
│  ┌─────────────────────────────────────┴─────────────────────────────────────┐  │
│  │                        UI & Lua Runtime Auto-Launcher                     │  │
│  │  • Notificación flotante / Animación Lottie: "Mochila Acoplada"          │  │
│  │  • Ejecuta script en espacio de memoria aislado (/sdcard/apps/...)        │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🏷️ 2. Estructura del Descriptor NFC (Payload NDEF / TLV)

El tag NFC de cada mochila almacena un descriptor ligero (entre 64 y 144 bytes) codificado en formato TLV (Type-Length-Value) o JSON binario compacto:

```json
{
  "bid": "MOD_LORA_SX1262",
  "name": "LoRa 915MHz Tactical",
  "ver": "1.0",
  "bus": "SPI",
  "pins": {
    "cs": 33,
    "rst": 35,
    "busy": 36,
    "irq": 37,
    "mosi": 21,
    "miso": 22,
    "sck": 20
  },
  "power_mv": 3300,
  "app": "/sdcard/apps/lora_tactical.lua"
}
```

### Tabla de Descriptores de Bus Soportados:

| Tipo de Bus | Parámetros Dinámicos Reconfigurables por NFC |
| :--- | :--- |
| **`SPI`** | `cs_gpio`, `rst_gpio`, `irq_gpio`, `busy_gpio`, `clock_speed_hz` |
| **`I2C`** | `sda_gpio`, `scl_gpio`, `i2c_addr`, `clock_speed_hz` (100k/400k) |
| **`UART`** | `tx_gpio`, `rx_gpio`, `baudrate`, `flow_control` |
| **`GPIO_RAW`**| `in_pins[]`, `out_pins[]`, `pwm_channels[]`, `adc_channels[]` |

---

## ⚡ 3. Dynamic PinMux Engine (Reconfiguración de Pines en Caliente)

La cabecera de expansión **JP1 (2×13 pines)** del ESP32-P4 ofrece pines de uso general configurables mediante el GPIO Matrix interno del ESP32:

1. **Estado Inactivo (Safe Default):**
   - Cuando no hay ninguna mochila acoplada, todos los pines de JP1 se colocan en **alta impedancia (Hi-Z)** con resistencias pull-down internas para evitar cortocircuitos o consumo fantasma.
2. **Transición al Acoplamiento (`onAttach`):**
   - El `BackpackManager` valida que los pines solicitados por la etiqueta NFC estén libres y pertenezcan a la cabecera JP1.
   - Aplica energía al pin de alimentación (si cuenta con control por transistor/LDO).
   - Enlaza las señales de silicio del periférico (SPI Host 2, I2C Master 1, etc.) a los pines físicos solicitados.
   - Envía el evento del sistema `CBDOS_EVENT_BACKPACK_ATTACHED`.
3. **Transición al Desacoplamiento (`onDetach`):**
   - Si el usuario retira la mochila físicamente, el lector NFC detecta la pérdida de señal de campo RF.
   - Se abortan de inmediato las transferencias DMA pendientes en el bus.
   - Se reestablecen los GPIOs a estado seguro (Hi-Z).
   - Se notifica a la Lua App activa invocando `on_backpack_detached()` y cerrando la vista de forma segura.

---

## 🎛️ 4. API Lua para Desarrolladores (`cbdos.backpack`)

Los scripts Lua pueden interactuar directamente con el gestor de mochilas para responder a eventos físicos o consultar las capacidades del módulo acoplado:

```lua
-- Registrar observador de acoplamiento de mochilas
cbdos.backpack.on_attach(function(module_info)
    ui.log("[+] Mochila detectada: " .. module_info.name)
    ui.log("    ID: " .. module_info.bid .. " | Bus: " .. module_info.bus)
    ui.toast("Mochila acoplada: " .. module_info.name, 3000)
    
    -- Acceder a los pines asignados dinámicamente
    local spi_device = cbdos.io.spi_open(module_info.pins.cs)
    -- Iniciar protocolo de comunicación con la mochila...
end)

cbdos.backpack.on_detach(function()
    ui.log("[-] Mochila retirada físicamente.")
    ui.toast("Mochila desconectada", 2000)
end)
```

---

## 💡 5. Casos de Uso y Ecosistema de Mochilas Modulares

| Mochila Modular | Interfaz / Pines | Tag NFC ID | Aplicación Automática |
| :--- | :--- | :--- | :--- |
| **📻 LoRa 915/868 MHz (SX1262)** | SPI + IRQ + BUSY | `MOD_LORA_SX1262` | `lora_mesh_chat.lua` |
| **📡 Sub-GHz CC1101 (300-928 MHz)** | SPI + GDO0/GDO2 | `MOD_CC1101_SUBGHZ`| `subghz_analyzer.lua` |
| **🌡️ Estación Meteorológica (BME680)** | I2C (SDA/SCL) | `MOD_ENV_BME680` | `weather_station.lua` |
| **📊 Osciloscopio / Analizador Lógico**| ADC DMA / GPIOs | `MOD_MINI_SCOPE` | `oscilloscope_view.lua` |
| **🛰️ Módulo GPS / GNSS + Brújula** | UART TX/RX + I2C | `MOD_GNSS_NEO8M` | `tactical_map.lua` |
| **🔋 Batería Inteligente (Fuel Gauge)** | I2C (BQ27441) | `MOD_SMART_BATTERY`| `battery_stats.lua` |

---

## 🛡️ 6. Protección y Convivencia con el Hub USB

El `BackpackManager` opera de forma totalmente coordinada con el subsistema **USB Host CDC**:
- Un usuario puede tener conectados **3 módems ESP32-C3 en el Hub USB-C** (manejando redes de alta velocidad ESP-NOW) y al mismo tiempo **una mochila LoRa acoplada por SPI/NFC**.
- El stack de ruteo de CBDos puede hacer puente transparente (*bridging*) de paquetes entre la mochila SPI y los módems USB sin conflicto de recursos ni colisión de interrupciones.
