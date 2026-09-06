# Roadmap de Portado: Herramientas Tácticas y Cyberdeck para CBDos

Este documento define la especificación técnica, arquitectura agnóstica y hoja de ruta para la incorporación en **CBDos** de las herramientas y capacidades seleccionadas provenientes de **Poseidon** y **Bruce**.

---

## 📌 Principios de Diseño en CBDos
1. **Agnosticismo de `core/` (C++ y LVGL 9.5):** La lógica de decodificación de protocolos, parseo de archivos y gestión de estados se implementa en `core/`, mientras que el acceso físico a pines/transceptores (CC1101, nRF24, IR, USB) se abstrae a través de interfaces HAL en `bsp/`.
2. **Offline-First:** Ninguna herramienta depende de conectividad externa a internet. Todas las bases de datos de señales, payloads y configuraciones residen en la MicroSD.
3. **Aprovechamiento de Pantalla Grande y Táctil:** Adaptación de las interfaces originales (pensadas para 240x135) a pantallas de alta resolución (480x800 en ESP32-P4 y 320x480 en ESP32-S3) con widgets ricos en LVGL 9.5 (gráficos tipo cascada/waterfall, listas táctiles, medidores tipo VU).

---

## 🛠️ Módulos y Funcionalidades Aprobadas para Implementación

### 1. 🔑 KERBEROS: Token de Seguridad FIDO2 / U2F Hardware (Passkeys)
* **Descripción:** Transforma el Cyberdeck en un autenticador físico USB (compatible con FIDO2/CTAP2 y U2F) para inicio de sesión sin contraseñas (Passkeys) en Google, GitHub, Microsoft, etc.
* **Arquitectura:**
  * Implementación nativa CTAP2/CBOR/ECDSA (NIST P-256) sobre la pila USB-Device (TinyUSB / USB-OTG).
  * **Interacción en UI:** Muestra en pantalla el nombre del sitio web (*Relying Party*) solicitante, permite introducir PIN en teclado táctil y requiere pulsar físicamente "Aprobar" en la pantalla táctil antes de generar la firma criptográfica.
* **Dependencia Hardware:** USB-OTG nativo (ESP32-P4 / ESP32-S3).

---

### 2. 🚨 Defensive Monitor (IDS Inalámbrico Local / Anomaly Detector)
* **Descripción:** Centinela pasivo en segundo plano que vigila el espectro electromagnético local para alertar de ataques o perturbaciones activas.
* **Detección de Patrones:**
  * *Deauth Flood / Broadcast Deauth:* Detección de tramas de desautenticación masivas.
  * *Evil Twin & Rogue AP:* Detección de puntos de acceso clonados con MAC o características anómalas.
  * *Beacon Spam & Karma:* Alertas de saturación de balizas falsas.
  * *BLE Spoofing & Flood:* Detección de ráfagas anómalas de anuncios Bluetooth.
* **Integración:** Icono de estado y advertencias visuales en la `HeaderBar` + alertas sonoras opcionales mediante el DAC de audio ES8311.

---

### 3. 🚁 Drone Remote ID Decoder (ASTM F3411-22a)
* **Descripción:** Escáner y decodificador pasivo de emisiones abiertas de identificación remota de drones comerciales transmitidas vía Bluetooth LE y WiFi Beacon.
* **Datos Mostrados en Pantalla:**
  * ID / Número de serie del dron y fabricante.
  * Altitud, velocidad de vuelo, rumbo y coordenadas GPS del dron.
  * Coordenadas GPS del operador/piloto y punto de despegue (*Home Point*).
  * Distancia estimada y tiempo de actualización.

---

### 4. 📍 Tracker Finder & Detector Anti-Stalking
* **Descripción:** Herramienta de detección y localización de balizas de seguimiento no deseadas (Apple AirTag / Find My, Samsung SmartTag, Tile, Chipolo).
* **Modos de Operación:**
  * *Scanner Pasivo:* Identifica dispositivos sospechosos que se desplazan de forma continuada cerca del usuario.
  * *Modo "Geiger":* Medidor táctil de proximidad en tiempo real basado en la variación del RSSI con retroalimentación acústica/visual conforme te acercas a la baliza.

---

### 5. 📻 Sub-GHz Flipper `.sub` Player & Raw Recorder (CC1101 SPI)
* **Descripción:** Motor para reproducir, grabar y decodificar señales de radioenlace Sub-GHz (300–928 MHz, incluyendo 315, 433.92, 868 y 915 MHz) usando el transceptor SPI CC1101.
* **Características:**
  * Compatibilidad directa con archivos `.sub` de Flipper Zero leídos desde `/sdcard/subghz/`.
  * Soporte de protocolos de código fijo y aprendizaje (Princeton, CAME, NICE, Linear, Holtek).
  * Grabación y reproducción de tramas RAW.

---

### 6. 📊 Analizador de Espectro RF / Waterfall Display (Sub-GHz y 2.4 GHz)
* **Descripción:** Representación visual del espectro electromagnético en tiempo real mediante barridos continuos de frecuencia.
* **Modos de Visualización:**
  * *Waterfall (Gráfico de Cascada):* Mapeo térmico de frecuencia vs. tiempo.
  * *Osciloscopio / Bar Graph:* Nivel de señal instantáneo (RSSI/dBm) con retención de picos (*Peak Hold*).
* **Hardware Soportado:** CC1101 (Sub-GHz) y nRF24L01+ (2.4 GHz).

---

### 7. 🖱️ Promiscuous 2.4 GHz & MouseJack (nRF24L01+)
* **Descripción:** Escaneo promiscuo en la banda de 2.4 GHz para auditar enlaces inalámbricos propietarios de periféricos (teclados y ratones inalámbricos sin cifrado estilo Logitech/Microsoft legacy).
* **Capacidades:**
  * Decodificación de direcciones MAC / payloads de paquetes ESB (*Enhanced ShockBurst*).
  * Detección de vulnerabilidades de inyección de pulsaciones (MouseJack) sobre periféricos vulnerables propios.

---

### 8. 📺 Universal IR Remote & Multi-Profile Cloner (Infrarrojos)
* **Descripción:** Sistema de control y clonación de mandos a distancia por infrarrojos mediante emisor y receptor IR.
* **Funcionalidades:**
  * Base de datos organizada en MicroSD (`/sdcard/ir/`) por marcas y categorías (TV, proyectores, climatización, sistemas de sonido).
  * Decodificador y analizador de protocolos (NEC, RC5, RC6, Sony SIRC, Samsung).
  * Función de apagado rápido / prueba multi-código para mantenimiento de pantallas.

---

### 9. 🐾 Cyber-Pet / Gotchi Asistente Autónomo (Estilo Argus)
* **Descripción:** Mascota virtual y asistente centinela reactivo que reside en el sistema.
* **Integración en CBDos:**
  * Motor de sprites de estados de ánimo dinámicos (animaciones basadas en el estado del Cyberdeck).
  * Reacciona a eventos en tiempo real: tráfico LoRa / MeshCore recibido, conexiones Wi-Fi/BLE, estado de la batería, carga del sistema y reproducción de audio.
  * Posibilidad de visualización como widget en la pantalla principal o integrado en la barra de estado.

---

## 📊 Matriz de Dependencias Hardware e Interfaces HAL

| Módulo | Hardware Requerido | Interfaz HAL Sugerida | Ubicación en CBDos |
| :--- | :--- | :--- | :--- |
| **KERBEROS FIDO2** | USB OTG nativo | `IUsbHidDevice` | `core/src/apps/kerberos/` |
| **Defensive Monitor** | Wi-Fi / BLE interno | `INetworkAdapter` / `IBleAdapter` | `core/src/services/defensive/` |
| **Drone Remote ID** | BLE / Wi-Fi interno | `IBleAdapter` | `core/src/apps/drone_id/` |
| **Tracker Finder** | BLE interno | `IBleAdapter` | `core/src/apps/tracker_finder/` |
| **Sub-GHz `.sub` Engine** | CC1101 (SPI) | `IRadioSubGhz` | `core/src/apps/subghz/` |
| **RF Spectrum Waterfall** | CC1101 / nRF24L01 | `IRadioSpectrum` | `core/src/apps/spectrum/` |
| **2.4 GHz MouseJack** | nRF24L01+ (SPI) | `IRadioNrf24` | `core/src/apps/rf24/` |
| **Universal IR Remote** | LED Emisor + Receptor IR | `IIrTransceiver` | `core/src/apps/ir_remote/` |
| **Cyber-Pet Gotchi** | CPU + LVGL 9.5 + Audio | `IAudioSink` / `SystemEvents` | `core/src/apps/cyberpet/` |
