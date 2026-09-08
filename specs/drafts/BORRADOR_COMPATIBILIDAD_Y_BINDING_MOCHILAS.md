# Borrador: matriz de compatibilidad y persistencia del binding

**Estado:** borrador de diseño (sin código).
**Problema que resuelve:** las mochilas evolucionan (firmwares nuevos, nuevos
radios como SX1262/SX1280) y el P4 debe seguir entendiéndolas sin reflasheo,
o rechazarlas con un mensaje claro en vez de comportarse raro.

## Versionado en dos ejes

- `descVersion` (formato del descriptor): entero. El gestor implementa los
  formatos 1..N conocidos.
- `proto` + `protoVersion` (protocolo que habla la mochila): p. ej.
  `meshcore-companion/1.x`, `c3bridge/1.0`, `cbdos-native/1.0`.

## Matriz de decisión

| Caso | Acción |
|------|--------|
| descriptor y proto conocidos | registro normal |
| `protoVersion` menor conocida, mismo mayor | aceptar + aviso "firmware antiguo" |
| `protoVersion` mayor desconocido | modo degradado (solo funciones del mayor conocido) + aviso "actualiza el P4" |
| `descVersion` desconocido | no registrar; mensaje "mochila no soportada por este sistema" |
| sin respuesta a ningún sondeo | serie genérico (terminal), nunca red |

## Persistencia (NVS, prefijo `mochila/`)

- `bind/<puerto>` → `{proto, idEstable}`: recuerda qué había en JP1/MX (vías
  sin hot-plug). En USB se prefiere el serie del dispositivo al puerto.
- `toggle/<proto>` → ON/OFF: el apagador por tipo de mochila.
- `ultima/<idEstable>` → alias y ajustes por mochila concreta.
- La identidad estable la da el dispositivo (serie USB / clave pública
  MeshCore / UID NFC), nunca el orden de enchufe.

## Migración y campo

- Cambiar el descriptor (p. ej. añadir campo `nfcUid`) sube `descVersion`;
  el gestor viejo lo rechaza con mensaje en vez de parsear mal: **fallo
  visible > fallo silencioso**.
- Regla de oro: ningún cambio de formato rompe mochilas ya vinculadas; como
  máximo piden re-vincular con aviso.

## Preguntas abiertas

1. ¿El alias por mochila vive en el P4 o debería viajar en la mochila
   (para que dos P4 la vean igual)? Propuesta: alias local P4 + nombre
   nativo del firmware como fallback.
2. ¿Cuántos bindings por puerto se conservan (una mochila por JP1 o varias
   rotando)? Propuesta: una activa + historial corto para re-selección.
