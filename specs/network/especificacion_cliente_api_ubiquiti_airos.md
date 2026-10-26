# Especificación Técnica y Plan: Cliente de API Ubiquiti AirOS (AirMAX) en CBDos

**Documento:** `specs/network/especificacion_cliente_api_ubiquiti_airos.md`  
**Módulo:** `cbdos::network::UbiquitiAirOsClient`  
**Estado:** 📋 Especificado / Listo para Revisión  
**Fecha:** 24 de Octubre de 2026  
**Autor:** kaber420 & Asistente Senior de Sistemas Embebidos  
**Archivos de Referencia Locales:** [`specs/network/references/omniwisp_ubiquiti/`](references/omniwisp_ubiquiti/)

---

## 1. Objetivo y Alcance

Permitir que **CBDos** (tanto en ESP32-P4 como en ESP32-S3) pueda consultar y monitorear de manera autónoma dispositivos inalámbricos **Ubiquiti AirMAX** (radios y Access Points airOS v7 / v8 / AC y M) directamente a través de su interfaz HTTP/HTTPS nativa.

El objetivo inmediato es:
1. Conectar y autenticar mediante HTTPS gestionando cookies y token de seguridad CSRF.
2. Consultar el estado operativo del AP (SSID, canal, ancho de banda, ruido, carga de aire, throughput).
3. Obtener el listado en vivo de estaciones CPE conectadas con sus métricas completas (Señal dBm, cadenas RSSI, ruido, throughput Tx/Rx, capacidad AirMAX, uptime).
4. Obtener el estado del cable de red Ethernet de los CPE y distancia del enlace.

---

## 2. Protocolo de Comunicación y Flujo HTTPS

Los dispositivos Ubiquiti AirOS utilizan una API web basada en cookies y validación CSRF. No requieren de software intermediario (como UNMS/UISP) para consultar datos en vivo.

```
+------------------+                                  +---------------------+
|   CBDos Client   |                                  |   Ubiquiti AirOS    |
|   (ESP32 P4/S3)  |                                  |   (AP / CPE)        |
+------------------+                                  +---------------------+
         |                                                       |
         |  1. POST /api/auth (username, password)               |
         |------------------------------------------------------>|
         |                                                       |
         |  2. 200 OK + Header "X-CSRF-ID: <token>"              |
         |            + Cookie de sesión (AIROS_SESSIONID)       |
         |<------------------------------------------------------|
         |                                                       |
         |  3. GET /status.cgi                                   |
         |     Header: "X-CSRF-ID: <token>"                      |
         |     Header: "Cookie: AIROS_SESSIONID=..."             |
         |------------------------------------------------------>|
         |                                                       |
         |  4. 200 OK (JSON con AP, radio, clientes, interfaces)|
         |<------------------------------------------------------|
         |                                                       |
         |  [Al finalizar / Desconectar]                         |
         |  5. POST /api/auth/logout                             |
         |------------------------------------------------------>|
         |  6. 200 OK                                            |
         |<------------------------------------------------------|
```

### 2.1. Autenticación (`POST /api/auth`)

- **URL**: `https://<HOST>:<PORT>/api/auth` (puerto por defecto: `443`, opcional `80`)
- **Método**: `POST`
- **Cabeceras**:
  - `User-Agent: Mozilla/5.0 UManager/1.0`
  - `Connection: keep-alive`
  - `Content-Type: application/x-www-form-urlencoded`
- **Cuerpo (Payload)**:
  ```
  username=<USUARIO>&password=<PASSWORD>
  ```
- **Validación de Éxito**:
  - Código HTTP `200 OK`.
  - Extraer de las cabeceras de respuesta:
    1. Header `X-CSRF-ID`: Almacenar el token de seguridad.
    2. Header `Set-Cookie`: Almacenar la cookie de sesión (ej. `AIROS_SESSIONID=...`).

### 2.2. Consulta de Estado en Vivo (`GET /status.cgi`)

- **URL**: `https://<HOST>:<PORT>/status.cgi`
- **Método**: `GET`
- **Cabeceras Requeridas**:
  - `X-CSRF-ID: <token>`
  - `Cookie: <cookie_guardada>`
  - `User-Agent: Mozilla/5.0 UManager/1.0`
- **Manejo de Errores de Sesión**:
  - Si retorna `401 Unauthorized` o `403 Forbidden`, la sesión expiró: el cliente debe reautenticar automáticamente y reintentar la petición una vez.

### 2.3. Cierre de Sesión (`POST /api/auth/logout`)

- **URL**: `https://<HOST>:<PORT>/api/auth/logout`
- **Método**: `POST`
- Permite liberar recursos y sesiones en el servidor web lighttpd del radio.

---

## 3. Estructura del Payload JSON (`status.cgi`)

El endpoint `/status.cgi` entrega un objeto JSON unificado. A continuación se documentan los nodos exactos que consume el sistema:

### 3.1. Identidad y Sistema (`host`)

```json
"host": {
  "hostname": "AP-Sector-Norte-01",
  "devmodel": "Rocket Prism 5AC Gen2",
  "fwversion": "WA.v8.7.1",
  "uptime": 1284500,
  "netrole": "bridge",
  "cpuload": 12,
  "freeram": 45120
}
```

### 3.2. Parámetros de Radio Inalámbrica (`wireless`)

```json
"wireless": {
  "essid": "WISP_FIBRA_01",
  "mode": "ap-ptmp-airmax-ac",
  "frequency": 5745,
  "chanbw": 20,
  "noisef": -96,
  "count": 14,
  "throughput": {
    "tx": 18500000,
    "rx": 4200000
  },
  "polling": {
    "use": 450,
    "tx_use": 280,
    "rx_use": 170
  },
  "sta": [ ... ]
}
```
*Nota: `polling.use`, `polling.tx_use` y `polling.rx_use` se expresan en décimas de porcentaje (ej. 450 = 45.0% de airtime).*

### 3.3. Estaciones / Clientes Conectados (`wireless.sta[]`)

Cada elemento del array representa un CPE asociado al sector:

```json
{
  "mac": "78:8A:20:11:22:33",
  "lastip": "192.168.10.45",
  "signal": -62,
  "chainrssi": [-63, -61],
  "noisefloor": -96,
  "distance": 1250,
  "remote": {
    "hostname": "CPE-Familia-Perez",
    "devmodel": "LiteBeam 5AC Gen2",
    "version": "WA.v8.7.1",
    "uptime": 45200,
    "tx_throughput": 2400,
    "rx_throughput": 850,
    "ethlist": [
      {
        "ifname": "eth0",
        "plugged": 1,
        "speed": 100,
        "duplex": 1
      }
    ]
  },
  "airmax": {
    "beam": 0,
    "signal": -62,
    "dl_capacity": 65000,
    "ul_capacity": 48000
  },
  "stats": {
    "rx_bytes": 104857600,
    "tx_bytes": 524288000,
    "rx_packets": 245000,
    "tx_packets": 680000
  }
}
```

### 3.4. Estado del Cable y Ethernet Remoto

En cada estación dentro de `sta[].remote.ethlist[0]`:
- `plugged`: `1` indica cable conectado a switch/router del cliente; `0` indica cable desconectado o puerto apagado.
- `speed`: `10`, `100` o `1000` (Mbps).
- `duplex`: `1` (Full Duplex) o `0` (Half Duplex).

---

## 4. Diseño de Estructuras de Datos en C++ (`core/`)

Para desacoplar el consumo de la red de la interfaz gráfica y de la lógica del sistema:

```cpp
namespace cbdos::network::ubiquiti {

struct UbiquitiCpeClient {
    std::string mac;
    std::string hostname;
    std::string ip_address;
    int16_t signal_dbm;              // Ej: -64
    int16_t chain0_dbm;              // Ej: -65
    int16_t chain1_dbm;              // Ej: -63
    int16_t noisefloor_dbm;          // Ej: -98
    uint32_t distance_meters;        // Distancia del enlace radio (en metros)
    
    // Throughput & Capacidad
    uint32_t tx_throughput_kbps;
    uint32_t rx_throughput_kbps;
    uint32_t dl_capacity_kbps;       // Capacidad AirMAX Download
    uint32_t ul_capacity_kbps;       // Capacidad AirMAX Upload
    
    // Tráfico acumulado
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint32_t uptime_seconds;
    
    // Diagnóstico Ethernet del CPE (Cable de red local)
    bool eth_plugged;                // true si hay cable conectado
    uint16_t eth_speed_mbps;         // 10, 100, 1000
    bool eth_full_duplex;
};

struct UbiquitiApStatus {
    std::string host_ip;
    std::string hostname;
    std::string dev_model;
    std::string fw_version;
    uint32_t uptime_seconds;
    bool is_online;
    std::string last_error;
    
    // Métricas de Radio
    std::string essid;
    uint32_t frequency_mhz;          // Ej: 5745
    uint16_t channel_width_mhz;      // Ej: 20, 40, 80
    int16_t noise_floor_dbm;
    uint16_t client_count;
    
    // Airtime (%) en décimas (ej. 450 = 45.0%)
    uint16_t airtime_total_pct10;
    uint16_t airtime_tx_pct10;
    uint16_t airtime_rx_pct10;
    
    // GPS
    uint8_t gps_satellites;
    double gps_latitude;
    double gps_longitude;
    
    // Clientes asociados
    std::vector<UbiquitiCpeClient> clients;
};

} // namespace cbdos::network::ubiquiti
```

---

## 5. Estrategia de Implementación en ESP32 (P4 y S3)

1. **Gestión de Memoria en Streaming**:
   - El payload de `/status.cgi` puede medir hasta 50 KB con múltiples clientes.
   - Usar lectura por chunks desde el socket HTTPS para evitar agotar la memoria interna.
   - Deserializar con `ArduinoJson` o `cJSON` en PSRAM.
2. **Autenticación Resiliente**:
   - Mantener el par `(cookie, csrf_token)` en memoria.
   - Ante cualquier respuesta `401/403`, reintentar una sola vez el ciclo `POST /api/auth` antes de marcar el dispositivo como fuera de línea.
3. **Compatibilidad Multi-Target**:
   - Implementado en `core/` sobre el contrato canónico `IHttpClient` de CBDos.

---

## 6. Fases de Ejecución

- [x] **Fase 0**: Descarga y preservación de archivos fuente de referencia locales en `specs/network/references/omniwisp_ubiquiti/`.
- [x] **Fase 1**: Especificación del protocolo, endpoints, payloads JSON y estructuras C++.
- [ ] **Fase 2**: Implementación de la clase `UbiquitiAirOsClient` en `core/services/` o `core/network/`.
- [ ] **Fase 3**: Pruebas de conexión HTTPS contra un equipo real Ubiquiti AirMAX en red local.
- [ ] **Fase 4**: Integración con vista LVGL (o comando en Shell `cbdos::shell`) para visualizar APs y clientes en vivo.
