# 📋 Plan de Implementación: Fase 1 - Pila TLS/HTTPS, Sockets Seguros y Restauración de Radio Online

**Fecha:** 1 de Noviembre de 2026  
**Estado:** Propuesta de Plan para Aprobación del Usuario  
**Ubicación:** `specs/architecture/plan_fase_1_pila_tls_https_y_radio_online.md`  
**Objetivo Primario:** Implementar el soporte TLS 1.2/1.3 nativo, habilitar el bundle de CAs de Mozilla, restaurar la reproducción de estaciones de **Radio Web Online por HTTPS** en `AudioPlayer`, y dejar la infraestructura de red lista para el futuro cliente KVM.

---

## 1. Justificación y Diagnóstico Técnico

### 1.1. ¿Por qué dejaron de funcionar las radios HTTPS?
Al auditar `core/src/audio/AudioPlayer.cpp` (L309-L343):
1. Cuando una URL de radio comienza por `https://` o una estación `http://` emite una redirección `301/302 Moved Permanently` hacia HTTPS:
   ```cpp
   if (urlToParse.rfind("https://", 0) == 0) {
       urlToParse = urlToParse.substr(8);
       portNum = 443;
   }
   client = cbdos::network::createSocket(cbdos::network::SocketType::Tcp);
   ```
2. El sistema abre un socket **TCP plano sin cifrar al puerto 443**.
3. Al enviar la cabecera `GET / HTTP/1.1`, el servidor HTTPS remoto (Cloudflare, Icecast/Shoutcast seguro, etc.) espera un *TLS ClientHello*. Al recibir texto plano en el puerto SSL, el servidor aborta la conexión inmediatamente arrojando:
   `HTTP/1.1 400 Bad Request: The plain HTTP request was sent to HTTPS port` o cerrando el socket (`nRead <= 0`).

### 1.2. Causa Raíz en la Capa HAL:
* `cbdos::network::SocketType` solo define `{ Tcp, Udp }`; **no existe `SocketType::Tls`**.
* En **ESP32-P4**, `P4SocketStream` usa sockets BSD planos (`sys/socket.h`).
* En **ESP32-S3**, `S3SocketStream` usa `WiFiClient` plano.
* En la capa REST (`hal_http_p4.cpp`), no está enlazado `esp_crt_bundle_attach` ni activo `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY`.

---

## 2. Alcance de la Fase 1

```
+-------------------------------------------------------------+
|             CAPA DE APLICACIÓN (Audio / Core)               |
|                                                             |
|   RadioManager / AudioPlayer (Helix MP3 Decoder)            |
|   -> Detección de URL (http:// vs https://)                 |
|   -> Creación de Socket según protocolo:                    |
|      - HTTP  -> createSocket(SocketType::Tcp)               |
|      - HTTPS -> createSocket(SocketType::Tls)               |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|               CAPA DE SOCKETS (cbdos::network)              |
|                                                             |
|   enum class SocketType { Tcp, Udp, Tls };                  |
|   virtual bool connect(host, port, timeoutMs)               |
|   virtual bool setInsecure(bool allowInsecure)              |
+-------------------------------------------------------------+
                              |
             +----------------+----------------+
             |                                 |
             v                                 v
+-----------------------------+ +-----------------------------+
|   HAL P4 (ESP32-P4 / IDF)   | |  HAL S3 (ESP32-S3 / PIO)    |
|                             | |                             |
|  - P4TlsSocketStream        | |  - S3TlsSocketStream        |
|    (esp_tls oficial)        | |    (WiFiClientSecure)       |
|  - Mozilla CA Bundle        | |  - Insecure flag support    |
|  - Acelerador Cripto HW P4  | |  - Buffer de streaming PSRAM|
+-----------------------------+ +-----------------------------+
```

---

## 3. Plan de Cambios Paso a Paso

### Paso 1: Extensión del Contrato de Sockets en Core (`core/include/cbdos/socket.hpp`)
* Agregar el valor `Tls` al enumerador:
  ```cpp
  enum class SocketType {
      Tcp,
      Udp,
      Tls   // Socket con cifrado TLS 1.2 / 1.3
  };
  ```
* Agregar método virtual en `ISocketStream` para controlar la política de certificados:
  ```cpp
  virtual void setInsecure(bool allowInsecure) { (void)allowInsecure; }
  ```

### Paso 2: Implementación de Socket TLS en ESP32-P4 (`bsp/esp32_p4_jc4880/hal/`)
1. **Configuración de Kernel (`bsp/esp32_p4_jc4880/sdkconfig`):**
   * Habilitar `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY=y` para permitir certificados autofirmados cuando se requiera.
2. **Implementación de `P4TlsSocketStream` en `hal_socket_p4.cpp`:**
   * Utilizar la API oficial de ESP-IDF `esp_tls_t`:
     * `esp_tls_init()`
     * `esp_tls_cfg_t` con `crt_bundle_attach = esp_crt_bundle_attach` (para radios HTTPS de Internet).
     * `esp_tls_conn_new_sync(host, port, &cfg, tls)`
     * Métodos `send()` -> `esp_tls_conn_write()`
     * Métodos `recv()` -> `esp_tls_conn_read()`
     * `close()` -> `esp_tls_conn_destroy()`
3. **Actualización de `P4SocketFactory`:**
   * Si `type == SocketType::Tls`, retornar instancia de `P4TlsSocketStream`.
4. **Actualización de `hal_http_p4.cpp` (Cliente REST):**
   * Enlazar `esp_crt_bundle_attach` cuando `!request.insecure`.

### Paso 3: Implementación de Socket TLS en ESP32-S3 (`bsp/esp32_s3_jc3248/hal/`)
1. **Implementación de `S3TlsSocketStream` en `hal_socket_s3.cpp`:**
   * Usar `WiFiClientSecure` de Arduino/ESP32.
   * Si `m_insecure == true`, invocar `m_client.setInsecure()`.
   * Para radios públicas de Internet, `WiFiClientSecure` valida automáticamente los certificados estándar.
2. **Actualización de `S3SocketFactory`:**
   * Instanciar `S3TlsSocketStream` cuando se solicite `SocketType::Tls`.

### Paso 4: Adaptación del Reproductor de Streaming (`core/src/audio/AudioPlayer.cpp`)
1. En `AudioPlayer::runStreamPlayback()`:
   * Determinar el tipo de socket según la URL:
     ```cpp
     bool isHttps = (currentUrl.rfind("https://", 0) == 0);
     cbdos::network::SocketType sockType = isHttps ? cbdos::network::SocketType::Tls 
                                                   : cbdos::network::SocketType::Tcp;
     client = cbdos::network::createSocket(sockType);
     ```
2. **Manejo Inteligente de Redirecciones HTTP -> HTTPS:**
   * Si una estación en `http://` devuelve un código `301`, `302` o `307` con cabecera `Location: https://...`:
     * El bucle cierra el socket TCP actual.
     * Actualiza `currentUrl` a la nueva ubicación HTTPS.
     * La siguiente iteración instancia automáticamente un socket `SocketType::Tls`.
3. Para URLs HTTPS que utilicen certificados autofirmados o no comerciales, marcar `client->setInsecure(true)`.

---

## 4. Matriz de Pruebas y Validación en Hardware Real

| # | Prueba | Entorno / Plataforma | Resultado Esperado |
| :-: | :--- | :---: | :--- |
| **T1** | Compilación limpia de ambas plataformas | ESP32-P4 (ESP-IDF) + ESP32-S3 (PlatformIO) | Cero errores y cero advertencias de compilación. |
| **T2** | Radio Web HTTP Clásica (Puerto 80/8000) | JC4880 (P4) / JC3248 (S3) | Reproducción continua por socket TCP sin regresiones. |
| **T3** | Radio Web HTTPS Nativa (Puerto 443) | JC4880 (P4) / JC3248 (S3) | Handshake TLS exitoso contra estación HTTPS pública (ej. SomaFM, RNE, etc.), decodificación Helix MP3 fluida. |
| **T4** | Redirección 301/302 de HTTP a HTTPS | JC4880 (P4) / JC3248 (S3) | Conmutación dinámica de TCP a TLS sin detener el hilo de audio. |
| **T5** | Petición REST HTTPS (P4) | Shell / Lua / `hal_http_p4` | Petición `GET https://api.github.com` responde con código 200 validado por CA Bundle. |

---

## 5. Beneficios Inmediatos de la Fase 1

1. **Restauración Inmediata de la Radio Online:** La gran mayoría de estaciones de radio por Internet migraron a streams HTTPS obligatorios; esta fase recupera el 100% del catálogo de emisoras.
2. **Cero Dependencias Bloqueantes:** Se ejecuta exclusivamente en la capa de red y HAL, sin alterar la UI ni el decoder Helix.
3. **Cimientos Listos para el Cliente KVM:** La tarea del futuro cliente KVM podrá utilizar directamente estos mismos sockets TLS acelerados por hardware en el P4 para recibir el video y comandos sin tocar la pila de red.

