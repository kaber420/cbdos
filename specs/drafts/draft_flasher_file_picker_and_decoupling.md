# 📋 Borrador de Diseño: Selector Universal de Firmwares y Desacoplamiento de Binarios en Flasher

## 1. Motivación y Objetivos
1. **Reducción de Peso de CBDos:**
   - Actualmente, `bsp/esp32_p4_jc4880/main/CMakeLists.txt` incrusta `assets/c6_slave.bin` (+1.2 MB) dentro de la Flash del P4.
   - Eliminar este binario embebido reduce el tamaño del ejecutable principal de **4.25 MB a ~3.0 MB**, eliminando advertencias de desbordamiento en la partición `app0` y acelerando los tiempos de compilación/flasheo en un 30%.
2. **Flexibilidad Operativa (Multi-Target Flasher):**
   - Eliminar nombres de archivo fijos (`/sdcard/firmware.bin` o `network_adapter.bin`).
   - Permitir al usuario explorar la tarjeta **MicroSD (`/sdcard/`)** o la memoria interna **SPIFFS (`/spiffs/`)** mediante un diálogo visual de selección de archivos en LVGL 9.5.
   - Capacidad de programar diferentes microcontroladores (ESP32-C6, ESP32-S3, ESP32-C3, STM32, etc.) seleccionando el firmware correspondiente sin renombrar archivos en la computadora.

---

## 2. Flujo de Usuario y Diseño de la Interfaz Gráfica (`FlasherView`)

```text
┌─────────────────────────────────────────────────────────────┐
│ 🔌 PROGRAMADOR / FLASHER UNIVERSAL                          │
├─────────────────────────────────────────────────────────────┤
│ 🎯 Dispositivo Objetivo:                                    │
│    [ ESP32-C6 Coprocesador (JP1)                      ▼ ]   │
│    Pines: TX:32, RX:28, BOOT:34, RST:54 | 115200 bps        │
├─────────────────────────────────────────────────────────────┤
│ 📁 Archivo de Firmware:                                     │
│    ┌──────────────────────────────────────────────┬──────┐  │
│    │ /sdcard/firmwares/c6_hosted_v3.0.6.bin       │ [📂] │  │
│    └──────────────────────────────────────────────┴──────┘  │
│    Tamaño: 1,182,608 bytes | Offset: 0x00000000             │
├─────────────────────────────────────────────────────────────┤
│ ⚙️ Ajustes Avanzados:                                       │
│    Velocidad: [ 460,800 bps ▼ ]   Offset: [ 0x00000  ▼ ]   │
├─────────────────────────────────────────────────────────────┤
│ [ ⚡ INICIAR FLASHEO ]                                      │
└─────────────────────────────────────────────────────────────┘
```

### Diálogo Modal de Selección de Archivos (File Picker Modal):
Al presionar el botón `[📂]`:
1. Se abre un diálogo modal de LVGL 9.5 explorando `/sdcard/firmwares/` (o raíz `/sdcard/`).
2. Filtra automáticamente archivos con extensión `.bin` y `.hex`.
3. Muestra nombre de archivo, tamaño y fecha.
4. Al seleccionar un archivo, se valida que exista y se actualiza la ruta activa del preset.

---

## 3. Modificaciones Técnicas Propuestas

### 3.1. En el Host ESP32-P4 (`bsp/esp32_p4_jc4880/`):
- **`main/CMakeLists.txt`:**
  - Remover la sección `EMBED_FILES "assets/c6_slave.bin"`.
  - Remover la compilación de objetos `.S` asociados al binario esclavo.
- **`hal/hal_flasher_p4.cpp`:**
  - Desactivar la bandera `useEmbeddedBin = true` en todos los presets.
  - El motor de flasheo (`esp-serial-flasher`) abrirá directamente el descriptor de archivo estándar `fopen(config.binPath.c_str(), "rb")` desde el VFS (`/sdcard/...` o `/spiffs/...`).

### 3.2. En el Core UI (`core/src/ui/views/FlasherView.cpp`):
- Agregar el botón de búsqueda de archivos junto a la etiqueta de ruta de firmware.
- Integrar la llamada al explorador de archivos nativo de CBDos (`FileDialog` o `FileManagerView` en modo selección).
- Recordar la última ruta seleccionada en la memoria NVS para mayor comodidad.

---

## 4. Directorio Estándar Recomendado en MicroSD
Se estandariza la estructura en la tarjeta MicroSD para organización de binarios:
```text
/sdcard/
└── firmwares/
    ├── c6_esphosted_v3.0.6.bin      (Firmware Wi-Fi 6 / Hosted actual)
    ├── c6_custom_raw802154.bin      (Firmware canal estrecho 2 MHz)
    ├── s3_sensor_node_v1.bin        (Nodo remoto satélite)
    └── badge_defcon.bin             (Firmware externo)
```

---

## 5. Beneficios de esta Implementación
- **Ahorro de Memoria:** Flash del P4 liberada en más de 1.2 MB.
- **Cero Acoplamiento:** CBDos ya no depende de versiones fijas de firmware de coprocesadores en su código fuente.
- **Independencia Total:** Puedes actualizar el firmware del C6 o programar cualquier chip externo simplemente copiando el archivo `.bin` a la MicroSD.
