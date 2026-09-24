# Plan de Investigación y Auditoría de Hardware: JC4880P443C

> **Propósito:** Establecer una metodología de auditoría estricta, exhaustiva y sin suposiciones para determinar con precisión física y eléctrica el 100% de los pines, conectores, buses y periféricos de la placa **Guition JC4880P443C (ESP32-P4)**. 
> Este proceso eliminará definitivamente cualquier dato heredado, inventado o aproximado.

---

## 1. Fuentes Primarias de Verificación (Únicas Autorizadas)

Toda la información eléctrica y de señales debe provenir exclusivamente de:
`/home/kaber420/Descargas/JC4880P443C_I_W/`

1. **Esquemáticos Electrónicos:**
   * `5-Schematic/JC4880P443_V1.0.pdf`
   * Hojas individuales: `1_PWR.png`, `2_LCD&CSI.png`, `3_ESP32-P4.png`, `4_USB&IO.png`, `5_485.png`, `6_CODEC&TFCARD.png`, `7_OTHER.png`.
2. **Código Fuente Oficial de Referencia (BSP / Config):**
   * `1-Demo/arduino_examples/`
   * `1-Demo/idf_examples/ESP-IDF_5.5.4/xiaozhi-esp32-main/main/boards/guition-jc4880p443/`

---

## 2. Metodología de Auditoría Pin a Pin (Matriz de 55 Pines)

Se auditará sistemáticamente cada uno de los pines GPIO del ESP32-P4 (GPIO 0 al GPIO 54).
Para cada pin se registrará obligatoriamente la siguiente ficha técnica:

```text
GPIO [N]:
- Pin del Módulo U3 (JC-ESP32P4-M3): [Número de pin físico del módulo]
- Etiqueta de Red (Net Label): [Nombre exacto en el esquemático]
- Destino Físico: [Chip soldado / Conector externo / Transistor / Divisor / Sin Conexión (NC)]
- Componente y Pin de Destino: [Ej. U1 (ES8311) Pin 9 / Conector J5 Pin 2 / R52]
- Función Eléctrica: [Entrada digital, Salida PWM, Bus I2S, Bus I2C, ADC, etc.]
- Macro en Código Oficial: [Nombre de la macro en config.h / bsp.h de Guition]
- Modo URM: [Internal (Soldado) / Expansion (Conector externo) / NC]
```

### Regla de Validación Cruzada:
Un pin solo se considerará **VERIFICADO** cuando:
1. La etiqueta de red coincida entre el módulo central (`U3`) y el periférico en el esquemático.
2. Coincida con la definición macro en los ejemplos oficiales de código.
3. Si existe contradicción, se registrará como **DISCREPANCIA ABIERTA** para su análisis.

---

## 3. Auditoría de Conectores de Expansión

Para los conectores donde se enchufan periféricos externos, el plan mapea **exclusivamente qué pin del conector se conecta a qué GPIO o línea de voltaje**:

1. **Cabecera JP1 (2×13 pines):**
   * Mapeo pin por pin (Pin 1 al Pin 26) contra su GPIO / 3V3 / 5V / GND.
2. **Conector J5 (UART Aux):**
   * Mapeo pin por pin (Pin 1 al Pin 4) contra GPIO TX / RX / VCC / GND.
3. **Conector J4 (RS-485):**
   * Mapeo de señales diferenciales A / B y qué GPIOs controlan el transceptor MAX485.
4. **Conector CN2 (UART0 Debug):**
   * Mapeo pin por pin (Pin 1 al Pin 4) contra GPIO 37 / 38 / VIN / GND.
5. **Conector FPC2 (Cámara CSI):**
   * Líneas de datos MIPI CSI, control I2C y señales de disparo/control.

---

## 4. Fases de Ejecución

1. **Paso 1: Mapeo de Hardware Interno Soldado (`Internal`)**
   * Identificar y contrastar esquemático vs código para: Display DSI, Touch GT911, Audio ES8311, SDMMC, Batería ADC y Coprocesador C6.
2. **Paso 2: Mapeo de Conectores de Expansión (`Expansion`)**
   * Mapear cada pin de los conectores `JP1`, `J5`, `J4` y `CN2`.
3. **Paso 3: Identificación de Pines Sin Conexión (`NC`)**
   * Documentar qué GPIOs del chip carecen de pista en el circuito.
4. **Paso 4: Consolidación Final para Device Tree y URM**
   * Generar la especificación definitiva para actualizar `boards/jc4880p443.json` y alimentar el URM de CBDos.
