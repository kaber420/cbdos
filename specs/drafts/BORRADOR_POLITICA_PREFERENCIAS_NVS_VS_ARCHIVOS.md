# Borrador: política de preferencias NVS vs archivos (MessagePack)

**Estado:** borrador de diseño (sin código).
**Principio:** NVS no es para todo. NVS solo para lo pequeño, crítico para el
arranque y del sistema. Cada app/mochila guarda lo suyo en su propio
directorio de la partición de archivos (SPIFFS), en binario MessagePack.

## Qué va en NVS (poco, pequeño, crítico)

- Toggles del sistema y por tipo de mochila (`mochila/meshcore=ON`).
- Credenciales WiFi, brillo/volumen, zona horaria.
- Binding puerto→mochila (solo el enlace, no su configuración).
- Criterio: si cabe en ~tens de bytes, se lee en arranque y sin ello el
  sistema no sabe comportarse → NVS. Todo lo demás → archivo.

## Qué va en archivos MessagePack (por app / por mochila)

```
/spiffs/apps/<appId>/prefs.msgpack        ← preferencias de cada app
/spiffs/mochilas/<idEstable>/prefs.msgpack ← alias, canal, TX, etc. por mochila
/spiffs/mochilas/<idEstable>/canales.msgpack
```

- Cada app crea su directorio y versiona su formato (`v` como primer campo;
  lector tolerante: campo desconocido se ignora, campo ausente usa default).
- Por qué MessagePack y no JSON: binario compacto (menos flash y RAM al
  parsear), tipos reales (int/float/bool/bin), y ya hay precedente en el
  proyecto para serializar contactos/historial en binario. JSON solo para lo
  que el usuario deba editar a mano (casi nada).
- Escritura atómica: escribir a `prefs.tmp` + renombrar (evita prefs rotas si
  se apaga a mitad). Al leer algo corrupto: defaults + aviso, nunca crash.
- Desgaste: SPIFFS ya nivela; además, guardar con debounce (no en cada
  pulsación, sino al salir o cada N segundos si hubo cambios).

## Límites explícitos

- Nada mayor de ~4 KB en NVS jamás (límite práctico por entrada).
- Nada crítico para arrancar solo en archivo (si SPIFFS falla, el sistema
  arranca con defaults sanos).
- Migración: lo que hoy esté en NVS y no cumpla el criterio se mueve a
  archivo con conversor una sola vez (versión de migración guardada en NVS).

## Preguntas abiertas

1. ¿Librería MessagePack concreta (m5stack/msgpack, nanopb-like, propia
   mínima)? Criterio: sin mallocs incontrolados, apta para PSRAM/heap
   limitado del P4.
2. ¿Cuota por app (p. ej. 64 KB) para que una app no llene SPIFFS? Propuesta:
   sí, con error visible al superar.
