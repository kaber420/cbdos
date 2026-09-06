# 📝 Borrador de Persistencia NVS & Preferencias de Sistema (CBDos v0.2.0)

**Fecha:** 20 de Agosto de 2026  
**Objetivo:** Especificación técnica para la persistencia de configuraciones globales (Brillo, Volumen, Modo Wi-Fi y Ajustes de Pantalla) en la memoria Flash NVS del ESP32-P4 / ESP32-S3.

---

## 🏆 1. Registro del Hito Alcanzado: Conexión Wi-Fi SDIO Operativa
- **Microcontroladores:** ESP32-P4 RISC-V @ 360 MHz (Host) + ESP32-C6 (Coprocesador Esclavo vía SDIO 4-bit Slot 1 @ 20 MHz).
- **Persistencia de Credenciales:** NVS Flash inicializada en `app_main` del ESP32-P4 bajo el espacio de nombres `cbdos_wifi`.
- **Arquitectura Offline-First:** Arranque rápido y autónomo sin inicializar coprocesadores en el encendido. La red se levanta exclusivamente bajo demanda.
- **Transmisión de Red y DHCP:** Integración limpia con `esp_netif_create_default_wifi_sta()` y resolución de tramas de red vía LwIP sin colapsos de memoria ni reinicios.

---

## ⚙️ 2. Estructura de Datos de Preferencias (`SystemConfig`)

Se integrará en `core/src/network/ConfigManager.h` para que esté disponible en todas las capas del sistema operativo:

```cpp
struct SystemConfig {
    uint8_t brightness = 70;          // Nivel de retroiluminación (10% a 100%, 70% térmicamente eficiente)
    uint8_t volume = 70;              // Nivel maestro de audio ES8311 (0% a 100%)
    bool autoConnectWifi = false;     // false = Offline Seguro | true = Conectar en arranque
    uint32_t screenTimeoutSeconds = 60;// Tiempo de apagado de pantalla en segundos (0 = desactivado)
    std::string defaultTheme = "dark"; // Tema visual ("dark", "cyberpunk", "retro")
};
```

---

## 🗄️ 3. Mapeo en Partición NVS (`cbdos_sys`)

| Clave NVS | Tipo | Valor por Defecto | Descripción |
| :--- | :--- | :--- | :--- |
| `bright` | `uint8_t` | `70` | Porcentaje de brillo de pantalla |
| `vol` | `uint8_t` | `70` | Porcentaje de volumen de salida de audio |
| `wifi_auto` | `uint8_t` | `0` (false) | 0: Modo manual/offline, 1: Conexión automática |
| `scr_tout` | `uint32_t` | `60` | Tiempo de inactividad para suspender pantalla |
| `theme` | `string` | `"dark"` | Identificador del tema visual activo |

---

## 🔄 4. Flujo de Inicialización y Sincronización

### 4.1 En el arranque del sistema (`app_main`)
1. Se inicializa el almacenamiento Flash NVS (`nvs_flash_init()`).
2. Se leen las preferencias con `ConfigManager::getInstance().loadSystem(sysCfg)`.
3. Se aplican los valores al hardware antes de renderizar la interfaz:
   - `cbdos::display::setBrightness(sysCfg.brightness);`
   - `cbdos::audio::setVolume(sysCfg.volume);`
4. Si `sysCfg.autoConnectWifi == true`, se invoca la tarea de conexión Wi-Fi en segundo plano sin bloquear la UI.

### 4.2 Desde los paneles de control táctiles
- **QuickSettingsPanel (Desplegable superior):**
  - Al desplazar los sliders de brillo o volumen, se ajusta el hardware en tiempo real.
  - Al soltar el dedo (`LV_EVENT_RELEASED`), se guarda el valor en NVS para no saturar la memoria Flash con escrituras intermedias.
- **WiFiConfigView:**
  - Interruptor *"Conectar automáticamente al iniciar"* vinculado al valor `wifi_auto`.
- **ConfigView (Ajustes del Sistema):**
  - Configuración de timeout de reposo y botón de restauración de fábrica de la NVS.

---

## 📋 5. Plan de Implementación de Código
1. **`ConfigManager.h` & `ConfigManager.cpp`:**
   - Añadir métodos `loadSystem(SystemConfig& cfg)` y `saveSystem(const SystemConfig& cfg)`.
   - Añadir métodos atómicos: `setBrightness(uint8_t)`, `getBrightness()`, `setVolume(uint8_t)`, `getVolume()`.
2. **`QuickSettingsPanel.cpp`:**
   - Cargar valores iniciales desde `ConfigManager` en la construcción.
   - Guardar en NVS al soltar el slider.
3. **`WiFiConfigView.cpp`:**
   - Añadir toggle para autoconexión persistente.
4. **Verificación Multi-Target:**
   - Validar compilación en ESP32-P4 (`idf.py build`) y ESP32-S3 (`pio run`).
