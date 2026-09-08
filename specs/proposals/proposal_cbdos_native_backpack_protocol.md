# Propuesta: protocolo nativo CBDos para mochilas propias (`cbdos-native`)

**Estado:** propuesta para revisión (no implementada).
**Motivación:** para firmwares propios (bridge C3 actual, futura mochila
SX1262/SX1280, módulos por pines o NFC) sí podemos exigir un mensaje de
identificación, a diferencia de MeshCore stock.

## Comando `GET_ID` (texto, inocuo, sin estado)

Toda mochila propia responde en su consola serie a:

```
> GET_ID
< ID proto=cbdos-native/1.0 vendor=CBDos model=SX1262-915 hw=1.0 fw=1.2.0 radio=sx1262@915 transport=usb-cdc ch=8 txmax=22 batt=1
```

- Una línea, parseable sin librerías, con los mismos campos del
  `BackpackDescriptor` (ver propuesta de identidad).
- Sin `GET_ID` → el gestor cae al sondeo por huella (MeshCore, C3 legacy).
- Versionado: `proto=cbdos-native/<mayor>.<menor>`; el gestor acepta mismo
  mayor con menor ≤ conocido, avisa si es más nuevo.

## Por qué el chip de radio va en el identificador

Con `radio=sx1262@915` vs `radio=sx1280@2.4` el gestor sabe banda, límites de
TX y qué UI mostrar sin código por modelo. Al crear una mochila nueva basta
con declarar su descriptor: cero cambios en el P4.

## Alcance explícito

- Solo para firmwares que versionemos nosotros. MeshCore stock queda fuera
  por diseño (huella, no `GET_ID`).
- Transporte agnóstico: la misma línea sirve por USB-CDC, UART (JP1/MX) y,
  a futuro, payload NFC.
- Migración del C3 actual: añadir `GET_ID` a `espnow_usb_bridge` manteniendo
  el status `0xAA 0x55` legacy como fallback.

## Criterio de aceptación

Mochila propia enchufada → identificada por `GET_ID` en <1 s, registrada con
su descriptor completo, sin selección manual de puerto ni protocolo.
