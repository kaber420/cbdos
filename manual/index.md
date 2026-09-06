<div style="text-align: center; padding: 3rem 1.5rem; background: radial-gradient(circle at 50% 0%, rgba(0, 240, 255, 0.18) 0%, rgba(10, 10, 15, 0) 75%); border-radius: 16px; border: 1px solid rgba(0, 240, 255, 0.25); margin-bottom: 2.5rem; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);">
  <h1 style="font-size: 2.8rem; font-weight: 900; margin-bottom: 0.6rem; background: linear-gradient(90deg, #00f0ff 0%, #7000ff 50%, #ff0077 100%); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: -0.5px;">
    CyBerDeck OS
  </h1>
  <p style="font-size: 1.25rem; color: #cbd5e1; max-width: 680px; margin: 0 auto 1.6rem auto; line-height: 1.6;">
    El sistema operativo gráfico y modular para cyberdecks y consolas portátiles basadas en ESP32.
  </p>
  
  <div style="display: flex; gap: 0.6rem; justify-content: center; flex-wrap: wrap; margin-bottom: 2.2rem;">
    <img src="https://img.shields.io/badge/LVGL-v9.5%20Strict-00f0ff?style=for-the-badge&logo=cplusplus&logoColor=black" alt="LVGL 9.5">
    <img src="https://img.shields.io/badge/Target-ESP32--P4%20%7C%20S3-7000ff?style=for-the-badge" alt="Multi-target">
    <img src="https://img.shields.io/badge/Mode-Offline--First-00ff66?style=for-the-badge" alt="Offline First">
    <img src="https://img.shields.io/badge/Runtime-Lua%2B%2B%20Sandboxed-ff0055?style=for-the-badge" alt="Lua++">
    <img src="https://img.shields.io/badge/Licence-GPL--3.0-orange?style=for-the-badge" alt="GPLv3">
  </div>

  <div style="display: flex; gap: 1.2rem; justify-content: center; flex-wrap: wrap;">
    <a href="getting-started/flashing.md" style="background: #00f0ff; color: #050b14; font-weight: 800; border-radius: 8px; padding: 0.75rem 1.8rem; text-decoration: none;">
      ⚡ Guía Rápida de Flasheo
    </a>
    <a href="developers/luapp.md" style="border: 1px solid #00f0ff; color: #00f0ff; border-radius: 8px; padding: 0.75rem 1.8rem; font-weight: 600; text-decoration: none;">
      💻 Escribir Apps en Lua++
    </a>
    <a href="hardware/supported-boards.md" style="border: 1px solid #7000ff; color: #a855f7; border-radius: 8px; padding: 0.75rem 1.8rem; font-weight: 600; text-decoration: none;">
      🔌 Hardware Compatible
    </a>
  </div>
</div>

---

## ⚙️ Capacidades del Sistema Operativo

### 🎨 Motor Vectorial & Animaciones Lottie
Integración del motor vectorial **rlottie** sobre LVGL v9.5. Permite renderizar animaciones vectoriales JSON fluidas y escalables para widgets interactivos, efectos visuales y mascotas virtuales sin pixelado ni consumo excesivo de memoria.

### 🗣️ Síntesis de Voz Offline (PicoTTS)
Motor TTS (Text-To-Speech) por software integrado en el núcleo. Permite que el sistema y las aplicaciones vocalicen alertas, textos y lecturas en voz alta a través del códec I2S Everest ES8311 sin necesidad de conexión a internet.

### 🖥️ Aceleración Gráfica 2D en PSRAM (LVGL v9.5)
Gestión avanzada de memoria gráfica: doble buffer ubicado en la memoria Hexal-PSRAM de alta velocidad con aceleración por hardware (DMA2D / PPA en ESP32-P4) a 60 FPS estables.

### 💾 Almacenamiento VFS en MicroSD (SDMMC 4-bit)
Sistema de archivos virtual FAT32/exFAT de alta velocidad a través del bus SDMMC de 4 bits con alimentación regulada por LDO VO4 (3.3V), garantizando transferencias rápidas para logs, música, assets y cartuchos.

### 💻 Entorno de Scripting Lua++ (.luapp)
Ejecuta micro-aplicaciones y herramientas dinámicas directamente desde la tarjeta MicroSD sin necesidad de recompilar el firmware. Máquina virtual aislada en PSRAM con bindings a pantalla, audio, archivos y USB.

### 🕹️ Gestor de Cartuchos en Particiones OTA
Permite instalar, alternar y arrancar firmwares o cartuchos independientes (.bin) almacenados en la MicroSD directamente en las particiones OTA de la memoria Flash del dispositivo, facilitando el cambio a sistemas dedicados (como emuladores con soporte para gamepad).

### ⌨️ Emulador HID USB Compuesto
Control nativo del periférico USB-OTG mediante TinyUSB en modo teclado y ratón compuesto, habilitando la ejecución reactiva de secuencias DuckyScript v2.

---

## 🎯 Placas y Dispositivos Soportados

| Dispositivo | SoC & Frecuencia | Pantalla & Táctil | Audio & Periféricos | Estado |
| :--- | :--- | :--- | :--- | :--- |
| **Guition JC4880P443C** | ESP32-P4 Dual-Core RISC-V @ 400 MHz | 4.3" IPS 480x800 MIPI-DPI + Goodix GT911 | Codec ES8311 + SDMMC 4-bit + Wi-Fi 6 C6 | ⭐ **Target Principal** |
| **Guition JC3248W535** | ESP32-S3 Dual-Core Xtensa @ 240 MHz | 3.5" IPS 320x480 QSPI + FT6336 | Amplificador I2S + SPI SD | 🟢 **Soportado** |

---

## 🛠️ Principios de Arquitectura

1. **Autonomía Operativa:** El sistema arranca y funciona con total independencia; las utilidades locales, el audio, los gráficos y el almacenamiento operan sin requerir conexiones de red obligatorias para el funcionamiento base.
2. **Arquitectura Basada en Eventos:** Los periféricos y buses responden a interrupciones y eventos asíncronos nativos del hardware, evitando bucles innecesarios de sondeo activo para optimizar el rendimiento y consumo de CPU.
