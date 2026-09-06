# 🔬 Especificaciones Técnicas e Ingeniería de CBDos (CyBerDeck OS)

Bienvenido al repositorio de especificaciones de ingeniería, notas de arquitectura interna, bitácoras de investigación y protocolos de bajo nivel de **CBDos**.

> 🌐 **¿Buscas el manual de usuario o la guía de primeros pasos?**  
> Visita el portal oficial de documentación en **[kaber420.github.io/cbdos](https://kaber420.github.io/cbdos/)** (mantenido en la carpeta `docs/`).

---

## 🗺️ Mapa de Navegación de Especificaciones

```
specs/
├── 📚 api/               -> Referencia técnica de APIs internas del SDK y Guías C++
├── 🏛️ architecture/      -> Filosofía del Core agnóstico, HAL y patrones arquitectónicos
├── 🔌 hardware/          -> Mapas de pines GPIO, conectores JP1 y periféricos
├── 💾 storage/           -> Almacenamiento SPIFFS, MicroSD y persistencia NVS
├── 🌐 network/           -> Protocolos RF (ESP-NOW, TLV, Pseudo-ARP) y coprocesadores
├── 💡 proposals/         -> Propuestas técnicas e implementaciones futuras
└── 📝 drafts/            -> Borradores de trabajo, bitácoras e ideas de evolución
```

---

### 1. 📚 [SDK y Guías de Desarrollo (`specs/api/`)](api/)
* **[Guía para Desarrollar una App en C++](api/how_to_create_an_app.md):** Tutorial paso a paso con código de ejemplo para crear vistas nativas en LVGL 9.5.
* **[Especificación de Lua++ y Formato `.luapp`](api/luapp_specification.md):** Guía de desarrollo de micro-aplicaciones dinámicas en Lua++ sin necesidad de compilar.
* **[Referencia de APIs del Sistema (SDK)](api/core_apis_reference.md):** Documentación completa de `cbdos::system`, `storage`, `audio`, `uart`, `flasher`, `display`, `network` y `DefaultTheme`.

---

### 2. 🏛️ [Arquitectura y HAL (`docs/architecture/`)](architecture/)
* **[Arquitectura Agnóstica y HAL](architecture/hal_and_core_architecture.md):** Filosofía de desacoplamiento, Ley de Pureza de `core/`, contratos C++ y tabla maestra de módulos HAL.
* **[Ecosistema de Lua Apps y Runtime Sandboxed](architecture/lua_app_ecosystem_and_runtime_architecture.md):** Especificación del runtime de Lua, aislamiento en PSRAM, APIs unificadas (`ui`, `hid`, `usb`, `radio`, `sys`) y modelo de extensibilidad sin compilar.
* **[Backpack Manager y Auto-Detección NFC](architecture/backpack_manager_and_dynamic_gpio_nfc_spec.md):** Arquitectura de mochilas modulares de hardware, auto-identificación por NFC y reconfiguración dinámica de pines GPIO/I2C/SPI en JP1.
* **[Estación Base y Router Multi-Radio Modular](architecture/multi_radio_hub_router_design.md):** Arquitectura de Hub USB táctico con múltiples dongles ESP32-C3 (ESP-NOW, LoRa 915 MHz y SX1280 FLRC) con infografías de hardware.
* **[Modularización de LuaBridge](architecture/modular_lua_bridge_architecture.md):** Arquitectura modular de bindings de Lua dividida por dominios (Kernel, Audio, GFX, FS, Net, Mesh).
* **[Diagnóstico y Solución de GFX 2D en P4](architecture/lua_gfx_p4_mipi_dpi_flicker_diagnosis.md):** Análisis de parpadeo DMA ping-pong y sincronización de caché L2 en displays MIPI-DPI.
* **[Expansión USB Host, Flasher y JTAG](architecture/usb_host_flasher_and_jtag_subsystem.md):** Especificación técnica para flasheo USB-C directo, dongles/módems de radio y depuración JTAG/SWD.
* **[Seguridad y Cifrado](architecture/security_and_encryption_specification.md):** Especificación de cifrado de paquetes de radio y autenticación de nodos.
* **[Transmisión Sigilosa y Métricas de Airtime](architecture/stealth_transmission_and_airtime_metrics.md):** Técnicas de evasión de espectro y control de tiempo de emisión.

---

### 3. 🔌 [Hardware y Pinouts (`docs/hardware/`)](hardware/)
* **[Hito Técnico: Programador de Campo Autónomo USB-C](hardware/usb_c_field_flasher_milestone.md):** Evidencia en hardware real del flasheo autónomo de ESP32-C3 desde el ESP32-P4 por cable Tipo-C con Auto-Bootloader por hardware.
* **[Mapa Completo de Pines y Puertos](hardware/pinouts_and_ports.md):** Pinouts GPIO de placas ESP32-P4 (JC4880) y ESP32-S3 (JC3248), pines del conector JP1, buses I2C/I2S/SPI y tabla de pines UART/Flasheador.
* **[Resumen de Hardware y Backups](hardware/hardware_summary.md):** Especificaciones de memoria Flash, PSRAM y periféricos soportados.

---

### 4. 💾 [Almacenamiento y Persistencia (`docs/storage/`)](storage/)
* **[Especificación SPIFFS y MsgPack](storage/spiffs_and_msgpack_spec.md):** Almacenamiento Flash interno estructurado con serialización MessagePack.
* **[Persistencia NVS y FastBoot](storage/nvs_persistencia_fastboot.md):** Almacenamiento de preferencias del usuario y arranque rápido.
* **[Listas de Reproducción y Almacenamiento Portable](storage/playlists_and_portable_storage.md):** Gestión de música y archivos multimedia en MicroSD.

---

### 5. 🌐 [Red, Mesh y Conectividad (`docs/network/`)](network/)
* **[Puente USB-Serial ↔ ESP-NOW (Dongle Gateway)](network/plan_espnow_usb_bridge.md):** Especificación del firmware del módem C3, protocolo de enmarcado serial `0xAA 0x55` y pasarela Python.
* **[Bitácora de Validación del Gateway-Router y Pseudo-ARP](network/bitacora_validacion_gateway_router_y_pseudo_arp.md):** Resultados de pruebas reales en vivo de navegación TLVGL sobre ESP-NOW.
* **[Especificación de Direccionamiento IPv4 Mesh y Pseudo-ARP](network/especificacion_direccionamiento_ipv4_mesh_y_pseudo_arp.md):** Jerarquía `10.x.y.z`, Short IDs y tabla Pseudo-ARP en SQLite3.
* **[Análisis Exhaustivo de Modelos de Identidad Mesh](network/analisis_exhaustivo_modelos_identidad_y_direccionamiento_mesh.md):** Estudio matemático de colisiones y escalabilidad de direccionamiento.
* **[Arquitectura de Pool Dinámico y Traducción UUID](network/arquitectura_pool_dinamico_short_id_y_traduccion_uuid.md):** Modelo de Pseudo-NAT asimétrico (Aire 3B $\leftrightarrow$ WAN 4B).
* **[Pruebas de Fragmentación y MicroChunking Mesh](network/pruebas_fragmentacion_microchunking_mesh.md):** Protocolo de micro-chunks de 2B para fragmentación L2 en ESP-NOW.
* **[Suite de Gestión y SaaS para Torres y Gateways](network/especificacion_suite_gestion_saas_torres_y_gateways.md):** Arquitectura del panel web de administración, ACL del Proxy, cuotas y editor visual.
* **[Guía de Coprocesador C6 Hosted](network/esp32_p4_c6_hosted_wifi.md):** Arquitectura de red WiFi 6 / Bluetooth con ESP32-C6 vía SDIO.
* **[Investigación de Firmware SDIO](network/investigacion_firmware_c6_sdio.md):** Análisis del firmware esclavo embebido.

---

### 6. 📝 [Borradores y Planes de Fase (`docs/drafts/`)](drafts/)
* **[Borrador de Protocolo 802.15.4 Crudo](drafts/draft_raw_802_15_4_protocol_design.md):** Diseño de comunicación Thread / Zigbee directo en el coprocesador C6.
* **[Diseño del Unified Radio Manager](drafts/draft_unified_radio_manager_design.md):** Gestor centralizado de múltiples transceptores de radio.
* **[Cliente SSH y Administración de Campo](drafts/draft_ssh_client_and_field_admin.md):** Especificación para terminal SSH embebida en CBDos.
* **[Borrador de Cartuchos Pico8 / Lua](drafts/BORRADOR_CARTRIDGE_LUA_PICO8.md):** Motor de juegos y emulación de cartuchos virtuales.
* **[Plan de Aplicación Sintetizador](drafts/plan_synth_app.md):** Generador de audio y teclado musical interactivo.

---

### 7. 💡 [Propuestas de Apps e Implementaciones Futuras (`docs/proposals/`)](proposals/)
* **[Índice Central de Propuestas](proposals/README.md):** Portal general de especificaciones de nuevas aplicaciones y herramientas proyectadas.
* **[Propuesta: Navegador Vectorial Alternet (v0.1.2)](drafts/BORRADOR_NAVEGADOR_VECTORIAL_v0.1.2.md):** Modelo de Documento Vectorial (V-DOM) para renderizado ultraligero de portales mesh con primitivas LVGL 9.5.
* **[Propuesta: Lua Sandbox & Scripting Web Seguro](proposals/proposal_vector_browser_lua_sandbox.md):** Especificación de seguridad (Zero-Trust VM, hooks de CPU anti-bucle, cuota de RAM) para scripting dinámico estilo JavaScript en el Navegador Vectorial.
* **[Propuesta: App Cliente SSH y Herramienta de Campo](proposals/proposal_ssh_client_and_lua_field_admin.md):** Especificación completa de terminal SSH interactiva ANSI, automatización en Lua para Ubiquiti / MikroTik y perfiles en MicroSD.

