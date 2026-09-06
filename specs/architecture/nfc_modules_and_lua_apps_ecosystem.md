## 1. Visión General: El Administrador de Cartuchos / Mochilas (`cbdos.backpack`)

CBDos implementa un servicio orquestador central denominado **`BackpackManager`** (expuesto en Lua como **`cbdos.backpack`**). Este subsistema es el encargado exclusivo de gestionar el ciclo de vida de los cartuchos/mochilas físicas mediante la lectura de tags NFC pasivos.

```text
 ┌─────────────────────────────────────────────────────────────────────────┐
 │                   CARTUCHOS / MOCHILAS FÍSICAS (3D)                     │
 │      [ Cartucho Osciloscopio ]   [ Cartucho Multímetro / INA226 ]       │
 └────────────────────────────────────┬────────────────────────────────────┘
                                      │ (Acoplamiento NFC NTAG213)
                                      ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │               ORQUESTADOR CENTRAL: cbdos.backpack                       │
 │  • Lee e interpreta el descriptor del tag NFC                           │
 │  • Asigna y valida configuración de pines de hardware                   │
 │  • Emite señal acústica de acoplamiento (consume cbdos.audio)           │
 │  • Lanza la app asociada o carga la vista UI                            │
 │  • Maneja el ciclo de vida onAttach() / onDetach()                      │
 └────────────────────────────────────┬────────────────────────────────────┘
                                      │
                                      ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │                      SERVICIOS BASE DEL SISTEMA (APIs)                  │
 ├───────────────────┬───────────────────┬───────────────────┬─────────────┤
 │    cbdos.io       │    cbdos.audio    │     cbdos.ui      │ cbdos.system│
 │  (GPIO, ADC, I2C, │ (Beeps de aviso,  │ (Renderizado,     │ (EventBus,  │
 │   SPI, PWM, UART) │  Alertas sonoras) │  Ventanas, Vistas)│  Storage)   │
 └───────────────────┴───────────────────┴───────────────────┴─────────────┘
```

---

## 2. Flujo de Auto-Detección y Acoplamiento en Caliente (Hot-Swap)

```text
 ┌──────────────────────────┐                   ┌──────────────────────────┐
 │  Módulo 3D / Cartucho    │                   │   CyberDeck (Host CBDos) │
 │  • Transceptor SX1262    │                   │  • Lector NFC (PN532)    │
 │  • Tag NFC (NTAG213)     │                   │  • Pantalla IPS 4.3"     │
 └────────────┬─────────────┘                   └────────────┬─────────────┘
              │                                              │
              │────── [ Acoplamiento Magnético ] ───────────>│
              │                                              │
              │<───── [ Lectura de Bloque NDEF / TLV ] ──────│ (T < 15 ms)
              │       Payload: ID Módulo, Pines, App Lua     │
              │                                              │
              │                                              │ 1. Inicializa Bus (SPI/I2C/UART)
              │                                              │ 2. Beep en Altavoz (ES8311)
              │                                              │ 3. Lanza /sdcard/apps/lora.lua
              │                                              │
              │────── [ Desacoplamiento Físico ] ───────────>│
              │                                              │ 1. Evento onModuleDetached()
              │                                              │ 2. Cierra LuaApp de forma segura
              │                                              │ 3. Libera GPIOs y apaga bus
```

---

## 3. Estructura de Datos en el Tag NFC (Module Descriptor)

El tag NFC almacena una estructura compacta binaria (o JSON ultra ligero de ~80 bytes):

```json
{
  "magic": "CBM",
  "v": 1,
  "type": "radio_lora",
  "name": "SX1262 915MHz",
  "bus": "SPI",
  "pins": {
    "cs": 15,
    "rst": 13,
    "dio1": 11,
    "busy": 9
  },
  "app": "lora_messenger.lua",
  "icon": "antenna_icon"
}
```

---

## 4. Fachada de Hardware para el Ecosistema Lua (`LuaEngine Facade`)

La integración de los cartuchos NFC proporciona una **fachada de hardware limpia en Lua** que abstrae los controladores de bajo nivel (C++/HAL), permitiendo que cualquier script o Luapp controle los pines y protocolos sin acoplamiento a librerías de interfaz:

```lua
-- /sdcard/apps/lora_module.lua
-- Ejemplo de script autónomo que interactúa directamente con la fachada de hardware

local radio = cbdos.radio
local system = cbdos.system

-- 1. Configuración de parámetros de hardware del cartucho
radio.init({
    chip = "SX1262",
    frequency = 915000, -- 915 MHz
    power = 14,         -- +14 dBm
    bandwidth = 250     -- 250 kHz
})

-- 2. Manejo de paquetes en segundo plano
radio.onReceive(function(sender_id, data, rssi)
    print(string.format("[RF RX] De: 0x%04X | RSSI: %d dBm | Bytes: %d", sender_id, rssi, #data))
end)

-- 3. Emisión de datos
function sendPayload(dst_id, payload)
    return radio.send(dst_id, payload)
end

-- 4. Hook de extracción física del cartucho
system.onModuleDetached(function()
    print("Módulo desconectado físicamente. Liberando recursos...")
    radio.powerOff()
end)
```

---

## 5. Fachadas de Instrumental y Diagnóstico de Hardware (Laboratory & Hardware Hacking Facades)

Al acoplar cartuchos con adaptadores de señal (ej. sondas de osciloscopio, atenuadores, shunts de corriente o expansores de pines), la fachada de Lua expone los módulos de medición y diagnóstico del sistema:

### 5.1. Fachada de Osciloscopio / ADC Rápido (`cbdos.scope`)
```lua
-- Módulo con circuito de acondicionamiento de señal conectado al ADC DMA
local scope = cbdos.scope

scope.configure({
    sample_rate = 1000000, -- 1 MSPS
    trigger_edge = "RISING",
    trigger_level_mv = 1650,
    timebase_us = 50
})

local samples = scope.capture(1024) -- Captura ráfaga por DMA en PSRAM
```

### 5.2. Fachada de Multímetro Digital / Shunt (`cbdos.meter`)
```lua
-- Módulo con chip INA219 / INA226 o ADC diferencial
local meter = cbdos.meter

local v = meter.readVoltage() -- Voltios (mV)
local i = meter.readCurrent() -- Corriente (mA)
local p = meter.readPower()   -- Potencia (mW)
```

### 5.3. Fachada de Escaneo y Análisis I2C / SPI / GPIO (`cbdos.bus` / `cbdos.logic`)
```lua
-- Módulo de pruebas de buses y breakout de pines
local bus = cbdos.bus

-- Escaneo automático de direcciones en el bus I2C del cartucho
local devices = bus.i2cScan()
for _, addr in ipairs(devices) do
    print(string.format("Dispositivo I2C detectado en: 0x%02X", addr))
end

-- Analizador Lógico básico (Captura de estados GPIO)
local logic = cbdos.logic
logic.startCapture({ pins = {4, 5, 6, 7}, duration_ms = 100 })
```

---

## 6. Catálogo de Cartuchos 3D de Instrumental y Hardware
| Cartucho / Módulo | Circuito / Periférico | Fachada Lua Asociada |
| :--- | :--- | :--- |
| **Sonda Osciloscopio** | ADC DMA + Divisor/OpAmp | `cbdos.scope` |
| **Multímetro / Vatímetro**| INA226 / ADS1115 | `cbdos.meter` |
| **I2C / SPI Sniffer** | Expansor + Level Shifter | `cbdos.bus` |
| **Logic Analyzer** | GPIOs dedicados (JP1) | `cbdos.logic` |
| **Radio LoRa / FLRC** | Semtech SX1262 / SX1280 | `cbdos.radio` |
| **IR Universal** | LED IR + Receptor TSOP | `cbdos.ir` |
| **RFID / NFC Tool** | NXP PN532 / RC522 | `cbdos.nfc` |

---

## 6. Ventajas del Ecosistema
1. **Zero Recompilación:** Escribes un archivo `.lua` en la MicroSD y la app funciona inmediatamente con aceleración gráfica LVGL a 60 FPS.
2. **App Store Descentralizada (Over-The-Air):** Puedes transferir una Luapp completa (de 2 a 4 KB) de un CyberDeck a otro vía **ESP-NOW / 802.15.4** en milisegundos.
3. **Ergonomía de Campo:** Cambiar de herramienta física (de un clonador RF a un comunicador LoRa o GPS) toma **1 segundo** con acoplamiento magnético y auto-detección NFC.
