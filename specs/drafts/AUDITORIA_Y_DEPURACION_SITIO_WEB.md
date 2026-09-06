# Auditoría y Depuración de Contenido: Sitio Web Oficial de CBDos (`site/`)

> **Fecha:** 6 de Septiembre de 2026  
> **Estado:** Borrador de correcciones técnicas requeridas  
> **Objetivo:** Eliminar información falsa, exagerada, buzzwords innecesarios ("modo sigilo", "campo", etc.) y reemplazar apps/APIs ficticias por las capacidades reales del código base.

---

## 1. Resumen de Hallazgos Críticos

El sitio web actual (`site/src/lib/`) heredó textos de borradores antiguos y conceptos de marketing inflados que **no se corresponden con la realidad técnica del proyecto**:

1. **Apps inexistentes en el catálogo:** Se promocionan `MeshChat` y `RF Packet Sniffer`, las cuales **no existen** en el código fuente de `core/`.
2. **Mezcla absurda de categorías:** Se crearon etiquetas artificiales como `BadUSB & Seguridad`, `Sistema & Campo` y `Radio & Mesh`.
3. **Comandos y APIs ficticias en el CLI interactivo:** En el Hero y en el Lua Playground se muestran llamadas inexistentes como `mesh.scan_nodes()`, `mesh.send_tlv()`, o "Modo Sigilo".
4. **Ausencia de las aplicaciones reales:** El sitio no muestra herramientas reales que sí están programadas y funcionando en `core/src/ui/views/` (Editor de Texto, Terminal, Grabadora de Audio, Visor de Galería).

---

## 2. Desglose Componente por Componente

### A. `Hero.svelte` (Portada y Terminal Interactivo)

* **Problema 1 (Falso módulo Mesh):**
  * **Actual:** La pestaña interactiva `mesh` ejecuta:
    ```bash
    mesh.scan_nodes()
    [MESH] Initializing ESP-NOW stealth interface (Channel 6)...
    [MESH] Node detected: MAC 48:27:E2:0A:11:F2 | ShortID: 0x11F2 | RSSI: -42 dBm
    [MESH] Sending TLV handshake (Encrypted AES-128 GCM)...
    ```
  * **Problema:** En el código no existe `mesh.scan_nodes()`, no existe ningún "modo sigilo" (término inventado sin sentido técnico), ni handshake GCM dinámico con ShortIDs. Además, `sys.status()` ya existe en la primera pestaña del terminal.
  * **Corrección:** 
    * Opción A (Recomendada): Eliminar la pestaña `mesh` por completo y dejar el terminal limpio con las 3 pestañas 100% reales (`status`, `badusb`, `audio`).
    * Opción B: Sustituirla por una característica real como la síntesis de voz offline con PicoTTS (`tts.speak("Sistema iniciado")`).
  * **Purga de buzzwords:** Eliminar términos como "modo sigilo" y "hardware táctico" en todo el sitio web.

* **Problema 2 (URL de clonado):**
  * **Actual:** Copia al portapapeles `git clone https://github.com/kaber420/CBD-os.git`.
  * **Corrección:** Usar la URL oficial en minúsculas: `https://github.com/kaber420/cbdos.git`.

---

### B. `Pillars.svelte` (Pilares Tecnológicos)

* **Problema 1 ("Redes Mesh & Modo Sigilo"):**
  * **Texto actual:** *"Redes Mesh & Modo Sigilo: Comunicaciones directas P2P fuera de internet, descubrimiento de nodos con Short IDs dinámicos y cifrado de transmisiones de radio."*
  * **Problema:** Puro humo/buzzwords.
  * **Corrección:** Cambiarlo por un pilar real: **Subsistema de Audio de Alta Fidelidad** (Decodificador Helix MP3 por software en Core 1, códec I2S Everest ES8311 con DMA) o **Almacenamiento y Archivos** (MicroSD SDMMC 4-bit FATFS autónomo).

* **Problema 2 ("Flasheador de Campo"):**
  * **Texto actual:** *"Flasheador de Campo Autónomo"*.
  * **Problema:** La palabra "campo" es pretenciosa e innecesaria.
  * **Corrección:** Llamarlo **Programador / Flasheador Autónomo**.

* **Problema 3 ("Seguridad FIDO2 & BadUSB / Hardware Security Module"):**
  * **Texto actual:** *"Hardware Security Module: Emulador HID USB para payloads DuckyScript, autenticación con token de seguridad, cifrado simétrico y aislamiento de secretos."*
  * **Problema:** CBDos no es un HSM físico. BadUSB no es seguridad criptográfica ni tiene relación conceptual con FIDO2/Kerberos.
  * **Corrección:** Separar con precisión técnica:
    * **Autenticación Kerberos / Passkey FIDO2** (Criptografía y 2FA).
    * **Emulación USB HID** (Inyección de keystrokes DuckyScript).

---

### C. `AppShowcase.svelte` (Catálogo de Aplicaciones)

* **Eliminaciones Obligatorias (No existen en el código):**
  * ❌ **`MeshChat P2P Cifrado` (`meshchat`)**: Eliminar por completo.
  * ❌ **`RF Packet Sniffer & Spectrum` (`pendiente-sniffer`)**: Eliminar por completo.
  * ❌ **Categoría `radio` ("Radio & Mesh")**: Eliminar la categoría y su botón de filtro.

* **Correcciones de Categorías y Etiquetas:**
  * ❌ `BadUSB & Seguridad` ➔ ✔️ **`USB & HID`** (para BadUSB).
  * ❌ `Sistema & Campo` ➔ ✔️ **`Sistema`** (para Flasheador y Explorador de Archivos).
  * ❌ `Seguridad & HID` ➔ ✔️ **`Seguridad`** (exclusivo para Kerberos / FIDO2).

* **Aplicaciones Reales a Incorporar (Están en `core/src/ui/views/`):**
  1. **Editor de Texto (`TextEditorView`):** Edición directa de archivos de texto, scripts y configuraciones en la MicroSD con teclado en pantalla o teclado físico. (Categoría: `Sistema`).
  2. **Terminal / Shell (`TerminalView`):** Consola interactiva local con acceso al sistema de archivos, comandos de sistema y ejecución de utilidades. (Categoría: `Sistema`).
  3. **Grabadora de Audio (`AudioRecorderView`):** Captura de audio directo a WAV/PCM en la MicroSD usando el códec Everest ES8311 por I2S. (Categoría: `Multimedia`).
  4. **Visor de Imágenes / Galería (`GalleryView`):** Renderizado de imágenes desde la tarjeta MicroSD con aceleración DMA2D. (Categoría: `Multimedia`).

---

### D. `LuaPlayground.svelte` (Consola y Ejemplos de Lua++)

* **Problema (Ejemplo de Mesh ficticio):**
  * El archivo incluye un script ficticio `mesh_beacon.luapp` usando métodos inventados de radio:
    ```lua
    local mesh = cbdos.mesh
    mesh.broadcast_tlv(0x01, "PING")
    ```
  * **Corrección:** Eliminar el ejemplo de mesh y reemplazarlo por un ejemplo real y útil de Lua++ en CBDos:
    * **Ejemplo 1:** Control de archivos / logs (`fs.read`, `fs.write`).
    * **Ejemplo 2:** Interfaz gráfica simple (Ventana LVGL con botones y etiquetas).
    * **Ejemplo 3:** Automatización BadUSB / DuckyScript.

---

### E. `FlashingGuide.svelte` (Guía de Instalación)

* **Revisión técnica de comandos:**
  * Verificar que los comandos para P4 (`idf.py build`, `idf.py flash`) y S3 (`pio run`) reflejen exactamente los parámetros del `AGENTS.md`.
  * La sección "Coprocesador C6" debe revisarse para no dar instrucciones ambiguas si el firmware del C6 aún se flashea por puente o USB-Serial directo.

---

## 3. Plan de Acción Inmediato

| Componente | Acción | Prioridad |
|---|---|---|
| `AppShowcase.svelte` | Eliminar `MeshChat` y `Sniffer`, quitar filtro `radio`, agregar apps reales (`TextEditor`, `AudioRecorder`, `Terminal`) | Inmediata |
| `Pillars.svelte` | Reemplazar "Modo Sigilo / Mesh" por Subsistema de Audio o Storage, limpiar "campo" | Alta |
| `Hero.svelte` | Eliminar pestaña `mesh` del CLI interactivo, corregir URL de git | Alta |
| `LuaPlayground.svelte` | Quitar ejemplo `mesh_beacon.luapp` y poner ejemplo de sistema/archivo real | Media |
