# 📋 Plan Arquitectónico: Migración del Subsistema SSH en ESP32-S3 a `libssh2` (mbedTLS)

**Fecha:** 22 de Octubre de 2026  
**Estado:** ✅ Implementado y Validado en Build Dual (Pasos 1 a 4 Completados)  
**Categoría:** Subsistema Redes & HAL (`bsp/esp32_s3_jc3248/hal/`, `bsp/esp32_s3_jc3248/platformio.ini`)  
**Targets Involucrados:** ESP32-S3 (Guition JC3248W535) con efecto unificador en ESP32-P4 (Guition JC4880P443C)

---

## 1. Justificación y Diagnóstico Técnico

### 1.1. Problemática de `LibSSH-ESP32` en el S3
Actualmente, el ESP32-S3 compila contra la biblioteca `LibSSH-ESP32` en `platformio.ini`. Esta librería presenta tres fallas estructurales graves para un RTOS embebido:
1. **Ausencia de Hilos Reales (`ssh_threads_noop`):** Fue portada a Arduino sin soporte de pthreads. Todos los mutexes internos son stubs vacíos. El acceso asíncrono o la concurrencia entre tareas corrompe las estructuras internas de la sesión.
2. **Demanda Excesiva de Memoria en Pila:** Durante la negociación de claves criptográficas y el intercambio KEX, `libssh` requiere **entre 30 KB y 40 KB de stack**. En FreeRTOS, las pilas de tareas se reservan en DRAM interna (`pvPortMalloc`), consumiendo la limitada memoria estática y provocando desbordamientos de pila (*Stack Overflow*) o bloqueos de Watchdog.
3. **Fragmentación Arquitectónica:** Tener `libssh` en el S3 y `libssh2` en el P4 duplica el esfuerzo de mantenimiento y genera discrepancias de comportamiento en timeouts, canales PTY y reintentos.

### 1.2. Ventajas Técnicas de `libssh2` (mbedTLS)
1. **Motor Criptográfico Nativo:** Utiliza el backend `-DLIBSSH2_MBEDTLS`. En el ESP32-S3, `mbedtls` ya viene integrado y acelerado por hardware (AES, SHA) dentro del framework Arduino/ESP-IDF.
2. **Huella Mínima de Memoria:** `libssh2` fue diseñada específicamente para clientes ligeros. Funciona establemente con una pila de **apenas 4 KB a 6 KB**, frente a los 40 KB de `libssh`.
3. **Modo No Bloqueante Nativo:** Dispone de soporte oficial `libssh2_session_set_blocking(m_session, 0)` combinado con `select()` de LwIP en Core 0, permitiendo un despacho reactivo de datos con consumo casi nulo de CPU cuando no hay tráfico.
4. **Código HAL 100% Unificado:** La implementación `ISshClient` de `bsp/esp32_p4_jc4880/hal/hal_ssh_p4.cpp` se convierte en código compartido para ambas placas.

---

## 2. Estrategia de Integración en PlatformIO (ESP32-S3)

Para integrar `libssh2` en el entorno PlatformIO (`bsp/esp32_s3_jc3248`):

### 2.1. Fuentes de `libssh2` y Configuración
Se utilizará la misma fuente C99 de `libssh2 1.11.0` que ya opera en el P4 (`bsp/esp32_p4_jc4880/managed_components/skuodi__libssh2_esp`):
- Ubicación: Se instala como biblioteca local en `bsp/esp32_s3_jc3248/lib/libssh2` (o compartido en `core/lib/libssh2`).
- Flags de compilación obligatorios en `platformio.ini`:
  ```ini
  -DLIBSSH2_MBEDTLS
  -DHAVE_UNISTD_H
  -DHAVE_SYS_TIME_H
  -DHAVE_SYS_SELECT_H
  -DHAVE_SNPRINTF
  -DHAVE_O_NONBLOCK
  ```

### 2.2. Reemplazo de Dependencia en `platformio.ini`
- **Eliminar:** `https://github.com/cb-technologies/LibSSH-ESP32.git` de `lib_deps`.
- **Añadir:** Inclusión de la biblioteca local de `libssh2` ligada al `mbedtls` nativo.

---

## 3. Arquitectura del HAL (`hal_ssh_s3.cpp`)

El nuevo [hal_ssh_s3.cpp](file:///home/kaber420/Documentos/proyectos/cbdos/bsp/esp32_s3_jc3248/hal/hal_ssh_s3.cpp) implementará `ISshClient` de forma idéntica a `hal_ssh_p4.cpp`:

```
┌────────────────────────────────────────────────────────┐
│                   TerminalView (Core)                  │
│             m_sshStream (SshStreamAdapter)             │
└──────────────────────────┬─────────────────────────────┘
                           │ (cbdos::ssh::connect / openShell)
                           ▼
┌────────────────────────────────────────────────────────┐
│                ISshClient (HAL Unificado)               │
│                  S3SshClient / P4SshClient             │
├────────────────────────────────────────────────────────┤
│ - Socket TCP BSD no bloqueante (LwIP)                  │
│ - Sesión libssh2 sobre mbedTLS por hardware            │
│ - Mutex std::mutex para sincronización de sesión       │
│ - Tarea de lectura 's3_ssh_rx' anclada a Core 0        │
│   (Pila segura de 6 KB, select() reactivo con 40ms tv) │
└────────────────────────────────────────────────────────┘
```

---

## 4. Plan de Ejecución por Pasos

| Paso | Acción | Archivo(s) | Validación |
| :---: | :--- | :--- | :--- |
| **Paso 1** | Incorporar `libssh2` con backend mbedTLS para PlatformIO | `bsp/esp32_s3_jc3248/lib/libssh2/` | ✅ Verificado: cabeceras y fuentes C resueltos vía `library.json` |
| **Paso 2** | Actualizar `platformio.ini` del S3 | `bsp/esp32_s3_jc3248/platformio.ini` | ✅ Verificado: `LibSSH-ESP32` eliminado, `-DLIBSSH2_MBEDTLS` activo |
| **Paso 3** | Reescribir `hal_ssh_s3.cpp` con `libssh2` | `bsp/esp32_s3_jc3248/hal/hal_ssh_s3.cpp` | ✅ Verificado: compilación limpia exit code 0 (`esp32s3_red`) |
| **Paso 4** | Verificar compilación dual P4 y S3 | P4 (ESP-IDF) + S3 (PlatformIO) | ✅ Verificado: ambos targets compilan con exit code 0 |
| **Paso 5** | Prueba de hardware en ESP32-S3 | Flasheo manos libres en JC3248W535 | Pendiente de despliegue en dispositivo físico |

---

## 5. Criterios de Aceptación
1. **Compilación Limpia:** Dual-target garantizado (P4 con ESP-IDF y S3 con PlatformIO).
2. **Cero Pánico de Watchdog:** El hilo de conexión y handshake en Core 0 consume menos de 6 KB de pila y no bloquea el planificador.
3. **Flujo de Shell Interactivo:** El canal de terminal remota abre correctamente y envía/recibe tráfico bidireccional sobre la pantalla de CBDos.
