#include "cbdos/audio.hpp"
#include "AudioHAL.h"
#include <esp_log.h>

static const char* TAG = "P4AudioHAL";

namespace cbdos {
namespace bsp {

class P4AudioSink : public cbdos::audio::IAudioSink {
public:
    bool init(uint32_t sampleRate = 44100, uint8_t channels = 2, uint8_t bitsPerSample = 16) override {
        (void)channels;
        (void)bitsPerSample;
        return AudioHAL::getInstance().init(sampleRate) == ESP_OK;
    }

    void deinit() override {
        // Driver reside permanente
    }

    size_t write(const void* pcmData, size_t sizeBytes, uint32_t timeoutMs = 1000) override {
        size_t written = 0;
        esp_err_t err = AudioHAL::getInstance().writeAudio(pcmData, sizeBytes, &written, timeoutMs);
        return (err == ESP_OK) ? written : 0;
    }

    void setVolume(uint8_t volumePercent) override {
        AudioHAL::getInstance().setVolume(volumePercent);
    }

    uint8_t getVolume() const override {
        return AudioHAL::getInstance().getVolume();
    }

    bool setSampleRate(uint32_t sampleRate) override {
        return AudioHAL::getInstance().setSampleRate(sampleRate) == ESP_OK;
    }

    void mute(bool enable) override {
        AudioHAL::getInstance().mute(enable);
    }

    bool isMuted() const override {
        return AudioHAL::getInstance().isMuted();
    }

    void playTone(uint32_t freqHz, uint32_t durationMs) override {
        AudioHAL::getInstance().playTone(freqHz, durationMs);
    }

    void playBeep() override {
        AudioHAL::getInstance().playBeep();
    }
};

class P4AudioSource : public cbdos::audio::IAudioSource {
public:
    bool init(const cbdos::audio::RecordConfig& cfg) override {
        AudioHAL::getInstance().setMicGain((float)cfg.micGainDb);
        return true;
    }

    void deinit() override {}

    size_t read(void* dest, size_t sizeBytes, uint32_t timeoutMs = 100) override {
        size_t readBytes = 0;
        esp_err_t err = AudioHAL::getInstance().readAudio(dest, sizeBytes, &readBytes, timeoutMs);
        return (err == ESP_OK) ? readBytes : 0;
    }

    void setMicGain(uint8_t gainDb) override {
        AudioHAL::getInstance().setMicGain((float)gainDb);
    }

    float getPeakLevel() override {
        return 0.0f;
    }
};

static P4AudioSink s_p4AudioSink;
static P4AudioSource s_p4AudioSource;

void initAudioBackendP4() {
    cbdos::audio::setAudioSink(&s_p4AudioSink);
    cbdos::audio::setAudioSource(&s_p4AudioSource);
    ESP_LOGI(TAG, "Backend de Audio P4 (ES8311 I2S/I2C) inyectado con exito");
}

} // namespace bsp
} // namespace cbdos
