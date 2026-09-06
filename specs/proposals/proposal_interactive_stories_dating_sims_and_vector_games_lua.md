# Propuesta de Arquitectura: Motor de Historias Interactivas, Novelas Visuales / Dating Sims y Juegos Vectoriales en Lua + ThorVG (v0.1.0)

**Estado:** 💡 Propuesta de Arquitectura & Ecosistema de Contenido  
**Target:** CBDos (ESP32-P4 / ESP32-S3)  
**Subsistemas:** Engine Lua 5.4, LVGL 9.5, ThorVG / Lottie, Audio Core (Helix/I2S), MicroSD Storage  
**Ubicación Oficial:** `docs/proposals/proposal_interactive_stories_dating_sims_and_vector_games_lua.md`  

---

## 1. Visión y Objetivos

El objetivo de esta propuesta es dotar a **CBDos** de un **motor de ejecución de contenido interactivo ligero y scriptable en Lua**, permitiendo a creadores, diseñadores y usuarios desarrollar y jugar:

1. **Novelas Visuales y Juegos de Citas / Dating Sims (Estilo Clásico Japonés / Ren'Py):**
   * Historias interactivas ramificadas donde el jugador interactúa con personajes, toma decisiones críticas, gestiona niveles de afinidad/afecto y desbloquea múltiples finales.
   * Ilustraciones y personajes con animaciones vectoriales fluidas (respiración, expresiones de ojos/boca con Lottie, poses dinámicas) sin pixelado en pantalla 480x800.
2. **Juegos Vectoriales Estilo Gamby / Playdate / PICO-8:**
   * Minijuegos arcade 2D, puzzles, mascotas virtuales interactivas (Cyber-Pet/Tamagotchi) y aventuras point & click.
3. **Flujo de Creación Cruzada (Android, PC, Tablet a MicroSD):**
   * Diseñar animaciones y assets en herramientas estándar (After Effects, LottieFiles, Figma, Canva, Inkscape), escribir la lógica en Lua desde cualquier teléfono o PC y ejecutarlo en CBDos simplemente copiando una carpeta a la MicroSD, **sin necesidad de compilar C++ ni instalar entornos de desarrollo**.

---

## 2. Arquitectura del Motor (CBDos Interactive Story & Game Engine)

El motor opera como una aplicación nativa de `core/` escrita en C++ que interpreta scripts en Lua y orquesta los subsistemas multimedia de bajo nivel:

```
┌──────────────────────────────────────────────────────────────────────────┐
│             PAQUETE DE JUEGO / HISTORIA EN MICROSD (/sd/games/...)       │
│  ├── main.lua                (Lógica, diálogos, toma de decisiones)     │
│  ├── assets/                                                             │
│  │   ├── characters/         (Animaciones Lottie de personajes / poses) │
│  │   ├── backgrounds/        (Fondos vectoriales SVG / TVG / Lottie)    │
│  │   ├── sounds/             (Efectos de sonido .wav)                   │
│  │   └── music/              (BGM .mp3 para cada escena)                │
└──────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                   MOTOR NATIVO C++ EN CBDOS (CORE)                       │
├──────────────────────────────────────────────────────────────────────────┤
│ 1. Lua Runtime & State Manager (Gestión de variables, afinidad y ramas)  │
│ 2. Capa Visual LVGL 9.5 + ThorVG:                                        │
│    • Capa 0 (Fondo): Render vectorial escalado a resolución nativa      │
│    • Capa 1 (Personajes): Sprites Lottie con cambio de animación/estado  │
│    • Capa 2 (UI / Diálogo): Caja de texto con efecto máquina de escribir│
│    • Capa 3 (Opciones): Modal táctil de selección de rutas               │
│ 3. Capa de Audio: Background Music (Helix MP3) + Sound FX (AudioSink)    │
│ 4. Sistema de Guardado: Persistencia de progreso en MicroSD (.save)     │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Capas Gráficas y Composición de Escena

Para novelas visuales y juegos de citas, el motor divide la pantalla en 4 capas LVGL independientes:

```
┌────────────────────────────────────────────────────────┐
│ [Capa 0: Fondo Vectorial / Escenario]                  │
│                                                        │
│             ┌─────────────────────────┐                │
│             │ [Capa 1: Personaje]     │                │
│             │ Animación Lottie:       │                │
│             │ • Expresión: Sonrojada  │                │
│             │ • Pose: Saludo          │                │
│             └─────────────────────────┘                │
│                                                        │
│ ┌────────────────────────────────────────────────────┐ │
│ │ [Capa 2: Cuadro de Diálogo Translúcido LVGL 9]      │ │
│ │ Sakura: "¿Te gustaría caminar juntos después de    │ │
│ │ clase?"                                            │ │
│ └────────────────────────────────────────────────────┘ │
│ ┌────────────────────────────────────────────────────┐ │
│ │ [Capa 3: Botones de Decisión Táctiles]             │ │
│ │ [ 1. ¡Claro que sí! ]   [ 2. Tengo club de kendo ] │ │
│ └────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────┘
```

---

## 4. Especificación de la API Lua (`story.*` / `game.*`)

La API expuesta a los scripts Lua está diseñada para ser ultra-legible, permitiendo a cualquier entusiasta escribir historias complejas con pocas líneas:

### 4.1 Ejemplo de Novela Visual / Dating Sim (`main.lua`)

```lua
-- Inicialización de la historia
story.title("Tardes de Cerezo")
story.author("Kaber420")

function main()
    -- Cargar fondo y música ambiental
    story.background("assets/backgrounds/escuela_tarde.json")
    audio.playBGM("assets/music/romance_theme.mp3", { loop = true, volume = 80 })

    -- Presentar personaje con animación Lottie
    story.showCharacter("sakura", "assets/characters/sakura_idle.json", { position = "center" })
    story.dialog("Sakura", "Hola... no esperaba encontrarte aquí en el pasillo a esta hora.")

    -- Cambio dinámico de expresión vectorial
    story.characterExpression("sakura", "assets/characters/sakura_shy.json")
    story.dialog("Sakura", "¿Te gustaría que regresemos caminando juntos a casa?")

    -- Menú de decisiones interactivo
    local decision = story.choose({
        "1. ¡Me encantaría, vamos!",
        "2. Lo siento, hoy debo entrenar en el dojo.",
        "3. (Invitarla a comer un helado primero)"
    })

    if decision == 1 then
        story.affinity.add("sakura", 10)
        audio.playSFX("assets/sounds/heart_up.wav")
        story.characterExpression("sakura", "assets/characters/sakura_happy.json")
        story.dialog("Sakura", "¡Genial! Déjame recoger mi mochila y nos vamos.")
        story.jumpTo("ruta_camino_casa")

    elseif decision == 2 then
        story.affinity.sub("sakura", 5)
        story.characterExpression("sakura", "assets/characters/sakura_sad.json")
        story.dialog("Sakura", "Oh... entiendo. Suerte con tu entrenamiento...")
        story.jumpTo("ruta_entrenamiento")

    elseif decision == 3 then
        story.affinity.add("sakura", 25)
        audio.playSFX("assets/sounds/super_bonus.wav")
        story.characterExpression("sakura", "assets/characters/sakura_blush.json")
        story.dialog("Sakura", "¿¡Un helado!? ¡Sí, conozco un lugar increíble cerca de la estación!")
        story.jumpTo("ruta_cita_helado")
    end
end
```

### 4.2 Ejemplo de Juego Vectorial Arcade / Gamby (`game.lua`)

```lua
-- Juego estilo Gamby / Arcade 2D
local player = { x = 240, y = 700, score = 0 }

function game.init()
    display.setFPS(60)
    game.loadVectorSprite("ship", "assets/player_ship.json")
    game.loadVectorSprite("meteor", "assets/meteor.json")
end

function game.update(dt)
    -- Lectura de toques en pantalla
    local touch = input.getTouch()
    if touch.isPressed then
        player.x = touch.x
    end
    
    -- Mover y verificar colisiones
    if game.checkCollision("ship", "meteor") then
        audio.playSFX("assets/sounds/explosion.wav")
        game.gameOver()
    end
end
```

---

## 5. Ventajas Técnicas y Ecosistema para CBDos

| Característica | Beneficio para el Usuario / Ecosistema |
| :--- | :--- |
| **Gráficos Vectoriales (Lottie / SVG / TVG)** | Cero pixelado en pantallas HD (480x800). Los assets pesan kilobytes en lugar de megabytes. |
| **Motor Lua Sandbox** | Si un script tiene un error lógico, el juego se cierra limpiamente sin provocar *kernel panic* ni reiniciar el ESP32. |
| **Almacenamiento en MicroSD** | Cientos de juegos e historias pueden compartirse como archivos comprimidos o carpetas y ejecutarse de inmediato. |
| **Audio I2S y Decodificación Helix** | Banda sonora en estéreo con calidad de CD (MP3 44.1 kHz) y efectos SFX simultáneos mediante el pipeline de audio de CBDos. |
| **Cero Toolchains para Creadores** | Creación accesible desde Android, iOS, Windows, Mac o Linux mediante editores de texto estándar. |

---

## 6. Plan de Implementación Futuro (Post-Refactorización Fase 2)

1. **Hito 1 (Pruebas de Concepto ThorVG / Lottie):** Validar la tasa de cuadros (FPS) y consumo de PSRAM de widgets `lv_lottie` con animación continua en ESP32-P4 y ESP32-S3.
2. **Hito 2 (Binding Lua-LVGL):** Crear la interfaz de enlace C++ $\leftrightarrow$ Lua para controlar widgets de diálogo, botones y animaciones.
3. **Hito 3 (Story App Launcher):** Integrar la aplicación "CBD Stories & Games" en el menú principal de CBDos para explorar y ejecutar proyectos desde `/sdcard/games/`.
