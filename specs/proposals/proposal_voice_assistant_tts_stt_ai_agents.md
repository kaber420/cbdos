# 🎙️ Propuesta Técnica: Asistente de Voz, TTS/STT & Chat Manos Libres para Agentes de IA en CBDos

* **Estado:** 💡 Propuesta Formal
* **Autor:** Equipo CBDos / kaber420
* **Fecha:** 2026-08-30
* **Target Primario:** ESP32-P4 (JC4880P443C - 400 MHz Dual-Core RISC-V, 32 MB Hexal-PSRAM, ES8311 I2S Audio)
* **Target Secundario:** ESP32-S3 (JC3248W535 - 240 MHz Dual-Core Xtensa, 8 MB Octal-PSRAM)
* **Framework Gráfico:** LVGL v9.5
* **Arquitectura:** Núcleo Agnóstico C++ (`core/`) + HAL Abstracta + Lua Scripting Engine

---

## 📋 1. Resumen Ejecutivo y Visión

La presente propuesta define la arquitectura y diseño funcional del subsistema de **Voz, Lector de Texto Offline y Audio Conversacional** de CBDos, permitiendo transformar el dispositivo en un **lector de libros/notas digital offline** y en un **terminal inteligente manos libres** para interacción con agentes.

### 🎯 Casos de Uso Principales:
1. **📖 Lector de Libros, Textos y Notas Offline (Prioridad Inmediata):**
   * Lectura de archivos de texto (`.txt`), libros electrónicos sin formato, notas de campo y chats guardados en la MicroSD.
   * Controles multimedia completos en pantalla: **Play**, **Pausa**, **Stop**, **Velocidad ajustable** (1.0x, 1.25x, 1.5x, 2.0x), **Tono** y navegación por párrafos.
   * **100% Offline y autónomo**, sin requerir Wi-Fi ni dependencias externas.
2. **📡 Mensajería Táctica en Redes de Bajo Ancho de Banda (LoRa / ESP-NOW / Mesh):**
   * Enviar audio por LoRa es inviable por la tasa de transferencia reducida, pero enviar **paquetes de texto plano de 20 a 100 bytes es instantáneo**.
   * El ESP32 recibe el texto por LoRa/Mesh y su **TTS On-Device lo sintetiza en voz al instante por el altavoz**.
   * Permite operaciones **manos y ojos libres** en campo, rescate, taller o conducción recibiendo alertas y mensajes de voz sin saturar el canal de radio.
3. **💬 Chat Manos Libres con Lectura Automática & Bots:**
   * Al estar ocupado, el usuario activa el *Modo Voz* y los mensajes entrantes de chats o bots se leen automáticamente en voz alta mediante la cola de voz (`VoiceQueue`).
4. **📻 Agente de IA vía Streaming de Radio Online (Zero Inversión en Código de Audio):**
   * El servidor local / Homelab procesa la respuesta del agente (Ollama, DeepSeek, Piper TTS) y emite un stream de audio HTTP estándar (`http://<server_ip>:<port>/voice.mp3`).
   * CBDos **reutiliza al 100% el cliente de Radio Online existente**, sintonizando el endpoint del agente y reproduciendo la respuesta con el decodificador Helix en PSRAM de forma fluida y sin reinventar la rueda.
5. **🎮 Novelas Visuales, Juegos y Aplicaciones con Voces:**
   * Síntesis de voz accesible desde Lua (`cbdos.voice.speak`) para historias interactivas, alertas del sistema y accesibilidad.

---

## 🏛️ 2. Cumplimiento de la Ley de Pureza Arquitectónica y Offline-First

Para preservar el desacoplamiento total del sistema:
* **`core/` 100% Agnóstico:** Se definen interfaces abstractas puras (`ITextToSpeechService`, `ISpeechToTextService`, `IAudioSink`) sin referencias a SDKs específicos.
* **Offline-First Estricto:** 
  * El sistema incluye un motor TTS ligero On-Device (eSpeak-NG / SAM / TinyTTS) que funciona **100% sin conexión a internet ni red**.
  * La conectividad a servidores locales o APIs en la nube para TTS neural / STT Whisper se activa **únicamente bajo demanda** cuando el usuario lo solicita explícitamente.

```
                   ┌───────────────────────────────────────┐
                   │          Servidores & Cloud           │
                   │  • Homelab (Ollama / Piper TTS)       │
                   │  • Cloud APIs (OpenAI / Groq Whisper) │
                   └──────────────────┬────────────────────┘
                                      │ (Wi-Fi bajo demanda / HTTP / WS)
                                      ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                           CBDos Core (Agnóstico)                          │
│                                                                           │
│   ┌───────────────────────────┐         ┌─────────────────────────────┐   │
│   │   App Chat / AI Agent UI  │         │     Lua Scripting Engine    │   │
│   │        (LVGL v9.5)        │         │      (cbdos.voice.*)        │   │
│   └─────────────┬─────────────┘         └──────────────┬──────────────┘   │
│                 │                                      │                  │
│                 ▼                                      ▼                  │
│   ┌───────────────────────────────────────────────────────────────────┐   │
│   │              VoiceManager / VoiceQueue (Gestión de Cola)          │   │
│   └─────────────┬──────────────────────────────────────┬──────────────┘   │
│                 │                                      │                  │
│                 ▼                                      ▼                  │
│   ┌───────────────────────────┐         ┌─────────────────────────────┐   │
│   │   ITextToSpeechService    │         │    ISpeechToTextService     │   │
│   │ (Local eSpeak / Stream)   │         │ (Local WakeNet / Whisper)   │   │
│   └─────────────┬─────────────┘         └──────────────┬──────────────┘   │
│                 │                                      │                  │
│                 ▼                                      ▲                  │
│   ┌───────────────────────────┐         ┌──────────────┴──────────────┐   │
│   │         IAudioSink        │         │        IAudioSource         │   │
│   │      (Playback PCM)       │         │       (Record / Mic)        │   │
└─────────────────┼──────────────────────────────────────┼──────────────────┘
                  ▼                                      ▲
┌─────────────────┼──────────────────────────────────────┼──────────────────┐
│   BSP ESP32-P4  ▼                                      │                  │
│   [ Everest ES8311 I2S DAC ] ──────────────► [ Micrófono ADC/I2S / PDM ]  │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 🔌 3. Modos de Operación: Offline vs. Cloud / Servidor

### A. Texto a Voz (TTS / Text-to-Speech)

| Modo | Motor / Backend | Calidad de Voz | Consumo de Recursos | Conectividad |
| :--- | :--- | :--- | :--- | :--- |
| **Local Offline (Ultraligero)** | **SAM / TinyTTS** | Sintética / Retro 8-bit | <50 KB Flash, <1% CPU | 100% Offline |
| **Local Offline (Multilenguaje)** | **eSpeak-NG / PicoTTS** | Sintética inteligible (Español nativo) | ~2 MB PSRAM, ~15% CPU P4 | 100% Offline |
| **Servidor Homelab (Neural)** | **Piper TTS / Kokoro TTS** | Ultra-realista / Humana | Cero CPU en ESP32 (Stream MP3/WAV) | Red Local (Wi-Fi) |
| **Cloud Neural API** | **OpenAI TTS / Edge-TTS / ElevenLabs**| Máxima calidad de estudio | Decodificación MP3 en PSRAM | Internet (Wi-Fi) |

### B. Voz a Texto (STT / Speech-to-Text / ASR)

| Modo | Motor / Backend | Funcionalidad | Latencia | Conectividad |
| :--- | :--- | :--- | :--- | :--- |
| **Local Offline (Comandos)** | **ESP-SR (WakeNet + MultiNet)** | Wake-words ("Hey CBDos") y comandos fijos | <100 ms | 100% Offline |
| **Local Offline (Frases Cortas)** | **Sherpa-ONNX / Whisper Tiny Int8**| Transcripción de oraciones básicas en PSRAM | ~1.5x RTF en P4 | 100% Offline |
| **Servidor / Cloud (Transcripción)**| **Whisper API (Groq / OpenAI / Local Server)**| Transcripción completa continua y dictado | <400 ms (Groq) | Red / Internet |

---

## 💻 4. Especificación Técnica de Interfaces C++ en `core/`

### 4.1 Interfaz `ITextToSpeechService`
```cpp
#pragma once
#include <string>
#include <functional>

namespace cbdos {
namespace voice {

enum class TTSVoiceQuality {
    OFFLINE_RETRO,      // SAM / TinyTTS
    OFFLINE_SYNTHETIC,  // eSpeak-NG
    ONLINE_NEURAL       // Servidor / Cloud Stream
};

class ITextToSpeechService {
public:
    virtual ~ITextToSpeechService() = default;

    /// @brief Inicializa el motor de síntesis de voz.
    virtual bool initialize() = 0;

    /// @brief Sintetiza y reproduce el texto indicado.
    /// @param text Texto a pronunciar.
    /// @param lang Código de idioma (ej. "es", "en").
    /// @param speed Factor de velocidad (0.5 a 2.0).
    virtual bool speak(const std::string& text, const std::string& lang = "es", float speed = 1.0f) = 0;

    /// @brief Detiene inmediatamente la reproducción actual y limpia la cola.
    virtual void stop() = 0;

    /// @brief Pausa o reanuda la locución actual.
    virtual void setPaused(bool paused) = 0;

    /// @brief Verifica si el sistema está hablando en este momento.
    virtual bool isSpeaking() const = 0;
};

} // namespace voice
} // namespace cbdos
```

### 4.2 Interfaz `ISpeechToTextService` y `IAudioSource`
```cpp
#pragma once
#include <string>
#include <functional>
#include <vector>

namespace cbdos {
namespace voice {

using STTCallback = std::function<void(const std::string& transcribedText, bool isFinal)>;

class ISpeechToTextService {
public:
    virtual ~ISpeechToTextService() = default;

    /// @brief Inicia la captura y transcripción de voz (Push-to-Talk o detección por voz).
    virtual bool startListening(STTCallback callback) = 0;

    /// @brief Detiene la escucha y procesa la transcripción final.
    virtual void stopListening() = 0;

    /// @brief Indica si el micrófono está activo escuchando.
    virtual bool isListening() const = 0;
};

} // namespace voice
} // namespace cbdos
```

---

## 📜 5. Integración con Scripting Lua (`cbdos.voice`)

Para permitir que cualquier aplicación, juego o bot de chat controle la voz sin tocar C++:

```lua
-- =========================================================================
-- Ejemplo: Bot de Chat con Modo Manos Libres
-- =========================================================================

-- Configurar parámetros de voz
cbdos.voice.set_voice("es", 1.1) -- Idioma español, velocidad 1.1x

-- Escuchar evento de nuevo mensaje recibido
on_message_received = function(sender, message_text)
    -- Si el Modo Voz está activo en la UI:
    if cbdos.voice.is_hands_free_enabled() then
        -- Limpiar caracteres especiales de Markdown antes de leer
        local clean_text = cbdos.voice.sanitize_markdown(message_text)
        cbdos.voice.speak(sender .. " dice: " .. clean_text)
    end
end

-- Pulsar para hablar (Push-to-Talk):
btn_mic:on_press(function()
    cbdos.voice.start_listening(function(text, is_final)
        if is_final then
            print("Usuario dictó: " .. text)
            send_to_ai_agent(text)
        end
    end)
end)

btn_mic:on_release(function()
    cbdos.voice.stop_listening()
end)
```

---

---

## 🎨 6. Diseño de UI en LVGL 9.5

### 6.1 Lector de Libros, Textos y Notas Offline (Prioridad Inmediata)
```
┌────────────────────────────────────────────────────────┐
│ 📖 Lector Offline: capitulo_01.txt        [ 🔋 ] [ 🔊 ] │ <-- HeaderBar
├────────────────────────────────────────────────────────┤
│ "En un lugar de la Mancha, de cuyo nombre no quiero    │
│  acordarme, no ha mucho tiempo que vivía un hidalgo    │
│  de los de lanza en astillero, adarga antigua, rocín   │
│  flaco y galgo corredor..."                            │
│                                                        │
│  [► Párrafo 1/14 en reproducción activa...]            │
├────────────────────────────────────────────────────────┤
│  [ ⏮️ Ant ]   [ ▶️ Play / ⏸️ Pausa ]   [ ⏭️ Sig ]   [ ⏹️ ] │
│                                                        │
│  Velocidad:  ( 1.0x )  [ 1.25x ]  ( 1.5x )  ( 2.0x )   │
│  Progreso:   [==============-----------------] 42%     │
└────────────────────────────────────────────────────────┘
```

### 6.2 Chat Manos Libres & Asistente IA (Streaming / Radio Online)
```
┌────────────────────────────────────────────────────────┐
│ 💬 Agente IA: DeepSeek-V4          [ 📶 Wi-Fi ] [ 🔊 ] │ <-- HeaderBar con Toggle Voz
├────────────────────────────────────────────────────────┤
│ [Kaber]: ¿Cuál es el estado del servidor de backups?   │
│                                                        │
│ [DeepSeek]: El backup incremental se completó a las   │
│ 03:00 AM con 0 errores. Espacio libre: 4.2 TB.         │
│                                                        │
│ [📻 Stream Audio: http://192.168.1.50:8000/voice.mp3] │
│ [ ⏸️ Pausar ]  [ 🔁 Repetir ]  [ ⏩ 1.25x ]            │
├────────────────────────────────────────────────────────┤
│ ┌──────────────────────────────────────┐ ┌───────────┐ │
│ │ Escribe un mensaje aquí...           │ │  🎙️ PTT   │ │ <-- Botón Push-to-Talk
│ └──────────────────────────────────────┘ └───────────┘ │
└────────────────────────────────────────────────────────┘
```

### Funciones de la Interfaz:
1. **Lector de Archivos de Texto:** Carga archivos `.txt` desde la MicroSD y los pagina con resaltado del párrafo que se está vocalizando.
2. **Toggle de Modo Manos Libres en la barra superior:** Activa o desactiva la lectura automática en cualquier pantalla.
3. **Cola de Reproducción Inteligente (`VoiceQueue`):** Los mensajes entrantes se apilan sin solaparse y se reproducen en orden cronológico.
4. **Sanitizador de Markdown:** Omite URLs largas, tablas ASCII complejas o bloques de código extensos, reemplazándolos con un aviso auditivo (*"adjunto bloque de código"*).
5. **Widget de Control Flotante:** Permite pausar, saltar al siguiente párrafo/mensaje o cambiar la velocidad al vuelo.

---

## 🗺️ 7. Plan de Implementación por Fases

| Fase | Tarea | Alcance | Target |
| :---: | :--- | :--- | :---: |
| **Fase 1 (Inmediata)** | **Motor TTS Offline & `ITextToSpeechService`** | Integrar motor local ligero (`eSpeak-NG` / `SAM`) en `core/` hacia `IAudioSink`. | Agnóstico / ESP32-P4 / S3 |
| **Fase 2 (Inmediata)** | **App Lector de Libros & Textos Offline** | Interfaz LVGL 9.5 para abrir `.txt`, Play/Pausa, velocidad (1x/1.25x/1.5x/2x) y progreso. | ESP32-P4 / S3 |
| **Fase 3** | **Streaming de Agente vía Radio Online** | Reutilizar el pipeline de Radio Online (`http://<ip>:<port>/stream.mp3`) para audio del servidor. | ESP32-P4 |
| **Fase 4** | **Binding Lua & Chat Manos Libres** | Exponer `cbdos.voice.*` en `LuaBridge` y añadir cola de voz (`VoiceQueue`) a chats. | ESP32-P4 / S3 |
| **Fase 5** | **STT Local & Conexión Whisper (PTT)** | Captura de micrófono I2S desde ES8311 con Push-to-Talk y envío a Whisper. | ESP32-P4 |

---

## 📌 8. Conclusión

Esta arquitectura ofrece lo mejor de ambos mundos:
1. **Utilidad Inmediata 100% Offline:** Un potente lector de libros y notas en el bolsillo sin depender de internet ni servidores.
2. **Integración Elegante y Sin Fricción a Futuro:** Reutilización directa del reproductor de **Radio Online** para escuchar las respuestas de cualquier agente de IA en la red local.
