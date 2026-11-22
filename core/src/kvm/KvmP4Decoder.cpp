#include "KvmP4Decoder.hpp"
#include <cstring>
#include <cmath>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char* TAG = "KvmP4Decoder";
#endif

namespace cbdos {
namespace kvm {

KvmP4Decoder::KvmP4Decoder() {
}

KvmP4Decoder::~KvmP4Decoder() {
    deinit();
}

bool KvmP4Decoder::init(uint32_t maxDecW, uint32_t maxDecH) {
    if (m_initialized) return true;

    m_maxW = maxDecW;
    m_maxH = maxDecH;

#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // 1. Motor de decodificación JPEG por hardware
    jpeg_decode_engine_cfg_t decCfg = {};
    decCfg.timeout_ms = 100;
    esp_err_t err = jpeg_new_decoder_engine(&decCfg, &m_jpegDec);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando motor JPEG HW: %d", err);
        return false;
    }

    // 2. Cliente PPA SRM (Scale-Rotate-Mirror) por hardware
    ppa_client_config_t ppaCfg = {};
    ppaCfg.oper_type = PPA_OPERATION_SRM;
    err = ppa_register_client(&ppaCfg, &m_ppaSrm);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error registrando cliente PPA SRM: %d", err);
        jpeg_del_decoder_engine(m_jpegDec);
        m_jpegDec = nullptr;
        return false;
    }

    // 3. Búfer intermedio para imagen decodificada (RGB565, 2 bytes/px, alineado a 128 bytes para L2 cache)
    m_intermediateCap = m_maxW * m_maxH * 2;
    m_intermediateBuf = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(128, m_intermediateCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );

    if (!m_intermediateBuf) {
        ESP_LOGE(TAG, "No hay memoria en PSRAM para búfer intermedio de %u bytes", (unsigned)m_intermediateCap);
        ppa_unregister_client(m_ppaSrm);
        jpeg_del_decoder_engine(m_jpegDec);
        m_ppaSrm = nullptr;
        m_jpegDec = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Decodificador Hardware JPEG y Acelerador PPA P4 listos (%u x %u)", (unsigned)m_maxW, (unsigned)m_maxH);
#endif

    m_initialized = true;
    return true;
}

void KvmP4Decoder::deinit() {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (m_intermediateBuf) {
        free(m_intermediateBuf);
        m_intermediateBuf = nullptr;
    }
    if (m_ppaSrm) {
        ppa_unregister_client(m_ppaSrm);
        m_ppaSrm = nullptr;
    }
    if (m_jpegDec) {
        jpeg_del_decoder_engine(m_jpegDec);
        m_jpegDec = nullptr;
    }
#endif
    m_initialized = false;
}

bool KvmP4Decoder::decodeAndScale(const uint8_t* jpegData, size_t jpegLen,
                                  void* outRgb565, uint32_t outW, uint32_t outH) {
    if (!m_initialized || !jpegData || jpegLen == 0 || !outRgb565 || outW == 0 || outH == 0) {
        return false;
    }

#if defined(CONFIG_IDF_TARGET_ESP32P4)
    // 1. Obtener información de dimensiones del encabezado JPEG
    jpeg_decode_picture_info_t picInfo = {};
    esp_err_t err = jpeg_decoder_get_info(jpegData, static_cast<uint32_t>(jpegLen), &picInfo);
    if (err != ESP_OK || picInfo.width == 0 || picInfo.height == 0) {
        return false;
    }

    if (picInfo.width > m_maxW || picInfo.height > m_maxH) {
        ESP_LOGW(TAG, "Frame JPEG excede límites (%u x %u > %u x %u)",
                 (unsigned)picInfo.width, (unsigned)picInfo.height, (unsigned)m_maxW, (unsigned)m_maxH);
        return false;
    }

    // 2. Decodificación por hardware directamente a RGB565 en búfer intermedio
    jpeg_decode_cfg_t decCfg = {};
    decCfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    decCfg.rgb_order     = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    decCfg.conv_std      = JPEG_YUV_RGB_CONV_STD_BT601;

    uint32_t decodedSize = 0;
    err = jpeg_decoder_process(m_jpegDec, &decCfg, jpegData, static_cast<uint32_t>(jpegLen),
                               m_intermediateBuf, static_cast<uint32_t>(m_intermediateCap), &decodedSize);
    if (err != ESP_OK) {
        return false;
    }

    // 3. Reescalado y conversión por silicio con PPA SRM hacia el búfer de destino
    ppa_srm_oper_config_t srmCfg = {};

    srmCfg.in.buffer          = m_intermediateBuf;
    srmCfg.in.pic_w           = picInfo.width;
    srmCfg.in.pic_h           = picInfo.height;
    srmCfg.in.block_w         = picInfo.width;
    srmCfg.in.block_h         = picInfo.height;
    srmCfg.in.block_offset_x  = 0;
    srmCfg.in.block_offset_y  = 0;
    srmCfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;

    srmCfg.out.buffer         = outRgb565;
    srmCfg.out.buffer_size    = outW * outH * 2;
    srmCfg.out.pic_w          = outW;
    srmCfg.out.pic_h          = outH;
    srmCfg.out.block_offset_x = 0;
    srmCfg.out.block_offset_y = 0;
    srmCfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    srmCfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0;
    srmCfg.scale_x            = static_cast<float>(outW) / static_cast<float>(picInfo.width);
    srmCfg.scale_y            = static_cast<float>(outH) / static_cast<float>(picInfo.height);
    srmCfg.mode               = PPA_TRANS_MODE_BLOCKING;

    err = ppa_do_scale_rotate_mirror(m_ppaSrm, &srmCfg);
    return (err == ESP_OK);
#else
    // Host / Fallback: nada que decodificar por silicio
    return true;
#endif
}

} // namespace kvm
} // namespace cbdos

