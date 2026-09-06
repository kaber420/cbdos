# Propuesta Técnica: Tokenización Semántica por Conceptos, Diccionarios Densos y Optimización de Airtime (CBD-Net)

**Estado:** Propuesta de Arquitectura / Borrador Técnico  
**Módulos Afectados:** `core/net/`, `core/crypto/`, `core/lexicon/`, `core/views/cbdnet/`, `docs/architecture/`  
**Targets Compatibles:** ESP32-P4 (MIPI-DPI 480x800, 32MB PSRAM) y ESP32-S3 (QSPI 320x480, 8MB PSRAM)  
**Medios Físicos de Transporte:** FLRC 2.4 GHz (Semtech SX1280), LoRa (Sub-GHz / 2.4 GHz), ESP-NOW, MQTT (WiFi/Ethernet), Serial / DuckyScript  

---

## 1. Resumen Ejecutivo y Motivación

En sistemas de comunicaciones inalámbricas embebidas, redes malladas (*mesh*) y enlaces de baja potencia (LoRa, FLRC, bandas ISM compartidas), el **Time-on-Air (Airtime)** es el recurso físico más escaso y restrictivo. 

Los enfoques tradicionales basados en texto plano (JSON, XML o cadenas UTF-8 completas) presentan tres fallas críticas:
1. **Saturación del Espectro:** Paquetes de 150 a 300 bytes permanecen en el aire durante cientos de milisegundos, disparando la probabilidad de colisiones de paquetes y consumiendo rápidamente los límites legales de *Duty Cycle* (1% por hora).
2. **Penalización Criptográfica:** El cifrado militar autenticado (ChaCha20-Poly1305 / AES-GCM) requiere una carga fija de metadatos de seguridad (Nonce/IV de 12 bytes y MAC/Tag de autenticación de 16 bytes). Añadir esta "mochila" a un texto no comprimido genera paquetes gigantescos.
3. **Fallo de Traducciones Literales:** Los sistemas de traducción palabra por palabra o basados en modelos pesados generan frases absurdas o incomprensibles al traducir modismos y expresiones coloquiales (ej. traducir *"What's up"* como *"¿Qué arriba?"* en lugar de *"¿Qué onda?"* o *"¿Qué pasa?"*).

**La Solución de CBDos:** Un protocolo de **Compresión Semántica por Conceptos (Concept-ID Tokenization)** basado en diccionarios temáticos densos con longitud variable (1, 2 y 3 bytes mediante conmutación de bancos). Este esquema reduce mensajes complejos a **8 - 15 bytes**, permitiendo incluir cifrado autenticado de grado militar manteniendo el paquete total por debajo de **~40 bytes**, exprimiendo el Airtime al mínimo absoluto y logrando traducciones naturales sin consumo de CPU.

---

## 2. El Motor Semántico de Conceptos (Concept-ID & Universal Localization)

### 2.1 Concepto vs. Cadena Literal
En lugar de codificar letras o palabras aisladas, el protocolo asigna identificadores numéricos únicos a **intenciones, modismos, estados y conceptos completos**:

```
                              [EMISOR - Español México]
                          Escribe: "¡Qué onda amigo!"
                                       │
                                       ▼
                   Parser Semántico detecta modismo indexado
                   Asigna Concept-ID: [0xF8, 0x0A, 0x12]
                   (ID: GREETING_INFORMAL_FRIEND)
                                       │
                       📡 Transmisión RF: 3 Bytes
                                       │
      ┌────────────────────────────────┼────────────────────────────────┐
      ▼                                ▼                                ▼
[RECEPTOR: es_ES (España)]    [RECEPTOR: en_US (USA)]      [RECEPTOR: es_DO (Caribe)]
Configuración Local:          Configuración Local:         Configuración Local:
👉 "¡Qué pasa tío!"           👉 "What's up bro!"          👉 "¡Klk pana!"
```

### 2.2 Detección en Tiempo Real y Marcador Visual en la UI (LVGL 9.5)
Durante la redacción del mensaje en el editor de texto de CBDos:
1. El motor de análisis sintáctico evalúa el buffer en tiempo real a medida que el usuario escribe.
2. Cuando se detecta una frase hecha, saludo o término que coincide con una entrada del diccionario semántico, la UI **aplica un color de resalte / marcador visual** (ej. un fondo suave verde/turquesa sobre el texto).
3. Esto le indica al usuario de forma transparente que esa frase completa se empaquetará como un **concepto denso de 2 o 3 bytes**, garantizando el mínimo consumo de radio y una localización perfecta en destino.

---

## 3. Estructura Binaria y Conmutación de Bancos (Variable Length Encoding)

El primer byte del flujo actúa como decodificador de longitud y selector de banco temático:

```
 ┌───────────────────────┬──────────────────────┬──────────────────────────────────────────────────────────┐
 │ RANGO (1er Byte)      │ TAMAÑO TOTAL         │ FUNCIÓN / CONTENIDO                                      │
 ├───────────────────────┼──────────────────────┼──────────────────────────────────────────────────────────┤
 │ 0x00 - 0x7F (0..127)  │ 1 Byte (MSB = 0)     │ ASCII directo, signos, números y conectores ultra-básicos│
 │ 0x80 - 0xDF (128..223)│ 2 Bytes              │ Vocabulario General Base (~24,000 palabras cotidianas)   │
 │ 0xE0 - 0xFF (224..255)│ 3 Bytes (1 Prefijo + │ SELECTOR DE BANCO TEMÁTICO / DICCIONARIO ESPECIALIZADO   │
 │                       │          2 Datos)    │ (Hasta 32 diccionarios × 65,536 entradas = 2M conceptos) │
 └───────────────────────┴──────────────────────┴──────────────────────────────────────────────────────────┘
```

### 3.1 Desglose de los Rangos

#### A. Rango 1 Byte (`0x00 - 0x7F`): Caracteres Crudos y Conectores
* **`0x00 - 0x1F`:** Conectores gramaticales de altísima frecuencia (`de`, `en`, `que`, `a`, `y`, `el`, `la`, `to`, `for`).
* **`0x20 - 0x7E`:** Caracteres ASCII estándar directos para palabras no indexadas, nombres propios o identificadores numéricos.

#### B. Rango 2 Bytes (`0x80 - 0xDF`): Vocabulario General Activo
* Permite codificar más de **24,000 palabras comunes** del lenguaje activo cotidiano.
* Palabras largas como *"agricultor"* o *"otorrinolaringólogo"* ocupan exactamente **2 bytes**, sin importar su longitud de caracteres.

#### C. Rango 3 Bytes (`0xE0 - 0xFF`): Selectores de Bancos Temáticos
El primer byte (`0xE0` a `0xFF`) selecciona el **Banco Especializado**, y los 2 bytes subsiguientes apuntan a la palabra, tecnicismo o frase dentro de ese banco:

```
                   BYTE 1                         BYTES 2 y 3
          [SELECTOR DE BANCO]                [ÍNDICE EN EL BANCO]
         ┌────────────────────┐            ┌──────────────────────┐
         │    0xF8 (248)      │     +      │      0x04, 0x1A      │
         └─────────┬──────────┘            └──────────┬───────────┘
                   │                                  │
                   ▼                                  ▼
      "Banco Regionalismos/Modismos"      "Entrada #1050 del Banco"
                                          👉 "GREETING_WHATS_UP"
```

#### Catálogo de Bancos Temáticos Propuestos (32 Bancos Disponibles):
* **`0xF0` (240): Banco Ciberseguridad / Hacking:** (`payload`, `handshake`, `exploit`, `buffer overflow`, `dump`).
* **`0xF1` (241): Banco Electrónica y Hardware:** (`osciloscopio`, `I2C bus`, `pull-up`, `esquema`, `pinout`).
* **`0xF2` (242): Banco Táctico / Operaciones:** (`coordenada GPS`, `punto de encuentro`, `frecuencia repetidora`, `status OK`).
* **`0xF3` (243): Banco Oficios y Ciencias:** (`agricultor`, `diagnóstico`, `mantenimiento`, `telemetría`).
* **`0xF8` (248): Banco Semántico / Modismos / Saludos:** Expresiones idiomáticas localizables automáticamente.

---

## 4. Optimización Extrema del Airtime (Time-on-Air)

### 4.1 Amortización del Overhead Criptográfico
Para que la comunicación sea segura, se emplea cifrado autenticado **ChaCha20-Poly1305**:

```
 ┌─────────────────────────────────────────────────────────────────────────────────┐
 │                   ESTRUCTURA DEL FRAME COMPACTO CIFRADO (~40 Bytes)             │
 ├─────────────────┬──────────────────────────┬──────────────────┬─────────────────┤
 │ NONCE / IV      │ CUERPO TOKENIZADO DENSO  │ METADATOS        │ AUTH TAG (MAC)  │
 │ (12 Bytes)      │ (8 - 14 Bytes)           │ (2 Bytes)        │ (16 Bytes)      │
 │ Vector único    │ 4 a 8 Conceptos /        │ ID Remitente /   │ Integridad      │
 │ anti-replay     │ Palabras del Diccionario │ Canal            │ Poly1305        │
 └─────────────────┴──────────────────────────┴──────────────────┴─────────────────┘
  │                                            │                  │
  └───────────── CARGA CIFRADA (ChaCha20) ─────┴──────────────────┘
```

### 4.2 Impacto en el Espectro Radioeléctrico

| Parámetro | Mensaje Tradicional (JSON/UTF-8) | CBD-Net (Diccionario Denso + Cifrado) | Mejora |
| :--- | :--- | :--- | :--- |
| **Tamaño de Payload** | 180 - 250 Bytes | **36 - 44 Bytes** | **~80% - 85% de reducción** |
| **Airtime @ FLRC 1.3 Mbps** | ~2.5 ms | **< 0.35 ms (350 microsegundos)** | **7x más rápido** |
| **Airtime @ LoRa SF7 / 125kHz**| ~280 ms | **~45 ms** | **6x menor ocupación** |
| **Tasa de Colisiones en Malla**| Media / Alta (canal ocupado) | **Casi nula (chispazos mínimos)** | **Máxima fiabilidad** |
| **Cumplimiento Duty Cycle 1%** | ~120 mensajes / hora | **> 800 mensajes / hora** | **6.6x más capacidad** |
| **Consumo Energético TX** | Alto (PA encendido cientos de ms) | **Ultra bajo (micro-ráfagas)** | **Prolonga semanas de batería** |

### 4.3 Modo Difusión Ligera: Transmisión de Obras Completas y Manuales en 36s/Hora
En escenarios civiles, radioafición o difusión pública (ej. Europa con banda 868 MHz sujeta a **1% de Duty Cycle = 36 segundos de emisión por hora**), donde no se requiere la "mochila" de cifrado militar ni HMAC:

* **El Problema en Texto Plano:** *Don Quijote de la Mancha* (~380,000 palabras) o la *Biblia* (~800,000 palabras) pesan entre **2.2 MB y 4.5 MB** en UTF-8. Transmitir eso por LoRa convencional tomaría días o violaría la ley en minutos.
* **El Milagro del Diccionario Denso:**
  * Al mapear cada palabra a **2 bytes** (o frases completas a 3 bytes), *Don Quijote* se reduce a **~700 KB**.
  * **En FLRC 2.4 GHz (1.3 Mbps):** ¡La obra completa se transmite en el aire en apenas **4.5 segundos** de emisión total!
  * **Bajo regulación europea de 36s/hora (LoRa/FSK):** Puedes transmitir **enciclopedias enteras de supervivencia, manuales técnicos de campo, guías médicas o bibliotecas completas** en unas pocas ventanas de tiempo sin saturar jamás el espectro ni infringir las normativas de radio.

### 4.4 Foros Técnicos, Sincronización Delta y "Winks" Vectoriales (Lottie / ThorVG)
El modelo de comunidad técnica (inspirado en foros de reparación/electrónica tipo *Clan GSM*, *XDA* o *Reddit*) se beneficia enormemente de la combinación de diccionarios densos y sincronización por diferencias (*Cherry-Picking / Delta Sync*):

1. **Estructura de Hilos y Suscripciones Selectivas:**
   * Los usuarios no descargan toda la red: se suscriben a tableros temáticos (`#reparacion_hardware`, `#esquemas`, `#exploits`) o a hilos de solución específicos.
   * **Cherry-Picking / Delta:** Al abrir la aplicación, el Cyberdeck envía solo una cabecera de 4 bytes con el último ID/Timestamp que tiene en caché (`SINCE_ID: 412`). El nodo transmisor solo responde con las respuestas o soluciones nuevas.

2. **Reacciones y "Winks" Vectoriales de 1-2 Bytes (MSN Messenger Style):**
   * El firmware y la MicroSD (`/sdcard/cbdnet/assets/`) almacenan una biblioteca de iconos técnicos y animaciones vectoriales renderizadas a 60 FPS con **ThorVG / Lottie** (ej. chip sobrecalentado, soldador humeante, pulso de éxito, osciloscopio activo, zumbido táctico).
   * Para enviar una animación de pantalla completa o un efecto visual con sonido:
     * **En el aire:** Solo viaja **1 o 2 bytes** (`TOKEN_WINK_SOLDERING_SMOKE` o `TOKEN_ANIM_SUCCESS`).
     * **En el receptor:** La pantalla IPS ST7701S reproduce la animación vectorial fluida y el códec ES8311 reproduce el efecto de sonido local correspondiente.

3. **Estrategia Híbrida de Conectividad (El Embudo de Redes):**
   * **Zona con Cobertura / WiFi o FLRC 2.4 GHz (Alta Velocidad):** Cuando el Cyberdeck tiene acceso a Internet o se conecta con un nodo de alta velocidad, descarga los deltas pesados (nuevos paquetes de diccionarios, nuevas animaciones Lottie/ThorVG y esquemas a la MicroSD).
   * **Modo Campo / Fuera de Línea (LoRa Sub-GHz / 2.4 GHz):** En el campo, las conversaciones, debates técnicos y "winks" se ejecutan al 100% utilizando únicamente los tokens de 2-3 bytes que disparan los recursos ya instalados en el hardware.

---

## 5. Implementación en la Arquitectura de CBDos

```
  ┌──────────────────────────────────────────────────────────────────┐
  │                         CAPA DE APLICACIÓN                       │
  │  CbdNetView / Chat UI (LVGL 9.5)                                 │
  │  - Editor con resaltado de conceptos reconocidos (Marcador)      │
  │  - Renderizado automático según Locale (es_MX, es_ES, en_US)     │
  └─────────────────────────────────┬────────────────────────────────┘
                                    │
  ┌─────────────────────────────────▼────────────────────────────────┐
  │                           NÚCLEO (core/)                         │
  │  1. LexiconEngine:                                               │
  │     - PSRAM Fast Cache: Offsets O(1) para decodificar.           │
  │     - Trie / Hash Table: Codificación en tiempo real.            │
  │     - Carga modular desde MicroSD: `/sdcard/cbdnet/lexicon_*.bin`│
  │  2. CryptoLayer:                                                 │
  │     - ChaCha20-Poly1305 (ESP32 Hardware / libsodium).            │
  └─────────────────────────────────┬────────────────────────────────┘
                                    │
  ┌─────────────────────────────────▼────────────────────────────────┐
  │                      CAPA DE TRANSPORTE / BSP                    │
  │  - FLRC 2.4 GHz / LoRa (SX1280 / SX1262)                         │
  │  - ESP-NOW / WiFi MQTT Client                                    │
  └──────────────────────────────────────────────────────────────────┘
```

### 5.1 Gestión de Memoria en PSRAM (Zero Lag)
* El diccionario base y los bancos especializados se leen desde la tarjeta MicroSD (`/sdcard/cbdnet/dict_main.bin`) y se alojan en la **PSRAM (32 MB en P4 / 8 MB en S3)**.
* **Decodificación Instantánea $O(1)$:** Un array de punteros a memoria permite acceder a la cadena de texto traducida en menos de 1 microsegundo por token.
* **Cero fragmentación de Heap:** No se utiliza `cJSON` ni asignaciones dinámicas por mensaje; el texto se escribe directamente en los buffers de LVGL 9.5.

---

## 6. Próximos Pasos para Desarrollo

1. **Definir el Formato Binario del Archivo de Diccionario (`lexicon.bin`):** Cabecera, tabla de offsets y tabla de cadenas.
2. **Implementar el Generador de Diccionarios en Host (Python/C++ Tool):** Script para compilar archivos YAML/JSON con términos, modismos y traducciones multilingües al binario denso de CBDos.
3. **Crear `LexiconEngine` en `core/lexicon/`:** Clases agnósticas en C++ para parseo y resolución de conceptos.
4. **Integrar el Resaltador Semántico en `CbdNetView` (LVGL 9.5):** Componente de texto interactivo con feedback de color para conceptos empaquetados.
