# Borrador: Arquitectura y Forma de CBDos

> Estado: BORRADOR — Para discusión y definición con el desarrollador.
> No refleja el estado actual del código sino el objetivo de diseño.

---

## 1. Cómo se ve un Sistema Operativo serio

Un OS embebido bien estructurado tiene capas claras.
Cada capa solo habla con la que tiene justo encima o debajo.

```
┌───────────────────────────────────────────────────────┐
│                    APLICACIONES                        │
│  Reproductor  Browser  Terminal  Reloj  Configuración  │
│  (usan la API del sistema, no saben nada de hardware)  │
├───────────────────────────────────────────────────────┤
│               API DEL SISTEMA (Syscall)                │
│  La interfaz pública. Una sola capa, coherente.        │
│  Ejemplos reales: POSIX, Win32, Android SDK            │
│                                                        │
│  display_flush()  audio_play()  net_send()             │
│  time_get()       storage_read() input_poll()          │
├───────────────────────────────────────────────────────┤
│           SERVICIOS DEL KERNEL (internos)              │
│  Implementan la lógica detrás de la API.               │
│  No son accesibles directamente por las apps.          │
│                                                        │
│  Scheduler  MemMgr  IPC  FileSystem  NetworkStack      │
├───────────────────────────────────────────────────────┤
│        HAL — Hardware Abstraction Layer                │
│  Interfaces abstractas. No saben de plataforma.        │
│  IDisplayDriver  IAudioSink  INetworkInterface         │
│  ITimeProvider   IStorageBackend  IInputDevice         │
├───────────────────────────────────────────────────────┤
│              DRIVERS (plataforma-específico)           │
│  Implementan los HAL para hardware concreto.           │
│  ST7701Driver  ES8311Sink  EspNowInterface             │
│  EspIdfTimeProvider  SdmmcBackend  GT911Input          │
├───────────────────────────────────────────────────────┤
│              KERNEL / RTOS                             │
│  FreeRTOS — tareas, colas, semáforos, memoria          │
├───────────────────────────────────────────────────────┤
│              HARDWARE                                  │
│  ESP32-S3  /  ESP32-P4  /  periféricos               │
└───────────────────────────────────────────────────────┘
```

**La regla de oro:** Una capa NUNCA salta hacia abajo más de un nivel.
Una app nunca habla con un driver directamente. Un servicio nunca habla con hardware directamente.

---

## 2. Cómo está CBDos ahora (realidad)

```
┌───────────────────────────────────────────────────────┐
│  UI / Apps (core/src/ui/)                             │
│  Vistas, componentes, modales                         │
│  ← BIEN: usan cbdos:: namespace como API             │
├───────────────────────────────────────────────────────┤
│  Módulos del sistema (core/src/)                      │
│  cbdos::time  cbdos::audio  cbdos::mesh               │
│  cbdos::storage  cbdos::network  cbdos::ui            │
│                                                        │
│  ← PROBLEMA: los módulos a veces se llaman entre sí   │
│    directamente en vez de pasar por una API central.   │
│    Ejemplo: time llamaba a mesh, mesh llamaba a time.  │
│                                                        │
│  ← PROBLEMA: "mesh" no es un nombre estándar.         │
│    Debería ser NetworkInterface o similar.             │
├───────────────────────────────────────────────────────┤
│  Interfaces abstractas (core/include/)                │
│  ITimeProvider  IAudioSink                            │
│  ← INCOMPLETO: falta INetworkInterface, IDisplay, etc. │
│    El MeshEngine/NetworkInterface no tiene interfaz    │
│    abstracta propia — expone la clase concreta.        │
├───────────────────────────────────────────────────────┤
│  HAL Concreto (bsp/hal/)                              │
│  EspIdfTimeProvider  ArduinoTimeProvider              │
│  EspNowTransport  (drivers de pantalla, touch, audio) │
│                                                        │
│  ← PROBLEMA: el BSP main.cpp también conecta módulos  │
│    de software entre sí, lo cual no le corresponde.   │
├───────────────────────────────────────────────────────┤
│  FreeRTOS + ESP-IDF / Arduino                         │
├───────────────────────────────────────────────────────┤
│  Hardware (ESP32-S3 / ESP32-P4)                       │
└───────────────────────────────────────────────────────┘
```

---

## 3. Problemas identificados

### 3.1 Los módulos de `core/` no tienen una API unificada
Cada módulo tiene su propio namespace (`cbdos::time::`, `cbdos::mesh::`, etc.)
pero no hay una capa única que centralice el acceso. Las apps importan
directamente el header del módulo que necesitan.

Esto no es necesariamente malo, pero significa que no hay un "contrato"
único entre el OS y las apps — cualquier cambio interno de un módulo puede
romper las apps.

### 3.2 El "NetworkInterface" (MeshEngine) no es abstraído
`MeshEngine` es una clase concreta que las vistas y otros módulos usan
directamente. No existe una interfaz `INetworkInterface` equivalente
al `ITimeProvider` que tienen el módulo de tiempo.

### 3.3 El BSP hace trabajo que no le corresponde
El `main.cpp` del BSP conecta módulos de software (`time` ↔ `mesh`).
Eso debería estar en `cbdos_core.cpp` — el inicializador del OS.

### 3.4 Nombres inconsistentes
- `MeshEngine` → debería ser `NetworkInterface` o similar
- `TimeSource::Federated` → ya corregido a `TimeSource::Tower`
- "Federated" como término de red → es topología, no nombre de componente

### 3.5 Arquitectura Unificada de Tiempo: API Única con Ingesta Dual Desacoplada
El sistema **no tiene RTC** físico. La hora real solo existe cuando un medio de red (Wi-Fi o Radio) la suministra:

#### 1. API Pública Unificada para Aplicaciones (`cbdos::time`)
Las aplicaciones (HeaderBar, editores, logs, explorador de archivos) acceden a una **única API agnóstica**:
```cpp
cbdos::time::getFormattedTime(buf, len); // "15:20"
cbdos::time::getEpoch();                 // time_t Unix
cbdos::time::isSynced();                 // bool
cbdos::time::getSource();                // SNTP / Tower / Local
```
A las aplicaciones no les interesa la procedencia de la hora ni la topología de red.

#### 2. Pipeline de Ingesta Dual Desacoplado
Por debajo, el reloj central del OS se alimenta de dos orígenes independientes:

```
                  ┌──────────────────────────────────────────┐
                  │       cbdos::time (Reloj Central)        │
                  │        setEpoch(epoch, TimeSource)       │
                  └──────────────▲────────────▲──────────────┘
                                 │            │
            ┌────────────────────┴───┐    ┌───┴────────────────────┐
            │  Origen 1: Wi-Fi       │    │  Origen 2: Radio       │
            │  (SNTP Tradicional)    │    │  (Micro-Broadcast PoP) │
            ├────────────────────────┤    ├────────────────────────┤
            │ • Activo solo con Wi-Fi│    │ • Siempre activo       │
            │ • pool.ntp.org         │    │ • Micro-Broadcast 7B   │
            │ • ITimeProvider (BSP)  │    │ • 0 bytes TX (Pasivo)  │
            │ • Marca: TimeSource::  │    │ • Marca: TimeSource::  │
            │          SNTP          │    │          Tower         │
            └────────────────────────┘    └────────────────────────┘
```

- **En modo Cyberdeck (Wi-Fi OFF):** El radio escucha el micro-broadcast de 7 bytes del PoP cada 60s, llama a `setEpoch(epoch, TimeSource::Tower)` y ajusta el reloj local con cero consumo de transmisión.
- **En modo Estación (Wi-Fi Conectado a Router):** El `ITimeProvider` ejecuta la consulta SNTP estándar a Internet y llama a `setEpoch(epoch, TimeSource::SNTP)`.

Ambos flujos son transparentes y no requieren APIs separadas ni acoplamientos entre módulos.

---

## 4. Arquitectura de Red: Broadcast por Hash y Servicios del PoP

### 4.1 El Problema del Nodo Desconectado
Un usuario que enciende su Cyberdeck en una zona sin Internet no sabe qué recursos existen en el aire. Sin una guía o portal de inicio, el dispositivo se queda "en ceros".

### 4.2 Solución: Micro-Broadcast de Metadatos de 7 Bytes (Zero Overhead)
En lugar de saturar el canal de radio transmitiendo páginas completas o nombres en texto plano, el PoP (Gateway/Torre) emite periódicamente un **micro-payload de exactamente 7 bytes**. 

*Nota: La MAC de origen y el Short ID derivado (`0xMAC[4]MAC[5]`) ya son entregados de forma nativa y gratuita por la capa física del radio (ESP-NOW), por lo que no se desperdicia ningún byte en identificadores dentro del payload.*

```
┌─────────────────────────┬──────────────────────────┬──────────────────────┐
│  Unix Epoch (Hora)      │  Hash Portada (CRC16)    │  Status Code         │
│        (4 Bytes)        │        (2 Bytes)         │      (1 Byte)        │
└─────────────────────────┴──────────────────────────┴──────────────────────┘
```

#### Desglose del Payload (7 Bytes):
1. **Unix Epoch (4 Bytes - uint32_t):** Tiempo Unix actual en segundos. El cliente ajusta su reloj del sistema de inmediato al recibirlo sin realizar ninguna petición (Sincronización pasiva tipo SNTP).
2. **Hash Portada (2 Bytes - uint16_t):** Checksum/CRC16 de la versión actual de la página de inicio o guía del PoP.
   - **Caché Válido:** Si el cliente ya tiene almacenado ese Hash en memoria Flash/PSRAM, abre la guía en 0 ms con 0 bytes de tráfico de radio.
   - **Caché Nuevo/Modificado:** Si el Hash no coincide y el usuario abre el navegador, el cliente solicita ese contenido puntual vía Unicast (`GET_CONTENT(Hash)`).
3. **Status Code (1 Byte - Bitmask de Capacidades y Estado):**
   - **Bit 0 (`INTERNET_UP`):** `1` = Salida activa a Internet global / `0` = Red estrictamente local.
   - **Bit 1 (`PROXY_OPEN`):** `1` = Proxy de navegación libre / `0` = Requiere autenticación/portal cautivo.
   - **Bit 2 (`BAAS_BUSY`):** `1` = Servidor digestor web (Tabs as a Service) ocupado / `0` = Disponible.
   - **Bit 3 (`ALERT_ACTIVE`):** `1` = Hay un aviso o alerta activa en el tablón del PoP.
   - **Bits 4..7:** Reservados para telemetría adicional o métricas de carga del PoP (0-15).

#### Cadencia de Emisión (Intervalo):
- **Intervalo Estándar:** **Cada 60 segundos (1 minuto)**. Es información indispensable para el ecosistema pero no urgente; 60 segundos mantiene el canal de radio con un uso de aire prácticamente cero y respeta la autonomía de batería de toda la red.
- **Disparo por Evento (Opcional):** Si el estado del PoP cambia drásticamente (ej. caída de Internet o alerta urgente), el Gateway puede emitir 1 ráfaga inmediata sin esperar al minuto.

### 4.3 Tablón de Anuncios y Portal del PoP (BCML / LVGL-TLV)
La portada del PoP funciona como la **Guía del Ecosistema Local**, ofreciendo servicios sin requerir Internet global:
- 💬 **Chat y Mensajería Local** (Malla/Radio).
- 📖 **Wiki y Mapas Locales** (Documentación offline almacenada en el Gateway).
- ⚙️ **Autenticación / Salida Proxy** (Acceso a Internet controlado/pagado).
- 🌐 **Tabs as a Service (BaaS - Browser as a Service)**.

### 4.4 Tabs as a Service / TLVGL Digestión Web (Arquitectura Existente y Funcional)
Este mecanismo ya fue diseñado y probado con éxito en el sistema mediante el motor `TLVGL / BCML`:
- El servidor Gateway procesa el contenido web y lo compila a bytecode `BCML / LVGL-TLV`.
- Se transmite en ráfagas ultra-densas sobre ESP-NOW (Normal y Long Range).
- El cliente (S3/P4) interpreta el stream binario y renderiza los widgets nativos de LVGL directamente a 60 FPS.
- La integración con el nuevo **Broadcast por Hash** complementa este sistema permitiendo el descubrimiento instantáneo y el cacheo local de la portada.

---

## 5. Objetivo de diseño (hacia dónde debería ir)

```
┌───────────────────────────────────────────────────────┐
│  Apps / UI                                            │
│  Usan la API pública: cbdos::display, cbdos::time,   │
│  cbdos::net, cbdos::audio, cbdos::storage             │
├───────────────────────────────────────────────────────┤
│  API del Sistema (core/include/cbdos/*.hpp)           │
│  Contratos estables. Nunca exponen internos.          │
├───────────────────────────────────────────────────────┤
│  Servicios del sistema (core/src/)                    │
│  cbdos::time  ←→  cbdos::net  (ambos al mismo nivel) │
│  cbdos::audio    cbdos::storage    cbdos::ui          │
│  Los módulos NO se conocen entre sí directamente.     │
│  Se comunican solo via callbacks inyectados al inicio.│
├───────────────────────────────────────────────────────┤
│  Adaptadores de plataforma (bsp/)                     │
│  Abstraen APIs específicas de ESP-IDF o Arduino.      │
│  NtpAdapter (esp_sntp_* / configTime)                 │
│  Drivers de pantalla, touch, audio, radio             │
├───────────────────────────────────────────────────────┤
│  FreeRTOS + ESP-IDF / Arduino                         │
├───────────────────────────────────────────────────────┤
│  Hardware                                             │
└───────────────────────────────────────────────────────┘
```

---

## 5. Pasos propuestos (para discutir con el desarrollador)

1. **Definir y documentar la API pública** de cada módulo como contrato estable.
2. **Crear `INetworkInterface`** equivalente al `ITimeProvider` para el módulo de red.
3. **Mover la conexión time↔mesh** de `bsp/main.cpp` a `core/cbdos_core.cpp`.
4. **Renombrar `MeshEngine`** a algo que refleje su función real (a definir).
5. **Definir qué entra en `cbdos_core.cpp`** vs qué entra en el BSP.

Ninguno de estos pasos requiere reescribir el sistema — son cambios graduales y quirúrgicos.

---

## Referencias
- Arquitectura de Linux: https://www.kernel.org/doc/html/latest/
- Arquitectura de FreeRTOS: https://www.freertos.org/Documentation/
- Proyecto espOS32 (fuente original): `/home/kaber420/Documentos/proyectos/espOS32`
