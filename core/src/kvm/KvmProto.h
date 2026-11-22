#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace cbdos {
namespace kvm {

// Opcodes del protocolo de cable (Wire Protocol) para WebSockets /ws y /video
enum KvmOpcode : uint8_t {
    // Cliente -> Servidor (C2D)
    WS_C2D_MOUSE_ABS    = 0x01, // Ratón absoluto (pantalla táctil)
    WS_C2D_MOUSE_REL    = 0x02, // Ratón relativo (trackpad/rueda)
    WS_C2D_KEYBOARD     = 0x03, // Teclado USB HID Boot (modifier + 6 keys)
    WS_C2D_CONSUMER     = 0x04, // Teclas multimedia consumer
    WS_C2D_RELEASE_ALL  = 0x05, // Liberar todas las teclas/botones
    WS_C2D_PING         = 0x06, // Ping de latencia y solicitud de estado
    WS_C2D_TAKEOVER     = 0x07, // Tomar control exclusivo del KVM
    WS_C2D_PAD          = 0x08, // Gamepad
    WS_C2D_VIDEO_ACK    = 0x09, // Confirmación de frame recibido (en /video)

    // Servidor -> Cliente (D2C)
    WS_D2C_STATUS       = 0x81, // Estado general y LEDs de teclado
    WS_D2C_PONG         = 0x82, // Respuesta de latencia
    WS_D2C_CONTROL      = 0x83, // Estado de control (libre, tomado, ajeno)
    WS_D2C_UPDATE       = 0x84, // Estado de actualización
    WS_D2C_VIDEO_ACKS   = 0x85  // Notificación: el servidor soporta control de flujo por ACKs
};

// Estados de control de la sesión
enum KvmControlState : uint8_t {
    CTRL_HELD_OTHER = 0, // Control en manos de otro cliente
    CTRL_YOU        = 1, // Este terminal CBDos tiene el control
    CTRL_FREE       = 2  // Control disponible
};

// Constantes de protocolo
static constexpr uint16_t KVM_ABS_MAX      = 32767;
static constexpr size_t   VIDEO_HDR_LEN    = 12;
static constexpr uint8_t  VIDEO_MAGIC      = 0x4B; // 'K'
static constexpr uint8_t  VIDEO_TYPE_JPEG  = 1;
static constexpr uint8_t  VIDEO_TYPE_H264  = 2;
static constexpr uint8_t  WS_SUBSCRIBE_PIC = 1;
static constexpr uint8_t  WS_SUBSCRIBE_TXT = 2;
static constexpr int      VIDEO_ACK_WINDOW = 2;

// Estructura de cabecera binaria de frame de video (12 bytes)
#pragma pack(push, 1)
struct KvmVideoHdr {
    uint8_t  magic;   // 0x4B ('K')
    uint8_t  type;    // 1: JPEG, 2: H.264
    uint16_t width;   // Ancho en píxeles (LE)
    uint16_t height;  // Alto en píxeles (LE)
    uint32_t seq;     // Número secuencial de cuadro (LE)
    uint16_t pts_ms;  // Timestamp en milisegundos del servidor (LE)

    static bool read(const uint8_t* in, size_t len, KvmVideoHdr& out) {
        if (!in || len < VIDEO_HDR_LEN) return false;
        if (in[0] != VIDEO_MAGIC) return false;

        out.magic  = in[0];
        out.type   = in[1];
        out.width  = static_cast<uint16_t>(in[2] | (in[3] << 8));
        out.height = static_cast<uint16_t>(in[4] | (in[5] << 8));
        out.seq    = static_cast<uint32_t>(in[6] | (in[7] << 8) | (in[8] << 16) | (in[9] << 24));
        out.pts_ms = static_cast<uint16_t>(in[10] | (in[11] << 8));
        return true;
    }

    void write(uint8_t out[VIDEO_HDR_LEN]) const {
        out[0]  = VIDEO_MAGIC;
        out[1]  = type;
        out[2]  = static_cast<uint8_t>(width & 0xFF);
        out[3]  = static_cast<uint8_t>(width >> 8);
        out[4]  = static_cast<uint8_t>(height & 0xFF);
        out[5]  = static_cast<uint8_t>(height >> 8);
        out[6]  = static_cast<uint8_t>(seq & 0xFF);
        out[7]  = static_cast<uint8_t>((seq >> 8) & 0xFF);
        out[8]  = static_cast<uint8_t>((seq >> 16) & 0xFF);
        out[9]  = static_cast<uint8_t>((seq >> 24) & 0xFF);
        out[10] = static_cast<uint8_t>(pts_ms & 0xFF);
        out[11] = static_cast<uint8_t>(pts_ms >> 8);
    }
};
#pragma pack(pop)

// Constructores de mensajes C2D
inline size_t buildMouseAbs(uint8_t out[8], uint8_t buttons, uint16_t x, uint16_t y, int8_t wheel = 0, int8_t pan = 0) {
    out[0] = WS_C2D_MOUSE_ABS;
    out[1] = buttons;
    out[2] = static_cast<uint8_t>(x & 0xFF);
    out[3] = static_cast<uint8_t>(x >> 8);
    out[4] = static_cast<uint8_t>(y & 0xFF);
    out[5] = static_cast<uint8_t>(y >> 8);
    out[6] = static_cast<uint8_t>(wheel);
    out[7] = static_cast<uint8_t>(pan);
    return 8;
}

inline size_t buildMouseRel(uint8_t out[8], uint8_t buttons, int16_t dx, int16_t dy, int8_t wheel = 0, int8_t pan = 0) {
    out[0] = WS_C2D_MOUSE_REL;
    out[1] = buttons;
    out[2] = static_cast<uint8_t>(dx & 0xFF);
    out[3] = static_cast<uint8_t>((dx >> 8) & 0xFF);
    out[4] = static_cast<uint8_t>(dy & 0xFF);
    out[5] = static_cast<uint8_t>((dy >> 8) & 0xFF);
    out[6] = static_cast<uint8_t>(wheel);
    out[7] = static_cast<uint8_t>(pan);
    return 8;
}

inline size_t buildKeyboard(uint8_t out[8], uint8_t modifier, const uint8_t keys[6]) {
    out[0] = WS_C2D_KEYBOARD;
    out[1] = modifier;
    if (keys) {
        std::memcpy(&out[2], keys, 6);
    } else {
        std::memset(&out[2], 0, 6);
    }
    return 8;
}

inline size_t buildVideoAck(uint8_t out[5], uint32_t seq) {
    out[0] = WS_C2D_VIDEO_ACK;
    out[1] = static_cast<uint8_t>(seq & 0xFF);
    out[2] = static_cast<uint8_t>((seq >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>((seq >> 16) & 0xFF);
    out[4] = static_cast<uint8_t>((seq >> 24) & 0xFF);
    return 5;
}

} // namespace kvm
} // namespace cbdos

