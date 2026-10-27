# Plan de Arquitectura e Implementación: Jerarquía de Niveles OmniWISP (Ciudad / Zona -> PoP -> Radio AP -> CPE)

**Módulo:** `cbdos::omniwisp` (`core/src/ui/views/omniwisp/`)  
**Fecha:** 26 de Octubre de 2026  
**Dispositivo Primario:** ESP32-S3 (JC3248W535, 320x480) y ESP32-P4 (JC4880, 480x800)

---

## 1. Reglas Fundamentales y Terminología

1. **Erradicación de la palabra "Torre":**
   - Se elimina la palabra "Torre" y "Torres" de toda la interfaz, modales, etiquetas y opciones.
   - La jerarquía oficial de la red WISP es:
     - **Nivel 1 (Raíz):** **Ciudad / Zona** (Agrupador geográfico principal, `parentId = ""`).
     - **Nivel 2 (Sitio):** **PoP** (Punto de Presencia físico dentro de la Ciudad/Zona, `parentId = ciudad.id`).
     - **Nivel 3 (Radio):** **Radio AP** (Sector/Antena montado en el PoP, `groupId = pop.id`).
     - **Nivel 4 (Cliente):** **CPE** (Estaciones conectadas al Radio AP).

2. **Cero Destrucción de Código Previsto:**
   - Aprovechar el soporte nativo existente de `parentId` en `NetworkGroup` y en `VaultService` (`ubnt_groups`).
   - No ocultar nada: todos los niveles son creables y navegables desde la UI.

---

## 2. Navegación y UI Optimizada para Pantalla de 320 px (JC3248)

En la pantalla de 320 px de ancho, la barra superior debe permitir filtrar la jerarquía sin saturar el espacio horizontal:

### 2.1. Barra Superior Compacta (TopBar)
Dos dropdowns optimizados:

1. **Dropdown de Filtro de Contexto / Jerarquía (175 px):**
   - Permite seleccionar el alcance de visualización.
   - Formato jerárquico claro:
     - `Todos los PoPs` (Muestra todos los APs sin filtro).
     - Si hay Ciudades/Zonas y PoPs:
       - `[Ciudad]` (Filtra todos los PoPs y APs de esa ciudad).
       - `  └ [PoP]` (Filtra solo los APs de ese PoP específico).

2. **Dropdown de Creación Rápida (`+ Nuevo ▾`) (115 px):**
   - Opciones desplegables completas:
     1. `Radio AP` $\rightarrow$ Abre `OmniwispAddApModal` para agregar antena al PoP activo.
     2. `PoP` $\rightarrow$ Abre `OmniwispAddGroupModal` en modo PoP (asociado a la Ciudad activa).
     3. `Ciudad / Zona` $\rightarrow$ Abre `OmniwispAddGroupModal` en modo Ciudad/Zona raíz (`parentId = ""`).

---

## 3. Modales Adaptados

### 3.1. `OmniwispAddGroupModal`:
- Título dinámico según se cree una Ciudad/Zona (`parentId.empty()`) o un PoP (`!parentId.empty()`):
  - Modo Ciudad: `"Crear Ciudad / Zona"`
  - Modo PoP: `"Crear PoP (Punto de Presencia)"`
- Si existen Ciudades/Zonas, un selector o campo para elegir a qué Ciudad pertenece el PoP.
- Sin mención a la palabra "Torre".
- Ancho: 300 px (dentro de los 320 px de pantalla).

### 3.2. `OmniwispAddApModal`:
- Título: `"+ Agregar Radio AP"`
- Selector dropdown del PoP asignado (si hay PoPs creados).
- Campos: Nombre del Sector, IP (red ruteada), Usuario AirOS, Contraseña.
- Ancho: 300 px con flag scrollable para teclado táctil.

---

## 4. Plan de Pasos Quirúrgicos

1. **Actualizar `omniwisp_models.hpp`:**
   - Asegurar que `NetworkGroup` distinga tipo o mantenga semántica limpia: Ciudad/Zona (`parentId == ""`) vs PoP (`parentId != ""`).
2. **Actualizar `OmniwispMainView.cpp`:**
   - Cambiar opciones de creación en `ddActions`: `Radio AP`, `PoP`, `Ciudad / Zona`.
   - Limpiar el dropdown de filtro eliminando la palabra "Torre" y mostrando `Todos los PoPs`, Ciudades y PoPs.
3. **Actualizar `OmniwispAddGroupModal.cpp`:**
   - Eliminar la palabra "Torre".
   - Permitir crear explícitamente Ciudad/Zona o PoP.
4. **Verificación de Compilación Multi-Target:**
   - ESP32-S3: `pio run -d bsp/esp32_s3_jc3248`
   - ESP32-P4: `idf.py -C bsp/esp32_p4_jc4880 build`
