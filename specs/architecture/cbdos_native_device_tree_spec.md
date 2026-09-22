# Especificación Técnica: CBDos Native Device Tree (CDT) y Gestor Unificado de Hardware

**Estado:** Propuesta de Arquitectura (Borrador)  
**Target:** ESP32-P4 / ESP32-S3 (Agnóstico)  

---

## 1. Resumen Ejecutivo

Actualmente, CBDos define la inicialización de hardware y la protección de pines de manera estática y "hardcodeada" en la capa BSP (ej. `S3GpioBackend::isPinAvailable()`). 

Para lograr la visión de un sistema operativo verdaderamente modular y aislar el núcleo (`core/`) del hardware subyacente, proponemos la arquitectura **CBDos Native Device Tree (CDT)**. Inspirado en Linux, el CDT es una representación estructurada (datos) del hardware presente.

El CDT trabajará en conjunto con el **GPIO Resource Manager (GRM)**. El núcleo leerá el Device Tree en el arranque, reservará automáticamente los pines del sistema en el GRM, y dejará el resto de pines libres y expuestos para el uso dinámico del **BackpackManager** (mochilas NFC) y las aplicaciones Lua.

---

## 2. Conceptos Clave

1. **CDT (Sistema Base):** Un archivo `cbdos.cdt` (JSON) que describe los componentes fijos de la placa base (Pantalla, I2S Audio, I2C Touch, MicroSD).
2. **Mini-DT (Mochilas NFC):** Un descriptor supercompacto (MsgPack) almacenado en el tag NFC de las mochilas de expansión, que describe los pines que la mochila necesita de la cabecera de expansión libre (JP1).
3. **GRM (Árbitro Central):** El único subsistema autorizado para conceder o denegar el uso de un pin.

---

## 3. Formato del Device Tree Base (JSON)

Para el sistema base, proponemos JSON por su legibilidad humana. Se puede compilar embebido en el firmware (como un string `const char*`) para que esté disponible en el milisegundo cero del arranque.

```json
{
  "board": "JC4880P443C",
  "version": "1.3",
  "devices": {
    "display": {
      "driver": "st7701s",
      "bus": "mipi_dpi",
      "pins": {
        "rst": 5,
        "bl": 23
      }
    },
    "audio": {
      "driver": "es8311",
      "bus": "i2s",
      "pins": {
        "mclk": 13,
        "bclk": 12,
        "ws": 10,
        "dout": 9,
        "pa": 11
      }
    },
    "expansion_header": {
      "id": "jp1",
      "allowed_pins": [20, 21, 22, 24, 25, 26, 33, 34, 35, 36, 37]
    }
  }
}
```

---

## 4. Nuevo Flujo de Arranque (Boot Sequence)

Con esta arquitectura, el arranque de CBDos cambia radicalmente hacia un modelo de SO completo:

1. **`app_main()` Inicia:**
2. **Carga del CDT:** El sistema parsea el string JSON del Device Tree.
3. **Bloqueo de Hardware (GRM):** El núcleo itera sobre todos los `devices` del CDT. Por cada pin listado, ejecuta `grm.claimPin(pin, "system:" + device_name, Exclusive, true)`.
4. **Validación de Cabecera:** El sistema registra en el GRM cuáles son los `allowed_pins` de la cabecera de expansión, marcándolos como la "reserva libre" (pool).
5. **Inicialización de Drivers:** Se instancian las clases HAL (`hal_display`, `hal_audio`). En lugar de usar números mágicos, los drivers consultan al Device Tree: `int bl_pin = DeviceTree::getPin("display", "bl");`.

---

## 5. Integración con BackpackManager (Mochilas NFC)

El ecosistema de mochilas usa el mismo concepto, pero de forma dinámica ("Plug & Play"):

1. **Lectura NFC:** Al acercar una mochila, se lee el "Mini-DT" (codificado en MsgPack por eficiencia).
2. **Parser y Petición:** El `BackpackManager` decodifica el MsgPack (ej. `{"bus":"SPI", "pins":{"cs":33, "mosi":21}}`).
3. **Arbitraje:** El manager solicita al GRM los pines 33 y 21. 
4. **Verificación Estricta:** El GRM verifica que esos pines:
   - Estén dentro del pool `allowed_pins` del Device Tree base.
   - NO estén bloqueados por el sistema (ej. audio).
   - NO estén en uso por otra aplicación Lua.
5. Si todo es exitoso, se configuran los buses dinámicos y se lanza el driver (`driver=meshcore-companion` o la app de Lua correspondiente).

---

## 6. Próximos Pasos (Hoja de Ruta)

Para implementar esto, sugerimos dividir el trabajo en las siguientes fases:

1. **Motor GRM:** Implementar `core/src/system/gpio_manager.cpp` (El árbitro de memoria).
2. **Estructuras Device Tree:** Crear el parser JSON para el sistema base y adaptarlo al boot.
3. **Limpieza BSP:** Eliminar las validaciones hardcodeadas de `hal_uart_p4.cpp` y forzar que los drivers pidan sus pines al CDT.
4. **Motor Backpack:** Construir el `BackpackManager` con soporte MsgPack para integrarse al GRM.
