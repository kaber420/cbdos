#include "AudioPlayer.hpp"
#include "cbdos/network.hpp"
#include "cbdos/log.hpp"
#include "cbdos/memory.hpp"
#include "cbdos/socket.hpp"
#include "mp3dec.h"
#include "aacdec.h"
#include <cstring>
#include <algorithm>

static const char* TAG = "AudioPlayer";

namespace cbdos {
namespace audio {

AudioPlayer& AudioPlayer::getInstance() {
    static AudioPlayer instance;
    return instance;
}

AudioPlayer::AudioPlayer() {
    m_mutex = cbdos::rtos::createMutex();
}

AudioPlayer::~AudioPlayer() {
    stop();
    if (m_mutex) {
        cbdos::rtos::deleteMutex(m_mutex);
        m_mutex = nullptr;
    }
}

AudioFormat AudioPlayer::detectFormat(const char* filepath) {
    if (!filepath) return AudioFormat::Unknown;
    std::string path = filepath;
    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    if (path.ends_with(".mp3")) return AudioFormat::MP3;
    if (path.ends_with(".wav")) return AudioFormat::WAV;
    if (path.ends_with(".aac") || path.ends_with(".m4a")) return AudioFormat::AAC;
    if (path.ends_with(".flac")) return AudioFormat::FLAC;
    return AudioFormat::Unknown;
}

std::string AudioPlayer::getCurrentTrackTitle() const {
    if (m_currentFile.empty()) return "Sin pista seleccionada";
    size_t lastSlash = m_currentFile.find_last_of("/\\");
    std::string name = (lastSlash != std::string::npos) ? m_currentFile.substr(lastSlash + 1) : m_currentFile;
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) {
        name = name.substr(0, lastDot);
    }
    return name;
}

std::string AudioPlayer::getFormatString() const {
    char buf[64];
    if (m_format == AudioFormat::MP3) {
        snprintf(buf, sizeof(buf), "Helix MP3 • %lu Hz • %lu kbps", m_sampleRate, m_bitrate / 1000);
    } else if (m_format == AudioFormat::AAC) {
        snprintf(buf, sizeof(buf), "Helix AAC • %lu Hz • %lu kbps", m_sampleRate, m_bitrate / 1000);
    } else if (m_format == AudioFormat::WAV) {
        snprintf(buf, sizeof(buf), "WAV PCM • %lu Hz • %s", m_sampleRate, m_channels == 2 ? "Estereo" : "Mono");
    } else {
        snprintf(buf, sizeof(buf), "Audio • %lu Hz", m_sampleRate);
    }
    return std::string(buf);
}

uint32_t AudioPlayer::getCurrentTimeSec() const {
    return m_currentSec;
}

float AudioPlayer::getProgress() const {
    if (m_totalTimeSec == 0) return 0.0f;
    float prog = (float)m_currentSec / (float)m_totalTimeSec;
    if (prog < 0.0f) prog = 0.0f;
    if (prog > 1.0f) prog = 1.0f;
    return prog;
}

void AudioPlayer::pause() {
    m_isPaused = true;
    CBD_LOG_I(TAG, "Audio pausado");
}

void AudioPlayer::resume() {
    m_isPaused = false;
    CBD_LOG_I(TAG, "Audio reanudado");
}

void AudioPlayer::seek(uint32_t targetSec) {
    if (targetSec > m_totalTimeSec) targetSec = m_totalTimeSec;
    m_seekRequestSec = (int32_t)targetSec;
    CBD_LOG_I(TAG, "Seek solicitado a %lu segundos", targetSec);
}

void AudioPlayer::stop() {
    m_stopRequested = true;
    m_isPaused = false;
    if (m_taskHandle) {
        int timeout = 150;
        while (m_isPlaying && timeout-- > 0) {
            cbdos::rtos::sleepMs(10);
        }
    }
    m_isPlaying = false;
}

static uint32_t getID3v2Size(FILE* f) {
    if (!f) return 0;
    uint8_t header[10];
    fseek(f, 0, SEEK_SET);
    if (fread(header, 1, 10, f) == 10) {
        if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
            uint32_t id3Size = ((header[6] & 0x7F) << 21) |
                               ((header[7] & 0x7F) << 14) |
                               ((header[8] & 0x7F) << 7)  |
                                (header[9] & 0x7F);
            uint32_t totalOffset = id3Size + 10;
            CBD_LOG_I(TAG, "ID3v2 detectado: saltando %lu bytes de metadatos/caratula", totalOffset);
            return totalOffset;
        }
    }
    fseek(f, 0, SEEK_SET);
    return 0;
}

// Sondea la pista MP3 para extraer frecuencia, canales y bitrate antes de iniciar el streaming (de espOS32)
static int probeMP3SampleRate(FILE* f, uint32_t id3Offset, uint8_t* outChans, uint32_t* outBitrate) {
    if (!f) return 44100;
    HMP3Decoder dec = MP3InitDecoder();
    if (!dec) return 44100;

    const int PROBE_SIZE = 4096;
    uint8_t* probeBuf = (uint8_t*)malloc(PROBE_SIZE);
    if (!probeBuf) {
        MP3FreeDecoder(dec);
        return 44100;
    }

    fseek(f, id3Offset, SEEK_SET);
    int bytesRead = fread(probeBuf, 1, PROBE_SIZE, f);
    fseek(f, id3Offset, SEEK_SET);

    if (bytesRead <= 0) {
        free(probeBuf);
        MP3FreeDecoder(dec);
        return 44100;
    }

    uint8_t* ptr = probeBuf;
    int left = bytesRead;

    int offset = MP3FindSyncWord(ptr, left);
    if (offset < 0) {
        free(probeBuf);
        MP3FreeDecoder(dec);
        return 44100;
    }
    ptr += offset;
    left -= offset;

    int16_t* tmpBuf = (int16_t*)malloc(MAX_NCHAN * MAX_NGRAN * MAX_NSAMP * sizeof(int16_t));
    int sampRate = 44100;
    if (tmpBuf) {
        int err = MP3Decode(dec, &ptr, &left, tmpBuf, 0);
        if (err == ERR_MP3_NONE) {
            MP3FrameInfo info;
            MP3GetLastFrameInfo(dec, &info);
            if (info.samprate > 0) sampRate = info.samprate;
            if (outChans) *outChans = (uint8_t)info.nChans;
            if (outBitrate) *outBitrate = (uint32_t)info.bitrate;
            CBD_LOG_I(TAG, "[Audio Probe] %d Hz, %d canales, %d kbps",
                     info.samprate, info.nChans, info.bitrate / 1000);
        }
        free(tmpBuf);
    }
    free(probeBuf);
    MP3FreeDecoder(dec);
    return sampRate;
}

bool AudioPlayer::play(const char* filepath) {
    if (!filepath) return false;

    stop();

    if (!cbdos::rtos::lockMutex(m_mutex, 500)) {
        return false;
    }

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        std::string altPath = filepath;
        if (altPath.rfind("/sdcard/", 0) == 0) {
            altPath = "/sd/" + altPath.substr(8);
            f = fopen(altPath.c_str(), "rb");
        } else if (altPath.rfind("/sd/", 0) == 0) {
            altPath = "/sdcard/" + altPath.substr(4);
            f = fopen(altPath.c_str(), "rb");
        }
    }
    if (!f) {
        CBD_LOG_E(TAG, "No se pudo abrir el archivo: %s", filepath);
        cbdos::rtos::unlockMutex(m_mutex);
        return false;
    }

    // Obtener tamaño del archivo
    fseek(f, 0, SEEK_END);
    m_fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    m_file = f;
    m_currentFile = filepath;
    m_format = detectFormat(filepath);
    m_bytesProcessed = 0;
    m_currentSec = 0;
    m_totalTimeSec = 0;
    m_isPlaying = true;
    m_isPaused = false;
    m_stopRequested = false;
    m_seekRequestSec = -1;

    CBD_LOG_I(TAG, "Iniciando Helix Audio Player para: %s (Tamano: %lu KB, Formato: %d)",
             filepath, m_fileSize / 1024, (int)m_format);

    // Si es MP3, ejecutar el Probe previo de espOS32 y configurar el I2S de inmediato
    if (m_format == AudioFormat::MP3) {
        uint32_t id3 = getID3v2Size(m_file);
        uint8_t chans = 2;
        uint32_t br = 128000;
        int realRate = probeMP3SampleRate(m_file, id3, &chans, &br);
        m_sampleRate = realRate;
        m_channels = chans;
        m_bitrate = br;
        cbdos::audio::setSampleRate(m_sampleRate);
    }

    m_taskHandle = cbdos::rtos::createTask(playbackTask, "helix_audio_task", 8192, this, 6, 0);

    cbdos::rtos::unlockMutex(m_mutex);
    return true;
}

bool AudioPlayer::playStream(const char* url) {
    if (!url || strlen(url) == 0) return false;

    if (!cbdos::network::isConnected()) {
        CBD_LOG_W(TAG, "[Stream] No se puede iniciar streaming sin conexion de red");
        return false;
    }

    stop();

    if (!cbdos::rtos::lockMutex(m_mutex, 500)) {
        return false;
    }

    m_file = nullptr;
    m_currentFile = url;
    m_format = AudioFormat::MP3;
    m_isStream = true;
    m_fileSize = 0;
    m_bytesProcessed = 0;
    m_currentSec = 0;
    m_totalTimeSec = 0;
    m_isPlaying = true;
    m_isPaused = false;
    m_stopRequested = false;
    m_seekRequestSec = -1;
    m_sampleRate = 44100;
    m_channels = 2;
    m_bitrate = 128000;

    cbdos::audio::setSampleRate(44100);

    CBD_LOG_I(TAG, "Iniciando Helix Stream Player para: %s", url);

    m_taskHandle = cbdos::rtos::createTask(streamPlaybackTask, "helix_stream_task", 12288, this, 6, 0);

    cbdos::rtos::unlockMutex(m_mutex);
    return true;
}

void AudioPlayer::playbackTask(void* param) {
    AudioPlayer* player = static_cast<AudioPlayer*>(param);
    player->runPlayback();
    cbdos::rtos::deleteTask(nullptr);
}

void AudioPlayer::streamPlaybackTask(void* param) {
    AudioPlayer* player = static_cast<AudioPlayer*>(param);
    player->runStreamPlayback();
    cbdos::rtos::deleteTask(nullptr);
}

void AudioPlayer::runStreamPlayback() {
    std::string currentUrl = m_currentFile;
    int maxRedirects = 5;
    std::unique_ptr<cbdos::network::ISocketStream> client;

    for (int r = 0; r < maxRedirects && !m_stopRequested; r++) {
        std::string host;
        uint16_t portNum = 80;
        std::string path = "/";

        std::string urlToParse = currentUrl;
        if (urlToParse.rfind("http://", 0) == 0) {
            urlToParse = urlToParse.substr(7);
        } else if (urlToParse.rfind("https://", 0) == 0) {
            urlToParse = urlToParse.substr(8);
            portNum = 443;
        }

        size_t slashIdx = urlToParse.find('/');
        if (slashIdx != std::string::npos) {
            host = urlToParse.substr(0, slashIdx);
            path = urlToParse.substr(slashIdx);
        } else {
            host = urlToParse;
            path = "/";
        }

        size_t colonIdx = host.find(':');
        if (colonIdx != std::string::npos) {
            portNum = (uint16_t)std::atoi(host.substr(colonIdx + 1).c_str());
            host = host.substr(0, colonIdx);
        }

        CBD_LOG_I(TAG, "[Stream] Conectando a %s:%u%s", host.c_str(), portNum, path.c_str());

        client = cbdos::network::createSocket(cbdos::network::SocketType::Tcp);
        if (!client) {
            CBD_LOG_E(TAG, "[Stream] Fallo al instanciar socket");
            break;
        }

        if (!client->connect(host, portNum, 6000)) {
            CBD_LOG_E(TAG, "[Stream] Fallo al conectar socket con %s:%u", host.c_str(), portNum);
            client.reset();
            break;
        }

        char req[512];
        int reqLen = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: CBDos-Radio/2.0\r\n"
            "Accept: */*\r\n"
            "Icy-MetaData: 0\r\n"
            "Connection: close\r\n\r\n",
            path.c_str(), host.c_str());

        if (client->send((const uint8_t*)req, reqLen) < 0) {
            CBD_LOG_E(TAG, "[Stream] Error enviando HTTP GET");
            client.reset();
            break;
        }

        // Leer cabecera HTTP
        std::string headerBuf;
        uint8_t c;
        while (headerBuf.find("\r\n\r\n") == std::string::npos && !m_stopRequested) {
            int n = client->recv(&c, 1, 3000);
            if (n <= 0) break;
            headerBuf.push_back((char)c);
            if (headerBuf.size() > 4096) break;
        }

        // Analizar si hay redirección (301, 302, 307)
        if (headerBuf.find("301 ") != std::string::npos ||
            headerBuf.find("302 ") != std::string::npos ||
            headerBuf.find("307 ") != std::string::npos) {
            size_t locPos = headerBuf.find("Location: ");
            if (locPos == std::string::npos) locPos = headerBuf.find("location: ");
            if (locPos != std::string::npos) {
                size_t locEnd = headerBuf.find("\r\n", locPos);
                std::string newLoc = headerBuf.substr(locPos + 10, locEnd - (locPos + 10));
                CBD_LOG_I(TAG, "[Stream] Redirigiendo a: %s", newLoc.c_str());
                currentUrl = newLoc;
                client.reset();
                continue;
            }
        }

        // Conectado y cabeceras leídas
        break;
    }

    if (!client || !client->isConnected() || m_stopRequested) {
        if (client) client->close();
        m_isPlaying = false;
        m_taskHandle = nullptr;
        return;
    }

    HMP3Decoder hMP3Decoder = MP3InitDecoder();
    if (!hMP3Decoder) {
        CBD_LOG_E(TAG, "[Stream] Fallo al inicializar decoder Helix MP3");
        client->close();
        m_isPlaying = false;
        m_taskHandle = nullptr;
        return;
    }

    const size_t IN_BUF_SIZE = 32768; // 32 KB de buffer en memoria
    uint8_t* inBuf = (uint8_t*)cbdos::mem::alloc_psram(IN_BUF_SIZE);
    int16_t* pcmBuf = (int16_t*)cbdos::mem::alloc_psram(MAX_NCHAN * MAX_NGRAN * MAX_NSAMP * sizeof(int16_t) * 2);

    if (!inBuf || !pcmBuf) {
        CBD_LOG_E(TAG, "[Stream] Memoria insuficiente para buffers");
        if (inBuf) cbdos::mem::free_mem(inBuf);
        if (pcmBuf) cbdos::mem::free_mem(pcmBuf);
        MP3FreeDecoder(hMP3Decoder);
        client->close();
        m_isPlaying = false;
        m_taskHandle = nullptr;
        return;
    }

    int bytesLeft = 0;
    uint8_t* readPtr = inBuf;
    uint64_t totalSamplesDecoded = 0;

    m_totalTimeSec = 0; // En streaming de radio la duración es infinita

    while (!m_stopRequested) {
        if (m_isPaused) {
            cbdos::rtos::sleepMs(50);
            continue;
        }

        if (bytesLeft < (int)(IN_BUF_SIZE / 2)) {
            if (bytesLeft > 0 && readPtr != inBuf) {
                memmove(inBuf, readPtr, bytesLeft);
            }
            readPtr = inBuf;
            size_t toRead = IN_BUF_SIZE - bytesLeft;
            int nRead = client->recv(inBuf + bytesLeft, toRead, 1000);
            if (nRead <= 0) {
                if (m_stopRequested) break;
                if (client->isConnected()) {
                    continue; // Timeout momentáneo de red, reintentar recepción
                }
                CBD_LOG_W(TAG, "[Stream] Socket cerrado o timeout por el servidor");
                break;
            }
            bytesLeft += nRead;
        }

        int offset = MP3FindSyncWord(readPtr, bytesLeft);
        if (offset < 0) {
            if (bytesLeft > 3) {
                memmove(inBuf, readPtr + bytesLeft - 3, 3);
                bytesLeft = 3;
            }
            readPtr = inBuf;
            cbdos::rtos::sleepMs(1);
            continue;
        }

        readPtr += offset;
        bytesLeft -= offset;

        int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, pcmBuf, 0);
        if (err == ERR_MP3_NONE) {
            MP3FrameInfo info;
            MP3GetLastFrameInfo(hMP3Decoder, &info);

            if (info.samprate > 0 && info.samprate != m_sampleRate) {
                m_sampleRate = info.samprate;
                cbdos::audio::setSampleRate(m_sampleRate);
            }
            m_channels = info.nChans;
            m_bitrate = info.bitrate;

            int outSamples = info.outputSamps;
            if (info.nChans == 1) {
                for (int i = outSamples - 1; i >= 0; i--) {
                    pcmBuf[i * 2]     = pcmBuf[i];
                    pcmBuf[i * 2 + 1] = pcmBuf[i];
                }
                outSamples *= 2;
            }

            cbdos::audio::writeAudio(pcmBuf, outSamples * sizeof(int16_t));
            totalSamplesDecoded += (info.outputSamps / (info.nChans ? info.nChans : 2));
            m_currentSec = totalSamplesDecoded / (m_sampleRate ? m_sampleRate : 44100);
        } else {
            cbdos::rtos::sleepMs(1);
        }
    }

    client->close();
    cbdos::mem::free_mem(inBuf);
    cbdos::mem::free_mem(pcmBuf);
    MP3FreeDecoder(hMP3Decoder);

    m_isPlaying = false;
    m_isPaused = false;
    m_taskHandle = nullptr;
    CBD_LOG_I(TAG, "[Stream] Reproduccion de radio finalizada");
}

void AudioPlayer::runPlayback() {
    if (m_format == AudioFormat::MP3) {
        runMp3Playback();
    } else if (m_format == AudioFormat::WAV) {
        runWavPlayback();
    } else {
        runMp3Playback();
    }

    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }

    m_isPlaying = false;
    m_isPaused = false;
    m_taskHandle = nullptr;
    CBD_LOG_I(TAG, "Reproduccion finalizada");
}

void AudioPlayer::runMp3Playback() {
    HMP3Decoder hMP3Decoder = MP3InitDecoder();
    if (!hMP3Decoder) {
        CBD_LOG_E(TAG, "Fallo al inicializar decoder Helix MP3");
        return;
    }

    // Saltar cabecera ID3v2 (carátula / metadatos) para ir directo al audio
    uint32_t id3Offset = getID3v2Size(m_file);
    fseek(m_file, id3Offset, SEEK_SET);

    const size_t IN_BUF_SIZE = 16384;
    uint8_t* inBuf = (uint8_t*)cbdos::mem::alloc_psram(IN_BUF_SIZE);
    // Buffer para muestras (suficiente para 2304 muestras estéreo de 16 bits)
    int16_t* pcmBuf = (int16_t*)cbdos::mem::alloc_psram(MAX_NCHAN * MAX_NGRAN * MAX_NSAMP * sizeof(int16_t) * 2);

    if (!inBuf || !pcmBuf) {
        CBD_LOG_E(TAG, "Memoria insuficiente para buffers Helix");
        if (inBuf) cbdos::mem::free_mem(inBuf);
        if (pcmBuf) cbdos::mem::free_mem(pcmBuf);
        MP3FreeDecoder(hMP3Decoder);
        return;
    }

    int bytesLeft = 0;
    uint8_t* readPtr = inBuf;
    uint64_t totalSamplesDecoded = 0;
    bool eofReached = false;

    m_totalTimeSec = (m_fileSize > id3Offset) ? ((m_fileSize - id3Offset) / 16000) : 180;

    while (!m_stopRequested && m_file && (!eofReached || bytesLeft > 0)) {
        if (m_isPaused) {
            cbdos::rtos::sleepMs(50);
            continue;
        }

        if (m_seekRequestSec >= 0) {
            if (m_totalTimeSec > 0) {
                uint32_t targetByte = id3Offset + (uint32_t)(((uint64_t)m_seekRequestSec * (m_fileSize - id3Offset)) / m_totalTimeSec);
                fseek(m_file, targetByte, SEEK_SET);
                m_currentSec = m_seekRequestSec;
                totalSamplesDecoded = (uint64_t)m_currentSec * m_sampleRate;
            }
            bytesLeft = 0;
            readPtr = inBuf;
            eofReached = false;
            m_seekRequestSec = -1;
        }

        // Rellenar buffer desde la SD cuando quede menos de la mitad
        if (bytesLeft < (int)(IN_BUF_SIZE / 2) && !eofReached) {
            if (bytesLeft > 0 && readPtr != inBuf) {
                memmove(inBuf, readPtr, bytesLeft);
            }
            readPtr = inBuf;
            size_t toRead = IN_BUF_SIZE - bytesLeft;
            size_t nRead = fread(inBuf + bytesLeft, 1, toRead, m_file);
            bytesLeft += nRead;
            if (nRead < toRead) {
                eofReached = true;
            }
        }

        int offset = MP3FindSyncWord(readPtr, bytesLeft);
        if (offset < 0) {
            if (eofReached) {
                // Fin del archivo alcanzado
                break;
            }
            if (bytesLeft > 3) {
                memmove(inBuf, readPtr + bytesLeft - 3, 3);
                bytesLeft = 3;
            }
            readPtr = inBuf;
            cbdos::rtos::sleepMs(1);
            continue;
        }

        readPtr += offset;
        bytesLeft -= offset;

        int err = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, pcmBuf, 0);
        if (err == ERR_MP3_NONE) {
            MP3FrameInfo info;
            MP3GetLastFrameInfo(hMP3Decoder, &info);
            
            if (info.samprate > 0 && info.samprate != m_sampleRate) {
                m_sampleRate = info.samprate;
                cbdos::audio::setSampleRate(m_sampleRate);
            }
            m_channels = info.nChans;
            m_bitrate = info.bitrate;

            if (info.bitrate > 0) {
                m_totalTimeSec = ((m_fileSize - id3Offset) * 8) / info.bitrate;
            }

            int outSamples = info.outputSamps;
            // Si la pista es Mono (1 canal), duplicar a Estéreo para el bus I2S
            if (info.nChans == 1) {
                for (int i = outSamples - 1; i >= 0; i--) {
                    pcmBuf[i * 2]     = pcmBuf[i];
                    pcmBuf[i * 2 + 1] = pcmBuf[i];
                }
                outSamples *= 2;
            }

            int pcmBytes = outSamples * sizeof(int16_t);
            cbdos::audio::writeAudio(pcmBuf, pcmBytes);

            totalSamplesDecoded += (info.outputSamps / (info.nChans > 0 ? info.nChans : 1));
            if (m_sampleRate > 0) {
                m_currentSec = (uint32_t)(totalSamplesDecoded / m_sampleRate);
            }
        } else if (err != ERR_MP3_INDATA_UNDERFLOW) {
            readPtr++;
            bytesLeft--;
            cbdos::rtos::sleepMs(1);
        } else {
            cbdos::rtos::sleepMs(1);
        }
    }

    cbdos::mem::free_mem(inBuf);
    cbdos::mem::free_mem(pcmBuf);
    MP3FreeDecoder(hMP3Decoder);
}

void AudioPlayer::runWavPlayback() {
    uint8_t riffHeader[12];
    if (fread(riffHeader, 1, 12, m_file) != 12 || memcmp(riffHeader, "RIFF", 4) != 0) {
        CBD_LOG_E(TAG, "Formato WAV invalido: No se encontro cabecera RIFF");
        return;
    }

    uint32_t dataSize = m_fileSize - 44;
    uint32_t byteRate = 176400; // 44.1kHz * 16-bit * 2ch por defecto

    bool dataFound = false;
    while (!dataFound && !feof(m_file)) {
        uint8_t chunkHeader[8];
        if (fread(chunkHeader, 1, 8, m_file) != 8) break;
        uint32_t chunkSize = *(uint32_t*)(chunkHeader + 4);

        if (memcmp(chunkHeader, "fmt ", 4) == 0) {
            uint8_t fmtData[16];
            fread(fmtData, 1, 16, m_file);
            uint16_t numCh = *(uint16_t*)(fmtData + 2);
            uint32_t sampleRate = *(uint32_t*)(fmtData + 4);
            byteRate = *(uint32_t*)(fmtData + 8);
            
            m_sampleRate = sampleRate;
            m_channels = numCh;
            cbdos::audio::setSampleRate(m_sampleRate);
            
            if (chunkSize > 16) {
                fseek(m_file, chunkSize - 16, SEEK_CUR);
            }
        } else if (memcmp(chunkHeader, "data", 4) == 0) {
            dataSize = chunkSize;
            dataFound = true;
        } else {
            fseek(m_file, chunkSize, SEEK_CUR);
        }
    }

    m_totalTimeSec = (byteRate > 0) ? (dataSize / byteRate) : 0;
    uint32_t dataStart = ftell(m_file);

    const size_t CHUNK_SIZE = 4096;
    uint8_t* pcmBuf = (uint8_t*)malloc(CHUNK_SIZE);
    if (!pcmBuf) return;

    while (!m_stopRequested && m_file) {
        if (m_isPaused) {
            cbdos::rtos::sleepMs(50);
            continue;
        }

        if (m_seekRequestSec >= 0) {
            uint32_t targetByte = (uint32_t)m_seekRequestSec * byteRate;
            if (targetByte > dataSize) targetByte = dataSize;
            fseek(m_file, dataStart + targetByte, SEEK_SET);
            m_bytesProcessed = targetByte;
            m_seekRequestSec = -1;
        }

        size_t nRead = fread(pcmBuf, 1, CHUNK_SIZE, m_file);
        if (nRead == 0) break;

        cbdos::audio::writeAudio(pcmBuf, nRead);

        m_bytesProcessed += nRead;
        if (byteRate > 0) {
            m_currentSec = m_bytesProcessed / byteRate;
        }
    }

    free(pcmBuf);
}

AudioStats AudioPlayer::getStats() const {
    AudioStats stats;
    stats.isPlaying = m_isPlaying && !m_isPaused;
    switch (m_format) {
        case AudioFormat::MP3: stats.codec = CodecType::MP3; break;
        case AudioFormat::AAC: stats.codec = CodecType::AAC; break;
        case AudioFormat::WAV: stats.codec = CodecType::WAV; break;
        default: stats.codec = CodecType::None; break;
    }
    stats.sampleRate = m_sampleRate;
    stats.channels = m_channels;
    stats.bitRate = m_bitrate;
    stats.bufferPercent = 100;
    return stats;
}

} // namespace audio
} // namespace cbdos
