# Plan Técnico: Reescritura del MeshCoreClient para Protocolo Oficial

## Problema Actual

El `MeshCoreClient` de CBDos implementa un protocolo **inventado** (`0x55 0xAA` + comandos `0x01`, `0x03`, etc.) que es incompatible con el firmware MeshCore real (Liam Cottle) que corre en los ESP32-S3/C3.

**Consecuencias:**
- No se pueden cambiar canales, alias, ni configuración del dongle
- No se reciben contactos reales
- No hay soporte de canales (públicos, hashtag, privados)
- Solo funciona "broadcast" porque no hay implementación de canales

---

## Protocolo Oficial de MeshCore

### Framing USB

```
App → Radio: '<' (0x3C) + longitud u16 LE (2 bytes) + frame
Radio → App: '>' (0x3E) + longitud u16 LE (2 bytes) + frame
```

**NOTA:** La longitud son 2 bytes little-endian (meshcore.js / meshcore-cli
usan `0x3C + len_lo + len_hi`; en BLE no hay framing y en TCP el proxy usa
`0x3C` en ambas direcciones).

### Comandos Binarios (App → Radio)

| Comando | Código | Descripción |
|---------|--------|-------------|
| `CMD_APP_START` | `0x01` | Inicializar comunicación (debe ser primero) |
| `CMD_DEVICE_QUERY` | `0x16` | Consultar info del dispositivo |
| `CMD_SEND_CHANNEL_MESSAGE` | `0x03` | Enviar mensaje a canal |
| `CMD_GET_MESSAGE` | `0x0A` | Obtener siguiente mensaje en cola |
| `CMD_GET_CHANNEL` | `0x1F` | Obtener info de canal |
| `CMD_SET_CHANNEL` | `0x20` | Crear/modificar canal |
| `CMD_GET_BATTERY` | `0x14` | Consultar batería |

### Formato de Comandos

#### CMD_APP_START (0x01)
```
Byte 0: 0x01
Bytes 1-7: Reservado (0x00)
Bytes 8+: Nombre de app (UTF-8, opcional)
```

#### CMD_DEVICE_QUERY (0x16)
```
Byte 0: 0x16
Byte 1: 0x03
```

#### CMD_SEND_CHANNEL_MESSAGE (0x03)
```
Byte 0: 0x03
Byte 1: Text Type (0 = plano; ver tipos de texto)
Byte 2: Channel Index (0-7)
Bytes 3-6: Timestamp (32-bit LE Unix seconds)
Bytes 7+: Mensaje texto (UTF-8)
```

#### CMD_GET_MESSAGE (0x0A)
```
Byte 0: 0x0A
```

#### CMD_GET_CHANNEL (0x1F)
```
Byte 0: 0x1F
Byte 1: Channel Index (0-7)
```

#### CMD_SET_CHANNEL (0x20)
```
Byte 0: 0x20
Byte 1: Channel Index (0-7)
Bytes 2-33: Nombre (32 bytes, UTF-8, null-padded)
Bytes 34-49: Secret (16 bytes)
```

#### CMD_GET_BATTERY (0x14)
```
Byte 0: 0x14
```

### Respuestas (Radio → App)

| Respuesta | Código | Descripción |
|-----------|--------|-------------|
| `PACKET_OK` | `0x00` | Comando exitoso |
| `PACKET_ERROR` | `0x01` | Error (con código) |
| `PACKET_SELF_INFO` | `0x05` | Info del nodo (nombre, clave, radio) |
| `PACKET_MSG_SENT` | `0x06` | Confirmación de envío |
| `PACKET_CONTACT_MSG_RECV` | `0x07` | Mensaje de contacto recibido |
| `PACKET_CHANNEL_MSG_RECV` | `0x08` | Mensaje de canal recibido |
| `PACKET_NO_MORE_MSGS` | `0x0A` | No hay más mensajes |
| `PACKET_BATTERY` | `0x0C` | Nivel de batería |
| `PACKET_DEVICE_INFO` | `0x0D` | Info del firmware |
| `PACKET_CHANNEL_INFO` | `0x12` | Info de canal |
| `PACKET_MESSAGES_WAITING` | `0x83` | Notificación de mensajes en cola |

### Formato de Respuestas

#### PACKET_SELF_INFO (0x05)
```
Byte 0: 0x05
Byte 1: Advertisement Type
Byte 2: TX Power
Byte 3: Max TX Power
Bytes 4-35: Public Key (32 bytes)
Bytes 36-39: Latitude (int32 LE / 1e6)
Bytes 40-43: Longitude (int32 LE / 1e6)
Byte 44: Multi ACKs
Byte 45: Adv Location Policy
Byte 46: Telemetry Mode
Byte 47: Manual Add Contacts
Bytes 48-51: Frequency (uint32 LE / 1000)
Bytes 52-55: Bandwidth (uint32 LE / 1000)
Byte 56: Spreading Factor
Byte 57: Coding Rate
Bytes 58+: Device Name (UTF-8)
```

#### PACKET_DEVICE_INFO (0x0D)
```
Byte 0: 0x0D
Byte 1: Firmware Version (uint8)
Para fw_ver >= 3:
  Byte 2: Max Contacts Raw (real = value * 2)
  Byte 3: Max Channels
  Bytes 4-7: BLE PIN (uint32 LE)
  Bytes 8-19: Firmware Build (12 bytes)
  Bytes 20-59: Model (40 bytes)
  Bytes 60-79: Version (20 bytes)
  Byte 80: Client repeat enabled
  Byte 81: Path hash mode
```

#### PACKET_CHANNEL_MSG_RECV (0x08)
```
Byte 0: 0x08
Byte 1: Channel Index (0-7)
Byte 2: Path Length
Byte 3: Text Type
Bytes 4-7: Timestamp (uint32 LE)
Bytes 8+: Mensaje texto (UTF-8)
```

#### PACKET_CHANNEL_MSG_RECV_V3 (0x11)
```
Byte 0: 0x11
Byte 1: SNR (int8 * 4)
Bytes 2-3: Reservado
Byte 4: Channel Index
Byte 5: Path Length
Byte 6: Text Type
Bytes 7-10: Timestamp (uint32 LE)
Bytes 11+: Mensaje texto
```

#### PACKET_CONTACT_MSG_RECV (0x07)
```
Byte 0: 0x07
Bytes 1-6: Public Key Prefix (6 bytes hex)
Byte 7: Path Length
Byte 8: Text Type
Bytes 9-12: Timestamp (uint32 LE)
Bytes 13-16: Signature (4 bytes, solo si txt_type == 2)
Bytes 17+: Mensaje texto
```

#### PACKET_CHANNEL_INFO (0x12)
```
Byte 0: 0x12
Byte 1: Channel Index
Bytes 2-33: Channel Name (32 bytes, null-terminated)
Bytes 34-49: Secret (16 bytes)
```

#### PACKET_BATTERY (0x0C)
```
Byte 0: 0x0C
Bytes 1-2: Battery Voltage (uint16 LE, mV)
Bytes 3-6: Used Storage (uint32 LE, KB)
Bytes 7-10: Total Storage (uint32 LE, KB)
```

#### PACKET_MSG_SENT (0x06)
```
Byte 0: 0x06
Byte 1: Route Flag (0=direct, 1=flood)
Bytes 2-5: Tag (uint32 LE)
Bytes 6-9: Suggested Timeout (uint32 LE, ms)
```

### Tipos de Texto (txt_type)

| Valor | Constante | Descripción |
|-------|-----------|-------------|
| 0 | `TXT_TYPE_PLAIN` | Texto plano |
| 1 | `TXT_TYPE_CLI_DATA` | Comando CLI |
| 2 | `TXT_TYPE_SIGNED_PLAIN` | Texto plano firmado |

### Códigos de Error

| Código | Constante | Descripción |
|--------|-----------|-------------|
| 1 | `ERR_CODE_UNSUPPORTED_CMD` | Comando no soportado |
| 2 | `ERR_CODE_NOT_FOUND` | No encontrado |
| 3 | `ERR_CODE_TABLE_FULL` | Tabla llena |
| 4 | `ERR_CODE_BAD_STATE` | Estado inválido |
| 5 | `ERR_CODE_FILE_IO_ERROR` | Error de E/S |
| 6 | `ERR_CODE_ILLEGAL_ARG` | Argumento inválido |

---

## Comandos CLI de Texto (por serial)

El firmware MeshCore acepta comandos de texto por serial para configuración.

### Cambiar Nombre/Alias
```bash
get name                    # Ver nombre actual
set name MiNuevoNombre      # Cambiar nombre (máx 31 bytes)
```

### Configurar Radio
```bash
get radio                   # Ver params (freq,bw,sf,cr)
set radio 915,250,9,7       # Establecer todos los params
set freq 915                # Cambiar solo frecuencia (MHz)
set tx 22                   # Cambiar potencia (dBm, 1-22)
```

### Canales
```bash
get channel <idx>           # Ver info de canal (0-7)
set channel <idx> <name> <secret_hex>  # Crear canal
```

### Sistema
```bash
ver                         # Versión firmware
board                       # Nombre del hardware
reboot                      # Reiniciar
get public.key              # Ver clave pública
get lat / set lat <deg>     # Latitud
get lon / set lon <deg>     # Longitud
clock                       # Hora actual UTC
time <epoch>                # Establecer hora
```

### Estadísticas
```bash
stats-core                  # Battery, uptime, queue
stats-radio                 # Noise floor, RSSI/SNR
stats-packets               # Contadores rx/tx
```

### Regiones (v1.10+)
```bash
region list                 # Ver regiones
region put <name>           # Crear región
region allowf <name>        # Permitir flooding
region denyf <name>         # Bloquear flooding
region save                 # Guardar cambios
```

---

## Configuración de Dongles sin Pantalla

Para dongles ESP32-S3/C3 sin pantalla ni botones, la configuración se realiza **por la misma vía serial** usando comandos CLI de texto.

### Mecanismo Propuesto

1. **Al conectar:** El P4 detecta el dongle y envía `CMD_APP_START`
2. **Configuración inicial:** Usa comandos CLI de texto:
   ```bash
   set name CBDos-Node       # Alias del nodo
   set radio 915,250,9,7     # Radio config
   set tx 22                 # Potencia
   ```
3. **Cambio dinámico:** La UI del P4 envía comandos CLI al dongle
4. **Persistencia:** El firmware MeshCore guarda en NVS automáticamente

### Flujo de Cambio de Alias

```
P4 UI → "Cambiar Nombre" → enviar "set name NuevoNombre\n" por serial
                          → dongle responde OK
                          → P4 envía CMD_APP_START para refrescar PACKET_SELF_INFO
                          → UI actualiza nombre mostrado
```

### Flujo de Cambio de Canal

```
P4 UI → "Canal 2" → enviar "set channel 2 MiCanal <secret_hex>\n"
                   → dongle responde OK
                   → P4 puede enviar mensajes al canal 2
```

---

## Arquitectura del Nuevo MeshCoreClient

### Enums y Constantes

```cpp
namespace cbdos::meshcore {

// Framing del protocolo oficial
static constexpr uint8_t FRAME_CHAR_APP_TO_RADIO = 0x3C;  // '<'
static constexpr uint8_t FRAME_CHAR_RADIO_TO_APP = 0x3E;  // '>'

// Comandos (App → Radio)
enum class Cmd : uint8_t {
    APP_START               = 0x01,
    SEND_CHANNEL_MESSAGE    = 0x03,
    GET_MESSAGE             = 0x0A,
    GET_BATTERY             = 0x14,
    DEVICE_QUERY            = 0x16,
    GET_CHANNEL             = 0x1F,
    SET_CHANNEL             = 0x20,
};

// Respuestas (Radio → App)
enum class PacketType : uint8_t {
    OK                      = 0x00,
    ERROR                   = 0x01,
    CONTACT_START           = 0x02,
    CONTACT                 = 0x03,
    CONTACT_END             = 0x04,
    SELF_INFO               = 0x05,
    MSG_SENT                = 0x06,
    CONTACT_MSG_RECV        = 0x07,
    CHANNEL_MSG_RECV        = 0x08,
    CURRENT_TIME            = 0x09,
    NO_MORE_MSGS            = 0x0A,
    BATTERY                 = 0x0C,
    DEVICE_INFO             = 0x0D,
    CONTACT_MSG_RECV_V3     = 0x10,
    CHANNEL_MSG_RECV_V3     = 0x11,
    CHANNEL_INFO            = 0x12,
    CHANNEL_DATA_RECV       = 0x1B,
    ADVERTISEMENT           = 0x80,
    ACK                     = 0x82,
    MESSAGES_WAITING        = 0x83,
    LOG_DATA                = 0x88,
};

// Códigos de error
enum class ErrCode : uint8_t {
    UNSUPPORTED_CMD         = 1,
    NOT_FOUND               = 2,
    TABLE_FULL              = 3,
    BAD_STATE               = 4,
    FILE_IO_ERROR           = 5,
    ILLEGAL_ARG             = 6,
};

// Tipo de texto
enum class TxtType : uint8_t {
    PLAIN                   = 0,
    CLI_DATA                = 1,
    SIGNED_PLAIN            = 2,
};

} // namespace cbdos::meshcore
```

### Estructuras de Datos

```cpp
namespace cbdos::meshcore {

struct MeshChannel {
    uint8_t index = 0;
    std::string name;
    uint8_t secret[16] = {0};
    bool isPublic = false;
    bool isHashtag = false;
};

struct SelfInfo {
    uint8_t advType = 0;
    int8_t txPower = 0;
    int8_t maxTxPower = 0;
    uint8_t publicKey[32] = {0};
    float latitude = 0.0f;
    float longitude = 0.0f;
    bool multiAcks = false;
    uint32_t frequencyHz = 0;
    uint32_t bandwidthHz = 0;
    uint8_t spreadingFactor = 0;
    uint8_t codingRate = 0;
    std::string name;
};

struct DeviceInfo {
    uint8_t firmwareVersion = 0;
    uint8_t maxContacts = 0;
    uint8_t maxChannels = 0;
    std::string firmwareBuild;
    std::string model;
    std::string version;
};

struct ChannelMessage {
    uint8_t channelIndex = 0;
    uint8_t pathLength = 0;
    TxtType txtType = TxtType::PLAIN;
    uint32_t timestamp = 0;
    std::string text;
    float snr = 0.0f;  // Solo V3
};

struct ContactMessage {
    std::string pubkeyPrefix;
    uint8_t pathLength = 0;
    TxtType txtType = TxtType::PLAIN;
    uint32_t timestamp = 0;
    std::string text;
    float snr = 0.0f;  // Solo V3
};

} // namespace cbdos::meshcore
```

### Nuevo Parser

```cpp
enum class ParseState {
    WaitFrameStart,    // Esperar '>' (0x3E)
    WaitLenLow,        // Byte bajo de longitud u16 LE
    WaitLenHigh,       // Byte alto de longitud u16 LE
    WaitPayload        // Leer payload completo
};

void MeshCoreClient::processByte(uint8_t byte) {
    switch (m_parseState) {
        case ParseState::WaitFrameStart:
            if (byte == FRAME_CHAR_RADIO_TO_APP) {
                m_parseState = ParseState::WaitLenLow;
            }
            // Ignorar otros bytes (puede haber basura CLI)
            break;

        case ParseState::WaitLenLow:
            m_rxPayloadLen = byte;
            m_parseState = ParseState::WaitLenHigh;
            break;

        case ParseState::WaitLenHigh:
            m_rxPayloadLen |= static_cast<uint16_t>(byte) << 8;
            m_rxPayload.clear();
            if (m_rxPayloadLen == 0 || m_rxPayloadLen > FRAME_MAX_PAYLOAD) {
                m_parseState = ParseState::WaitFrameStart;
            } else {
                m_parseState = ParseState::WaitPayload;
            }
            break;

        case ParseState::WaitPayload:
            m_rxPayload.push_back(byte);
            if (m_rxPayload.size() >= m_rxPayloadLen) {
                handleFrame(m_rxPayload);
                m_parseState = ParseState::WaitFrameStart;
            }
            break;
    }
}
```

### Métodos Principales

```cpp
class MeshCoreClient {
public:
    // Conexión
    bool connect(const std::string& portId, uint32_t baudrate = 115200);
    void disconnect();

    // Handshake
    bool sendAppStart(const std::string& appName = "CBDos");
    bool queryDeviceInfo();
    bool queryBattery();

    // Mensajes
    bool sendChannelMessage(uint8_t channelIdx, const std::string& text);
    bool getNextMessage();
    bool pollMessages();  // Llama a getNextMessage hasta NO_MORE_MSGS

    // Canales
    bool getChannel(uint8_t idx);
    bool setChannel(uint8_t idx, const std::string& name, const uint8_t secret[16]);

    // Configuración por CLI de texto
    bool sendCliCommand(const std::string& cmd);
    bool setName(const std::string& name);
    bool setRadio(uint32_t freqMhz, uint32_t bwKhz, uint8_t sf, uint8_t cr);
    bool setTxPower(int8_t dBm);
    bool setFrequency(uint32_t freqMhz);
    bool reboot();

    // Getters
    const SelfInfo& getSelfInfo() const;
    const DeviceInfo& getDeviceInfo() const;
    const std::vector<MeshChannel>& getChannels() const;
    const std::vector<ChannelMessage>& getMessages() const;

    // Callbacks
    void setOnMessageCallback(std::function<void(const ChannelMessage&)> cb);
    void setOnSelfInfoCallback(std::function<void(const SelfInfo&)> cb);
    void setOnMessagesWaitingCallback(std::function<void()> cb);

private:
    void handleFrame(const std::vector<uint8_t>& payload);
    void handleSelfInfo(const std::vector<uint8_t>& data);
    void handleDeviceInfo(const std::vector<uint8_t>& data);
    void handleChannelMsg(const std::vector<uint8_t>& data, bool v3);
    void handleContactMsg(const std::vector<uint8_t>& data, bool v3);
    void handleChannelInfo(const std::vector<uint8_t>& data);
    void handleBattery(const std::vector<uint8_t>& data);
    void handleOk(const std::vector<uint8_t>& data);
    void handleError(const std::vector<uint8_t>& data);

    // Envío de frames
    bool sendFrame(Cmd cmd, const uint8_t* payload = nullptr, size_t len = 0);
    bool sendText(const std::string& text);

    // Estado
    bool m_connected = false;
    std::string m_activePort;
    ParseState m_parseState = ParseState::WaitFrameStart;
    std::vector<uint8_t> m_rxPayload;

    // Datos
    SelfInfo m_selfInfo;
    DeviceInfo m_deviceInfo;
    std::vector<MeshChannel> m_channels;
    std::vector<ChannelMessage> m_messages;

    // Callbacks
    std::function<void(const ChannelMessage&)> m_onMessageCb;
    std::function<void(const SelfInfo&)> m_onSelfInfoCb;
    std::function<void()> m_onMessagesWaitingCb;
};
```

---

## Archivos a Modificar

| Archivo | Cambio |
|---------|--------|
| `core/include/cbdos/meshcore/meshcore_types.hpp` | Reescritura completa con nuevos enums y structs |
| `core/include/cbdos/meshcore/meshcore_client.hpp` | Nuevos métodos y estado |
| `core/src/meshcore/meshcore_client.cpp` | Reescritura del parser y comandos |
| `core/src/ui/views/MeshCoreView.cpp` | UI para canales y configuración |

---

## Orden de Implementación

### Fase 1: Parser Binario
- [x] Framing `<`/`>` con longitud u16 LE (2 bytes, no 1)
- [x] Nuevo parser de 4 estados con resync ante longitud anómala
- [x] Manejo de basura CLI entre frames

### Fase 2: Handshake
- [ ] Implementar `CMD_APP_START`
- [ ] Parsear `PACKET_SELF_INFO`
- [ ] Parsear `PACKET_DEVICE_INFO`

### Fase 3: Mensajes
- [ ] Implementar `CMD_SEND_CHANNEL_MESSAGE`
- [ ] Parsear `PACKET_MSG_SENT`
- [ ] Implementar `CMD_GET_MESSAGE`
- [ ] Parsear `PACKET_CHANNEL_MSG_RECV` / `_V3`
- [ ] Parsear `PACKET_CONTACT_MSG_RECV` / `_V3`
- [ ] Manejar `PACKET_MESSAGES_WAITING`

### Fase 4: Canales
- [ ] Implementar `CMD_GET_CHANNEL`
- [ ] Implementar `CMD_SET_CHANNEL`
- [ ] Parsear `PACKET_CHANNEL_INFO`
- [ ] GestiÃ³n de canales pÃºblicos/privados/hashtag

### Fase 5: ConfiguraciÃ³n CLI
- [ ] Enviar comandos de texto por serial
- [ ] MÃ©todos helper: `setName()`, `setRadio()`, etc.
- [ ] Parsear respuestas CLI

### Fase 6: BaterÃ­a y Extras
- [ ] Implementar `CMD_GET_BATTERY`
- [ ] Parsear `PACKET_BATTERY`
- [ ] Manejar `PACKET_OK` / `PACKET_ERROR`

### Fase 7: UI
- [ ] Selector de canal (0-7)
- [ ] BotÃ³n "Crear Canal"
- [ ] BotÃ³n "Configurar" (envÃ­a comandos CLI)
- [ ] Mostrar info de baterÃ­a del dongle

---

## Referencias

- [MeshCore Companion Protocol](https://docs.meshcore.io/companion_protocol/)
- [MeshCore CLI Commands](https://docs.meshcore.io/cli_commands/)
- [meshcore.js (JavaScript reference)](https://github.com/meshcore-dev/meshcore.js)
- [meshcore_c (C99 library)](https://github.com/SH3D/meshcore_c)
- [meshcore-cli (Python CLI)](https://github.com/meshcore-dev/meshcore-cli)
