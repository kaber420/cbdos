# Propuesta: MeshCore como interfaz de red nativa (auto-conexión)

**Estado:** propuesta para revisión (no implementada).
**Depende de:** `proposal_backpack_identity_and_network_interfaces.md`.

## Problema

El S3 con companion funciona en la vista MeshCore, pero es una isla: no aparece
en el gestor de red, no alimenta al `MeshEngine` y exige elegir puerto a mano.

## Propuesta

Un adaptador `MeshCoreInterface : INetworkInterface (+ IMeshTransport)` que
envuelva al `MeshCoreClient` existente:

- **Detección:** la hace el `BackpackManager` (huella `APP_START`/`SELF_INFO`);
  este adaptador no sondea, solo recibe el puerto ya identificado.
- **Auto-conexión:** si el toggle "MeshCore" está ON y aparece la mochila, el
  gestor abre el puerto, espera el handshake (con el reintento ~2 s ya
  implementado en el cliente) y registra la interfaz con el nombre real del
  nodo (`3A381F27`, …). Toggle OFF → se ignora.
- **Apagador funcional:** el toggle es el "encendido/apagado" que pedía el
  usuario: con MeshCore ON + dongle conectado, todo funciona directo, como un
  sistema inteligente; al desconectar, la interfaz se desregistra sola.
- **Mapeo de tráfico:** mensajes de canal MeshCore ⇄ paquetes del MeshEngine
  (canal 0 ↔ scope público por defecto; el resto, configurable). Los DMs
  (0x07/0x10) se exponen como tráfico dirigido si el motor lo soporta; si no,
  quedan solo en la vista de chat (sin bloquear la propuesta).

## No incluye (a propósito)

- Reescribir el protocolo: se usa el companion oficial tal cual.
- Routing entre mallas heterogéneas (MeshCore ⇄ C3/TLV): es fase posterior y
  tiene implicaciones de cifrado/identidad que merecen propuesta propia.

## Criterio de aceptación

Enchufar el S3 con el toggle ON → en <5 s aparece "Mochila MeshCore
\<nombre\>" como interfaz activa en el gestor, sin tocar la vista MeshCore.
