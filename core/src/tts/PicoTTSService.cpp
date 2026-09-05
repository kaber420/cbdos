#include "PicoTTSService.hpp"
#include "cbdos/audio.hpp"
#include "cbdos/log.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "picotts/lib/picoapi.h"
#include "picotts/esp_picorsrc.h"
}

#define CBDOS_LOGE(tag, format, ...) CBD_LOG_E(tag, format, ##__VA_ARGS__)
#define CBDOS_LOGW(tag, format, ...) CBD_LOG_W(tag, format, ##__VA_ARGS__)
#define CBDOS_LOGI(tag, format, ...) CBD_LOG_I(tag, format, ##__VA_ARGS__)

namespace cbdos {
namespace tts {

static const char* TAG = "TTS";
static const unsigned int PICO_MEM_SIZE = 1100000;
static const char* VOICE_NAME = "PicoVoice";

PicoTTSService& PicoTTSService::getInstance() {
    static PicoTTSService instance;
    return instance;
}

PicoTTSService::PicoTTSService() {
    m_mutex = cbdos::rtos::createMutex();
}

PicoTTSService::~PicoTTSService() {
    shutdown();
    if (m_mutex) {
        cbdos::rtos::deleteMutex(m_mutex);
        m_mutex = nullptr;
    }
}

static void* loadFileToMemory(const char* path, size_t* outSize) {
    if (!path || !outSize) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        return nullptr;
    }

    void* buf = malloc(sz);
    if (!buf) {
        CBDOS_LOGE(TAG, "No hay memoria suficiente para cargar %s (%ld bytes)", path, sz);
        fclose(f);
        return nullptr;
    }

    size_t readBytes = fread(buf, 1, sz, f);
    fclose(f);

    if (readBytes != (size_t)sz) {
        CBDOS_LOGE(TAG, "Error leyendo %s (leidos %zu de %ld)", path, readBytes, sz);
        free(buf);
        return nullptr;
    }

    *outSize = (size_t)sz;
    return buf;
}

bool PicoTTSService::loadVoiceFiles() {
    if (m_taBuffer && m_sgBuffer) return true;

    // Lista de rutas posibles donde pueden encontrarse los diccionarios
    const char* taPaths[] = {
        "/sdcard/tts/es/es-ES_ta.bin",
        "/sdcard/tts/es-ES_ta.bin",
        "/sdcard/system/tts/es/es-ES_ta.bin",
        "/sdcard/system/tts/es-ES_ta.bin",
        "/sdcard/resources/tts/es/es-ES_ta.bin",
        "/system/tts/es/es-ES_ta.bin",
        "/system/tts/es-ES_ta.bin"
    };

    const char* sgPaths[] = {
        "/sdcard/tts/es/es-ES_zl0_sg.bin",
        "/sdcard/tts/es-ES_zl0_sg.bin",
        "/sdcard/system/tts/es/es-ES_zl0_sg.bin",
        "/sdcard/system/tts/es-ES_zl0_sg.bin",
        "/sdcard/resources/tts/es/es-ES_zl0_sg.bin",
        "/system/tts/es/es-ES_zl0_sg.bin",
        "/system/tts/es-ES_zl0_sg.bin"
    };

    for (size_t i = 0; i < sizeof(taPaths)/sizeof(taPaths[0]); i++) {
        m_taBuffer = loadFileToMemory(taPaths[i], &m_taSize);
        if (m_taBuffer) {
            CBDOS_LOGI(TAG, "Diccionario TA cargado desde: %s (%zu bytes)", taPaths[i], m_taSize);
            break;
        }
    }

    for (size_t i = 0; i < sizeof(sgPaths)/sizeof(sgPaths[0]); i++) {
        m_sgBuffer = loadFileToMemory(sgPaths[i], &m_sgSize);
        if (m_sgBuffer) {
            CBDOS_LOGI(TAG, "Diccionario SG cargado desde: %s (%zu bytes)", sgPaths[i], m_sgSize);
            break;
        }
    }

    if (!m_taBuffer || !m_sgBuffer) {
        CBDOS_LOGW(TAG, "No se encontraron archivos de voz en /sdcard/system/tts/es/ (TA=%p, SG=%p)", m_taBuffer, m_sgBuffer);
        unloadVoiceFiles();
        return false;
    }

    return true;
}

void PicoTTSService::unloadVoiceFiles() {
    if (m_taBuffer) {
        free(m_taBuffer);
        m_taBuffer = nullptr;
        m_taSize = 0;
    }
    if (m_sgBuffer) {
        free(m_sgBuffer);
        m_sgBuffer = nullptr;
        m_sgSize = 0;
    }
}

bool PicoTTSService::init() {
    if (m_state == TTSState::Idle || m_state == TTSState::Speaking || m_state == TTSState::Paused) {
        return true;
    }

    cbdos::rtos::lockMutex(m_mutex);

    if (!loadVoiceFiles()) {
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    m_picoMemArea = malloc(PICO_MEM_SIZE);
    if (!m_picoMemArea) {
        CBDOS_LOGE(TAG, "Fallo al reservar memoria para motor PicoTTS (%u bytes)", PICO_MEM_SIZE);
        unloadVoiceFiles();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    int ret = pico_initialize(m_picoMemArea, PICO_MEM_SIZE, (pico_System*)&m_picoSystem);
    if (ret != PICO_OK) {
        CBDOS_LOGE(TAG, "pico_initialize fallo con codigo: %d", ret);
        free(m_picoMemArea);
        m_picoMemArea = nullptr;
        unloadVoiceFiles();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    ret = esp_pico_loadResource((pico_System)m_picoSystem, m_taBuffer, (pico_Resource*)&m_picoTaResource);
    if (ret != PICO_OK) {
        CBDOS_LOGE(TAG, "Fallo al cargar recurso TA en Pico (%d)", ret);
        shutdown();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    ret = esp_pico_loadResource((pico_System)m_picoSystem, m_sgBuffer, (pico_Resource*)&m_picoSgResource);
    if (ret != PICO_OK) {
        CBDOS_LOGE(TAG, "Fallo al cargar recurso SG en Pico (%d)", ret);
        shutdown();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    ret = pico_createVoiceDefinition((pico_System)m_picoSystem, (const pico_Char*)VOICE_NAME);
    if (ret != PICO_OK) {
        CBDOS_LOGE(TAG, "Fallo pico_createVoiceDefinition (%d)", ret);
        shutdown();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    pico_Retstring str;
    pico_getResourceName((pico_System)m_picoSystem, (pico_Resource)m_picoTaResource, str);
    pico_addResourceToVoiceDefinition((pico_System)m_picoSystem, (const pico_Char*)VOICE_NAME, (const pico_Char*)str);

    pico_getResourceName((pico_System)m_picoSystem, (pico_Resource)m_picoSgResource, str);
    pico_addResourceToVoiceDefinition((pico_System)m_picoSystem, (const pico_Char*)VOICE_NAME, (const pico_Char*)str);

    ret = pico_newEngine((pico_System)m_picoSystem, (const pico_Char*)VOICE_NAME, (pico_Engine*)&m_picoEngine);
    if (ret != PICO_OK) {
        CBDOS_LOGE(TAG, "Fallo al instanciar motor pico_newEngine (%d)", ret);
        shutdown();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    m_running = true;
    m_taskHandle = cbdos::rtos::createTask(ttsTaskEntry, "cbdos_tts", 16384, this, 2, 0);
    if (!m_taskHandle) {
        CBDOS_LOGE(TAG, "No se pudo crear tarea FreeRTOS de TTS");
        shutdown();
        m_state = TTSState::Error;
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    m_state = TTSState::Idle;
    CBDOS_LOGI(TAG, "Motor PicoTTS inicializado correctamente en espanol (16 kHz)");

    cbdos::rtos::unlockMutex(m_mutex);
    return true;
}

void PicoTTSService::shutdown() {
    m_running = false;
    m_stopRequested = true;

    if (m_taskHandle) {
        cbdos::rtos::sleepMs(50);
        m_taskHandle = nullptr;
    }

    if (m_picoEngine) {
        pico_disposeEngine((pico_System)m_picoSystem, (pico_Engine*)&m_picoEngine);
        pico_releaseVoiceDefinition((pico_System)m_picoSystem, (const pico_Char*)VOICE_NAME);
        m_picoEngine = nullptr;
    }

    if (m_picoSgResource) {
        esp_pico_unloadResource((pico_System)m_picoSystem, (pico_Resource*)&m_picoSgResource);
        m_picoSgResource = nullptr;
    }

    if (m_picoTaResource) {
        esp_pico_unloadResource((pico_System)m_picoSystem, (pico_Resource*)&m_picoTaResource);
        m_picoTaResource = nullptr;
    }

    if (m_picoSystem) {
        pico_terminate((pico_System*)&m_picoSystem);
        m_picoSystem = nullptr;
    }

    if (m_picoMemArea) {
        free(m_picoMemArea);
        m_picoMemArea = nullptr;
    }

    unloadVoiceFiles();
    m_state = TTSState::Uninitialized;
}

bool PicoTTSService::speak(const std::string& text) {
    if (text.empty()) return false;

    if (m_state == TTSState::Uninitialized || m_state == TTSState::Error) {
        if (!init()) {
            return false;
        }
    }

    cbdos::rtos::lockMutex(m_mutex);
    m_textQueue.push(text);
    m_stopRequested = false;
    cbdos::rtos::unlockMutex(m_mutex);

    return true;
}

void PicoTTSService::stop() {
    cbdos::rtos::lockMutex(m_mutex);
    m_stopRequested = true;
    while (!m_textQueue.empty()) {
        m_textQueue.pop();
    }
    if (m_picoEngine) {
        pico_resetEngine((pico_Engine)m_picoEngine, PICO_RESET_FULL);
    }
    m_state = TTSState::Idle;
    cbdos::rtos::unlockMutex(m_mutex);
}

void PicoTTSService::pause() {
    m_pauseRequested = true;
    m_state = TTSState::Paused;
}

void PicoTTSService::resume() {
    m_pauseRequested = false;
    if (m_state == TTSState::Paused) {
        m_state = m_textQueue.empty() ? TTSState::Idle : TTSState::Speaking;
    }
}

bool PicoTTSService::isSpeaking() const {
    return m_state == TTSState::Speaking;
}

TTSState PicoTTSService::getState() const {
    return m_state;
}

void PicoTTSService::setSpeed(int speedPercent) {
    if (speedPercent < 20) speedPercent = 20;
    if (speedPercent > 500) speedPercent = 500;
    m_speedPercent = speedPercent;
}

void PicoTTSService::setPitch(int pitchPercent) {
    if (pitchPercent < 20) pitchPercent = 20;
    if (pitchPercent > 500) pitchPercent = 500;
    m_pitchPercent = pitchPercent;
}

void PicoTTSService::ttsTaskEntry(void* param) {
    auto* self = static_cast<PicoTTSService*>(param);
    self->runTask();
}

void PicoTTSService::runTask() {
    CBDOS_LOGI(TAG, "Tarea de procesamiento TTS en ejecucion (Core 0, prioridad 2)");

    while (m_running) {
        std::string currentText;

        cbdos::rtos::lockMutex(m_mutex);
        if (!m_textQueue.empty() && !m_pauseRequested && !m_stopRequested) {
            currentText = m_textQueue.front();
            m_textQueue.pop();
        }
        cbdos::rtos::unlockMutex(m_mutex);

        if (currentText.empty()) {
            if (m_state == TTSState::Speaking) {
                m_state = TTSState::Idle;
            }
            cbdos::rtos::sleepMs(20);
            continue;
        }

        m_state = TTSState::Speaking;

        // Formatear texto con etiquetas prosódicas si aplica
        std::string formattedText;
        if (m_speedPercent != 100 || m_pitchPercent != 100) {
            char prefix[64];
            snprintf(prefix, sizeof(prefix), "<speed level=\"%d\"><pitch level=\"%d\">", m_speedPercent, m_pitchPercent);
            formattedText = prefix + currentText + "</pitch></speed>";
        } else {
            formattedText = currentText;
        }

        // Asegurar que el texto termine en puntuación y delimitador nulo para que PicoTTS compute la fonética completa
        std::string processedText = formattedText;
        char lastChar = processedText.empty() ? ' ' : processedText.back();
        if (lastChar != '.' && lastChar != '!' && lastChar != '?' && lastChar != '\n') {
            processedText.push_back('.');
        }
        processedText.push_back('\0');

        const uint8_t* textPtr = (const uint8_t*)processedText.c_str();
        size_t remainingLen = processedText.length();

        // Búfer en memoria (PSRAM) para acumular la frase completa a 16 kHz Mono
        std::vector<int16_t> monoPcm;
        monoPcm.reserve(16000 * 4); // Espacio reservado para ~4 segundos de audio

        // Fase 1: Alimentar el texto al motor y drenar datos intermedios
        while (remainingLen > 0 && !m_stopRequested && m_running) {
            if (m_pauseRequested) {
                cbdos::rtos::sleepMs(50);
                continue;
            }

            pico_Int16 processed = 0;
            int ret = pico_putTextUtf8((pico_Engine)m_picoEngine, (const pico_Char*)textPtr, (pico_Int16)remainingLen, &processed);
            if (ret != PICO_OK) {
                CBDOS_LOGE(TAG, "Error en pico_putTextUtf8 (%d)", ret);
                break;
            }

            if (processed > 0) {
                textPtr += processed;
                remainingLen -= processed;
            } else {
                // Buffer de texto temporalmente lleno, extraer muestras para liberar espacio
                int16_t outbuf[128];
                pico_Int16 bytes = 0, type = 0;
                int status = pico_getData((pico_Engine)m_picoEngine, (void*)outbuf, sizeof(outbuf), &bytes, &type);
                if (bytes > 0) {
                    size_t samples = bytes / 2;
                    monoPcm.insert(monoPcm.end(), outbuf, outbuf + samples);
                }
                cbdos::rtos::sleepMs(1);
            }
        }

        // Fase 2: Drenar todo el audio restante hasta que el sintetizador quede IDLE
        int status = PICO_STEP_BUSY;
        size_t stepCount = 0;
        while (status == PICO_STEP_BUSY && !m_stopRequested && m_running) {
            if (m_pauseRequested) {
                cbdos::rtos::sleepMs(50);
                continue;
            }

            int16_t outbuf[128];
            pico_Int16 bytes = 0, type = 0;
            status = pico_getData((pico_Engine)m_picoEngine, (void*)outbuf, sizeof(outbuf), &bytes, &type);

            if (bytes > 0) {
                size_t samples = bytes / 2;
                monoPcm.insert(monoPcm.end(), outbuf, outbuf + samples);
            }

            // Desahogo periódico cada 16 pasos para alimentar el Watchdog
            if ((++stepCount % 16) == 0) {
                cbdos::rtos::sleepMs(1);
            }
        }

        CBDOS_LOGI(TAG, "Síntesis en memoria completada: %zu muestras mono (16 kHz)", monoPcm.size());

        // Fase 3: Remuestreo continuo global a 44.1 kHz Estéreo y Reproducción limpia
        auto* sink = cbdos::audio::getAudioSink();
        if (sink && !monoPcm.empty() && !m_stopRequested && m_running) {
            // Asegurar volumen audible
            if (sink->getVolume() < 60) {
                sink->setVolume(80);
            }
            sink->setSampleRate(44100);

            // Remuestreo matemático lineal continuo sin saltos de fase
            // Ratio: 44100 / 16000 = 2.75625 (step = 16000.0f / 44100.0f)
            const float step = 16000.0f / 44100.0f;
            const size_t inSamples = monoPcm.size();
            const size_t outFrames = (size_t)((float)inSamples * (44100.0f / 16000.0f));

            std::vector<int16_t> stereo44k;
            stereo44k.resize(outFrames * 2);

            float inPos = 0.0f;
            for (size_t outIdx = 0; outIdx < outFrames; ++outIdx) {
                size_t i0 = (size_t)inPos;
                if (i0 >= inSamples) break;
                size_t i1 = (i0 + 1 < inSamples) ? i0 + 1 : i0;
                float frac = inPos - (float)i0;
                int16_t s0 = monoPcm[i0];
                int16_t s1 = monoPcm[i1];
                int16_t sample = (int16_t)(s0 + frac * (float)(s1 - s0));

                stereo44k[outIdx * 2]     = sample;
                stereo44k[outIdx * 2 + 1] = sample;
                inPos += step;
            }

            // Reproducir en bloques estándar de 1024 frames estéreo (4096 bytes)
            const size_t chunkSizeFrames = 1024;
            const size_t chunkSizeBytes = chunkSizeFrames * 2 * sizeof(int16_t);
            size_t totalBytes = stereo44k.size() * sizeof(int16_t);
            const uint8_t* p = (const uint8_t*)stereo44k.data();

            CBDOS_LOGI(TAG, "Iniciando reproduccion DMA continua: %zu bytes estéreo (44.1 kHz)", totalBytes);

            while (totalBytes > 0 && !m_stopRequested && m_running) {
                if (m_pauseRequested) {
                    cbdos::rtos::sleepMs(50);
                    continue;
                }
                size_t toWrite = (totalBytes > chunkSizeBytes) ? chunkSizeBytes : totalBytes;
                sink->write(p, toWrite, 1000);
                p += toWrite;
                totalBytes -= toWrite;
            }
        }

        // Limpiar motor si hubo cancelación
        if (m_stopRequested) {
            pico_resetEngine((pico_Engine)m_picoEngine, PICO_RESET_FULL);
            m_stopRequested = false;
        }

        m_state = TTSState::Idle;
    }

    CBDOS_LOGI(TAG, "Tarea TTS finalizada");
}

} // namespace tts
} // namespace cbdos
