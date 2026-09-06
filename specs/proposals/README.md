# 💡 Propuestas de Aplicaciones e Implementaciones Futuras

Bienvenido al centro de propuestas de software, nuevas aplicaciones y expansiones del ecosistema **CBDos (CyBerDeck OS)**.

Este directorio almacena las especificaciones de diseño, borradores de arquitectura y planes funcionales para aplicaciones que están en fase de propuesta o diseño conceptual antes de su desarrollo en C++ y LVGL 9.5.

---

## 🛠️ Herramientas de Campo, Red y Navegación WISP/ISP / Mesh

| Propuesta | Descripción | Estado | Archivo |
| :--- | :--- | :--- | :--- |
| **Navegador Vectorial Alternet (V-DOM v0.1.2)** | Visualizador de páginas web vectoriales ultraligeras en V-DOM (JSON/TLV) renderizadas con primitivas LVGL 9.5 sobre redes Mesh/ESP-NOW. | 💡 Propuesta / Borrador | [`../drafts/BORRADOR_NAVEGADOR_VECTORIAL_v0.1.2.md`](../drafts/BORRADOR_NAVEGADOR_VECTORIAL_v0.1.2.md) |
| **Lua Sandbox & Scripting Web Seguro** | Entorno de aislamiento de Lua capado (Zero-Trust VM, hooks anti-bucle, cuota de RAM) para dinamismo estilo JavaScript en el Navegador Vectorial. | 💡 Propuesta / Borrador | [`proposal_vector_browser_lua_sandbox.md`](proposal_vector_browser_lua_sandbox.md) |
| **Cliente SSH & Tool de Campo** | Terminal SSH interactiva ANSI + automatización de antenas Ubiquiti / MikroTik con scripts Lua. | 💡 Propuesta / Borrador | [`proposal_ssh_client_and_lua_field_admin.md`](proposal_ssh_client_and_lua_field_admin.md) |
| **Tokenización Semántica & Optimización de Airtime (CBD-Net)** | Compresión semántica por conceptos (1/2/3 bytes por bancos), detección visual con marcador en UI y reducción radical de Airtime de RF con cifrado ChaCha20-Poly1305. | 💡 Propuesta Formal | [`proposal_semantic_concept_tokenization_and_airtime_optimization.md`](proposal_semantic_concept_tokenization_and_airtime_optimization.md) |
| **Pasarela USB ↔ ESP-NOW Gateway** | Módem firmware C3, protocolo serial `0xAA 0x55` y pasarela Python/CBDos. | 🔬 En investigación | [`../network/plan_espnow_usb_bridge.md`](../network/plan_espnow_usb_bridge.md) |
| **Monitor RAW 802.15.4** | Capturador y analizador de paquetes Thread / Zigbee directo en coprocesador C6. | 💡 Propuesta / Borrador | [`../drafts/draft_raw_802_15_4_protocol_design.md`](../drafts/draft_raw_802_15_4_protocol_design.md) |

---

## 🎮 Multimedia, Juegos y Creatividad

| Propuesta | Descripción | Estado | Archivo |
| :--- | :--- | :--- | :--- |
| **Pico-8 / Lua Cartridge Engine** | Motor de juegos y emulador de cartuchos virtuales en Lua con API gráfica dedicada. | 💡 Propuesta / Borrador | [`../drafts/BORRADOR_CARTRIDGE_LUA_PICO8.md`](../drafts/BORRADOR_CARTRIDGE_LUA_PICO8.md) |
| **Historias Interactivas, Dating Sims & Juegos Vectoriales** | Motor de novelas visuales interactivas, dating sims estilo japonés clásico y juegos arcade vectoriales tipo Gamby con Lua + ThorVG/Lottie + Audio MP3/WAV. | 💡 Propuesta Formal | [`proposal_interactive_stories_dating_sims_and_vector_games_lua.md`](proposal_interactive_stories_dating_sims_and_vector_games_lua.md) |
| **Sintetizador de Audio Interactivo** | Generador de formas de onda (Sine, Square, Saw, PWM), polifonía básica y teclado en pantalla. | 💡 Propuesta / Borrador | [`../drafts/plan_synth_app.md`](../drafts/plan_synth_app.md) |

---

## 🤖 Inteligencia Artificial, Asistente de Voz y Comunicación

| Propuesta | Descripción | Estado | Archivo |
| :--- | :--- | :--- | :--- |
| **Asistente de Voz, TTS/STT & Chat Manos Libres** | Subsistema de voz conversacional (TTS local eSpeak/SAM + Streaming Neural, STT Push-to-Talk) para interactuar con agentes de IA (Ollama/DeepSeek/Claude) y lectura de chat en segundo plano. | 💡 Propuesta Formal | [`proposal_voice_assistant_tts_stt_ai_agents.md`](proposal_voice_assistant_tts_stt_ai_agents.md) |

---

## 🔒 Seguridad e Interacción Táctica

| Propuesta | Descripción | Estado | Archivo |
| :--- | :--- | :--- | :--- |
| **DuckyScript Interactivo & BadUSB HID** | Inyector de pulsaciones de teclas BadUSB con consola interactiva por serial/UI y runner Lua. | 🔬 En desarrollo | [`../drafts/ducky_badusb_lua_interactive_spec.md`](../drafts/ducky_badusb_lua_interactive_spec.md) |

---

## 🎒 Hardware, Expansión & Mochilas Modulares

| Propuesta | Descripción | Estado | Archivo |
| :--- | :--- | :--- | :--- |
| **Mochilas Modulares Hot-Plug por NFC** | Detección de módulos acoplables (SX1280 FLRC, SX1262 LoRa, CardKB, GPS) con etiquetas NFC y carga perezosa de drivers en la HAL. | 💡 Propuesta Formal | [`proposal_backpack_nfc_hotplug_hardware_modules.md`](proposal_backpack_nfc_hotplug_hardware_modules.md) |

---

## 📌 Guía para Registrar una Nueva Propuesta de App

Toda nueva propuesta debe seguir la plantilla estándar e incluir:
1. **Visión General y Casos de Uso:** Justificación técnica y público objetivo.
2. **Cumplimiento Arquitectónico:** Agnosticismo de `core/`, separación HAL/BSP y principio Offline-First.
3. **Contratos de C++ e Interfaces:** Definición de clases abstractas en `core/hal/` y servicios.
4. **Integración con Scripting (Lua):** Definición de funciones expuestas en `LuaBridge`.
5. **UI Mockup / Diseño LVGL 9.5:** Descripción o boceto del layout gráfico.
6. **Plan por Fases:** Desglose modular de tareas.
