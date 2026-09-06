# 🔒 Especificación Técnica: Cifrado, Autenticación y Mitigación de Bufferbloat / Ataques por Inundación en CBDos

## 1. Arquitectura de Seguridad y Cifrado Multicapa

CBDos implementa un modelo de seguridad por capas que cubre: **el aire (RF)**, **el bus de datos interno (SDIO/MSPI)** y **la memoria física (Flash y PSRAM)**.

---

### 1.1. Esquemas Criptográficos en el Aire

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                          MODOS DE SEGURIDAD EN EL AIRE                      │
├────────────────────────────────┬────────────────────────────────────────────┤
│ MODO 1: GRUPAL / TÁCTICO       │ MODO 2: PRIVADO 1 A 1 (E2EE)               │
├────────────────────────────────┼────────────────────────────────────────────┤
│ • Clave Compartida (PSK)       │ • Claves Efímeras Curve25519 (X25519)      │
│ • Cifrado: AES-256-CTR / GCM   │ • Intercambio Diffie-Hellman (ECDH)        │
│ • Autenticación: HMAC-SHA256   │ • Cifrado de Sesión: AES-256-GCM           │
│ • Tag Autenticidad: 4 u 8 bytes│ • Perfect Forward Secrecy (PFS)            │
│ • Propósito: Escuadrones / Red │ • Propósito: Conversaciones Confidenciales │
└────────────────────────────────┴────────────────────────────────────────────┘
```

#### A. Modo Grupal (AES-256 + HMAC-SHA256)
1. **Derivación de Claves (HKDF):** De una contraseña compartida (PSK), se derivan:
   - $K_{enc}$ (Clave de 256 bits para cifrado AES).
   - $K_{auth}$ (Clave de 256 bits para firma HMAC).
2. **Estructura del Paquete en el Aire:**
   ```text
   [ MeshHeader (3-4B) ] [ Nonce/Contador (4B) ] [ Payload Cifrado AES-256 ] [ HMAC Tag (4B) ]
   ```
3. **Validación:** El receptor valida el HMAC antes de intentar descifrar el payload. Si no coincide, el paquete se destruye de inmediato.

#### B. Modo Privado Extremo a Extremo (E2EE con ECDH + AES-256)
1. **Identidad Criptográfica:** Cada CyberDeck cuenta con un par de claves asimétricas generadas localmente (Clave Privada en almacenamiento seguro, Clave Pública compartida como ID).
2. **Secreto Compartido:** Se calcula $S = \text{ECDH}(\text{Priv}_A, \text{Pub}_B)$.
3. **Cifrado Autenticado (AEAD):** Se utiliza **AES-256-GCM**, que cifra los datos y produce una etiqueta de autenticación (Tag) integrada en un solo paso acelerado por hardware.

---

### 1.2. Seguridad Física y en Memoria (Hardware-Accelerated)

1. **Cifrado de PSRAM al Vuelo (XTS-AES-256 Inline Encryption):**
   - En el **ESP32-P4**, toda transferencia entre la CPU y los **32 MB de Hexal-PSRAM** por el bus MSPI a 200 MHz se cifra y descifra automáticamente por hardware.
   - **Protección Anti-Forense:** Previene ataques de lectura directa de pistas (*Bus Sniffing*) o congelación de chips (*Cold Boot Attacks*).
2. **Cifrado de Flash SPI:**
   - Todo el firmware, almacenamiento SPIFFS/FATFS y claves privadas en Flash se encuentran cifrados con una clave única en eFuses protegidos contra lectura externa.
3. **Zeroization (Limpieza Segura):**
   - Las variables y buffers con claves de sesión o texto en claro se borran de inmediato usando `memset_s()` tras renderizarse en pantalla.

---

## 2. Mitigación de Bufferbloat, Saturación por Inundación (Flood) y Ataques DoS

Para evitar que ráfagas de interferencia, atacantes malintencionados o tormentas de broadcast congelen la CPU o agoten la memoria RAM, CBDos implementa **4 anillos de protección**:

```text
  [ RF / Paquetes Entrantes ]
              │
              ▼
   ┌──────────────────────┐
   │ 1. Filtro MAC/PAN ID │  ── (Descarte en Silicio / Hardware)
   └──────────┬───────────┘
              │ (Paquete válido para este nodo)
              ▼
   ┌──────────────────────┐
   │ 2. Early Drop HMAC   │  ── (Falla HMAC -> Destrucción en < 2 µs)
   └──────────┬───────────┘
              │ (Paquete legítimo)
              ▼
   ┌──────────────────────┐
   │ 3. Leaky Bucket      │  ── (Exceso de tasa -> Bloqueo temporal)
   └──────────┬───────────┘
              │
              ▼
   ┌──────────────────────┐
   │ 4. Ring Buffer Fijo  │  ── (Búfer Estático en PSRAM -> Cero allocs)
   └──────────┬───────────┘
              │
              ▼
    [ Procesamiento Core ]
```

---

### 2.1. Búferes Circulares Estáticos con Cero Asignación Dinámica (Anti-OOM)
- **Problema:** El uso de `malloc()` o `new` en interrupciones de radio permite que un atacante consuma el heap del microcontrolador hasta provocar un fallo de *Out-Of-Memory (OOM)*.
- **Implementación en CBDos:**
  - Se preasigna al arrancar un **Ring Buffer estático** de tamaño fijo (ej. 64 ranuras de 256 bytes = 16 KB fijos en PSRAM).
  - La memoria nunca crece en tiempo de ejecución.
  - Si el búfer se satura por tráfico extremo, se aplica descarte activo (**Tail-Drop / CoDel**): los paquetes excedentes se descartan en la capa física sin degradar el rendimiento del sistema operativo ni de la interfaz gráfica LVGL (que sigue corriendo a 60 FPS estables).

---

### 2.2. Descarte Temprano por Firma HMAC (Early Drop)
- Antes de copiar el paquete a la cola principal del sistema o notificar a la interfaz de usuario:
  1. El controlador de interrupción extrae los 4 bytes finales de HMAC.
  2. El hardware criptográfico ejecuta la comprobación de integridad.
  3. Si la firma es incorrecta (paquete falsificado, corrupto o ruido), **se descarta en menos de 2 microsegundos**, liberando el receptor inmediatamente.

---

### 2.3. Limitador de Tasa por Nodo (Leaky Bucket Rate Limiter)
- Para prevenir que un transmisor legítimo con fallas o un nodo hostil sature el canal:
  - Cada dirección de origen (`src_id`) tiene asignado un contador en una tabla Hash de bajo consumo.
  - **Límite Estricto:** Máximo **10 paquetes por segundo** por nodo.
  - Si un nodo excede el límite, entra en estado de **Penalización / Cooldown (5 segundos)** donde todos sus paquetes se silencian a nivel de controlador.

---

### 2.4. Protección contra Desbordamiento de Pila (Stack Protection & Canaries)
- Todas las estructuras de recepción utilizan contenedores de tamaño estrictamente delimitado (`std::array<uint8_t, MAX_FRAME_SIZE>`).
- Se mantiene activo el mecanismo de **FreeRTOS Stack Canaries**: si una tarea sufriera un desbordamiento, la CPU entra en pánico controlado y reinicia el subsistema de radio sin comprometer la memoria persistente ni filtrar claves criptográficas.

---

## 3. Resumen de Parámetros Técnicos

| Parámetro | Valor Estándar | Función de Seguridad |
| :--- | :--- | :--- |
| **Cifrado Simétrico** | AES-256-CTR / AES-256-GCM | Confidencialidad de datos en vuelo y en PSRAM |
| **Intercambio Asimétrico** | Curve25519 (X25519) | Establecimiento de sesión P2P sin servidor |
| **Firma e Integridad** | HMAC-SHA256 (Tag 4B / 8B) | Prevención de falsificación (Anti-Spoofing) |
| **Anti-Repetición** | Nonce de 32 bits + Timestamp | Prevención de ataques Replay |
| **Búfer de Recepción** | Ring Buffer Estático (64 slots) | Inmunidad total contra desbordamiento de memoria (Anti-OOM) |
| **Tasa Máxima por Nodo** | 10 paquetes / segundo | Mitigación de saturación por inundación (Anti-Flood) |
