# Especificación Técnica: Cliente de API MikroTik RouterOS (v6/v7) y Monitor Dedicado en CBDos

**Documento:** `specs/network/especificacion_cliente_api_mikrotik_routeros.md`  
**Módulo Core:** `cbdos::network::MikrotikClient`  
**Módulo UI:** `cbdos::ui::MikrotikRouterView`  
**Estado:** 📋 Especificación Blindada / Lista para Implementación  
**Fecha:** 28 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Auditoría AI:** Validado y refinado mediante `opencode/glm-5.3-flash-free`  

---

## 1. Justificación Arquitectónica: Separación Estricta de Ubiquiti y MikroTik

Aunque ambos fabricantes conviven en torres de telecomunicaciones (WISPs, ISPs y redes corporativas), responden a naturalezas operativas completamente distintas:

* **Ubiquiti (AirOS / AirMAX):** Dispositivo de **radio pura (Capa 1/2 RF)**. Su métrica esencial es la alineación de parábolas, constelación de modulación, balance de cadenas RSSI (Chain 0 vs Chain 1) y piso de ruido.
* **MikroTik (RouterOS):** Sistema de **enrutamiento, switching y control de tráfico (Capas 2, 3 y 4)**. Su métrica esencial son los puertos físicos (Ethernet/SFP), estado de enlace negociado (10M/100M/1G/10G), consumo en tiempo real por interfaz, asignación de leases DHCP, tablas ARP, reglas de firewall, salud física de placa (temperatura y voltaje) y registros inalámbricos secundarios.

**Decisión:** Se rechaza cualquier intento de unificar ambas vistas en una sola pantalla híbrida para evitar sobrecarga condicional (`if (isMikrotik)`). Se crea la suite **`MikrotikRouterView`**, completamente desacoplada de `OmniwispView`, especializada en la naturaleza de RouterOS y de mantenimiento trivial.

---

## 2. Capa de Transporte y Protocolo de Palabras (*Word Protocol*)

### 2.1. Conexión de Red Nativa en CBDos
El cliente operará de forma transparente sobre la abstracción de sockets existente en el sistema:
* **Contrato HAL:** [`cbdos::network::ISocketStream`](file:///home/kaber420/Documentos/proyectos/cbdos/core/include/cbdos/socket.hpp)
* **Instanciación:** `cbdos::network::createSocket(SocketType::Tcp)`
* **Puerto:** TCP `8728` (estándar plano). *Nota de alcance:* El puerto TLS `8729` queda explícitamente fuera de alcance para la Fase 1 al no ser requerido para administración táctica en red local/LAN.
* **Multi-Target:** Opera sin modificaciones sobre lwIP en el **ESP32-P4** ([`hal_socket_p4.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_p4_jc4880/hal/hal_socket_p4.cpp)) y sobre el stack de red en el **ESP32-S3** ([`hal_socket_s3.cpp`](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_socket_s3.cpp)).

### 2.2. Codificación Exhaustiva de Longitud de Palabras
Cada sentencia de RouterOS está compuesta por una secuencia de palabras (*words*), terminada por una palabra vacía (byte `0x00`). Antes de cada palabra se codifica su longitud en bytes según la siguiente tabla completa:

| Longitud del Texto (bytes) | Tamaño Prefijo | Codificación en Bytes (Big Endian) |
| :--- | :---: | :--- |
| **0 a 127** (`< 0x80`) | 1 byte | `length` directo (`0x00` .. `0x7F`) |
| **128 a 16,383** (`< 0x4000`) | 2 bytes | `0x80 \| (length >> 8)`, `length & 0xFF` |
| **16,384 a 2,097,151** (`< 0x200000`) | 3 bytes | `0xC0 \| (length >> 16)`, `(length >> 8) & 0xFF`, `length & 0xFF` |
| **2,097,152 a 268,435,455** (`< 0x10000000`) | 4 bytes | `0xE0 \| (length >> 24)`, `(length >> 16) & 0xFF`, `(length >> 8) & 0xFF`, `length & 0xFF` |
| **>= 268,435,456** | 5 bytes | `0xF0`, seguido de 4 bytes de longitud entera |

> ⚠️ **Regla de Protección de RAM (Word Cap):** Ninguna palabra en RouterOS para telemetría legítima excede unos cientos de bytes. Si el decodificador detecta un prefijo de longitud mayor a **16 KB (16,384 bytes)**, se asume corrupción de trama o desincronización de stream, abortando la lectura y reconectando el socket para evitar desbordamiento de memoria en el ESP32.

### 2.3. Autenticación Transparente Dual (v6 y v7)
RouterOS modificó su mecanismo de autenticación a partir de la versión 6.43. El cliente implementará auto-detección determinista:

```text
┌────────────────────────────────────────────────────────┐
│                   Cliente envía:                       │
│       /login                                           │
│       =name=<usuario>                                  │
│       =password=<clave>                                │
│       [byte 0x00]                                      │
└───────────────────────────┬────────────────────────────┘
                            │
               ¿Qué responde el Router?
               ┌────────────┴────────────┐
               ▼                         ▼
        [ Recibe !done ]          [ Recibe !done con ]
        Autenticado con éxito     [ =ret=HEX_CHALLENGE ]
        (RouterOS v7 y v6.43+)    (RouterOS v6.0..v6.42 Legacy)
                                         │
                                         ▼
                                  Calcula MD5:
                                  0x00 + clave + challenge_bytes
                                  Formatea en hex MINÚSCULA estricta
                                         │
                                         ▼
                                  Envía:
                                  /login
                                  =name=<usuario>
                                  =response=00<md5_hex_minusc>
                                  [byte 0x00]
                                         │
                                ┌────────┴────────┐
                                ▼                 ▼
                             !done             !trap
                             (OK)         (Clave errónea)
```

**Reglas de Autenticación:**
1. El challenge devuelto en `=ret=` por equipos legacy consta exactamente de **16 caracteres hex (8 bytes binarios)**.
2. El hash MD5 devuelto en `=response=00...` **debe estar en minúsculas** (RouterOS compara el string literal sensible a mayúsculas).
3. **Módulo Criptográfico Portable:** Para no depender de cabeceras divergentes entre ESP-IDF (P4) y Arduino Core (S3), se incluirá una implementación C agnóstica de MD5 en `core/src/network/mikrotik/md5.h` (~120 líneas libres de dependencias externas).
4. Si se recibe `!trap` durante el handshake, las credenciales son inválidas: **está prohibido reintentar en bucle infinito** para evitar bloqueos por fuerza bruta en el router.

---

## 3. Matriz de Casos Borde y Robustez en el Transporte

| # | Caso Borde | Comportamiento del Socket | Solución en `MikrotikClient` |
|---|---|---|---|
| 1 | **Fragmentación TCP** | Un paquete de red puede llegar dividido en múltiples fragmentos de `recv()`. | **Máquina de Estados Incremental:** `STATE_READ_LEN` ➔ `STATE_READ_PAYLOAD` ➔ `STATE_DISPATCH`. Si faltan bytes, el parser preserva el buffer parcial sin bloquear. |
| 2 | **Escritura Parcial (`send`)** | `ISocketStream::send()` puede escribir menos bytes de los solicitados si el buffer lwIP está saturado. | Implementación de `sendAll(data, len)` con bucle y reintentos hasta transferir la totalidad de la sentencia. |
| 3 | **Cierre por Inactividad (`!fatal`)** | RouterOS cierra conexiones API inactivas tras ~60s enviando la sentencia `!fatal`. | Captura inmediata de `!fatal`: marca el cliente como desconectado y notifica a la UI de forma transparente. |
| 4 | **Saturación de Memoria por Dumps Masivos** | Un router con cientos de interfaces o miles de leases puede enviar megabytes de texto. | **Mandato Estricto `.proplist`:** Toda consulta incluye solo los campos requeridos (ej: `*=.proplist=.id,name,type,running*`). Además, se topa el número de leases DHCP procesados a los 50 más recientes. |
| 5 | **Comandos In-Flight Solapados** | RouterOS API permite comandos simultáneos etiquetados con `.tag`, pero complica la lógica de buffers en MCU. | **Peticiones Secuenciales (1 in-flight):** Cola FIFO estricta donde cada consulta espera el `!done` o `!trap` de la anterior antes de disparar la siguiente. |

---

## 4. Fuentes Reales de Datos en RouterOS

Cumpliendo con la **Regla 6 (Cero Mocks / Cero Simulaciones)**, los datos se extraerán con precisión de sus comandos nativos:

1. **Hardware e Identidad:**
   * `/system/identity/print` ➔ Atributo `=name`.
   * `/system/routerboard/print` ➔ Atributos `=model`, `=current-firmware`.
   * `/system/resource/print` ➔ Atributos `=uptime`, `=cpu-load`, `=free-memory`, `=total-memory`, `=version`, `=architecture-name`.
   * `/system/health/print` ➔ Atributos `=voltage` y `=temperature` (si el router carece de sensores de salud y devuelve `!trap` o vacío, se marca `hasHealthSensors = false`).

2. **Puertos y Tráfico en Vivo (Port Matrix):**
   * **Nombre y Estado:** `/interface/print` con `*=.proplist=.id,name,type,running,disabled,mac-address,comment*`.
   * **Velocidad Negociada:** `/interface/ethernet/print` con `*=.proplist=.id,name,speed*`. La velocidad de enlace (10M/100M/1G/10G) se extrae de aquí.
   * **Cálculo de Tráfico en Vivo:** RouterOS API no provee tasa instantánea directa. Se calcula el **Delta de Bytes** entre dos ciclos de refresco consecutivos dividido entre la diferencia de tiempo:
     $$\text{Rate (Kbps)} = \frac{(\text{Bytes}_{\text{actual}} - \text{Bytes}_{\text{previo}}) \times 8}{\Delta t_{(\text{segundos})} \times 1000}$$

3. **Clientes LAN (DHCP Leases):**
   * `/ip/dhcp-server/lease/print` con `*=.proplist=.id,address,mac-address,host-name,status,expires-after*`.

4. **Registros Inalámbricos (v6 vs v7):**
   * Si `rosVersion` empieza por `"6."`: se consulta `/interface/wireless/registration-table/print`.
   * Si `rosVersion` empieza por `"7."`: se consulta primero `/interface/wifi/registration-table/print`; si no existe o devuelve error, se hace fallback a `/interface/wireless/registration-table/print`.

---

## 5. Arquitectura Multihilo y Seguridad de Hilos con LVGL v9.5

Cumpliendo con el modelo de hardware del ESP32:
* **Core 0:** Ejecuta las tareas de fondo de FreeRTOS (`mikrotik_worker_task`), controlando los sockets lwIP y decodificando los buffers binarios.
* **Core 1:** Ejecuta el loop de renderizado de LVGL v9.5 (`lvglTask`) a 60 FPS estables.

```text
┌─────────────────────────────────────────┐               ┌─────────────────────────────────────────┐
│                 CORE 0                  │               │                 CORE 1                  │
│       (MikrotikClient Worker Task)      │               │       (MikrotikRouterView / LVGL)       │
└────────────────────┬────────────────────┘               └────────────────────┬────────────────────┘
                     │                                                         │
  1. Recibe bytes de │                                                         │
     RouterOS :8728  │                                                         │ Renderiza pantalla
                     │                                                         │ a 60 FPS constante
  2. Parsea sentencia│                                                         │
     a C++ Struct    │                                                         │
                     │                                                         │
  3. Despacha evento │   lv_async_call([](void* data) {                        │
     thread-safe ────┼──►    // Ejecutado en Core 1                            │
                     │       auto cur = UIManager::getCurrentView();           │
                     │       if (cur && cur->getName() == "MikrotikRouterView")│
                     │           cur->renderData(data);                        │
                     │   });                                                   │
                     │                                                         │ 4. Core 1 pinta
                     │                                                         │    la UI sin locks
```

---

## 6. Diseño de Interfaz Táctil: `MikrotikRouterView` (LVGL v9.5)

Derivada de `BaseView`, alojada dentro de `m_container` y con integración obligatoria a `HeaderBar`.

```text
+--------------------------------------------------------------------------+
| [ < ]  MIKROTIK ROUTERBOARD: Core-CerroSur              [v7.14.3] [100%] |
+--------------------------------------------------------------------------+
|  IDENTIDAD Y HARDWARE                                                    |
|  +--------------------+  +-------------------+  +---------------------+  |
|  | MODEL: RB4011iGS+  |  | CPU: [|||||   ]24%|  | RAM: 820MB / 1024MB |  |
|  | TEMP:  39.5 °C     |  | VOLT: 24.1 V      |  | UPTIME: 14d 06:12:00|  |
|  +--------------------+  +-------------------+  +---------------------+  |
|                                                                          |
|  MATRIZ DE PUERTOS FÍSICOS (PORT MATRIX)                                 |
|  +--------+ +--------+ +--------+ +--------+ +--------+ +--------+       |
|  |  eth1  | |  eth2  | |  eth3  | |  eth4  | |  eth5  | |  sfp1  |  ...  |
|  | [1 Gbps| | [1 Gbps| |  DOWN  | |[100 Mbps| |  DOWN  | |[10 Gbps|       |
|  |▲ 12.4M | |▲  1.2M | |        | |▲  0.1M | |        | |▲ 85.0M |       |
|  |▼ 45.1M | |▼  0.8M | |        | |▼  0.3M | |        | |▼120.4M |       |
|  +--------+ +--------+ +--------+ +--------+ +--------+ +--------+       |
|                                                                          |
|  [ PESTAÑAS: [ PUERTOS ]   [ CLIENTES DHCP ]   [ WIRELESS ]   [ CONTROL ]]
|  +--------------------------------------------------------------------+  |
|  | IP             MAC                HOSTNAME          EXPIRA EN      |  |
|  | 192.168.88.10  00:0C:42:11:22:33  CPE-Residencial1  06:45:10       |  |
|  | 192.168.88.24  B4:FB:E4:99:88:77  Camara-DomoSur    18:12:00       |  |
|  | 192.168.88.50  BC:24:11:AA:BB:CC  Laptop-Tecnico    01:20:00       |  |
|  +--------------------------------------------------------------------+  |
+--------------------------------------------------------------------------+
```

### Componentes Visuales:
* **Tarjeta de Hardware:** Muestra identidad, modelo RouterBOARD, CPU (barra visual), RAM, temperatura y voltaje en tiempo real.
* **Matriz de Puertos (Port Matrix):**
  * Cada puerto físico es un botón táctil con indicación visual de estado:
    * **Verde esmeralda (`#00F5D4` / `#10B981`):** Enlace activo 1 Gbps / 10 Gbps.
    * **Ámbar (`#F59E0B`):** Enlace activo 100 Mbps / 10 Mbps (posible problema de cableado o PoE).
    * **Gris oscuro (`#1E293B`):** Puerto desconectado (Link Down).
    * **Rojo (`#EF4444`):** Deshabilitado administrativamente.
  * Muestra el consumo instantáneo (TX / RX) en megabits o kilobits por segundo.
* **Pestañas de Contexto:**
  * **Puertos:** Lista tabular con detalles de MAC, MTU y flags.
  * **Clientes DHCP:** Tabla de leases activos con IP, MAC y Hostname.
  * **Wireless:** Clientes inalámbricos asociados si el equipo cuenta con radio.
  * **Control:** Reinicio táctil del equipo (`/system/reboot`) con modal de confirmación.

---

## 7. Plan de Implementación por Fases

- [ ] **Fase 1: Motor de Protocolo y Criptografía (`core/src/network/mikrotik/`)**
  - Implementar rutina portable C `md5.h` (agnóstica de plataforma).
  - Implementar `MikrotikCodec` con máquina de estados incremental y protección contra fragmentación TCP.
  - Implementar handshake de autenticación dual (detección v7/v6.43+ directo y fallback MD5 legacy).
  - Validación con vectores de prueba sintácticos (prefijos de 1, 2, 3, 4 y 5 bytes).

- [ ] **Fase 2: Cliente Asíncrono de Red (`MikrotikClient`)**
  - Tarea FreeRTOS worker en Core 0 con pila de 8 KB.
  - Gestión de cola de comandos secuenciales (1 in-flight).
  - Consultas con `.proplist` estricto y cálculo de throughput por delta de tiempo.
  - Despacho seguro de respuestas hacia Core 1 con `lv_async_call`.

- [ ] **Fase 3: Vista de Usuario en LVGL v9.5 (`MikrotikRouterView`)**
  - Implementar la vista en `core/src/ui/views/mikrotik/MikrotikRouterView.cpp`.
  - Construir la Matriz Táctil de Puertos (Port Matrix) dinámica.
  - Pestañas para Leases DHCP, Recursos y Control.

- [ ] **Fase 4: Integración al Sistema y Validación Multi-Target**
  - Registrar vista en `UIManager` y enlazar acceso rápido desde el Dashboard / Suite de Red.
  - Compilación limpia en **ESP32-P4 (ESP-IDF)**:
    `. /home/kaber420/esp/esp-idf/export.sh && idf.py -C bsp/esp32_p4_jc4880 build`
  - Compilación limpia en **ESP32-S3 (PlatformIO)**:
    `pio run -d bsp/esp32_s3_jc3248`
