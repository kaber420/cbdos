# 🎹 Plan de Implementación: CyBerDeck Synthesizer (SynthView)

## 📌 1. Objetivo
Diseñar una aplicación táctil interactiva de síntesis musical de baja latencia para **CBDos v0.2.0**, que funcione idéntica y con ultra bajo consumo de recursos tanto en **ESP32-P4** como en **ESP32-S3**.

---

## 🏗️ 2. Arquitectura del Generador de Síntesis
* **Ubicación:** `core/src/ui/views/SynthView.hpp` y `core/src/ui/views/SynthView.cpp`.
* **Cero librerías externas:** Síntesis matemática en tiempo real conectada a `cbdos::audio::writeAudio()`.
* **Formas de Onda:**
  1. **Cuadrada (Square 8-bit / Chiptune):** Clásica para sonidos retro estilo GameBoy/NES.
  2. **Senoidal (Sine Wave):** Tonos puros y suaves.
  3. **Sierra (Sawtooth) / Triangular:** Sonido armónico brillante y sintético.
* **Envolvente ADSR Ligera:** Curva de ataque (*Attack*) y desvanecimiento (*Decay*) para evitar chasquidos en la bocina.

---

## 🎨 3. Interfaz de Usuario (LVGL 9.5 Universal)
* **Teclado Táctil:**
  * 12 notas por octava (Teclas blancas y negras con respuesta visual al presionar y soltar).
* **Selectores Superiores:**
  * Selector de Forma de Onda: `[ 🔲 Cuadrada ] [ 〰️ Senoidal ] [ 📐 Sierra ]`.
  * Selector de Octava: `[ ◀ Octava - ]  Octava 4  [ Octava + ▶ ]`.
  * Slider de Volumen y Decay (duración de la nota).
* **Fondo:** Neón Cyberpunk con el Wallpaper activo del sistema.

---

## ⚡ 4. Rendimiento y Compatibilidad
* **ESP32-P4 (480x800):** Escala a teclas grandes con layout cómodo. Consumo de CPU < 1%.
* **ESP32-S3 (320x480):** Escala a layout compacto. Consumo de CPU < 2%.
* **Latencia de audio:** Menor a 10 ms (respuesta táctil instantánea).
