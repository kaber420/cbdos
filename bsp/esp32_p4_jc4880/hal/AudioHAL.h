#pragma once

#include <cstdint>
#include <cstddef>
#include <esp_err.h>
#include <driver/i2s_std.h>
#include <driver/i2c_master.h>
#include "esp_codec_dev.h"
#include "cbdos_device_tree.h"

#define BOARD_AUDIO_I2S_PORT       I2S_NUM_0
#define BOARD_AUDIO_CODEC_ADDR     0x30

class AudioHAL {
public:
    static AudioHAL& getInstance() {
        static AudioHAL instance;
        return instance;
    }

    esp_err_t init(uint32_t sampleRate = 44100);
    esp_err_t setSampleRate(uint32_t sampleRate);
    
    void setVolume(uint8_t volumePercent);
    uint8_t getVolume() const { return currentVolume; }
    
    void mute(bool enable);
    bool isMuted() const { return muted; }

    esp_err_t writeAudio(const void* src, size_t size, size_t* bytesWritten, uint32_t timeoutMs = 1000);
    esp_err_t readAudio(void* dest, size_t size, size_t* bytesRead, uint32_t timeoutMs = 100);
    void setMicGain(float dbGain);
    
    void playTone(uint32_t freqHz, uint32_t durationMs);
    void playBeep();
    void playStartupChime();

    bool isInitialized() const { return initialized; }

private:
    AudioHAL();
    ~AudioHAL();

    AudioHAL(const AudioHAL&) = delete;
    AudioHAL& operator=(const AudioHAL&) = delete;

    esp_codec_dev_handle_t playDevHandle = nullptr;
    esp_codec_dev_handle_t recordDevHandle = nullptr;
    
    uint8_t currentVolume = 70;
    bool muted = false;
    bool initialized = false;
    uint32_t currentSampleRate = 44100;
};

#ifdef __cplusplus
extern "C" {
#endif
    bool Board_Audio_Init(uint32_t sampleRate);
    void Board_Audio_SetVolume(uint8_t volumePercent);
    int Board_Audio_Write(const void* data, size_t size);
    int Board_Audio_Read(void* data, size_t size);
#ifdef __cplusplus
}
#endif
