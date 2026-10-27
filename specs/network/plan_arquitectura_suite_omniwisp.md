# Plan de Arquitectura: Suite OmniWISP - Tres Vistas Principales (Discovery, Dispositivos, Configuración)

**Documento:** `specs/network/plan_arquitectura_suite_omniwisp.md`  
**Módulo:** `cbdos::omniwisp` (`core/src/ui/views/omniwisp/`)  
**Estado:** Propuesta Formal para Aprobación  
**Reglas Obligatorias:** Cero emojis (solo símbolos oficiales LVGL `LV_SYMBOL_*` o texto ASCII), LVGL v9.5 estricto, responsive para 320x480 (JC3248) y 480x800 (JC4880), Thread-safety en FreeRTOS y hardware real (cero mocks).

---

## 1. Motivación y Arquitectura General

La suite **OmniWISP** se reorganiza en tres pestañas principales de pantalla completa accesibles desde la barra superior de la aplicación:

```
┌──────────────────────────────────────────────────────────────┐
│       [ Discovery ]   [ Dispositivos ]   [ Ajustes ]         │
└──────────────────────────────────────────────────────────────┘
```

Las pestañas superiores usan **exclusivamente texto plano limpio** (sin símbolos ni iconos añadidos), optimizando el ancho táctil de ~100 px por pestaña en pantallas de 320 px de ancho.

El botón `[+ Nuevo]` (para crear manualmente AP, PoP o Zona) se ubica **exclusivamente dentro de la pestaña `Dispositivos`**, donde reside la gestión del inventario guardado en la bóveda (`VaultService`).

---

## 2. Detalle de Cada Pestaña

### 2.1 Pestaña `Discovery` (Escáner de Red en Vivo)
- **Propósito:** Inspección rápida en campo de todos los equipos presentes en el segmento de red (switches de torre, radios en mástil o enlaces de cliente).
- **Contenido a Pantalla Completa:**
  - Barra de control superior: Botón `[ LV_SYMBOL_REFRESH Escanear ]` e indicador de estado (`"3 equipos detectados"` / `"Escaneando..."`).
  - Lista scrollable de tarjetas de equipos:
    - Identifica si el equipo es AP, Estación (CPE) o Router según los TLVs de UBNT Discovery (puerto UDP 10001) o MikroTik MNDP (puerto UDP 5678).
    - Datos visibles: Modelo/Hostname, IP, MAC y versión de Firmware.
    - Dos acciones táctiles por equipo:
      1. **`Conectar`**: Abre directamente la telemetría del radio al vuelo para alineación y diagnóstico físico (sin obligar a guardarlo en un sitio).
      2. **`Agregar`**: Abre el modal de agregar AP con la IP y Nombre ya precargados para asociarlo a un PoP de `Dispositivos`.

### 2.2 Pestaña `Dispositivos` (Inventario Organizado en Vault)
- **Propósito:** Gestión de la red instalada (Zonas -> PoPs -> APs / Sectores).
- **Controles Superiores:**
  - Dropdown de filtro: `Todos` o filtrado por PoP / Zona específica.
  - Botón de acción **`[ LV_SYMBOL_PLUS Nuevo ]`**:
    - Opciones: `AP`, `PoP`, `Zona`.
- **Lista de Sectores Guardados:**
  - Cada sector muestra su estado de telemetría y salud.
  - Al tocar un sector, navega al monitor detallado del sector (`SectorMonitor`) y a la tabla de CPEs asociados (`CpeDiagnostics`).

### 2.3 Pestaña `Configuración` (Ajustes de la Suite)
- **Propósito:** Parámetros operativos y credenciales por defecto para evitar tecleo reiterativo en campo.
- **Campos de Configuración:**
  - **Credenciales de Campo por Defecto:**
    - Usuario predeterminado (ej. `ubnt`).
    - Contraseña de campo predeterminada (guardada en Vault).
    - Puerto web (443 HTTPS o 80 HTTP).
  - **Parámetros de Escáner Discovery:**
    - Timeout de broadcast UDP (3000 ms por defecto).
  - **Parámetros de Telemetría:**
    - Intervalo de refresco de sondeo `/status.cgi` (ej. 2000 ms, 4000 ms).

---

## 3. Modularización de Archivos C++

Para mantener el código limpio y desacoplado:

1. **`OmniwispCommon.hpp`:**
   - Enum de pestaña activa: `enum class OmniTab { Discovery, Devices, Settings };`.
2. **`OmniwispDiscoveryView.hpp` / `.cpp`:**
   - Renderizado y lógica de la vista a pantalla completa del escáner en vivo.
3. **`OmniwispDevicesView.hpp` / `.cpp`:**
   - Renderizado del catálogo de PoPs, sectores y el dropdown `[+ Nuevo]`.
4. **`OmniwispSettingsView.hpp` / `.cpp`:**
   - Formulario de configuración de credenciales y timeouts de la herramienta.
5. **`OmniwispAddApModal.hpp` / `.cpp`:**
   - Vuelve a ser un formulario limpio de 4 campos (Nombre, IP, Usuario, Password) para guardado rápido sin amontonamiento de pestañas internas.
6. **`OmniwispView.hpp` / `.cpp`:**
   - Orquestador principal que dibuja la barra de 3 pestañas y maneja el estado activo.

---

## 4. Criterios de Aceptación
1. Compilación limpia y sin advertencias en ESP32-S3 (PlatformIO) y ESP32-P4 (ESP-IDF).
2. Prohibición total de emojis: únicamente macros `LV_SYMBOL_*` de LVGL o texto plano ASCII.
3. Ergonomía táctil impecable en 320x480 (JC3248) sin ventanas colapsadas.
4. Cero datos falsos: solo telemetría y broadcasts reales del hardware.
