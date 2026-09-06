# Especificación Técnica: Motor de Mascota Virtual Lottie (Cyber-Pet Engine) para CBDos

Este documento detalla la arquitectura, máquina de estados, integración con **LVGL 9.5 (ThorVG)** y el flujo de diseño/edición de animaciones vectoriales **Lottie (.json)** para la mascota virtual de **CBDos**.

---

## 🎯 1. Visión General y Objetivos
* **Experiencia de Usuario Cyberdeck:** Proporcionar un centinela/compañero animado fluido (a 60 FPS) que dé vida a la interfaz de usuario, reaccionando en tiempo real al tráfico LoRa (MeshCore), reproducción de música, nivel de batería y eventos del sistema.
* **Flexibilidad de Personalización (Skins):** La mascota no está grabada a fuego en el código fuente; se carga como un conjunto de archivos JSON vectoriales desde `/sdcard/pet/active/`, permitiendo cambiar de mascota o crear temas personalizados simplemente arrastrando archivos a la tarjeta MicroSD.
* **Eficiencia de Recursos:** Uso del motor **ThorVG** integrado en LVGL 9.5, aprovechando la abundante memoria PSRAM (32 MB en ESP32-P4 / 8 MB en ESP32-S3) con un consumo de almacenamiento flash/SD mínimo (archivos de 10 a 40 KB).

---

## 🧠 2. Máquina de Estados y Eventos del Sistema

La mascota opera como una máquina de estados desacoplada en `core/src/services/pet/` que se suscribe a los eventos del sistema:

```
                                  ┌────────────────────────────────┐
                                  │      EVENTOS DEL SISTEMA       │
                                  ├────────────────────────────────┤
                                  │ • MeshCore (Mensaje LoRa P2P)  │
                                  │ • Reproductor Audio (MP3 I2S)  │
                                  │ • Batería / Alimentación       │
                                  │ • Inactividad / Sleep          │
                                  │ • IDS / Defensive Monitor      │
                                  └───────────────┬────────────────┘
                                                  │
                                                  ▼
                                 ┌─────────────────────────────────┐
                                 │       CyberPetManager (C++)     │
                                 │      (Máquina de Estados)       │
                                 └────────────────┬────────────────┘
                                                  │
                ┌───────────────────┬─────────────┴───────┬───────────────────┐
                ▼                   ▼                     ▼                   ▼
        [ PetState::IDLE ]   [ PetState::ALERT ]   [ PetState::DANCE ]  [ PetState::SLEEP ]
          "idle.json"          "alert.json"          "dance.json"         "sleep.json"
          (Bucle suave)      (1 disparo + burbuja)   (Sincronizado MP3)   (Ahorro energía)
```

### Tabla de Estados y Comportamientos

| Estado (`enum PetState`) | Archivo Lottie | Disparador / Evento | Comportamiento en Pantalla |
| :--- | :--- | :--- | :--- |
| **`IDLE`** *(Por defecto)* | `idle.json` | Sin actividad especial | Animación en bucle suave (respiración, parpadeo de ojos, escaneo ligero). |
| **`ALERT_MSG`** | `alert.json` | Mensaje LoRa recibido en MeshCore | Salto / cara de sorpresa + burbuja de diálogo (*"¡Mensaje de 0xXXXX!"*). |
| **`HAPPY_ACK`** | `happy.json` | Mensaje enviado confirmado con ACK `✓✓` | Animación de celebración / guiño de ojos. |
| **`MUSIC_PLAYING`** | `dance.json` | Audio Player activo en segundo plano | Mascota con audífonos o bailando al ritmo del audio. |
| **`DEFENSIVE_WARN`** | `warning.json` | Alerta de IDS (Deauth / Jamming) | Modo centinela con escudo / aura roja y pitido por ES8311. |
| **`LOW_BATTERY`** | `sleepy.json` | Batería < 20% | Bostezos, ojos entrecerrados o animación de recarga necesaria. |
| **`SLEEPING`** | `sleep.json` | Inactividad prolongada (>2 min) | Durmiendo plácidamente con efecto `Zzz`. |

---

## 🛠️ 3. Implementación Técnica en LVGL 9.5 (C++)

### Integración de ThorVG / Lottie
En LVGL 9.5, el soporte de Lottie utiliza el motor vectorial ThorVG:

```cpp
// Ejemplo de controlador UI para el widget de la mascota
class CyberPetWidget {
public:
    void init(lv_obj_t* parent, int width, int height) {
        // Crear el objeto Lottie nativo de LVGL 9.5
        m_lottieObj = lv_lottie_create(parent);
        lv_obj_set_size(m_lottieObj, width, height);
        
        // Asignar buffer dinámico en PSRAM para la rasterización de ThorVG
        m_renderBuffer = heap_caps_malloc(width * height * 4, MALLOC_CAP_SPIRAM);
        lv_lottie_set_buffer(m_lottieObj, width, height, m_renderBuffer);
        
        // Cargar estado inicial
        loadState(PetState::IDLE);
    }

    void loadState(PetState state) {
        const char* path = getPathForState(state);
        // Carga directa del archivo JSON vectorial desde MicroSD
        lv_lottie_set_src_file(m_lottieObj, path);
    }

private:
    lv_obj_t* m_lottieObj = nullptr;
    void* m_renderBuffer = nullptr;
};
```

---

## 🎨 4. Flujo de Trabajo y Herramientas para Diseñar/Editar Animaciones Lottie

Crear animaciones vectoriales desde cero puede llevar mucho tiempo, pero existen flujos modernos y rápidos para obtener y adaptar mascotas sin esfuerzo:

### A. Dónde conseguir animaciones base (Gratis / Open Source)
1. **[LottieFiles.com](https://lottiefiles.com):** La biblioteca más grande del mundo. Buscando términos como *"cyberpunk bot"*, *"robot pet"*, *"tamagotchi"*, *"dragon"*, *"cat mascot"*, *"glitch face"*, hay miles de animaciones gratuitas bajo licencia comercial/abierta.
2. **[IconScout](https://iconscout.com/lotties) & [Lordicon](https://lordicon.com):** Paquetes de iconos y personajes interactivos en formato JSON.

### B. Herramientas Sencillas para Editar Lottie (Sin usar After Effects)
* **Editor Web de LottieFiles (Gratis en navegador):** Permite cambiar colores (ej. ponerle a la mascota la paleta verde Matrix o Cyan cyberpunk de CBDos), ocultar capas, cambiar la velocidad o recortar la duración sin instalar ningún software.
* **Figma + Lottie Plugin:** Se puede dibujar el personaje en vectores en Figma y exportarlo directamente a `.json` con el plugin de Lottie.
* **Modificación directa por JSON:** Al ser texto plano JSON, parámetros globales como colores hexadecimales (`#00FFFF`, `#FF0055`) se pueden reemplazar con un simple *Buscar y Reemplazar*.

---

## 📂 5. Estructura de Archivos en la Tarjeta MicroSD

Para que el usuario pueda instalar diferentes temas o mascotas:

```text
/sdcard/
 └── pet/
      ├── active -> skin_cyber_bot/       (Enlace o selección activa)
      │
      ├── skin_cyber_bot/                 (Pack de mascota Robot)
      │    ├── manifest.json              (Metadatos: Nombre, Creador, Versión)
      │    ├── idle.json
      │    ├── alert.json
      │    ├── happy.json
      │    ├── dance.json
      │    ├── warning.json
      │    └── sleep.json
      │
      └── skin_retro_gotchi/              (Pack alternativo Tamagotchi Pixel)
           ├── manifest.json
           ├── idle.json
           └── ...
```

---

## 🚀 6. Fases de Implementación en CBDos

1. **Fase 1 (HAL & Core):** Activar `LV_USE_THORVG = 1` y `LV_USE_LOTTIE = 1` en la configuración de LVGL 9.5 con asignación de memoria en PSRAM.
2. **Fase 2 (Servicio C++):** Desarrollar `CyberPetManager` en `core/src/services/pet/` con la máquina de estados y suscripción a eventos de MeshCore y Audio.
3. **Fase 3 (UI Widget & Diálogos):** Crear el componente `CyberPetWidget` con la burbuja de diálogo flotante (*Speech Bubble*) para mostrar mensajes de texto rápidos.
4. **Fase 4 (Pack de Animaciones Inicial):** Empaquetar un pack básico de 5 animaciones Lottie (`idle`, `alert`, `dance`, `warning`, `sleep`) probado en el simulador y cargado desde MicroSD.
