# Propuesta de Arquitectura: API de Wallpapers Dinámicos y Effect SDK (v0.1.0)

## 📌 1. Visión General y Objetivos
CBDos v0.2.1 ha demostrado la potencia de su motor de renderizado vectorial nativo sobre **LVGL 9.5** a **60 FPS** (alineado con VSYNC MIPI-DSI en ESP32-P4 y QSPI en ESP32-S3), a través de fondos dinámicos como *Constelación Neón*, *Ondas Senoidales*, *Carrito Cómic 2D (Vocho Retro en Parallax)* y *Luciérnagas en el Bosque Vectorial*.

Esta especificación establece las bases para evolucionar el subsistema actual hacia una **API de Wallpapers y Effect SDK**, permitiendo a cualquier desarrollador o usuario crear, personalizar y empaquetar fondos dinámicos vectoriales únicos mediante componentes reutilizables (*Building Blocks*), sin necesidad de recompilar la lógica del sistema.

---

## 🧩 2. Arquitectura de la API de Efectos y Primitivas Vectoriales

En lugar de requerir animaciones monolíticas o estáticas, la futura API proporcionará un catálogo de **Drivers de Efectos y Físicas (*Effect Drivers*)** empaquetados y optimizados en C++:

### A. Drivers de Física y Movimiento
* **`ParticleEmitterDriver`**: Emisor de partículas con masa, gravedad, fricción, deriva orgánica, aceleración y rebote suave en bordes.
* **`NodeLinkerDriver`**: Motor de vinculación que calcula distancias euclidianas entre nodos cercanos y traza redes de tensión o constelaciones translucidas.
* **`SineWaveRibbonDriver`**: Generador de curvas senoidales, cuerdas vibrantes, ondas marinas o latiguillos vectoriales.
* **`ParallaxLayerDriver`**: Gestor de scroll horizontal multicapa con velocidades independientes para fondos lejanos (montañas/luna), medios (casitas/cactus) y frontales (carreteras/suelos).
* **`Vehicle2DDriver`**: Controlador de vehículos 2D con suspensión oscilatoria, ruedas giratorias con rines vectoriales y conos de iluminación.
* **`LightPulsarDriver`**: Modulador de opacidad y aura luminoso (*fade in / fade out*) con osciladores senoidales para luciérnagas, faros o neón.

### B. Drivers de Integración de Sistema y Audio (Atmósfera)
* **`SoundAtmosphereBridge`**: Permite vincular el fondo dinámico a la reproducción opcional de pistas ambientales de ruido blanco, lluvia o grillos nocturnos mediante la capa de audio `IAudioSink` (ES8311 / I2S).
* **`AudioReactiveDriver`**: Permite que las partículas, ondas o luces respondan en tiempo real al espectro de frecuencias o volumen del reproductor de música de CBDos.

---

## 📄 3. Formato del Paquete de Wallpaper (`.cbdwall`)

Los fondos personalizados se empaquetarán en un formato de contenedor ligero para la tarjeta MicroSD o la memoria Flash interna (`/wallpapers/` en LittleFS):

```
+-----------------------------------------------------------------------+
|  [HEADER] Firma Mágica "CBDW" + Versión de Especificación             |
+-----------------------------------------------------------------------+
|  [METADATA - MessagePack] (cbdos::msgpack::MsgPackReader)             |
|  - Título, Autor, Versión y Descripción del Fondo                     |
|  - Colección de Capas y Componentes (Listas de Effect Drivers)        |
|  - Paleta de Colores Vectoriales (Hex / HSL)                          |
|  - Parámetros de Física (Gravedad, Velocidades de Scroll, Densidad)    |
|  - Configuración de Audio Ambiental Opcional (Ruido Blanco/Lluvia)     |
+-----------------------------------------------------------------------+
|  [PAYLOAD] (Opcional) Sprites Vectoriales, Formas o Audio PCM/Helix   |
+-----------------------------------------------------------------------+
```

---

## 🎨 4. Futuro Editor de Wallpapers e Integración con SDK UI

A futuro, CBDos incorporará:
1. **Editor Gráfico en Dispositivo / Web SDK**: Una herramienta visual (aplicación nativa en CBDos o editor web) para añadir capas, ajustar colores neón, variar densidades de partículas y previsualizar la escena 2D en tiempo real.
2. **Estilización de Interfaz**: Capacidad de sincronizar el tema de color global de CBDos (`DefaultTheme`) con la paleta de colores del wallpaper animado activo.

---

## 🛠️ 5. Hoja de Ruta de Implementación (*Roadmap*)
- [x] **Fase 1 (v0.2.1 - Completada):** Renderizado vectorial nativo a 60 FPS (`Constellation`, `Waves`, `ComicDrive-Vocho`, `Fireflies`).
- [ ] **Fase 2:** Abstracción de la interfaz `IWallpaperEffect` y registro dinámico de `EffectDrivers` en C++.
- [ ] **Fase 3:** Integración del parser de paquetes `.cbdwall` usando `MsgPackReader` en `WallpaperManager`.
- [ ] **Fase 4:** Creación de la aplicación de usuario "Editor de Wallpapers" y soporte para audio ambiental en fondos.
