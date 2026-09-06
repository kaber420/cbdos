# Errores Arquitectónicos en la Implementación de Sincronización de Tiempo

## Resumen

La implementación de sincronización de tiempo introdujo conceptos incorrectos que mezclan
terminología de la arquitectura de red con la lógica de sincronización de tiempo.
Este documento registra los errores para su corrección futura.

---

## Error 1: Confusión de terminología — `TimeSource::Federated`

**Qué se hizo:** Se renombró `TimeSource::Mesh` a `TimeSource::Federated`.

**Por qué es incorrecto:**
- "Federada" es un concepto de **topología de red**. Los ASNs son federaciones.
  Las Zonas y Torres son subdivisiones de esa federación. Es arquitectura de routing.
- La hora **no viene de "la red federada"** como concepto abstracto.
  La hora viene de la **Torre/Gateway** (el C3 conectado a la SBC).
- El nombre correcto sería `TimeSource::Tower` o `TimeSource::Gateway`.

---

## Error 2: Concepto inventado — leer hora de "vecinos" clientes

**Qué se hizo:** Se agregó lógica para que un cliente CBDos (S3/P4) pueda leer
la hora de los beacons de **otros clientes CBDos** cercanos si estos "tienen hora".

**Por qué es incorrecto:**
- Un cliente **no es un gateway**. El S3 y el P4 son dispositivos de usuario final.
- Si un cliente tiene la hora mal, y otro la lee de él, ambos estarán mal.
- La única fuente autoritativa de tiempo es la **SBC/C3 gateway**, que tiene
  conexión a Internet y sincroniza via SNTP real.

---

## Error 3: RTT mal interpretado

**Qué se hizo:** En lugar de implementar compensación de RTT (Round Trip Time)
correctamente, se inventó un mecanismo de "lectura de beacons de vecinos".

**Cómo funciona RTT real en NTP:**
```
T1: Cliente manda solicitud
T2: Servidor recibe
T3: Servidor manda respuesta
T4: Cliente recibe

RTT = (T4 - T1) - (T3 - T2)
Hora corregida = T3 + RTT/2
```

- Para SNTP por Wi-Fi: el protocolo NTP ya maneja esto internamente.
- Para beacon del C3 por ESP-NOW: el beacon es broadcast unidireccional.
  RTT no aplica directamente. Solo aplicaría si se hace solicitud-respuesta.

---

## Error 4: `broadcastTowerBeacon()` en clientes

Los dispositivos S3 y P4 son clientes, nunca gateways.
El gateway es la SBC con el C3 como radio bridge. Es infraestructura fija.
Un cliente emitiendo beacons de tiempo sin tener internet es absurdo.

---

## Arquitectura Correcta

```
[Internet]
    |
   [SBC]          <- Fuente autoritativa de tiempo (SNTP real)
    |
   [C3]           <- Radio bridge, emite beacons ESP-NOW con epoch de SBC
    |
  [S3/P4 CBDos]   <- Solo reciben beacon del C3. NUNCA emiten hora.
```

---

## Tabla de limpieza pendiente

| Componente                              | Estado correcto              |
|-----------------------------------------|------------------------------|
| Toggle Auto-Sync                        | Mantener                     |
| SNTP por Wi-Fi cada 3 horas             | Mantener                     |
| Backoff exponencial si SNTP falla       | Mantener                     |
| Recibir beacon del C3 y aplicar epoch   | Mantener                     |
| `TimeSource::Federated`                 | Renombrar a `TimeSource::Tower` |
| `broadcastTowerBeacon()` en clientes    | Eliminar de core/            |
| `sendTowerProbe()` en `TimeManager`     | Eliminar                     |
| Jerarquía SNTP > Federated con lockout  | Eliminar                     |
| Leer hora de clientes vecinos           | Eliminar                     |
