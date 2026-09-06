# 📱 Aplicaciones Integradas en CBDos

CBDos incluye un conjunto de aplicaciones nativas construidas con **LVGL v9.5**, optimizadas para pantallas táctiles y organizadas en un *Dashboard* fluido y moderno.

---

## 🛠️ Herramientas de Sistema

| Aplicación | Icono / Función | Descripción |
| :--- | :--- | :--- |
| **Terminal** | Consola Interactiva | Shell interactiva con soporte de comandos del sistema (`ls`, `cat`, `free`, `reboot`, etc.) y emulación ANSI. |
| **Serial Terminal** | Depuración Serial | Monitor serie con detección automática de baudrate, envío de datos y control de reinicio / DFU. |
| **SSH Client** | Administración Remota | Cliente SSH nativo para conectarte por terminal a servidores, routers Ubiquiti, MikroTik o Raspberry Pi. |
| **Flasher** | Programador Autónomo | Permite flashear otros chips (ESP32-C3, ESP32-C6) directamente por USB-C o pines UART con auto-bootloader. |
| **File Manager** | Explorador de Archivos | Navega por la MicroSD y la memoria Flash interna SPIFFS; crea carpetas, visualiza archivos y lanza apps. |
| **Text Editor** | Editor de Texto | Edición y visualización rápida de notas y scripts de configuración directamente en la pantalla táctil. |

---

## 🎵 Multimedia y Entretenimiento

| Aplicación | Descripción |
| :--- | :--- |
| **Music Player** | Reproductor de audio de alta fidelidad con decodificación MP3 por software (Helix) y WAV nativo. Soporta carátulas y listas de reproducción. |
| **Voice Recorder** | Grabadora de voz directa a la tarjeta MicroSD utilizando el micrófono integrado y el códec de audio ES8311. |
| **Gallery** | Visor de imágenes táctil con soporte de zoom y previsualización de capturas o gráficos RGB565 / JPEG. |
| **Cartridge Engine** | Motor para cargar juegos y cartuchos retro (PICO-8, binarios optimizados) desde la MicroSD. |

---

## 📶 Red y Configuración

| Aplicación | Descripción |
| :--- | :--- |
| **WiFi Manager** | Escaneo de puntos de acceso, conexión segura WPA2/WPA3 y control de estado bajo demanda. |
| **Mesh Config** | Configuración de la red ad-hoc de malla sobre ESP-NOW, asignación de Short IDs y estado de nodos cercanos. |
| **Wallpaper Picker** | Selector dinámico de fondos de pantalla del sistema con previsualización en miniatura. |
| **Settings / Config** | Ajuste de brillo, calibración táctil, fecha/hora y parámetros de energía (Power Manager). |
