# Registro de Decisión Arquitectónica (ADR): Sincronización de Audio, Interactividad y Localización Multi-Idioma

**Estado:** 📐 Aprobado / Especificación de Referencia  
**Módulo:** `core/engine/story/` & `IAudioSink`  
**Autor:** CBDos Architecture Core Team  
**Fecha:** Agosto 2026  
**Documento Relacionado:** [`motor_animacion_parallax_y_narrativa_interactiva_spec.md`](file:///home/kaber420/Documentos/proyectos/cbdos/docs/architecture/motor_animacion_parallax_y_narrativa_interactiva_spec.md)

---

## 🎯 1. Contexto y Problema de Diseño

En el desarrollo del motor de cuentos y entretenimiento interactivo para **CBDos** (y su contraparte en **Android**), la narrativa no sigue una línea de tiempo fija o pasiva como una película de video. Al tratarse de un entorno **interactivo táctil para niños y usuarios**, el avance de la historia depende directamente de las acciones del usuario:
- El niño puede detenerse minutos explorando una escena o tocando elementos reactivos.
- Los diálogos y efectos sonoros deben dispararse en el **milisegundo exacto** en que ocurre una acción o animación.

Surge la disyuntiva técnica sobre cómo estructurar el subsistema de audio:
1. **Opción A (Audio Sprite Monolítico):** Un único archivo MP3 largo por capítulo con una tabla de marcas de tiempo (*offsets* de inicio y fin).
2. **Opción B (Clips Modulares Desacoplados por Evento):** Múltiples pistas de audio individuales organizadas por tipo (música ambiental, diálogos, efectos sonoros y carpetas de idioma).

---

## 🔬 2. Análisis Comparativo y Retos Técnicos en Microcontroladores

| Criterio Técnico | Opción A: Audio Sprite Monolítico (`single.mp3`) | Opción B: Clips Modulares Desacoplados (`tracks/*.mp3`) |
| :--- | :--- | :--- |
| **Latencia de Disparo (Trigger Delay)** | ⚠️ **20 a 60 ms de latencia**. Saltar a un punto arbitrario requiere buscar el *frame header* de MP3 y purgar el decodificador Helix. | 🚀 **Inmediata (< 5 ms)**. El decodificador arranca directamente desde el byte 0 del archivo. |
| **Compatibilidad con Tasa Variable (VBR)** | ❌ **Inviable**. En MP3 VBR no existe relación matemática lineal entre bytes y milisegundos. Requiere forzar CBR. | ✅ **100% Compatible**. Soporta cualquier formato, tasa o compresión sin importar si es VBR o CBR. |
| **Música de Fondo Simultánea (BGM + SFX)** | ❌ **Imposible**. Si la música y los diálogos están en el mismo archivo, pausar el diálogo corta la música de fondo. | ✅ **Nativo**. Permite tener música ambiental en loop continuo mientras los diálogos se activan bajo demanda. |
| **Soporte Multi-Idioma (i18n)** | ❌ **Pésimo**. Traducir una historia requiere volver a grabar y reexportar un archivo maestro de 10 minutos completo. | 🚀 **Excepcional**. Solo se reemplazan los clips de voz de la carpeta del idioma correspondiente. |
| **Consumo de Almacenamiento Flash / SD** | ⚖️ Idéntico (1 archivo de 1 MB). | ⚖️ Idéntico (10 archivos de 100 KB = 1 MB total). |

---

## 🏆 3. Decisión de Diseño Adoptada (ADR)

Se adopta oficialmente la **Opción B: Clips Modulares Desacoplados por Evento y Canal**, estructurados bajo un sistema de capas de audio:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        CANAL 1: BGM (Background Music)                 │
│ • Música instrumental ambiental en loop suave (MP3 64-128 kbps)        │
├────────────────────────────────────────────────────────────────────────┤
│                        CANAL 2: VOICE (Narración / Diálogos)           │
│ • Activado por eventos de timeline o toques (MP3 / WAV comprimido)     │
│ • Ducking automático: Atenúa el Canal 1 un 40% mientras suena la voz   │
├────────────────────────────────────────────────────────────────────────┤
│                        CANAL 3: SFX (Efectos Inmediatos)               │
│ • Risas, saltos, campanas, reacciones táctiles (WAV / PCM en PSRAM)    │
│ • Cero latencia (0 ms de respuesta al tacto)                          │
└────────────────────────────────────────────────────────────────────────┘
```

### Justificación de la Elección:
1. **Naturaleza No Lineal:** Permite que el niño interactúe libremente sin romper la continuidad de la música de fondo.
2. **Cero Complejidad de Búsqueda:** Elimina la necesidad de algoritmos de indexación de *frames* MP3 en tiempo de ejecución en el ESP32.
3. **Máxima Flexibilidad para Creadores:** Cualquier persona puede añadir efectos de sonido o corregir una frase específica sin programas de edición de audio complejos.

---

## 🌐 4. Sistema de Localización Multi-Idioma Dinámico (i18n / l10n)

El sistema separa estrictamente los recursos visuales y efectos universales de las pistas de voz y textos dependientes del idioma.

### 4.1 Estructura Estándar del Paquete de Cuento
```
mi_cuento.cbdstory/
├── manifest.json
├── story.json               <-- Estructura de escenas y textos multi-idioma
├── assets/
│   ├── sprites/             <-- Gráficos universales
│   └── sfx/                 <-- Sonidos universales (risas, magia, pasos)
└── audio/
    ├── bgm/
    │   └── forest_loop.mp3  <-- Música común
    └── voices/
        ├── es/              <-- Español: d01_intro.mp3, d02_estrella.mp3
        ├── en/              <-- Inglés:  d01_intro.mp3, d02_estrella.mp3
        └── fr/              <-- Francés: d01_intro.mp3, d02_estrella.mp3
```

### 4.2 Esquema Declarativo Multi-Idioma (`story.json`)
El motor evalúa la variable de sistema `{lang}` en caliente:

```json
{
  "scene_id": "bosque_encantado",
  "actors": [
    {
      "id": "star_button",
      "on_touch": {
        "sfx": "assets/sfx/twinkle.wav",
        "voice": "audio/voices/{lang}/d02_estrella.mp3",
        "dialog": {
          "es": "¡Encontraste la estrella brillante!",
          "en": "You found the shining star!",
          "fr": "Tu as trouvé l'étoile brillante!"
        }
      }
    }
  ]
}
```

---

## 🎓 5. Impacto Pedagógico y Beneficios para la Comunidad

1. **Aprendizaje Bilingüe en Vivo:**
   - La interfaz puede incluir un selector de idioma visible. Un niño o profesor puede escuchar una frase en español y, con un solo toque, volver a escucharla en inglés para practicar pronunciación y vocabulario.
2. **Accesibilidad y Lenguas Regionales:**
   - Comunidades locales o educadores pueden traducir cuentos a lenguas indígenas o dialectos regionales simplemente grabando notas de voz y creando una carpeta `audio/voices/qu/` (Quechua), `audio/voices/nah/` (Náhuatl), etc., sin necesidad de conocimientos de programación.
3. **Compatibilidad Total con Android / APK:**
   - La misma estructura de carpetas y JSON es leída directamente por la app en smartphones o tablets, garantizando un ecosistema de contenido unificado.
