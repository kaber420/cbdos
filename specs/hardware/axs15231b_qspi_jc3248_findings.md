# Especificación Técnica Hardware: Subsistema de Display y Touch AXS15231B (JC3248W535)

## 1. Fuente de la Verdad (CBDos Working Implementation)
- **Implementación funcional:** `bsp/esp32_s3_jc3248/.pio/libdeps/esp32s3/JC3248W535 Driver/`
  - `JC3248W535_Display.cpp` / `.h`
  - `JC3248W535_Touch.cpp` / `.h`
  - `Arduino_AXS15231B.cpp` / `.h`
  - `Arduino_ESP32QSPI.cpp` / `.h`

---

## 2. Mapa de Pines y Hardware

| Función | Pin GPIO | Descripción |
|---|---|---|
| **QSPI CS** | GPIO 45 | Chip Select activo bajo |
| **QSPI SCLK** | GPIO 47 | Reloj QSPI (32 MHz, Modo 0) |
| **QSPI SDIO0 (MOSI)** | GPIO 21 | Línea de datos 0 |
| **QSPI SDIO1 (MISO)** | GPIO 48 | Línea de datos 1 |
| **QSPI SDIO2 (WP)** | GPIO 40 | Línea de datos 2 |
| **QSPI SDIO3 (HD)** | GPIO 39 | Línea de datos 3 |
| **LCD RST** | -1 (NC) | No conectado físicamente (Reset por software 0x01) |
| **LCD Backlight** | GPIO 1 | Control de retroiluminación (LEDC / HIGH activo) |
| **Touch I2C SDA** | GPIO 4 | Datos I2C del panel táctil |
| **Touch I2C SCL** | GPIO 8 | Reloj I2C del panel táctil (400 kHz) |
| **Touch INT** | GPIO 3 | Interrupción táctil |
| **Touch Addr** | 0x3B | Dirección esclavo I2C |

---

## 3. Protocolo de Bus QSPI AXS15231B

### A. Estructura de Tramas SPI
- **Modo SPI:** 0 (CPOL=0, CPHA=0).
- **Frecuencia:** 32 MHz.
- **Estructura de Comando:** 8 bits de Opcode + 24 bits de Dirección/Comando + Datos.
  - **Opcode de Comandos (Single SPI):** `0x02`.
  - **Opcode de Envío de Píxeles (Quad SPI):** `0x32`.
  - **Dirección de Envío de Comandos:** `((uint32_t)cmd) << 8`.
  - **Dirección de Transmisión de Píxeles:** `0x003C00` (comando RAMWRC `0x3C` desplazado 8 bits).

### B. Ventana de Direccionamiento y Escritura por Slices en QSPI
1. **Primer Slice (`y_start == 0`):**
   - **CASET (0x2A):** Define el rango horizontal completo `x_start` a `x_end - 1` (0 a 319) con Opcode `0x02`.
   - **RASET (0x2B):** Define la altura completa activa del panel `0` a `479` con Opcode `0x02`.
   - **RAMWR (0x2C):** Se envía con Opcode `0x32` y dirección `0x002C00` para reiniciar el puntero de memoria del controlador en el origen `(0, 0)`.
2. **Slices Consecutivos (`y_start > 0`):**
   - **PROHIBIDO enviar RASET intermedio:** Enviar comandos RASET o re-enviar `0x2C` en transferencias fraccionadas por QSPI resetea el puntero vertical del AXS15231B, provocando superposición de bloques (la parte inferior se dibuja arriba y la headerbar se va al fondo).
   - **RAMWRC (0x3C):** Se envían los datos de píxeles consecutivos con Opcode `0x32` y dirección `0x003C00` (*Memory Write Continue*), permitiendo que el hardware avance secuencialmente línea por línea sin descalibrar el puntero.
3. **Inicialización del Panel:**
   - En la inicialización, **PROHIBIDO** enviar el comando `0x22` (All Pixels Off), ya que fuerza todos los píxeles a negro e inhibe el refresco visual.

---

## 4. Resolución y Configuración de Pantalla

- **Resolución Física:** 320 x 480 píxeles.
- **Orientación Nativa:** Portrait (Rotación 0: 320 píxeles horizontal, 480 píxeles vertical).
- **Formato de Color:** RGB565 (16 bits por píxel).
- **Inversión de Color:** `LCD_CMD_INVOFF` (`0x20` - No invertido).
- **MADCTL:** `0x00` para rotación 0 (RGB, no mirror, no swap).

### Discrepancia Crítica en sdkconfig:
En `configs/sdkconfig.JC3248W535EN`:
```ini
CONFIG_LV_HOR_RES_MAX=480
CONFIG_LV_VER_RES_MAX=480
```
Debe ser:
```ini
CONFIG_LV_HOR_RES_MAX=320
CONFIG_LV_VER_RES_MAX=480
```
Porque la pantalla es de 320x480 en modo Portrait. Con 480x480, los layouts de LVGL asumen 480 px de ancho, desbordando y recortando la interfaz lateralmente ("se ve un pedacito").

---

## 5. Controlador Táctil (AXS15231B Touch)

- **I2C Addr:** `0x3B`.
- **Trama de Consulta:**
  `{0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08}` (8 bytes).
- **Validación de Toque:**
  - `data[0] == 0` Y `data[1] != 0`.
- **Extracción de Coordenadas:**
  - `x = ((data[2] & 0x0F) << 8) | data[3];`
  - `y = ((data[4] & 0x0F) << 8) | data[5];`
  - Resolución nativa devuelta: 320x480.
