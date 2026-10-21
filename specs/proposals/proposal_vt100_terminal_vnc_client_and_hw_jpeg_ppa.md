# 🖥️ Propuesta Formal: Terminal VT100 (`libvterm`), Cliente VNC RFB y Aceleración JPEG/PPA para Administración de Redes

**Fecha:** 20 de Octubre de 2026  
**Estado:** 💡 Propuesta Formal  
**Categoría:** Herramientas de Campo, Red y Administración de Servidores (`core/src/network/`, `core/src/ui/views/`, `bsp/esp32_p4_jc4880/`)  
**Código de Referencia Auditado (`references/NucleoOS-P4/`):**
- `components/libvterm/` + `components/nv_apps/terminal_app.cpp`
- `components/nv_secondscreen/ss_vnc.cpp` + `ss_engine.cpp` + `ss_des.c`
- `components/nv_apps/gallery_jpeg_hw.cpp` (`driver/jpeg_decode.h` + `driver/ppa.h`)

---

## 1. Visión General y Aplicación Directa en Redes / SysAdmin

Para que **CBDos** sea la herramienta definitiva de administración de redes WISP/ISP, servidores y laboratorio LAN (`LAN NetLab`), tres componentes probados en `NucleoOS-P4` resuelven cuellos de botella críticos en sesiones remotas, cámaras IP y control de equipos en campo:

1. **Emulación Terminal ANSI/VT100 Completa (`libvterm`) para SSH y Consola Serie (Punto 4):**
   - **Problema actual:** Cuando nuestro cliente SSH (`cbdos::ssh`) o consola UART se conecta a un router MikroTik, antena Ubiquiti, switch Cisco o servidor Linux y ejecuta programas interactivos de pantalla completa (`htop`, `nano`, `tmux`, `raspi-config`, `/system/resource/monitor`), un `lv_textarea` normal imprime códigos de escape crudos (`^[[2J^[[H^[[32m`).
   - **Solución:** Integrar `libvterm` (biblioteca C pura de ~45 KB sin dependencias) acoplada a un widget LVGL 9.5 que renderiza una rejilla de celdas con paleta de 16/256 colores ANSI, pantalla alternativa (`altscreen`), posicionamiento de cursor y scrollback en PSRAM.

2. **Cliente VNC (RFB) y Segunda Pantalla Táctil para Servidores/PCs (Punto 2):**
   - **Problema actual:** Muchos equipos en racks, mini-PCs, NVR de cámaras, servidores Windows/Linux o Raspberry Pi requieren acceso gráfico KVM/escritorio remoto en campo cuando no tienen monitor conectado.
   - **Solución:** Incorporar el cliente **VNC (protocolo RFB 3.3/3.7/3.8)** basado en `ss_vnc.cpp`, compatible con TightVNC, UltraVNC, x11vnc, wayvnc, macOS Screen Sharing y droidVNC, con descubrimiento automático por mDNS (`_rfb._tcp`), autenticación VNC-DES (`ss_des.c`) y mapeo táctil directo (1 dedo = clic/arrastre izquierdo, 2 dedos = clic derecho / rueda de scroll).

3. **Decodificador JPEG + Escalador PPA por Hardware en ESP32-P4 (Punto 1):**
   - **Problema actual:** Decodificar y reescalar por software frames de **Cámaras IP (ONVIF/RTSP/HTTP Snapshot en LAN NetLab)** o rectángulos **VNC Tight-JPEG** consume demasiada CPU.
   - **Solución:** En el BSP del ESP32-P4 (`jc4880`), usar los periféricos físicos `driver/jpeg_decode.h` y `driver/ppa.h` (Pixel-Processing Accelerator) siguiendo las reglas exactas de alineación de caché y bloques MCU de `gallery_jpeg_hw.cpp`. Esto permite decodificar y escalar streams MJPEG/JPEG a **30 FPS reales con 0% de carga en la CPU principal** (y mantener un fallback ligero `tjpgd` en el ESP32-S3).

---

## 2. Arquitectura Técnica de los 3 Subsistemas

### 2.1. Subsistema 1: Acelerador Hardware JPEG + PPA (`cbdos::gfx::HwCodec`)
Basado en la implementación verificada de `references/NucleoOS-P4/components/nv_apps/gallery_jpeg_hw.cpp`:

- **Reglas críticas del hardware ESP32-P4 descubiertas en el código de referencia:**
  1. **Redondeo a bloques MCU (`16x16` / `16x8` / `8x8`):** El decodificador hardware del P4 escribe MCUs completos. Si una cámara IP o captura envía `1920x1080` en YUV420, el buffer de salida **debe** alojar `1920x1088` bytes (`rows = (h + 15) & ~15`), usando `jpeg_alloc_decoder_mem(..., JPEG_DEC_ALLOC_OUTPUT_BUFFER)`. De lo contrario, el driver de ESP-IDF falla.
  2. **Orden de bytes BGR para LVGL 9.5 RGB565:** Se debe configurar `cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR` (que en el periférico JPEG del P4 significa *little-endian RGB565*, el formato exacto que usa LVGL 9.5 y el panel MIPI-DSI).
  3. **Alineación estricta de 64 bytes para el PPA (`GALLERY_PPA_ALIGN = 64`):** El bloque PPA SRM (Scale-Rotate-Mirror) exige que tanto el puntero de salida como el tamaño del buffer en PSRAM sean múltiplos exactos de la línea de caché de 64 bytes (`heap_caps_aligned_alloc(64, aligned_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`).

- **Interfaz HAL Agnóstica en `core/include/cbdos/image_accel.hpp`:**
  ```cpp
  namespace cbdos {
  namespace gfx {
      struct DecodedImage {
          uint16_t* pixels565 = nullptr;
          int width = 0;
          int height = 0;
          int stridePx = 0; // pitch en píxeles (múltiplo de 8 o 16)
      };

      // En P4 usa HW JPEG + PPA; en S3 usa TJpgDec por software
      bool decodeJpegToRgb565(const uint8_t* jpgData, size_t jpgLen, DecodedImage& outImg);
      bool decodeAndScaleJpeg(const uint8_t* jpgData, size_t jpgLen, uint16_t* dstRgb565, int dstW, int dstH, bool coverCrop = false);
      void freeDecodedImage(DecodedImage& img);
  }
  }
  ```
- **Dónde impacta en Redes:**
  - **Visor de Cámaras IP / RTSP / HTTP Snapshot en `LAN NetLab`:** Permite ver cámaras Hikvision, Dahua, Axis, ESP32-CAM o ONVIF en tiempo real dentro de una tarjeta LVGL 9.5.
  - **Cliente VNC (`ENC_TIGHT` con JPEG):** Descomprime los rectángulos JPEG del escritorio remoto directamente por hardware.

---

### 2.2. Subsistema 2: Cliente VNC RFB y Monitor Táctil de Red (`VncClientService` & `VncViewerView`)
Basado en `references/NucleoOS-P4/components/nv_secondscreen/ss_vnc.cpp` (1,175 líneas):

- **Capacidades del Protocolo RFB soportadas:**
  - **Handshake y Versiones:** RFB `3.3`, `3.7`, `3.8` y `3.889` (Apple Remote Desktop / macOS Screen Sharing).
  - **Seguridad:** `None` y `VNC Authentication` (reto-respuesta DES de 8 bytes con inversión de bits por byte implementada en `ss_des.c` de apenas 180 líneas).
  - **Codificaciones (`Encodings`):**
    - `Raw (0)`, `CopyRect (1)`, `Hextile (5)`, `Zlib (6)`, `Tight (7)` (con sub-encoding JPEG acelerado por HW en P4) y `ZRLE (16)` (descomprimido al vuelo con `miniz`/`tinfl` en ROM).
    - `DesktopSize (-223)` y `ExtendedDesktopSize (-308)`: Si el servidor VNC soporta redimensionamiento virtual (`Xvnc`, Linux GNOME/KDE virtual monitor), CBDos solicita automáticamente la resolución nativa de su pantalla (`800x480` en JC4880 o `480x320` en S3), convirtiéndose en un monitor táctil perfecto sin pérdida de nitidez.
  - **Escalado en Vivo sin Buffer 4K intermedio:** Al igual que hace `ss_vnc.cpp`, las filas decodificadas de un servidor 1080p/4K se mapean al vuelo hacia el canvas/framebuffer del panel sin reservar un framebuffer remoto gigante en RAM.
  - **Conexión Directa, Escucha Inversa (Puerto `5500`) y Descubrimiento mDNS (`_rfb._tcp`):**
    - Se integra directamente con `LAN NetLab`: cuando el escáner LAN detecta el puerto TCP `5900` abierto en un host, aparece el botón **"Abrir Escritorio VNC"** junto al botón de **"Abrir Terminal SSH"**.

---

### 2.3. Subsistema 3: Emulador Terminal VT100/xterm (`libvterm` + `TerminalWidget` LVGL 9.5)
Basado en `references/NucleoOS-P4/components/libvterm/` y `components/nv_apps/terminal_app.cpp`:

- **Arquitectura del Emulador:**
  - **Motor `libvterm` en `core/third_party/libvterm/`:** Código C puro estándar que mantiene el estado de la matriz de caracteres (`VTerm`, `VTermScreen`, ej. `80x24` o `100x28` celdas), atributos (`bold`, `underline`, `reverse`), colores ANSI 16/256/RGB, regiones de scroll, pantalla alternativa (`altscreen` para `htop`/`vim`/`nano`) y respuestas automáticas `DSR/CPR`.
  - **Renderizado Reactivo por Filas Dañadas (`damage` callback):**
    - `libvterm` notifica exactamente qué filas (`start_row..end_row`) cambiaron al recibir datos por SSH o UART.
    - El widget de CBDos en LVGL 9.5 solo invalida las filas sucias (`lv_obj_invalidate_area`) y dibuja con fuente monoespaciada (`DejaVu Sans Mono` / `JetBrains Mono`).
  - **Scrollback de 1,000 líneas en PSRAM (`SbCell`):** Las líneas que salen por la parte superior en modo consola normal se guardan en un anillo en PSRAM y se pueden revisar deslizando el dedo verticalmente.
  - **Barra Táctica de Teclas Especiales (Extra-Keys Row):**
    - Fila compacta en la parte inferior sobre el teclado en pantalla con: `ESC`, `TAB`, `CTRL` (sticky/armado), `ALT`, `↑`, `↓`, `←`, `→`, `/`, `-`, `|` y `Ctrl+C`.
  - **Doble Backend Conectable:**
    1. **Sesión SSH (`cbdos::ssh`):** Canal PTY interactivo completo con envío de tamaño de ventana (`SIGWINCH` / PTY resize) y secuencias xterm en modo raw.
    2. **Consola Serie / UART (`cbdos::uart`):** Para configurar switches administrables, routers Cisco/MikroTik por cable consola RJ45-UART o placas embebidas.

---

## 3. Integración con `LuaBridge` (`cbdos.ssh`, `cbdos.vnc`, `cbdos.camera`)

Además de las vistas nativas en LVGL 9.5, los nuevos componentes enriquecen los scripts `.luapp` y la automatización:

```lua
-- 1. Abrir la vista interactiva VT100 conectada a un host SSH o UART desde una .luapp
cbdos.ssh.open_terminal({ host = "192.168.1.1", user = "admin", pass = "secret" })

-- 2. Lanzar el visor VNC hacia un servidor detectado en LAN Recon
cbdos.vnc.connect("192.168.1.50", 5900, "mipassword")

-- 3. Capturar y dibujar un snapshot JPEG de una cámara IP en un canvas LVGL usando HW JPEG + PPA
local jpg_bytes = http.get("http://192.168.1.64/onvif-http/snapshot")
if jpg_bytes then
    cbdos.canvas.draw_jpeg(my_canvas, jpg_bytes, 0, 0, 320, 240)
end
```

---

## 4. Plan de Implementación Modular por Fases

| Fase | Módulo a Implementar | Archivos Base de Referencia (`NucleoOS-P4`) | Destino en `CBDos` |
| :---: | :--- | :--- | :--- |
| **Fase 1** | **Terminal VT100/ANSI (`libvterm`) para SSH y UART** | `components/libvterm/*.c` y `components/nv_apps/terminal_app.cpp` | `core/src/terminal/VTermWidget.cpp` + integración en `SshTerminalView` y `UartConsoleView` |
| **Fase 2** | **Aceleración Hardware JPEG + PPA en ESP32-P4** | `components/nv_apps/gallery_jpeg_hw.cpp` (`decode_locked`, `gallery_ppa_scale_*`) | `core/include/cbdos/image_accel.hpp` + `bsp/esp32_p4_jc4880/src/p4_jpeg_ppa.cpp` + binding `cbdos.canvas.draw_jpeg` |
| **Fase 3** | **Cliente VNC RFB con mDNS y soporte Tight-JPEG / ZRLE** | `components/nv_secondscreen/ss_vnc.cpp` y `ss_des.c` | `core/src/network/VncClient.cpp` + `core/src/ui/views/VncViewerView.cpp` + botón directo desde `LAN NetLab` (puerto 5900) |
