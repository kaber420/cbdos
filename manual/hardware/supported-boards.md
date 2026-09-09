# 🔌 Hardware Oficial y Placas Soportadas

CBDos está diseñado bajo una arquitectura modular desacoplada que permite soportar múltiples placas y pantallas con facilidad. A continuación se detallan las placas de desarrollo comerciales oficialmente integradas.

---

## 1. Guition JC4880P443C (Target Principal)

Es la plataforma insigne para el desarrollo de CBDos, basada en el módulo **JC-ESP32P4-M3 Rev 1.3**.

```
+-------------------------------------------------------------+
|               Guition JC4880P443C (ESP32-P4)                |
|                                                             |
|  [ Pantalla IPS 4.3" 480x800 MIPI-DPI ST7701S @ 60 FPS ]    |
|  [ Panel Táctil Capacitivo Goodix GT911 (I2C) ]             |
|                                                             |
|  SoC: ESP32-P4 RISC-V Dual-Core @ 400 MHz                   |
|  RAM: 32 MB Hexal-PSRAM | Flash: 16 MB NOR Flash            |
|  Audio: Everest ES8311 (I2C + I2S) + Amplificador PA        |
|  Ranura MicroSD: Slot 0 SDMMC 4-bit (GPIO 39-44)            |
|  Conectividad: Coprocesador ESP32-C6-MINI (WiFi 6 / BT 5)   |
|  Cabecera JP1: Conector de expansión 2x13 pines             |
+-------------------------------------------------------------+
```

### Especificaciones Clave
* **Microcontrolador:** ESP32-P4 (Dual-Core RISC-V 400 MHz).
* **Aceleración Gráfica:** Hardware DMA2D (PPA) y soporte de renderizado LVGL v9.5 en PSRAM.
* **Táctil:** Goodix GT911 (I2C SDA=7, SCL=8, RST=3, INT=4).
* **Audio Codec:** ES8311 (I2C SDA=7, SCL=8; I2S MCLK=13, BCLK=12, WS=10, DOUT=9; Amp PA=11).
* **Almacenamiento:** MicroSD en bus SDMMC de alta velocidad a 4 bits con control LDO VO4 (3.3V).

> 💡 **Detalles Técnicos:**  
> Para consultar el mapa de pines completo de la cabecera JP1 y periféricos, revisa el documento de ingeniería [`specs/hardware/pinouts_and_ports.md`](https://github.com/kaber420/cbdos/blob/main/specs/hardware/pinouts_and_ports.md).

---

## 2. Guition JC3248W535 (Target Portátil S3)

Una placa ultra compacta con pantalla de 3.5 pulgadas orientada a cyberdecks de bolsillo.

### Especificaciones Clave
* **Microcontrolador:** ESP32-S3 (Dual-Core Xtensa @ 240 MHz).
* **Memoria:** 16 MB Flash / 8 MB Octal-PSRAM.
* **Pantalla:** 3.5" IPS 320x480 con controlador AXS15231B vía bus QSPI.
* **Panel Táctil:** Capacitivo FT6336 / AXS15231B.
* **Framework de compilación:** PlatformIO + pioarduino.
