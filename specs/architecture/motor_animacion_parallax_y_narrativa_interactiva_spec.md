# Especificación Técnica: Motor de Animación Parallax y Narrativa Interactiva Multiplataforma

**Estado:** 📐 Propuesta / Futura Implementación  
**Versión de Especificación:** v1.0.0  
**Área:** `core/engine/story/` & Ecosistema de Contenido Creativo (CBDos / Android / PC)  
**Autor:** CBDos Architecture Core Team  
**Fecha:** Agosto 2026  

---

## 🧭 1. Resumen Ejecutivo y Visión

En dispositivos embebidos basados en microcontroladores (ESP32-P4 / ESP32-S3), la reproducción de video tradicional (MP4/H.264/MPEG) resulta inviable o ineficiente debido a las altas demandas de tasa de transferencia de almacenamiento, consumo masivo de memoria Flash y sobrecarga de decodificación en CPU.

Para solventar esta limitación y dotar al ecosistema **CBDos** de una plataforma de entretenimiento, educación y narrativa interactiva de alto impacto, se define el **Motor de Escenas Parallax y Animación por Marionetas 2D (Procedural Puppet Theater Engine)**.

Este motor no solo permite crear cuentos interactivos para niños, tutoriales y mascotas virtuales a **60 FPS** fluidos en microcontroladores con consumo mínimo de Flash y PSRAM, sino que se concibe desde el primer día como un **estándar multiplataforma**:
- **Dispositivos Embebidos (CBDos en ESP32-P4 / ESP32-S3):** Renderizado vectorial ligero (ThorVG / LVGL 9.5), assets de baja/media huella y decodificación de audio Helix/ES8311.
- **Dispositivos Móviles (App / APK Android) & PC:** Ejecución del mismo guión e interactividad, con escalabilidad automática a texturas en alta resolución, efectos visuales enriquecidos y compatibilidad bidireccional de contenidos.
- **Ecosistema Creativo Abierto:** Un formato de paquete de historia unificado (`.cbdstory`) que permite a autores, educadores y creadores de contenido diseñar y distribuir historias interactivas sin necesidad de recompilar el firmware del sistema operativo.

---

## 🎨 2. Paradigma de Animación: Marionetas 2D vs Video Convencional

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                   COMPARATIVA TÉCNICA: VIDEO VS MOTOR PARALLAX                   │
├───────────────────────────────┬──────────────────────────────────────────────────┤
│ Video Tradicional (Frame a Frame)│ Motor Parallax Procedural (CBDos Story Engine)  │
├───────────────────────────────┼──────────────────────────────────────────────────┤
│ ❌ 20 a 100 MB por minuto de video │ ✅ Menos de 500 KB por historia completa (assets)│
│ ❌ No interactivo (solo play/pause)│ ✅ Totalmente interactivo (táctil, giroscopio)  │
│ ❌ Decodificación pesada en CPU  │ ✅ Acelerado por hardware (DMA2D / LVGL 9.5)     │
│ ❌ Resolución fija (pixelado)    │ ✅ Escala vectorial independiente de resolución  │
│ ❌ Dificultad para traducir texto│ ✅ Textos y voces multi-idioma desacoplados     │
└───────────────────────────────┴──────────────────────────────────────────────────┘
```

### 2.1 Despiece de Personajes (Puppet / Cutout Rigging)
En lugar de secuencias de imágenes estáticas, los personajes y elementos del mundo se descomponen en piezas jerárquicas:
- **Cuerpo base / Torso**
- **Cabeza y expresiones** (Ojos abiertos/cerrados/guiño, Boca en fonemas o emociones)
- **Extremidades** (Brazos, piernas articuladas mediante puntos de pivote)
- **Accesorios** (Sombreros, herramientas, varitas)

A través de las APIs nativas de LVGL 9.5 (`lv_image_set_rotation`, `lv_image_set_scale`, `lv_image_set_pivot`, `lv_anim_t`), el motor interpola matemáticamente en tiempo real la posición, el ángulo y la escala con curvas de aceleración física (*elastic, bounce, ease-in-out*), logrando animaciones orgánicas y reactivas.

---

## 🏗️ 3. Arquitectura del Motor (Stack de Renderizado por Capas)

El motor divide el espacio visual en un árbol de capas con factores de profundidad independientes:

```
┌────────────────────────────────────────────────────────────────────────┐
│ [Capa 5] OVERLAY & UI (Factor 1.0x fijo)                               │
│ • Subtítulos tipográficos, globos de diálogo estilo cómic              │
│ • Botones de navegación de página (<  >), controles de audio/pausa     │
├────────────────────────────────────────────────────────────────────────┤
│ [Capa 4] FOREGROUND (Factor 1.2x - 1.4x)                               │
│ • Hojas de árboles en primer plano, hierba frontal, partículas de luz  │
├────────────────────────────────────────────────────────────────────────┤
│ [Capa 3] ACTION LAYER / PUPPETS (Factor 1.0x)                          │
│ • Personajes interactivos, objetos tocables, puertas, cofres mágicos   │
├────────────────────────────────────────────────────────────────────────┤
│ [Capa 2] MIDGROUND (Factor 0.5x)                                       │
│ • Casas de la aldea, árboles medianos, caminos, colinas cercanas       │
├────────────────────────────────────────────────────────────────────────┤
│ [Capa 1] BACKGROUND (Factor 0.2x)                                      │
│ • Montañas lejanas, nubes suaves, gradiente de cielo, sol/luna         │
└────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Cámara Virtual y Ecuación de Desplazamiento Parallax
La posición visible en pantalla de cada objeto $i$ perteneciente a una capa con factor de profundidad $K_{layer}$ se calcula mediante:

$$X_{render}(i) = X_{world}(i) - (Camera_X \times K_{layer})$$
$$Y_{render}(i) = Y_{world}(i) - (Camera_Y \times K_{layer})$$

La cámara virtual puede ser conducida por:
1. **Línea de Tiempo (Timeline):** Movimientos cinematográficos automáticos (*pans*, zooms).
2. **Entrada Táctil (Drag & Fling):** El usuario arrastra la escena con el dedo para explorar el entorno.
3. **Sensores Físicos (Giroscopio/Acelerómetro):** Efecto de profundidad holográfico según la inclinación del dispositivo.

---

## 📦 4. Formato Estándar de Contenido: Paquete `.cbdstory`

Una historia se empaqueta en una estructura de directorio o archivo comprimido `.cbdstory` en la tarjeta MicroSD (o almacenamiento interno en Android):

```
mi_cuento_interactivo.cbdstory/
├── manifest.json            # Metadatos (título, autor, idioma, targets compatibles)
├── story.json               # Línea de tiempo principal y grafo de escenas
├── assets/
│   ├── vectors/             # Vectores ThorVG (.tvg) o SVG optimizados
│   ├── sprites_low/         # Sprites optimizados para ESP32 (RGB565 / Indexed)
│   ├── sprites_high/        # Sprites HD para versión Android (PNG 32-bit con Alpha)
│   └── audio/               # Música de fondo y voces (MP3 / WAV comprimido)
└── scripts/ (Opcional)
    └── logic.lua            # Minijuegos o lógica condicional avanzada
```

### 4.1 Esquema de Guión Declarativo (`story.json`)
```json
{
  "story_title": "El Conejito de las Estrellas",
  "version": "1.0",
  "initial_scene": "bosque_nocturno",
  "scenes": {
    "bosque_nocturno": {
      "music": "audio/night_forest.mp3",
      "parallax_layers": [
        { "id": "sky", "asset": "vectors/night_sky.tvg", "depth": 0.1, "loop_x": true },
        { "id": "mountains", "asset": "vectors/mountains.tvg", "depth": 0.25 },
        { "id": "trees", "asset": "sprites_low/trees.bin", "depth": 0.6 },
        { "id": "ground", "asset": "sprites_low/ground.bin", "depth": 1.0 }
      ],
      "actors": [
        {
          "id": "rabbit",
          "root_asset": "actors/rabbit/",
          "pos": { "x": 120, "y": 380 },
          "interactive": true,
          "on_touch": { "play_sound": "audio/giggle.mp3", "play_anim": "jump_spin" }
        }
      ],
      "timeline": [
        { "time_ms": 0,    "type": "camera_pan", "to_x": 200, "duration_ms": 3000, "ease": "in_out" },
        { "time_ms": 1500, "type": "actor_anim", "actor": "rabbit", "anim": "wave_hand" },
        { "time_ms": 2500, "type": "dialog", "text": "¡Hola amiguito! ¿Puedes tocar la estrella brillante?", "voice": "audio/v_hello.mp3" },
        { "time_ms": 4000, "type": "wait_interaction", "target_actor": "rabbit", "timeout_ms": 10000 }
      ]
    }
  }
}
```

### 4.2 Presupuesto de Almacenamiento y Eficiencia (Storage Budget)
A diferencia de los videos tradicionales en MP4 (que pesan de 50 a 100 MB por 5 minutos y saturan el ancho de banda del microcontrolador), una historia o juego interactivo completo en el motor CBDos requiere únicamente unos pocos megabytes:

| Componente del Paquete | Contenido | Peso Promedio |
| :--- | :--- | :--- |
| **Gráficos Vectoriales (Lottie / ThorVG)** | 8 a 15 escenas, personajes y efectos | **~300 KB – 600 KB** |
| **Guión, Escenas y Lógica (`story.json` / Lua)** | Diálogos, grafo de decisiones y posiciones | **~30 KB – 50 KB** |
| **Música de Fondo (BGM)** | 2 pistas ambientales en loop (MP3 @ 96 kbps) | **~1.5 MB** |
| **Voces y Diálogos Narrados** | 20 a 30 frases grabadas en MP3 | **~1.5 MB** |
| **Efectos de Sonido (SFX)** | 10 a 15 efectos de sonido (WAV/PCM cortos) | **~300 KB** |
| **TOTAL POR CUENTO / JUEGO COMPLETO** | **Experiencia interactiva a 60 FPS** | 🏆 **~3.5 MB a 4.5 MB** |

> [!TIP]
> **Capacidad Masiva en MicroSD:** En una tarjeta MicroSD estándar de 8 GB a 16 GB, es posible almacenar una **biblioteca de más de 2,000 cuentos interactivos y minijuegos completos**, funcionando offline con latencia de carga inferior a 1 segundo.

---

## 📱 5. Estrategia Multiplataforma (CBDos & Android APK)

El diseño desacoplado garantiza que el mismo contenido creado por la comunidad funcione en ambos mundos con degradación/mejora elegante (*graceful enhancement*):

```
                                 ┌──────────────────────────────┐
                                 │   Paquete de Historia        │
                                 │      (.cbdstory)             │
                                 └──────────────┬───────────────┘
                                                │
                       ┌────────────────────────┴────────────────────────┐
                       ▼                                                 ▼
        ┌──────────────────────────────┐                  ┌──────────────────────────────┐
        │ Target ESP32-P4 / ESP32-S3   │                  │ Target Android App / APK     │
        ├──────────────────────────────┤                  ├──────────────────────────────┤
        │ • Render: LVGL 9.5 + ThorVG  │                  │ • Render: Flutter / Compose  │
        │ • Texturas: RGB565 en PSRAM  │                  │ • Texturas: RGBA 4K / Canvas │
        │ • Audio: ES8311 / Helix MP3  │                  │ • Audio: OpenSL / MediaPlayer│
        │ • CPU: RISC-V @ 400 MHz      │                  │ • Efectos: Shaders / Luces   │
        └──────────────────────────────┘                  └──────────────────────────────┘
```

1. **Gestión de Recursos Adaptativa:**
   - En **CBDos**, el cargador de assets selecciona la carpeta `sprites_low/` o interpreta primitivas vectoriales directamente hacia buffers de display MIPI-DPI vía DMA2D.
   - En **Android**, el motor companion selecciona `sprites_high/` y añade efectos de posprocesado (sombras suaves, partículas de brillo, físicas aceleradas por GPU).
2. **Cero Ruptura de Formato:**
   - La lógica del guión, las coordenadas normalizadas ($0.0 \dots 1.0$), las pausas interactivas y la sincronización de audio permanecen 100% idénticas entre plataformas.

---

## 🎯 6. Casos de Uso y Aplicaciones en el Ecosistema

### 6.1 Cuentos Educativos e Infantiles Interactivos
- Fomento a la lectura interactiva: los niños tocan las palabras para escuchar su pronunciación o tocan los personajes para desencadenar reacciones divertidas y sonidos.
- Portabilidad total en hardware dedicado offline (sin distracciones de internet ni anuncios).

### 6.2 Videojuegos 2D Livianos y Aventuras Gráficas Point & Click
- **Aventuras Gráficas e Historias Ramificadas:** Diálogos interactivos con opciones múltiples, recolección de pistas e inventario de objetos en pantalla.
- **Minijuegos Educativos y de Habilidad:** Juegos de memoria con cartas vectoriales animadas (*Memory Match*), encontrar las diferencias en escenas parallax vivas, puzzles de lógica y juegos de ritmo musical sincronizados con el reproductor de audio.

### 6.3 Guías de Onboarding y Tutoriales Animados del Sistema
- Tutorial interactivo al encender CBDos por primera vez: un asistente animado guía al usuario paso a paso señalando físicamente la pantalla para explicar la navegación táctil, configuración de Wi-Fi y uso de herramientas.

### 6.4 Mascotas Virtuales y Asistentes de Escritorio
- Personajes reactivos integrados como widgets o fondos de pantalla animados (Live Wallpapers) que reaccionan al estado de la batería, hora del día o eventos del sistema.

---

## 🗓️ 7. Hoja de Ruta para su Implementación en CBDos

- [ ] **Fase Concept & Tooling:** Definición formal del parser JSON y validación del renderizador ThorVG/LVGL 9.5 en `core/`.
- [ ] **Fase Prototipo P4:** Implementación de la clase base `ParallaxSceneView` en `core/ui/views/` con 4 capas de scroll independiente.
- [ ] **Fase Timeline & Audio Sync:** Integración del secuenciador de eventos en el tiempo enlazado a `IAudioSink`.
- [ ] **Fase SDK y Reproductor Android:** Publicación del visor multiplataforma para creadores de contenido.
