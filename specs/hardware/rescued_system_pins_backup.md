# Respaldo de Pines Críticos del Sistema (ESP32-P4 JC4880)

Este es un respaldo de seguridad extraído de `board_config_generated.h` y del plan original, para evitar cualquier pérdida de datos antes de unificar el Device Tree. 

Estos son los 28 pines del sistema (System Locked Pins) que no deben ser tocados por las mochilas:

```yaml
resources:
  system_locked:
    display_and_touch: [5, 23, 3, 4]      # RST, BL PWM, Touch RST, Touch INT
    i2c_bus_0: [7, 8]                     # Compartido entre Touch, Códec de Audio y cabecera JP1
    audio_i2s: [13, 12, 10, 9, 48, 11]    # Audio Codec y Amplificador
    sdmmc: [39, 40, 41, 42, 43, 44]       # Ranura MicroSD
    sdio_and_strapping: [18, 19, 14, 15, 16, 17, 54] # Comunicación con ESP32-C6 y strapping pins
    console_uart: [37, 38]                # UART0 nativo de debug (Puerto MX)
    power_control: [36]                   # GPIO 36 controla la alimentación ESP_3V3 del C6
```

**Lista Cruda Extraída (`board_config_generated.h`):**
`3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 23, 36, 37, 38, 39, 40, 41, 42, 43, 44, 48, 54`

> [!CAUTION]
> Esta información está asegurada aquí. No se borrará ningún archivo autogenerado ni código de C++ hasta que estos pines estén debidamente integrados en el archivo `jc4880p443.json`.
