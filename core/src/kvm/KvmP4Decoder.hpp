#pragma once

#if defined(__has_include)
  #if __has_include("sdkconfig.h")
    #include "sdkconfig.h"
  #endif
#endif

#include <cstdint>
#include <cstddef>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "driver/jpeg_decode.h"
#include "driver/ppa.h"
#endif

namespace cbdos {
namespace kvm {

class KvmP4Decoder {
public:
    KvmP4Decoder();
    ~KvmP4Decoder();

    bool init(uint32_t maxDecW = 1920, uint32_t maxDecH = 1088);
    void deinit();

    /**
     * @brief Decodifica JPEG por hardware y escala/rota mediante PPA SRM al búfer destino RGB565.
     * @param jpegData Puntero a datos comprimidos JPEG.
     * @param jpegLen Longitud del flujo JPEG en bytes.
     * @param outRgb565 Búfer de salida RGB565 (alineado a 128 bytes si está en PSRAM).
     * @param outW Ancho de salida deseado (ej. 800 o 640).
     * @param outH Alto de salida deseado (ej. 448 o 480).
     * @return true si la decodificación y reescalado por silicio fueron exitosos.
     */
    bool decodeAndScale(const uint8_t* jpegData, size_t jpegLen,
                        void* outRgb565, uint32_t outW, uint32_t outH);

    bool isReady() const { return m_initialized; }

private:
    bool m_initialized{false};
    uint32_t m_maxW{1920};
    uint32_t m_maxH{1088};
    uint8_t* m_intermediateBuf{nullptr};
    size_t   m_intermediateCap{0};

#if defined(CONFIG_IDF_TARGET_ESP32P4)
    jpeg_decoder_handle_t m_jpegDec{nullptr};
    ppa_client_handle_t   m_ppaSrm{nullptr};
#endif
};

} // namespace kvm
} // namespace cbdos
