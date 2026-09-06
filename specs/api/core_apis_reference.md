# 📚 Manual de APIs del Sistema (CBDos SDK & Core APIs)

Este documento es la **guía oficial de referencia para desarrolladores**. Aquí se describen todas las APIs, servicios y abstracciones que ofrece el núcleo de **CBDos** para crear aplicaciones, juegos y herramientas de sistema.

---

## 🧭 Índice de APIs
1. [cbdos::system - Información y Control del Sistema](#1-cbdossystem)
2. [cbdos::storage - Sistema de Archivos (SD y Flash)](#2-cbdosstorage)
3. [cbdos::audio - Reproducción de Sonido y Radio](#3-cbdosaudio)
4. [cbdos::uart - Comunicación Serial](#4-cbdosuart)
5. [cbdos::flasher - Flasheo Universal de Microcontroladores](#5-cbdosflasher)
6. [cbdos::display - Capacidades de Pantalla y Brillo](#6-cbdosdisplay)
7. [cbdos::network - Conectividad WiFi y Tiempo](#7-cbdosnetwork)
8. [cbdos::theme & DefaultTheme - Sistema de Estilos LVGL](#8-cbdostheme--defaulttheme)
9. [cbdos::lua::LuappManager - Gestor de Apps Lua (.luapp)](#9-cbdoslualuappmanager-y-luappview)
10. [cbdos::hid - Emulación USB HID (Teclado, Ratón y LEDs)](#10-cbdoshid---emulación-usb-hid-teclado-ratón-y-leds)
11. [cbdos::ducky - Intérprete DuckyScript para BadUSB](#11-cbdosducky---intérprete-duckyscript-para-badusb)
12. [Disparadores Tácticos Fuera de Banda (Radio + HID + Lua)](#12-disparadores-tácticos-fuera-de-banda-radio--hid--lua)

---

## 1. `cbdos::system`
Header: `#include "cbdos/system.hpp"`

Permite consultar métricas de rendimiento, memoria, temperatura y control del microcontrolador.

```cpp
// Obtener tiempos (milisegundos y microsegundos)
uint32_t ms = cbdos::system::getTimeMs();
uint64_t us = cbdos::system::getTimeUs();

// Pausas y Yield
cbdos::system::sleepMs(50);
cbdos::system::yieldTask();

// Métricas de Memoria RAM (Internal Heap y PSRAM)
size_t freeHeap   = cbdos::system::getFreeHeap();
size_t totalHeap  = cbdos::system::getTotalHeap();
size_t freePsram  = cbdos::system::getFreePsram();
size_t totalPsram = cbdos::system::getTotalPsram();

// Temperatura del SoC en °C
float temp = cbdos::system::getCpuTemperature();

// Logs estructurados del sistema
cbdos::system::log(cbdos::system::LogLevel::Info, "MiApp", "Iniciando proceso con valor: %d", 42);

// Reinicio de hardware
cbdos::system::restart();
```

---

## 2. `cbdos::storage`
Header: `#include "cbdos/storage.hpp"`

Abstracción transparente para trabajar con la memoria Flash interna (`/spiffs`) y la tarjeta MicroSD (`/sdcard`).

```cpp
// Comprobar estado de montaje
bool sdOk    = cbdos::storage::isSdMounted();
bool flashOk = cbdos::storage::isFlashMounted();

// Listar archivos de un directorio
std::vector<cbdos::storage::FileEntry> files = cbdos::storage::listDir("/sdcard/music");
for (const auto& file : files) {
    printf("Nombre: %s | Tamaño: %u bytes | Es Dir: %s\n", 
           file.name.c_str(), (unsigned)file.size, file.isDirectory ? "SI" : "NO");
}

// Comprobar existencia
if (cbdos::storage::fileExists("/sdcard/config.json")) { ... }

// Leer archivo completo a std::string
std::string data = cbdos::storage::readFile("/sdcard/nota.txt");

// Escribir archivo completo
bool ok = cbdos::storage::writeFile("/sdcard/logs/test.txt", "Hola mundo desde CBDos!");

// Crear carpetas
cbdos::storage::makeDir("/sdcard/mis_datos");

// Borrar y Copiar
cbdos::storage::deleteFile("/sdcard/archivo_viejo.txt");
cbdos::storage::copyFile("/spiffs/default.cfg", "/sdcard/config.cfg");

// Estadísticas de almacenamiento
cbdos::storage::StorageStats sdStats = cbdos::storage::getSdCardStats();
printf("SD Total: %llu MB | Libre: %llu MB\n", sdStats.totalBytes / (1024*1024), sdStats.freeBytes / (1024*1024));
```

---

## 3. `cbdos::audio`
Header: `#include "cbdos/audio.hpp"`

Control del subsistema de sonido, códec I2S y volumen general.

```cpp
// Control de volumen general (0 a 100)
cbdos::audio::setVolume(80);
int vol = cbdos::audio::getVolume();

// Reproducción de audio y tonos
cbdos::audio::playFile("/sdcard/musica/track.mp3");
cbdos::audio::playTone(1000, 150); // Beep 1 kHz, 150 ms
cbdos::audio::stop();

// Grabación de Audio WAV (Micrófono ES8311 en ESP32-P4)
cbdos::audio::RecordConfig cfg;
cfg.sampleRate = 16000; // 16 kHz recomendado para voz o 44100 Hz
cfg.channels = 1;        // Mono
cfg.bitsPerSample = 16;  // 16-bit PCM
cfg.micGainDb = 24;      // Ganancia de entrada analógica (0 a 30 dB)

// Iniciar y detener grabación streaming hacia MicroSD
cbdos::audio::recordStart("/sdcard/recordings/memo.wav", cfg);
bool recording = cbdos::audio::isRecording();
float peakLevel = cbdos::audio::getMicPeakLevel(); // 0.0f a 1.0f para animar vúmetros
cbdos::audio::recordStop();

// Lectura de streaming crudo PCM desde el micrófono
size_t bytesRead = cbdos::audio::readAudio(buffer, sizeof(buffer), 100);
```

---

## 4. `cbdos::uart`
Header: `#include "cbdos/uart.hpp"`

Control bidireccional de puertos serie de hardware (UART) para consolas, comunicación con sensores o pentesting de routers.

```cpp
// Inicializar UART en pines específicos y velocidad deseada
int txPin = cbdos::uart::getDefaultTxPin();
int rxPin = cbdos::uart::getDefaultRxPin();
cbdos::uart::init(txPin, rxPin, 115200);

// Comprobar bytes disponibles
size_t avail = cbdos::uart::available();
if (avail > 0) {
    // Leer como string
    std::string text = cbdos::uart::readString(avail);
    
    // O leer a buffer binario
    uint8_t buf[128];
    size_t bytesRead = cbdos::uart::read(buf, sizeof(buf));
}

// Enviar comandos / datos
cbdos::uart::writeString("help\r\n");
cbdos::uart::write((const uint8_t*)"\x03", 1); // Enviar Ctrl+C

// Cambiar velocidad al vuelo
cbdos::uart::setBaudrate(9600);

// Cerrar puerto
cbdos::uart::deinit();
```

---

## 5. `cbdos::flasher`
Header: `#include "cbdos/flasher.hpp"`

Servicio autónomo para quemar firmware a microcontroladores ESP conectados por UART.

```cpp
// Obtener presets disponibles para la placa actual
const auto& presets = cbdos::flasher::getPresets();

// Configurar flasheo personalizado
cbdos::flasher::FlasherConfig cfg;
cfg.txPin = 32;
cfg.rxPin = 28;
cfg.bootPin = 34;
cfg.rstPin = 54;
cfg.baudRate = 115200;
cfg.flashOffset = 0x0;
cfg.binPath = "/sdcard/nuevo_firmware.bin";

// Iniciar flasheo no bloqueante con callback de progreso
cbdos::flasher::startFlash(cfg, [](cbdos::flasher::FlasherStatus status, int pct, const char* msg) {
    printf("[Progreso: %d%%] Estado: %s\n", pct, msg);
});
```

---

## 6. `cbdos::display`
Header: `#include "cbdos/display.hpp"`

Consulta de resolución y control de brillo de pantalla.

```cpp
// Obtener capacidades del display actual
cbdos::display::DisplayCaps caps = cbdos::display::getCapabilities();
printf("Resolución: %dx%d @ %d FPS | Color: %s\n", 
       caps.width, caps.height, caps.targetFps, caps.colorFormat.c_str());

// Ajustar brillo de pantalla (0 a 100%)
cbdos::display::setBrightness(90);
int brightness = cbdos::display::getBrightness();
```

---

## 7. `cbdos::network`
Header: `#include "cbdos/network.hpp"`

Estado y control de conectividad WiFi y sincronización horaria.

```cpp
// Estado de conexión
cbdos::network::NetworkStatus status = cbdos::network::getStatus();
if (status == cbdos::network::NetworkStatus::Connected) {
    std::string ip = cbdos::network::getIpAddress();
    int rssi = cbdos::network::getRssi();
    printf("Conectado con IP: %s (Señal: %d dBm)\n", ip.c_str(), rssi);
}

// Conectar a red WiFi bajo demanda
cbdos::network::connectWifi("MiWiFi_SSID", "MiPassword123");
```

---

## 8. `DefaultTheme` y Estilos de Interfaz (LVGL 9.5)
Header: `#include "../themes/DefaultTheme.h"`

Helpers visuales predefinidos para mantener una estética consistente y atractiva en todo el sistema operativo.

```cpp
// Crear una tarjeta con elevación (sombra y borde sutil)
lv_obj_t* card = lv_obj_create(parent);
lv_obj_set_size(card, 200, 100);
DefaultTheme::applyRaisedCard(card, 12); // Radio de 12px

// Crear un contenedor hundido (ideal para paneles de datos o terminales)
lv_obj_t* sunken = lv_obj_create(parent);
DefaultTheme::applySunkenCard(sunken, 8);

// Aplicar estilo de botón estándar
lv_obj_t* btn = lv_button_create(parent);
DefaultTheme::applyButton(btn, 10);

// Colores oficiales del tema
lv_color_t bg       = DefaultTheme::getBgColor();          // Fondo oscuro (#161821)
lv_color_t primary  = DefaultTheme::getPrimaryAccent();     // Verde azulado (#00F5D4)
lv_color_t second   = DefaultTheme::getSecondaryAccent();   // Violeta Cyberpunk (#9D4EDD)
lv_color_t text     = DefaultTheme::getTextColor();         // Blanco suave (#F1F5F9)
lv_color_t muted    = DefaultTheme::getMutedTextColor();    // Gris (#94A3B8)
```

---

## 9. `cbdos::lua::LuappManager` y `LuappView`
Header: `#include "lua/LuappManager.hpp"` y `#include "ui/views/LuappView.hpp"`

Servicio de escaneo, parseo de metadatos y lanzamiento aislado de aplicaciones portables `.luapp`.

```cpp
// Escanear carpeta de aplicaciones en la SD
auto& mgr = cbdos::lua::LuappManager::getInstance();
mgr.scanApps("/sdcard/apps");

// Iterar aplicaciones descubiertas
for (const auto& app : mgr.getDiscoveredApps()) {
    printf("App: %s | Icono: %s | Color: 0x%06X | Ruta: %s\n",
           app.name.c_str(), app.icon.c_str(), (unsigned)app.accentColor, app.filePath.c_str());
}

// Instanciar y lanzar la vista de una App Lua
auto view = std::make_shared<cbdos::ui::LuappView>("/sdcard/apps/winamp_mini.luapp", "Winamp Retro", LV_SYMBOL_AUDIO);
cbdos::ui::UIManager::getInstance().pushView(view);
```

---

## 10. `cbdos::hid` - Emulación USB HID (Teclado, Ratón y LEDs)
Header: `#include "cbdos/hid.hpp"`

Permite emular periféricos USB HID interactivos (BadUSB interactivo, StreamDeck, MacroPad, Mouse/Touchpad) y monitorizar el estado de los LEDs del host (Caps Lock, Num Lock, Scroll Lock). Por seguridad y para evitar interferir con la interfaz CDC/Serial de flasheo en el arranque, la pila USB HID inicia **desactivada por defecto** y debe activarse bajo demanda.

```cpp
// Control dinámico bajo demanda del canal USB HID (Teclado/Ratón)
cbdos::hid::enable();  // Inicializa TinyUSB/USB HID y habilita D+/D-
cbdos::hid::disable(); // Desconecta D+/D- y libera la pila USB hardware
bool active = cbdos::hid::isEnabled();

// Comprobar estado de conexión USB HID
if (cbdos::hid::isEnabled() && cbdos::hid::isConnected() && cbdos::hid::isReady()) {
    // Pulsación simple de tecla
    cbdos::hid::sendKeyPress(cbdos::hid::KEY_ENTER);

    // Combinación con modificadores (ej. GUI + R para Windows Run)
    cbdos::hid::sendCombo(cbdos::hid::KEY_MOD_LGUI, cbdos::hid::KEY_R);

    // Enviar cadena de texto tipeada
    cbdos::hid::sendString("notepad.exe\n", 15); // con retardo de 15ms por tecla

    // Movimiento y clic de ratón
    cbdos::hid::mouseMove(100, -50, 0); // deltaX, deltaY, scroll
    cbdos::hid::mouseClick(cbdos::hid::MOUSE_BTN_LEFT);
}

// Consultar estado de LEDs del host
uint8_t leds = cbdos::hid::getLedState();
bool capsOn = (leds & cbdos::hid::LED_CAPSLOCK) != 0;

// Espera interactiva de evento de LED con timeout
bool eventReceived = cbdos::hid::waitForLedEvent(cbdos::hid::LED_CAPSLOCK, 5000);
```

### Bindings en Lua (`hid.*` o `cbdos.hid.*`):
```lua
-- Activar USB HID bajo demanda desde script
hid.enable() -- o hid.start()

if hid.is_connected() then
    hid.press_gui("r")
    hid.delay(200)
    hid.type("cmd.exe\n", 10)
    
    -- Espera a que el usuario o el script del host cambie NumLock
    local ok = hid.wait_led_event(hid.LED_NUMLOCK, 5000)
    if ok then
        hid.type("echo Host respondio!\n")
    end
end

-- Desactivar USB HID al terminar trabajo
hid.disable() -- o hid.stop()
```

---

## 11. `cbdos::ducky` - Intérprete DuckyScript para BadUSB
Header: `#include "cbdos/ducky.hpp"`

Permite cargar y ejecutar scripts estándar de BadUSB (ficheros `.dd` o `.txt` con sintaxis DuckyScript).

```cpp
// Cargar y ejecutar script DuckyScript
auto& runner = cbdos::ducky::DuckyInterpreter::getInstance();
if (runner.loadFile("/sdcard/payloads/recon.dd")) {
    runner.start();
}

// En el bucle de la aplicación / vista:
while (runner.isRunning()) {
    runner.step(); // Ejecución paso a paso no bloqueante
    cbdos::system::sleepMs(10);
}
```

### Bindings en Lua (`ducky.*` o `cbdos.ducky.*`):
```lua
ducky.set_default_delay(50)
if ducky.load_file("/sdcard/payloads/payload.dd") then
    ducky.run()
    while ducky.is_running() do
        sys.sleep(50)
    end
end
```

---

## 12. Disparadores Tácticos Fuera de Banda (Radio + HID + Lua)
Referencia detallada de arquitectura: [`oob_tactical_radio_hid_trigger_spec.md`](file:///home/kaber420/Documentos/proyectos/cbdos/docs/architecture/oob_tactical_radio_hid_trigger_spec.md)

CBDos permite enlazar eventos de radio no-IP (**ESP-NOW LR, LoRa, FLRC, Wi-Fi HaLow, Zigbee**) con inyecciones inmediatas por USB HID físico:

```lua
-- Receptor de disparos tácticos por radio
radio.onReceive(function(sender, payload, rssi)
    if payload == "TRIGGER_LOCK" then
        hid.combo(hid.MOD_GUI, hid.KEY_L)
    elseif payload == "RUN_PAYLOAD" then
        ducky.runFile("/sdcard/payloads/recon.dd")
    end
end)
```

---

## 13. `cbdos::tts` - Text-to-Speech (TTS) Offline en Español
Header: `#include "cbdos/tts.hpp"`

Servicio de síntesis de voz natural y texto a voz 100% offline basado en **SVOX Pico TTS**. Diseñado para accesibilidad en el Cyberdeck, asistentes de voz locales, notificaciones audibles en campo y lectura de terminales o payloads.

### Modelos de Voz Requeridos
El motor carga los diccionarios lingüísticos y fonéticos dinámicamente desde la tarjeta MicroSD para no saturar la memoria Flash:
* `/sdcard/tts/es/es-ES_ta.bin` (Diccionario de texto y análisis morfológico en español, ~250 KB).
* `/sdcard/tts/es/es-ES_zl0_sg.bin` (Modelo acústico de síntesis de voz en español, ~591 KB).

### Ejemplo en C++ (Aplicaciones y Vistas)
```cpp
#include "cbdos/tts.hpp"

// Obtener la instancia del servicio TTS
auto* tts = cbdos::tts::getTextToSpeechService();
if (tts) {
    // 1. Opcional: Configurar velocidad y tono (100 = velocidad/tono normal)
    tts->setSpeed(105); // 105% velocidad
    tts->setPitch(100); // 100% tono

    // 2. Encolar frase para sintetizar y reproducir
    tts->speak("Hola, bienvenido a CBDos");

    // 3. Control de reproducción
    if (tts->isSpeaking()) {
        // tts->pause();  // Pausar
        // tts->resume(); // Reanudar
        // tts->stop();   // Detener y limpiar cola
    }

    // 4. Liberación de memoria (Opcional):
    // Si la aplicación va a cerrar o se necesita toda la PSRAM para un emulador,
    // se puede descargar el motor y liberar los ~1.95 MB de memoria de trabajo:
    // tts->shutdown();
}
```

### Prueba Rápida desde Consola Serial (`/dev/ttyACM0`)
El firmware incluye un comando interactivo en caliente en la CLI serial para validar voz sin recompilar:
```bash
tts <texto a pronunciar>
```
*Ejemplo:*
```text
tts Hola mundo, saludos desde CBDos
```
*Telemetría esperada en consola:*
```text
[SERIAL_CLI_IN] tts Hola mundo, saludos desde CBDos
[SERIAL_CLI_OUT] 🗣️ Sintetizando voz: "Hola mundo, saludos desde CBDos"...
[SERIAL_CLI_OUT] ✅ Texto encolado en PicoTTS.
Síntesis en memoria completada: 34560 muestras mono (16 kHz)
Iniciando reproduccion DMA continua: 381024 bytes estéreo (44.1 kHz)
```


