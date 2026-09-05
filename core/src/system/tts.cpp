#include "cbdos/tts.hpp"

namespace cbdos {
namespace tts {

static ITextToSpeechService* s_ttsService = nullptr;

void setTTSService(ITextToSpeechService* service) {
    s_ttsService = service;
}

ITextToSpeechService* getTTSService() {
    return s_ttsService;
}

bool init() {
    if (s_ttsService) {
        return s_ttsService->init();
    }
    return false;
}

bool speak(const std::string& text) {
    if (s_ttsService) {
        return s_ttsService->speak(text);
    }
    return false;
}

void stop() {
    if (s_ttsService) {
        s_ttsService->stop();
    }
}

void pause() {
    if (s_ttsService) {
        s_ttsService->pause();
    }
}

void resume() {
    if (s_ttsService) {
        s_ttsService->resume();
    }
}

bool isSpeaking() {
    if (s_ttsService) {
        return s_ttsService->isSpeaking();
    }
    return false;
}

TTSState getState() {
    if (s_ttsService) {
        return s_ttsService->getState();
    }
    return TTSState::Uninitialized;
}

void setSpeed(int speedPercent) {
    if (s_ttsService) {
        s_ttsService->setSpeed(speedPercent);
    }
}

void setPitch(int pitchPercent) {
    if (s_ttsService) {
        s_ttsService->setPitch(pitchPercent);
    }
}

} // namespace tts
} // namespace cbdos
