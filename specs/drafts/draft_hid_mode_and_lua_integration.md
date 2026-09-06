# Borrador de Diseño: Control Dinámico de Modo HID y Enlaces Lua (CBDos v0.2.1)

## 📌 1. Motivación y Problema a Resolver
En versiones previas, la inicialización del stack USB HID en el arranque secuestraba los endpoints USB en el ESP32-S3, provocando la pérdida del puerto serie CDC (`/dev/ttyACM0`) e impidiendo el reseteo por software y el flasheo automático desde la PC.

El objetivo de este borrador es definir cómo desacoplar totalmente el modo HID para que:
1. El sistema arranque siempre en modo **Serie Puro** (Seguro y Flasheable).
2. El modo HID se active **exclusivamente bajo demanda** mediante llamadas explícitas desde C++ o desde **Scripts Lua**.
3. Al terminar una acción o cerrar la app, el USB regrese a su modo inactivo/seguro sin requerir reinicio del microcontrolador.

---

## 🛠️ 2. Propuesta de Modificaciones en Código

### A. Interfaz Abstracta (`core/include/cbdos/hid.hpp`)
```cpp
namespace cbdos {
namespace hid {

enum class HidMode {
    Disabled = 0,   // Puerto Serie puro (Default al arrancar)
    Keyboard,       // Teclado USB activo
    Mouse,          // Ratón USB activo
    Composite       // Teclado + Ratón activos
};

class IHidDriver {
public:
    virtual ~IHidDriver() = default;
    virtual bool setMode(HidMode mode) = 0;
    virtual HidMode getMode() const = 0;
    virtual bool isConnected() = 0;
    virtual void sendReport(uint8_t modifiers, const uint8_t keycodes[6]) = 0;
    virtual void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) = 0;
    virtual uint8_t getHostLedState() = 0;
};

// Funciones globales de la API
bool setMode(HidMode mode);
HidMode getMode();
bool isReady();

} // namespace hid
} // namespace cbdos
```

---

### B. Mapeo en el Motor de Lua (`core/src/lua/LuaBridge.cpp`)
Se registrará la tabla global `cbdos.hid` con las siguientes funciones:

```cpp
// Enlace C++ <-> Lua
static int lua_hid_set_mode(lua_State* L) {
    const char* modeStr = luaL_checkstring(L, 1);
    cbdos::hid::HidMode mode = cbdos::hid::HidMode::Disabled;
    if (strcmp(modeStr, "keyboard") == 0) mode = cbdos::hid::HidMode::Keyboard;
    else if (strcmp(modeStr, "mouse") == 0) mode = cbdos::hid::HidMode::Mouse;
    else if (strcmp(modeStr, "composite") == 0) mode = cbdos::hid::HidMode::Composite;
    
    bool ok = cbdos::hid::setMode(mode);
    lua_pushboolean(L, ok);
    return 1;
}

static int lua_hid_get_mode(lua_State* L) {
    auto mode = cbdos::hid::getMode();
    switch (mode) {
        case cbdos::hid::HidMode::Keyboard: lua_pushstring(L, "keyboard"); break;
        case cbdos::hid::HidMode::Mouse: lua_pushstring(L, "mouse"); break;
        case cbdos::hid::HidMode::Composite: lua_pushstring(L, "composite"); break;
        default: lua_pushstring(L, "disabled"); break;
    }
    return 1;
}

static int lua_hid_write(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    // Escribe texto carácter por carácter convirtiendo a keycodes USB
    // ...
    return 0;
}
```

---

### C. Implementación en los BSPs

#### Target ESP32-S3 (`bsp/esp32_s3_jc3248/hal/hal_hid_s3.cpp`):
- `setMode(HidMode::Disabled)`: Llama a `m_keyboard.end()`, `m_mouse.end()` o apaga los descriptores HID para no saturar el bus.
- `setMode(HidMode::Keyboard/Mouse/Composite)`: Inicializa los endpoints TinyUSB sobre el bus USB compuesto sin desconectar `Serial`.

#### Target ESP32-P4 (`bsp/esp32_p4_jc4880/hal/hal_hid_p4.cpp`):
- `setMode(...)`: Controla el driver nativo de TinyUSB de ESP-IDF conectando/desconectando la emulación física en el puerto USB OTG.

---

## 📝 3. Ejemplo de Uso en Aplicación / Script

```lua
-- Script interactivo de prueba: badusb_demo.lua
print("Iniciando secuencia HID...")

-- 1. Activar teclado bajo demanda
cbdos.hid.set_mode("keyboard")
cbdos.system.sleep(600)

-- 2. Ejecutar comandos
cbdos.hid.press_keys({"GUI", "r"})
cbdos.system.sleep(400)
cbdos.hid.write("cmd.exe\n")
cbdos.system.sleep(800)
cbdos.hid.write("echo Hola desde CBDos en Lua! > saludo.txt\n")

-- 3. Apagar modo HID y restaurar puerto serie estándar
cbdos.hid.set_mode("disabled")
print("Secuencia finalizada con exito.")
```
