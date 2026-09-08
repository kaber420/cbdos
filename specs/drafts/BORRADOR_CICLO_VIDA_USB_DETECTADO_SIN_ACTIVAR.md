# Borrador: ciclo de vida USB — detectado sin activar

**Estado:** borrador de diseño (sin código).
**Idea del usuario:** el sistema puede mantener el USB como *detectado* pero
*sin activar*: se sabe qué hay enchufado sin abrir el puerto ni gastar
recursos.

## Tres estados (máquina explícita)

```
AUSENTE → DETECTADO → ACTIVO
   ↑          ↓           ↓
   └──── (al retirar, desde cualquiera)
```

- **AUSENTE:** nada enchufado (o retirado). Sin entradas en UI salvo historial.
- **DETECTADO:** identificado (huella o `GET_ID`), puerto **cerrado**, sin
  driver corriendo, sin tráfico. El sistema lo muestra como disponible
  ("Mochila MeshCore 3A381F27 — lista") y recuerda binding y toggle.
- **ACTIVO:** puerto abierto en exclusiva por su driver, handshake hecho,
  interfaz registrada en red. Solo se llega aquí si el toggle está ON (auto)
  o el usuario lo activa a mano.

## Transiciones

| Evento | Efecto |
|--------|--------|
| hot-plug / enumeración | AUSENTE → DETECTADO (sondeo) |
| toggle ON + DETECTADO | → ACTIVO (apertura + handshake) |
| toggle OFF + ACTIVO | → DETECTADO (cierre limpio, se conserva identidad) |
| activación manual desde UI | → ACTIVO aunque el toggle esté OFF (una vez; no cambia el toggle) |
| retirada física | → AUSENTE desde donde esté (revocación al dueño si era ACTIVO) |
| driver colgado (timeout actividad) | → DETECTADO + aviso (recupera el puerto) |

## Por qué "detectado sin usar" importa

- **Cero costo:** sin hilos RX, sin tráfico, sin pelea por el OTG; varios
  candidatos pueden estar DETECTADOS a la vez (uno por puerto físico cuando
  haya hub; hoy uno).
- **Decisión informada:** la UI muestra qué hay antes de activar, con su
  descriptor (radio, protocolo, versión) para advertir incompatibilidades
  *antes* de abrir.
- **Arranque rápido:** el P4 no abre nada solo; solo detecta. Activar es
  explícito (toggle o gesto), nunca sorpresa.

## Preguntas abiertas

1. ¿DETECTADO caduca si el dispositivo duerme sin responder al sondeo
   periódico ligero? Propuesta: re-sondeo cada 30 s solo si sigue enumerado;
   si no responde 3 veces → AUSENTE + aviso.
2. ¿La terminal serie puede "espiar" un puerto DETECTADO sin activarlo?
   Propuesta: sí, en modo solo-lectura opcional (útil para diagnóstico), sin
   cambiar el estado.
