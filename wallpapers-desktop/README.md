# CBDOS Wallpapers de Escritorio

Wallpapers animados interactivos recreados desde los efectos visuales de CBDOS (CyBerDeck OS).

Basados en el motor `AnimatedWallpaper` del proyecto CBDOS, adaptados para navegadores web con HTML5 Canvas.

## Wallpapers Disponibles

| Archivo | Efecto CBDOS Original | Interactividad |
|---|---|---|
| `fireflies.html` | Luciernagas del Bosque | Ninguna (ambiente) |
| `touch-swarm.html` | Particulas Touch Magnet | Mouse atrae particulas, lineas de conexion, anillos pulsantes |
| `matrix-rain.html` | Matrix Code Rain | Mouse acelera la lluvia de caracteres cercana |
| `constellation.html` | Constelacion Neon | Particulas flotantes con lineas de conexion |
| `synthwave.html` | Synthwave 80s Grid | Grid perspectiva 3D animada |
| `waves.html` | Ondas Neon | Ondas senoidales animadas |

## Como Usar

### Opcion 1: Navegador en pantalla completa (cualquier SO)

1. Abre el archivo `.html` en tu navegador (Chrome, Firefox, Edge)
2. Presiona `F11` para entrar en pantalla completa
3. Listo, tienes tu wallpaper animado

### Opcion 2: Lively Wallpaper (Windows, gratis)

1. Descarga [Lively Wallpaper](https://rocksdanister.github.io/lively/) (gratuito y open source)
2. Instala y abre Lively
3. Arrastra el archivo `.html` directamente a la ventana de Lively
4. Selecciona "Monitor" para ponerlo como fondo de escritorio

### Opcion 3: Wallpaper Engine (Windows, Steam)

1. Compra/descarga [Wallpaper Engine](https://store.steampowered.com/app/431960/) en Steam
2. En la pestaña "Discover", haz clic en "Create Wallpaper"
3. Selecciona "Web" como tipo
4. Apunta al archivo `.html` local
5. Ajusta FPS y calidad segun tu hardware

### Opcion 4: Mac / Linux

- **Mac:** Usa [Wallpaper Wizard 2](https://www.fliqlo.com/) o simplemente abre en navegador + F11
- **Linux:** Usa [Komorebi](https://github.com/cheesecakeufo/komorebi) o [Wallpaper Engine para Linux](https://github.com/Almamu/linux-wallpaperengine)

## Sobre la Legalidad

- **Matrix Code Rain:** El efecto visual de "lluvia de caracteres digitales" es un concepto generico. No esta patentado. Solo el nombre "Matrix" es marca registrada de Warner Bros., pero el efecto visual en si no tiene restricciones legales.
- **Todos los demas:** Son efectos matematicos/visuales genericos (particulas, ondas, constelaciones). Sin restricciones.

## Rendimiento

Estos wallpapers usan HTML5 Canvas y son muy ligeros:
- ~2-5% de CPU en idle
- ~10-15% de CPU con interaccion activa del mouse
- Sin uso de GPU dedicada (renderizado por software)
- Funcionan en cualquier dispositivo con navegador moderno

## Personalizacion

Puedes modificar los parametros directamente en el codigo HTML:
- `PARTICLE_COUNT`: Numero de particulas
- `CONNECTION_DIST`: Distancia maxima para lineas de conexion
- Colores en hex (busca los valores `rgba` o `#hex` en el codigo)
- Velocidades de animacion (multiplicadores en `update()`)
