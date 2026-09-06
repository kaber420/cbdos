# 🌐 Plan de Arquitectura: Fase 3 - Sockets, Streaming y Desacoplamiento de Red por Sub-Fases (CBDos v0.2.1)

## 📌 1. Visión y Objetivo Arquitectónico

El objetivo fundamental de la **Fase 3** es erradicar de forma segura y modular todas las inclusiones de sockets POSIX crudos (`<sys/socket.h>`, `<netdb.h>`, `<arpa/inet.h>`, `<unistd.h>`) en el `core/` de **CBDos**.

Esto garantiza que el streaming de audio y la navegación TLV utilicen contratos abstractos en C++ inyectados por el BSP, sin romper la compilación ni las funcionalidades en ningún target (**ESP32-P4** y **ESP32-S3**).

---

## 🤖 2. Modelo de Orquestación Multi-Agente (Loop Antigravity + OpenCode)

Para acelerar el desarrollo, asegurar calidad estricta y evitar regresiones:

1. **Orquestador Principal (Antigravity):**
   - Diseña e implementa los contratos HAL.
   - Prepara los archivos de implementación y adapta los puntos de entrada del BSP.
   - Lanza las compilaciones cruzadas (`idf.py build` y `pio run`).

2. **Revisor & Auditor Secundario (OpenCode Subagent):**
   - Ejecuta revisiones estáticas de código mediante `opencode run` para verificar ausencia de fugas de memoria, punteros nulos y gestión correcta de timeouts.
   - Audita que ninguna inclusión prohibida permanezca en los archivos refactorizados.

3. **Loop de Validación Multi-Target:**
   - Compilación cruzada en paralelo: `idf.py build` (ESP32-P4) + `pio run -d bsp/esp32_s3_jc3248` (ESP32-S3).

```
┌─────────────────────────────────────────────────────────────┐
│                 ORQUESTADOR (Antigravity)                   │
│   • Genera Contratos HAL y código en core/ y bsp/           │
└──────────────────────────────┬──────────────────────────────┘
                               │ Entrega código
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 AUDITOR (OpenCode Subagent)                 │
│   • Inspección estática: opencode run "auditar..."          │
│   • Validación de Zero Leaks y Zero Forbidden Includes      │
└──────────────────────────────┬──────────────────────────────┘
                               │ Aprobado
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                 BUILD MULTI-TARGET CI LOOP                  │
│   • ESP32-P4: idf.py build (ESP-IDF 5.5)                    │
│   • ESP32-S3: pio run (PlatformIO Arduino)                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 📋 3. Plan Detallado por Sub-Fases

---

### 🔹 Sub-Fase 3.1: Contrato HAL `ISocketStream` y Adaptadores BSP (Base Segura)

#### Objetivo
Definir la interfaz abstracta `ISocketStream` y proveer sus implementaciones concretas en ambos microcontroladores sin modificar aún el código de las aplicaciones (`TlvBrowserView` / `AudioPlayer`).

#### 1. Definición del Contrato HAL
* **Archivo:** `core/include/cbdos/socket_stream.hpp`
* **Definición C++:**
  ```cpp
  #pragma once
  #include <cstdint>
  #include <cstddef>
  #include <memory>

  namespace cbdos {
  namespace net {

  class ISocketStream {
  public:
      virtual ~ISocketStream() = default;
      virtual bool connect(const char* host, uint16_t port, uint32_t timeoutMs = 5000) = 0;
      virtual int write(const uint8_t* data, size_t len) = 0;
      virtual int read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs = 1000) = 0;
      virtual bool isConnected() const = 0;
      virtual void close() = 0;
  };

  class ISocketFactory {
  public:
      virtual ~ISocketFactory() = default;
      virtual std::unique_ptr<ISocketStream> createSocket() = 0;
  };

  void setSocketFactory(ISocketFactory* factory);
  ISocketFactory* getSocketFactory();
  std::unique_ptr<ISocketStream> createSocket();

  } // namespace net
  } // namespace cbdos
  ```

#### 2. Implementaciones en BSP
* **Target ESP32-P4:** `bsp/esp32_p4_jc4880/hal/hal_socket_p4.cpp`
  * Implementación basada en LwIP nativo (`lwip/sockets.h`).
* **Target ESP32-S3:** `bsp/esp32_s3_jc3248/hal/hal_socket_s3.cpp`
  * Implementación basada en `WiFiClient` de Arduino / Framework Expressif.
* **Registro en arranque:** Invocar `cbdos::bsp::initSocketTransport()` en los `main.cpp` de ambos targets.

#### 3. Bucle de Validación (Sub-Fase 3.1)
```bash
# 1. Auditoría con OpenCode
opencode run "Verificar que hal_socket_p4.cpp y hal_socket_s3.cpp manejen timeouts y cierres de socket sin fugas de memoria"

# 2. Compilación ESP32-P4
. /home/kaber420/esp/esp-idf/export.sh && cd bsp/esp32_p4_jc4880 && idf.py build

# 3. Compilación ESP32-S3
pio run -d bsp/esp32_s3_jc3248
```

---

### 🔹 Sub-Fase 3.2: Migración Aislada de `TlvBrowserView`

#### Objetivo
Reemplazar todos los sockets directos en la vista del navegador TLV por llamadas a `cbdos::net::createSocket()`.

#### 1. Modificaciones
* **Archivo:** `core/src/ui/views/TlvBrowserView.cpp`
  * Retirar:
    ```cpp
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    ```
  * Reemplazar la tarea `fetch_task` para que cree un socket mediante `cbdos::net::createSocket()` y lea la respuesta del Gateway por flujo.

#### 2. Bucle de Validación (Sub-Fase 3.2)
```bash
# 1. Auditoría con OpenCode
opencode run "Auditar TlvBrowserView.cpp y verificar que no queden llamadas a socket(), connect(), ni includes POSIX"

# 2. Compilaciones cruzadas
idf.py build && pio run -d bsp/esp32_s3_jc3248
```

---

### 🔹 Sub-Fase 3.3: Migración de Streaming en `AudioPlayer`

#### Objetivo
Desacoplar la recepción de flujo continuo de audio MP3/AAC/FLAC por HTTP en el reproductor de radio online.

#### 1. Modificaciones
* **Archivo:** `core/src/audio/AudioPlayer.cpp`
  * Retirar includes de sockets crudos.
  * Reemplazar la conexión de streaming HTTP por `ISocketStream`.
* **Archivo:** `core/src/audio/AudioPlayer.hpp`
  * Retirar cualquier descriptor de socket o tipo crudo.

#### 2. Bucle de Validación (Sub-Fase 3.3)
```bash
# 1. Auditoría con OpenCode
opencode run "Verificar que AudioPlayer.cpp no contenga includes de sys/socket.h y que el streaming sea gestionado correctamente por ISocketStream"

# 2. Compilación Dual Final
idf.py build && pio run -d bsp/esp32_s3_jc3248

# 3. Verificación de Cero Infracciones en Core
grep -rnE "#include <(sys/socket\.h|netdb\.h|arpa/inet\.h|unistd\.h)>" core/
```

---

## ✅ 4. Criterio de Éxito de la Fase 3 Completa

1. `core/` tiene **CERO** llamadas o includes a sockets POSIX crudos.
2. `TlvBrowserView` y `AudioPlayer` funcionan de forma transparente tanto en ESP32-P4 como en ESP32-S3.
3. Ambos targets compilan al 100% sin advertencias ni errores.
