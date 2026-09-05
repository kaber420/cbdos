#pragma once

#include "cbdos/tts.hpp"
#include "cbdos/rtos.hpp"
#include <string>
#include <queue>

namespace cbdos {
namespace tts {

class PicoTTSService : public ITextToSpeechService {
public:
    static PicoTTSService& getInstance();

    bool init() override;
    bool speak(const std::string& text) override;
    void stop() override;
    void pause() override;
    void resume() override;
    bool isSpeaking() const override;
    TTSState getState() const override;
    void setSpeed(int speedPercent) override;
    void setPitch(int pitchPercent) override;

    /// @brief Libera los recursos de voz y memoria para liberar PSRAM cuando no se usa
    void shutdown();

private:
    PicoTTSService();
    ~PicoTTSService() override;
    PicoTTSService(const PicoTTSService&) = delete;
    PicoTTSService& operator=(const PicoTTSService&) = delete;

    static void ttsTaskEntry(void* param);
    void runTask();

    bool loadVoiceFiles();
    void unloadVoiceFiles();

    TTSState m_state{TTSState::Uninitialized};
    int m_speedPercent{100};
    int m_pitchPercent{100};

    // FreeRTOS y sincronización
    cbdos::rtos::TaskHandle m_taskHandle{nullptr};
    cbdos::rtos::MutexHandle m_mutex{nullptr};
    std::queue<std::string> m_textQueue;
    bool m_running{false};
    bool m_stopRequested{false};
    bool m_pauseRequested{false};

    // Recursos PicoTTS
    void* m_picoMemArea{nullptr};
    void* m_picoSystem{nullptr};
    void* m_picoEngine{nullptr};
    void* m_picoTaResource{nullptr};
    void* m_picoSgResource{nullptr};

    // Buffers de archivos fonéticos en PSRAM
    void* m_taBuffer{nullptr};
    size_t m_taSize{0};
    void* m_sgBuffer{nullptr};
    size_t m_sgSize{0};
};

} // namespace tts
} // namespace cbdos
