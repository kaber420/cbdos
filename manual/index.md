<div style="text-align: center; padding: 3rem 1.5rem; background: radial-gradient(circle at 50% 0%, rgba(0, 240, 255, 0.18) 0%, rgba(10, 10, 15, 0) 75%); border-radius: 16px; border: 1px solid rgba(0, 240, 255, 0.25); margin-bottom: 2.5rem; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);">
  <h1 style="font-size: 2.8rem; font-weight: 900; margin-bottom: 0.6rem; background: linear-gradient(90deg, #00f0ff 0%, #7000ff 50%, #ff0077 100%); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: -0.5px;">
    CyBerDeck OS
  </h1>
  <p style="font-size: 1.25rem; color: #cbd5e1; max-width: 680px; margin: 0 auto 1.6rem auto; line-height: 1.6;">
    El sistema operativo embebido, desacoplado y <b>offline-first</b> para cyberdecks, consolas portátiles y hardware táctico ESP32.
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

## ⚡ Pilares Tecnológicos

### 🖥️ UI Cyberpunk & 60 FPS
Motor gráfico de alto rendimiento con **LVGL v9.5**, aceleración de hardware DMA2D / PPA en ESP32-P4, animaciones Lottie y widgets táctiles vectoriales.
→ [Ver catálogo de aplicaciones](apps/overview.md)

### 🔲 Multi-Target Desacoplado
Arquitectura agnóstica (`core/`) en C++ estándar y contratos HAL. Corre simultáneamente en **ESP32-P4** (ESP-IDF 5.5 nativo) y **ESP32-S3** (PlatformIO).
→ [Ver placas soportadas](hardware/supported-boards.md)

### 💻 Ecosistema Lua++ (.luapp)
Ejecuta micro-aplicaciones dinámicas desde la tarjeta MicroSD sin necesidad de recompilar ni instalar toolchains. Máquina virtual segura en PSRAM con APIs completas.
→ [Guía rápida de Lua++](developers/luapp.md)

### 📡 Redes Mesh & Modo Sigilo
Comunicaciones ad-hoc fuera de internet vía **ESP-NOW**, túneles de radio, paquetes TLV y tablas de ruteo dinámico con Short IDs.
→ [Ver especificaciones RF](https://github.com/kaber420/CBD-os/tree/main/specs/network)

### ⚡ Flasheador Autónomo de Campo
Convierte tu CyBerDeck en un programador portátil: flashea microcontroladores externos (ESP32-C3, C6) por USB-C o UART directamente desde la MicroSD.
→ [Ver detalles de Flasher](apps/overview.md#herramientas-de-sistema-y-campo)

### 🛡️ Seguridad Física FIDO2 / U2F
Autenticación mediante módulo Kerberos, cifrado simétrico/asimétrico de transmisiones inalámbricas y aislamiento estricto de credenciales.
→ [Especificación de seguridad](https://github.com/kaber420/CBD-os/blob/main/specs/architecture/security_and_encryption_specification.md)

---

## 🎯 Placas y Dispositivos Soportados

| Dispositivo | SoC & Frecuencia | Pantalla & Táctil | Audio & Periféricos | Estado |
| :--- | :--- | :--- | :--- | :--- |
| **Guition JC4880P443C** | ESP32-P4 Dual-Core RISC-V @ 400 MHz | 4.3" IPS 480x800 MIPI-DPI + Goodix GT911 | Codec ES8311 + SDMMC 4-bit + Wi-Fi 6 C6 | ⭐ **Target Principal** |
| **Guition JC3248W535** | ESP32-S3 Dual-Core Xtensa @ 240 MHz | 3.5" IPS 320x480 QSPI + FT6336 | Amplificador I2S + SPI SD | 🟢 **Soportado** |

---

## 🛠️ Filosofía "Zero-Poll & Offline-First"

CBDos está concebido desde su concepción bajo dos directrices de ingeniería innegociables:

1. **Autonomía Total sin Internet:** El 100% de las funciones del sistema (pantalla, táctil, audio, emuladores retro, almacenamiento, terminales y flasheador) operan de forma inmediata al pulsar el botón de encendido, sin depender de Wi-Fi, routers o servidores externos.
2. **Arquitectura 100% Reactiva:** Prohibido el gasto inútil de ciclos de CPU mediante bucles de sondeo (*polling*). Todos los periféricos (USB Hotplug, inserción de MicroSD, paquetes de radio, teclado) responden exclusivamente a interrupciones y eventos asíncronos nativos.
