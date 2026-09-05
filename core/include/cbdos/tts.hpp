#pragma once

#include <string>
#include <cstdint>
#include <functional>

namespace cbdos {
namespace tts {

enum class TTSState {
    Uninitialized,
    Idle,
    Speaking,
    Paused,
    Error
};

class ITextToSpeechService {
public:
    virtual ~ITextToSpeechService() = default;

    /// @brief Inicializa el motor TTS (carga modelos fonéticos si no están en memoria).
    virtual bool init() = 0;

    /// @brief Encola o sintetiza un texto para reproducirlo en voz alta.
    /// @param text Texto a pronunciar en UTF-8.
    /// @return true si se encoló o comenzó la reproducción.
    virtual bool speak(const std::string& text) = 0;

    /// @brief Detiene la locución actual y vacía la cola de voz.
    virtual void stop() = 0;

    /// @brief Pausa la locución activa.
    virtual void pause() = 0;

    /// @brief Reanuda la locución pausada.
    virtual void resume() = 0;

    /// @brief Consulta si el sistema está vocalizando en este momento.
    virtual bool isSpeaking() const = 0;

    /// @brief Obtiene el estado actual del servicio TTS.
    virtual TTSState getState() const = 0;

    /// @brief Ajusta la velocidad de habla (en porcentaje: ej. 100 = normal, 120 = más rápido).
    virtual void setSpeed(int speedPercent) = 0;

    /// @brief Ajusta el tono de la voz (en porcentaje: ej. 100 = normal).
    virtual void setPitch(int pitchPercent) = 0;
};

// Registro y acceso al servicio TTS
void setTTSService(ITextToSpeechService* service);
ITextToSpeechService* getTTSService();

// Funciones de conveniencia globales
bool init();
bool speak(const std::string& text);
void stop();
void pause();
void resume();
bool isSpeaking();
TTSState getState();
void setSpeed(int speedPercent);
void setPitch(int pitchPercent);

} // namespace tts
} // namespace cbdos
