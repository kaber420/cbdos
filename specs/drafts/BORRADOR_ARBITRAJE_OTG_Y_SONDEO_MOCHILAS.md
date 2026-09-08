# Borrador: arbitraje del puerto OTG y secuencia de sondeo

**Estado:** borrador de diseño (sin código).
**Problema que resuelve:** hoy la vista MeshCore y el Slot 2 abren el mismo
OTG sin saber el uno del otro. Con dos mochilas y un solo puerto físico, hace
falta un árbitro con reglas explícitas.

## Dueño exclusivo

- Un solo `owner` del OTG a la vez: `MeshCore`, `C3Bridge`, `Flasher`,
  `Terminal` o `Nadie`.
- Reglas:
  1. Quien lo abre primero lo conserva hasta liberarlo (sin robo).
  2. El `BackpackManager` es el único que abre para **sondear**; los drivers
     reciben el puerto ya identificado (nunca sondean por su cuenta).
  3. El flasheo / la terminal piden el puerto al manager (cola con timeout,
     no apropiación).
- Si el dueño muere o cuelga (timeout de actividad configurable), el manager
  recupera el puerto y lo marca libre. Sin esto, un driver colgado secuestra
  el OTG para siempre.

## Secuencia de sondeo (al aparecer un USB o a petición)

```
abrir OTG (exclusivo, timeout 1 s)
  → enviar APP_START, esperar SELF_INFO (timeout 1.5 s)
      sí → MeshCore (descriptor por huella). FIN.
  → enviar status C3 (0xAA 0x55 ...), esperar status válido (timeout 1 s)
      sí → C3 bridge. FIN.
  → enviar "GET_ID\\n", esperar línea "ID ..." (timeout 1 s)
      sí → mochila nativa CBDos. FIN.
  → serie genérico (se ofrece a la terminal, no se registra como red).
liberar OTG
```

- Ningún sondeo modifica el dispositivo (todos son lecturas/consultas).
- Orden: de más específico a más genérico; MeshCore primero porque su
  `APP_START` es inocuo y su respuesta es inconfundible.
- Tiempo total acotado: <5 s en el peor caso.

## Hot-plug vs arranque

- Dispositivo presente al arrancar → sondeo diferido (el USB tarda ~1.5 s en
  enumerar; sondear antes da falso "nada").
- Dispositivo enchufado en caliente → evento del `UsbDeviceManager` →
  sondeo inmediato.
- Dispositivo retirado → el dueño recibe revocación, libera, y la interfaz
  se desregistra (el toggle decide si se reabre al reaparecer).

## Preguntas abiertas (a decidir antes de implementar)

1. ¿El sondeo vive en el P4 (donde está el OTG) o también debe funcionar si
   el gestor corre en otro lado? Hoy: solo P4.
2. ¿Timeout global configurable desde UI o constante? Propuesta: constante
   compilada + override por NVS para campo.
3. ¿Qué pasa si el usuario abre la terminal serie sobre un puerto con dueño?
   Propuesta: la terminal pide prestado (el dueño pausa) o se le niega con
   mensaje claro, nunca robo silencioso.
