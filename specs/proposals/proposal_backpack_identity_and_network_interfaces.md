# Propuesta: Identidad versionada de mochilas e interfaces de red

**Estado:** propuesta para revisión (no implementada).
**Contexto:** hoy el P4 tiene dos islas: la vista MeshCore (puerto serie USB,
protocolo `<`/`>` oficial) y el Slot 2 del gestor de red (transporte C3 con
protocolo `0xAA 0x55` propio). Ambas pelean por el mismo OTG sin un árbitro,
y ninguna se autodescubre: el usuario elige el puerto a mano.

## Objetivo

Que el sistema identifique **cualquier mochila** (USB, pines JP1/MX, NFC a
futuro) y la registre como interfaz de red, con auto-conexión por toggle.

## Descriptor de mochila (versionado)

```cpp
struct BackpackDescriptor {
    uint8_t  descVersion = 1;     // versión del formato (matriz de compat)
    std::string proto;            // "meshcore-companion", "c3bridge", "cbdos-native"
    std::string protoVersion;     // versión del protocolo que habla el firmware
    std::string vendor, model, hwRev, fwVersion;
    std::string radio;            // "espnow", "sx1262@915", "sx1280@2.4", ...
    std::string transport;        // "usb-cdc", "uart-jp1", "uart-mx", "nfc"
    // Capacidades que el gestor usa sin código por modelo:
    uint8_t  maxChannels = 0;
    int8_t   maxTxDbm = 0;
    bool     hasBattery = false;
};
```

- Regla de compatibilidad: se aceptan versiones conocidas; versión más nueva
  desconocida → aviso (modo degradado); versión vieja incompatible → rechazo.
- Para firmwares **propios** (C3 bridge, futura SX1262) el descriptor lo entrega
  el firmware con un comando `GET_ID` de texto. Para MeshCore stock (no responde
  a nada custom) el descriptor se **reconstruye por huella**: `APP_START` →
  `SELF_INFO` (nombre, radio, canales) + `DEVICE_QUERY` (modelo, versión).

## Vías de identificación, un solo registro

| Vía | Detección | Nota |
|-----|-----------|------|
| USB OTG | hot-plug VID/PID/serie + sondeo de protocolo con timeout | totalmente automática |
| JP1 / MX (pines) | sin hot-plug físico: vinculación puerto→mochila una vez, persistida | el sistema recuerda |
| NFC (futuro) | el tap entrega el descriptor | misma lógica, otra vía |

## `BackpackManager` (nuevo, en `core`)

Responsabilidades únicas: **propiedad exclusiva del puerto** (un driver a la
vez en el OTG), **sondeo** (APP_START → C3-status → serie genérica, cada uno
con timeout, ninguno modifica el dispositivo), **binding persistido** y
**registro** del resultado como `INetworkInterface` en el
`NetworkInterfaceManager`.

## Toggle por tipo de mochila

- `MeshCore: ON/OFF`, `C3 bridge: ON/OFF`, … (persistido en NVS).
- Mochila conocida + toggle ON → el gestor la abre, hace handshake y la
  registra sola ("magia"). Toggle OFF → ni la toca (imprescindible con dos
  mochilas y un solo OTG).

## Fases sugeridas

1. `BackpackDescriptor` + matriz de compat + sondeo USB (MeshCore y C3).
2. Propiedad exclusiva del OTG + registro como `INetworkInterface`.
3. Binding persistido para JP1/MX + toggles en UI + hook NFC (solo interfaz).

## Riesgos

- Un sondeo erróneo puede confundir dispositivos tontos: mitigar con timeouts
  cortos y comandos inocuos (`APP_START`, `GET_STATUS` no cambian estado).
- No sobrediseñar: hay 2 mochilas reales; nada de framework de plugins, solo
  el manager mínimo descrito arriba.
